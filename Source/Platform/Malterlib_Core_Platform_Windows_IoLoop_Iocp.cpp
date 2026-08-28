// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Windows_IoLoop_Iocp_Internal.h"
#include "Malterlib_Core_Platform_Windows_Optional.h"

using namespace NMib;
using namespace NMib::NMemory;
using namespace NMib::NSys;

CIocpRegistration::CIocpRegistration()
{
	m_PollOp.m_pRegistration = this;
	m_PollOp.m_Kind = EIocpOpKind::mc_Poll;

	for (auto &RecvOp : m_RecvOps)
	{
		RecvOp.m_pRegistration = this;
		RecvOp.m_Kind = EIocpOpKind::mc_Recv;
	}
}

// For SO_SNDBUF=0 TCP, report acceptance immediately to avoid an acknowledgement round trip per message.
// The later completion packet releases buffers; registration options publish this distinction before dispatch.
bool CIocpRegistration::f_SendCompletesOnAck() const
{
	return m_Options.m_bSendCompletesOnAck;
}

CIoLoop_Iocp::CIoLoop_Iocp()
{
	mp_pIo = &fg_IoSubSystem_Windows();
	// One concurrent thread: the port is parked in by exactly the thread that hosts the loop
	mp_hPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
	mp_bCreated = mp_hPort != nullptr;
}

CIoLoop_Iocp::~CIoLoop_Iocp()
{
	// Owner shutdown must finish all deregistrations and kernel obligations before loop destruction.
	DMibFastCheck(mp_nDeregistering == 0);
	DMibFastCheck(mp_PendingIoOps.f_IsEmpty());
	DMibFastCheck(mp_InlineCompletions.f_IsEmpty());
	DMibFastCheck(mp_StreamFlushQueue.f_IsEmpty());

	for (CIocpAfdGroup *pGroup : mp_AfdGroups)
	{
		CloseHandle(pGroup->m_hAfd);
		fg_DeleteObject(CDefaultAllocator(), pGroup);
	}

	if (mp_hPort)
		CloseHandle(mp_hPort);
}

bool CIoLoop_Iocp::f_IsCreated() const
{
	return mp_bCreated;
}

auto CIoLoop_Iocp::fp_CreateRegistration() -> NSys::CIoLoopRegistration *
{
	return fg_ConstructObject<CIocpRegistration>(CDefaultAllocator());
}

void CIoLoop_Iocp::fp_WakeKernel()
{
#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_StatsEnabled())
		mp_pIo->m_IocpStats.m_nWakePosts.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif

	PostQueuedCompletionStatus(mp_hPort, 0, gc_IocpKey_Wake, nullptr);
}

void CIoLoop_Iocp::f_RequestReadiness(NSys::CIoLoopRegistration *_pRegistration, NSys::EIoLoopEvent _EventMask)
{
	if (_EventMask == NSys::EIoLoopEvent::mc_None)
		return;

	// Only the zero-to-nonzero requester queues a notification; exchange consumes earlier bits without losing later requests.
	NSys::EIoLoopEvent Previous = NSys::EIoLoopEvent(_pRegistration->m_RequestedEvents.f_FetchOr(uint32(_EventMask), NAtomic::gc_MemoryOrder_AcquireRelease));
	if (Previous != NSys::EIoLoopEvent::mc_None)
		return;

	CIoLoopChange Change;
	Change.m_bReadinessRequest = true;
	Change.m_Handle = _pRegistration->m_Handle;
	Change.m_pRegistration = _pRegistration;
	mp_ChangeQueue.f_Push(fg_Move(Change));
	fp_SignalWake();
}

void CIoLoop_Iocp::f_Deregister(NSys::CIoLoopRegistration *_pRegistration)
{
	CIoLoopDeregWait DeregWait;
	DeregWait.m_Event.f_ResetSignaled();

	fp_PushRemoval(_pRegistration, &DeregWait, {});

	if (fp_IsOwnerThread())
	{
		// The owner drives removal through cancellation packets, parking briefly between passes.
		// Never deregister synchronously inside dispatch: its pin prevents the count reaching zero.
		DMibFastCheck(mp_nDispatchDepth == 0);

		while (!DeregWait.m_bDone.f_Load(NAtomic::gc_MemoryOrder_Acquire))
			fp_IterateTimeout(true, 1);
	}
	else
		DeregWait.m_Event.f_Wait();
}

