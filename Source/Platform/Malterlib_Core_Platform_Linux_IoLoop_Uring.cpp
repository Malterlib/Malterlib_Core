// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Linux_IoLoop_Uring_Internal.h"
#include "Malterlib_Core_Platform_Linux_IoSubSystem.h"
#include "Malterlib_Core_Platform_POSIX_ErrNo.h"

#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

using namespace NMib;
using namespace NMib::NMemory;
using namespace NMib::NSys;

CIoLoop_IoUring::CIoLoop_IoUring()
{
	bool bSqPoll = false;
#if DMibConfig_IoDebug_Enable
	bSqPoll = NMib::NSys::fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStrNonTracked("MalterlibIoUringSqPoll")) == "1";
#endif

	mp_pIo = &fg_IoSubSystem_Linux();

	if (mp_pIo->m_bUringAvailable && mp_Ring.f_Create(gc_UringLoopSqEntries, gc_UringLoopCqEntries, true, bSqPoll))
		mp_bRingCreated = true;
}

CIoLoop_IoUring::~CIoLoop_IoUring()
{
	// Owner shutdown must finish deregistrations and all send-buffer releases before destroying the ring.
	// Release functors retain actor operation trackers, whose drains precede loop teardown.
	DMibFastCheck(mp_nDeregistering == 0);
	DMibFastCheck(mp_PendingIoOps.f_IsEmpty());
	DMibFastCheck(mp_StreamFlushQueue.f_IsEmpty());
	DMibFastCheck(mp_NotifyPending.f_IsEmpty());

	if (mp_bRingCreated)
		mp_Ring.f_Destroy();
}

auto CIoLoop_IoUring::fp_CreateRegistration() -> NSys::CIoLoopRegistration *
{
	return fg_ConstructObject<CUringRegistration>(CDefaultAllocator());
}

bool CIoLoop_IoUring::f_IsRingCreated() const
{
	return mp_bRingCreated;
}

void CIoLoop_IoUring::f_RequestReadiness(NSys::CIoLoopRegistration *_pRegistration, NSys::EIoLoopEvent _EventMask)
{
	if (_EventMask == NSys::EIoLoopEvent::mc_None)
		return;

	// Only the zero-to-nonzero requester queues a notification; exchange consumes all earlier bits without losing later requests.
	NSys::EIoLoopEvent Previous = NSys::EIoLoopEvent(_pRegistration->m_RequestedEvents.f_FetchOr(uint32(_EventMask), NAtomic::gc_MemoryOrder_AcquireRelease));
	if (Previous != NSys::EIoLoopEvent::mc_None)
		return;

	CIoLoopChange Change;
	Change.m_bReadinessRequest = true;
	Change.m_Handle = _pRegistration->m_Handle;
	Change.m_pRegistration = _pRegistration;
	mp_ChangeQueue.f_Push(fg_Move(Change));
	fp_SignalWake();
}

CIoUringSqe &CIoLoop_IoUring::fp_PrepareSqe()
{
	CIoUringSqe Sqe;
	fg_MemClear(&Sqe, sizeof(Sqe));
	mp_PendingSqes.f_Insert(Sqe);
	return mp_PendingSqes[mp_PendingSqes.f_GetLen() - 1];
}

void CIoLoop_IoUring::fp_ArmPoll(CUringRegistration *_pRegistration, uint64 _Tag, uint32 _PollMask, bool _bMultishot)
{
	// Level-at-arm polling catches readiness after would-block without a separate probe; require a fresh request to avoid spinning.
	// A multishot poll reports the level once at arm and then one CQE per wakeup that carries a requested bit
	CIoUringSqe &Sqe = fp_PrepareSqe();
	Sqe.m_Opcode = gc_IoUringOp_PollAdd;
	Sqe.m_Fd = _pRegistration->m_Handle;
	Sqe.m_Len = _bMultishot ? gc_IoUringPoll_AddMulti : 0;
	Sqe.m_OpFlags = _PollMask;
	Sqe.m_UserData = (uint64)(umint)_pRegistration | _Tag;
	++_pRegistration->m_nOutstanding;
}

