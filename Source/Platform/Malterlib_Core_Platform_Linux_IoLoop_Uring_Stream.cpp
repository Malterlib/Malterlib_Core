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

// Builds the standing receive's ring and registers it with the kernel, then lends it a full set
// of freshly allocated buffers. False leaves the registration with no ring and the stream is
// failed by the caller — the probe passed at startup, so this failing is resource exhaustion,
// not absence of the feature
bool CIoLoop_IoUring::fp_StartStream(CUringRegistration *_pRegistration, NStorage::TCSharedPointer<NSys::CIoStreamBackpressure> &&_pBackpressure, umint &_nReported)
{
	// The ring's shape follows the socket's own buffer size — the transport's fragment size
	// plus its framing allowances: capacity for a full fragment, sliced into hot blocks. A
	// socket at or below the slice keeps its buffer whole with the minimum count, so
	// small-fragment connections do not inflate their rings. Sizes are padded to the
	// allocation's real size class, so the capacity the allocator hands over anyway is
	// receive space instead of waste. The env overrides pin either number directly
	umint nSocketBytes = NMemory::CDefaultAllocator::f_SizePadded(_pRegistration->m_nStreamBufferBytes);

	umint nBufferBytes = nSocketBytes;
	if (!fg_UringReceiveBufferBytesOverride() && nSocketBytes > gc_UringReceiveSliceBytes)
		nBufferBytes = NMemory::CDefaultAllocator::f_SizePadded(gc_UringReceiveSliceBytes);

	umint nBuffers = fg_UringReceiveBuffersOverride();
	if (!nBuffers)
		nBuffers = fg_Clamp((nSocketBytes + nBufferBytes - 1) / nBufferBytes, gc_UringMinReceiveBuffers, gc_UringMaxReceiveBuffers);

	umint nEntries = 1;
	while (nEntries < nBuffers)
		nEntries <<= 1;

	umint nRingBytes = fg_UringRingBytes(nEntries);

	void *pRingMem = fg_UringAllocRing(nRingBytes);
	if (!pRingMem)
		return false;

	uint16 Bgid;
	if (!mp_FreeBgids.f_IsEmpty())
	{
		Bgid = mp_FreeBgids.f_Pop();
	}
	else
		Bgid = mp_NextBgid++;

	bool bIncremental = CIoUringRing::fs_ReceiveStreamIncremental();

	CIoUringBufReg Reg;
	fg_MemClear(&Reg, sizeof(Reg));
	Reg.m_RingAddr = (uint64)(umint)pRingMem;
	Reg.m_nRingEntries = (uint32)nEntries;
	Reg.m_Bgid = Bgid;
	if (bIncremental)
		Reg.m_Flags = gc_IoUringPbufRing_Incremental;

	if (CIoUringRing::fs_Register(mp_Ring.m_RingFd, gc_IoUringRegister_PbufRing, &Reg, 1) != 0)
	{
		mp_FreeBgids.f_Insert(Bgid);
		CDefaultAllocator::f_Free(pRingMem, nRingBytes);
		return false;
	}

	auto *pRing = fg_ConstructObject<CUringRecvRing>(CDefaultAllocator());
	pRing->m_pRingMem = pRingMem;
	pRing->m_nRingMemSize = nRingBytes;
	pRing->m_pRingEntries = (CIoUringBuf *)pRingMem;
	pRing->m_pRingTail = (uint16 *)((uint8 *)pRingMem + gc_UringPbufRingTailOffset);
	pRing->m_nRingEntries = nEntries;
	pRing->m_Bgid = Bgid;
	pRing->m_nBuffers = nBuffers;
	pRing->m_nBufferBytes = nBufferBytes;
	pRing->m_bIncremental = bIncremental;
	pRing->m_BufferSlots.f_SetLen(nBuffers);
	if (bIncremental)
		pRing->m_SlotOffsets.f_SetLen(nBuffers);
	pRing->m_PublishOrder.f_SetLen(nEntries);
	pRing->m_pBackpressure = fg_Move(_pBackpressure);

	// Sized for the consumer's hold-and-burst-return cycle, not just the posted set: a consumer
	// assembling a message holds a ring's worth of buffers while the refills allocate fresh
	// ones, then returns them all at once — a free list bounded at one ring drops that burst
	// to the allocator and the next refills go cold again. A few rings of slack keeps the
	// blocks cycling here instead
	pRing->m_pRecycler = fg_Construct();
	pRing->m_pRecycler->m_nBufferBytes = pRing->m_nBufferBytes;
	pRing->m_pRecycler->m_nMaxFree = 4 * nBuffers;

	// The stream charges whole buffer capacities, so the window has to be able to hold a
	// full ring of them on top of what the consumer keeps; a window denominated in the
	// socket's own buffer size would otherwise park a ring of larger buffers immediately.
	// Raised before any charge exists, so no delivery can have read the smaller limit
	if (pRing->m_pBackpressure && pRing->m_pBackpressure->m_nLimitBytes)
	{
		umint nCapacity = 4 * nBuffers * pRing->m_nBufferBytes;
		if (pRing->m_pBackpressure->m_nLimitBytes < nCapacity)
		{
			pRing->m_pBackpressure->m_nLimitBytes = nCapacity;
			pRing->m_pBackpressure->m_nResumeBytes = nCapacity / 2;
		}
	}

	_pRegistration->m_pRecvRing = pRing;

	for (umint iBuffer = 0; iBuffer < nBuffers; ++iBuffer)
		fp_RefillBid(_pRegistration, (uint16)iBuffer);

#if DMibConfig_IoDebug_Enable
	if (fg_UringTraceEnabled())
		fg_UringTrace("stream-start", _pRegistration->m_pToken, _pRegistration->m_Fd, (uint32)nBuffers);
#endif

	return true;
}

