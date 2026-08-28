// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Linux_IoLoop_Uring_Internal.h"
#include "Malterlib_Core_Platform_Linux_IoSubSystem.h"
#include <Mib/Time/Stopwatch>
#include "Malterlib_Core_Platform_Linux_TcpInfo.h"
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

// Use zero-copy only for eligible peers and sends large enough to amortize page pinning and notification.
bool CIoLoop_IoUring::fp_IsZeroCopyEligible(CUringRegistration *_pRegistration, umint _nBytes)
{
	if (!mp_pIo->m_UringCaps.m_bSendZeroCopy)
		return false;

	// Below this the pin plus the extra completion costs more than the copy it replaces
	constexpr umint c_nMinZeroCopyBytes = 16 * 1024;

	if (_nBytes < c_nMinZeroCopyBytes)
		return false;

	fg_UringProbePeerClass(mp_pIo, _pRegistration);

	return _pRegistration->m_bZeroCopyEligible.f_Load(NAtomic::gc_MemoryOrder_Relaxed);
}

// Refuse after exhausting the 16-bit group namespace; never reuse an active ID.
bool CIoLoop_IoUring::fp_AllocateBgid(uint16 &o_Bgid)
{
	if (!mp_FreeBgids.f_IsEmpty())
	{
		o_Bgid = mp_FreeBgids.f_PopBack();
		return true;
	}

	if (mp_nNextBgid > TCLimitsInt<uint16>::mc_Max)
		return false;

	o_Bgid = uint16(mp_nNextBgid++);
	return true;
}

// False means ring room is temporarily insufficient; defer and retry in order.
// Ring-creation failure is resource exhaustion after a successful capability probe and must fail the transfer.
bool CIoLoop_IoUring::fp_PublishSend(CUringRegistration *_pRegistration, CUringIoOp *_pOp, umint &_nReported)
{
	CUringSendRing *pRing = _pRegistration->m_pSendRing;
	if (!pRing)
	{
		umint nEntries = 1024;
		umint nRingBytes = fg_UringRingBytes(nEntries);
		void *pRingMem = fg_UringAllocRing(nRingBytes);

		uint16 Bgid = 0;
		bool bRegistered = false;
		if (pRingMem && fp_AllocateBgid(Bgid))
		{
			CIoUringBufReg Reg;
			fg_MemClear(&Reg, sizeof(Reg));
			Reg.m_RingAddr = (uint64)(umint)pRingMem;
			Reg.m_nRingEntries = (uint32)nEntries;
			Reg.m_Bgid = Bgid;
			bRegistered = CIoUringRing::fs_Register(mp_Ring.m_RingFd, gc_IoUringRegister_PbufRing, &Reg, 1) == 0;
			if (!bRegistered)
				mp_FreeBgids.f_Insert(Bgid);
		}

		if (!bRegistered)
		{
			if (pRingMem)
				CDefaultAllocator::f_Free(pRingMem, nRingBytes);

			++_pRegistration->m_nOutstanding;
			_pOp->m_fOnComplete(NSys::CIoCompletion{.m_Error = ENOMEM, .m_Status = NSys::EIoCompletionStatus::mc_Error});
			--_pRegistration->m_nOutstanding;
			++_nReported;
			if (_pOp->m_fOnBufferReleased)
				_pOp->m_fOnBufferReleased();
			fg_DeleteObject(NMemory::CDefaultAllocator(), _pOp);

			return true;
		}

		pRing = fg_ConstructObject<CUringSendRing>(CDefaultAllocator());
		pRing->m_pRingMem = pRingMem;
		pRing->m_nRingMemSize = nRingBytes;
		pRing->m_pRingEntries = (CIoUringBuf *)pRingMem;
		pRing->m_pRingTail = (uint16 *)((uint8 *)pRingMem + gc_UringPbufRingTailOffset);
		pRing->m_nRingEntries = nEntries;
		pRing->m_Bgid = Bgid;
		_pRegistration->m_pSendRing = pRing;
	}

	constexpr umint c_MaxEntryBytes = TCLimitsInt<uint32>::mc_Max;
	umint nVectors = (umint)_pOp->m_MsgHdr.msg_iovlen;
	umint nEntries = 0;
	umint nBytes = 0;
	for (umint iVector = 0; iVector < nVectors; ++iVector)
	{
		// Divided rather than rounded up by addition, which wraps where umint is 32 bits wide
		umint nVectorBytes = _pOp->m_IoVecs[iVector].iov_len;
		nEntries += nVectorBytes / c_MaxEntryBytes + (nVectorBytes % c_MaxEntryBytes != 0);
		nBytes += nVectorBytes;
	}

	if (pRing->m_nEntriesOutstanding + nEntries > pRing->m_nRingEntries)
		return false;

	// The kernel selects at most the signed 32-bit range per bundle and cuts the entry that crosses it, consuming it whole
	// with the cut applied and the rest of it never sent. Submission caps one operation below that, so an empty ring always takes it
	constexpr umint c_MaxRingBytes = umint(TCLimitsInt<int32>::mc_Max);
	if (pRing->m_nBytesOutstanding + nBytes > c_MaxRingBytes)
		return false;

	for (umint iVector = 0; iVector < nVectors; ++iVector)
	{
		uint8 const *pData = (uint8 const *)_pOp->m_IoVecs[iVector].iov_base;
		umint nRemaining = _pOp->m_IoVecs[iVector].iov_len;
		while (nRemaining)
		{
			umint nChunk = fg_Min(nRemaining, c_MaxEntryBytes);
			pRing->f_PublishSpan(pData, nChunk);
			pData += nChunk;
			nRemaining -= nChunk;
		}
	}
	pRing->f_CommitTail();
	pRing->m_nEntriesOutstanding += nEntries;
	pRing->m_nBytesOutstanding += nBytes;

	CUringSendRecord &Record = pRing->m_Records.f_Insert();
	Record.m_nBytes = nBytes;
	Record.m_nEntries = nEntries;
	Record.m_fOnComplete = fg_Move(_pOp->m_fOnComplete);
	Record.m_fOnBufferReleased = fg_Move(_pOp->m_fOnBufferReleased);

#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_StatsEnabled())
	{
		if (_pOp->m_EnqueueStamp)
		{
			mp_pIo->m_UringStats.m_nSendSubmitLagNs.f_FetchAdd(fg_UringStatsNow() - _pOp->m_EnqueueStamp, NAtomic::gc_MemoryOrder_Relaxed);
			mp_pIo->m_UringStats.m_nSendSubmitLagOps.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		}
		mp_pIo->m_UringStats.m_nSendBytesRequested.f_FetchAdd(nBytes, NAtomic::gc_MemoryOrder_Relaxed);
		mp_pIo->m_UringStats.m_nSendPublishes.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		if (nBytes)
			mp_pIo->m_UringStats.m_SendSizeBuckets[fg_Min(umint(fg_GetHighestBitSet(nBytes)), umint(32))].f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
	}
#endif

	fg_DeleteObject(NMemory::CDefaultAllocator(), _pOp);

	if (!_pRegistration->m_pSendOp)
		fp_ArmSendBundle(_pRegistration);

	return true;
}

