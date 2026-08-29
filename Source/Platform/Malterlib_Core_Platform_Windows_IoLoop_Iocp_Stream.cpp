// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Windows_IoLoop_Iocp_Internal.h"

using namespace NMib;
using namespace NMib::NMemory;
using namespace NMib::NSys;

bool CIoLoop_Iocp::f_StartReceiveStream(NSys::CIoLoopRegistration *_pRegistration, umint _nBufferBytes, NStorage::TCSharedPointer<NSys::CIoStreamBackpressure> _pBackpressure, NSys::FIoStreamSink &&_fSink)
{
	if (!f_SupportsReceiveStream())
		return false;

	DMibSafeCheck(_nBufferBytes != 0, "A receive stream needs buffers with room in them");

	auto *pOp = fg_ConstructObject<CIocpPendingOp>(CDefaultAllocator());
	pOp->m_pRegistration = static_cast<CIocpRegistration *>(_pRegistration);
	pOp->m_bStreamStart = true;
	// A buffer descriptor carries a 32 bit length
	pOp->m_nBytes = fg_Min(_nBufferBytes, umint(1) << 30);
	if (umint nOverride = fg_IocpRecvBufferBytesOverride())
		pOp->m_nBytes = nOverride;
	pOp->m_fSink = fg_Move(_fSink);
	pOp->m_pBackpressure = fg_Move(_pBackpressure);

	fp_QueuePendingOp(pOp);

	return true;
}

void CIoLoop_Iocp::f_ResumeReceiveStream(NSys::CIoLoopRegistration *_pRegistration)
{
	auto *pOp = fg_ConstructObject<CIocpPendingOp>(CDefaultAllocator());
	pOp->m_pRegistration = static_cast<CIocpRegistration *>(_pRegistration);
	pOp->m_bStreamResume = true;

	fp_QueuePendingOp(pOp);
}

// Sizes the stream's buffers and its recycler. The buffer follows the socket's own size — the
// transport's fragment plus its framing allowances — sliced into hot blocks when large; a socket
// at or below the slice keeps its buffer whole. Sizes are padded to the allocation's real size
// class, so the capacity the allocator hands over anyway is receive space instead of waste
bool CIoLoop_Iocp::fp_StartStream(CIocpRegistration *_pRegistration, NStorage::TCSharedPointer<NSys::CIoStreamBackpressure> &&_pBackpressure)
{
	umint nSocketBytes = NMemory::CDefaultAllocator::f_SizePadded(_pRegistration->m_nStreamBufferBytes);

	umint nBufferBytes = nSocketBytes;
	if (!fg_IocpRecvBufferBytesOverride() && nSocketBytes > gc_IocpReceiveSliceBytes)
		nBufferBytes = NMemory::CDefaultAllocator::f_SizePadded(gc_IocpReceiveSliceBytes);

	umint nDepth = fg_IocpRecvDepth();

	_pRegistration->m_nRecvBufferBytes = nBufferBytes;
	_pRegistration->m_nRecvDepth = nDepth;
	_pRegistration->m_pBackpressure = fg_Move(_pBackpressure);

	// Sized for the consumer's hold-and-burst-return cycle, not just the posted set: a consumer
	// assembling a message holds several buffers while the posts allocate fresh ones, then
	// returns them all at once — a free list bounded at the posted set drops that burst to the
	// allocator and the next posts go cold again
	_pRegistration->m_pRecycler = fg_Construct();
	_pRegistration->m_pRecycler->m_nBufferBytes = nBufferBytes;
	_pRegistration->m_pRecycler->m_nMaxFree = 4 * nDepth + 4;

	// The stream charges whole buffer capacities, so the window has to be able to hold a full
	// set of them on top of what the consumer keeps; a window denominated in the socket's own
	// buffer size would otherwise park a set of larger buffers immediately. Raised before any
	// charge exists, so no delivery can have read the smaller limit
	auto &pBackpressure = _pRegistration->m_pBackpressure;
	if (pBackpressure && pBackpressure->m_nLimitBytes)
	{
		umint nCapacity = 4 * nDepth * nBufferBytes;
		if (pBackpressure->m_nLimitBytes < nCapacity)
		{
			pBackpressure->m_nLimitBytes = nCapacity;
			pBackpressure->m_nResumeBytes = nCapacity / 2;
		}
	}

#if DMibConfig_IoDebug_Enable
	if (fg_IocpTraceEnabled())
		fg_IocpTrace("stream-start", _pRegistration->m_pToken, _pRegistration->m_Handle, (uint32)nBufferBytes);
#endif

	return true;
}