void CIoLoop_Iocp::fp_QueuePendingOp(CIocpPendingOp *_pOp)
{
	// Owner submissions are consumed before its next park. Before ownership is claimed, the durable pending wake selects the conservative path.
	bool bOwnerThread = fp_IsOwnerThread();
	{
		DMibLock(mp_IoOpLock);
		mp_PendingIoOps.f_Insert(_pOp);
	}

	if (!bOwnerThread)
		fp_SignalWake();
}

// AFD handles must post packets even for polls satisfied at issue; do not enable skip-success.
CIocpAfdGroup *CIoLoop_Iocp::fp_AcquireAfdGroup()
{
	for (CIocpAfdGroup *pGroup : mp_AfdGroups)
	{
		if (pGroup->m_nRegistrations < gc_IocpAfdGroupSize)
		{
			++pGroup->m_nRegistrations;
			return pGroup;
		}
	}

	auto const &Nt = NLocal::g_OptionalFunctions;

	wchar_t const *pDeviceName = L"\\Device\\Afd\\Malterlib";
	UNICODE_STRING Name;
	Name.Buffer = (PWSTR)pDeviceName;
	Name.Length = (USHORT)(wcslen(pDeviceName) * sizeof(wchar_t));
	Name.MaximumLength = (USHORT)(Name.Length + sizeof(wchar_t));

	OBJECT_ATTRIBUTES Attributes;
	InitializeObjectAttributes(&Attributes, &Name, 0, nullptr, nullptr);

	IO_STATUS_BLOCK IoStatus;
	HANDLE hAfd = nullptr;
	NTSTATUS Status = Nt.m_fNtCreateFile(&hAfd, SYNCHRONIZE, &Attributes, &IoStatus, nullptr, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, gc_NtFile_Open, 0, nullptr, 0);
	if (Status != gc_NtStatus_Success || !hAfd)
		return nullptr;

	if (!CreateIoCompletionPort(hAfd, mp_hPort, gc_IocpKey_Afd, 0))
	{
		CloseHandle(hAfd);
		return nullptr;
	}

	SetFileCompletionNotificationModes(hAfd, FILE_SKIP_SET_EVENT_ON_HANDLE);

	auto *pGroup = fg_ConstructObject<CIocpAfdGroup>(CDefaultAllocator());
	pGroup->m_hAfd = hAfd;
	pGroup->m_nRegistrations = 1;
	mp_AfdGroups.f_InsertLast(pGroup);

	return pGroup;
}

void CIoLoop_Iocp::fp_ReleaseAfdGroup(CIocpRegistration *_pRegistration)
{
	if (!_pRegistration->m_pAfdGroup)
		return;

	--_pRegistration->m_pAfdGroup->m_nRegistrations;
	_pRegistration->m_pAfdGroup = nullptr;
}