void CIoLoop_IoUring::fp_ArmStream(CUringRegistration *_pRegistration)
{
	DMibFastCheck(!_pRegistration->m_bStreamArmed && _pRegistration->m_pRecvRing);

	// Bundled: one completion covers as many published buffers as the kernel had data for,
	// which is what keeps the completion count proportional to arrivals rather than to
	// buffer-sized pieces of them. Support rides the same kernel feature the send-bundle
	// probe already gates completion mode on
	CIoUringSqe &Sqe = fp_PrepareSqe();
	Sqe.m_Opcode = gc_IoUringOp_Recv;
	Sqe.m_Fd = _pRegistration->m_Fd;
	Sqe.m_Flags = gc_IoUringSqeFlag_BufferSelect;
	Sqe.m_IoPrio = gc_IoUringRecv_Multishot | gc_IoUringRecvSend_Bundle;
	Sqe.m_BufIndex = _pRegistration->m_pRecvRing->m_Bgid;
	Sqe.m_UserData = (uint64)(umint)_pRegistration | gc_UringTag_RecvStream;

	++_pRegistration->m_nOutstanding;
	_pRegistration->m_bStreamArmed = true;

#if DMibConfig_IoDebug_Enable
	if (fg_UringStatsEnabled())
		g_UringStats.m_nStreamArms.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif
}

