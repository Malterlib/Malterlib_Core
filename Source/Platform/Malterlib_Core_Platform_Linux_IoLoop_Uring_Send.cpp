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

// Whether this send may hand the kernel its pages instead of copying them. The peer decides it,
// once: a local peer parks the pages in its own receive queue, where they stay pinned until it
// reads and where the copy they replace was never going over a wire anyway. Small sends pay more
// for the pinning and the second completion than the copy costs them
bool CIoLoop_IoUring::fp_IsZeroCopyEligible(CUringRegistration *_pRegistration, umint _nBytes)
{
	if (!CIoUringRing::fs_SendZeroCopySupported())
		return false;

	// Below this the pin plus the extra completion costs more than the copy it replaces
	constexpr umint c_nMinZeroCopyBytes = 16 * 1024;

	if (_nBytes < c_nMinZeroCopyBytes)
		return false;

	fg_UringProbePeerClass(_pRegistration);

	return _pRegistration->m_bZeroCopyEligible;
}

// Publishes one transfer's spans into the registration's send ring and records it, creating the
// ring on the first send. False means the ring has no room for this transfer's spans right now —
// the caller re-pends the operation and the coverage that frees entries retries it. A ring that
// cannot be created fails the transfer loudly: the probe passed at startup, so this is resource
// exhaustion, not absence of the feature
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
		if (pRingMem)
		{
			if (!mp_FreeBgids.f_IsEmpty())
				Bgid = mp_FreeBgids.f_Pop();
			else
				Bgid = mp_NextBgid++;

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

	umint nVectors = (umint)_pOp->m_MsgHdr.msg_iovlen;
	if (pRing->m_nEntriesOutstanding + nVectors > pRing->m_nRingEntries)
		return false;

	umint nBytes = 0;
	for (umint iVector = 0; iVector < nVectors; ++iVector)
	{
		pRing->f_PublishSpan(_pOp->m_IoVecs[iVector].iov_base, _pOp->m_IoVecs[iVector].iov_len);
		nBytes += _pOp->m_IoVecs[iVector].iov_len;
	}
	pRing->f_CommitTail();
	pRing->m_nEntriesOutstanding += nVectors;

	CUringSendRecord &Record = pRing->m_Records.f_Insert();
	Record.m_nBytes = nBytes;
	Record.m_nEntries = nVectors;
	Record.m_fOnComplete = fg_Move(_pOp->m_fOnComplete);
	Record.m_fOnBufferReleased = fg_Move(_pOp->m_fOnBufferReleased);

#if DMibConfig_IoDebug_Enable
	if (fg_UringStatsEnabled())
	{
		if (_pOp->m_EnqueueStamp)
		{
			g_UringStats.m_nSendSubmitLagNs.f_FetchAdd(fg_UringStatsNow() - _pOp->m_EnqueueStamp, NAtomic::gc_MemoryOrder_Relaxed);
			g_UringStats.m_nSendSubmitLagOps.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		}
		g_UringStats.m_nSendBytesRequested.f_FetchAdd(nBytes, NAtomic::gc_MemoryOrder_Relaxed);
		g_UringStats.m_nSendPublishes.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		if (nBytes)
			g_UringStats.m_SendSizeBuckets[fg_GetHighestBitSet(nBytes)].f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
	}
#endif

	fg_DeleteObject(NMemory::CDefaultAllocator(), _pOp);

	if (!_pRegistration->m_pSendOp)
		fp_ArmSendBundle(_pRegistration);

	return true;
}