// Completion binding persists for handle lifetime. Rebind through native replacement or reject;
// a different unserviced port would swallow completions. Packets identify operations rather than association keys.
bool CIoLoop_Iocp::fp_Associate(CIocpRegistration *_pRegistration, int &o_Error)
{
	SOCKET Socket = (SOCKET)_pRegistration->m_Handle;

	HANDLE hBase = nullptr;
	DWORD nBytes = 0;
	if (WSAIoctl(Socket, SIO_BASE_HANDLE, nullptr, 0, &hBase, sizeof(hBase), &nBytes, nullptr, nullptr) != 0 || !hBase)
		hBase = (HANDLE)Socket;
	_pRegistration->m_hBase = hBase;

	// Never bound, and no notification modes either: those stick to the handle as well
	if (_pRegistration->m_Options.m_bReadinessOnly)
		return true;

	if (CreateIoCompletionPort((HANDLE)Socket, mp_hPort, (ULONG_PTR)_pRegistration, 0))
		_pRegistration->m_bAssociated = true;
	else
	{
		DWORD Error = GetLastError();
		if (Error != ERROR_INVALID_PARAMETER)
		{
			o_Error = (int)Error;
			return false;
		}

		auto const &Nt = NLocal::g_OptionalFunctions;
		if (Nt.m_fNtSetInformationFile)
		{
			CNtFileCompletionInformation Information;
			Information.m_hPort = mp_hPort;
			Information.m_pKey = _pRegistration;

			IO_STATUS_BLOCK IoStatus;
			NTSTATUS Status = Nt.m_fNtSetInformationFile
				(
					(HANDLE)Socket
					, &IoStatus
					, &Information
					, sizeof(Information)
					, (FILE_INFORMATION_CLASS)gc_NtFileInformation_ReplaceCompletionInformation
				)
			;
			if (Status == gc_NtStatus_Success)
				_pRegistration->m_bAssociated = true;
		}

		if (!_pRegistration->m_bAssociated)
		{
			o_Error = (int)Error;
			return false;
		}
	}

	// Skip-success is safe only without a layered provider. Inherited handles must reassert it regardless of override,
	// because the mode cannot be removed and synchronous transfers may already omit their packets.
	bool bWantSkipSuccess = mp_pIo->f_SkipSuccessEnabled() || _pRegistration->m_Options.m_bInheritedHandle;

	UCHAR Modes = FILE_SKIP_SET_EVENT_ON_HANDLE;
	if (bWantSkipSuccess && hBase == (HANDLE)Socket)
	{
		WSAPROTOCOL_INFOW ProtocolInfo;
		int nProtocolInfo = sizeof(ProtocolInfo);
		if (getsockopt(Socket, SOL_SOCKET, SO_PROTOCOL_INFOW, (char *)&ProtocolInfo, &nProtocolInfo) == 0 && (ProtocolInfo.dwServiceFlags1 & XP1_IFS_HANDLES))
			Modes |= FILE_SKIP_COMPLETION_PORT_ON_SUCCESS;
	}

	if (SetFileCompletionNotificationModes((HANDLE)Socket, Modes))
		_pRegistration->m_bSkipSuccess = (Modes & FILE_SKIP_COMPLETION_PORT_ON_SUCCESS) != 0;

#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_StatsEnabled())
	{
		mp_pIo->m_IocpStats.m_nRegistrations.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		if (_pRegistration->m_bSkipSuccess)
			mp_pIo->m_IocpStats.m_nSkipSuccessSockets.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
	}
#endif

	return true;
}

// Adopt with an unused key; packets identify operations. Rebind existing associations or reject handles whose completions would go elsewhere.
bool CIoLoop_Iocp::f_AdoptHandle(NSys::CIoLoopHandle _Handle, int &o_Error)
{
	if (CreateIoCompletionPort((HANDLE)_Handle, mp_hPort, 0, 0))
		return true;

	o_Error = (int)GetLastError();
	if (o_Error != ERROR_INVALID_PARAMETER)
		return false;

	auto const &Nt = NLocal::g_OptionalFunctions;
	if (!Nt.m_fNtSetInformationFile)
		return false;

	CNtFileCompletionInformation Information;
	Information.m_hPort = mp_hPort;
	Information.m_pKey = nullptr;

	IO_STATUS_BLOCK IoStatus;
	NTSTATUS Status = Nt.m_fNtSetInformationFile((HANDLE)_Handle, &IoStatus, &Information, sizeof(Information), (FILE_INFORMATION_CLASS)gc_NtFileInformation_ReplaceCompletionInformation);
	if (Status == gc_NtStatus_Success)
		return true;

	if (Nt.m_fRtlNtStatusToDosError)
		o_Error = (int)Nt.m_fRtlNtStatusToDosError(Status);

	return false;
}

// Pin registration across consumer callbacks so callback-triggered teardown cannot free it.
void CIoLoop_Iocp::fp_DispatchReadiness(CIocpRegistration *_pRegistration, NSys::EIoLoopEvent _Events, int _Error, umint &_nReported)
{
	++_pRegistration->m_nOutstanding;
	++mp_nDispatchDepth;
	_pRegistration->m_fOnEvents(_pRegistration->m_pToken, _Events, _Error);
	--mp_nDispatchDepth;
	--_pRegistration->m_nOutstanding;
	++_nReported;
}

