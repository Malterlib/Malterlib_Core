// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Windows_IoLoop_Iocp_Internal.h"
#include "Malterlib_Core_Platform_Windows_Optional.h"

using namespace NMib;
using namespace NMib::NMemory;
using namespace NMib::NSys;

namespace
{
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

// Translate NTSTATUS through the socket provider while the handle remains open; synchronous issue failures already carry WSA errors.
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
					mp_pIo->m_IocpStats.m_nSendPacketLagPendingNs.f_FetchAdd(LagNs, NAtomic::gc_MemoryOrder_Relaxed);
					mp_pIo->m_IocpStats.m_nSendPacketLagPendingOps.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
				}
				else
				{
					mp_pIo->m_IocpStats.m_nSendPacketLagSyncNs.f_FetchAdd(LagNs, NAtomic::gc_MemoryOrder_Relaxed);
					mp_pIo->m_IocpStats.m_nSendPacketLagSyncOps.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
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
		fp_DispatchReadiness(pRegistration, NSys::EIoLoopEvent::mc_Error, (int)NLocal::g_OptionalFunctions.m_fRtlNtStatusToDosError(_pOp->m_Status), _nReported);
		return;
	}

	// AFD can report unrequested state. Mask against armed interest and rearm when none matches; the completed transition cannot retrigger it.
	ULONG ReportedEvents = _pOp->m_PollInfo.m_nHandles ? _pOp->m_PollInfo.m_Handles[0].m_Events : 0;
	ULONG AfdEvents = ReportedEvents & _pOp->m_ArmedEvents;

#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_StatsEnabled())
		mp_pIo->m_IocpStats.m_nPollEvents.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
	if (mp_pIo->f_TraceEnabled())
	{
		mp_pIo->f_Trace
			(
				(AfdEvents & (gc_AfdPoll_Disconnect | gc_AfdPoll_Abort | gc_AfdPoll_LocalClose | gc_AfdPoll_ConnectFail)) ? "close-event" : "poll-event"
				, pRegistration->m_pToken
				, pRegistration->m_Handle
				, ReportedEvents | (_pOp->m_ArmedEvents << 16)
			)
		;
	}
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

// List registrations once and retain an obligation while listed so removal cannot free a pending inline completion.
void CIoLoop_Iocp::fp_QueueInlineCompletion(CIocpRegistration *_pRegistration)
{
	if (_pRegistration->m_InlineLink.f_IsInList())
		return;

	++_pRegistration->m_nOutstanding;
	mp_InlineCompletions.f_Insert(_pRegistration);
}

// Bound inline reports per pass so synchronously completing streams cannot starve the worker queue; leftovers prevent parking.
void CIoLoop_Iocp::fp_DrainInlineCompletions(umint &_nReported)
{
	if (mp_bDrainingInline)
		return;

	mp_bDrainingInline = true;

	umint nProcessed = 0;
	while (nProcessed < gc_IocpMaxInlinePerPass)
	{
		CIocpRegistration *pRegistration = mp_InlineCompletions.f_Pop();
		if (!pRegistration)
			break;
		++nProcessed;

		DMibCheck(pRegistration->m_nOutstanding != 0);
		--pRegistration->m_nOutstanding;

		fp_ReportCompletedSends(pRegistration, _nReported);
		fp_ReportCompletedRecvs(pRegistration, _nReported);
		fp_TryAcknowledge(pRegistration, _nReported);
	}

	mp_bDrainingInline = false;
}