// Places the standing bundle send. It carries no data — the kernel snapshots the published ring
// entries at issue and finishes each internally — so re-arming after a completion has nothing to
// gather and happens in the same pass that reaped it
void CIoLoop_IoUring::fp_ArmSendBundle(CUringRegistration *_pRegistration)
{
	DMibFastCheck(!_pRegistration->m_pSendOp && _pRegistration->m_pSendRing);

	CUringIoOp *pOp = fg_ConstructObject<CUringIoOp>(NMemory::CDefaultAllocator());
	pOp->m_pRegistration = _pRegistration;
	pOp->m_bBundle = true;
	_pRegistration->m_pSendOp = pOp;

#if DMibConfig_IoDebug_Enable
	if (fg_UringStatsEnabled())
	{
		g_UringStats.m_nSendOps.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		if (_pRegistration->m_SendIdleStamp)
		{
			g_UringStats.m_nSendIdleNs.f_FetchAdd(fg_UringStatsNow() - _pRegistration->m_SendIdleStamp, NAtomic::gc_MemoryOrder_Relaxed);
			g_UringStats.m_nSendIdleGaps.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
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
	Sqe.m_OpFlags = MSG_NOSIGNAL;
	Sqe.m_UserData = (uint64)(umint)pOp | gc_UringTag_SendOp;

	++_pRegistration->m_nOutstanding;
}

// Turns a bundle completion's bytes into the callers': records complete in publish order, each
// firing its completion and — the kernel copied at send, nothing references the spans — its
// release, in the contract's order
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

// The connection is over: every record still waiting hears it, covered bytes reported with the
// terminal status, and the ring forgets them
void CIoLoop_IoUring::fp_FailSendRecords(CUringRegistration *_pRegistration, NSys::EIoCompletionStatus _Status, int32 _Error, umint &_nReported)
{
	CUringSendRing *pRing = _pRegistration->m_pSendRing;
	if (!pRing)
		return;

	_pRegistration->m_bSendPublishStalled = false;

	while (!pRing->m_Records.f_IsEmpty())
	{
		CUringSendRecord &Record = pRing->m_Records.f_GetFirst();
		pRing->m_nEntriesOutstanding -= Record.m_nEntries;

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

// Tears the send ring down at acknowledgement time; records were failed by the cancellation
// path before this runs
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

// A zero copy send stops being any registration's obligation once its result is in, so this is
// what still knows about it. Swap removal keeps the operation's own index honest
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

	_pOp->m_iNotifyPending = ~umint(0);
}

umint CIoLoop_IoUring::f_GetCompletionSendDepth() const
{
	return f_SupportsCompletionIo() ? fg_UringSendDepth() : 1;
}

// Prompt unless zero copy can still apply: a kernel without it releases at the result, and so
// does a registration whose peer probe settled on local. Before the probe the answer has to be
// the cautious one, since the first send is what runs it
bool CIoLoop_IoUring::f_SendReleaseIsPrompt(NSys::CIoLoopRegistration const *_pRegistration) const
{
	if (!CIoUringRing::fs_SendZeroCopySupported())
		return true;

	auto *pRegistration = static_cast<CUringRegistration const *>(_pRegistration);
	return pRegistration->m_bZeroCopyProbed && !pRegistration->m_bZeroCopyEligible;
}

bool CIoLoop_IoUring::f_SubmitSendVectored(NSys::CIoLoopRegistration *_pRegistration, NSys::CIoSpan const *_pSpans, umint _nSpans, NSys::FIoCompletion &&_fOnComplete, NSys::FIoBufferReleased &&_fOnBufferReleased)
{
	if (!f_SupportsCompletionIo())
		return false;

	CUringIoOp *pOp = fg_ConstructObject<CUringIoOp>(NMemory::CDefaultAllocator());
	pOp->m_pRegistration = static_cast<CUringRegistration *>(_pRegistration);

	umint nVectors = 0;
	for (umint iSpan = 0; iSpan < _nSpans && nVectors < gc_UringMaxSendVectors; ++iSpan)
	{
		if (!_pSpans[iSpan].m_nBytes)
			continue;

		pOp->m_IoVecs[nVectors].iov_base = (void *)_pSpans[iSpan].m_pData;
		pOp->m_IoVecs[nVectors].iov_len = _pSpans[iSpan].m_nBytes;
		++nVectors;
	}

	if (!nVectors)
	{
		fg_DeleteObject(NMemory::CDefaultAllocator(), pOp);
		return false;
	}

	fg_MemClear(&pOp->m_MsgHdr, sizeof(pOp->m_MsgHdr));
	pOp->m_MsgHdr.msg_iov = pOp->m_IoVecs;
	pOp->m_MsgHdr.msg_iovlen = (decltype(pOp->m_MsgHdr.msg_iovlen))nVectors;
	pOp->m_fOnComplete = fg_Move(_fOnComplete);
	pOp->m_fOnBufferReleased = fg_Move(_fOnBufferReleased);
#if DMibConfig_IoDebug_Enable
	if (fg_UringStatsEnabled())
		pOp->m_EnqueueStamp = fg_UringStatsNow();
#endif

	// The raw registration pointer needs no reference of its own while the operation waits in
	// the queue: every enqueue — sends here, stream starts and resumes alike — is sequenced by
	// the socket's owner, which queues its last operation before it requests removal, and the
	// acknowledgement that frees a registration sweeps this queue first (see the drain in
	// fp_Iterate). Even the backpressure resume, triggered by buffer releases on arbitrary
	// threads, reaches this queue only through the owning actor and its validity checks
	bool bOwnerThread = fp_IsOwnerThread();
	{
		DMibLock(mp_IoOpLock);
		mp_PendingIoOps.f_Insert(pOp);
	}

	if (!bOwnerThread)
		fp_SignalWake();

	return true;
}
