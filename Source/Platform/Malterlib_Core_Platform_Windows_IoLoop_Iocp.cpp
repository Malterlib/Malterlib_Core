// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Windows_IoLoop_Iocp_Internal.h"

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

CIoLoop_Iocp::CIoLoop_Iocp()
{
	// One concurrent thread: the port is parked in by exactly the thread that hosts the loop
	mp_hPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
	mp_bCreated = mp_hPort != nullptr;
}

CIoLoop_Iocp::~CIoLoop_Iocp()
{
	DMibFastCheck(mp_nDeregistering == 0);

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
	if (fg_IocpStatsEnabled())
		g_IocpStats.m_nWakePosts.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif

	PostQueuedCompletionStatus(mp_hPort, 0, gc_IocpKey_Wake, nullptr);
}

void CIoLoop_Iocp::f_RequestReadiness(NSys::CIoLoopRegistration *_pRegistration, NSys::EIoLoopEvent _EventMask)
{
	if (_EventMask == NSys::EIoLoopEvent::mc_None)
		return;

	// The word is the coalescer: whoever turns it nonzero owns the queue notification, and every
	// later request before the loop consumes it only accumulates bits. Consumption is a single
	// exchange on the loop's thread, so a bit is either taken by that exchange or its requester
	// found the word zero and pushed a fresh notification
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
		// This thread is the one that drives the loop, so waiting here would be waiting for
		// itself. Apply the removal directly instead, which is what the loop would have done.
		// Removal settles through the cancelled operations' packets, which take the kernel a
		// moment, so the self-drive parks briefly between passes instead of spinning on an
		// empty port.
		//
		// From inside a dispatch this could never settle: the pin the dispatch holds keeps the
		// registration's count nonzero. Pool-hosted sockets deregister asynchronously for that
		// reason; a synchronous removal from a callback is a caller bug, so it trips here
		DMibFastCheck(mp_nDispatchDepth == 0);

		while (!DeregWait.m_bDone.f_Load(NAtomic::gc_MemoryOrder_Acquire))
			fp_IterateTimeout(true, 1);
	}
	else
		DeregWait.m_Event.f_Wait();
}

void CIoLoop_Iocp::fp_QueuePendingOp(CIocpPendingOp *_pOp)
{
	// The owner's own submissions skip the wake: pending operations are picked up at the start
	// of every iterate and the owner cannot park again without iterating. The loop may not have
	// an owner yet — it is claimed by the enable message on its queue's thread — which reads as
	// not the owner, the conservative side: the wake's pending bit is durable, so the owner's
	// first park finds the operation instead of sleeping past it
	bool bOwnerThread = fp_IsOwnerThread();
	{
		DMibLock(mp_IoOpLock);
		mp_PendingIoOps.f_Insert(_pOp);
	}

	if (!bOwnerThread)
		fp_SignalWake();
}

// Opens the AFD device handle a group's polls are issued on and associates it with the port. The
// handle skips the port on nothing: a poll satisfied at issue still has to post its packet
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

	auto const &Nt = fg_IocpNtFunctions();

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