// Level-at-arm polls catch readiness after would-block. Require fresh directional requests and advance close state to avoid spinning.
// False means issue failed and its error was reported.
bool CIoLoop_Iocp::fp_ArmPoll(CIocpRegistration *_pRegistration, ULONG _AfdEvents, umint &_nReported)
{
	CIocpPollOp &Op = _pRegistration->m_PollOp;
	DMibFastCheck(!Op.m_bIssued && _pRegistration->m_pAfdGroup);

	fg_MemClear(&Op.m_Overlapped, sizeof(Op.m_Overlapped));
	Op.m_Status = 0;
	Op.m_nBytes = 0;
	Op.m_Error = 0;
	Op.m_bCompleted = false;
	Op.m_pGroup = _pRegistration->m_pAfdGroup;
	Op.m_ArmedEvents = _AfdEvents;
	Op.m_PollInfo.m_Timeout.QuadPart = INT64_MAX;
	Op.m_PollInfo.m_nHandles = 1;
	Op.m_PollInfo.m_bExclusive = FALSE;
	Op.m_PollInfo.m_Handles[0].m_Handle = _pRegistration->m_hBase;
	Op.m_PollInfo.m_Handles[0].m_Events = _AfdEvents;
	Op.m_PollInfo.m_Handles[0].m_Status = 0;

	auto const &Nt = NLocal::g_OptionalFunctions;
	NTSTATUS Status = Nt.m_fNtDeviceIoControlFile
		(
			Op.m_pGroup->m_hAfd
			, nullptr
			, nullptr
			, &Op.m_Overlapped
			, (PIO_STATUS_BLOCK)&Op.m_Overlapped
			, gc_AfdIoctl_Poll
			, &Op.m_PollInfo
			, sizeof(Op.m_PollInfo)
			, &Op.m_PollInfo
			, sizeof(Op.m_PollInfo)
		)
	;

	if (Status != gc_NtStatus_Success && Status != gc_NtStatus_Pending)
	{
		// Report a failed arm or the consumer could wait forever without a kernel operation.
		fp_DispatchReadiness(_pRegistration, NSys::EIoLoopEvent::mc_Error, (int)Nt.m_fRtlNtStatusToDosError(Status), _nReported);
		return false;
	}

	Op.m_bIssued = true;
	_pRegistration->m_bPollArmed = true;
	_pRegistration->m_bPollCancelRequested = false;
	++_pRegistration->m_nOutstanding;

#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_StatsEnabled())
		mp_pIo->m_IocpStats.m_nPollArms.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
	if (mp_pIo->f_TraceEnabled())
		mp_pIo->f_Trace(Status == gc_NtStatus_Pending ? "poll-arm" : "poll-arm-immediate", _pRegistration->m_pToken, _pRegistration->m_Handle, (uint32)_AfdEvents);
#endif

	return true;
}

// Combine accept/read interest without knowing socket mode; completed connects report writability.
ULONG CIoLoop_Iocp::fp_WantedPollEvents(CIocpRegistration const *_pRegistration) const
{
	ULONG Events = 0;

	switch (_pRegistration->m_ClosePollState)
	{
	case CIocpRegistration::EClosePoll::mc_Disconnect:
		Events |= gc_AfdPoll_Disconnect | gc_AfdPoll_Abort | gc_AfdPoll_ConnectFail | gc_AfdPoll_LocalClose;
		break;
	case CIocpRegistration::EClosePoll::mc_AbortOnly:
		Events |= gc_AfdPoll_Abort | gc_AfdPoll_LocalClose;
		break;
	case CIocpRegistration::EClosePoll::mc_Terminal:
		break;
	}

	if (_pRegistration->m_bReadWanted && !_pRegistration->m_bCompletionModeRead)
		Events |= gc_AfdPoll_Receive | gc_AfdPoll_Accept;
	if (_pRegistration->m_bWriteWanted && !_pRegistration->m_bCompletionModeWrite)
		Events |= gc_AfdPoll_Send;

	return Events;
}

// Change interest by cancelling then rearming from completion. Every cancel outcome leaves one target packet, so no retry is needed.
void CIoLoop_Iocp::fp_UpdatePoll(CIocpRegistration *_pRegistration, umint &_nReported)
{
	if (_pRegistration->m_bDeregistering || !_pRegistration->m_pAfdGroup)
		return;

	ULONG Wanted = fp_WantedPollEvents(_pRegistration);

	if (!_pRegistration->m_bPollArmed)
	{
		if (Wanted)
			fp_ArmPoll(_pRegistration, Wanted, _nReported);

		return;
	}

	if (_pRegistration->m_PollOp.m_ArmedEvents == Wanted || _pRegistration->m_bPollCancelRequested)
		return;

	fp_CancelPoll(_pRegistration);
}

