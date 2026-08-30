// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "../Malterlib_Core_IoLoop_Internal.h"
#include "Malterlib_Core_Platform_Windows_Afd.h"
#include <Mib/Core/IoStream>

struct CIocpRegistration;
struct CIocpAfdGroup;
struct CIocpPendingOp;

// Io debugging knobs the socket layer consults, answered once for the process like the loop's
// own; constants without the io debugging overrides
#if DMibConfig_IoDebug_Enable
bool fg_IocpLoopbackFastPathEnabled();
umint fg_IocpSocketBufferBytesOverride();
umint fg_IocpSocketSendBufferBytesOverride();
bool fg_IocpDirectSendEnabled();
#else
constexpr bool fg_IocpLoopbackFastPathEnabled()
{
	return true;
}

constexpr umint fg_IocpSocketBufferBytesOverride()
{
	return 0;
}

constexpr umint fg_IocpSocketSendBufferBytesOverride()
{
	return umint(-1);
}

constexpr bool fg_IocpDirectSendEnabled()
{
	return true;
}
#endif

// The stream's block reuse, shared with every buffer born from it. A dying buffer returns its
// block here instead of to the allocator, and the next post takes newest first, so the block the
// kernel copies into next is the one most recently touched. Consumers die on any thread while
// the loop posts on its own, hence the lock; it outlives the stream through the buffers' own
// references, and the stream's teardown marks it dead so late returns fall through to the
// allocator
struct CIocpBufferRecycler final
{
	~CIocpBufferRecycler();

	bool f_TryReturn(uint8 *_pBlock);
	uint8 *f_TryTake();
	void f_Die();

	NMib::NThread::CMutual m_Lock;
	NMib::NContainer::TCVector<uint8 *> m_FreeBlocks;
	umint m_nBufferBytes = 0;
	umint m_nMaxFree = 0;
	bool m_bDead = false;
};

// One receive buffer, as the thing that keeps itself alive: the kernel fills it, the segment
// carries a shared reference to it up the stack, and whoever drops the last reference frees it
// right there and releases its backpressure charge. There is no return protocol to forget: the
// destructor is the release event, and it runs exactly once on whichever thread that happens
struct CIocpStreamBuffer final : NMib::CVirtualDestroyBase
{
	~CIocpStreamBuffer() override;

	uint8 *m_pData = nullptr;
	umint m_nDataBytes = 0;
	NMib::NStorage::TCSharedPointer<CIocpBufferRecycler> m_pRecycler;
	NMib::NStorage::TCSharedPointer<NMib::NSys::CIoStreamBackpressure> m_pBackpressure;

	// The capacity this buffer was charged against the stream's accounting — the allocation, not
	// the payload, because memory is what the limit bounds
	umint m_nCharged = 0;
};

// The receives a stream keeps with the kernel at once. With the unix sockets' direct sends (see
// fg_SubmitSendVectored in the socket layer) a receive already pending when the send arrives is
// filled straight from the sender's pages, so what is posted ahead decides how much of the
// stream takes the single copy: four slices of a quarter megabyte measured 31 GB/s on one
// connection and 71 GB/s on three against 22 and 48 for two of 64 KiB, while larger sets fall
// off again once the posted memory outgrows the cache (eight of a megabyte: 17 and 30). The cap
// exists for the override to explore around that
constexpr umint gc_IocpMaxRecvDepth = 16;
constexpr umint gc_IocpDefaultRecvDepth = 4;

// The sends a registration keeps with the kernel at once. An overlapped send copies into the
// socket's buffer and completes at once, so with one in flight every send would pay a full loop
// round trip before the next could be issued; several in flight let a whole burst be with the
// kernel together. Capped at what the consumers can have outstanding
constexpr umint gc_IocpDefaultSendDepth = 4;
constexpr umint gc_IocpMaxSendDepth = 8;

enum class EIocpOpKind : uint8
{
	mc_Poll
	, mc_Send
	, mc_Recv
};

// One kernel operation: the OVERLAPPED the kernel names it by, first, so the pointer a dequeued
// completion packet carries is the operation itself with no lookup, and its IO_STATUS_BLOCK —
// which overlays the OVERLAPPED's first two words — is where the native calls report. Every
// issued operation produces exactly one completion, whether from the port or inline when the
// handle skips the port on synchronous success, and is counted against its registration from
// issue to report
struct alignas(16) CIocpOp
{
	OVERLAPPED m_Overlapped;
	CIocpRegistration *m_pRegistration = nullptr;