// Issue snapshots published entries, so rearm remaining records in the same reap pass.
void CIoLoop_IoUring::fp_ArmSendBundle(CUringRegistration *_pRegistration)
{
	DMibFastCheck(!_pRegistration->m_pSendOp && _pRegistration->m_pSendRing);

	CUringIoOp *pOp = fg_ConstructObject<CUringIoOp>(NMemory::CDefaultAllocator());
	pOp->m_pRegistration = _pRegistration;
	pOp->m_bBundle = true;
	_pRegistration->m_pSendOp = pOp;

#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_StatsEnabled())
	{
		mp_pIo->m_UringStats.m_nSendOps.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		if (_pRegistration->m_SendIdleStamp)
		{
			mp_pIo->m_UringStats.m_nSendIdleNs.f_FetchAdd(fg_UringStatsNow() - _pRegistration->m_SendIdleStamp, NAtomic::gc_MemoryOrder_Relaxed);
			mp_pIo->m_UringStats.m_nSendIdleGaps.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
			_pRegistration->m_SendIdleStamp = 0;
		}
	}
#endif

	CIoUringSqe &Sqe = fp_PrepareSqe();
	Sqe.m_Opcode = gc_IoUringOp_Send;
	Sqe.m_Fd = _pRegistration->m_Handle;
	Sqe.m_Flags = gc_IoUringSqeFlag_BufferSelect;
	Sqe.m_IoPrio = gc_IoUringRecvSend_Bundle;
	Sqe.m_BufIndex = _pRegistration->m_pSendRing->m_Bgid;
	// Bundles must finish each entry; retiring a short entry would lose its unsent suffix.
	Sqe.m_OpFlags = MSG_NOSIGNAL | MSG_WAITALL;
	Sqe.m_UserData = (uint64)(umint)pOp | gc_UringTag_SendOp;

	++_pRegistration->m_nOutstanding;
}