// A fresh buffer for the kernel to fill, unless the backpressure gate holds it back — over the
// limit nothing is returned, and the release that crosses the resume threshold is what brings
// the stream back through fp_ResumeStream. The count is re-checked after parking, so a release
// racing the park cannot strand the stream below the limit
NStorage::TCSharedPointer<CIocpStreamBuffer> CIoLoop_Iocp::fp_TakeStreamBuffer(CIocpRegistration *_pRegistration)
{
	auto &Backpressure = _pRegistration->m_pBackpressure;

	if (Backpressure && Backpressure->m_nLimitBytes)
	{
		if (Backpressure->m_nOutstandingBytes.f_Load(NAtomic::gc_MemoryOrder_Acquire) >= Backpressure->m_nLimitBytes)
		{
			Backpressure->m_bParked.f_Store(1, NAtomic::gc_MemoryOrder_SequentiallyConsistent);

			if (Backpressure->m_nOutstandingBytes.f_Load(NAtomic::gc_MemoryOrder_Acquire) >= Backpressure->m_nLimitBytes)
			{
#if DMibConfig_IoDebug_Enable
				if (fg_IocpStatsEnabled())
					g_IocpStats.m_nStreamParks.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
				if (fg_IocpTraceEnabled())
					fg_IocpTrace("stream-park", _pRegistration->m_pToken, _pRegistration->m_Handle, 0);
#endif

				return {};
			}

			// The count dropped between the check and the park; whoever released may or may not
			// have claimed the flag — either way this post proceeds, and a claimed resume finds
			// nothing parked, which is harmless
			Backpressure->m_bParked.f_Exchange(0, NAtomic::gc_MemoryOrder_AcquireRelease);
		}
	}

	NStorage::TCSharedPointer<CIocpStreamBuffer> pBuffer = fg_Construct();
	pBuffer->m_pData = _pRegistration->m_pRecycler->f_TryTake();
	if (!pBuffer->m_pData)
	{
		pBuffer->m_pData = (uint8 *)NMemory::CDefaultAllocator::f_Alloc(_pRegistration->m_nRecvBufferBytes);

#if DMibConfig_IoDebug_Enable
		if (fg_IocpStatsEnabled())
			g_IocpStats.m_nRecvBufferAllocs.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif
	}
#if DMibConfig_IoDebug_Enable
	else if (fg_IocpStatsEnabled())
		g_IocpStats.m_nRecvBufferReuses.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif
	pBuffer->m_nDataBytes = _pRegistration->m_nRecvBufferBytes;
	pBuffer->m_pRecycler = _pRegistration->m_pRecycler;
	pBuffer->m_pBackpressure = Backpressure;
	// Charged when the first bytes are delivered from it; a buffer the kernel never fills
	// releases nothing
	pBuffer->m_nCharged = 0;

	return pBuffer;
}