// Lends the kernel a fresh buffer under _Bid, unless the backpressure gate holds it back — over
// the limit the bid is parked instead, and the release that crosses the resume threshold is
// what brings it back through fp_ResumeStream. The count is re-checked after parking, so a
// release racing the park cannot strand the stream below the limit
bool CIoLoop_IoUring::fp_RefillBid(CUringRegistration *_pRegistration, uint16 _Bid)
{
	CUringRecvRing *pRing = _pRegistration->m_pRecvRing;
	auto &Backpressure = pRing->m_pBackpressure;

	if (Backpressure && Backpressure->m_nLimitBytes)
	{
		if (Backpressure->m_nOutstandingBytes.f_Load(NAtomic::gc_MemoryOrder_Acquire) >= Backpressure->m_nLimitBytes)
		{
			Backpressure->m_bParked.f_Store(1, NAtomic::gc_MemoryOrder_SequentiallyConsistent);

			if (Backpressure->m_nOutstandingBytes.f_Load(NAtomic::gc_MemoryOrder_Acquire) >= Backpressure->m_nLimitBytes)
			{
				pRing->m_UnfilledBids.f_Insert(_Bid);

#if DMibConfig_IoDebug_Enable
				if (fg_UringStatsEnabled())
					g_UringStats.m_nStreamParks.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
				if (fg_UringTraceEnabled())
					fg_UringTrace("stream-park", _pRegistration->m_pToken, _pRegistration->m_Fd, _Bid);
#endif

				return false;
			}

			// The count dropped between the check and the park; whoever released may or may not
			// have claimed the flag — either way this bid proceeds, and a claimed resume finds
			// nothing parked, which is harmless
			Backpressure->m_bParked.f_Exchange(0, NAtomic::gc_MemoryOrder_AcquireRelease);
		}
	}

	NStorage::TCSharedPointer<CUringStreamBuffer> pBuffer = fg_Construct();
	pBuffer->m_pData = pRing->m_pRecycler->f_TryTake();
	if (!pBuffer->m_pData)
	{
		pBuffer->m_pData = (uint8 *)NMemory::CDefaultAllocator::f_Alloc(pRing->m_nBufferBytes);

#if DMibConfig_IoDebug_Enable
		if (fg_UringStatsEnabled())
		{
			g_UringStats.m_nRecvBufferAllocs.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
			g_UringStats.m_nRecvBufferAllocBytes.f_FetchAdd(pRing->m_nBufferBytes, NAtomic::gc_MemoryOrder_Relaxed);
		}
#endif
	}
#if DMibConfig_IoDebug_Enable
	else if (fg_UringStatsEnabled())
		g_UringStats.m_nRecvBufferReuses.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif
	pBuffer->m_nDataBytes = pRing->m_nBufferBytes;
	pBuffer->m_pRecycler = pRing->m_pRecycler;
	pBuffer->m_pBackpressure = Backpressure;
	// Charged when the first bytes are delivered from it; a buffer the kernel never fills
	// releases nothing
	pBuffer->m_nCharged = 0;

	pRing->f_PublishBuffer(_Bid, *pBuffer);
	pRing->m_BufferSlots[_Bid] = fg_Move(pBuffer);
	if (pRing->m_bIncremental)
		pRing->m_SlotOffsets[_Bid] = 0;

	return true;
}

// The stream's owner answering a resume: refills the bids the gate parked and re-arms a
// receive the dry ring terminated. Loop thread, via the pending queue
void CIoLoop_IoUring::fp_ResumeStream(CUringRegistration *_pRegistration)
{
	CUringRecvRing *pRing = _pRegistration->m_pRecvRing;
	if (!pRing || _pRegistration->m_bStreamEnded || _pRegistration->m_bDeregistering)
		return;

#if DMibConfig_IoDebug_Enable
	if (fg_UringStatsEnabled())
		g_UringStats.m_nStreamResumes.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif

	NContainer::TCVector<uint16> Unfilled = fg_Move(pRing->m_UnfilledBids);
	for (umint iBid = 0; iBid < Unfilled.f_GetLen(); ++iBid)
	{
		if (!fp_RefillBid(_pRegistration, Unfilled[iBid]))
		{
			// The gate re-parked this bid on its own; the rest of the batch has to park with
			// it, or the slots they name would never be tracked or published again and the
			// ring would shrink for good
			for (umint iRemaining = iBid + 1; iRemaining < Unfilled.f_GetLen(); ++iRemaining)
				pRing->m_UnfilledBids.f_Insert(Unfilled[iRemaining]);

			break;
		}
	}

	if (pRing->m_UnfilledBids.f_GetLen() < pRing->m_nBuffers && _pRegistration->m_bStreamNeedsRearm && !_pRegistration->m_bStreamArmed)
	{
		_pRegistration->m_bStreamNeedsRearm = false;
		fp_ArmStream(_pRegistration);

#if DMibConfig_IoDebug_Enable
		if (fg_UringTraceEnabled())
			fg_UringTrace("stream-rearm", _pRegistration->m_pToken, _pRegistration->m_Fd, 0);
#endif
	}
}