void CIoLoop_Iocp::fp_ArmRequested(CIocpRegistration *_pRegistration, NSys::EIoLoopEvent _EventMask, umint &_nReported)
{
	if (fg_IsSet(_EventMask, NSys::EIoLoopEvent::mc_Read))
		_pRegistration->m_bReadWanted = true;
	if (fg_IsSet(_EventMask, NSys::EIoLoopEvent::mc_Write))
		_pRegistration->m_bWriteWanted = true;

	// AFD has no event when local shutdown completes a prior half-close; replay held disconnect state for explicit close requests.
	if (fg_IsSet(_EventMask, NSys::EIoLoopEvent::mc_ReadClosed) && _pRegistration->m_bDisconnectReported)
		fp_DispatchReadiness(_pRegistration, NSys::EIoLoopEvent::mc_ReadClosed, 0, _nReported);

	fp_UpdatePoll(_pRegistration, _nReported);
}

void CIoLoop_Iocp::fp_CancelPoll(CIocpRegistration *_pRegistration)
{
	if (!_pRegistration->m_bPollArmed || _pRegistration->m_bPollCancelRequested)
		return;

	_pRegistration->m_bPollCancelRequested = true;

#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_StatsEnabled())
		mp_pIo->m_IocpStats.m_nPollCancels.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif

	CancelIoEx(_pRegistration->m_PollOp.m_pGroup->m_hAfd, &_pRegistration->m_PollOp.m_Overlapped);
}

void CIoLoop_Iocp::fp_CancelOutstanding(CIocpRegistration *_pRegistration, umint &_nReported)
{
	fp_CancelPoll(_pRegistration);

	// The caller retains the open handle until acknowledgement, making handle-wide cancellation safe.
	if (_pRegistration->m_nSendsInFlight || _pRegistration->m_nRecvsInFlight)
		CancelIoEx((HANDLE)_pRegistration->m_Handle, nullptr);

	// Sends that never reached the kernel have nothing to cancel there; marked cancelled in
	// place, they report in order behind whatever is still in flight
	fp_CancelDeferredSends(_pRegistration);
	fp_ReportCompletedSends(_pRegistration, _nReported);

	// A parked stream has no kernel receive to cancel but still owes a terminal segment.
	if (!_pRegistration->m_nRecvsInFlight && _pRegistration->m_bStreamStarted && !_pRegistration->m_bStreamEnded)
		fp_EndStream(_pRegistration, NSys::EIoCompletionStatus::mc_Cancelled, 0, _nReported);
}

NContainer::TCVector<CIocpPendingOp *> CIoLoop_Iocp::fp_TakePendingOps(CIocpRegistration *_pRegistration)
{
	NContainer::TCVector<CIocpPendingOp *> Taken;

	DMibLock(mp_IoOpLock);
	if (mp_PendingIoOps.f_IsEmpty())
		return Taken;

	NContainer::TCVector<CIocpPendingOp *> Kept;
	Kept.f_Reserve(mp_PendingIoOps.f_GetLen());
	for (CIocpPendingOp *pOp : mp_PendingIoOps)
	{
		if (pOp->m_pRegistration == _pRegistration)
			Taken.f_InsertLast(pOp);
		else
			Kept.f_InsertLast(pOp);
	}
	mp_PendingIoOps = fg_Move(Kept);

	return Taken;
}

// Issue owner-ordered final sends before applying removal, matching readiness-send-before-close semantics.
// Removal cancels whatever the kernel cannot accept immediately.
void CIoLoop_Iocp::fp_FlushPendingOpsBeforeRemoval(CIocpRegistration *_pRegistration, umint &_nReported)
{
	NContainer::TCVector<CIocpPendingOp *> Ops = fp_TakePendingOps(_pRegistration);
	for (CIocpPendingOp *pOp : Ops)
		fp_ApplyPendingOp(pOp, _nReported);
}

