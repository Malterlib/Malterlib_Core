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

// False leaves no receive ring. After capability probing, failure is resource exhaustion and must terminate the stream.
bool CIoLoop_IoUring::fp_StartStream(CUringRegistration *_pRegistration, NStorage::TCSharedPointer<NSys::CIoStreamBackpressure> &&_pBackpressure, umint &_nReported)
{
	// Slice full-fragment capacity into cache-friendly blocks; expose allocator padding as usable receive space.
	umint nSocketBytes = NMemory::CDefaultAllocator::f_SizePadded(_pRegistration->m_nStreamBufferBytes);

	umint nBufferBytes = nSocketBytes;
	if (!mp_pIo->f_ReceiveBufferBytesOverride() && nSocketBytes > gc_UringReceiveSliceBytes)
		nBufferBytes = NMemory::CDefaultAllocator::f_SizePadded(gc_UringReceiveSliceBytes);

	umint nBuffers = mp_pIo->f_ReceiveBuffersOverride();
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
	if (!fp_AllocateBgid(Bgid))
	{
		CDefaultAllocator::f_Free(pRingMem, nRingBytes);
		return false;
	}

	// Incremental consumption bounds retained capacity to payload plus partial end buffers.
	CIoUringBufReg Reg;
	fg_MemClear(&Reg, sizeof(Reg));
	Reg.m_RingAddr = (uint64)(umint)pRingMem;
	Reg.m_nRingEntries = (uint32)nEntries;
	Reg.m_Bgid = Bgid;
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
	pRing->m_BufferSlots.f_SetLen(nBuffers);
	pRing->m_SlotOffsets.f_SetLen(nBuffers);
	pRing->m_PublishOrder.f_SetLen(nEntries);
	pRing->m_pBackpressure = fg_Move(_pBackpressure);

	// Message assembly returns several rings of buffers in a burst; retain enough to avoid cycling through cold allocator blocks.
	pRing->m_pRecycler = fg_Construct();
	pRing->m_pRecycler->m_nBufferBytes = pRing->m_nBufferBytes;
	pRing->m_pRecycler->m_nMaxFree = 4 * nBuffers;

	// Before charging any buffer, reserve a full ring beyond the consumer hold using actual allocation capacity.
	if (pRing->m_pBackpressure && pRing->m_pBackpressure->m_nLimitBytes)
	{
		auto &Backpressure = *pRing->m_pBackpressure;
		umint nCapacity = 4 * nBuffers * pRing->m_nBufferBytes;
		if (Backpressure.m_nLimitBytes < nCapacity)
		{
			Backpressure.m_nLimitBytes = nCapacity;
			Backpressure.m_nResumeBytes = nCapacity / 2;
		}

		// A held range pins at most two partial end buffers. Reserve one more for progress and resume when only the hold remains.
		if (umint nHold = Backpressure.m_nConsumerHoldBytes)
		{
			umint nSlack = 3 * pRing->m_nBufferBytes;
			umint nPinnable = fg_Min(nHold, TCLimitsInt<umint>::mc_Max - nSlack) + nSlack - pRing->m_nBufferBytes;
			Backpressure.m_nLimitBytes = fg_Max(Backpressure.m_nLimitBytes, nPinnable + pRing->m_nBufferBytes);
			Backpressure.m_nResumeBytes = fg_Max(Backpressure.m_nResumeBytes, nPinnable);
		}
	}

	_pRegistration->m_pRecvRing = pRing;

	for (umint iBuffer = 0; iBuffer < nBuffers; ++iBuffer)
		fp_RefillBid(_pRegistration, (uint16)iBuffer);

#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_TraceEnabled())
		fg_UringTrace("stream-start", _pRegistration->m_pToken, _pRegistration->m_Handle, (uint32)nBuffers);
#endif

	return true;
}