// Binds the socket to the port and settles what its polls name. The association is permanent for
// the handle's lifetime, so a handle that was registered before — the websocket upgrade re-
// registers a given-up handle with the loop that owned it — refuses a second binding; that is
// accepted as already bound, and a rebind is attempted through the native replace where the
// system offers it. Completion packets name the operation, never the key, so a stale key from
// an earlier binding is harmless
bool CIoLoop_Iocp::fp_Associate(CIocpRegistration *_pRegistration, int &o_Error)
{
	SOCKET Socket = (SOCKET)_pRegistration->m_Handle;

	HANDLE hBase = nullptr;
	DWORD nBytes = 0;
	if (WSAIoctl(Socket, SIO_BASE_HANDLE, nullptr, 0, &hBase, sizeof(hBase), &nBytes, nullptr, nullptr) != 0 || !hBase)
		hBase = (HANDLE)Socket;
	_pRegistration->m_hBase = hBase;

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

		auto const &Nt = fg_IocpNtFunctions();
		if (Nt.m_fNtSetInformationFile)
		{
			CNtFileCompletionInformation Information;
			Information.m_hPort = mp_hPort;
			Information.m_pKey = _pRegistration;

			IO_STATUS_BLOCK IoStatus;
			NTSTATUS Status = Nt.m_fNtSetInformationFile((HANDLE)Socket, &IoStatus, &Information, sizeof(Information), (FILE_INFORMATION_CLASS)gc_NtFileInformation_ReplaceCompletionInformation);
			if (Status == gc_NtStatus_Success)
				_pRegistration->m_bAssociated = true;
		}
	}

	// Skipping the port on synchronous success spares a transfer that completes at issue its
	// packet round trip. Only sound for a provider whose handle is the socket itself: a layered
	// provider between the two would see a completion its handle never posted
	UCHAR Modes = FILE_SKIP_SET_EVENT_ON_HANDLE;
	if (fg_IocpSkipSuccessEnabled() && hBase == (HANDLE)Socket)
	{
		WSAPROTOCOL_INFOW ProtocolInfo;
		int nProtocolInfo = sizeof(ProtocolInfo);
		if (getsockopt(Socket, SOL_SOCKET, SO_PROTOCOL_INFOW, (char *)&ProtocolInfo, &nProtocolInfo) == 0 && (ProtocolInfo.dwServiceFlags1 & XP1_IFS_HANDLES))
			Modes |= FILE_SKIP_COMPLETION_PORT_ON_SUCCESS;
	}

	if (SetFileCompletionNotificationModes((HANDLE)Socket, Modes))
		_pRegistration->m_bSkipSuccess = (Modes & FILE_SKIP_COMPLETION_PORT_ON_SUCCESS) != 0;

#if DMibConfig_IoDebug_Enable
	if (fg_IocpStatsEnabled())
	{
		g_IocpStats.m_nRegistrations.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		if (_pRegistration->m_bSkipSuccess)
			g_IocpStats.m_nSkipSuccessSockets.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
	}
#endif

	return true;
}

// Dispatches into the consumer under a pin on the outstanding count: anything the callback
// triggers on this thread finds the count nonzero and cannot free the record while it is still
// in hand
void CIoLoop_Iocp::fp_DispatchReadiness(CIocpRegistration *_pRegistration, NSys::EIoLoopEvent _Events, int _Error, umint &_nReported)
{
	++_pRegistration->m_nOutstanding;
	++mp_nDispatchDepth;
	_pRegistration->m_fOnEvents(_pRegistration->m_pToken, _Events, _Error);
	--mp_nDispatchDepth;
	--_pRegistration->m_nOutstanding;
	++_nReported;
}

// Single shot and level at arm: readiness that already exists completes the poll immediately,
// which is what makes request-after-would-block lossless with no separate probe. Spin is
// impossible because read and write bits are only armed on a fresh request, which only follows
// a would-block observation, and the close bits each fire at most once. False means the poll
// never reached the kernel and the failure was reported
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

	auto const &Nt = fg_IocpNtFunctions();
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
		// Nothing is owed: the poll never entered the kernel. Reported as an error event so a
		// failed arm dies loudly instead of leaving the consumer waiting for readiness forever
		fp_DispatchReadiness(_pRegistration, NSys::EIoLoopEvent::mc_Error, (int)Nt.m_fRtlNtStatusToDosError(Status), _nReported);
		return false;
	}

	Op.m_bIssued = true;
	_pRegistration->m_bPollArmed = true;
	_pRegistration->m_bPollCancelRequested = false;
	++_pRegistration->m_nOutstanding;

#if DMibConfig_IoDebug_Enable
	if (fg_IocpStatsEnabled())
		g_IocpStats.m_nPollArms.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
	if (fg_IocpTraceEnabled())
		fg_IocpTrace(Status == gc_NtStatus_Pending ? "poll-arm" : "poll-arm-immediate", _pRegistration->m_pToken, _pRegistration->m_Handle, (uint32)_AfdEvents);
#endif

	return true;
}

// The events the socket's one poll should be asking for right now: the close class its state
// still has to report, plus each direction a readiness report is owed on and completion
// transfers do not cover. Accept and receive together: a listener never sees receive and a
// stream never sees accept, so one mask serves both without knowing which the socket is; a
// completed connect turns send on, which is what writability means on every platform
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