	// The NTSTATUS the completion carried; gc_NtStatus_Cancelled says cancelled
	NTSTATUS m_Status = 0;
	DWORD m_nBytes = 0;

	// A WSA error already known at issue, for an operation that failed synchronously and never
	// produced a status of its own
	int m_Error = 0;

	EIocpOpKind m_Kind = EIocpOpKind::mc_Send;

	// With the kernel, or completed inline and waiting in the loop's inline list
	bool m_bIssued = false;

	// The result has landed and is waiting to be reported in order
	bool m_bCompleted = false;
};

static_assert(offsetof(CIocpOp, m_Overlapped) == 0);

// One AFD poll: single shot, level at arm, exactly like a ring poll. The events it was armed for
// are kept apart from the poll block, which the completion overwrites with what it reports —
// and AFD reports whatever holds on the socket when a state change completes the poll, asked
// for or not, so the result has to be masked against the interest
struct CIocpPollOp : public CIocpOp
{
	CAfdPollInfo m_PollInfo;
	CIocpAfdGroup *m_pGroup = nullptr;
	ULONG m_ArmedEvents = 0;
};

// One completion send, in the registration's issue-order FIFO. The buffers point straight at the
// caller's spans; the kernel copies them at issue, so the released functor runs right after the
// completion
struct CIocpSendOp : public CIocpOp
{
	NMib::NSys::FIoCompletion m_fOnComplete;
	NMib::NSys::FIoBufferReleased m_fOnBufferReleased;
	WSABUF m_Buffers[NMib::NSys::gc_IoLoopMaxSubmitSpans];
	DWORD m_nBuffers = 0;
	umint m_nRequested = 0;
	CIocpSendOp *m_pNext = nullptr;

	// The completion was reported when the kernel accepted the send (see
	// CIocpRegistration::m_bSendCompletesOnAck); the packet only runs the release
	bool m_bCompletionReported = false;
#if DMibConfig_IoDebug_Enable
	uint64 m_EnqueueStamp = 0;
#endif
};

// One standing receive of a stream, posted into the untouched tail of a loop-owned buffer
struct CIocpRecvOp : public CIocpOp
{
	NMib::NStorage::TCSharedPointer<CIocpStreamBuffer> m_pBuffer;
	umint m_Offset = 0;
	WSABUF m_Buffer;
	DWORD m_Flags = 0;
	CIocpRecvOp *m_pNext = nullptr;
};

// The IOCP backend's registration record: the public handle plus the loop-thread-owned arming
// and teardown state, reachable directly from every completion with no lookup. The direct
// pointer is safe because of obligation counting: the record is freed only when its outstanding
// count reaches zero, the count decrements only when an operation is reported, and every issued
// operation produces exactly one completion — so no completion anywhere can name a freed record
struct CIocpRegistration : public NMib::NSys::CIoLoopRegistration
{
	CIocpRegistration();

	// The close-class interest the poll always carries, so idle and completion-mode windows
	// still learn of peer death. The peer's half close first; a half-closed peer keeps that bit
	// level set, so after it fires only aborts are asked for, and after those nothing — each
	// state fires at most once, so the level semantics cannot spin
	enum class EClosePoll : uint8
	{
		mc_Disconnect
		, mc_AbortOnly
		, mc_Terminal
	};

	// Completions owed to this registration: polls, transfers, and the pin a running callback
	// holds so the record survives anything the callback triggers
	umint m_nOutstanding = 0;

	// What AFD polls name: the provider's base handle, which differs from the socket only under
	// a layered service provider
	HANDLE m_hBase = nullptr;
	CIocpAfdGroup *m_pAfdGroup = nullptr;

	// Exactly one poll with the kernel per socket, carrying the union of what is owed: AFD
	// services one poll per socket at a time — with several outstanding, an event completes
	// whichever it likes and the one that asked for it stays pending — so the interest is
	// combined into one request and the request is cancelled and re-armed when it changes
	CIocpPollOp m_PollOp;
	bool m_bPollArmed = false;
	bool m_bPollCancelRequested = false;