// Posts one receive into the untouched tail of its buffer, taking a fresh buffer when it has
// none. False means the gate parked the stream and nothing was posted. Every posted receive
// leaves exactly one completion owed, packet or inline, counted from here to its report
bool CIoLoop_Iocp::fp_PostRecv(CIocpRegistration *_pRegistration, CIocpRecvOp &_Op)
{
	DMibFastCheck(!_Op.m_bIssued);

	if (!_Op.m_pBuffer)
	{
		_Op.m_pBuffer = fp_TakeStreamBuffer(_pRegistration);
		if (!_Op.m_pBuffer)
			return false;

		_Op.m_Offset = 0;
	}

	fg_MemClear(&_Op.m_Overlapped, sizeof(_Op.m_Overlapped));
	_Op.m_Status = 0;
	_Op.m_nBytes = 0;
	_Op.m_Error = 0;
	_Op.m_Flags = 0;
	_Op.m_bCompleted = false;
	_Op.m_Buffer.buf = (CHAR *)_Op.m_pBuffer->m_pData + _Op.m_Offset;
	_Op.m_Buffer.len = (ULONG)(_Op.m_pBuffer->m_nDataBytes - _Op.m_Offset);

	_Op.m_bIssued = true;
	++_pRegistration->m_nOutstanding;
	++_pRegistration->m_nRecvsInFlight;

	_Op.m_pNext = nullptr;
	if (_pRegistration->m_pRecvTail)
		_pRegistration->m_pRecvTail->m_pNext = &_Op;
	else
		_pRegistration->m_pRecvHead = &_Op;
	_pRegistration->m_pRecvTail = &_Op;

#if DMibConfig_IoDebug_Enable
	if (fg_IocpStatsEnabled())
		g_IocpStats.m_nRecvPosts.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif

	DWORD nReceived = 0;
	int Ret = WSARecv((SOCKET)_pRegistration->m_Handle, &_Op.m_Buffer, 1, &nReceived, &_Op.m_Flags, &_Op.m_Overlapped, nullptr);

#if DMibConfig_IoDebug_Enable
	if (fg_IocpTraceEnabled())
		fg_IocpTrace(Ret == 0 ? "recv-post-done" : "recv-post", _pRegistration->m_pToken, _pRegistration->m_Handle, Ret == 0 ? (uint32)nReceived : (uint32)WSAGetLastError());
#endif

	if (Ret == 0)
	{
		if (_pRegistration->m_bSkipSuccess)
		{
			_Op.m_bCompleted = true;
			_Op.m_nBytes = nReceived;
			fp_QueueInlineCompletion(_pRegistration);

#if DMibConfig_IoDebug_Enable
			if (fg_IocpStatsEnabled())
				g_IocpStats.m_nRecvInline.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif
		}

		return true;
	}

	int Error = WSAGetLastError();
	if (Error == WSA_IO_PENDING)
		return true;

	_Op.m_bCompleted = true;
	_Op.m_Status = gc_NtStatus_Unsuccessful;
	_Op.m_Error = Error;
	fp_QueueInlineCompletion(_pRegistration);

	return true;
}

// Posts every receive the stream keeps that is not already with the kernel; a post the gate
// refuses leaves the stream waiting for a resume
void CIoLoop_Iocp::fp_ArmStream(CIocpRegistration *_pRegistration)
{
	_pRegistration->m_bStreamNeedsRearm = false;

#if DMibConfig_IoDebug_Enable
	if (fg_IocpTraceEnabled())
		fg_IocpTrace("stream-arm", _pRegistration->m_pToken, _pRegistration->m_Handle, (uint32)_pRegistration->m_nRecvDepth | ((uint32)_pRegistration->m_RecvOps[0].m_bIssued << 8) | ((uint32)_pRegistration->m_nRecvsInFlight << 16));
#endif

	for (umint iOp = 0; iOp < _pRegistration->m_nRecvDepth; ++iOp)
	{
		CIocpRecvOp &Op = _pRegistration->m_RecvOps[iOp];
		if (Op.m_bIssued)
			continue;

		if (!fp_PostRecv(_pRegistration, Op))
		{
			_pRegistration->m_bStreamNeedsRearm = true;
			return;
		}
	}
}

// The stream's owner answering a resume: re-posts what the gate parked. Loop thread, via the
// pending queue
void CIoLoop_Iocp::fp_ResumeStream(CIocpRegistration *_pRegistration)
{
	if (!_pRegistration->m_bStreamStarted || _pRegistration->m_bStreamEnded || _pRegistration->m_bDeregistering)
		return;

	if (!_pRegistration->m_bStreamNeedsRearm)
		return;

#if DMibConfig_IoDebug_Enable
	if (fg_IocpStatsEnabled())
		g_IocpStats.m_nStreamResumes.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
	if (fg_IocpTraceEnabled())
		fg_IocpTrace("stream-rearm", _pRegistration->m_pToken, _pRegistration->m_Handle, 0);
#endif

	fp_ArmStream(_pRegistration);
}