void CIoLoop_IoUring::fp_ArmRequested(CUringRegistration *_pRegistration, NSys::EIoLoopEvent _EventMask)
{
	if (fg_IsSet(_EventMask, NSys::EIoLoopEvent::mc_Read) && !_pRegistration->m_bReadPollArmed)
	{
		_pRegistration->m_bReadPollArmed = true;
		fp_ArmPoll(_pRegistration, gc_UringTag_ReadPoll, EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP);
	}

	if (fg_IsSet(_EventMask, NSys::EIoLoopEvent::mc_Write) && !_pRegistration->m_bWritePollArmed)
	{
		_pRegistration->m_bWritePollArmed = true;
		fp_ArmPoll(_pRegistration, gc_UringTag_WritePoll, EPOLLOUT | EPOLLERR | EPOLLHUP);
	}
}

// Cancel exact user data. Count both target and cancel CQEs; already-completed or already-reaped targets need no retry.
// Zero-copy sends use their operation address rather than the registration address.
void CIoLoop_IoUring::fp_PrepareCancel(CUringRegistration *_pRegistration, uint64 _TargetUserData)
{
	CIoUringSqe &Sqe = fp_PrepareSqe();
	Sqe.m_Opcode = gc_IoUringOp_AsyncCancel;
	Sqe.m_Fd = -1;
	Sqe.m_Addr = _TargetUserData;
	Sqe.m_UserData = (uint64)(umint)_pRegistration | gc_UringTag_Cancel;
	++_pRegistration->m_nOutstanding;
}

void CIoLoop_IoUring::fp_CancelOutstanding(CUringRegistration *_pRegistration, umint &_nReported)
{
	uint64 Base = (uint64)(umint)_pRegistration;

	if (_pRegistration->m_bReadPollArmed)
		fp_PrepareCancel(_pRegistration, Base | gc_UringTag_ReadPoll);
	if (_pRegistration->m_bWritePollArmed)
		fp_PrepareCancel(_pRegistration, Base | gc_UringTag_WritePoll);
	if (_pRegistration->m_ClosePoll == CUringRegistration::EClosePoll::mc_Armed)
		fp_PrepareCancel(_pRegistration, Base | gc_UringTag_ClosePoll);

	if (_pRegistration->m_pSendOp)
		fp_PrepareCancel(_pRegistration, (uint64)(umint)_pRegistration->m_pSendOp | gc_UringTag_SendOp);
	else if (_pRegistration->m_pSendRing)
	{
		// Published operations with no kernel work need immediate cancellation or they would never finish.
		fp_FailSendRecords(_pRegistration, NSys::EIoCompletionStatus::mc_Cancelled, 0, _nReported);
	}

	// An armed stream terminates through cancellation; a buffer-parked stream still owes an immediate terminal segment.
	if (_pRegistration->m_bStreamArmed)
		fp_PrepareCancel(_pRegistration, Base | gc_UringTag_RecvStream);
	else if (_pRegistration->m_fStreamSink && !_pRegistration->m_bStreamEnded)
		fp_EndStream(_pRegistration, NSys::EIoCompletionStatus::mc_Cancelled, 0, _nReported);
}

void CIoLoop_IoUring::fp_SweepPendingOps(CUringRegistration *_pRegistration)
{
	// Owner-ordered submission and removal can cross an iterate boundary. Sweep pending operations before freeing
	// the registration so their cancellation still has a valid identity.
	NContainer::TCVector<CUringIoOp *> Swept;
	{
		DMibLock(mp_IoOpLock);
		if (!mp_PendingIoOps.f_IsEmpty())
		{
			NContainer::TCVector<CUringIoOp *> Kept;
			Kept.f_Reserve(mp_PendingIoOps.f_GetLen());
			for (CUringIoOp *pOp : mp_PendingIoOps)
			{
				if (pOp->m_pRegistration == _pRegistration)
					Swept.f_InsertLast(pOp);
				else
					Kept.f_InsertLast(pOp);
			}
			mp_PendingIoOps = fg_Move(Kept);
		}
	}

	for (CUringIoOp *pOp : Swept)
	{
		if (pOp->m_bStreamStart)
		{
			NSys::CIoStreamSegment Segment;
			Segment.m_Status = NSys::EIoCompletionStatus::mc_Cancelled;
			pOp->m_fSink(fg_Move(Segment));
		}
		else if (!pOp->m_bStreamResume)
		{
			pOp->m_fOnComplete(NSys::CIoCompletion{.m_Status = NSys::EIoCompletionStatus::mc_Cancelled});
			if (pOp->m_fOnBufferReleased)
				pOp->m_fOnBufferReleased();
		}

		fg_DeleteObject(NMemory::CDefaultAllocator(), pOp);
	}
}