void CIoLoop_IoUring::fp_ArmStream(CUringRegistration *_pRegistration)
{
	DMibFastCheck(!_pRegistration->m_bStreamArmed && _pRegistration->m_pRecvRing);

	CIoUringSqe &Sqe = fp_PrepareSqe();
	Sqe.m_Opcode = gc_IoUringOp_Recv;
	Sqe.m_Fd = _pRegistration->m_Handle;
	Sqe.m_Flags = gc_IoUringSqeFlag_BufferSelect;
	Sqe.m_IoPrio = gc_IoUringRecv_Multishot | gc_IoUringRecvSend_Bundle;
	Sqe.m_BufIndex = _pRegistration->m_pRecvRing->m_Bgid;
	Sqe.m_UserData = (uint64)(umint)_pRegistration | gc_UringTag_RecvStream;

	++_pRegistration->m_nOutstanding;
	_pRegistration->m_bStreamArmed = true;

#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_StatsEnabled())
		mp_pIo->m_UringStats.m_nStreamArms.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif
}

// Recheck capacity after parking so a concurrent release cannot strand a stream below its limit.
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
				if (mp_pIo->f_StatsEnabled())
					mp_pIo->m_UringStats.m_nStreamParks.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
				if (mp_pIo->f_TraceEnabled())
					fg_UringTrace("stream-park", _pRegistration->m_pToken, _pRegistration->m_Handle, _Bid);
#endif

				return false;
			}

			// Clear the parked flag only when no earlier bids remain parked; otherwise they can permanently disappear from the ring.
			if (pRing->m_UnfilledBids.f_IsEmpty())
				Backpressure->m_bParked.f_Exchange(0, NAtomic::gc_MemoryOrder_AcquireRelease);
		}
	}

	NStorage::TCSharedPointer<CUringStreamBuffer> pBuffer = fg_Construct();
	pBuffer->m_pData = pRing->m_pRecycler->f_TryTake();
	if (!pBuffer->m_pData)
	{
		pBuffer->m_pData = (uint8 *)NMemory::CDefaultAllocator::f_Alloc(pRing->m_nBufferBytes);

#if DMibConfig_IoDebug_Enable
		if (mp_pIo->f_StatsEnabled())
		{
			mp_pIo->m_UringStats.m_nRecvBufferAllocs.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
			mp_pIo->m_UringStats.m_nRecvBufferAllocBytes.f_FetchAdd(pRing->m_nBufferBytes, NAtomic::gc_MemoryOrder_Relaxed);
		}
#endif
	}
#if DMibConfig_IoDebug_Enable
	else if (mp_pIo->f_StatsEnabled())
		mp_pIo->m_UringStats.m_nRecvBufferReuses.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif
	pBuffer->m_nDataBytes = pRing->m_nBufferBytes;
	pBuffer->m_pRecycler = pRing->m_pRecycler;
	pBuffer->m_pBackpressure = Backpressure;
	// Charged when the first bytes are delivered from it; a buffer the kernel never fills
	// releases nothing
	pBuffer->m_nCharged = 0;

	pRing->f_PublishBuffer(_Bid, *pBuffer);
	pRing->m_BufferSlots[_Bid] = fg_Move(pBuffer);
	pRing->m_SlotOffsets[_Bid] = 0;

	return true;
}

