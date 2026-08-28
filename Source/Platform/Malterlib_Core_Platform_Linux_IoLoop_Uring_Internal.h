// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "../Malterlib_Core_IoSubSystem.h"

#include "Malterlib_Core_Platform_Linux_IoLoop_Uring.h"

#include <sys/socket.h>

// The CQ must hold many completions per SQ entry for multishot receives and bundled sends.
constexpr uint32 gc_UringLoopSqEntries = 256;
constexpr uint32 gc_UringLoopCqEntries = 4096;

constexpr uint64 gc_UringUserData_Pipe = 1;
constexpr uint64 gc_UringUserData_Futex = 2;
constexpr uint64 gc_UringUserData_LoopOwnLimit = 16;
constexpr uint64 gc_UringUserData_TagMask = 15;

constexpr uint64 gc_UringTag_ReadPoll = 1; // Low-bit tags preserve the full aligned pointer, including arm64 memory tags.
constexpr uint64 gc_UringTag_WritePoll = 2;
constexpr uint64 gc_UringTag_ClosePoll = 3;
constexpr uint64 gc_UringTag_Cancel = 4;
constexpr uint64 gc_UringTag_RecvStream = 5;
constexpr uint64 gc_UringTag_SendOp = 6; // Names the operation because release notification may outlive registration.

static_assert(alignof(CUringRegistration) > gc_UringUserData_TagMask);

constexpr umint gc_UringMaxSendVectors = NMib::NSys::gc_IoLoopMaxSubmitSpans;

// The producer tail overlays the first ring entry's reserved field, as the ABI lays it out
constexpr umint gc_UringPbufRingTailOffset = 14;

umint fg_UringRingBytes(umint _nEntries);
void *fg_UringAllocRing(umint _nRingBytes);

// Retain headers, vectors and callbacks until kernel completion. Zero-copy notifications may outlive registration:
// count only the result against registration, since waiting for peer reads before closing its descriptor can deadlock teardown.
struct alignas(16) CUringIoOp
{
	CUringRegistration *m_pRegistration;
	NMib::NSys::FIoCompletion m_fOnComplete;
	NMib::NSys::FIoBufferReleased m_fOnBufferReleased; // Runs once after completion when kernel references end, including cancellation before submission.

	NMib::NSys::FIoStreamSink m_fSink;
	NMib::NStorage::TCSharedPointer<NMib::NSys::CIoStreamBackpressure> m_pBackpressure;

	umint m_iNotifyPending = ~umint(0); // All-ones unless retained in the loop's notification list.
	CUringRegistration *m_pNotifyRegistration = nullptr; // Cleared at deregistration acknowledgement before a late notification can access registration state.
	uint64 m_ReleaseLagIssueStamp = 0;

	umint m_nBytes;
	umint m_nRequested = 0; // Offered byte count used for outstanding-byte accounting and short-send detection.

#if DMibConfig_IoDebug_Enable
	uint64 m_EnqueueStamp = 0; // Statistics timestamp separating caller delay from loop submission delay.
#endif

	bool m_bStreamStart = false; // Control message installing the standing stream; not a kernel transfer.
	bool m_bStreamResume = false;
	bool m_bZeroCopy = false; // Buffers remain kernel-owned after the result until notification arrives.
	bool m_bBundle = false; // Carries no payload; completion covers published records in FIFO order.

	msghdr m_MsgHdr;
	iovec m_IoVecs[gc_UringMaxSendVectors];
};

static_assert(alignof(CUringIoOp) > gc_UringUserData_TagMask);

constexpr umint gc_UringDefaultSendDepth = 8; // Maximum unreleased zero-copy generations; kernel sends remain sequential.
constexpr umint gc_UringMaxSendDepth = 8;

constexpr umint gc_UringMinReceiveBuffers = 4; // Minimum provided receive buffers to keep the kernel fed ahead of the consumer.
constexpr umint gc_UringMaxReceiveBuffers = 64;

constexpr umint gc_UringReceiveSliceBytes = 64 * 1024; // Bound recycler working-set size while amortizing per-buffer bookkeeping.

enum class EUringZeroCopyOverride
{
	mc_None
	, mc_Never
	, mc_Always
};

