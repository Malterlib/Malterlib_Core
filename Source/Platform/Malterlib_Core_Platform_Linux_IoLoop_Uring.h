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

// Loop-thread state addressed by CQE user data. Retain the record until all counted kernel obligations and callback pins are released.
struct CUringRegistration : public NMib::NSys::CIoLoopRegistration
{
	NMib::NSys::CIoSendWindow m_SendWindow; // Owner-managed except release-lag epochs, which use their own lock.

	// A multishot poll reports each socket wakeup that carries a close bit, so a later local shutdown
	// or error is seen after a half-close. A one-shot poll could not be rearmed then: the kernel folds
	// EPOLLRDHUP into every poll mask, and the standing half-close would complete it at once
	enum class EClosePoll : uint8
	{
		mc_NotArmed
		, mc_Armed
		, mc_Terminal
	};

	umint m_nOutstanding = 0; // Kernel obligations plus callback pins; zero permits deregistration acknowledgement.

	CUringIoOp *m_pSendOp = nullptr; // One kernel send per descriptor preserves order; notification lifetime belongs to the operation.
#if DMibConfig_IoDebug_Enable
	uint64 m_SendIdleStamp = 0; // Zero when no result has arrived since the last placement; statistics only.
#endif

	CUringSendRing *m_pSendRing = nullptr; // Local peers publish send ranges in FIFO order; remote peers retain zero-copy operations.

	CUringRecvRing *m_pRecvRing = nullptr; // Standing multishot receive; delivered buffers retain their own lifetime and backpressure charge.
	NMib::NFunction::TCFunctionMovable<void (NMib::NSys::CIoStreamSegment &&_Segment)> m_fStreamSink;
	umint m_nStreamBufferBytes = 0;
	NMib::NSys::CIoStreamSegment m_PendingStreamSegment; // Merge adjacent arrivals; flush before nonadjacent data, terminal delivery, and reap completion.

	CIoLoopDeregWait *m_pDeregWait = nullptr; // Held until the registration's outstanding count reaches zero.
	NMib::NFunction::TCFunctionMovable<void ()> m_fOnDeregistered;

	DMibListLinkDS_Link(CUringRegistration, m_StreamFlushLink);

	EClosePoll m_ClosePoll = EClosePoll::mc_NotArmed;
	bool m_bReadPollArmed = false;
	bool m_bWritePollArmed = false;
	bool m_bCompletionModeRead = false; // Cancel readiness independently per direction when completion I/O takes over.
	bool m_bCompletionModeWrite = false;
	bool m_bDeregistering = false;
	bool m_bStreamSegmentPending = false; // True exactly while linked in the stream flush queue.
	bool m_bStreamArmed = false; // Counts one obligation until terminal CQE; never rearm after terminal sink delivery.
	bool m_bStreamNeedsRearm = false;
	bool m_bStreamEnded = false;

	NMib::NAtomic::TCAtomic<bool> m_bZeroCopyProbed = false; // Release-publishes eligibility from the loop thread; readers acquire before using it.
	NMib::NAtomic::TCAtomic<bool> m_bZeroCopyEligible = false;

	bool m_bSendPublishStalled = false; // Defer all later sends until ring room returns, preserving publish order.
	int32 m_SendError = 0; // Failed sends permanently stop publication; the ring may still contain released buffer addresses.
	umint m_nNotifyPending = 0; // Per-registration occupancy used to exclude self-queued latency samples.
};

// Kernel obligations retain registrations until reaped; descriptors stay open until deregistration acknowledgement.
// Zero-copy release notifications retain their operations separately so descriptor closure cannot deadlock on peer reads.
struct CIoLoop_IoUring : public CIoLoop_POSIXBase
{
	CIoLoop_IoUring();
	~CIoLoop_IoUring() override;

	bool f_IsRingCreated() const;

	void f_RequestReadiness(NMib::NSys::CIoLoopRegistration *_pRegistration, NMib::NSys::EIoLoopEvent _EventMask) override;

