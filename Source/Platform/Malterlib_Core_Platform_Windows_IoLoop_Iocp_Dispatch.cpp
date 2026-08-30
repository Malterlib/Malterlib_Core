// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Windows_IoLoop_Iocp_Internal.h"

using namespace NMib;
using namespace NMib::NMemory;
using namespace NMib::NSys;

namespace
{
	// AFD's event bits to the loop's normalized vocabulary
	NSys::EIoLoopEvent fg_IoLoopEventsFromAfd(ULONG _AfdEvents)
	{
		using NSys::EIoLoopEvent;

		EIoLoopEvent Events = EIoLoopEvent::mc_None;
		if (_AfdEvents & (gc_AfdPoll_Receive | gc_AfdPoll_Accept))
			Events |= EIoLoopEvent::mc_Read;
		if (_AfdEvents & gc_AfdPoll_Send)
			Events |= EIoLoopEvent::mc_Write;
		if (_AfdEvents & gc_AfdPoll_Disconnect)
			Events |= EIoLoopEvent::mc_ReadClosed;
		if (_AfdEvents & (gc_AfdPoll_Abort | gc_AfdPoll_LocalClose))
			Events |= EIoLoopEvent::mc_Hup;
		if (_AfdEvents & gc_AfdPoll_ConnectFail)
			Events |= EIoLoopEvent::mc_Error;

		return Events;
	}
}

// The WSA error a failed transfer completed with. A failure at issue was recorded then; a
// failure the kernel reported is translated by the provider from the status it left in the
// overlapped, which the socket — still open, by contract, until the acknowledgement — answers
int CIoLoop_Iocp::fp_OpError(CIocpRegistration *_pRegistration, CIocpOp *_pOp)
{
	if (_pOp->m_Error)
		return _pOp->m_Error;

	if (_pOp->m_Status == gc_NtStatus_Success)
		return 0;

	DWORD nBytes = 0;
	DWORD Flags = 0;
	if (!WSAGetOverlappedResult((SOCKET)_pRegistration->m_Handle, &_pOp->m_Overlapped, &nBytes, FALSE, &Flags))
		return WSAGetLastError();

	return 0;
}

void CIoLoop_Iocp::fp_DispatchEntry(OVERLAPPED_ENTRY const &_Entry, umint &_nReported)
{
	// A wake carries no operation: the caller drains its queue when this pass returns
	if (!_Entry.lpOverlapped)
		return;

	auto *pOp = (CIocpOp *)_Entry.lpOverlapped;
	pOp->m_Status = (NTSTATUS)_Entry.Internal;
	pOp->m_nBytes = _Entry.dwNumberOfBytesTransferred;

	DMibFastCheck(pOp->m_Kind == EIocpOpKind::mc_Poll ? _Entry.lpCompletionKey == gc_IocpKey_Afd : _Entry.lpCompletionKey != gc_IocpKey_Afd);

	fp_DispatchOp(pOp, _nReported);
}

void CIoLoop_Iocp::fp_DispatchOp(CIocpOp *_pOp, umint &_nReported)
{
	CIocpRegistration *pRegistration = _pOp->m_pRegistration;

	switch (_pOp->m_Kind)
	{
	case EIocpOpKind::mc_Poll:
		fp_DispatchPoll(static_cast<CIocpPollOp *>(_pOp), _nReported);
		break;

	case EIocpOpKind::mc_Send:
		_pOp->m_bCompleted = true;
#if DMibConfig_IoDebug_Enable
		if (mp_pIo->f_StatsEnabled())
		{
			auto *pSend = static_cast<CIocpSendOp *>(_pOp);
			if (pSend->m_IssueStamp)
			{
				uint64 LagNs = fg_IocpStatsNow() - pSend->m_IssueStamp;
				if (pSend->m_bPendingAtIssue)
				{
					g_IocpStats.m_nSendPacketLagPendingNs.f_FetchAdd(LagNs, NAtomic::gc_MemoryOrder_Relaxed);
					g_IocpStats.m_nSendPacketLagPendingOps.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
				}
				else
				{
					g_IocpStats.m_nSendPacketLagSyncNs.f_FetchAdd(LagNs, NAtomic::gc_MemoryOrder_Relaxed);
					g_IocpStats.m_nSendPacketLagSyncOps.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
				}
			}
		}
#endif
		fp_ReportCompletedSends(pRegistration, _nReported);
		break;

	case EIocpOpKind::mc_Recv:
		_pOp->m_bCompleted = true;
		fp_ReportCompletedRecvs(pRegistration, _nReported);
		break;
	}

	fp_TryAcknowledge(pRegistration, _nReported);
}