// Resolve in publish order, completion before release; copied bundle sends retain no kernel reference afterwards.
void CIoLoop_IoUring::fp_CoverSendRecords(CUringRegistration *_pRegistration, umint _nBytes, umint &_nReported)
{
	CUringSendRing *pRing = _pRegistration->m_pSendRing;
	umint nRemaining = _nBytes;

	while (nRemaining)
	{
		DMibFastCheck(!pRing->m_Records.f_IsEmpty());
		if (pRing->m_Records.f_IsEmpty())
			break;

		CUringSendRecord &Record = pRing->m_Records.f_GetFirst();

		umint nTake = fg_Min(nRemaining, Record.m_nBytes - Record.m_nCovered);
		Record.m_nCovered += nTake;
		nRemaining -= nTake;

		if (Record.m_nCovered < Record.m_nBytes)
			break;

		pRing->m_nEntriesOutstanding -= Record.m_nEntries;
		pRing->m_nBytesOutstanding -= Record.m_nBytes;
		_pRegistration->m_bSendPublishStalled = false;

		NSys::FIoCompletion fOnComplete = fg_Move(Record.m_fOnComplete);
		NSys::FIoBufferReleased fOnBufferReleased = fg_Move(Record.m_fOnBufferReleased);
		umint nRecordBytes = Record.m_nBytes;
		pRing->m_Records.f_Remove(Record);

		++_pRegistration->m_nOutstanding;
		fOnComplete(NSys::CIoCompletion{.m_nBytes = nRecordBytes});
		--_pRegistration->m_nOutstanding;
		++_nReported;

		if (fOnBufferReleased)
			fOnBufferReleased();
	}
}

// A bundle can end early when CQ space runs out. Only the kernel's consumed position proves which entries
// were selected; bytes missing from those entries cannot be recovered by rearming the ring.
// The check must precede every rearm: a rearm over a consumed but unsent record would credit the next
// record's bytes to it. Nothing short of the kernel's position proves an entry unconsumed.
int32 CIoLoop_IoUring::fp_GetSendBundleError(CUringRegistration *_pRegistration)
{
	CUringSendRing *pRing = _pRegistration->m_pSendRing;
	if (!pRing || pRing->m_Records.f_IsEmpty())
		return 0;

	CIoUringBufStatus Status = {};
	Status.m_Bgid = pRing->m_Bgid;
	if (CIoUringRing::fs_Register(mp_Ring.m_RingFd, gc_IoUringRegister_PbufStatus, &Status, 1) != 0)
		return errno;

	return pRing->f_HasLostEntries(uint16(Status.m_Head)) ? ECONNRESET : 0;
}

// Report terminal status with already-covered bytes for every pending record.
void CIoLoop_IoUring::fp_FailSendRecords(CUringRegistration *_pRegistration, NSys::EIoCompletionStatus _Status, int32 _Error, umint &_nReported)
{
	CUringSendRing *pRing = _pRegistration->m_pSendRing;
	if (!pRing)
		return;

	if (_Status == NSys::EIoCompletionStatus::mc_Error)
		_pRegistration->m_SendError = _Error;

	_pRegistration->m_bSendPublishStalled = false;

	while (!pRing->m_Records.f_IsEmpty())
	{
		CUringSendRecord &Record = pRing->m_Records.f_GetFirst();
		pRing->m_nEntriesOutstanding -= Record.m_nEntries;
		pRing->m_nBytesOutstanding -= Record.m_nBytes;

		NSys::FIoCompletion fOnComplete = fg_Move(Record.m_fOnComplete);
		NSys::FIoBufferReleased fOnBufferReleased = fg_Move(Record.m_fOnBufferReleased);
		umint nCovered = Record.m_nCovered;
		pRing->m_Records.f_Remove(Record);

		++_pRegistration->m_nOutstanding;
		fOnComplete(NSys::CIoCompletion{.m_nBytes = nCovered, .m_Error = _Error, .m_Status = _Status});
		--_pRegistration->m_nOutstanding;
		++_nReported;

		if (fOnBufferReleased)
			fOnBufferReleased();
	}
}