// Process owner-sequenced resumes on the loop thread; refill parked bids before rearming a dry stream.
void CIoLoop_IoUring::fp_ResumeStream(CUringRegistration *_pRegistration)
{
	CUringRecvRing *pRing = _pRegistration->m_pRecvRing;
	if (!pRing || _pRegistration->m_bStreamEnded || _pRegistration->m_bDeregistering)
		return;

#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_StatsEnabled())
		mp_pIo->m_UringStats.m_nStreamResumes.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif

	NContainer::TCVector<uint16> Unfilled = fg_Move(pRing->m_UnfilledBids);
	for (umint iBid = 0; iBid < Unfilled.f_GetLen(); ++iBid)
	{
		if (!fp_RefillBid(_pRegistration, Unfilled[iBid]))
		{
			// If one bid reparks, retain the rest of the batch too or those slots are permanently lost.
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
		if (mp_pIo->f_TraceEnabled())
			fg_UringTrace("stream-rearm", _pRegistration->m_pToken, _pRegistration->m_Handle, 0);
#endif
	}
}

// Pin the registration across sink callbacks so callback-triggered teardown cannot free it.
void CIoLoop_IoUring::fp_DeliverStreamSegment(CUringRegistration *_pRegistration, NSys::CIoStreamSegment &&_Segment, umint &_nReported)
{
	++_pRegistration->m_nOutstanding;
	_pRegistration->m_fStreamSink(fg_Move(_Segment));
	--_pRegistration->m_nOutstanding;
	++_nReported;
}

void CIoLoop_IoUring::fp_FlushStreamSegment(CUringRegistration *_pRegistration, umint &_nReported)
{
	if (!_pRegistration->m_bStreamSegmentPending)
		return;

	_pRegistration->m_bStreamSegmentPending = false;
	mp_StreamFlushQueue.f_Remove(_pRegistration);

	NSys::CIoStreamSegment Segment = fg_Move(_pRegistration->m_PendingStreamSegment);
	_pRegistration->m_PendingStreamSegment = NSys::CIoStreamSegment();

	fp_DeliverStreamSegment(_pRegistration, fg_Move(Segment), _nReported);
}

// Pop from the front: callback reentry may flush or restage registrations while this walk runs.
void CIoLoop_IoUring::fp_FlushStreamSegments(umint &_nReported)
{
	while (CUringRegistration *pRegistration = mp_StreamFlushQueue.f_GetFirst())
		fp_FlushStreamSegment(pRegistration, _nReported);
}

// All termination paths must deliver exactly one terminal segment.
void CIoLoop_IoUring::fp_EndStream(CUringRegistration *_pRegistration, NSys::EIoCompletionStatus _Status, int32 _Error, umint &_nReported)
{
	if (_pRegistration->m_bStreamEnded || !_pRegistration->m_fStreamSink)
		return;

	fp_FlushStreamSegment(_pRegistration, _nReported);

	_pRegistration->m_bStreamEnded = true;

	NSys::CIoStreamSegment Segment;
	Segment.m_Status = _Status;
	Segment.m_Error = _Error;

	fp_DeliverStreamSegment(_pRegistration, fg_Move(Segment), _nReported);
}

// Consumers may retain buffers after ring teardown. Their final releases settle accounting; the weak resume callback tolerates owner destruction.
void CIoLoop_IoUring::fp_ReleaseStream(CUringRegistration *_pRegistration)
{
	// Unlink staged segments before acknowledgement frees the registration; drop their buffer charge normally.
	if (_pRegistration->m_bStreamSegmentPending)
	{
		_pRegistration->m_bStreamSegmentPending = false;
		_pRegistration->m_PendingStreamSegment = NSys::CIoStreamSegment();
		mp_StreamFlushQueue.f_Remove(_pRegistration);
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

	// Mark the recycler dead before late returns; consumer references keep its state alive.
	if (pRing->m_pRecycler)
		pRing->m_pRecycler->f_Die();

	fg_DeleteObject(CDefaultAllocator(), pRing);
	_pRegistration->m_pRecvRing = nullptr;
	_pRegistration->m_fStreamSink = NSys::FIoStreamSink();
}

auto CIoLoop_IoUring::f_StartReceiveStream
	(
		NSys::CIoLoopRegistration *_pRegistration
		, umint _nBufferBytes
		, NStorage::TCSharedPointer<NSys::CIoStreamBackpressure> _pBackpressure
		, NSys::FIoStreamSink &&_fSink
	)
	-> bool
{
	if (!f_SupportsReceiveStream())
		return false;

	DMibSafeCheck(_nBufferBytes != 0, "A receive stream needs buffers with room in them");

	CUringIoOp *pOp = fg_ConstructObject<CUringIoOp>(NMemory::CDefaultAllocator());
	pOp->m_pRegistration = static_cast<CUringRegistration *>(_pRegistration);
	pOp->m_bStreamStart = true;
	// The ring entry carries a 32 bit length
	pOp->m_nBytes = fg_Min(_nBufferBytes, umint(1) << 30);
	if (umint nOverride = mp_pIo->f_ReceiveBufferBytesOverride())
		pOp->m_nBytes = nOverride;
	pOp->m_fSink = fg_Move(_fSink);
	pOp->m_pBackpressure = fg_Move(_pBackpressure);

	// Owner submissions need no wake because the next iterate consumes them before parking.
	// Before ownership is claimed, use the conservative wake path; the queued enable and durable pending bit guarantee service.
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

	bool bOwnerThread = fp_IsOwnerThread();
	{
		DMibLock(mp_IoOpLock);
		mp_PendingIoOps.f_Insert(pOp);
	}

	if (!bOwnerThread)
		fp_SignalWake();
}
