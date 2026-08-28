// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Windows_IoLoop_Iocp_Internal.h"

using namespace NMib;
using namespace NMib::NMemory;
using namespace NMib::NSys;

auto CIoLoop_Iocp::f_StartReceiveStream
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

	auto *pOp = fg_ConstructObject<CIocpPendingOp>(CDefaultAllocator());
	pOp->m_pRegistration = static_cast<CIocpRegistration *>(_pRegistration);
	pOp->m_bStreamStart = true;
	// A buffer descriptor carries a 32 bit length
	pOp->m_nBytes = fg_Min(_nBufferBytes, umint(1) << 30);
	if (umint nOverride = mp_pIo->f_RecvBufferBytesOverride())
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

// Expose allocator padding as receive space and slice large transport buffers to keep recycler blocks cache-friendly.
bool CIoLoop_Iocp::fp_StartStream(CIocpRegistration *_pRegistration, NStorage::TCSharedPointer<NSys::CIoStreamBackpressure> &&_pBackpressure)
{
	umint nSocketBytes = NMemory::CDefaultAllocator::f_SizePadded(_pRegistration->m_nStreamBufferBytes);

	umint nBufferBytes = nSocketBytes;
	if (!mp_pIo->f_RecvBufferBytesOverride() && nSocketBytes > gc_IocpReceiveSliceBytes)
		nBufferBytes = NMemory::CDefaultAllocator::f_SizePadded(gc_IocpReceiveSliceBytes);

	umint nDepth = mp_pIo->f_RecvDepth();

	_pRegistration->m_nRecvBufferBytes = nBufferBytes;
	_pRegistration->m_nRecvDepth = nDepth;
	_pRegistration->m_pBackpressure = fg_Move(_pBackpressure);

	// Retain buffer return bursts from message assembly beyond the posted set to avoid cold reallocation.
	_pRegistration->m_pRecycler = fg_Construct();
	_pRegistration->m_pRecycler->m_nBufferBytes = nBufferBytes;
	_pRegistration->m_pRecycler->m_nMaxFree = fg_Max(4 * nDepth, gc_IocpRecyclerMaxFreeBytes / nBufferBytes);

	// Reserve full posted-buffer capacity beyond the consumer hold before any delivery can observe the limit.
	auto &pBackpressure = _pRegistration->m_pBackpressure;
	if (pBackpressure && pBackpressure->m_nLimitBytes)
	{
		umint nCapacity = 4 * nDepth * nBufferBytes;
		if (pBackpressure->m_nLimitBytes < nCapacity)
		{
			pBackpressure->m_nLimitBytes = nCapacity;
			pBackpressure->m_nResumeBytes = nCapacity / 2;
		}

		// A retained range can pin two partial buffers per receive in flight. Reserve an extra buffer for progress
		// and set resume so releasing everything except the hold restarts the stream.
		if (umint nHold = pBackpressure->m_nConsumerHoldBytes)
		{
			umint nSlack = (2 * nDepth + 1) * nBufferBytes;
			umint nPinnable = fg_Min(nHold, TCLimitsInt<umint>::mc_Max - nSlack) + nSlack - nBufferBytes;
			pBackpressure->m_nLimitBytes = fg_Max(pBackpressure->m_nLimitBytes, nPinnable + nBufferBytes);
			pBackpressure->m_nResumeBytes = fg_Max(pBackpressure->m_nResumeBytes, nPinnable);
		}
	}

#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_TraceEnabled())
		mp_pIo->f_Trace("stream-start", _pRegistration->m_pToken, _pRegistration->m_Handle, (uint32)nBufferBytes);
#endif

	return true;
}

// Recheck capacity after parking so a concurrent release cannot strand a stream below its limit.
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
				if (mp_pIo->f_StatsEnabled())
					mp_pIo->m_IocpStats.m_nStreamParks.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
				if (mp_pIo->f_TraceEnabled())
					mp_pIo->f_Trace("stream-park", _pRegistration->m_pToken, _pRegistration->m_Handle, 0);
#endif

				return {};
			}

			// Keep the parked flag while earlier posts still need resume; clearing it could permanently lose receive slots.
			if (!_pRegistration->m_bStreamNeedsRearm)
				Backpressure->m_bParked.f_Exchange(0, NAtomic::gc_MemoryOrder_AcquireRelease);
		}
	}

	NStorage::TCSharedPointer<CIocpStreamBuffer> pBuffer = fg_Construct();
	pBuffer->m_pData = _pRegistration->m_pRecycler->f_TryTake();
	if (!pBuffer->m_pData)
	{
		pBuffer->m_pData = (uint8 *)NMemory::CDefaultAllocator::f_Alloc(_pRegistration->m_nRecvBufferBytes);

#if DMibConfig_IoDebug_Enable
		if (mp_pIo->f_StatsEnabled())
			mp_pIo->m_IocpStats.m_nRecvBufferAllocs.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif
	}