// Release at acknowledgement, after cancellation has failed remaining records.
void CIoLoop_IoUring::fp_ReleaseSendRing(CUringRegistration *_pRegistration)
{
	CUringSendRing *pRing = _pRegistration->m_pSendRing;
	if (!pRing)
		return;

	DMibFastCheck(pRing->m_Records.f_IsEmpty());

	CIoUringBufReg Unreg;
	fg_MemClear(&Unreg, sizeof(Unreg));
	Unreg.m_Bgid = pRing->m_Bgid;
	CIoUringRing::fs_Register(mp_Ring.m_RingFd, gc_IoUringRegister_UnregisterPbufRing, &Unreg, 1);
	mp_FreeBgids.f_Insert(pRing->m_Bgid);

	CDefaultAllocator::f_Free(pRing->m_pRingMem, pRing->m_nRingMemSize);
	fg_DeleteObject(CDefaultAllocator(), pRing);
	_pRegistration->m_pSendRing = nullptr;
}

// Notifications outlive registration obligations; maintain the swapped operation's list index.
void CIoLoop_IoUring::fp_ReleaseNotifyPending(CUringIoOp *_pOp)
{
	umint iPending = _pOp->m_iNotifyPending;
	if (iPending == ~umint(0))
		return;

	DMibFastCheck(iPending < mp_NotifyPending.f_GetLen() && mp_NotifyPending[iPending] == _pOp);

	CUringIoOp *pLast = mp_NotifyPending[mp_NotifyPending.f_GetLen() - 1];
	mp_NotifyPending[iPending] = pLast;
	pLast->m_iNotifyPending = iPending;
	mp_NotifyPending.f_SetLen(mp_NotifyPending.f_GetLen() - 1);
	mp_nNotifyPendingBytes -= _pOp->m_nRequested;

	// Sample release lag only for low-occupancy sends whose registration still exists.
	if (auto *pRegistration = _pOp->m_pNotifyRegistration)
	{
		--pRegistration->m_nNotifyPending;
		if (_pOp->m_ReleaseLagIssueStamp)
		{
			uint64 Now = uint64(NTime::NPlatform::fg_TimerRaw_PreciseGet());
			NSys::fg_SampleIoSendReleaseLag(pRegistration->m_SendWindow, Now - _pOp->m_ReleaseLagIssueStamp, Now, mp_pIo->m_nWindowShrinkAfterTicks);
		}
	}
#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_StatsEnabled() && _pOp->m_EnqueueStamp)
	{
		uint64 LagNs = fg_UringStatsNow() - _pOp->m_EnqueueStamp;
		mp_pIo->m_UringStats.m_nSendNotifLagNs.f_FetchAdd(LagNs, NAtomic::gc_MemoryOrder_Relaxed);
		mp_pIo->m_UringStats.m_nSendNotifLagOps.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		if (LagNs > mp_pIo->m_UringStats.m_nSendNotifLagMaxNs.f_Load(NAtomic::gc_MemoryOrder_Relaxed))
			mp_pIo->m_UringStats.m_nSendNotifLagMaxNs.f_Store(LagNs, NAtomic::gc_MemoryOrder_Relaxed);
	}
#endif

	_pOp->m_iNotifyPending = ~umint(0);
}

umint CIoLoop_IoUring::f_GetCompletionSendDepth() const
{
	return f_SupportsCompletionIo() ? mp_pIo->f_SendDepth() : 1;
}

// Before peer probing, return false conservatively because the first send can still choose zero-copy.
bool CIoLoop_IoUring::f_SendReleaseIsPrompt(NSys::CIoLoopRegistration const *_pRegistration) const
{
	if (!mp_pIo->m_UringCaps.m_bSendZeroCopy)
		return true;

	auto *pRegistration = static_cast<CUringRegistration const *>(_pRegistration);
	return pRegistration->m_bZeroCopyProbed.f_Load(NAtomic::gc_MemoryOrder_Acquire) && !pRegistration->m_bZeroCopyEligible.f_Load(NAtomic::gc_MemoryOrder_Relaxed);
}

// Owner-sequenced with sends; this ceiling bounds the consumer while TCP retains its own kernel limits.
void CIoLoop_IoUring::f_SetSendWindow(NSys::CIoLoopRegistration *_pRegistration, umint _nBytes)
{
	auto &Window = static_cast<CUringRegistration *>(_pRegistration)->m_SendWindow;
	Window.m_nMaxBytes = _nBytes;

	// A lowered ceiling must also lower the start and effective window.
	if (_nBytes && Window.m_nStartBytes > _nBytes)
		Window.m_nStartBytes = _nBytes;
	if (_nBytes && Window.m_nEffectiveBytes > _nBytes)
		Window.m_nEffectiveBytes = _nBytes;
}