// Dispatches one segment into the sink under a pin on the outstanding count, like every other
// dispatch: anything the sink triggers finds the count nonzero and cannot free the record
void CIoLoop_IoUring::fp_DeliverStreamSegment(CUringRegistration *_pRegistration, NSys::CIoStreamSegment _Segment, umint &_nReported)
{
	++_pRegistration->m_nOutstanding;
	_pRegistration->m_fStreamSink(fg_Move(_Segment));
	--_pRegistration->m_nOutstanding;
	++_nReported;
}

// Delivers a registration's merged, undelivered segment, if one stands. Idempotent through the
// flag, because the flush queue may hold the registration more than once
void CIoLoop_IoUring::fp_FlushStreamSegment(CUringRegistration *_pRegistration, umint &_nReported)
{
	if (!_pRegistration->m_bStreamSegmentPending)
		return;

	_pRegistration->m_bStreamSegmentPending = false;

	NSys::CIoStreamSegment Segment = fg_Move(_pRegistration->m_PendingStreamSegment);
	_pRegistration->m_PendingStreamSegment = NSys::CIoStreamSegment();

	fp_DeliverStreamSegment(_pRegistration, fg_Move(Segment), _nReported);
}

// The end-of-pass flush. Guarded against re-entry — a sink can reach fp_PlaceBatch, whose full
// queue reaps — and walked by index, because a flush can restage a registration behind it
void CIoLoop_IoUring::fp_FlushStreamSegments(umint &_nReported)
{
	if (mp_bFlushingStreamSegments)
		return;

	mp_bFlushingStreamSegments = true;

	for (umint iEntry = 0; iEntry < mp_StreamFlushQueue.f_GetLen(); ++iEntry)
		fp_FlushStreamSegment(mp_StreamFlushQueue[iEntry], _nReported);

	mp_StreamFlushQueue.f_Clear();
	mp_bFlushingStreamSegments = false;
}

// The stream's one terminal segment. Guarded so every path that can end a stream — end of
// stream, error, cancellation, a deregistration finding it unarmed — delivers it exactly once
void CIoLoop_IoUring::fp_EndStream(CUringRegistration *_pRegistration, NSys::EIoCompletionStatus _Status, int32 _Error, umint &_nReported)
{
	if (_pRegistration->m_bStreamEnded || !_pRegistration->m_fStreamSink)
		return;

	// What arrived before the end goes first
	fp_FlushStreamSegment(_pRegistration, _nReported);

	_pRegistration->m_bStreamEnded = true;

	NSys::CIoStreamSegment Segment;
	Segment.m_Status = _Status;
	Segment.m_Error = _Error;

	fp_DeliverStreamSegment(_pRegistration, fg_Move(Segment), _nReported);
}

// Tears the ring down at acknowledgement time: the kernel-side registration goes — the
// descriptor is closing anyway — and the slots drop their references. Buffers still with
// consumers live on through the references the segments carry, and their releases still settle
// the backpressure accounting; the resume functor reaches its owner through a weak reference,
// so a release after the owner is gone is a quiet no-op
void CIoLoop_IoUring::fp_ReleaseStream(CUringRegistration *_pRegistration)
{
	// A merged segment still staged here was overtaken by the deregistration — the consumer
	// stopped caring, and the end path flushed everything a sink still wanted. The reference
	// drops so the buffer's backpressure charge settles, and the flush queue forgets the
	// registration before the acknowledgement frees it under the queue's feet
	if (_pRegistration->m_bStreamSegmentPending)
	{
		_pRegistration->m_bStreamSegmentPending = false;
		_pRegistration->m_PendingStreamSegment = NSys::CIoStreamSegment();
	}

	for (umint iEntry = mp_StreamFlushQueue.f_GetLen(); iEntry > 0;)
	{
		--iEntry;
		if (mp_StreamFlushQueue[iEntry] == _pRegistration)
			mp_StreamFlushQueue.f_Remove(iEntry);
	}

	CUringRecvRing *pRing = _pRegistration->m_pRecvRing;
	if (!pRing)
	{
		_pRegistration->m_fStreamSink = NSys::FIoStreamSink();
		return;
	}

	CIoUringBufReg Unreg;
	fg_MemClear(&Unreg, sizeof(Unreg));
	Unreg.m_Bgid = pRing->m_Bgid;
	CIoUringRing::fs_Register(mp_Ring.m_RingFd, gc_IoUringRegister_UnregisterPbufRing, &Unreg, 1);
	mp_FreeBgids.f_Insert(pRing->m_Bgid);

	CDefaultAllocator::f_Free(pRing->m_pRingMem, pRing->m_nRingMemSize);
	pRing->m_pRingMem = nullptr;

	// Stacked blocks go back to the allocator and late returns fall through from now on;
	// buffers still with consumers keep the recycler itself alive through their references
	if (pRing->m_pRecycler)
		pRing->m_pRecycler->f_Die();

	fg_DeleteObject(CDefaultAllocator(), pRing);
	_pRegistration->m_pRecvRing = nullptr;
	_pRegistration->m_fStreamSink = NSys::FIoStreamSink();
}

