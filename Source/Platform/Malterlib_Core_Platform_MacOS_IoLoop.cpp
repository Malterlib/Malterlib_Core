// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_MacOS_IoLoop.h"
#include "Malterlib_Core_Platform_POSIX_ErrNo.h"

#include <unistd.h>
#include <errno.h>
#include <sys/event.h>

using namespace NMib;
using namespace NMib::NMemory;
using namespace NMib::NSys;

CIoLoop_KQueue::CIoLoop_KQueue()
{
	mp_KQueue = kqueue();

	// Failure leaves no shared socket loop; report and trap because no caller can recover by unwinding.
	if (mp_KQueue == -1)
	{
		NStr::CStr KQueueError = NPlatform::fg_FormatErrno("kqueue (io loop)", errno) + "\n";
		fg_ConsoleErrorOutput(KQueueError.f_Span());
		DMibPDebugBreak;
	}
}

CIoLoop_KQueue::~CIoLoop_KQueue()
{
	close(mp_KQueue);
}

auto CIoLoop_KQueue::fp_CreateRegistration() -> NSys::CIoLoopRegistration *
{
	return fg_ConstructObject<CKQueueRegistration>(CDefaultAllocator());
}

umint CIoLoop_KQueue::fp_Iterate(bool _bBlock)
{
	static const int nMaxEvents = 64;
	struct kevent StackEvents[nMaxEvents];
	struct kevent *pIncomingEvents = StackEvents;
	int nEventCapacity = nMaxEvents;
	timespec PollTimeout = {0, 0};
	umint nReported = 0;

	auto Changes = fg_Move(mp_ChangeQueue.f_Take());

	// Removal descriptors remain open until acknowledgement, so kevent can delete by number.
	NContainer::TCVector<struct kevent> ApplyChanges;
	for (auto const &Change : Changes)
	{
		if (Change.m_bInternal)
		{
			struct kevent CurEvent;
			fg_MemClear(&CurEvent, sizeof(struct kevent));
			CurEvent.ident = Change.m_Handle;
			CurEvent.filter = EVFILT_READ;
			CurEvent.flags = EV_CLEAR | EV_ADD;
			CurEvent.udata = nullptr;
			ApplyChanges.f_Insert(CurEvent);

			continue;
		}

		uint16_t Flags = Change.m_bRemove ? (uint16_t)(EV_CLEAR | EV_DELETE) : (uint16_t)(EV_CLEAR | EV_ADD);
		auto *pRegistration = Change.m_pRegistration;

		static_cast<CKQueueRegistration *>(pRegistration)->m_bAddFailed = false;

		// Close interest is independent of readiness interest: the peer's half-close arrives as EV_EOF
		// on the read filter and the local shutdown as EV_EOF on the write filter, so every registration
		// installs both, and the readiness a filter reports outside the mask is dropped at dispatch
		for (int16_t Filter : {int16_t(EVFILT_READ), int16_t(EVFILT_WRITE)})
		{
			struct kevent CurEvent;
			fg_MemClear(&CurEvent, sizeof(struct kevent));
			CurEvent.ident = Change.m_Handle;
			CurEvent.filter = Filter;
			CurEvent.flags = Flags;
			CurEvent.udata = pRegistration;
			ApplyChanges.f_Insert(CurEvent);
		}
	}

	// A change-bearing pass must not block: apply changes and deliver acknowledgements even without a ready event.
	bool bBlock = _bBlock && Changes.f_IsEmpty();
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

	// Reserve one event slot per change; otherwise an EV_ERROR receipt can stop kevent before the remaining changes apply.
	NContainer::TCVector<struct kevent> LargeEvents;
	if (ApplyChanges.f_GetLen() > umint(nMaxEvents))
	{
		LargeEvents.f_SetLen(ApplyChanges.f_GetLen());
		pIncomingEvents = LargeEvents.f_GetArray();
		nEventCapacity = int(ApplyChanges.f_GetLen());
	}

	// EINTR occurs after changelist application; resubmission is safe because repeated deletes return ENOENT.
	// Other failures must not acknowledge removals the kernel may never have applied.
	int nEvents;
	do
	{
		nEvents = kevent
			(
				mp_KQueue
				, ApplyChanges.f_GetArray()
				, ApplyChanges.f_GetLen()
				, pIncomingEvents
				, nEventCapacity
				, bBlock ? nullptr : &PollTimeout
			)
		;
	}
	while (nEvents == -1 && errno == EINTR)
		;

	if (nEvents == -1)
	{
		NStr::CStr KEventError = NPlatform::fg_FormatErrno("kevent (io loop)", errno) + "\n";
		fg_ConsoleErrorOutput(KEventError.f_Span());
		DMibPDebugBreak;
	}

	if (bBlock)
		mp_WakeState.f_Store(0, NAtomic::gc_MemoryOrder_Release);

	for (int iEvent = 0; iEvent < nEvents; ++iEvent)
	{
		auto const &Event = pIncomingEvents[iEvent];

		if (Event.ident == mp_ReadWritePipe[0])
		{
			if (Event.filter == EVFILT_READ)
			{
				char Buf[16];

				int ReadRet;
				do
				{
					ReadRet = read(mp_ReadWritePipe[0], Buf, sizeof(Buf));
				}
				while(ReadRet > 0)
					;
			}
			continue;
		}

		auto *pRegistration = (NSys::CIoLoopRegistration *)Event.udata;

		// Darwin echoes registration flags, including EV_ADD; decode error and EOF before ordinary readiness.
		NSys::EIoLoopEvent Events;
		int Error = 0;
		if (Event.flags & EV_ERROR)
		{
			Events = NSys::EIoLoopEvent::mc_Error;
			Error = Event.data ? (int)Event.data : -1;

			// EV_DELETE/ENOENT can acknowledge a retried removal, not a socket failure.
			// A rejected add must suppress its registration-applied notification.
			if (Event.flags & EV_DELETE)
			{
				if (Error == ENOENT)
					continue;
			}
			else
				static_cast<CKQueueRegistration *>(pRegistration)->m_bAddFailed = true;
		}
		else if (Event.flags & EV_EOF)
		{
			if (Event.filter == EVFILT_WRITE)
				Events = NSys::EIoLoopEvent::mc_WriteClosed;
			else if (Event.filter == EVFILT_READ)
				Events = NSys::EIoLoopEvent::mc_ReadClosed;
			else
			{
				DMibFastCheck(false);

				continue;
			}
		}
		else if (Event.filter == EVFILT_READ)
			Events = NSys::EIoLoopEvent::mc_Read;
		else if (Event.filter == EVFILT_WRITE)
			Events = NSys::EIoLoopEvent::mc_Write;
		else
		{
			DMibFastCheck(false);

			continue;
		}

		// A filter installed for its close events also reports readiness the registration never asked for
		if (!fg_IsSet(pRegistration->m_EventMask, Events) && (Events == NSys::EIoLoopEvent::mc_Read || Events == NSys::EIoLoopEvent::mc_Write))
			continue;

		++nReported;
		pRegistration->m_fOnEvents(pRegistration->m_pToken, Events, Error);
	}

	// Process changes: acknowledgements and registration-applied notifications run after the
	// events, on the full change list
	for (auto &Change : Changes)
	{
		if (Change.m_bInternal)
			continue;

		if (Change.m_bRemove)
		{
			// Count acknowledgement callbacks as work; they can enqueue locally without signalling.
			CIoLoopDeferredAck Ack{Change.m_pRegistration, Change.m_pDeregWait, fg_Move(Change.m_fOnDeregistered)};
			fp_RunDeregAcknowledgement(Ack);
			++nReported;
		}
		else if (Change.m_bNotifyRegistered && !static_cast<CKQueueRegistration *>(Change.m_pRegistration)->m_bAddFailed)
		{
			Change.m_pRegistration->m_fOnEvents(Change.m_pRegistration->m_pToken, NSys::EIoLoopEvent::mc_None, 0);
			++nReported;
		}
	}

	return nReported;
}

NMib::NSys::ICIoLoop *fg_CreatePlatformIoLoop()
{
	return fg_ConstructObject<CIoLoop_KQueue>(CAllocator_NonTrackedHeap());
}