#if DMibConfig_IoDebug_Enable
uint64 fg_UringStatsNow();
#endif

void fg_UringProbePeerClass(CIoSubSystem_Linux *_pIo, CUringRegistration *_pRegistration);

// Reuse newest blocks first for cache locality. A lock protects cross-thread returns and loop refills.
// Buffers retain the recycler after ring teardown; once marked dead, late returns go to the allocator.
struct CUringBufferRecycler final
{
	~CUringBufferRecycler();

	bool f_TryReturn(uint8 *_pBlock);
	uint8 *f_TryTake();
	void f_Die();

	NMib::NThread::CMutual m_Lock;
	NMib::NContainer::TCVector<uint8 *> m_FreeBlocks;
	umint m_nBufferBytes = 0;
	umint m_nMaxFree = 0;
	bool m_bDead = false;
};

// The final shared reference releases buffer capacity and storage on whichever thread drops it.
struct CUringStreamBuffer final : NMib::CVirtualDestroyBase
{
	~CUringStreamBuffer() override;

	uint8 *m_pData = nullptr;
	umint m_nDataBytes = 0;
	NMib::NStorage::TCSharedPointer<CUringBufferRecycler> m_pRecycler;
	NMib::NStorage::TCSharedPointer<NMib::NSys::CIoStreamBackpressure> m_pBackpressure;
	umint m_nCharged = 0; // Capacity charged to backpressure, not delivered payload.
};

// Registration-owned, loop-thread state. Delivered buffers can outlive the ring through segment references.
struct CUringRecvRing
{
	uint16 f_OrderFront() const;
	void f_OrderPop();
	void f_PublishBuffer(uint16 _Bid, CUringStreamBuffer &_Buffer);

	void *m_pRingMem = nullptr;
	umint m_nRingMemSize = 0;
	CIoUringBuf *m_pRingEntries = nullptr;
	uint16 *m_pRingTail = nullptr;
	umint m_nRingEntries = 0;

	umint m_nBuffers = 0;
	umint m_nBufferBytes = 0;

	uint16 m_Tail = 0;
	uint16 m_Bgid = 0;

	NMib::NContainer::TCVector<NMib::NStorage::TCSharedPointer<CUringStreamBuffer>> m_BufferSlots; // Empty slots are parked by backpressure until refill.
	NMib::NContainer::TCVector<uint16> m_UnfilledBids;
	NMib::NContainer::TCVector<umint> m_SlotOffsets; // Incremental receive offset; a buffer stays kernel-owned until full.
	NMib::NContainer::TCVector<uint16> m_PublishOrder; // Kernel consumption order; bundle CQEs name only the first buffer and a partial tail remains at the front.
	umint m_iOrderHead = 0;
	umint m_nOrderCount = 0;

	NMib::NStorage::TCSharedPointer<NMib::NSys::CIoStreamBackpressure> m_pBackpressure;
	NMib::NStorage::TCSharedPointer<CUringBufferRecycler> m_pRecycler;
};

// Resolve callbacks in publish order as bundle bytes cover the record FIFO.
struct CUringSendRecord
{
	umint m_nBytes = 0;
	umint m_nCovered = 0;
	umint m_nEntries = 0;
	NMib::NSys::FIoCompletion m_fOnComplete;
	NMib::NSys::FIoBufferReleased m_fOnBufferReleased;
};

// Registration-owned, loop-thread state mapping published send spans to completion callbacks.
struct CUringSendRing
{
	void f_PublishSpan(void const *_pData, umint _nBytes);
	void f_CommitTail();
	bool f_HasLostEntries(uint16 _KernelHead) const;

	void *m_pRingMem = nullptr;
	umint m_nRingMemSize = 0;
	CIoUringBuf *m_pRingEntries = nullptr;
	uint16 *m_pRingTail = nullptr;
	umint m_nRingEntries = 0;
	umint m_nEntriesOutstanding = 0; // Do not overwrite ring entries until completion covers them.
	umint m_nBytesOutstanding = 0; // A bundle selects at most the signed 32-bit range; an entry cut at that boundary is consumed whole with the cut applied.

	NMib::NContainer::TCLinkedList<CUringSendRecord> m_Records;

	uint16 m_Tail = 0;
	uint16 m_Bgid = 0;
};
