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

	// The word is the coalescer: whoever turns it nonzero owns the queue notification, and every
	// later request before the loop consumes it only accumulates bits. Consumption is a single
	// exchange on the loop's thread, so a bit is either taken by that exchange or its requester
	// found the word zero and pushed a fresh notification
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

void CIoLoop_IoUring::fp_ArmPoll(CUringRegistration *_pRegistration, uint64 _Tag, uint32 _PollMask)
{
	// Single shot and level at arm: readiness that already exists completes the poll immediately,
	// which is what makes request-after-would-block lossless with no separate probe. Spin is
	// impossible because a poll is only armed on a fresh request, which only follows a would-block
	// observation
	CIoUringSqe &Sqe = fp_PrepareSqe();
	Sqe.m_Opcode = gc_IoUringOp_PollAdd;
	Sqe.m_Fd = _pRegistration->m_Handle;
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

// Cancel by user data is exact: each obligation is submitted under a value of its own, produces
// exactly one result whatever the outcome, and the cancel itself is one more counted CQE. Every
// result is acceptable — found-and-cancelled, already completing, or already reaped — so no
// outcome needs a retry.
//
// The target is passed whole rather than built from a tag, because a zero copy send is submitted
// under its operation's address instead of this registration's
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
	if (_pRegistration->m_ClosePoll == CUringRegistration::EClosePoll::mc_ArmedRdHup || _pRegistration->m_ClosePoll == CUringRegistration::EClosePoll::mc_ArmedHupOnly)
		fp_PrepareCancel(_pRegistration, Base | gc_UringTag_ClosePoll);

	if (_pRegistration->m_pSendOp)
		fp_PrepareCancel(_pRegistration, (uint64)(umint)_pRegistration->m_pSendOp | gc_UringTag_SendOp);
	else if (_pRegistration->m_pSendRing)
	{
		// Published transfers with no operation in flight have nothing kernel side to cancel;
		// they are failed right here or nothing ever would
		fp_FailSendRecords(_pRegistration, NSys::EIoCompletionStatus::mc_Cancelled, 0, _nReported);
	}

	// An armed stream terminates through the cancel's completion; one waiting for buffers has
	// nothing with the kernel, so its terminal segment is delivered right here — the sink is owed
	// exactly one either way
	if (_pRegistration->m_bStreamArmed)
		fp_PrepareCancel(_pRegistration, Base | gc_UringTag_RecvStream);
	else if (_pRegistration->m_fStreamSink && !_pRegistration->m_bStreamEnded)
		fp_EndStream(_pRegistration, NSys::EIoCompletionStatus::mc_Cancelled, 0, _nReported);
}

void CIoLoop_IoUring::fp_SweepPendingOps(CUringRegistration *_pRegistration)
{
	// A completion submission can land in the pending queue from another thread after a pass has
	// already moved the queue for processing: the submit strictly precedes the removal in the io
	// object owner's order, but an actor migrating between the two jobs can put them on opposite
	// sides of one iterate. Swept here, before the registration is freed, such an operation is
	// cancelled while its registration pointer is still unambiguous
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

	// The count is zero, so no CQE anywhere names this registration and freeing it is safe. The
	// acknowledgement counts as dispatched work: its continuation can enqueue locally, and a
	// pass that reports nothing could otherwise park past the enqueue
	fp_SweepPendingOps(_pRegistration);
	fp_ReleaseStream(_pRegistration);
	fp_ReleaseSendRing(_pRegistration);
	--mp_nDeregistering;

#if DMibConfig_IoDebug_Enable
	if (fg_UringTraceEnabled())
		fg_UringTrace("ack", _pRegistration->m_pToken, _pRegistration->m_Handle, 0);
#endif

	CIoLoopDeferredAck Ack{_pRegistration, _pRegistration->m_pDeregWait, fg_Move(_pRegistration->m_fOnDeregistered)};
	fg_RunDeregAcknowledgement(Ack);
	++_nReported;
}