// Brings the poll with the kernel in line with the wanted events: arms one when none is out,
// leaves one asking for exactly this alone, and cancels one asking for something else — the
// cancellation's completion re-arms with whatever is wanted by then. Every cancel outcome yields
// exactly one packet — found and cancelled, already completing, or already queued — so the
// count stays exact and no outcome needs a retry
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

	fp_UpdatePoll(_pRegistration, _nReported);
}

void CIoLoop_Iocp::fp_CancelPoll(CIocpRegistration *_pRegistration)
{
	if (!_pRegistration->m_bPollArmed || _pRegistration->m_bPollCancelRequested)
		return;

	_pRegistration->m_bPollCancelRequested = true;

#if DMibConfig_IoDebug_Enable
	if (fg_IocpStatsEnabled())
		g_IocpStats.m_nPollCancels.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
#endif

	CancelIoEx(_pRegistration->m_PollOp.m_pGroup->m_hAfd, &_pRegistration->m_PollOp.m_Overlapped);
}

void CIoLoop_Iocp::fp_CancelOutstanding(CIocpRegistration *_pRegistration, umint &_nReported)
{
	fp_CancelPoll(_pRegistration);

	// Every transfer the socket has with the kernel, whichever thread issued it. The caller
	// keeps the socket open until the acknowledgement, which is what makes cancelling by
	// handle valid throughout
	if (_pRegistration->m_nSendsInFlight || _pRegistration->m_nRecvsInFlight)
		CancelIoEx((HANDLE)_pRegistration->m_Handle, nullptr);

	// Sends that never reached the kernel have nothing to cancel there; marked cancelled in
	// place, they report in order behind whatever is still in flight
	fp_CancelDeferredSends(_pRegistration);
	fp_ReportCompletedSends(_pRegistration, _nReported);

	// Receives with the kernel terminate the stream through their cancelled completions; a
	// stream waiting for buffers has nothing with the kernel, so its terminal segment is
	// delivered right here — the sink is owed exactly one either way
	if (!_pRegistration->m_nRecvsInFlight && _pRegistration->m_bStreamStarted && !_pRegistration->m_bStreamEnded)
		fp_EndStream(_pRegistration, NSys::EIoCompletionStatus::mc_Cancelled, 0, _nReported);
}

// Removes and returns the queued submissions naming one registration, in queue order
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

// Submissions queued ahead of a removal enter the kernel before the removal is applied, rather
// than being failed in place: a caller that submits a last send and closes in the same breath —
// the websocket reply to a peer's close, before the application tears the connection down — gets
// the same outcome a synchronous readiness send would have given it, the bytes in the kernel
// ahead of the close. What the kernel cannot take at once is cancelled by the removal like any
// other transfer still in flight
void CIoLoop_Iocp::fp_FlushPendingOpsBeforeRemoval(CIocpRegistration *_pRegistration, umint &_nReported)
{
	NContainer::TCVector<CIocpPendingOp *> Ops = fp_TakePendingOps(_pRegistration);
	for (CIocpPendingOp *pOp : Ops)
		fp_ApplyPendingOp(pOp, _nReported);
}

