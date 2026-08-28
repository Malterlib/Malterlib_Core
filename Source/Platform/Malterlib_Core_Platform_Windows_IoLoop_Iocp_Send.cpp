// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Windows_IoLoop_Iocp_Internal.h"
#include "Malterlib_Core_Platform_Windows_TcpInfo.h"
#include <Mib/Time/Stopwatch>

using namespace NMib;
using namespace NMib::NMemory;
using namespace NMib::NSys;

auto CIoLoop_Iocp::f_SubmitSendVectored
	(
		NSys::CIoLoopRegistration *_pRegistration
		, NSys::CIoSpan const *_pSpans
		, umint _nSpans
		, NSys::FIoCompletion &&_fOnComplete
		, NSys::FIoBufferReleased &&_fOnBufferReleased
	)
	-> umint
{
	if (!f_SupportsCompletionIo())
		return 0;

	CIocpSendOp *pOp = fg_ConstructObject<CIocpSendOp>(CDefaultAllocator());
	pOp->m_pRegistration = static_cast<CIocpRegistration *>(_pRegistration);
	pOp->m_Kind = EIocpOpKind::mc_Send;

	// The buffers name the caller's spans in place: the kernel copies them at issue. A span
	// beyond what one buffer can describe, or one that would take the operation past the 32 bit
	// transferred count the packet reports, is clamped and ends the gather; what was described
	// is what the answer says was scheduled, and the caller offers the remainder again
	umint nBuffers = 0;
	bool bClamped = false;
	for (umint iSpan = 0; iSpan < _nSpans && nBuffers < NSys::gc_IoLoopMaxSubmitSpans && !bClamped; ++iSpan)
	{
		umint nBytes = _pSpans[iSpan].m_nBytes;
		if (!nBytes)
			continue;

		if (nBytes > umint(ULONG_MAX) - pOp->m_nRequested)
		{
			nBytes = umint(ULONG_MAX) - pOp->m_nRequested;
			bClamped = true;
			if (!nBytes)
				break;
		}

		pOp->m_Buffers[nBuffers].buf = (CHAR *)_pSpans[iSpan].m_pData;
		pOp->m_Buffers[nBuffers].len = (ULONG)nBytes;
		pOp->m_nRequested += nBytes;
		++nBuffers;
	}

	if (!nBuffers)
	{
		fg_DeleteObject(CDefaultAllocator(), pOp);
		return 0;
	}

	pOp->m_nBuffers = (DWORD)nBuffers;
	pOp->m_fOnComplete = fg_Move(_fOnComplete);
	pOp->m_fOnBufferReleased = fg_Move(_fOnBufferReleased);

	// The window's granularity sample; sequenced by the socket's owner like the asks
	if (pOp->m_nRequested > pOp->m_pRegistration->m_SendWindow.m_nLargestSendBytes)
		pOp->m_pRegistration->m_SendWindow.m_nLargestSendBytes = pOp->m_nRequested;
#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_StatsEnabled())
		pOp->m_EnqueueStamp = fg_IocpStatsNow();
#endif

	// The raw registration pointer needs no reference of its own while the operation waits in
	// the queue: every enqueue — sends here, stream starts and resumes alike — is sequenced by
	// the socket's owner, which queues its last operation before it requests removal, and the
	// acknowledgement that frees a registration sweeps this queue first
	auto *pPending = fg_ConstructObject<CIocpPendingOp>(CDefaultAllocator());
	pPending->m_pRegistration = pOp->m_pRegistration;
	pPending->m_pSendOp = pOp;
	fp_QueuePendingOp(pPending);

	return pOp->m_nRequested;
}

void CIoLoop_Iocp::f_SetSendWindow(NSys::CIoLoopRegistration *_pRegistration, umint _nBytes)
{
	// The ask side reads the ceiling directly; setting the window falls under the same owner
	// sequencing as the sends, so no queue trip is needed for it
	auto &Window = static_cast<CIocpRegistration *>(_pRegistration)->m_SendWindow;
	Window.m_nMaxBytes = _nBytes;

	// A ceiling lowered under a window the path had already grown brings the window down with
	// it; the ask compares against the effective size, and the shrink rule never goes below the
	// start
	if (_nBytes && Window.m_nStartBytes > _nBytes)
		Window.m_nStartBytes = _nBytes;
	if (_nBytes && Window.m_nEffectiveBytes > _nBytes)
		Window.m_nEffectiveBytes = _nBytes;

	auto *pOp = fg_ConstructObject<CIocpPendingOp>(CDefaultAllocator());
	pOp->m_pRegistration = static_cast<CIocpRegistration *>(_pRegistration);
	pOp->m_bSendWindow = true;
	pOp->m_nBytes = _nBytes;

	fp_QueuePendingOp(pOp);
}

