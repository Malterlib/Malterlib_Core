// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Linux_IoSubSystem.h"
#include <Mib/Time/Stopwatch>

#include "Malterlib_Core_Platform_Linux_IoLoop_Uring_Internal.h"
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

void CIoLoop_IoUring::fp_PlaceBatch(umint &_nReported)
{
	// Only a full SQ forces an early enter. Reap on EBUSY; pending entries must be fully built before callbacks can run.
	for (;;)
	{
		while (mp_iNextSqe < mp_PendingSqes.f_GetLen())
		{
			CIoUringSqe *pSqe = mp_Ring.f_GetSqe();
			if (!pSqe)
				break;

			*pSqe = mp_PendingSqes[mp_iNextSqe];
			++mp_iNextSqe;
		}

		if (mp_iNextSqe == mp_PendingSqes.f_GetLen())
		{
			mp_PendingSqes.f_Clear();
			mp_iNextSqe = 0;

			return;
		}

		int SubmitResult = mp_Ring.f_Submit(0, false);
		if (SubmitResult < 0)
		{
			if (SubmitResult != -EBUSY && SubmitResult != -EAGAIN)
				DMibErrorNet(NMib::NPlatform::fg_FormatErrno("io_uring submit (io loop)", -SubmitResult));

			fp_ReapAll(_nReported);
		}
	}
}

void CIoLoop_IoUring::fp_ReapAll(umint &_nReported)
{
	for (;;)
	{
		CIoUringCqe *pCqe = mp_Ring.f_PeekCqe();
		if (!pCqe)
		{
			// GETEVENTS flushes kernel overflow into an empty CQ; no progress after flushing is an invariant failure.
			if (!mp_Ring.f_CqOverflowPending())
				break;

			mp_Ring.f_Submit(0, true);

			pCqe = mp_Ring.f_PeekCqe();
			if (!pCqe)
			{
				DMibFastCheck(false);
				break;
			}
		}

		uint64 UserData = pCqe->m_UserData;
		int32 Res = pCqe->m_Res;
		uint32 Flags = pCqe->m_Flags;
		mp_Ring.f_AdvanceCq();

		fp_DispatchCqe(UserData, Res, Flags, _nReported);
	}

	fp_FlushStreamSegments(_nReported);
}