void CIoLoop_Iocp::fp_SweepPendingOps(CIocpRegistration *_pRegistration)
{
	// A submission can land in the pending queue from another thread after a pass has already
	// moved the queue for processing: the submit strictly precedes the removal in the io object
	// owner's order, but an actor migrating between the two jobs can put them on opposite sides
	// of one iterate. Swept here, before the registration is freed, such an operation is
	// cancelled while its registration pointer is still unambiguous
	NContainer::TCVector<CIocpPendingOp *> Swept = fp_TakePendingOps(_pRegistration);

	for (CIocpPendingOp *pOp : Swept)
	{
#if DMibConfig_IoDebug_Enable
		if (fg_IocpTraceEnabled())
			fg_IocpTrace("pending-op-swept", _pRegistration->m_pToken, _pRegistration->m_Handle, pOp->m_bStreamStart ? 1 : pOp->m_bStreamResume ? 2 : 3);
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

	// The count is zero, so no completion anywhere names this registration and freeing it is
	// safe. The acknowledgement counts as dispatched work: its continuation can enqueue locally,
	// and a pass that reports nothing could otherwise park past the enqueue
	fp_SweepPendingOps(_pRegistration);
	fp_ReleaseSends(_pRegistration);
	fp_ReleaseStream(_pRegistration);
	fp_ReleaseAfdGroup(_pRegistration);
	--mp_nDeregistering;

#if DMibConfig_IoDebug_Enable
	if (fg_IocpTraceEnabled())
		fg_IocpTrace("ack", _pRegistration->m_pToken, _pRegistration->m_Handle, 0);
#endif

	CIoLoopDeferredAck Ack{_pRegistration, _pRegistration->m_pDeregWait, fg_Move(_pRegistration->m_fOnDeregistered)};
	fg_RunDeregAcknowledgement(Ack);
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

			// Requests raced ahead of the removal in the owner's order are moot: the consumer
			// stops caring before it requests removal
			pRegistration->m_RequestedEvents.f_Exchange(0, NAtomic::gc_MemoryOrder_AcquireRelease);
			pRegistration->m_bDeregistering = true;
			++mp_nDeregistering;
			pRegistration->m_pDeregWait = Change.m_pDeregWait;
			pRegistration->m_fOnDeregistered = fg_Move(Change.m_fOnDeregistered);
#if DMibConfig_IoDebug_Enable
			if (fg_IocpTraceEnabled())
				fg_IocpTrace("deregister", pRegistration->m_pToken, Change.m_Handle, (uint32)pRegistration->m_nOutstanding);
#endif

			fp_CancelOutstanding(pRegistration, _nReported);
			fp_TryAcknowledge(pRegistration, _nReported);
		}
		else if (Change.m_bReadinessRequest)
		{
			// The notification can outlive its usefulness — the direction flipped to
			// completion transfers, or the removal was queued behind this entry — in which
			// case those bits are consumed and dropped. Each direction is dropped on its own,
			// so a socket driven by submitted operations one way still arms readiness the other
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
			if (fg_IocpTraceEnabled())
				fg_IocpTrace("register", pRegistration->m_pToken, Change.m_Handle, uint32(pRegistration->m_EventMask));
#endif

			int Error = 0;
			pRegistration->m_pAfdGroup = fp_AcquireAfdGroup();
			if (!pRegistration->m_pAfdGroup || !fp_Associate(pRegistration, Error))
			{
				// A registration the kernel refused would be accounted for but never polled — a
				// silent hang. Reporting it as an error event makes it die loudly. The callback
				// is dispatched work, so a pass that reports nothing cannot park past whatever it
				// enqueues; the removal that follows finds nothing outstanding and acknowledges
				fp_ReleaseAfdGroup(pRegistration);
				fp_DispatchReadiness(pRegistration, NSys::EIoLoopEvent::mc_Error, Error ? Error : ERROR_NOT_ENOUGH_MEMORY, _nReported);

				continue;
			}

			// The implicit initial request seeded by f_Register: a level-at-arm poll reports
			// pre-registration readiness by completing immediately, so nothing needs a probe.
			// The close class rides the same poll from the start, so peer death reaches idle
			// and completion-mode windows too
			NSys::EIoLoopEvent Requested = NSys::EIoLoopEvent(pRegistration->m_RequestedEvents.f_Exchange(0, NAtomic::gc_MemoryOrder_AcquireRelease));
			fp_ArmRequested(pRegistration, Requested, _nReported);

			if (Change.m_bNotifyRegistered)
				fp_DispatchReadiness(pRegistration, NSys::EIoLoopEvent::mc_None, 0, _nReported);
		}
	}
}

void CIoLoop_Iocp::fp_ProcessPendingOps(umint &_nReported)
{
	// Queued transfers are applied after the registration changes, so an operation for a
	// registration added in this same pass finds it, and one for a registration whose removal
	// was processed above is completed as cancelled here instead of entering the kernel. The
	// registration pointer is safe to follow: the owner queues operations before it queues the
	// removal, and the acknowledgement that frees a registration sweeps this queue first
	NContainer::TCVector<CIocpPendingOp *> IoOps;
	{
		DMibLock(mp_IoOpLock);
		IoOps = fg_Move(mp_PendingIoOps);
	}

	for (CIocpPendingOp *pOp : IoOps)
		fp_ApplyPendingOp(pOp, _nReported);
}