#if DMibConfig_IoDebug_Enable
	else if (mp_pIo->f_StatsEnabled())
		mp_pIo->m_IocpStats.m_nRecvBufferReuses.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif
	pBuffer->m_nDataBytes = _pRegistration->m_nRecvBufferBytes;
	pBuffer->m_pRecycler = _pRegistration->m_pRecycler;
	pBuffer->m_pBackpressure = Backpressure;
	// Charged when the first bytes are delivered from it; a buffer the kernel never fills
	// releases nothing
	pBuffer->m_nCharged = 0;

	return pBuffer;
}

// False means backpressure parked the post. Every issued receive owes exactly one packet or inline completion.
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
	if (mp_pIo->f_StatsEnabled())
		mp_pIo->m_IocpStats.m_nRecvPosts.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif

	DWORD nReceived = 0;
	int Ret = WSARecv((SOCKET)_pRegistration->m_Handle, &_Op.m_Buffer, 1, &nReceived, &_Op.m_Flags, &_Op.m_Overlapped, nullptr);

	// Capture last error before tracing can overwrite it; pending receives still own their buffers.
	int Error = Ret == 0 ? 0 : WSAGetLastError();

#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_TraceEnabled())
		mp_pIo->f_Trace(Ret == 0 ? "recv-post-done" : "recv-post", _pRegistration->m_pToken, _pRegistration->m_Handle, Ret == 0 ? (uint32)nReceived : (uint32)Error);
#endif

	if (Ret == 0)
	{
		if (_pRegistration->m_bSkipSuccess)
		{
			_Op.m_bCompleted = true;
			_Op.m_nBytes = nReceived;
			fp_QueueInlineCompletion(_pRegistration);

#if DMibConfig_IoDebug_Enable
			if (mp_pIo->f_StatsEnabled())
				mp_pIo->m_IocpStats.m_nRecvInline.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif
		}

		return true;
	}

	if (Error == WSA_IO_PENDING)
		return true;

	_Op.m_bCompleted = true;
	_Op.m_Status = gc_NtStatus_Unsuccessful;
	_Op.m_Error = Error;
	fp_QueueInlineCompletion(_pRegistration);

	return true;
}