void CIoLoop_IoUring::fp_TryAcknowledge(CUringRegistration *_pRegistration, umint &_nReported)
{
	if (!_pRegistration->m_bDeregistering || _pRegistration->m_nOutstanding)
		return;

	// Zero obligations means no CQE can name this registration. Count acknowledgement callbacks as dispatched work.
	fp_SweepPendingOps(_pRegistration);
	fp_ReleaseStream(_pRegistration);
	fp_ReleaseSendRing(_pRegistration);

	// Notifications that outlive this registration must not touch its count or feed its window
	for (CUringIoOp *pOp : mp_NotifyPending)
	{
		if (pOp->m_pNotifyRegistration == _pRegistration)
			pOp->m_pNotifyRegistration = nullptr;
	}
	--mp_nDeregistering;

#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_TraceEnabled())
		fg_UringTrace("ack", _pRegistration->m_pToken, _pRegistration->m_Handle, 0);
#endif

	CIoLoopDeferredAck Ack{_pRegistration, _pRegistration->m_pDeregWait, fg_Move(_pRegistration->m_fOnDeregistered)};
	fp_RunDeregAcknowledgement(Ack);
	++_nReported;
}

umint CIoLoop_IoUring::fp_Iterate(bool _bBlock)
{
	umint nReported = 0;

	// The driving thread must enable the disabled ring to claim single-issuer ownership.
	if (mp_Ring.m_bNeedsEnable) [[unlikely]]
	{
		// An unusable ring leaves no socket loop; report and trap because no caller can recover by unwinding.
		if (!mp_Ring.f_EnableRings())
		{
			NStr::CStr EnableError = NMib::NPlatform::fg_FormatErrno("io_uring enable rings (io loop)", errno) + "\n";
			NMib::NSys::fg_ConsoleErrorOutput(EnableError.f_Span());
			DMibPDebugBreak;
		}
	}

	{
		auto Changes = fg_Move(mp_ChangeQueue.f_Take());
		for (auto &Change : Changes)
		{
			if (Change.m_bRemove)
			{
				auto *pRegistration = static_cast<CUringRegistration *>(Change.m_pRegistration);

				pRegistration->m_RequestedEvents.f_Exchange(0, NAtomic::gc_MemoryOrder_AcquireRelease);
				pRegistration->m_bDeregistering = true;
				++mp_nDeregistering;
				pRegistration->m_pDeregWait = Change.m_pDeregWait;
				pRegistration->m_fOnDeregistered = fg_Move(Change.m_fOnDeregistered);
#if DMibConfig_IoDebug_Enable
				if (mp_pIo->f_TraceEnabled())
					fg_UringTrace("deregister", pRegistration->m_pToken, Change.m_Handle, (uint32)pRegistration->m_nOutstanding);
#endif

				fp_CancelOutstanding(pRegistration, nReported);
				fp_TryAcknowledge(pRegistration, nReported);
			}
			else if (Change.m_bInternal)
			{
				CIoUringSqe &Sqe = fp_PrepareSqe();
				Sqe.m_Opcode = gc_IoUringOp_PollAdd;
				Sqe.m_Fd = Change.m_Handle;
				Sqe.m_OpFlags = EPOLLIN;
				Sqe.m_Len = gc_IoUringPoll_AddMulti;
				Sqe.m_UserData = gc_UringUserData_Pipe;
			}
			else if (Change.m_bReadinessRequest)
			{
				auto *pRegistration = static_cast<CUringRegistration *>(Change.m_pRegistration);

				// Discard requests for removed registrations or directions now using completion I/O; preserve the other direction independently.
				NSys::EIoLoopEvent Requested = NSys::EIoLoopEvent(pRegistration->m_RequestedEvents.f_Exchange(0, NAtomic::gc_MemoryOrder_AcquireRelease));

				if (pRegistration->m_bCompletionModeRead)
					Requested &= ~NSys::EIoLoopEvent::mc_Read;
				if (pRegistration->m_bCompletionModeWrite)
					Requested &= ~NSys::EIoLoopEvent::mc_Write;

				if (!pRegistration->m_bDeregistering)
					fp_ArmRequested(pRegistration, Requested);
			}
			else
			{
				auto *pRegistration = static_cast<CUringRegistration *>(Change.m_pRegistration);
#if DMibConfig_IoDebug_Enable
				if (mp_pIo->f_TraceEnabled())
					fg_UringTrace("register", pRegistration->m_pToken, Change.m_Handle, uint32(pRegistration->m_EventMask));
#endif

				// Level-at-arm polls replay initial readiness. Keep close interest even while completion I/O is idle.
				NSys::EIoLoopEvent Requested = NSys::EIoLoopEvent(pRegistration->m_RequestedEvents.f_Exchange(0, NAtomic::gc_MemoryOrder_AcquireRelease));
				fp_ArmRequested(pRegistration, Requested);
				pRegistration->m_ClosePoll = CUringRegistration::EClosePoll::mc_Armed;
				fp_ArmPoll(pRegistration, gc_UringTag_ClosePoll, EPOLLRDHUP, true);

				if (Change.m_bNotifyRegistered)
				{
					// Pin during callbacks and count them as work so local enqueues cannot be slept past.
					++pRegistration->m_nOutstanding;
					pRegistration->m_fOnEvents(pRegistration->m_pToken, NSys::EIoLoopEvent::mc_None, 0);
					--pRegistration->m_nOutstanding;
					++nReported;
				}
			}
		}
	}

	// Apply registration changes before submissions. Owner ordering and the removal sweep keep pending registration pointers valid.
	{
		NContainer::TCVector<CUringIoOp *> IoOps;
		{
			DMibLock(mp_IoOpLock);
			IoOps = fg_Move(mp_PendingIoOps);
		}

		// Keep one kernel send per descriptor to preserve wire order; completed data can keep transmitting while the next send is submitted.
		NContainer::TCVector<CUringIoOp *> Deferred;

		for (CUringIoOp *pOp : IoOps)
		{
			auto *pRegistration = pOp->m_pRegistration;

			if (pRegistration->m_bDeregistering)
			{
				// Sends complete in submission order, so these wait for the kernel work ahead of them;
				// the acknowledgement sweep cancels them once that has reported
				Deferred.f_InsertLast(pOp);
				continue;
			}

			if (pOp->m_bStreamResume)
			{
				if (pRegistration->m_pRecvRing)
					fp_ResumeStream(pRegistration);

				fg_DeleteObject(NMemory::CDefaultAllocator(), pOp);
				continue;
			}

			if (!pOp->m_bStreamStart && pRegistration->m_SendError)
			{
				++pRegistration->m_nOutstanding;
				pOp->m_fOnComplete(NSys::CIoCompletion{.m_Error = pRegistration->m_SendError, .m_Status = NSys::EIoCompletionStatus::mc_Error});
				--pRegistration->m_nOutstanding;
				if (pOp->m_fOnBufferReleased)
					pOp->m_fOnBufferReleased();
				fg_DeleteObject(NMemory::CDefaultAllocator(), pOp);
				++nReported;

				continue;
			}

			// Cancel readiness only for the direction switching to completion I/O; retain the other direction and close interest.
			bool &bDirectionMode = pOp->m_bStreamStart ? pRegistration->m_bCompletionModeRead : pRegistration->m_bCompletionModeWrite;
			if (!bDirectionMode)
			{
				bDirectionMode = true;

				if (pOp->m_bStreamStart)
				{
					if (pRegistration->m_bReadPollArmed)
						fp_PrepareCancel(pRegistration, (uint64)(umint)pRegistration | gc_UringTag_ReadPoll);
				}
				else
				{
					if (pRegistration->m_bWritePollArmed)
						fp_PrepareCancel(pRegistration, (uint64)(umint)pRegistration | gc_UringTag_WritePoll);
				}

#if DMibConfig_IoDebug_Enable
				if (mp_pIo->f_TraceEnabled())
					fg_UringTrace(pOp->m_bStreamStart ? "completion-mode-read" : "completion-mode-write", pRegistration->m_pToken, pRegistration->m_Handle, 0);
#endif
			}

			if (pOp->m_bStreamStart)
			{
				DMibFastCheck(!pRegistration->m_fStreamSink);
				pRegistration->m_fStreamSink = fg_Move(pOp->m_fSink);
				pRegistration->m_nStreamBufferBytes = pOp->m_nBytes;

				if (fp_StartStream(pRegistration, fg_Move(pOp->m_pBackpressure), nReported))
					fp_ArmStream(pRegistration);
				else
					fp_EndStream(pRegistration, NSys::EIoCompletionStatus::mc_Error, ENOMEM, nReported);

				fg_DeleteObject(NMemory::CDefaultAllocator(), pOp);
				continue;
			}

			// Remote sends retain the zero-copy path; local sends use a provided-buffer bundle to avoid resubmission gaps.
			fg_UringProbePeerClass(mp_pIo, pRegistration);

			if (!pRegistration->m_bZeroCopyEligible.f_Load(NAtomic::gc_MemoryOrder_Relaxed))
			{
				// In-order or not at all: an operation deferred for ring room holds everything
				// behind it for its registration back too
				if (pRegistration->m_bSendPublishStalled || !fp_PublishSend(pRegistration, pOp, nReported))
				{
					pRegistration->m_bSendPublishStalled = true;
					Deferred.f_InsertLast(pOp);
				}

				continue;
			}

			if (pRegistration->m_pSendOp)
			{
				Deferred.f_InsertLast(pOp);
				continue;
			}

			CIoUringSqe &Sqe = fp_PrepareSqe();

			pRegistration->m_pSendOp = pOp;

#if DMibConfig_IoDebug_Enable
			if (mp_pIo->f_StatsEnabled() && pOp->m_EnqueueStamp)
			{
				mp_pIo->m_UringStats.m_nSendSubmitLagNs.f_FetchAdd(fg_UringStatsNow() - pOp->m_EnqueueStamp, NAtomic::gc_MemoryOrder_Relaxed);
				mp_pIo->m_UringStats.m_nSendSubmitLagOps.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
			}

			if (mp_pIo->f_StatsEnabled() && pRegistration->m_SendIdleStamp)
			{
				mp_pIo->m_UringStats.m_nSendIdleNs.f_FetchAdd(fg_UringStatsNow() - pRegistration->m_SendIdleStamp, NAtomic::gc_MemoryOrder_Relaxed);
				mp_pIo->m_UringStats.m_nSendIdleGaps.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
				pRegistration->m_SendIdleStamp = 0;
			}
#endif

			umint nSendBytes = 0;
			for (umint iVector = 0; iVector < (umint)pOp->m_MsgHdr.msg_iovlen; ++iVector)
				nSendBytes += pOp->m_IoVecs[iVector].iov_len;

			pOp->m_nRequested = nSendBytes;
			pOp->m_bZeroCopy = fp_IsZeroCopyEligible(pRegistration, nSendBytes);

#if DMibConfig_IoDebug_Enable
			if (mp_pIo->f_StatsEnabled())
			{
				mp_pIo->m_UringStats.m_nSendOps.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
				mp_pIo->m_UringStats.m_nSendBytesRequested.f_FetchAdd(nSendBytes, NAtomic::gc_MemoryOrder_Relaxed);
				if (pOp->m_bZeroCopy)
				{
					mp_pIo->m_UringStats.m_nSendZcOps.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
					mp_pIo->m_UringStats.m_SendZcSizeBuckets[fg_Min(umint(fg_GetHighestBitSet(nSendBytes)), umint(32))].f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
				}
				else
					mp_pIo->m_UringStats.m_SendSizeBuckets[fg_Min(umint(fg_GetHighestBitSet(nSendBytes)), umint(32))].f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
			}
#endif

			Sqe.m_Opcode = pOp->m_bZeroCopy ? gc_IoUringOp_SendMsgZc : gc_IoUringOp_SendMsg;
			Sqe.m_Fd = pRegistration->m_Handle;
			Sqe.m_Addr = (uint64)(umint)&pOp->m_MsgHdr;
			Sqe.m_Len = 1;
			// With one kernel send per descriptor, MSG_WAITALL can finish short writes without reordering.
			// Cancellation still reports bytes already placed, preserving the truncation point.
			Sqe.m_OpFlags = MSG_NOSIGNAL | MSG_WAITALL;

			// Zero-copy notification can outlive registration, so user data names the operation.
			Sqe.m_UserData = (uint64)(umint)pOp | gc_UringTag_SendOp;

			++pRegistration->m_nOutstanding;
		}

		// Ahead of anything submitted while the pass ran: a deferred operation is older than
		// those, and a registration's sends leave in submission order
		if (!Deferred.f_IsEmpty())
		{
			DMibLock(mp_IoOpLock);
			Deferred.f_Insert(fg_Move(mp_PendingIoOps));
			mp_PendingIoOps = fg_Move(Deferred);
		}
	}

	// Dispatched work above may have enqueued for this thread, so a blocking pass must hand
	// control back instead of parking on top of it
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

	if (bBlock && mp_pParkEvent && !mp_bFutexArmed)
	{
		// Wait on the queue event's zero count so normal job signals wake the ring too. An existing token fails the arm and returns this pass.
		if (mp_pParkEvent->f_TryWait())
		{
			// A pending token means work was signaled while nothing was armed; hand control back
			mp_WakeState.f_Store(0, NAtomic::gc_MemoryOrder_Release);
			bBlock = false;
		}
		else
		{
			mp_pParkEvent->f_ExternalWaiterRegister();

			CIoUringSqe &Sqe = fp_PrepareSqe();
			Sqe.m_Opcode = gc_IoUringOp_FutexWait;
			Sqe.m_Fd = (int32)(gc_IoUringFutex2_SizeU32 | gc_IoUringFutex2_Private);
			Sqe.m_Addr = (uint64)(umint)mp_pParkEvent->f_GetExternalFutexWord();
			Sqe.m_Off = 0;
			Sqe.m_Addr3 = gc_IoUringFutex_BitsetMatchAny;
			Sqe.m_UserData = gc_UringUserData_Futex;

			mp_bFutexArmed = true;
		}
	}

	fp_PlaceBatch(nReported);

	// Reaping to make ring room can enqueue local work; abandon parking after any dispatch.
	if (bBlock && nReported)
	{
		mp_WakeState.f_Store(0, NAtomic::gc_MemoryOrder_Release);
		bBlock = false;
	}

	if (bBlock)
	{
		[[maybe_unused]] int SubmitRet = mp_Ring.f_Submit(1, true);

		// A busy result means the completion ring overflowed and the reap below drains it;
		// anything else failing here would spin the parking loop at full speed
		DMibFastCheck(SubmitRet >= 0 || SubmitRet == -EBUSY);

		mp_WakeState.f_Store(0, NAtomic::gc_MemoryOrder_Release);
	}
	else if (mp_Ring.m_nPendingSubmit)
		mp_Ring.f_Submit(0, false);

	fp_ReapAll(nReported);

	// Rearms prepared during the reap go to the kernel now rather than at the next pass
	fp_PlaceBatch(nReported);
	if (mp_Ring.m_nPendingSubmit)
		mp_Ring.f_Submit(0, false);

	return nReported;
}

void CIoLoop_IoUring::f_DrainForShutdown()
{
	fp_Iterate(false);

	// Cancellation is multistage; keep iterating until every deregistration settles.
	while (mp_nDeregistering)
		fp_Iterate(true);
}

void CIoLoop_IoUring::f_SetParkEvent(NThread::CEventAutoReset *_pEvent)
{
	if (mp_bRingCreated && mp_pIo->m_UringCaps.m_bFutexWait)
		mp_pParkEvent = _pEvent;
}

bool CIoLoop_IoUring::f_ParksOnQueueEvent() const
{
	return mp_pParkEvent != nullptr;
}

bool CIoLoop_IoUring::f_SupportsCompletionIo() const
{
	return mp_bRingCreated && mp_pIo->m_UringCaps.m_bCompletion;
}

bool CIoLoop_IoUring::f_SupportsReceiveStream() const
{
	return mp_bRingCreated && mp_pIo->m_UringCaps.m_bReceiveStream;
}