// Reports from the head while the head is complete, like the sends: the kernel fills the
// receives in issue order, so this is stream order whatever order the packets arrived in.
// Delivered bytes are staged for the end-of-pass flush; the receive is re-posted into what is
// left of its buffer, or into a fresh one when the tail is too short to be worth a receive
void CIoLoop_Iocp::fp_ReportCompletedRecvs(CIocpRegistration *_pRegistration, umint &_nReported)
{
	while (_pRegistration->m_pRecvHead && _pRegistration->m_pRecvHead->m_bCompleted)
	{
		CIocpRecvOp *pOp = _pRegistration->m_pRecvHead;
		_pRegistration->m_pRecvHead = pOp->m_pNext;
		if (!_pRegistration->m_pRecvHead)
			_pRegistration->m_pRecvTail = nullptr;
		pOp->m_pNext = nullptr;

		pOp->m_bIssued = false;
		--_pRegistration->m_nRecvsInFlight;
		DMibCheck(_pRegistration->m_nOutstanding != 0);
		--_pRegistration->m_nOutstanding;

		if (_pRegistration->m_bStreamEnded)
		{
			// A receive that outlived the terminal — the second of a pair completing after the
			// first ended the stream — has nothing to say; its buffer goes back
			pOp->m_pBuffer.f_Clear();
			continue;
		}

		if (pOp->m_Status == gc_NtStatus_Cancelled)
		{
			pOp->m_pBuffer.f_Clear();
			fp_EndStream(_pRegistration, NSys::EIoCompletionStatus::mc_Cancelled, 0, _nReported);
			continue;
		}

		if (pOp->m_Status != gc_NtStatus_Success)
		{
#if DMibConfig_IoDebug_Enable
			if (fg_IocpStatsEnabled())
				g_IocpStats.m_nRecvErrors.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif

			int Error = fp_OpError(_pRegistration, pOp);
			pOp->m_pBuffer.f_Clear();
			fp_EndStream(_pRegistration, NSys::EIoCompletionStatus::mc_Error, Error, _nReported);
			continue;
		}

		if (!pOp->m_nBytes)
		{
			pOp->m_pBuffer.f_Clear();
			fp_EndStream(_pRegistration, NSys::EIoCompletionStatus::mc_Done, 0, _nReported);

			// The peer's half close reached this side through the receive rather than through
			// the close poll: AFD reports a disconnect the receive consumed to no poll, so the
			// readiness side hears of it here, after the terminal segment has gone out — which
			// is the order a consumer holding close states until the stream runs dry expects
			if (!_pRegistration->m_bDeregistering)
			{
#if DMibConfig_IoDebug_Enable
				if (fg_IocpTraceEnabled())
					fg_IocpTrace("stream-eof-close", _pRegistration->m_pToken, _pRegistration->m_Handle, 0);
#endif

				fp_DispatchReadiness(_pRegistration, NSys::EIoLoopEvent::mc_ReadClosed, 0, _nReported);
			}

			continue;
		}

		umint nBytes = pOp->m_nBytes;

#if DMibConfig_IoDebug_Enable
		if (fg_IocpTraceEnabled())
			fg_IocpTrace("recv-report", _pRegistration->m_pToken, _pRegistration->m_Handle, (uint32)nBytes);
		if (fg_IocpStatsEnabled())
		{
			g_IocpStats.m_nRecvSegments.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
			g_IocpStats.m_nRecvBytes.f_FetchAdd(nBytes, NAtomic::gc_MemoryOrder_Relaxed);
			g_IocpStats.m_RecvSizeBuckets[fg_GetHighestBitSet(nBytes)].f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		}
#endif

		// The capacity is charged once per buffer, with its first delivered bytes: it is pinned
		// from then on until every segment referencing it has been released
		CIocpStreamBuffer &Buffer = *pOp->m_pBuffer;
		if (!Buffer.m_nCharged)
		{
			Buffer.m_nCharged = Buffer.m_nDataBytes;
			if (Buffer.m_pBackpressure)
				Buffer.m_pBackpressure->m_nOutstandingBytes.f_FetchAdd(Buffer.m_nCharged, NAtomic::gc_MemoryOrder_AcquireRelease);
		}

		NSys::CIoStreamSegment Segment;
		Segment.m_pData = Buffer.m_pData + pOp->m_Offset;
		Segment.m_nBytes = nBytes;
		Segment.m_pOwner = pOp->m_pBuffer.f_ShareAsConst();
		fp_StageStreamSegment(_pRegistration, fg_Move(Segment), _nReported);

		pOp->m_Offset += nBytes;

		// The kernel only ever writes the untouched tail while consumers read the delivered
		// head, so the buffer is shared safely; a tail too short for a worthwhile receive
		// retires the buffer — consumers keep it alive — and the next post takes a fresh one
		if (Buffer.m_nDataBytes - pOp->m_Offset < gc_IocpRecvMinPostBytes)
		{
			pOp->m_pBuffer.f_Clear();
			pOp->m_Offset = 0;
		}

		if (!_pRegistration->m_bStreamEnded && !_pRegistration->m_bDeregistering)
		{
			if (!fp_PostRecv(_pRegistration, *pOp))
				_pRegistration->m_bStreamNeedsRearm = true;
		}
		else if (_pRegistration->m_bDeregistering && !_pRegistration->m_nRecvsInFlight)
		{
			// The cancel this completion outran finds nothing to cancel, so no later completion
			// delivers the terminal; the sink is owed it here
			pOp->m_pBuffer.f_Clear();
			fp_EndStream(_pRegistration, NSys::EIoCompletionStatus::mc_Cancelled, 0, _nReported);
		}
	}
}