	// Readiness reports owed, one-shot: cleared when the poll reports that direction
	bool m_bReadWanted = false;
	bool m_bWriteWanted = false;
	EClosePoll m_ClosePollState = EClosePoll::mc_Disconnect;

	// Whether the peer's half close has been reported, by the poll or by the stream running dry.
	// Level state rather than an edge: asked for again it is answered again, since neither the
	// poll nor the stream has anything further to say about it
	bool m_bDisconnectReported = false;

	// Sends in submission order: issued ones first, then those waiting for a slot. Results are
	// reported from the head only while the head is complete, so a later completion overtaking
	// an earlier one at the port waits in place — submit order is report order by construction,
	// which is the invariant the consumers' byte accounting rests on
	CIocpSendOp *m_pSendHead = nullptr;
	CIocpSendOp *m_pSendTail = nullptr;
	CIocpSendOp *m_pSendNextToIssue = nullptr;
	umint m_nSendsInFlight = 0;
#if DMibConfig_IoDebug_Enable
	uint64 m_SendIdleStamp = 0;
#endif

	// The receive stream: its standing receives in issue order, reported from the head like the
	// sends, each into its own buffer's untouched tail
	CIocpRecvOp m_RecvOps[gc_IocpMaxRecvDepth];
	CIocpRecvOp *m_pRecvHead = nullptr;
	CIocpRecvOp *m_pRecvTail = nullptr;
	umint m_nRecvDepth = 0;
	umint m_nRecvsInFlight = 0;
	NMib::NSys::FIoStreamSink m_fStreamSink;
	NMib::NStorage::TCSharedPointer<NMib::NSys::CIoStreamBackpressure> m_pBackpressure;
	NMib::NStorage::TCSharedPointer<CIocpBufferRecycler> m_pRecycler;
	umint m_nStreamBufferBytes = 0;
	umint m_nRecvBufferBytes = 0;

	// One merged, undelivered segment: consecutive receives into one buffer land back to back,
	// so completions reaped in the same pass merge into one sink call — the sink is an actor
	// hop, and the hop count is most of what the receive path costs
	NMib::NSys::CIoStreamSegment m_PendingStreamSegment;

	// The acknowledgement obligations moved out of the removal's change entry when the
	// deregistration was processed, held until the outstanding count reaches zero
	CIoLoopDeregWait *m_pDeregWait = nullptr;
	NMib::NFunction::TCFunctionMovable<void ()> m_fOnDeregistered;

	// Completion transfers active, per direction: that direction's readiness poll is cancelled
	// and further requests for it ignored
	bool m_bCompletionModeRead = false;
	bool m_bCompletionModeWrite = false;
	bool m_bDeregistering = false;
	bool m_bStreamSegmentPending = false;
	bool m_bStreamStarted = false;
	bool m_bStreamNeedsRearm = false;
	bool m_bStreamEnded = false;

	// The handle skips the port when a transfer completes synchronously, so such completions
	// are reported from the loop's inline list instead of a packet
	bool m_bSkipSuccess = false;

	// The socket sends without a send buffer: the kernel transmits from the caller's pages and
	// finishes the send only once the peer has acknowledged it. That is the release, not the
	// completion — the completion is the ordering point the consumers consume bytes on, and
	// waiting for the acknowledgement there serialized every message on a round trip, with
	// the tail segment of each waiting out the peer's delayed acknowledgement. So the
	// completion is reported as soon as the kernel has accepted the send, like the queued
	// result of a zero copy send on Linux, and the packet runs only the release; the promise
	// of a prompt release is withdrawn accordingly. Set by the socket layer at registration,
	// before any submission or any consumer asks
	bool m_bSendCompletesOnAck = false;

	// The socket's completions provably arrive on this loop's port: the handle was bound here at
	// registration, or rebound here. False for a handle that kept an earlier binding the system
	// would not replace — readiness still works, transfers must not be submitted
	bool m_bAssociated = false;

	// Listed in the loop's inline completion queue, holding one obligation on the count while
	// there so the record outlives its entry
	bool m_bInlinePending = false;
};