	void f_SetSendWindow(NMib::NSys::CIoLoopRegistration *_pRegistration, umint _nBytes) override;
	bool f_IsSendWindowFull(NMib::NSys::CIoLoopRegistration *_pRegistration, umint _nUnreleasedBytes, umint _nStartBytes) override;

	void f_SetParkEvent(NMib::NThread::CEventAutoReset *_pEvent) override;
	bool f_ParksOnQueueEvent() const override;
	void f_DrainForShutdown() override;
	bool f_SupportsCompletionIo() const override;
	umint f_GetCompletionSendDepth() const override;
	bool f_SendReleaseIsPrompt(NMib::NSys::CIoLoopRegistration const *_pRegistration) const override;
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

private:
	umint fp_Iterate(bool _bBlock) override;
	auto fp_CreateRegistration() -> NMib::NSys::CIoLoopRegistration * override;

	CIoUringSqe &fp_PrepareSqe();
	void fp_ArmPoll(CUringRegistration *_pRegistration, uint64 _Tag, uint32 _PollMask, bool _bMultishot = false);
	void fp_ArmRequested(CUringRegistration *_pRegistration, NMib::NSys::EIoLoopEvent _EventMask);
	void fp_PrepareCancel(CUringRegistration *_pRegistration, uint64 _TargetUserData);
	bool fp_IsZeroCopyEligible(CUringRegistration *_pRegistration, umint _nBytes);
	bool fp_AllocateBgid(uint16 &o_Bgid);
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
	int32 fp_GetSendBundleError(CUringRegistration *_pRegistration);
	void fp_ArmSendBundle(CUringRegistration *_pRegistration);
	void fp_CoverSendRecords(CUringRegistration *_pRegistration, umint _nBytes, umint &_nReported);
	void fp_FailSendRecords(CUringRegistration *_pRegistration, NMib::NSys::EIoCompletionStatus _Status, int32 _Error, umint &_nReported);
	void fp_ReleaseSendRing(CUringRegistration *_pRegistration);
	void fp_PlaceBatch(umint &_nReported);
	void fp_ReapAll(umint &_nReported);
	void fp_DispatchCqe(uint64 _UserData, int32 _Res, uint32 _Flags, umint &_nReported);
	void fp_TryAcknowledge(CUringRegistration *_pRegistration, umint &_nReported);
	void fp_SweepPendingOps(CUringRegistration *_pRegistration);

	CIoUringRing mp_Ring;
	umint mp_nDeregistering = 0; // Shutdown must iterate until this reaches zero.

	NMib::NThread::CEventAutoReset *mp_pParkEvent = nullptr; // An armed futex wait must register as an event waiter so job signals issue a wake.

	NMib::NThread::CMutual mp_IoOpLock; // Protects cross-thread submissions; the loop consumes them before its next park.
	NMib::NContainer::TCVector<CUringIoOp *> mp_PendingIoOps;

	NMib::NContainer::TCVector<CIoUringSqe> mp_PendingSqes; // Fully prepared entries shared across reaping/reentry; the flush index prevents resubmission.
	umint mp_iNextSqe = 0;

	NMib::NContainer::TCVector<CUringIoOp *> mp_NotifyPending; // Result already reported; release notification can outlive registration.
	umint mp_nNotifyPendingBytes = 0;

	DMibListLinkDS_List(CUringRegistration, m_StreamFlushLink) mp_StreamFlushQueue; // First-staged order, each registration linked once until delivery or drop.

	CIoSubSystem_Linux *mp_pIo = nullptr;

	NMib::NContainer::TCVector<uint16> mp_FreeBgids; // Reuse released 16-bit group IDs; the wider allocation counter detects exhaustion without wrapping.
	umint mp_nNextBgid = 0;

	bool mp_bRingCreated = false;
	bool mp_bFutexArmed = false;
};