bool CIoLoop_IoUring::f_StartReceiveStream(NSys::CIoLoopRegistration *_pRegistration, umint _nBufferBytes, NStorage::TCSharedPointer<NSys::CIoStreamBackpressure> _pBackpressure, NSys::FIoStreamSink &&_fSink)
{
	if (!f_SupportsReceiveStream())
		return false;

	DMibSafeCheck(_nBufferBytes != 0, "A receive stream needs buffers with room in them");

	CUringIoOp *pOp = fg_ConstructObject<CUringIoOp>(NMemory::CDefaultAllocator());
	pOp->m_pRegistration = static_cast<CUringRegistration *>(_pRegistration);
	pOp->m_bStreamStart = true;
	// The ring entry carries a 32 bit length
	pOp->m_nBytes = fg_Min(_nBufferBytes, umint(1) << 30);
	if (umint nOverride = fg_UringReceiveBufferBytesOverride())
		pOp->m_nBytes = nOverride;
	pOp->m_fSink = fg_Move(_fSink);
	pOp->m_pBackpressure = fg_Move(_pBackpressure);

	// The owner's own submissions skip the wake: pending operations are picked up at the start of
	// every iterate and the owner cannot park again without iterating, so its next park hands them
	// to the kernel fused with the wait.
	//
	// The loop may not have an owner yet: it is claimed by the enable message on its queue's
	// thread, and a submission can run before that message has been processed. This reads as not
	// the owner then, which is the conservative side — the operation waits in the pending queue
	// and the wake's pending bit is durable, so the owner's first park finds it instead of
	// sleeping past it. An owner iterate is always coming, because bindings are only handed out
	// after the enable messages were dispatched.
	//
	// Owner-sequenced before any removal like every pending-queue enqueue — see
	// f_SubmitSendVectored for why the raw registration pointer is safe to queue
	bool bOwnerThread = fp_IsOwnerThread();
	{
		DMibLock(mp_IoOpLock);
		mp_PendingIoOps.f_Insert(pOp);
	}

	if (!bOwnerThread)
		fp_SignalWake();

	return true;
}

void CIoLoop_IoUring::f_ResumeReceiveStream(NSys::CIoLoopRegistration *_pRegistration)
{
	CUringIoOp *pOp = fg_ConstructObject<CUringIoOp>(NMemory::CDefaultAllocator());
	pOp->m_pRegistration = static_cast<CUringRegistration *>(_pRegistration);
	pOp->m_bStreamResume = true;

	// Owner-sequenced before any removal like every pending-queue enqueue — see
	// f_SubmitSendVectored for why the raw registration pointer is safe to queue
	bool bOwnerThread = fp_IsOwnerThread();
	{
		DMibLock(mp_IoOpLock);
		mp_PendingIoOps.f_Insert(pOp);
	}

	if (!bOwnerThread)
		fp_SignalWake();
}