umint CIoLoop_IoUring::fp_Iterate(bool _bBlock)
{
	umint nReported = 0;

	// First use on the driving thread claims single-issuer ownership of the ring, which was
	// created disabled on whichever thread constructed the loop
	if (mp_Ring.m_bNeedsEnable) [[unlikely]]
	{
		if (!mp_Ring.f_EnableRings())
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("io_uring enable rings (io loop)", errno));
	}

	{
		auto Changes = fg_Move(mp_ChangeQueue.f_Take());
		for (auto &Change : Changes)
		{
			if (Change.m_bRemove)
			{
				auto *pRegistration = static_cast<CUringRegistration *>(Change.m_pRegistration);

				// Requests raced ahead of the removal in the owner's order are moot: the
				// consumer stops caring before it requests removal
				pRegistration->m_RequestedEvents.f_Exchange(0, NAtomic::gc_MemoryOrder_AcquireRelease);
				pRegistration->m_bDeregistering = true;
				++mp_nDeregistering;
				pRegistration->m_pDeregWait = Change.m_pDeregWait;
				pRegistration->m_fOnDeregistered = fg_Move(Change.m_fOnDeregistered);
#if DMibConfig_IoDebug_Enable
				if (fg_UringTraceEnabled())
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

				// The notification can outlive its usefulness — the direction flipped to
				// completion transfers, or the removal was queued behind this entry (never
				// ahead: the owner requests before it removes) — in which case those bits are
				// consumed and dropped. Each direction is dropped on its own, so a socket
				// driven by submitted operations one way still arms readiness the other
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
				if (fg_UringTraceEnabled())
					fg_UringTrace("register", pRegistration->m_pToken, Change.m_Handle, uint32(pRegistration->m_EventMask));
#endif

				// The implicit initial request seeded by f_Register: level-at-arm polls report
				// pre-registration readiness by completing immediately, so nothing needs a
				// probe. The close poll stands from the start, so peer death reaches idle and
				// completion-mode windows too
				NSys::EIoLoopEvent Requested = NSys::EIoLoopEvent(pRegistration->m_RequestedEvents.f_Exchange(0, NAtomic::gc_MemoryOrder_AcquireRelease));
				fp_ArmRequested(pRegistration, Requested);
				pRegistration->m_ClosePoll = CUringRegistration::EClosePoll::mc_ArmedRdHup;
				fp_ArmPoll(pRegistration, gc_UringTag_ClosePoll, EPOLLRDHUP);

				if (Change.m_bNotifyRegistered)
				{
					// The callback is dispatched work: it can enqueue locally, and a pass that
					// reports nothing could park past the enqueue. Pinned like every dispatch
					++pRegistration->m_nOutstanding;
					pRegistration->m_fOnEvents(pRegistration->m_pToken, NSys::EIoLoopEvent::mc_None, 0);
					--pRegistration->m_nOutstanding;
					++nReported;
				}
			}
		}
	}

	// Queued completion transfers enter the ring after the registration changes, so an operation
	// for a registration added in this same pass finds it, and one for a registration whose
	// removal was processed above is completed as cancelled here instead of entering the kernel.
	// The registration pointer is safe to follow: the owner queues operations before it queues
	// the removal, and the acknowledgement that frees a registration sweeps this queue first
	{
		NContainer::TCVector<CUringIoOp *> IoOps;
		{
			DMibLock(mp_IoOpLock);
			IoOps = fg_Move(mp_PendingIoOps);
		}

		// A send whose descriptor already has one with the kernel waits in the queue: one send in
		// flight per descriptor is the whole ordering story, and the wire stays busy anyway
		// because a completed send's data keeps transmitting from the TCP queue while the next is
		// submitted
		NContainer::TCVector<CUringIoOp *> Deferred;

		for (CUringIoOp *pOp : IoOps)
		{
			auto *pRegistration = pOp->m_pRegistration;

			if (pRegistration->m_bDeregistering)
			{
				// The registration left the ring before the operation could enter it, so nothing
				// kernel side references the buffers and the cancellation is reported right here,
				// still on the loop's thread as the contract promises. A resume message carries
				// no functor and is simply moot
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
				continue;
			}

			// The first operation of a direction flips that direction to completion transfers:
			// its readiness poll is cancelled — every arriving segment would otherwise fire a
			// readiness edge nobody consumes — while the other direction is left alone and the
			// standing close poll keeps reporting peer death. One exact cancel per armed poll,
			// no retry protocol
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
				if (fg_UringTraceEnabled())
					fg_UringTrace(pOp->m_bStreamStart ? "completion-mode-read" : "completion-mode-write", pRegistration->m_pToken, pRegistration->m_Handle, 0);
#endif
			}

			if (pOp->m_bStreamResume)
			{
				// A backpressure release rescheduling the stream; moot for one already gone
				if (pRegistration->m_pRecvRing)
					fp_ResumeStream(pRegistration);

				fg_DeleteObject(NMemory::CDefaultAllocator(), pOp);
				continue;
			}

			if (pOp->m_bStreamStart)
			{
				// A message, not an operation: build the ring, install the sink, arm the
				// standing receive
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

			// The peer settles the path once, at the first send: a remote peer keeps the
			// single-operation path so zero copy stays available; a local peer's transfers are
			// published into the bundle ring, where one operation drains everything published
			// at its issue and nothing waits on a caller between operations
			fg_UringProbePeerClass(pRegistration);

			if (!pRegistration->m_bZeroCopyEligible)
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
			if (fg_UringStatsEnabled() && pOp->m_EnqueueStamp)
			{
				mp_pIo->m_UringStats.m_nSendSubmitLagNs.f_FetchAdd(fg_UringStatsNow() - pOp->m_EnqueueStamp, NAtomic::gc_MemoryOrder_Relaxed);
				mp_pIo->m_UringStats.m_nSendSubmitLagOps.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
			}

			if (fg_UringStatsEnabled() && pRegistration->m_SendIdleStamp)
			{
				mp_pIo->m_UringStats.m_nSendIdleNs.f_FetchAdd(fg_UringStatsNow() - pRegistration->m_SendIdleStamp, NAtomic::gc_MemoryOrder_Relaxed);
				mp_pIo->m_UringStats.m_nSendIdleGaps.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
				pRegistration->m_SendIdleStamp = 0;
			}
#endif

			umint nSendBytes = 0;
			for (umint iVector = 0; iVector < (umint)pOp->m_MsgHdr.msg_iovlen; ++iVector)
				nSendBytes += pOp->m_IoVecs[iVector].iov_len;

#if DMibConfig_IoDebug_Enable
			pOp->m_nRequested = nSendBytes;
#endif
			pOp->m_bZeroCopy = fp_IsZeroCopyEligible(pRegistration, nSendBytes);

#if DMibConfig_IoDebug_Enable
			if (fg_UringStatsEnabled())
			{
				mp_pIo->m_UringStats.m_nSendOps.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
				mp_pIo->m_UringStats.m_nSendBytesRequested.f_FetchAdd(nSendBytes, NAtomic::gc_MemoryOrder_Relaxed);
				if (pOp->m_bZeroCopy)
				{
					mp_pIo->m_UringStats.m_nSendZcOps.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
					mp_pIo->m_UringStats.m_SendZcSizeBuckets[fg_GetHighestBitSet(nSendBytes)].f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
				}
				else
					mp_pIo->m_UringStats.m_SendSizeBuckets[fg_GetHighestBitSet(nSendBytes)].f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
			}
#endif

			Sqe.m_Opcode = pOp->m_bZeroCopy ? gc_IoUringOp_SendMsgZc : gc_IoUringOp_SendMsg;
			Sqe.m_Fd = pRegistration->m_Handle;
			Sqe.m_Addr = (uint64)(umint)&pOp->m_MsgHdr;
			Sqe.m_Len = 1;
			// The kernel runs the retry loop itself: with exactly one operation ever in flight
			// there is no successor a retry could race, and finishing the buffer in-kernel is
			// what keeps the pipe from draining for a completion round trip per remainder — a
			// short send would report back through the loop and the actor before the rest could
			// even be offered again. A cancelled operation still reports the bytes it placed,
			// so a teardown's truncation point stays known
			Sqe.m_OpFlags = MSG_NOSIGNAL | MSG_WAITALL;

			// Named by the operation rather than the registration: a zero copy send's
			// notification shares this value with its result and outlives the registration
			Sqe.m_UserData = (uint64)(umint)pOp | gc_UringTag_SendOp;

			++pRegistration->m_nOutstanding;
		}

		if (!Deferred.f_IsEmpty())
		{
			DMibLock(mp_IoOpLock);
			for (CUringIoOp *pOp : Deferred)
				mp_PendingIoOps.f_Insert(pOp);
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
		// The queue event's park rides in the ring: a futex wait on the event's count word makes
		// the wake a job signal already performs reach this blocking wait, so the signal path
		// pays nothing extra for a loop-hosting queue. Waiting while the word is zero mirrors the
		// event's own waiters; a token that is already there fails the arm at the kernel's value
		// check and wakes this pass instead
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

	// Deregistration settles through completions — cancels and the obligations they terminate —
	// so blocking iterates make progress until no registration is still on its way out
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

void CIoLoop_IoUring::f_AbandonPendingTeardown()
{
	// Every pool thread is joined and the exiting owner drained the loop to quiescence, so no
	// registration is mid-deregistration; only the userspace obligations matter — continuations
	// of removals still queued, and completion functors of operations that never reached the
	// kernel. The ring is destroyed wholesale
	DMibFastCheck(mp_nDeregistering == 0);

	auto Changes = fg_Move(mp_ChangeQueue.f_Take());
	for (auto &Change : Changes)
	{
		if (!Change.m_bRemove)
			continue;

		CIoLoopDeferredAck Ack{Change.m_pRegistration, Change.m_pDeregWait, fg_Move(Change.m_fOnDeregistered)};
		fg_RunDeregAcknowledgement(Ack);
	}

	NContainer::TCVector<CUringIoOp *> IoOps;
	{
		DMibLock(mp_IoOpLock);
		IoOps = fg_Move(mp_PendingIoOps);
	}
	for (CUringIoOp *pOp : IoOps)
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

	// Zero copy sends whose notification never came. Their results were reported long ago and
	// their registrations are gone; what is left is the send buffer's keep alive, which the ring
	// going away with them is what finally releases. Destroying the ring is what makes this safe:
	// no completion can name them afterwards
	for (CUringIoOp *pOp : mp_NotifyPending)
	{
		pOp->m_iNotifyPending = ~umint(0);
		if (pOp->m_fOnBufferReleased)
			pOp->m_fOnBufferReleased();
		fg_DeleteObject(NMemory::CDefaultAllocator(), pOp);
	}

	mp_NotifyPending.f_Clear();
}

bool CIoLoop_IoUring::f_SupportsCompletionIo() const
{
	return mp_bRingCreated && mp_pIo->m_UringCaps.m_bCompletion;
}

bool CIoLoop_IoUring::f_SupportsReceiveStream() const
{
	return mp_bRingCreated && mp_pIo->m_UringCaps.m_bReceiveStream;
}