// The IOCP backend: request-based readiness through single-shot AFD polls, completion transfers
// as overlapped sends and receives, all completing on one port the hosting thread parks in.
//
// Lifetimes rest on two invariants. Every issued operation produces exactly one completion,
// counted against its registration from issue to report, so a deregistering registration whose
// count reaches zero is provably unnamed anywhere and is freed with no generation or drain-before
// -free protocol. And a registered socket stays open and owned by its caller until the
// deregistration acknowledgement runs, so it is targetable by handle throughout
struct CIoLoop_Iocp : public CIoLoop_Base
{
	CIoLoop_Iocp();
	~CIoLoop_Iocp() override;

	// False when the port could not be created; the factory then hands out no loop
	bool f_IsCreated() const;

	void f_RequestReadiness(NMib::NSys::CIoLoopRegistration *_pRegistration, NMib::NSys::EIoLoopEvent _EventMask) override;
	void f_Deregister(NMib::NSys::CIoLoopRegistration *_pRegistration) override;

	void f_DrainForShutdown() override;
	void f_AbandonPendingTeardown() override;
	bool f_SupportsCompletionIo() const override;
	umint f_GetCompletionSendDepth() const override;
	bool f_SubmitSendVectored(NMib::NSys::CIoLoopRegistration *_pRegistration, NMib::NSys::CIoSpan const *_pSpans, umint _nSpans, NMib::NSys::FIoCompletion &&_fOnComplete, NMib::NSys::FIoBufferReleased &&_fOnBufferReleased) override;
	bool f_SupportsReceiveStream() const override;
	bool f_SendReleaseIsPrompt(NMib::NSys::CIoLoopRegistration const *_pRegistration) const override;
	bool f_StartReceiveStream(NMib::NSys::CIoLoopRegistration *_pRegistration, umint _nBufferBytes, NMib::NStorage::TCSharedPointer<NMib::NSys::CIoStreamBackpressure> _pBackpressure, NMib::NSys::FIoStreamSink &&_fSink) override;
	void f_ResumeReceiveStream(NMib::NSys::CIoLoopRegistration *_pRegistration) override;

private:
	umint fp_Iterate(bool _bBlock) override;
	umint fp_IterateTimeout(bool _bBlock, DWORD _TimeoutMs);
	auto fp_CreateRegistration() -> NMib::NSys::CIoLoopRegistration * override;
	void fp_WakeKernel() override;

	void fp_QueuePendingOp(CIocpPendingOp *_pOp);
	void fp_ProcessChanges(umint &_nReported);
	void fp_ProcessPendingOps(umint &_nReported);
	void fp_ApplyPendingOp(CIocpPendingOp *_pOp, umint &_nReported);
	NMib::NContainer::TCVector<CIocpPendingOp *> fp_TakePendingOps(CIocpRegistration *_pRegistration);
	void fp_FlushPendingOpsBeforeRemoval(CIocpRegistration *_pRegistration, umint &_nReported);

	bool fp_Associate(CIocpRegistration *_pRegistration, int &o_Error);
	CIocpAfdGroup *fp_AcquireAfdGroup();
	void fp_ReleaseAfdGroup(CIocpRegistration *_pRegistration);
	bool fp_ArmPoll(CIocpRegistration *_pRegistration, ULONG _AfdEvents, umint &_nReported);
	ULONG fp_WantedPollEvents(CIocpRegistration const *_pRegistration) const;
	void fp_UpdatePoll(CIocpRegistration *_pRegistration, umint &_nReported);
	void fp_ArmRequested(CIocpRegistration *_pRegistration, NMib::NSys::EIoLoopEvent _EventMask, umint &_nReported);
	void fp_CancelPoll(CIocpRegistration *_pRegistration);
	void fp_CancelOutstanding(CIocpRegistration *_pRegistration, umint &_nReported);
	void fp_SweepPendingOps(CIocpRegistration *_pRegistration);
	void fp_TryAcknowledge(CIocpRegistration *_pRegistration, umint &_nReported);
	void fp_DispatchReadiness(CIocpRegistration *_pRegistration, NMib::NSys::EIoLoopEvent _Events, int _Error, umint &_nReported);

