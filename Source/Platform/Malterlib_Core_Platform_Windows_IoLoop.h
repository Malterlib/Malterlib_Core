// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "../Malterlib_Core_IoLoop_Internal.h"
#include "Malterlib_Core_Platform_Windows_Afd.h"
#include <Mib/Core/IoStream>

struct CIocpRegistration;
struct CIocpAfdGroup;
struct CIocpPendingOp;

// Reuse newest blocks first. A lock protects cross-thread returns and loop posts.
// Buffers retain the recycler after stream teardown; once dead, late returns go to the allocator.
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

// The last shared reference releases storage and backpressure capacity on whichever thread drops it.
struct CIocpStreamBuffer final : NMib::CVirtualDestroyBase
{
	~CIocpStreamBuffer() override;

	uint8 *m_pData = nullptr;
	umint m_nDataBytes = 0;
	NMib::NStorage::TCSharedPointer<CIocpBufferRecycler> m_pRecycler;
	NMib::NStorage::TCSharedPointer<NMib::NSys::CIoStreamBackpressure> m_pBackpressure;
	umint m_nCharged = 0; // Charged allocation capacity, not payload.
};

constexpr umint gc_IocpMaxRecvDepth = 16; // Cap posted receives to bound memory and cache footprint.
constexpr umint gc_IocpDefaultRecvDepth = 4;

constexpr umint gc_IocpDefaultSendDepth = 8; // Pipeline copied sends to avoid per-send loop round trips; acknowledgement-bound sends use a byte window.
constexpr umint gc_IocpMaxSendDepth = 8;

enum class EIocpOpKind : uint8
{
	mc_Poll
	, mc_Send
	, mc_Recv
};

// OVERLAPPED must be first; IO_STATUS_BLOCK overlays its first two words.
// Every issued operation completes once, via the port or inline skip-success handling, and retains its registration until reported.
struct alignas(16) CIocpOp
{
	OVERLAPPED m_Overlapped;
	CIocpRegistration *m_pRegistration = nullptr;

	NTSTATUS m_Status = 0; // Completion NTSTATUS; Cancelled denotes cancellation.
	DWORD m_nBytes = 0;
	int m_Error = 0; // WSA error from synchronous issue failure without a completion status.

	EIocpOpKind m_Kind = EIocpOpKind::mc_Send;
	bool m_bIssued = false; // With the kernel or awaiting inline reporting.
	bool m_bCompleted = false; // Result arrived; awaiting its turn in issue order.
};

static_assert(offsetof(CIocpOp, m_Overlapped) == 0);

// AFD can report unrequested state. Preserve armed interest separately because completion overwrites the poll block.
struct CIocpPollOp : public CIocpOp
{
	CAfdPollInfo m_PollInfo;
	CIocpAfdGroup *m_pGroup = nullptr;
	ULONG m_ArmedEvents = 0;
};

// Issue-order send entry borrowing caller spans until buffer release.
struct CIocpSendOp : public CIocpOp
{
	NMib::NSys::FIoCompletion m_fOnComplete;
	NMib::NSys::FIoBufferReleased m_fOnBufferReleased;
	WSABUF m_Buffers[NMib::NSys::gc_IoLoopMaxSubmitSpans];
	DWORD m_nBuffers = 0;
	umint m_nRequested = 0;
	CIocpSendOp *m_pNext = nullptr;

	uint64 m_ReleaseLagIssueStamp = 0; // Submit-to-release sample for acknowledgement-completing sends.
	bool m_bCompletionReported = false;
#if DMibConfig_IoDebug_Enable
	uint64 m_EnqueueStamp = 0;
	uint64 m_IssueStamp = 0;
	bool m_bPendingAtIssue = false;
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

// Kernel operations and callback pins retain the record; release only after the outstanding count reaches zero.
struct CIocpRegistration : public NMib::NSys::CIoLoopRegistration
{
	CIocpRegistration();

	bool f_SendCompletesOnAck() const;

	// Keep close interest for idle/completion streams. After half-close, poll only aborts; repeating level-set close state would spin.
	enum class EClosePoll : uint8
	{
		mc_Disconnect
		, mc_AbortOnly
		, mc_Terminal
	};

	umint m_nOutstanding = 0; // Kernel operations plus callback pins; zero permits removal acknowledgement.

	HANDLE m_hBase = nullptr; // Provider base handle used by AFD; differs under layered service providers.
	CIocpAfdGroup *m_pAfdGroup = nullptr;
	CIocpPollOp m_PollOp; // One union-interest poll per socket; multiple AFD polls can consume each other's readiness.

	CIocpSendOp *m_pSendHead = nullptr; // Issue-order FIFO; only report complete head entries so port reordering cannot reorder callbacks.
	CIocpSendOp *m_pSendTail = nullptr;
	CIocpSendOp *m_pSendNextToIssue = nullptr;
	umint m_nSendsInFlight = 0;
	umint m_nSendBytesInFlight = 0; // Unreleased bytes for acknowledgement-completing sends.
	umint m_nSendWindowBytes = 0;
	NMib::NSys::CIoSendWindow m_SendWindow; // Owner-managed except cross-thread release-lag epochs, which use their own lock.
#if DMibConfig_IoDebug_Enable
	uint64 m_SendIdleStamp = 0;
#endif

