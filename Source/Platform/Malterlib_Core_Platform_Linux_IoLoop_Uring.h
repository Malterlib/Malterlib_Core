// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Core_Platform_Linux_IoLoop.h"
#include "Malterlib_Core_Platform_Linux_IoUring.h"
#include <Mib/Core/IoStream>

struct CIoSubSystem_Linux;
struct CUringIoOp;
struct CUringRecvRing;
struct CUringSendRing;

// The uring backend's registration record: the public handle plus the loop-thread-owned arming
// and teardown state, reachable directly from completion user data with no lookup. The direct
// pointer is safe because of obligation counting: the record is freed only when its outstanding
// count reaches zero, the count decrements only when a CQE is reaped, and every SQE produces
// exactly one CQE — so no CQE anywhere can name a freed record
struct CUringRegistration : public NMib::NSys::CIoLoopRegistration
{
	// The adaptive window the consumer asks about; owned by the asker, never read by the loop
	NMib::NSys::CIoSendWindow m_SendWindow;

	// Close events ride their own standing poll so idle and completion-mode windows still learn
	// of peer death. Armed for EPOLLRDHUP first; a half-closed peer keeps RDHUP level-set, so
	// after it fires the rearm asks for errors-and-hangup only, and after those fire nothing is
	// rearmed — each state fires at most once, so the level semantics cannot spin
	enum class EClosePoll : uint8
	{
		mc_NotArmed
		, mc_ArmedRdHup
		, mc_ArmedHupOnly
		, mc_Terminal
	};

	// CQEs owed to this registration: single-shot polls, transfer operations, cancels, and the
	// pin a running callback holds so the record survives anything the callback triggers
	umint m_nOutstanding = 0;

	// Exactly one send is ever with the kernel per descriptor: later submits wait in the pending
	// queue until this one completes, which is what keeps the stream in order with no link
	// protocol — the result completion means the data is queued in the TCP stack and still
	// transmitting, so the next send submitted then keeps the wire busy. The operation is named
	// by its own address rather than this registration's because a zero copy send answers a
	// second time after the registration can be gone
	CUringIoOp *m_pSendOp = nullptr;
#if DMibConfig_IoDebug_Enable
	// When the last send's result CQE landed, so the gap until the next send's placement is
	// measurable; 0 when no send has completed since the last placement. Io-stats only
	uint64 m_SendIdleStamp = 0;
#endif

	// Local-peer sends as published ranges over a provided-buffer ring: every transfer's spans
	// are published the moment they are placed and one bundle operation drains, in ring order,
	// everything published at its issue — the kernel finishes each entry internally, so a bundle
	// never reports a short send. Remote peers keep the single-operation path for zero copy;
	// which side a registration is on is settled by the peer probe at its first send
	CUringSendRing *m_pSendRing = nullptr;

	// Inbound payload as one standing multishot receive over a provided-buffer ring: the kernel
	// picks the next buffer itself and keeps receiving as long as the ring has any, so there is
	// no completion-to-resubmit bubble to pipeline over. Buffers are allocated fresh as the
	// kernel consumes them — the memory manager is the reuse layer — and each delivered buffer
	// frees itself, and releases its backpressure charge, when the last reference anywhere drops
	CUringRecvRing *m_pRecvRing = nullptr;
	NMib::NFunction::TCFunctionMovable<void (NMib::NSys::CIoStreamSegment &&_Segment)> m_fStreamSink;
	umint m_nStreamBufferBytes = 0;

	// One merged, undelivered segment: an incremental ring lands consecutive receives back to
	// back in one buffer, so completions reaped in the same pass merge into one sink call —
	// the sink is an actor hop, and one hop per skb-sized completion is most of what the
	// receive path costs. Flushed when a completion cannot extend it, before any terminal,
	// and at the end of every reap pass
	NMib::NSys::CIoStreamSegment m_PendingStreamSegment;

	// The acknowledgement obligations moved out of the removal's change entry when the
	// deregistration was processed, held until the outstanding count reaches zero
	CIoLoopDeregWait *m_pDeregWait = nullptr;
	NMib::NFunction::TCFunctionMovable<void ()> m_fOnDeregistered;

	EClosePoll m_ClosePoll = EClosePoll::mc_NotArmed;
	bool m_bReadPollArmed = false;
	bool m_bWritePollArmed = false;

	// Completion transfers active, per direction: that direction's readiness poll is cancelled
	// and further requests for it ignored, and its payload is delivered by the completions of
	// submitted operations. Kept apart so a caller can drive one direction with submitted
	// operations while the other stays on readiness — cancelling both polls on the first
	// operation of either direction would leave the readiness half deaf
	bool m_bCompletionModeRead = false;
	bool m_bCompletionModeWrite = false;
	bool m_bDeregistering = false;
	bool m_bStreamSegmentPending = false;