// Merged rather than delivered: consecutive arrivals land back to back in one buffer, so a
// chunk that continues the staged segment extends it in place. One sink call then carries what
// several completions delivered. Anything that cannot extend flushes what stands and stages
// itself; the pass flushes whatever remains at its end
void CIoLoop_Iocp::fp_StageStreamSegment(CIocpRegistration *_pRegistration, NSys::CIoStreamSegment &&_Segment, umint &_nReported)
{
	CIoStreamSegment &Pending = _pRegistration->m_PendingStreamSegment;
	if
	(
		_pRegistration->m_bStreamSegmentPending
		&& Pending.m_pOwner == _Segment.m_pOwner
		&& (uint8 const *)Pending.m_pData + Pending.m_nBytes == (uint8 const *)_Segment.m_pData
	)
	{
		Pending.m_nBytes += _Segment.m_nBytes;
		return;
	}

	fp_FlushStreamSegment(_pRegistration, _nReported);

	_pRegistration->m_PendingStreamSegment = fg_Move(_Segment);
	_pRegistration->m_bStreamSegmentPending = true;
	mp_StreamFlushQueue.f_InsertLast(_pRegistration);
}

// Dispatches one segment into the sink under a pin on the outstanding count, like every other
// dispatch: anything the sink triggers finds the count nonzero and cannot free the record
void CIoLoop_Iocp::fp_DeliverStreamSegment(CIocpRegistration *_pRegistration, NSys::CIoStreamSegment _Segment, umint &_nReported)
{
	++_pRegistration->m_nOutstanding;
	++mp_nDispatchDepth;
	_pRegistration->m_fStreamSink(fg_Move(_Segment));
	--mp_nDispatchDepth;
	--_pRegistration->m_nOutstanding;
	++_nReported;
}

// Delivers a registration's merged, undelivered segment, if one stands. Idempotent through the
// flag, because the flush queue may hold the registration more than once
void CIoLoop_Iocp::fp_FlushStreamSegment(CIocpRegistration *_pRegistration, umint &_nReported)
{
	if (!_pRegistration->m_bStreamSegmentPending)
		return;

	_pRegistration->m_bStreamSegmentPending = false;

	NSys::CIoStreamSegment Segment = fg_Move(_pRegistration->m_PendingStreamSegment);
	_pRegistration->m_PendingStreamSegment = NSys::CIoStreamSegment();

	fp_DeliverStreamSegment(_pRegistration, fg_Move(Segment), _nReported);
}

// The end-of-pass flush. Guarded against re-entry and walked by index, because a flush can
// restage a registration behind it
void CIoLoop_Iocp::fp_FlushStreamSegments(umint &_nReported)
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
void CIoLoop_Iocp::fp_EndStream(CIocpRegistration *_pRegistration, NSys::EIoCompletionStatus _Status, int32 _Error, umint &_nReported)
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

// Tears the stream down at acknowledgement time. Buffers still with consumers live on through
// the references the segments carry, and their releases still settle the backpressure
// accounting; the resume functor reaches its owner through a weak reference, so a release after
// the owner is gone is a quiet no-op
void CIoLoop_Iocp::fp_ReleaseStream(CIocpRegistration *_pRegistration)
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

	DMibFastCheck(!_pRegistration->m_nRecvsInFlight);
	for (auto &Op : _pRegistration->m_RecvOps)
		Op.m_pBuffer.f_Clear();

	// Stacked blocks go back to the allocator and late returns fall through from now on;
	// buffers still with consumers keep the recycler itself alive through their references
	if (_pRegistration->m_pRecycler)
	{
		_pRegistration->m_pRecycler->f_Die();
		_pRegistration->m_pRecycler.f_Clear();
	}

	_pRegistration->m_pBackpressure.f_Clear();
	_pRegistration->m_fStreamSink = NSys::FIoStreamSink();
}