// Owner sequencing protects window state; only cross-thread lag epochs need locking.
bool CIoLoop_IoUring::f_IsSendWindowFull(NSys::CIoLoopRegistration *_pRegistration, umint _nUnreleasedBytes, umint _nStartBytes)
{
	auto *pRegistration = static_cast<CUringRegistration *>(_pRegistration);

	// Copied sends release promptly; only zero-copy needs an acknowledgement-sized window.
	if (!pRegistration->m_bZeroCopyEligible.f_Load(NAtomic::gc_MemoryOrder_Relaxed))
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

	uint64 Now = uint64(NTime::NPlatform::fg_TimerRaw_PreciseGet());
	if (Window.m_QueryStamp && Now - Window.m_QueryStamp < mp_pIo->m_nWindowQueryIntervalTicks)
		return true;

	Window.m_QueryStamp = Now;

	umint nDeliveryRate = 0;
	bool bAppLimited = false;
	if (fg_Linux_QueryPathDeliveryRate((int)pRegistration->m_Handle, nDeliveryRate, bAppLimited))
		NSys::fg_ConsiderIoSendWindowGrowth(Window, nDeliveryRate, bAppLimited, Now, mp_pIo->m_nTicksPerSecond, mp_pIo->m_nWindowShrinkAfterTicks);

	return _nUnreleasedBytes >= Window.m_nEffectiveBytes;
}

auto CIoLoop_IoUring::f_SubmitSendVectored
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

	CUringIoOp *pOp = fg_ConstructObject<CUringIoOp>(NMemory::CDefaultAllocator());
	pOp->m_pRegistration = static_cast<CUringRegistration *>(_pRegistration);

	// Cap at signed MAX_RW_COUNT rounded down for 64-KiB pages. Larger requests would appear as short failures
	// even when the kernel sent its entire imported prefix. End the gather at the cap to preserve order.
	constexpr umint c_MaxRequested = umint(TCLimitsInt<int32>::mc_Max) & ~umint(0xFFFF);
	umint nVectors = 0;
	umint nRequested = 0;
	for (umint iSpan = 0; iSpan < _nSpans && nVectors < gc_UringMaxSendVectors; ++iSpan)
	{
		if (!_pSpans[iSpan].m_nBytes)
			continue;

		umint nBytes = fg_Min(_pSpans[iSpan].m_nBytes, c_MaxRequested - nRequested);
		if (!nBytes)
			break;

		pOp->m_IoVecs[nVectors].iov_base = (void *)_pSpans[iSpan].m_pData;
		pOp->m_IoVecs[nVectors].iov_len = nBytes;
		nRequested += nBytes;
		++nVectors;

		if (nBytes < _pSpans[iSpan].m_nBytes)
			break;
	}

	if (!nVectors)
	{
		fg_DeleteObject(NMemory::CDefaultAllocator(), pOp);
		return 0;
	}

	auto *pRegistration = static_cast<CUringRegistration *>(_pRegistration);
	if (nRequested > pRegistration->m_SendWindow.m_nLargestSendBytes)
		pRegistration->m_SendWindow.m_nLargestSendBytes = nRequested;

	fg_MemClear(&pOp->m_MsgHdr, sizeof(pOp->m_MsgHdr));
	pOp->m_MsgHdr.msg_iov = pOp->m_IoVecs;
	pOp->m_MsgHdr.msg_iovlen = (decltype(pOp->m_MsgHdr.msg_iovlen))nVectors;
	pOp->m_fOnComplete = fg_Move(_fOnComplete);
	pOp->m_fOnBufferReleased = fg_Move(_fOnBufferReleased);
#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_StatsEnabled())
		pOp->m_EnqueueStamp = fg_UringStatsNow();
#endif

	// Owner ordering puts every submission before removal; acknowledgement sweeps this queue before freeing its raw registration pointers.
	bool bOwnerThread = fp_IsOwnerThread();
	{
		DMibLock(mp_IoOpLock);
		mp_PendingIoOps.f_Insert(pOp);
	}

	if (!bOwnerThread)
		fp_SignalWake();

	return nRequested;
}