// The consumer’s ask before it gathers another batch; see f_IsSendWindowFull in the interface.
// Owner-sequenced with the sends, so the window state needs no locking beyond the lag epochs'
// own, which the loop's samples share
bool CIoLoop_Iocp::f_IsSendWindowFull(NSys::CIoLoopRegistration *_pRegistration, umint _nUnreleasedBytes, umint _nStartBytes)
{
	auto *pRegistration = static_cast<CIocpRegistration *>(_pRegistration);
	if (!pRegistration->f_SendCompletesOnAck())
		return false;

	auto &Window = pRegistration->m_SendWindow;
	if (!Window.m_nMaxBytes)
		return false;

	if (!Window.m_nEffectiveBytes)
	{
		Window.m_nStartBytes = fg_Clamp(_nStartBytes, umint(1), Window.m_nMaxBytes);
		Window.m_nEffectiveBytes = Window.m_nStartBytes;
	}

	if (_nUnreleasedBytes < Window.m_nEffectiveBytes)
		return false;

	// Full while more wants out is the moment to ask the path, at most every 10 ms
	uint64 Now = uint64(NTime::NPlatform::fg_TimerRaw_PreciseGet());
	if (Window.m_QueryStamp && Now - Window.m_QueryStamp < mp_pIo->m_nWindowQueryIntervalTicks)
		return true;

	Window.m_QueryStamp = Now;

	umint nDeliveryRate = 0;
	bool bAppLimited = false;
	if (fg_Windows_QueryPathDeliveryRate((SOCKET)pRegistration->m_Handle, Window.m_LastBytesOut, Window.m_LastStamp, nDeliveryRate, bAppLimited))
		NSys::fg_ConsiderIoSendWindowGrowth(Window, nDeliveryRate, bAppLimited, Now, mp_pIo->m_nTicksPerSecond, mp_pIo->m_nWindowShrinkAfterTicks);

	return _nUnreleasedBytes >= Window.m_nEffectiveBytes;
}

// Loop thread: the send takes its place at the tail of the registration's FIFO and is issued
// at once if a slot is free
void CIoLoop_Iocp::fp_AppendSend(CIocpRegistration *_pRegistration, CIocpSendOp *_pOp, umint &_nReported)
{
	_pOp->m_pNext = nullptr;
	if (_pRegistration->m_pSendTail)
		_pRegistration->m_pSendTail->m_pNext = _pOp;
	else
		_pRegistration->m_pSendHead = _pOp;
	_pRegistration->m_pSendTail = _pOp;

	if (!_pRegistration->m_pSendNextToIssue)
		_pRegistration->m_pSendNextToIssue = _pOp;

	fp_IssueDeferredSends(_pRegistration, _nReported);
}

void CIoLoop_Iocp::fp_IssueDeferredSends(CIocpRegistration *_pRegistration, umint &_nReported)
{
	umint nDepth = mp_pIo->f_SendDepth();

	// A socket whose sends finish at the acknowledgement holds its window in the unacknowledged
	// bytes, so those are issued within the window in bytes rather than within a count of sends
	bool bByBytes = _pRegistration->f_SendCompletesOnAck() && _pRegistration->m_nSendWindowBytes;
	while
	(
		_pRegistration->m_pSendNextToIssue
		&& (bByBytes ? _pRegistration->m_nSendBytesInFlight < _pRegistration->m_nSendWindowBytes : _pRegistration->m_nSendsInFlight < nDepth)
	)
	{
		CIocpSendOp *pOp = _pRegistration->m_pSendNextToIssue;
		_pRegistration->m_pSendNextToIssue = pOp->m_pNext;

		fp_IssueSend(_pRegistration, pOp, _nReported);
	}

#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_StatsEnabled() && _pRegistration->m_pSendNextToIssue)
		mp_pIo->m_IocpStats.m_nSendDeferred.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif
}

