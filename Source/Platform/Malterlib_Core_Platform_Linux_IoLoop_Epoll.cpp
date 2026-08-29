// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Linux_IoLoop_Epoll.h"
#include "Malterlib_Core_Platform_POSIX_ErrNo.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dlfcn.h>

using namespace NMib;
using namespace NMib::NMemory;
using namespace NMib::NSys;

CIoLoop_Epoll::CIoLoop_Epoll()
{
	int (* fLocal_epoll_create1)(int __flags) __THROW;

	(void * &)fLocal_epoll_create1 = dlsym(RTLD_DEFAULT, "epoll_create1");

	if (fLocal_epoll_create1)
		mp_EpollFd = fLocal_epoll_create1(EPOLL_CLOEXEC);
	else
		mp_EpollFd = epoll_create(1);

	if (mp_EpollFd == -1)
		DMibErrorNet(NMib::NPlatform::fg_FormatErrno("epoll_create (io loop)", errno));

	if (!fLocal_epoll_create1)
		fcntl(mp_EpollFd, F_SETFD, fcntl(mp_EpollFd, F_GETFD) | FD_CLOEXEC);
}

CIoLoop_Epoll::~CIoLoop_Epoll()
{
	close(mp_EpollFd);
}

umint CIoLoop_Epoll::fp_Iterate(bool _bBlock)
{
	static const int nMaxEvents = 64;
	struct epoll_event IncomingEvents[nMaxEvents];
	umint nReported = 0;

	{
		auto Changes = fg_Move(mp_ChangeQueue.f_Take());
		for (auto &Change : Changes)
		{
			if (Change.m_bRemove)
			{
				// The descriptor stays open and owned until this removal is acknowledged, so
				// the number still names the registered file and the delete targets it
				// directly. ENOENT stays tolerated for a registration whose add failed and
				// was reported as an error event — its number was never added
				epoll_event DeleteEv;
				fg_MemClear(&DeleteEv, sizeof(DeleteEv));
				[[maybe_unused]] int DeleteReturn = epoll_ctl(mp_EpollFd, EPOLL_CTL_DEL, Change.m_Handle, &DeleteEv);
				DMibFastCheck(DeleteReturn != -1 || errno == ENOENT);

				// The acknowledgement obligations ride the entry, so every removal
				// acknowledges independently — including one whose descriptor number matches
				// an adjacent removal's. The continuation is dispatched work: it can enqueue
				// locally, and a pass that reports nothing could park past the enqueue
				CIoLoopDeferredAck Ack{Change.m_pRegistration, Change.m_pDeregWait, fg_Move(Change.m_fOnDeregistered)};
				fg_RunDeregAcknowledgement(Ack);
				++nReported;

				continue;
			}

			if (Change.m_bInternal)
			{
				epoll_event Ev;
				fg_MemClear(&Ev, sizeof(Ev));
				Ev.events = EPOLLIN;
				Ev.data.fd = Change.m_Handle;
				[[maybe_unused]] int Return = epoll_ctl(mp_EpollFd, EPOLL_CTL_ADD, Change.m_Handle, &Ev);
				DMibFastCheck(Return != -1);

				continue;
			}

			// The first socket on the loop is late enough for the log to be configured
			fg_UringLogMemlockFallback();

			auto *pRegistration = Change.m_pRegistration;

			epoll_event Ev;
			fg_MemClear(&Ev, sizeof(Ev));
			Ev.events = EPOLLET | EPOLLRDHUP | fg_PollInterestFromIoLoopMask(pRegistration->m_EventMask);
			Ev.data.ptr = pRegistration;

			int Return = epoll_ctl(mp_EpollFd, EPOLL_CTL_ADD, Change.m_Handle, &Ev);
			[[maybe_unused]] int Error = Return == -1 ? errno : 0;
			// An add can fail environmentally with a correct caller (the max_user_watches
			// limit, memory pressure). Anything else failing is a broken invariant in this
			// layer and still trips
			DMibFastCheck(Return != -1 || Error == ENOSPC || Error == ENOMEM);

			if (Return == -1)
			{
				// A swallowed registration failure would leave the connection in the caller's
				// bookkeeping but never polled — a silent hang. Reporting it as an error
				// event makes it die loudly in every build, untested hardware and exhausted
				// watch limits included. The callback is dispatched work: it can enqueue
				// locally, and a pass that reports nothing could park past the enqueue — the
				// failed registration would never produce an event to wake the loop again
				pRegistration->m_fOnEvents(pRegistration->m_pToken, NSys::EIoLoopEvent::mc_Error, Error);
				++nReported;

				continue;
			}

			if (Change.m_bNotifyRegistered)
			{
				// Dispatched work, same as above
				pRegistration->m_fOnEvents(pRegistration->m_pToken, NSys::EIoLoopEvent::mc_None, 0);
				++nReported;
			}
		}
	}

	// A pass whose change processing ran callbacks must not block: those callbacks can
	// enqueue local work without signaling the loop, and a park would sleep past it
	bool bBlock = _bBlock && nReported == 0;
	if (bBlock)
	{
		// Commit to parking, unless a wake is already owed, in which case this pass polls and
		// the caller re-checks its work when the iterate returns
		EWakeState Previous = EWakeState(mp_WakeState.f_FetchOr(uint32(EWakeState::mc_Parked), NAtomic::gc_MemoryOrder_SequentiallyConsistent));
		if (fg_IsSet(Previous, EWakeState::mc_Pending))
		{
			mp_WakeState.f_Store(0, NAtomic::gc_MemoryOrder_Release);
			bBlock = false;
		}
	}

	int nEvents;
	do
	{
		nEvents = epoll_wait(mp_EpollFd, IncomingEvents, nMaxEvents, bBlock ? -1 : 0);
	}
	while (nEvents == -1 && errno == EINTR)
		;

	if (bBlock)
		mp_WakeState.f_Store(0, NAtomic::gc_MemoryOrder_Release);

	for (int iIncomingEvent = 0; iIncomingEvent < nEvents; ++iIncomingEvent)
	{
		struct epoll_event const &Event = IncomingEvents[iIncomingEvent];

		if (!(Event.events & (EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLERR | EPOLLHUP)))
			continue;

		if ((Event.events & EPOLLIN) && Event.data.fd == mp_ReadWritePipe[0])
		{
			char Buf[16];
			int ReadRet;
			do
			{
				ReadRet = read(mp_ReadWritePipe[0], Buf, sizeof(Buf));
			}
			while(ReadRet > 0)
				;

			continue;
		}

		auto *pRegistration = (NSys::CIoLoopRegistration *)Event.data.ptr;

#if DMibConfig_IoDebug_Enable
		if (fg_UringTraceEnabled() && (Event.events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)))
			fg_UringTrace("close-event", pRegistration->m_pToken, pRegistration->m_Handle, Event.events);
#endif

		pRegistration->m_fOnEvents(pRegistration->m_pToken, fg_IoLoopEventsFromPollBits(Event.events), 0);
		++nReported;
	}

	return nReported;
}
