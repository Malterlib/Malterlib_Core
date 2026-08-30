// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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
	// Copies the prepared batch into free submission slots; the entries ride the pass's next
	// enter — fused with the park's blocking submit on a parking pass. Only a full submission
	// queue forces an early enter to make room, and completion pressure there (-EBUSY) is
	// answered by reaping in place — safe because prepared entries are complete when appended,
	// so nothing is ever half-built when callbacks run
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
			// The visible ring is dry; completions can still wait in the kernel-side overflow
			// backlog. An enter with get-events moves the backlog into the now-empty ring; no
			// progress after a flush attempt has no way forward and trips instead of spinning
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
			// The queue event fired, or a token already existed when the wait armed. Consuming
			// the token resets the auto reset event; the caller drains its queue when this pass
			// returns. The waiter registration ends here, so a signal landing after this skips
			// the futex wake and leaves a token the next park finds by itself
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

	// Taken before anything is read as a registration, because for this tag the user data is not
	// one: a send names its own operation. A zero copy send answers twice under a single value —
	// the result, which is the registration's obligation and is counted as one, and the
	// notification, which says the pages are free again and can arrive long after the
	// registration is gone
	if (Tag == gc_UringTag_SendOp)
	{
		auto *pOp = (CUringIoOp *)(umint)(_UserData & ~gc_UringUserData_TagMask);

		if (_Flags & gc_IoUringCqe_FNotif)
		{
#if DMibConfig_IoDebug_Enable
			if (fg_UringStatsEnabled())
				mp_pIo->m_UringStats.m_nSendNotifs.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif

			// The pages are released and the buffers may be reused, which is the released
			// functor's whole message. Deleting the operation gives up the send buffer's keep
			// alive; nothing else refers to it by now
			fp_ReleaseNotifyPending(pOp);
			if (pOp->m_fOnBufferReleased)
				pOp->m_fOnBufferReleased();
			fg_DeleteObject(NMemory::CDefaultAllocator(), pOp);
			++_nReported;

			return;
		}

		auto *pSendRegistration = pOp->m_pRegistration;
		DMibCheck(pSendRegistration->m_nOutstanding != 0);
		--pSendRegistration->m_nOutstanding;

		DMibFastCheck(pSendRegistration->m_pSendOp == pOp);
		pSendRegistration->m_pSendOp = nullptr;

		// The standing bundle: its bytes cover the record FIFO in publish order, and while
		// records remain it is re-issued in this same pass — the kernel snapshots the ring at
		// issue, so entries published while it flew wait for the re-arm, not for any caller
		if (pOp->m_bBundle)
		{
			CUringSendRing *pSendRing = pSendRegistration->m_pSendRing;
			fg_DeleteObject(NMemory::CDefaultAllocator(), pOp);

			if (_Res >= 0)
			{
#if DMibConfig_IoDebug_Enable
				if (fg_UringStatsEnabled())
					mp_pIo->m_UringStats.m_nSendBytesSent.f_FetchAdd((uint64)(uint32)_Res, NAtomic::gc_MemoryOrder_Relaxed);
#endif

				fp_CoverSendRecords(pSendRegistration, (umint)(uint32)_Res, _nReported);
			}
			else if (_Res != -ENOBUFS)
			{
				// The connection is over for sends; -ENOBUFS is only the arm racing an empty
				// ring and carries on below
#if DMibConfig_IoDebug_Enable
				if (fg_UringStatsEnabled() && _Res != -ECANCELED)
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
			else if (fg_UringStatsEnabled())
				pSendRegistration->m_SendIdleStamp = fg_UringStatsNow();
#endif

			fp_TryAcknowledge(pSendRegistration, _nReported);

			return;
		}

#if DMibConfig_IoDebug_Enable
		if (fg_UringStatsEnabled())
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
		else
			Result.m_nBytes = (umint)(uint32)_Res;

#if DMibConfig_IoDebug_Enable
		if (fg_UringStatsEnabled())
		{
			if (_Res >= 0)
			{
				mp_pIo->m_UringStats.m_nSendBytesSent.f_FetchAdd((uint64)(uint32)_Res, NAtomic::gc_MemoryOrder_Relaxed);
				if ((umint)(uint32)_Res < pOp->m_nRequested)
					mp_pIo->m_UringStats.m_nSendShort.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
			}
			else if (_Res != -ECANCELED)
				mp_pIo->m_UringStats.m_nSendErrors.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		}
#endif

		++pSendRegistration->m_nOutstanding;
		pOp->m_fOnComplete(Result);
		--pSendRegistration->m_nOutstanding;
		++_nReported;

		// The registration is done with it either way; what decides the operation's own fate is
		// whether a notification was promised. A send that promised none releases its buffers
		// right here — after the completion, as the contract orders them
		pOp->m_pRegistration = nullptr;

		if (_Flags & gc_IoUringCqe_FMore)
		{
			pOp->m_iNotifyPending = mp_NotifyPending.f_GetLen();
			mp_NotifyPending.f_InsertLast(pOp);
			mp_nNotifyPendingBytes += pOp->m_nRequested;
#if DMibConfig_IoDebug_Enable
			if (fg_UringStatsEnabled())
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

	// The standing receive: counted as one obligation while armed, so only a terminal completion
	// — one without more-to-come — consumes it. Data lands in a buffer the kernel picked from the
	// pool ring, named on the completion itself
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
			// Bytes arrived, bundled: one completion covers a run of published buffers. It
			// names only the first bid — consumption follows publish order, every buffer but
			// the last is filled to the end, and on an incremental ring a partially filled
			// tail stays with the kernel at its offset — so the walk below attributes the
			// bytes over the ring's own publish-order queue. Retired buffers leave the ring
			// for good: the segments carry them by reference up the stack, their capacity is
			// charged against the stream's accounting, and fresh buffers take the bids over —
			// unless the charge is what put the stream over its limit, in which case the bid
			// parks until a release brings it back
			DMibFastCheck(_Flags & gc_IoUringCqe_FBuffer);

			CUringRecvRing *pRing = pStreamRegistration->m_pRecvRing;
			[[maybe_unused]] uint16 Bid = (uint16)(_Flags >> gc_IoUringCqe_BufferShift);
			DMibFastCheck(pRing && Bid < pRing->m_nBuffers);
			DMibFastCheck(pRing->m_nOrderCount != 0 && pRing->f_OrderFront() == Bid);

#if DMibConfig_IoDebug_Enable
			if (fg_UringStatsEnabled())
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
				umint Offset = pRing->m_bIncremental ? pRing->m_SlotOffsets[ChunkBid] : 0;
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

				// A buffer filled to its end is done with the kernel whatever the ring mode;
				// a plain ring additionally retires a partial tail, discarding its remainder.
				// Deliberately not consulting the buf-more flag: on a bundled completion it
				// describes only the NAMED (first) bid, and the kernel retains a partially
				// filled tail regardless of it — probed byte-exact against the target kernel,
				// including partial tails surviving multishot termination and re-arm — so the
				// arithmetic here is the authoritative retirement rule, and a kernel that ever
				// broke it would trip the publish-order check above rather than corrupt
				bool bBufferRetired = nChunk == nRoom || !pRing->m_bIncremental;
				if (bBufferRetired)
				{
					pRing->f_OrderPop();
					pRing->m_BufferSlots[ChunkBid].f_Clear();
					if (pRing->m_bIncremental)
						pRing->m_SlotOffsets[ChunkBid] = 0;
				}
				else
					pRing->m_SlotOffsets[ChunkBid] = Offset + nChunk;

				// Merged rather than delivered: consecutive arrivals land back to back in one
				// buffer, so a chunk that continues the staged segment extends it in place.
				// One sink call then carries what several completions delivered — the call is
				// an actor hop, and the hop count is most of what the receive path costs.
				// Anything that cannot extend flushes what stands and stages itself; the reap
				// pass flushes whatever remains when the ring is dry
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
					mp_StreamFlushQueue.f_InsertLast(pStreamRegistration);
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
				if (fg_UringStatsEnabled())
					mp_pIo->m_UringStats.m_nStreamEnobufs.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif

				// The ring ran dry at the moment the kernel looked. Every consumed bid is
				// refilled in the same pass its data was dispatched, so a dry ring here means
				// either a transient burst — refilled already, re-arm at once — or the
				// backpressure gate parking bids, in which case the release that crosses the
				// resume threshold re-arms through fp_ResumeStream. A leaving registration gets
				// neither: the sink is owed its terminal now
				CUringRecvRing *pDryRing = pStreamRegistration->m_pRecvRing;

				if (pStreamRegistration->m_bDeregistering)
					fp_EndStream(pStreamRegistration, NSys::EIoCompletionStatus::mc_Cancelled, 0, _nReported);
				else if (pDryRing && pDryRing->m_UnfilledBids.f_IsEmpty())
					fp_ArmStream(pStreamRegistration);
				else
				{
					pStreamRegistration->m_bStreamNeedsRearm = true;

#if DMibConfig_IoDebug_Enable
					if (fg_UringTraceEnabled())
						fg_UringTrace("stream-dry", pStreamRegistration->m_pToken, pStreamRegistration->m_Handle, 0);
#endif
				}
			}
			else if (_Res > 0)
			{
				// The kernel delivered and ended the operation in one completion; the stream
				// itself is fine and arms again. A leaving registration instead gets its
				// terminal here: the cancel this completion outran finds nothing to cancel, so
				// no later completion delivers it, and the sink would otherwise be cleared at
				// acknowledgement with the consumer still waiting on an active stream
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
				if (fg_UringStatsEnabled())
					mp_pIo->m_UringStats.m_nRecvErrors.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif

				fp_EndStream(pStreamRegistration, NSys::EIoCompletionStatus::mc_Error, -_Res, _nReported);
			}

			fp_TryAcknowledge(pStreamRegistration, _nReported);
		}

		return;
	}

	auto *pRegistration = (CUringRegistration *)(umint)(_UserData & ~gc_UringUserData_TagMask);

	DMibCheck(pRegistration->m_nOutstanding != 0);
	--pRegistration->m_nOutstanding;

	// Dispatches into the consumer under a pin on the outstanding count: anything the callback
	// triggers on this thread — a synchronous deregistration re-entering the iterate included —
	// finds the count nonzero and cannot free the record while it is still in hand
	auto fDispatchEvents = [&](uint32 _PollBits)
		{
#if DMibConfig_IoDebug_Enable
			if (fg_UringTraceEnabled() && (_PollBits & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)))
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

			if ((uint32)_Res & (EPOLLERR | EPOLLHUP))
				pRegistration->m_ClosePoll = CUringRegistration::EClosePoll::mc_Terminal;
			else if (!pRegistration->m_bDeregistering)
			{
				// The peer half-closed; errors and full hangup are still to come, and they are
				// the only events a zero mask reports, so this arm fires at most once more
				pRegistration->m_ClosePoll = CUringRegistration::EClosePoll::mc_ArmedHupOnly;
				fp_ArmPoll(pRegistration, gc_UringTag_ClosePoll, 0);
			}
			else
				pRegistration->m_ClosePoll = CUringRegistration::EClosePoll::mc_NotArmed;
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