// Hands one send to the kernel. Every outcome leaves exactly one completion owed: a packet for
// a pended or, without skip mode, a synchronously finished send; an inline entry for one that
// finished at issue under skip mode or failed at issue. Counted from here to its report
// A send the kernel has accepted on a socket that finishes sends only at the acknowledgement:
// the whole request is reported as completed here, in issue order — which is submission
// order, so the consumers' byte accounting sees what it would from a buffered send — and
// the packet, when it comes, runs the release alone. An overlapped stream send transfers all
// of its bytes unless the connection fails, and a failure after this report reaches the
// consumer through the socket, as it does for a buffered send whose bytes the kernel took
void CIoLoop_Iocp::fp_ReportSendAccepted(CIocpRegistration *_pRegistration, CIocpSendOp *_pOp, umint &_nReported)
{
	if (!_pRegistration->f_SendCompletesOnAck() || _pOp->m_bCompletionReported)
		return;

	_pOp->m_bCompletionReported = true;

#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_TraceEnabled())
		mp_pIo->f_Trace("send-accepted", _pRegistration->m_pToken, _pRegistration->m_Handle, (uint32)_pOp->m_nRequested);
#endif

	NSys::CIoCompletion Result;
	Result.m_nBytes = _pOp->m_nRequested;

	++_pRegistration->m_nOutstanding;
	++mp_nDispatchDepth;
	_pOp->m_fOnComplete(Result);
	--mp_nDispatchDepth;
	--_pRegistration->m_nOutstanding;
	++_nReported;
}