void CIoLoop_Iocp::fp_ArmStream(CIocpRegistration *_pRegistration)
{
	_pRegistration->m_bStreamNeedsRearm = false;

#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_TraceEnabled())
	{
		mp_pIo->f_Trace
			(
				"stream-arm"
				, _pRegistration->m_pToken
				, _pRegistration->m_Handle
				, (uint32)_pRegistration->m_nRecvDepth | ((uint32)_pRegistration->m_RecvOps[0].m_bIssued << 8) | ((uint32)_pRegistration->m_nRecvsInFlight << 16)
			)
		;
	}
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

void CIoLoop_Iocp::fp_ResumeStream(CIocpRegistration *_pRegistration)
{
	if (!_pRegistration->m_bStreamStarted || _pRegistration->m_bStreamEnded || _pRegistration->m_bDeregistering)
		return;

	if (!_pRegistration->m_bStreamNeedsRearm)
		return;

#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_StatsEnabled())
		mp_pIo->m_IocpStats.m_nStreamResumes.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
	if (mp_pIo->f_TraceEnabled())
		mp_pIo->f_Trace("stream-rearm", _pRegistration->m_pToken, _pRegistration->m_Handle, 0);
#endif

	fp_ArmStream(_pRegistration);
}

// Report in receive issue order regardless of packet order. Bound inline reposts per call so an always-readable socket
// cannot bypass the per-pass fairness limit; retain completed leftovers on the inline list.
void CIoLoop_Iocp::fp_ReportCompletedRecvs(CIocpRegistration *_pRegistration, umint &_nReported)
{
	umint nReportedHere = 0;
	while (_pRegistration->m_pRecvHead && _pRegistration->m_pRecvHead->m_bCompleted)
	{
		if (nReportedHere == gc_IocpMaxRecvReportsPerCall)
		{
			fp_QueueInlineCompletion(_pRegistration);
			break;
		}
		++nReportedHere;

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
			if (mp_pIo->f_StatsEnabled())
				mp_pIo->m_IocpStats.m_nRecvErrors.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
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

			// A receive can consume EOF before AFD reports it. Deliver stream terminal first, then readiness disconnect state.
			if (!_pRegistration->m_bDeregistering)
			{
#if DMibConfig_IoDebug_Enable
				if (mp_pIo->f_TraceEnabled())
					mp_pIo->f_Trace("stream-eof-close", _pRegistration->m_pToken, _pRegistration->m_Handle, 0);
#endif

				_pRegistration->m_bDisconnectReported = true;
				fp_DispatchReadiness(_pRegistration, NSys::EIoLoopEvent::mc_ReadClosed, 0, _nReported);
			}

			continue;
		}

		umint nBytes = pOp->m_nBytes;

#if DMibConfig_IoDebug_Enable
		if (mp_pIo->f_TraceEnabled())
			mp_pIo->f_Trace("recv-report", _pRegistration->m_pToken, _pRegistration->m_Handle, (uint32)nBytes);
		if (mp_pIo->f_StatsEnabled())
		{
			mp_pIo->m_IocpStats.m_nRecvSegments.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
			mp_pIo->m_IocpStats.m_nRecvBytes.f_FetchAdd(nBytes, NAtomic::gc_MemoryOrder_Relaxed);
			mp_pIo->m_IocpStats.m_RecvSizeBuckets[fg_GetHighestBitSet(nBytes)].f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
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

		// Consumers read only the delivered head while the kernel fills the untouched tail. Fill buffers before retirement
		// or partial unused capacity can exceed the backpressure allowance.
		if (pOp->m_Offset == Buffer.m_nDataBytes)
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

// Merge adjacent arrivals to reduce sink calls; flush before nonadjacent data and at pass end.
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
	mp_StreamFlushQueue.f_Insert(_pRegistration);
}

// Pin registration across sink callbacks so callback-triggered teardown cannot free it.
void CIoLoop_Iocp::fp_DeliverStreamSegment(CIocpRegistration *_pRegistration, NSys::CIoStreamSegment &&_Segment, umint &_nReported)
{
	++_pRegistration->m_nOutstanding;
	++mp_nDispatchDepth;
	_pRegistration->m_fStreamSink(fg_Move(_Segment));
	--mp_nDispatchDepth;
	--_pRegistration->m_nOutstanding;
	++_nReported;
}

void CIoLoop_Iocp::fp_FlushStreamSegment(CIocpRegistration *_pRegistration, umint &_nReported)
{
	if (!_pRegistration->m_bStreamSegmentPending)
		return;

	_pRegistration->m_bStreamSegmentPending = false;
	mp_StreamFlushQueue.f_Remove(_pRegistration);

	NSys::CIoStreamSegment Segment = fg_Move(_pRegistration->m_PendingStreamSegment);
	_pRegistration->m_PendingStreamSegment = NSys::CIoStreamSegment();

	fp_DeliverStreamSegment(_pRegistration, fg_Move(Segment), _nReported);
}

// Walk from the front so callbacks that restage entries append behind remaining work.
void CIoLoop_Iocp::fp_FlushStreamSegments(umint &_nReported)
{
	while (CIocpRegistration *pRegistration = mp_StreamFlushQueue.f_GetFirst())
		fp_FlushStreamSegment(pRegistration, _nReported);
}

// All termination paths must deliver exactly one terminal segment.
void CIoLoop_Iocp::fp_EndStream(CIocpRegistration *_pRegistration, NSys::EIoCompletionStatus _Status, int32 _Error, umint &_nReported)
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

// Consumer references outlive stream teardown and release backpressure normally; the weak resume callback tolerates owner destruction.
void CIoLoop_Iocp::fp_ReleaseStream(CIocpRegistration *_pRegistration)
{
	// Unlink staged segments before acknowledgement frees registration; release their buffer charge normally.
	if (_pRegistration->m_bStreamSegmentPending)
	{
		_pRegistration->m_bStreamSegmentPending = false;
		_pRegistration->m_PendingStreamSegment = NSys::CIoStreamSegment();
		mp_StreamFlushQueue.f_Remove(_pRegistration);
	}

	DMibFastCheck(!_pRegistration->m_nRecvsInFlight);
	for (auto &Op : _pRegistration->m_RecvOps)
		Op.m_pBuffer.f_Clear();

	// Mark the recycler dead before late returns; consumers retain its state until their buffers are released.
	if (_pRegistration->m_pRecycler)
	{
		_pRegistration->m_pRecycler->f_Die();
		_pRegistration->m_pRecycler.f_Clear();
	}

	_pRegistration->m_pBackpressure.f_Clear();
	_pRegistration->m_fStreamSink = NSys::FIoStreamSink();
}