	// The standing receive counts as one outstanding obligation while armed; it ends the arm on
	// any terminal completion and is re-armed by the loop — after a data-carrying termination, or
	// once buffers come back when the ring ran dry. Ended means the terminal segment went to the
	// sink and nothing arms again
	bool m_bStreamArmed = false;
	bool m_bStreamNeedsRearm = false;
	bool m_bStreamEnded = false;

	// Whether this descriptor's sends may hand the kernel their pages instead of copying them.
	// Decided once, from the peer, the first time a send becomes an operation: a local peer holds
	// the pages in its own receive queue, so zero copy there pins the most memory of anything we
	// run and saves nothing
	bool m_bZeroCopyProbed = false;
	bool m_bZeroCopyEligible = false;

	// A publish that found no ring room defers its operation; everything behind it for this
	// registration must defer too or the stream would reorder. Cleared when coverage frees
	// entries; the deferred operations retry on the next pass
	bool m_bSendPublishStalled = false;
};

// The io_uring backend: request-based readiness through single-shot ring polls, completion
// transfers as ring operations, and the hosting queue's park folded into the ring's blocking wait
// through a futex wait on the queue event's count word.
//
// Lifetimes rest on two invariants. Every SQE produces exactly one CQE, counted against its
// registration from prepare to reap, so a deregistering registration whose count reaches zero is
// provably unnamed anywhere and is freed with no generation or drain-before-free protocol. And a
// registered descriptor stays open and owned by its caller until the deregistration
// acknowledgement runs, so it is targetable by number throughout
struct CIoLoop_IoUring : public CIoLoop_POSIXBase
{
	CIoLoop_IoUring();
	~CIoLoop_IoUring() override;

	// False when ring creation failed even though the probe passed; the factory then falls back
	// to the epoll backend
	bool f_IsRingCreated() const;

	void f_RequestReadiness(NMib::NSys::CIoLoopRegistration *_pRegistration, NMib::NSys::EIoLoopEvent _EventMask) override;

	void f_SetSendWindow(NMib::NSys::CIoLoopRegistration *_pRegistration, umint _nBytes) override;
	bool f_IsSendWindowFull(NMib::NSys::CIoLoopRegistration *_pRegistration, umint _nUnreleasedBytes, umint _nStartBytes) override;

	void f_SetParkEvent(NMib::NThread::CEventAutoReset *_pEvent) override;
	bool f_ParksOnQueueEvent() const override;
	void f_DrainForShutdown() override;
	void f_AbandonPendingTeardown() override;
	bool f_SupportsCompletionIo() const override;
	umint f_GetCompletionSendDepth() const override;
	bool f_SendReleaseIsPrompt(NMib::NSys::CIoLoopRegistration const *_pRegistration) const override;
	bool f_SubmitSendVectored(NMib::NSys::CIoLoopRegistration *_pRegistration, NMib::NSys::CIoSpan const *_pSpans, umint _nSpans, NMib::NSys::FIoCompletion &&_fOnComplete, NMib::NSys::FIoBufferReleased &&_fOnBufferReleased) override;
	bool f_SupportsReceiveStream() const override;
	bool f_StartReceiveStream(NMib::NSys::CIoLoopRegistration *_pRegistration, umint _nBufferBytes, NMib::NStorage::TCSharedPointer<NMib::NSys::CIoStreamBackpressure> _pBackpressure, NMib::NSys::FIoStreamSink &&_fSink) override;
	void f_ResumeReceiveStream(NMib::NSys::CIoLoopRegistration *_pRegistration) override;

private:
	umint fp_Iterate(bool _bBlock) override;
	auto fp_CreateRegistration() -> NMib::NSys::CIoLoopRegistration * override;