void CIoLoop_Iocp::fp_SweepPendingOps(CIocpRegistration *_pRegistration)
{
	// Owner-ordered submission/removal can cross iterate boundaries; sweep before freeing the registration identity.
	NContainer::TCVector<CIocpPendingOp *> Swept = fp_TakePendingOps(_pRegistration);

	for (CIocpPendingOp *pOp : Swept)
	{
#if DMibConfig_IoDebug_Enable
		if (mp_pIo->f_TraceEnabled())
			mp_pIo->f_Trace("pending-op-swept", _pRegistration->m_pToken, _pRegistration->m_Handle, pOp->m_bStreamStart ? 1 : pOp->m_bStreamResume ? 2 : 3);
#endif

		if (pOp->m_bStreamStart)
		{
			NSys::CIoStreamSegment Segment;
			Segment.m_Status = NSys::EIoCompletionStatus::mc_Cancelled;
			pOp->m_fSink(fg_Move(Segment));
		}
		else if (pOp->m_pSendOp)
		{
			pOp->m_pSendOp->m_fOnComplete(NSys::CIoCompletion{.m_Status = NSys::EIoCompletionStatus::mc_Cancelled});
			if (pOp->m_pSendOp->m_fOnBufferReleased)
				pOp->m_pSendOp->m_fOnBufferReleased();
			fg_DeleteObject(CDefaultAllocator(), pOp->m_pSendOp);
		}

		fg_DeleteObject(CDefaultAllocator(), pOp);
	}
}

void CIoLoop_Iocp::fp_TryAcknowledge(CIocpRegistration *_pRegistration, umint &_nReported)
{
	if (!_pRegistration->m_bDeregistering || _pRegistration->m_nOutstanding)
		return;

	// At zero obligations, sweep pending work before freeing. Count acknowledgement callbacks so local enqueues cannot be slept past.
	fp_SweepPendingOps(_pRegistration);
	fp_ReleaseSends(_pRegistration);
	fp_ReleaseStream(_pRegistration);
	fp_ReleaseAfdGroup(_pRegistration);
	--mp_nDeregistering;

#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_TraceEnabled())
		mp_pIo->f_Trace("ack", _pRegistration->m_pToken, _pRegistration->m_Handle, 0);
#endif

	CIoLoopDeferredAck Ack{_pRegistration, _pRegistration->m_pDeregWait, fg_Move(_pRegistration->m_fOnDeregistered)};
	fp_RunDeregAcknowledgement(Ack);
	++_nReported;
}

void CIoLoop_Iocp::fp_ProcessChanges(umint &_nReported)
{
	auto Changes = fg_Move(mp_ChangeQueue.f_Take());
	for (auto &Change : Changes)
	{
		auto *pRegistration = static_cast<CIocpRegistration *>(Change.m_pRegistration);

		if (Change.m_bRemove)
		{
			fp_FlushPendingOpsBeforeRemoval(pRegistration, _nReported);

			pRegistration->m_RequestedEvents.f_Exchange(0, NAtomic::gc_MemoryOrder_AcquireRelease);
			pRegistration->m_bDeregistering = true;
			++mp_nDeregistering;
			pRegistration->m_pDeregWait = Change.m_pDeregWait;
			pRegistration->m_fOnDeregistered = fg_Move(Change.m_fOnDeregistered);
#if DMibConfig_IoDebug_Enable
			if (mp_pIo->f_TraceEnabled())
				mp_pIo->f_Trace("deregister", pRegistration->m_pToken, Change.m_Handle, (uint32)pRegistration->m_nOutstanding);
#endif

			fp_CancelOutstanding(pRegistration, _nReported);
			fp_TryAcknowledge(pRegistration, _nReported);
		}
		else if (Change.m_bReadinessRequest)
		{
			// Discard requests only for removed or completion-driven directions; preserve readiness in the other direction.
			NSys::EIoLoopEvent Requested = NSys::EIoLoopEvent(pRegistration->m_RequestedEvents.f_Exchange(0, NAtomic::gc_MemoryOrder_AcquireRelease));

			if (pRegistration->m_bCompletionModeRead)
				Requested &= ~NSys::EIoLoopEvent::mc_Read;
			if (pRegistration->m_bCompletionModeWrite)
				Requested &= ~NSys::EIoLoopEvent::mc_Write;

			fp_ArmRequested(pRegistration, Requested, _nReported);
		}
		else
		{
#if DMibConfig_IoDebug_Enable
			if (mp_pIo->f_TraceEnabled())
				mp_pIo->f_Trace("register", pRegistration->m_pToken, Change.m_Handle, uint32(pRegistration->m_EventMask));
#endif

			int Error = 0;
			pRegistration->m_pAfdGroup = fp_AcquireAfdGroup();
			if (!pRegistration->m_pAfdGroup || !fp_Associate(pRegistration, Error))
			{
				// Report rejected registration as dispatched work; otherwise the connection or work enqueued by its callback can stall forever.
				fp_ReleaseAfdGroup(pRegistration);
				fp_DispatchReadiness(pRegistration, NSys::EIoLoopEvent::mc_Error, Error ? Error : ERROR_NOT_ENOUGH_MEMORY, _nReported);

				continue;
			}

			// Level-at-arm polls replay initial readiness; keep close interest for idle and completion-mode streams.
			NSys::EIoLoopEvent Requested = NSys::EIoLoopEvent(pRegistration->m_RequestedEvents.f_Exchange(0, NAtomic::gc_MemoryOrder_AcquireRelease));
			fp_ArmRequested(pRegistration, Requested, _nReported);

			if (Change.m_bNotifyRegistered)
				fp_DispatchReadiness(pRegistration, NSys::EIoLoopEvent::mc_None, 0, _nReported);
		}
	}
}