// One queued submission applied on the loop's thread: cancelled if its registration has left,
// otherwise the direction it drives flipped to completion transfers and the operation issued
void CIoLoop_Iocp::fp_ApplyPendingOp(CIocpPendingOp *_pOp, umint &_nReported)
{
	auto *pRegistration = _pOp->m_pRegistration;

	if (pRegistration->m_bDeregistering || !pRegistration->m_pAfdGroup)
	{
		// The registration left the port before the operation could enter it, so nothing
		// kernel side references the buffers and the cancellation is reported right here,
		// still on the loop's thread as the contract promises. A resume message carries
		// nothing and is simply moot
#if DMibConfig_IoDebug_Enable
		if (fg_IocpTraceEnabled())
			fg_IocpTrace("pending-op-cancelled", pRegistration->m_pToken, pRegistration->m_Handle, _pOp->m_bStreamStart ? 1 : _pOp->m_bStreamResume ? 2 : 3);
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

	// The first operation of a direction flips that direction to completion transfers: its
	// readiness poll is cancelled — every arriving segment would otherwise fire a readiness
	// edge nobody consumes — while the other direction is left alone and the standing close
	// poll keeps reporting peer death
	bool &bDirectionMode = _pOp->m_bStreamStart ? pRegistration->m_bCompletionModeRead : pRegistration->m_bCompletionModeWrite;
	if (!bDirectionMode)
	{
		bDirectionMode = true;
		fp_UpdatePoll(pRegistration, _nReported);

#if DMibConfig_IoDebug_Enable
		if (fg_IocpTraceEnabled())
			fg_IocpTrace(_pOp->m_bStreamStart ? "completion-mode-read" : "completion-mode-write", pRegistration->m_pToken, pRegistration->m_Handle, 0);
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

	fp_AppendSend(pRegistration, _pOp->m_pSendOp);
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

	// Transfers that completed at issue are reported before the park decision, or the pass
	// would sleep on top of a result it already holds
	fp_DrainInlineCompletions(nReported);

	// Dispatched work above may have enqueued for this thread, so a blocking pass must hand
	// control back instead of parking on top of it; inline completions left for the next pass
	// likewise keep it from parking
	bool bBlock = _bBlock && nReported == 0 && mp_iNextInline == mp_InlineCompletions.f_GetLen();
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
	if (fg_IocpStatsEnabled())
	{
		g_IocpStats.m_nWaits.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		g_IocpStats.m_nPackets.f_FetchAdd(nEntries, NAtomic::gc_MemoryOrder_Relaxed);
	}
#endif

	if (bBlock)
		mp_WakeState.f_Store(0, NAtomic::gc_MemoryOrder_Release);

	// The entries stay valid across everything a dispatch triggers: each names an operation
	// its registration still counts as outstanding, so no inner pass can free it
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

	// Deregistration settles through completions — the cancelled operations' packets — so
	// blocking iterates make progress until no registration is still on its way out
	while (mp_nDeregistering)
		fp_Iterate(true);
}

void CIoLoop_Iocp::f_AbandonPendingTeardown()
{
	// Every pool thread is joined and the exiting owner drained the loop to quiescence, so no
	// registration is mid-deregistration; only the userspace obligations matter — continuations
	// of removals still queued, and completion functors of operations that never reached the
	// kernel. The port is closed wholesale
	DMibFastCheck(mp_nDeregistering == 0);

	CIoLoop_Base::f_AbandonPendingTeardown();

	NContainer::TCVector<CIocpPendingOp *> IoOps;
	{
		DMibLock(mp_IoOpLock);
		IoOps = fg_Move(mp_PendingIoOps);
	}

	for (CIocpPendingOp *pOp : IoOps)
	{
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

bool CIoLoop_Iocp::f_SupportsCompletionIo() const
{
	return mp_bCreated && fg_IocpCompletionEnabled();
}

umint CIoLoop_Iocp::f_GetCompletionSendDepth() const
{
	return f_SupportsCompletionIo() ? fg_IocpSendDepth() : 1;
}

bool CIoLoop_Iocp::f_SupportsReceiveStream() const
{
	return mp_bCreated && fg_IocpCompletionEnabled();
}