	CIocpRecvOp m_RecvOps[gc_IocpMaxRecvDepth]; // Receive issue order; report from the head into each buffer's untouched tail.
	CIocpRecvOp *m_pRecvHead = nullptr;
	CIocpRecvOp *m_pRecvTail = nullptr;
	umint m_nRecvDepth = 0;
	umint m_nRecvsInFlight = 0;
	NMib::NSys::FIoStreamSink m_fStreamSink;
	NMib::NStorage::TCSharedPointer<NMib::NSys::CIoStreamBackpressure> m_pBackpressure;
	NMib::NStorage::TCSharedPointer<CIocpBufferRecycler> m_pRecycler;
	umint m_nStreamBufferBytes = 0;
	umint m_nRecvBufferBytes = 0;
	NMib::NSys::CIoStreamSegment m_PendingStreamSegment; // Adjacent arrivals merged to reduce sink callbacks.

	CIoLoopDeregWait *m_pDeregWait = nullptr; // Held until outstanding operations and callback pins are released.
	NMib::NFunction::TCFunctionMovable<void ()> m_fOnDeregistered;

	DMibListLinkDS_Link(CIocpRegistration, m_StreamFlushLink);
	DMibListLinkDS_Link(CIocpRegistration, m_InlineLink);

	bool m_bPollArmed = false;
	bool m_bPollCancelRequested = false;
	bool m_bReadWanted = false;
	bool m_bWriteWanted = false;
	EClosePoll m_ClosePollState = EClosePoll::mc_Disconnect;
	bool m_bDisconnectReported = false; // Held level state; a new close request may replay it even with no further kernel event.

	bool m_bCompletionModeRead = false; // Readiness is disabled independently for directions using completion I/O.
	bool m_bCompletionModeWrite = false;
	bool m_bDeregistering = false;
	bool m_bStreamStarted = false;
	bool m_bStreamSegmentPending = false; // True exactly while linked in the stream flush queue.
	bool m_bStreamNeedsRearm = false;
	bool m_bStreamEnded = false;

	bool m_bSkipSuccess = false; // Synchronous success produces inline reporting instead of a port packet.
	bool m_bAssociated = false; // Loop-thread state; false forbids transfer submission but permits readiness polls.
};

// Issued operations retain registration until reported; the caller keeps the socket open until removal acknowledgement.
struct CIoSubSystem_Windows;

struct CIoLoop_Iocp : public CIoLoop_Base
{
	CIoLoop_Iocp();
	~CIoLoop_Iocp() override;

	bool f_IsCreated() const;

	void f_RequestReadiness(NMib::NSys::CIoLoopRegistration *_pRegistration, NMib::NSys::EIoLoopEvent _EventMask) override;
	void f_Deregister(NMib::NSys::CIoLoopRegistration *_pRegistration) override;
	void f_DrainForShutdown() override;
	bool f_SupportsCompletionIo() const override;
	umint f_GetCompletionSendDepth() const override;
	auto f_SubmitSendVectored
		(
			NMib::NSys::CIoLoopRegistration *_pRegistration
			, NMib::NSys::CIoSpan const *_pSpans
			, umint _nSpans
			, NMib::NSys::FIoCompletion &&_fOnComplete
			, NMib::NSys::FIoBufferReleased &&_fOnBufferReleased
		)
		-> umint override
	;
	bool f_SupportsReceiveStream() const override;
	bool f_SendReleaseIsPrompt(NMib::NSys::CIoLoopRegistration const *_pRegistration) const override;
	auto f_StartReceiveStream
		(
			NMib::NSys::CIoLoopRegistration *_pRegistration
			, umint _nBufferBytes
			, NMib::NStorage::TCSharedPointer<NMib::NSys::CIoStreamBackpressure> _pBackpressure
			, NMib::NSys::FIoStreamSink &&_fSink
		)
		-> bool override
	;
	void f_ResumeReceiveStream(NMib::NSys::CIoLoopRegistration *_pRegistration) override;
	void f_SetSendWindow(NMib::NSys::CIoLoopRegistration *_pRegistration, umint _nBytes) override;
	bool f_IsSendWindowFull(NMib::NSys::CIoLoopRegistration *_pRegistration, umint _nUnreleasedBytes, umint _nStartBytes) override;
	bool f_AdoptHandle(NMib::NSys::CIoLoopHandle _Handle, int &o_Error) override;

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
	void fp_ReportSendAtIssue(CIocpRegistration *_pRegistration, CIocpSendOp *_pOp, umint &_nReported);
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
	CIoSubSystem_Windows *mp_pIo = nullptr;
	NMib::NContainer::TCVector<CIocpAfdGroup *> mp_AfdGroups; // Each AFD handle serves a bounded registration group.
	umint mp_nDeregistering = 0; // Shutdown iterates until this reaches zero.

	NMib::NThread::CMutual mp_IoOpLock; // Cross-thread submissions are applied only by the loop.
	NMib::NContainer::TCVector<CIocpPendingOp *> mp_PendingIoOps;

	DMibListLinkDS_List(CIocpRegistration, m_InlineLink) mp_InlineCompletions; // List and pin registrations once; reporting one operation may free other completed FIFO entries.

	DMibListLinkDS_List(CIocpRegistration, m_StreamFlushLink) mp_StreamFlushQueue; // First-staged order; unlink on delivery or drop.

	umint mp_nDispatchDepth = 0; // Blocking deregistration inside a callback would wait on its own pin.

	bool mp_bCreated = false;
	bool mp_bDrainingInline = false;
};