	CIoUringSqe &fp_PrepareSqe();
	void fp_ArmPoll(CUringRegistration *_pRegistration, uint64 _Tag, uint32 _PollMask);
	void fp_ArmRequested(CUringRegistration *_pRegistration, NMib::NSys::EIoLoopEvent _EventMask);
	void fp_PrepareCancel(CUringRegistration *_pRegistration, uint64 _TargetUserData);
	bool fp_IsZeroCopyEligible(CUringRegistration *_pRegistration, umint _nBytes);
	void fp_ReleaseNotifyPending(CUringIoOp *_pOp);
	void fp_CancelOutstanding(CUringRegistration *_pRegistration, umint &_nReported);
	bool fp_StartStream(CUringRegistration *_pRegistration, NMib::NStorage::TCSharedPointer<NMib::NSys::CIoStreamBackpressure> &&_pBackpressure, umint &_nReported);
	void fp_ArmStream(CUringRegistration *_pRegistration);
	bool fp_RefillBid(CUringRegistration *_pRegistration, uint16 _Bid);
	void fp_ResumeStream(CUringRegistration *_pRegistration);
	void fp_DeliverStreamSegment(CUringRegistration *_pRegistration, NMib::NSys::CIoStreamSegment &&_Segment, umint &_nReported);
	void fp_FlushStreamSegment(CUringRegistration *_pRegistration, umint &_nReported);
	void fp_FlushStreamSegments(umint &_nReported);
	void fp_EndStream(CUringRegistration *_pRegistration, NMib::NSys::EIoCompletionStatus _Status, int32 _Error, umint &_nReported);
	void fp_ReleaseStream(CUringRegistration *_pRegistration);
	bool fp_PublishSend(CUringRegistration *_pRegistration, CUringIoOp *_pOp, umint &_nReported);
	void fp_ArmSendBundle(CUringRegistration *_pRegistration);
	void fp_CoverSendRecords(CUringRegistration *_pRegistration, umint _nBytes, umint &_nReported);
	void fp_FailSendRecords(CUringRegistration *_pRegistration, NMib::NSys::EIoCompletionStatus _Status, int32 _Error, umint &_nReported);
	void fp_ReleaseSendRing(CUringRegistration *_pRegistration);
	void fp_PlaceBatch(umint &_nReported);
	void fp_ReapAll(umint &_nReported);
	void fp_DispatchCqe(uint64 _UserData, int32 _Res, uint32 _Flags, umint &_nReported);
	void fp_TryAcknowledge(CUringRegistration *_pRegistration, umint &_nReported);
	void fp_SweepPendingOps(CUringRegistration *_pRegistration);

	// The ring, owned and driven only by the loop's thread
	CIoUringRing mp_Ring;

	// Registrations mid-deregistration; f_DrainForShutdown iterates until none remain
	umint mp_nDeregistering = 0;

	// The hosting queue's wake event, folded into the ring's blocking wait through a futex wait
	// on its count word so job signals reach a parked loop with no wake of their own. The armed
	// flag tracks the one outstanding futex wait; while it is armed the loop counts as a waiter
	// on the event, which is what makes signals issue the futex wake at all
	NMib::NThread::CEventAutoReset *mp_pParkEvent = nullptr;

	// Completion transfers queued from any thread; they enter the ring only on the loop's thread,
	// at the start of its next pass. The owner's own submissions need no wake because that pass
	// runs before it can park again
	NMib::NThread::CMutual mp_IoOpLock;
	NMib::NContainer::TCVector<CUringIoOp *> mp_PendingIoOps;

	// SQEs prepared during a pass, handed to the kernel at the flush points. Loop state rather
	// than pass locals: a synchronous deregistration re-enters the iterate from a callback, and
	// the inner pass flushes the outer frame's prepared entries — each is complete when appended
	// — or its own entries could never enter a full submission queue. The index tracks how far
	// flushing got, so a re-entered flush resumes instead of resubmitting
	NMib::NContainer::TCVector<CIoUringSqe> mp_PendingSqes;
	umint mp_iNextSqe = 0;

	// Zero copy sends whose result has been reported and whose notification is still owed. They
	// are no longer any registration's obligation — see CUringIoOp — so this is what still knows
	// about them when the ring is destroyed with notifications outstanding
	NMib::NContainer::TCVector<CUringIoOp *> mp_NotifyPending;
	umint mp_nNotifyPendingBytes = 0;

	// Registrations holding a merged, undelivered receive segment, in first-staged order;
	// flushed and cleared at the end of every reap pass. May hold a registration more than
	// once — a flush inside the pass restages — which the pending flag makes harmless
	NMib::NContainer::TCVector<CUringRegistration *> mp_StreamFlushQueue;

	// The io subsystem, cached since each access through the getter is an atomic operation
	CIoSubSystem_Linux *mp_pIo = nullptr;

	// Buffer group ids are a per-ring u16 namespace; freed ids are reused before the counter
	// grows so a long-lived loop cannot wrap into an id still registered
	NMib::NContainer::TCVector<uint16> mp_FreeBgids;
	uint16 mp_NextBgid = 0;

	bool mp_bRingCreated = false;
	bool mp_bFutexArmed = false;
	bool mp_bFlushingStreamSegments = false;
};