void CIoLoop_Iocp::fp_ProcessPendingOps(umint &_nReported)
{
	// Apply registration changes first. Owner ordering and the removal sweep keep pending pointers valid until cancellation is reported.
	NContainer::TCVector<CIocpPendingOp *> IoOps;
	{
		DMibLock(mp_IoOpLock);
		IoOps = fg_Move(mp_PendingIoOps);
	}

	for (CIocpPendingOp *pOp : IoOps)
		fp_ApplyPendingOp(pOp, _nReported);
}

void CIoLoop_Iocp::fp_ApplyPendingOp(CIocpPendingOp *_pOp, umint &_nReported)
{
	auto *pRegistration = _pOp->m_pRegistration;

	if (pRegistration->m_bDeregistering || !pRegistration->m_pAfdGroup)
	{
		// Removal before issue owes cancellation on the loop thread; no kernel buffer references exist.
#if DMibConfig_IoDebug_Enable
		if (mp_pIo->f_TraceEnabled())
			mp_pIo->f_Trace("pending-op-cancelled", pRegistration->m_pToken, pRegistration->m_Handle, _pOp->m_bStreamStart ? 1 : _pOp->m_bStreamResume ? 2 : 3);
#endif

		if (_pOp->m_bStreamStart)
		{
			NSys::CIoStreamSegment Segment;
			Segment.m_Status = NSys::EIoCompletionStatus::mc_Cancelled;
			_pOp->m_fSink(fg_Move(Segment));
		}
		else if (_pOp->m_pSendOp)
		{
			_pOp->m_pSendOp->m_fOnComplete(NSys::CIoCompletion{.m_Status = NSys::EIoCompletionStatus::mc_Cancelled});
			if (_pOp->m_pSendOp->m_fOnBufferReleased)
				_pOp->m_pSendOp->m_fOnBufferReleased();
			fg_DeleteObject(CDefaultAllocator(), _pOp->m_pSendOp);
		}

		fg_DeleteObject(CDefaultAllocator(), _pOp);
		return;
	}

	if (_pOp->m_bStreamResume)
	{
		fp_ResumeStream(pRegistration);
		fg_DeleteObject(CDefaultAllocator(), _pOp);
		return;
	}

	if (_pOp->m_bSendWindow)
	{
		pRegistration->m_nSendWindowBytes = _pOp->m_nBytes;
		fp_IssueDeferredSends(pRegistration, _nReported);
		fg_DeleteObject(CDefaultAllocator(), _pOp);
		return;
	}

	// Disable readiness only for the direction entering completion I/O; keep the other direction and close interest.
	bool &bDirectionMode = _pOp->m_bStreamStart ? pRegistration->m_bCompletionModeRead : pRegistration->m_bCompletionModeWrite;
	if (!bDirectionMode)
	{
		bDirectionMode = true;
		fp_UpdatePoll(pRegistration, _nReported);

#if DMibConfig_IoDebug_Enable
		if (mp_pIo->f_TraceEnabled())
			mp_pIo->f_Trace(_pOp->m_bStreamStart ? "completion-mode-read" : "completion-mode-write", pRegistration->m_pToken, pRegistration->m_Handle, 0);
#endif
	}

	if (_pOp->m_bStreamStart)
	{
		DMibFastCheck(!pRegistration->m_bStreamStarted);
		pRegistration->m_fStreamSink = fg_Move(_pOp->m_fSink);
		pRegistration->m_nStreamBufferBytes = _pOp->m_nBytes;
		pRegistration->m_bStreamStarted = true;

		if (fp_StartStream(pRegistration, fg_Move(_pOp->m_pBackpressure)))
			fp_ArmStream(pRegistration);
		else
			fp_EndStream(pRegistration, NSys::EIoCompletionStatus::mc_Error, WSAENOBUFS, _nReported);

		fg_DeleteObject(CDefaultAllocator(), _pOp);
		return;
	}

	fp_AppendSend(pRegistration, _pOp->m_pSendOp, _nReported);
	fg_DeleteObject(CDefaultAllocator(), _pOp);
}