void CIoLoop_Iocp::fp_IssueSend(CIocpRegistration *_pRegistration, CIocpSendOp *_pOp, umint &_nReported)
{
	fg_MemClear(&_pOp->m_Overlapped, sizeof(_pOp->m_Overlapped));
	_pOp->m_Status = 0;
	_pOp->m_nBytes = 0;
	_pOp->m_Error = 0;
	_pOp->m_bCompleted = false;
	_pOp->m_bIssued = true;
	++_pRegistration->m_nOutstanding;
	++_pRegistration->m_nSendsInFlight;
	_pRegistration->m_nSendBytesInFlight += _pOp->m_nRequested;

	// The packet is the release on an acknowledgement-completing socket; its lag from here
	// feeds the window's sliding minimum. Only a send with at most one ahead of it samples:
	// one issued into a standing queue measures the queue, and a window sized by its own
	// occupancy only grows the occupancy
	if (_pRegistration->f_SendCompletesOnAck() && _pRegistration->m_nSendsInFlight <= 2)
		_pOp->m_ReleaseLagIssueStamp = uint64(NTime::NPlatform::fg_TimerRaw_PreciseGet());
#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_StatsEnabled())
	{
		if (_pRegistration->m_nSendsInFlight > mp_pIo->m_IocpStats.m_nSendMaxInFlight.f_Load(NAtomic::gc_MemoryOrder_Relaxed))
			mp_pIo->m_IocpStats.m_nSendMaxInFlight.f_Store(_pRegistration->m_nSendsInFlight, NAtomic::gc_MemoryOrder_Relaxed);
		if (_pRegistration->m_nSendBytesInFlight > mp_pIo->m_IocpStats.m_nSendMaxBytesInFlight.f_Load(NAtomic::gc_MemoryOrder_Relaxed))
			mp_pIo->m_IocpStats.m_nSendMaxBytesInFlight.f_Store(_pRegistration->m_nSendBytesInFlight, NAtomic::gc_MemoryOrder_Relaxed);
	}
#endif

#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_StatsEnabled())
	{
		if (_pOp->m_EnqueueStamp)
		{
			mp_pIo->m_IocpStats.m_nSendSubmitLagNs.f_FetchAdd(fg_IocpStatsNow() - _pOp->m_EnqueueStamp, NAtomic::gc_MemoryOrder_Relaxed);
			mp_pIo->m_IocpStats.m_nSendSubmitLagOps.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		}

		if (_pRegistration->m_SendIdleStamp)
		{
			mp_pIo->m_IocpStats.m_nSendIdleNs.f_FetchAdd(fg_IocpStatsNow() - _pRegistration->m_SendIdleStamp, NAtomic::gc_MemoryOrder_Relaxed);
			mp_pIo->m_IocpStats.m_nSendIdleGaps.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
			_pRegistration->m_SendIdleStamp = 0;
		}

		mp_pIo->m_IocpStats.m_nSendOps.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		mp_pIo->m_IocpStats.m_nSendBytesRequested.f_FetchAdd(_pOp->m_nRequested, NAtomic::gc_MemoryOrder_Relaxed);
		mp_pIo->m_IocpStats.m_SendSizeBuckets[fg_Min(umint(fg_GetHighestBitSet(_pOp->m_nRequested)), umint(32))].f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
	}
#endif

#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_StatsEnabled())
		_pOp->m_IssueStamp = fg_IocpStatsNow();
#endif

	DWORD nSent = 0;
	int Ret = WSASend((SOCKET)_pRegistration->m_Handle, _pOp->m_Buffers, _pOp->m_nBuffers, &nSent, 0, &_pOp->m_Overlapped, nullptr);

#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_TraceEnabled())
		mp_pIo->f_Trace(Ret == 0 ? "send-issue-done" : "send-issue", _pRegistration->m_pToken, _pRegistration->m_Handle, Ret == 0 ? (uint32)nSent : (uint32)WSAGetLastError());
#endif

	if (Ret == 0)
	{
		if (_pRegistration->m_bSkipSuccess)
		{
			_pOp->m_bCompleted = true;
			_pOp->m_nBytes = nSent;
			fp_QueueInlineCompletion(_pRegistration);

#if DMibConfig_IoDebug_Enable
			if (mp_pIo->f_StatsEnabled())
				mp_pIo->m_IocpStats.m_nSendInline.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif
		}
		else
			fp_ReportSendAccepted(_pRegistration, _pOp, _nReported);

		return;
	}

	int Error = WSAGetLastError();
	if (Error == WSA_IO_PENDING)
	{
#if DMibConfig_IoDebug_Enable
		if (mp_pIo->f_StatsEnabled())
		{
			mp_pIo->m_IocpStats.m_nSendPendingAtIssue.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
			_pOp->m_bPendingAtIssue = true;
		}
#endif

		fp_ReportSendAccepted(_pRegistration, _pOp, _nReported);
		return;
	}

	// Failed at issue: nothing reached the kernel and no packet follows. Reported through the
	// FIFO like any other result, so it still lands in submission order
	_pOp->m_bCompleted = true;
	_pOp->m_Status = gc_NtStatus_Unsuccessful;
	_pOp->m_Error = Error;
	fp_QueueInlineCompletion(_pRegistration);
}