void CIoLoop_IoUring::fp_DispatchCqe(uint64 _UserData, int32 _Res, uint32 _Flags, umint &_nReported)
{
	if (_UserData < gc_UringUserData_LoopOwnLimit)
	{
		if (_UserData == gc_UringUserData_Futex)
		{
			// End waiter registration after consuming the token; later signals leave a token for the next park.
			mp_bFutexArmed = false;
			mp_pParkEvent->f_ExternalWaiterUnregister();
			mp_pParkEvent->f_TryWait();
			++_nReported;
		}
		else if (_UserData == gc_UringUserData_Pipe)
		{
			char Buf[16];
			int ReadRet;
			do
			{
				ReadRet = read(mp_ReadWritePipe[0], Buf, sizeof(Buf));
			}
			while (ReadRet > 0)
				;

			if (!(_Flags & gc_IoUringCqe_FMore))
			{
				CIoUringSqe &Sqe = fp_PrepareSqe();
				Sqe.m_Opcode = gc_IoUringOp_PollAdd;
				Sqe.m_Fd = mp_ReadWritePipe[0];
				Sqe.m_OpFlags = EPOLLIN;
				Sqe.m_Len = gc_IoUringPoll_AddMulti;
				Sqe.m_UserData = gc_UringUserData_Pipe;
			}
		}

		return;
	}

	uint64 Tag = _UserData & gc_UringUserData_TagMask;

	// Send tags name operations, not registrations. Their release notification can arrive after registration destruction.
	if (Tag == gc_UringTag_SendOp)
	{
		auto *pOp = (CUringIoOp *)(umint)(_UserData & ~gc_UringUserData_TagMask);

		if (_Flags & gc_IoUringCqe_FNotif)
		{
#if DMibConfig_IoDebug_Enable
			if (mp_pIo->f_StatsEnabled())
				mp_pIo->m_UringStats.m_nSendNotifs.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif

			fp_ReleaseNotifyPending(pOp);
			if (pOp->m_fOnBufferReleased)
				pOp->m_fOnBufferReleased();
			fg_DeleteObject(NMemory::CDefaultAllocator(), pOp);
			++_nReported;

			return;
		}

		auto *pSendRegistration = pOp->m_pRegistration;

		// A bundle imports at most 256 entries per pass (PEEK_MAX_IMPORT). F_MORE retains the request; only the final CQE retires it.
		if (pOp->m_bBundle && (_Flags & gc_IoUringCqe_FMore))
		{
			DMibFastCheck(_Res > 0);
			if (_Res > 0)
			{
#if DMibConfig_IoDebug_Enable
				if (mp_pIo->f_StatsEnabled())
					mp_pIo->m_UringStats.m_nSendBytesSent.f_FetchAdd((uint64)(uint32)_Res, NAtomic::gc_MemoryOrder_Relaxed);
#endif

				fp_CoverSendRecords(pSendRegistration, (umint)(uint32)_Res, _nReported);
			}

			return;
		}

		DMibCheck(pSendRegistration->m_nOutstanding != 0);
		--pSendRegistration->m_nOutstanding;

		DMibFastCheck(pSendRegistration->m_pSendOp == pOp);
		pSendRegistration->m_pSendOp = nullptr;

		// Rearm only entries the completed request did not consume, preserving publish order.
		if (pOp->m_bBundle)
		{
			CUringSendRing *pSendRing = pSendRegistration->m_pSendRing;
			fg_DeleteObject(NMemory::CDefaultAllocator(), pOp);

			if (_Res >= 0 || _Res == -ENOBUFS)
			{
				if (_Res > 0)
				{
#if DMibConfig_IoDebug_Enable
					if (mp_pIo->f_StatsEnabled())
						mp_pIo->m_UringStats.m_nSendBytesSent.f_FetchAdd((uint64)(uint32)_Res, NAtomic::gc_MemoryOrder_Relaxed);
#endif

					fp_CoverSendRecords(pSendRegistration, (umint)(uint32)_Res, _nReported);
				}

				// Missing bytes from consumed entries cannot be sent by rearming. Unconsumed entries
				// can remain after a normal early completion caused by CQ pressure.
				if (int32 Error = fp_GetSendBundleError(pSendRegistration))
				{
#if DMibConfig_IoDebug_Enable
					if (mp_pIo->f_StatsEnabled())
						mp_pIo->m_UringStats.m_nSendShort.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif

					fp_FailSendRecords(pSendRegistration, NSys::EIoCompletionStatus::mc_Error, Error, _nReported);
				}
			}
			else
			{
				// The connection is over for sends
#if DMibConfig_IoDebug_Enable
				if (mp_pIo->f_StatsEnabled() && _Res != -ECANCELED)
					mp_pIo->m_UringStats.m_nSendErrors.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif

				fp_FailSendRecords
					(
						pSendRegistration
						, _Res == -ECANCELED ? NSys::EIoCompletionStatus::mc_Cancelled : NSys::EIoCompletionStatus::mc_Error
						, _Res == -ECANCELED ? 0 : -_Res
						, _nReported
					)
				;
			}

			if (pSendRegistration->m_bDeregistering)
				fp_FailSendRecords(pSendRegistration, NSys::EIoCompletionStatus::mc_Cancelled, 0, _nReported);

			if (pSendRing && !pSendRing->m_Records.f_IsEmpty() && !pSendRegistration->m_bDeregistering)
				fp_ArmSendBundle(pSendRegistration);
#if DMibConfig_IoDebug_Enable
			else if (mp_pIo->f_StatsEnabled())
				pSendRegistration->m_SendIdleStamp = fg_UringStatsNow();
#endif

			fp_TryAcknowledge(pSendRegistration, _nReported);

			return;
		}

#if DMibConfig_IoDebug_Enable
		if (mp_pIo->f_StatsEnabled())
			pSendRegistration->m_SendIdleStamp = fg_UringStatsNow();
#endif

		NSys::CIoCompletion Result;
		if (_Res == -ECANCELED)
			Result.m_Status = NSys::EIoCompletionStatus::mc_Cancelled;
		else if (_Res < 0)
		{
			Result.m_Status = NSys::EIoCompletionStatus::mc_Error;
			Result.m_Error = -_Res;
		}
		else if ((umint)(uint32)_Res < pOp->m_nRequested)
		{
			// MSG_WAITALL short completion means failure or partial cancellation. Report Error with the bytes placed; Done promises the whole scheduled range.
			Result.m_nBytes = (umint)(uint32)_Res;
			Result.m_Status = NSys::EIoCompletionStatus::mc_Error;
			Result.m_Error = ECONNRESET;
#if DMibConfig_IoDebug_Enable
			if (mp_pIo->f_StatsEnabled())
				mp_pIo->m_UringStats.m_nSendShort.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif
		}
		else
			Result.m_nBytes = (umint)(uint32)_Res;

#if DMibConfig_IoDebug_Enable
		if (mp_pIo->f_StatsEnabled())
		{
			if (Result.m_Status == NSys::EIoCompletionStatus::mc_Done)
				mp_pIo->m_UringStats.m_nSendBytesSent.f_FetchAdd((uint64)(uint32)_Res, NAtomic::gc_MemoryOrder_Relaxed);
			else if (Result.m_Status == NSys::EIoCompletionStatus::mc_Error)
				mp_pIo->m_UringStats.m_nSendErrors.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		}
#endif

		++pSendRegistration->m_nOutstanding;
		pOp->m_fOnComplete(Result);
		--pSendRegistration->m_nOutstanding;
		++_nReported;

		// Only promised notifications retain the operation; otherwise release buffers immediately after reporting completion.
		pOp->m_pRegistration = nullptr;

		if (_Flags & gc_IoUringCqe_FMore)
		{
			// Sample only low occupancy on this registration; otherwise latency measures its own queue and feeds unbounded growth.
			if (pSendRegistration->m_nNotifyPending <= 1)
				pOp->m_ReleaseLagIssueStamp = uint64(NTime::NPlatform::fg_TimerRaw_PreciseGet());

			pOp->m_pNotifyRegistration = pSendRegistration;
			++pSendRegistration->m_nNotifyPending;

			pOp->m_iNotifyPending = mp_NotifyPending.f_GetLen();
			mp_NotifyPending.f_InsertLast(pOp);
			mp_nNotifyPendingBytes += pOp->m_nRequested;
#if DMibConfig_IoDebug_Enable
			if (mp_pIo->f_StatsEnabled())
			{
				pOp->m_EnqueueStamp = fg_UringStatsNow();
				if (mp_NotifyPending.f_GetLen() > mp_pIo->m_UringStats.m_nSendMaxInFlight.f_Load(NAtomic::gc_MemoryOrder_Relaxed))
					mp_pIo->m_UringStats.m_nSendMaxInFlight.f_Store(mp_NotifyPending.f_GetLen(), NAtomic::gc_MemoryOrder_Relaxed);
				if (mp_nNotifyPendingBytes > mp_pIo->m_UringStats.m_nSendMaxBytesInFlight.f_Load(NAtomic::gc_MemoryOrder_Relaxed))
					mp_pIo->m_UringStats.m_nSendMaxBytesInFlight.f_Store(mp_nNotifyPendingBytes, NAtomic::gc_MemoryOrder_Relaxed);
			}
#endif
		}
		else
		{
			if (pOp->m_fOnBufferReleased)
				pOp->m_fOnBufferReleased();
			fg_DeleteObject(NMemory::CDefaultAllocator(), pOp);
		}

		fp_TryAcknowledge(pSendRegistration, _nReported);

		return;
	}

	// A multishot receive retains one obligation until a CQE without F_MORE.
	if (Tag == gc_UringTag_RecvStream)
	{
		auto *pStreamRegistration = (CUringRegistration *)(umint)(_UserData & ~gc_UringUserData_TagMask);
		bool bMore = (_Flags & gc_IoUringCqe_FMore) != 0;

		if (!bMore)
		{
			DMibCheck(pStreamRegistration->m_nOutstanding != 0);
			--pStreamRegistration->m_nOutstanding;
			pStreamRegistration->m_bStreamArmed = false;
		}

		if (_Res > 0)
		{
			// Bundle CQEs name only the first buffer. Attribute bytes in publish order, with only the tail partially filled.
			// Retired buffers stay with consumers; backpressure may delay publishing replacements.
			DMibFastCheck(_Flags & gc_IoUringCqe_FBuffer);

			CUringRecvRing *pRing = pStreamRegistration->m_pRecvRing;
			[[maybe_unused]] uint16 Bid = (uint16)(_Flags >> gc_IoUringCqe_BufferShift);
			DMibFastCheck(pRing && Bid < pRing->m_nBuffers);
			DMibFastCheck(pRing->m_nOrderCount != 0 && pRing->f_OrderFront() == Bid);

#if DMibConfig_IoDebug_Enable
			if (mp_pIo->f_StatsEnabled())
			{
				mp_pIo->m_UringStats.m_nRecvSegments.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
				mp_pIo->m_UringStats.m_nRecvBytes.f_FetchAdd((uint64)(uint32)_Res, NAtomic::gc_MemoryOrder_Relaxed);
				mp_pIo->m_UringStats.m_RecvSizeBuckets[fg_GetHighestBitSet((umint)(uint32)_Res)].f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
			}
#endif

			umint nRemaining = (umint)(uint32)_Res;
			while (nRemaining)
			{
				DMibFastCheck(pRing->m_nOrderCount != 0);

				uint16 ChunkBid = pRing->f_OrderFront();
				umint Offset = pRing->m_SlotOffsets[ChunkBid];
				umint nRoom = pRing->m_nBufferBytes - Offset;
				umint nChunk = fg_Min(nRemaining, nRoom);

				NStorage::TCSharedPointer<CUringStreamBuffer> pBuffer = pRing->m_BufferSlots[ChunkBid];
				DMibFastCheck(pBuffer);

				// The capacity is charged once per buffer, with its first delivered bytes: it
				// is pinned from then on until every segment referencing it has been released
				if (!Offset)
				{
					pBuffer->m_nCharged = pRing->m_nBufferBytes;
					if (pRing->m_pBackpressure)
						pRing->m_pBackpressure->m_nOutstandingBytes.f_FetchAdd(pBuffer->m_nCharged, NAtomic::gc_MemoryOrder_AcquireRelease);
				}

				NSys::CIoStreamSegment Segment;
				Segment.m_pData = pBuffer->m_pData + Offset;
				Segment.m_nBytes = nChunk;
				Segment.m_pOwner = pBuffer.f_ShareAsConst();
				pBuffer.f_Clear();

				// F_BUF_MORE describes only the first named buffer in a bundle. Retire by filled length;
				// a partial tail remains kernel-owned across termination and rearm.
				bool bBufferRetired = nChunk == nRoom;
				if (bBufferRetired)
				{
					pRing->f_OrderPop();
					pRing->m_BufferSlots[ChunkBid].f_Clear();
					pRing->m_SlotOffsets[ChunkBid] = 0;
				}
				else
					pRing->m_SlotOffsets[ChunkBid] = Offset + nChunk;

				// Merge adjacent arrivals to reduce actor hops; flush before a nonadjacent segment and when reaping ends.
				CIoStreamSegment &Pending = pStreamRegistration->m_PendingStreamSegment;
				if
				(
					pStreamRegistration->m_bStreamSegmentPending
					&& Pending.m_pOwner == Segment.m_pOwner
					&& (uint8 const *)Pending.m_pData + Pending.m_nBytes == (uint8 const *)Segment.m_pData
				)
				{
					Pending.m_nBytes += Segment.m_nBytes;
				}
				else
				{
					fp_FlushStreamSegment(pStreamRegistration, _nReported);

					pStreamRegistration->m_PendingStreamSegment = fg_Move(Segment);
					pStreamRegistration->m_bStreamSegmentPending = true;
					mp_StreamFlushQueue.f_Insert(pStreamRegistration);
				}

				if (bBufferRetired)
					fp_RefillBid(pStreamRegistration, ChunkBid);

				nRemaining -= nChunk;
			}
		}

		if (!bMore)
		{
			if (_Res == -ENOBUFS)
			{
#if DMibConfig_IoDebug_Enable
				if (mp_pIo->f_StatsEnabled())
					mp_pIo->m_UringStats.m_nStreamEnobufs.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif

				// ENOBUFS can be a transient burst or backpressure. Rearm refilled rings; parked streams await resume, while deregistering streams owe a terminal.
				CUringRecvRing *pDryRing = pStreamRegistration->m_pRecvRing;

				if (pStreamRegistration->m_bDeregistering)
					fp_EndStream(pStreamRegistration, NSys::EIoCompletionStatus::mc_Cancelled, 0, _nReported);
				else if (pDryRing && pDryRing->m_UnfilledBids.f_IsEmpty())
					fp_ArmStream(pStreamRegistration);
				else
				{
					pStreamRegistration->m_bStreamNeedsRearm = true;

#if DMibConfig_IoDebug_Enable
					if (mp_pIo->f_TraceEnabled())
						fg_UringTrace("stream-dry", pStreamRegistration->m_pToken, pStreamRegistration->m_Handle, 0);
#endif
				}
			}
			else if (_Res > 0)
			{
				// A data-carrying terminal CQE can outrun cancellation. If deregistering, deliver the stream terminal now; no later CQE will do it.
				if (!pStreamRegistration->m_bStreamEnded && !pStreamRegistration->m_bDeregistering)
					fp_ArmStream(pStreamRegistration);
				else if (pStreamRegistration->m_bDeregistering)
					fp_EndStream(pStreamRegistration, NSys::EIoCompletionStatus::mc_Cancelled, 0, _nReported);
			}
			else if (_Res == 0)
				fp_EndStream(pStreamRegistration, NSys::EIoCompletionStatus::mc_Done, 0, _nReported);
			else if (_Res == -ECANCELED)
				fp_EndStream(pStreamRegistration, NSys::EIoCompletionStatus::mc_Cancelled, 0, _nReported);
			else
			{
#if DMibConfig_IoDebug_Enable
				if (mp_pIo->f_StatsEnabled())
					mp_pIo->m_UringStats.m_nRecvErrors.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif

				fp_EndStream(pStreamRegistration, NSys::EIoCompletionStatus::mc_Error, -_Res, _nReported);
			}

			fp_TryAcknowledge(pStreamRegistration, _nReported);
		}

		return;
	}

	auto *pRegistration = (CUringRegistration *)(umint)(_UserData & ~gc_UringUserData_TagMask);

	// A multishot poll keeps its obligation while its CQEs carry the more flag
	DMibCheck(pRegistration->m_nOutstanding != 0);
	if (!(_Flags & gc_IoUringCqe_FMore))
		--pRegistration->m_nOutstanding;

	// Pin registration across consumer callbacks so callback-triggered teardown cannot free it in use.
	auto fDispatchEvents = [&](uint32 _PollBits)
		{
#if DMibConfig_IoDebug_Enable
			if (mp_pIo->f_TraceEnabled() && (_PollBits & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)))
				fg_UringTrace("close-event", pRegistration->m_pToken, pRegistration->m_Handle, _PollBits);
#endif

			++pRegistration->m_nOutstanding;
			pRegistration->m_fOnEvents(pRegistration->m_pToken, fg_IoLoopEventsFromPollBits(_PollBits), 0);
			--pRegistration->m_nOutstanding;
			++_nReported;
		}
	;

	switch (Tag)
	{
	case gc_UringTag_ReadPoll:
		pRegistration->m_bReadPollArmed = false;
		if (_Res >= 0)
			fDispatchEvents((uint32)_Res);
		else if (_Res != -ECANCELED)
			fDispatchEvents(EPOLLERR);

		break;

	case gc_UringTag_WritePoll:
		pRegistration->m_bWritePollArmed = false;
		if (_Res >= 0)
			fDispatchEvents((uint32)_Res);
		else if (_Res != -ECANCELED)
			fDispatchEvents(EPOLLERR);

		break;

	case gc_UringTag_ClosePoll:
		if (_Res == -ECANCELED)
			pRegistration->m_ClosePoll = CUringRegistration::EClosePoll::mc_NotArmed;
		else if (_Res >= 0)
		{
			fDispatchEvents((uint32)_Res);

			// The kernel ends a multishot poll when the completion queue overflows; a fresh one reports
			// the standing level once and then waits for the next wakeup, so this cannot spin
			if (!(_Flags & gc_IoUringCqe_FMore))
			{
				if (!pRegistration->m_bDeregistering)
					fp_ArmPoll(pRegistration, gc_UringTag_ClosePoll, EPOLLRDHUP, true);
				else
					pRegistration->m_ClosePoll = CUringRegistration::EClosePoll::mc_NotArmed;
			}
		}
		else
		{
			fDispatchEvents(EPOLLERR);
			pRegistration->m_ClosePoll = CUringRegistration::EClosePoll::mc_Terminal;
		}

		break;

	case gc_UringTag_Cancel:
		// Every cancel outcome leaves its target terminating with a CQE of its own; only the
		// count mattered
		break;

	default:
		DMibFastCheck(false);
		break;
	}

	fp_TryAcknowledge(pRegistration, _nReported);
}