umint CIoLoop_Iocp::fp_Iterate(bool _bBlock)
{
	return fp_IterateTimeout(_bBlock, _bBlock ? INFINITE : 0);
}

umint CIoLoop_Iocp::fp_IterateTimeout(bool _bBlock, DWORD _TimeoutMs)
{
	umint nReported = 0;

	fp_ProcessChanges(nReported);
	fp_ProcessPendingOps(nReported);

	// Drain inline completions and flush staged receive data before parking; neither guarantees a future packet to wake the loop.
	fp_DrainInlineCompletions(nReported);
	fp_FlushStreamSegments(nReported);

	// Return after dispatch or with inline work pending; local enqueues need not signal another wake.
	bool bBlock = _bBlock && nReported == 0 && mp_InlineCompletions.f_IsEmpty();
	if (bBlock)
	{
		// Commit to parking, unless a wake is already owed, in which case this pass polls and
		// the caller re-checks its work when the iterate returns
		EWakeState Previous = EWakeState(mp_WakeState.f_FetchOr(uint32(EWakeState::mc_Parked), NAtomic::gc_MemoryOrder_SequentiallyConsistent));
		if (fg_IsSet(Previous, EWakeState::mc_Pending))
		{
			mp_WakeState.f_Store(0, NAtomic::gc_MemoryOrder_Release);
			bBlock = false;
		}
	}

	OVERLAPPED_ENTRY Entries[gc_IocpDequeueBatch];
	ULONG nEntries = 0;
	if (!GetQueuedCompletionStatusEx(mp_hPort, Entries, gc_IocpDequeueBatch, &nEntries, bBlock ? _TimeoutMs : 0, FALSE))
		nEntries = 0;

#if DMibConfig_IoDebug_Enable
	if (mp_pIo->f_StatsEnabled())
	{
		mp_pIo->m_IocpStats.m_nWaits.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		mp_pIo->m_IocpStats.m_nPackets.f_FetchAdd(nEntries, NAtomic::gc_MemoryOrder_Relaxed);
	}
#endif

	if (bBlock)
		mp_WakeState.f_Store(0, NAtomic::gc_MemoryOrder_Release);

	// Outstanding operations pin every batch entry across callback reentry.
	for (ULONG iEntry = 0; iEntry < nEntries; ++iEntry)
		fp_DispatchEntry(Entries[iEntry], nReported);

	// Re-posts and deferred issues during the dispatch can complete at issue too
	fp_DrainInlineCompletions(nReported);
	fp_FlushStreamSegments(nReported);

	return nReported;
}

void CIoLoop_Iocp::f_DrainForShutdown()
{
	fp_Iterate(false);

	// Keep iterating until cancellation packets settle every deregistration.
	while (mp_nDeregistering)
		fp_Iterate(true);
}

bool CIoLoop_Iocp::f_SupportsCompletionIo() const
{
	return mp_bCreated && mp_pIo->f_CompletionEnabled();
}

umint CIoLoop_Iocp::f_GetCompletionSendDepth() const
{
	return f_SupportsCompletionIo() ? mp_pIo->f_SendDepth() : 1;
}

bool CIoLoop_Iocp::f_SupportsReceiveStream() const
{
	return mp_bCreated && mp_pIo->f_CompletionEnabled();
}
