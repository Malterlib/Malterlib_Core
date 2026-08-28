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

	// A loop that cannot be created leaves the process nothing to run its sockets on: this is
	// reported and trapped rather than thrown, since no caller has anything to unwind to
	if (mp_KQueue == -1)
	{
		NStr::CStr KQueueError = NPlatform::fg_FormatErrno("kqueue (io loop)", errno) + "\n";
		fg_ConsoleErrorOutput(KQueueError.f_Span());
		DMibPDebugBreak; // Failed to create kqueue
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

	// Build the kernel changelist, one kevent per registered filter. The descriptor a removal
	// names stays open and owned until its acknowledgement runs, so the delete targets the
	// registered file directly by number
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

		// Fresh for this pass's receipts
		static_cast<CKQueueRegistration *>(pRegistration)->m_bAddFailed = false;

		if (fg_IsSet(pRegistration->m_EventMask, NSys::EIoLoopEvent::mc_Read))
		{
			struct kevent CurEvent;
			fg_MemClear(&CurEvent, sizeof(struct kevent));
			CurEvent.ident = Change.m_Handle;
			CurEvent.filter = EVFILT_READ;
			CurEvent.flags = Flags;
			CurEvent.udata = pRegistration;
			ApplyChanges.f_Insert(CurEvent);
		}

		if (fg_IsSet(pRegistration->m_EventMask, NSys::EIoLoopEvent::mc_Write))
		{
			struct kevent CurEvent;
			fg_MemClear(&CurEvent, sizeof(struct kevent));
			CurEvent.ident = Change.m_Handle;
			CurEvent.filter = EVFILT_WRITE;
			CurEvent.flags = Flags;
			CurEvent.udata = pRegistration;
			ApplyChanges.f_Insert(CurEvent);
		}
	}

	// Registration changes ride along with the wait, and a pass that carries them must not
	// block, so the changes take effect even when no event is due. Withheld deletes count
	// too: their acknowledgements below must run promptly
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

	// The event list holds at least one entry per change: a change that fails comes back as an
	// EV_ERROR event, and an event list too small for those receipts has kevent stop at the
	// first failure and return -1 with the rest of the changelist unapplied. A batch past the
	// stack buffer borrows a larger one for the pass
	NContainer::TCVector<struct kevent> LargeEvents;
	if (ApplyChanges.f_GetLen() > umint(nMaxEvents))
	{
		LargeEvents.f_SetLen(ApplyChanges.f_GetLen());
		pIncomingEvents = LargeEvents.f_GetArray();
		nEventCapacity = int(ApplyChanges.f_GetLen());
	}

	// The acknowledgement walk below frees sockets on the assumption that their removals were
	// applied. An interrupted kevent has applied its changelist before sleeping (per-change
	// failures come back as EV_ERROR events instead), so the retry re-submits the same
	// changes harmlessly — reapplied deletes just report ENOENT. Anything else failing here
	// would break that assumption and trips instead of silently acknowledging removals the
	// kernel never saw
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
		++nReported;

		// kevent to the loop's normalized vocabulary. One category per event: the error and
		// EOF classes come first, then plain filter readiness. Darwin echoes the registration
		// flags on returned events, so EV_ADD rides along and carries no information here
		NSys::EIoLoopEvent Events;
		int Error = 0;
		if (Event.flags & EV_ERROR)
		{
			Events = NSys::EIoLoopEvent::mc_Error;
			Error = Event.data ? (int)Event.data : -1;

			// A receipt is the changelist entry itself with EV_ERROR added, so its flags still say
			// what was asked, and a registration added and removed in the same pass gets one for
			// each. A delete the interrupted kevent above had already applied comes back as
			// ENOENT when the retry resubmits it: the receipt for a removal, not a socket
			// failure, and the acknowledgement below is what reports the removal. A rejected add
			// (ENOMEM, a descriptor kqueue refuses) is reported as this error, and the applied
			// notification below would then claim a registration the kernel never made, so it is
			// withdrawn; the epoll and completion port loops report the same way
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
			// The acknowledgement obligations ride the entry, so every removal acknowledges
			// independently. The continuation is dispatched work: it can enqueue locally,
			// and a pass that reports nothing could park past the enqueue
			CIoLoopDeferredAck Ack{Change.m_pRegistration, Change.m_pDeregWait, fg_Move(Change.m_fOnDeregistered)};
			fp_RunDeregAcknowledgement(Ack);
			++nReported;
		}
		else if (Change.m_bNotifyRegistered && !static_cast<CKQueueRegistration *>(Change.m_pRegistration)->m_bAddFailed)
		{
			// Dispatched work, same as above
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