	void fp_AppendSend(CIocpRegistration *_pRegistration, CIocpSendOp *_pOp, umint &_nReported);
	void fp_IssueSend(CIocpRegistration *_pRegistration, CIocpSendOp *_pOp, umint &_nReported);
	void fp_IssueDeferredSends(CIocpRegistration *_pRegistration, umint &_nReported);
	void fp_ReportSendAccepted(CIocpRegistration *_pRegistration, CIocpSendOp *_pOp, umint &_nReported);
	void fp_ReportCompletedSends(CIocpRegistration *_pRegistration, umint &_nReported);
	void fp_CancelDeferredSends(CIocpRegistration *_pRegistration);
	void fp_ReleaseSends(CIocpRegistration *_pRegistration);

	bool fp_StartStream(CIocpRegistration *_pRegistration, NMib::NStorage::TCSharedPointer<NMib::NSys::CIoStreamBackpressure> &&_pBackpressure);
	NMib::NStorage::TCSharedPointer<CIocpStreamBuffer> fp_TakeStreamBuffer(CIocpRegistration *_pRegistration);
	bool fp_PostRecv(CIocpRegistration *_pRegistration, CIocpRecvOp &_Op);
	void fp_ArmStream(CIocpRegistration *_pRegistration);
	void fp_ResumeStream(CIocpRegistration *_pRegistration);
	void fp_ReportCompletedRecvs(CIocpRegistration *_pRegistration, umint &_nReported);
	void fp_StageStreamSegment(CIocpRegistration *_pRegistration, NMib::NSys::CIoStreamSegment &&_Segment, umint &_nReported);
	void fp_DeliverStreamSegment(CIocpRegistration *_pRegistration, NMib::NSys::CIoStreamSegment &&_Segment, umint &_nReported);
	void fp_FlushStreamSegment(CIocpRegistration *_pRegistration, umint &_nReported);
	void fp_FlushStreamSegments(umint &_nReported);
	void fp_EndStream(CIocpRegistration *_pRegistration, NMib::NSys::EIoCompletionStatus _Status, int32 _Error, umint &_nReported);
	void fp_ReleaseStream(CIocpRegistration *_pRegistration);

	void fp_DispatchEntry(OVERLAPPED_ENTRY const &_Entry, umint &_nReported);
	void fp_DispatchOp(CIocpOp *_pOp, umint &_nReported);
	void fp_DispatchPoll(CIocpPollOp *_pOp, umint &_nReported);
	void fp_QueueInlineCompletion(CIocpRegistration *_pRegistration);
	void fp_DrainInlineCompletions(umint &_nReported);
	int fp_OpError(CIocpRegistration *_pRegistration, CIocpOp *_pOp);

	HANDLE mp_hPort = nullptr;

	// The \Device\Afd handles polls are issued on, each serving a bounded number of registrations
	NMib::NContainer::TCVector<CIocpAfdGroup *> mp_AfdGroups;

	// Registrations mid-deregistration; f_DrainForShutdown iterates until none remain
	umint mp_nDeregistering = 0;

	// Completion transfers and stream messages queued from any thread; they are applied only on
	// the loop's thread, at the start of its next pass
	NMib::NThread::CMutual mp_IoOpLock;
	NMib::NContainer::TCVector<CIocpPendingOp *> mp_PendingIoOps;

	// Registrations with a transfer that completed without a packet — synchronously under skip
	// mode, or by failing at issue — whose completed operations are reported like any other's.
	// Registrations rather than operations: reporting one operation reports everything complete
	// ahead of it in its FIFO, so an operation's own entry could name one already freed. Each
	// registration is listed at most once and pinned while listed. Loop state walked by index,
	// because a report can add to it
	NMib::NContainer::TCVector<CIocpRegistration *> mp_InlineCompletions;
	umint mp_iNextInline = 0;

	// Registrations holding a merged, undelivered receive segment, in first-staged order;
	// flushed and cleared at the end of every pass
	NMib::NContainer::TCVector<CIocpRegistration *> mp_StreamFlushQueue;

	// Nonzero while a callback runs on this thread: a blocking deregistration from inside one
	// would wait for a count the pin keeps nonzero
	umint mp_nDispatchDepth = 0;

	bool mp_bCreated = false;
	bool mp_bFlushingStreamSegments = false;
	bool mp_bDrainingInline = false;
};