// Reports from the head while the head is complete: a later send's completion that arrived
// first waits in place, so submit order is report order. The released functor runs right after
// the completion — the kernel copied the buffers at issue — and the freed slot issues the next
// deferred send. The head is re-read after every callback, since a callback may submit
void CIoLoop_Iocp::fp_ReportCompletedSends(CIocpRegistration *_pRegistration, umint &_nReported)
{
	while (_pRegistration->m_pSendHead && _pRegistration->m_pSendHead->m_bCompleted)
	{
		CIocpSendOp *pOp = _pRegistration->m_pSendHead;
		_pRegistration->m_pSendHead = pOp->m_pNext;
		if (!_pRegistration->m_pSendHead)
			_pRegistration->m_pSendTail = nullptr;
		if (_pRegistration->m_pSendNextToIssue == pOp)
			_pRegistration->m_pSendNextToIssue = pOp->m_pNext;

		NSys::CIoCompletion Result;
		if (pOp->m_bIssued)
		{
			pOp->m_bIssued = false;
			--_pRegistration->m_nSendsInFlight;
			_pRegistration->m_nSendBytesInFlight -= pOp->m_nRequested;
			DMibCheck(_pRegistration->m_nOutstanding != 0);
			--_pRegistration->m_nOutstanding;

			if (pOp->m_Status == gc_NtStatus_Cancelled)
				Result.m_Status = NSys::EIoCompletionStatus::mc_Cancelled;
			else if (pOp->m_Status != gc_NtStatus_Success)
			{
				Result.m_Status = NSys::EIoCompletionStatus::mc_Error;
				Result.m_Error = fp_OpError(_pRegistration, pOp);
			}
			else if (pOp->m_nBytes < pOp->m_nRequested)
			{
				// A stream send completes whole or with an error; the kernel ending one short is
				// the connection going away under it, and the contract is that Done means all of
				// the scheduled bytes, so it is reported as the failure it is. The bytes that did
				// leave travel with it, so a teardown's truncation point stays known
				Result.m_nBytes = pOp->m_nBytes;
				Result.m_Status = NSys::EIoCompletionStatus::mc_Error;
				Result.m_Error = WSAECONNRESET;
#if DMibConfig_IoDebug_Enable
				if (mp_pIo->f_StatsEnabled())
					mp_pIo->m_IocpStats.m_nSendShort.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif
			}
			else
				Result.m_nBytes = pOp->m_nBytes;
		}
		else
		{
			// Never issued: cancelled in place by the deregistration
			Result.m_Status = NSys::EIoCompletionStatus::mc_Cancelled;
		}

#if DMibConfig_IoDebug_Enable
		if (mp_pIo->f_StatsEnabled())
		{
			if (Result.m_Status == NSys::EIoCompletionStatus::mc_Done)
				mp_pIo->m_IocpStats.m_nSendBytesSent.f_FetchAdd(Result.m_nBytes, NAtomic::gc_MemoryOrder_Relaxed);
			else if (Result.m_Status == NSys::EIoCompletionStatus::mc_Error)
				mp_pIo->m_IocpStats.m_nSendErrors.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);

			if (!_pRegistration->m_nSendsInFlight)
				_pRegistration->m_SendIdleStamp = fg_IocpStatsNow();
		}
#endif

#if DMibConfig_IoDebug_Enable
		if (mp_pIo->f_TraceEnabled())
			mp_pIo->f_Trace("send-report", _pRegistration->m_pToken, _pRegistration->m_Handle, (uint32)Result.m_nBytes | ((uint32)Result.m_Status << 24));
#endif

		if (pOp->m_ReleaseLagIssueStamp)
		{
			uint64 Now = uint64(NTime::NPlatform::fg_TimerRaw_PreciseGet());
			NSys::fg_SampleIoSendReleaseLag(_pRegistration->m_SendWindow, Now - pOp->m_ReleaseLagIssueStamp, Now, mp_pIo->m_nWindowShrinkAfterTicks);
		}

		++_pRegistration->m_nOutstanding;
		++mp_nDispatchDepth;
		// Reported already for a send accepted on an acknowledgement-completing socket; what the
		// packet says then is the release, and a failure it carries reaches the consumer through
		// the socket instead
		if (!pOp->m_bCompletionReported)
			pOp->m_fOnComplete(Result);
		if (pOp->m_fOnBufferReleased)
			pOp->m_fOnBufferReleased();
		--mp_nDispatchDepth;
		--_pRegistration->m_nOutstanding;
		++_nReported;

		fg_DeleteObject(CDefaultAllocator(), pOp);
	}

	if (!_pRegistration->m_bDeregistering)
		fp_IssueDeferredSends(_pRegistration, _nReported);
}

// Sends that never reached the kernel are cancelled in place; they report in order behind the
// issued ones, whose cancelled completions the kernel delivers
void CIoLoop_Iocp::fp_CancelDeferredSends(CIocpRegistration *_pRegistration)
{
	for (CIocpSendOp *pOp = _pRegistration->m_pSendNextToIssue; pOp; pOp = pOp->m_pNext)
	{
		pOp->m_bCompleted = true;
		pOp->m_Status = gc_NtStatus_Cancelled;
	}

	_pRegistration->m_pSendNextToIssue = nullptr;
}

bool CIoLoop_Iocp::f_SendReleaseIsPrompt(NSys::CIoLoopRegistration const *_pRegistration) const
{
	return !static_cast<CIocpRegistration const *>(_pRegistration)->f_SendCompletesOnAck();
}

void CIoLoop_Iocp::fp_ReleaseSends(CIocpRegistration *_pRegistration)
{
	// Nothing can remain: every issued send was reported before the count reached zero, and
	// every deferred one was cancelled in place and reported behind them
	DMibFastCheck(!_pRegistration->m_pSendHead && !_pRegistration->m_nSendsInFlight);
}