void CIoLoop_Iocp::fp_DispatchPoll(CIocpPollOp *_pOp, umint &_nReported)
{
	CIocpRegistration *pRegistration = _pOp->m_pRegistration;

	DMibCheck(pRegistration->m_nOutstanding != 0);
	--pRegistration->m_nOutstanding;
	_pOp->m_bIssued = false;
	pRegistration->m_bPollArmed = false;
	pRegistration->m_bPollCancelRequested = false;

	if (_pOp->m_Status == gc_NtStatus_Cancelled)
	{
		// Cancelled to change what it asks for, or by the deregistration; the former re-arms
		fp_UpdatePoll(pRegistration, _nReported);
		return;
	}

	if (_pOp->m_Status != gc_NtStatus_Success)
	{
		pRegistration->m_ClosePollState = CIocpRegistration::EClosePoll::mc_Terminal;
		fp_DispatchReadiness(pRegistration, NSys::EIoLoopEvent::mc_Error, (int)fg_IocpNtFunctions().m_fRtlNtStatusToDosError(_pOp->m_Status), _nReported);
		return;
	}

	// A state change completes the poll with everything that holds on the socket — a settled
	// connect answers with the send bit whether asked for or not — so only what this poll asked
	// for counts. A completion with none of it is not a report: the interest still stands and
	// the poll goes back out, which cannot spin because the transition that completed it is over
	// and a fresh arm only completes on what it asks for
	ULONG ReportedEvents = _pOp->m_PollInfo.m_nHandles ? _pOp->m_PollInfo.m_Handles[0].m_Events : 0;
	ULONG AfdEvents = ReportedEvents & _pOp->m_ArmedEvents;

#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_StatsEnabled())
		g_IocpStats.m_nPollEvents.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
	if (mp_pIo->f_TraceEnabled())
		mp_pIo->f_Trace((AfdEvents & (gc_AfdPoll_Disconnect | gc_AfdPoll_Abort | gc_AfdPoll_LocalClose | gc_AfdPoll_ConnectFail)) ? "close-event" : "poll-event", pRegistration->m_pToken, pRegistration->m_Handle, ReportedEvents | (_pOp->m_ArmedEvents << 16));
#endif

	// Each direction's report is one-shot; the close class advances its state so each of its
	// events is asked for at most once more
	if (AfdEvents & (gc_AfdPoll_Receive | gc_AfdPoll_Accept))
		pRegistration->m_bReadWanted = false;
	if (AfdEvents & gc_AfdPoll_Send)
		pRegistration->m_bWriteWanted = false;
	if (AfdEvents & (gc_AfdPoll_Abort | gc_AfdPoll_LocalClose | gc_AfdPoll_ConnectFail))
		pRegistration->m_ClosePollState = CIocpRegistration::EClosePoll::mc_Terminal;
	else if (AfdEvents & gc_AfdPoll_Disconnect)
	{
		pRegistration->m_ClosePollState = CIocpRegistration::EClosePoll::mc_AbortOnly;
		pRegistration->m_bDisconnectReported = true;
	}

	NSys::EIoLoopEvent Events = fg_IoLoopEventsFromAfd(AfdEvents);
	if (Events != NSys::EIoLoopEvent::mc_None)
		fp_DispatchReadiness(pRegistration, Events, 0, _nReported);

	fp_UpdatePoll(pRegistration, _nReported);
}

// Lists a registration whose transfer completed without a packet, once, pinned: the entry holds
// an obligation so the record cannot be acknowledged and freed while it is still named here
void CIoLoop_Iocp::fp_QueueInlineCompletion(CIocpRegistration *_pRegistration)
{
	if (_pRegistration->m_bInlinePending)
		return;

	_pRegistration->m_bInlinePending = true;
	++_pRegistration->m_nOutstanding;
	mp_InlineCompletions.f_InsertLast(_pRegistration);
}

// Transfers that completed without a packet, reported like any other. Walked by index because a
// report can re-post and add to the list; bounded per pass so a stream that keeps completing at
// issue cannot starve the hosting queue — leftovers keep the pass from parking
void CIoLoop_Iocp::fp_DrainInlineCompletions(umint &_nReported)
{
	if (mp_bDrainingInline)
		return;

	mp_bDrainingInline = true;

	umint nProcessed = 0;
	while (mp_iNextInline < mp_InlineCompletions.f_GetLen() && nProcessed < gc_IocpMaxInlinePerPass)
	{
		CIocpRegistration *pRegistration = mp_InlineCompletions[mp_iNextInline];
		++mp_iNextInline;
		++nProcessed;

		pRegistration->m_bInlinePending = false;
		DMibCheck(pRegistration->m_nOutstanding != 0);
		--pRegistration->m_nOutstanding;

		fp_ReportCompletedSends(pRegistration, _nReported);
		fp_ReportCompletedRecvs(pRegistration, _nReported);
		fp_TryAcknowledge(pRegistration, _nReported);
	}

	if (mp_iNextInline == mp_InlineCompletions.f_GetLen())
	{
		mp_InlineCompletions.f_Clear();
		mp_iNextInline = 0;
	}
	else
	{
		mp_InlineCompletions.f_Remove(0, mp_iNextInline);
		mp_iNextInline = 0;
	}

	mp_bDrainingInline = false;
}
