// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Core_Platform_Linux_IoLoop_Uring.h"

#include <sys/socket.h>

// Completion user data. Values below the loop-own limit are the loop's own waits; anything else
// is a registration record address with a tag in the low four bits, which the record's alignment
// keeps zero. The pointer rides intact — no generation steals high bits — so every 64 bit address
// layout works, arm64 memory tagging included
constexpr uint64 gc_UringUserData_Pipe = 1;
constexpr uint64 gc_UringUserData_Futex = 2;
constexpr uint64 gc_UringUserData_LoopOwnLimit = 16;
constexpr uint64 gc_UringUserData_TagMask = 15;

// One tag per obligation a registration can have in flight, so a cancel can target each exactly
// and a completion resolves its state with no lookup
constexpr uint64 gc_UringTag_ReadPoll = 1;
constexpr uint64 gc_UringTag_WritePoll = 2;
constexpr uint64 gc_UringTag_ClosePoll = 3;
constexpr uint64 gc_UringTag_Cancel = 4;
// The standing multishot receive, addressed by its registration: only one exists per descriptor,
// and every buffer it fills is named by the completion itself
constexpr uint64 gc_UringTag_RecvStream = 5;
// A send names its operation rather than its registration, because a zero copy one answers a
// second time — the notification — after the registration can be gone. See CUringIoOp
constexpr uint64 gc_UringTag_SendOp = 6;

static_assert(alignof(CUringRegistration) > gc_UringUserData_TagMask);

// The vectors a completion send accepts in one operation; matches what the readiness send path
// hands to sendmsg, and NMib::NSys::gc_IoLoopMaxSubmitSpans promises callers above no more arrive
constexpr umint gc_UringMaxSendVectors = NMib::NSys::gc_IoLoopMaxSubmitSpans;

// The producer tail overlays the first ring entry's reserved field, as the ABI lays it out
constexpr umint gc_UringPbufRingTailOffset = 14;

// Provided-buffer ring memory: plain user memory the kernel pins at registration, which only
// has to start on a page boundary — the heap's aligned path satisfies that without a mapping
// of its own. Sized in whole pages since the pin covers whole pages anyway
umint fg_UringRingBytes(umint _nEntries);
void *fg_UringAllocRing(umint _nRingBytes);

// An in-flight completion transfer. The operation owns its completion functor and, for sends, the
// gathered vectors: the kernel reads the header and vectors until the completion arrives, so they
// must live in memory that survives until the operation's completions are reaped.
//
// A zero copy send is the one operation that outlives its registration. It answers twice — a result
// and, once the pages are released, a notification — and the notification waits on the peer, which
// can take as long as the peer likes. Counting it against the registration would deadlock teardown:
// the acknowledgement that closes the descriptor waits for the outstanding count, and for a peer
// that has stopped reading the pages are only released when that descriptor closes. So only the
// result is counted, the registration is freed on it, and the operation owns itself from there —
// which is also why a zero copy send's user data names the operation and not the registration
struct alignas(16) CUringIoOp
{
	CUringRegistration *m_pRegistration;
	NMib::NSys::FIoCompletion m_fOnComplete;

	// Runs exactly once, after the completion, when the kernel no longer references the send's
	// buffers: right after the result for an ordinary send, at the notification for a zero copy
	// one, and after the cancelled completion on every path that never reached the kernel
	NMib::NSys::FIoBufferReleased m_fOnBufferReleased;

	NMib::NSys::FIoStreamSink m_fSink;
	NMib::NStorage::TCSharedPointer<NMib::NSys::CIoStreamBackpressure> m_pBackpressure;

	// Where the loop holds this operation while its notification is outstanding, so a teardown
	// that destroys the ring can still release what never got one. Not an index into anything
	// while the operation is an ordinary one
	umint m_iNotifyPending = ~umint(0);

	// Receive stream buffer size request
	umint m_nBytes;

#if DMibConfig_IoDebug_Enable
	// What the send offered, for the exit statistics' short-send accounting
	umint m_nRequested = 0;
	// When the submit entry queued this operation, io-stats only: splits the send idle gap
	// into the caller's share (completion to enqueue) and the loop's (enqueue to placement)
	uint64 m_EnqueueStamp = 0;
#endif

	// Messages rather than operations: stream-start carries the sink and backpressure for the
	// standing receive to the loop thread, where the ring is built and the multishot receive
	// armed; stream-resume answers a backpressure release by refilling parked bids there
	bool m_bStreamStart = false;
	bool m_bStreamResume = false;

	// Sent with the pages handed to the kernel rather than copied, so a notification is still owed
	// after the result and the buffers may not be touched until it arrives
	bool m_bZeroCopy = false;

	// The standing bundle send: carries no data of its own — the kernel picks the published
	// ring entries — and is re-issued while records remain. Its completion covers the records
	// FIFO in publish order
	bool m_bBundle = false;

	// Send gather
	msghdr m_MsgHdr;
	iovec m_IoVecs[gc_UringMaxSendVectors];
};

static_assert(alignof(CUringIoOp) > gc_UringUserData_TagMask);

// How many sends' buffers may await their zero copy notifications at once — the generation
// cap MalterlibIoUringSendDepth overrides. Not an operation concurrency: one send is in
// flight regardless, and the next is submitted on its result while the data keeps
// transmitting from the TCP queue. One reproduces the unpipelined path exactly
constexpr umint gc_UringDefaultSendDepth = 2;
constexpr umint gc_UringMaxSendDepth = 8;

// Buffers in a receive stream's provided ring, overridden by MalterlibIoUringReceiveBuffers.
// The kernel receives as long as any are free, so this bounds how far the stream runs ahead
// of the consumer — the flow control the per-operation depth used to be
constexpr umint gc_UringMinReceiveBuffers = 4;
constexpr umint gc_UringMaxReceiveBuffers = 64;

// The slice a large socket buffer is cut into for the provided ring: small enough that the
// blocks cycle hot through the recycler — buffer heat, not operation count, is what the
// completion receive path's cost is made of, and bundled completions keep the operation
// count independent of how finely the ring is sliced — while large enough that per-buffer
// bookkeeping stays negligible. A socket whose own buffer is smaller keeps it whole
constexpr umint gc_UringReceiveSliceBytes = 64 * 1024;

// How many sends this loop will keep with the kernel at once for one descriptor. Answered once,
// like the other io debugging knobs. One reproduces the unpipelined path exactly
#if DMibConfig_IoDebug_Enable
umint fg_UringSendDepth();
#else
constexpr umint fg_UringSendDepth()
{
	return gc_UringDefaultSendDepth;
}
#endif

// Debug override for the number of buffers in a receive stream's provided ring,
// 0 = derive the count from the socket's own buffer size
#if DMibConfig_IoDebug_Enable
umint fg_UringReceiveBuffersOverride();
#else
constexpr umint fg_UringReceiveBuffersOverride()
{
	return 0;
}
#endif

// Debug override for the size of each receive stream buffer, 0 = use the socket's own size
#if DMibConfig_IoDebug_Enable
umint fg_UringReceiveBufferBytesOverride();
#else
constexpr umint fg_UringReceiveBufferBytesOverride()
{
	return 0;
}
#endif

// Cumulative io statistics, reported at process exit when MalterlibIoStats=1: what the
// benchmarks need to check assumptions against — how big the deliveries actually are,
// whether the ring ever runs dry, how often the window parks, and what the send pipeline is
// doing. Relaxed atomics: several loops write them, exactness per counter is not the point.
// Everything here — the counters, the clock, the dump, and every recording site — exists
// only in builds carrying the io debugging overrides
#if DMibConfig_IoDebug_Enable
struct CUringStats
{
	NMib::NAtomic::TCAtomic<uint64> m_nRecvSegments = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nRecvBytes = 0;
	NMib::NAtomic::TCAtomic<uint64> m_RecvSizeBuckets[33] = {};
	NMib::NAtomic::TCAtomic<uint64> m_nRecvBufferAllocs = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nRecvBufferReuses = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendPublishes = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendSubmitLagNs = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendSubmitLagOps = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nRecvBufferAllocBytes = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nStreamArms = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nStreamEnobufs = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nStreamParks = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nStreamResumes = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendOps = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendZcOps = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendShort = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendBytesRequested = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendBytesSent = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendNotifs = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendErrors = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendIdleGaps = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendIdleNs = 0;
	NMib::NAtomic::TCAtomic<uint64> m_SendSizeBuckets[33] = {};
	NMib::NAtomic::TCAtomic<uint64> m_SendZcSizeBuckets[33] = {};
	NMib::NAtomic::TCAtomic<uint64> m_nRecvErrors = 0;
};

extern CUringStats g_UringStats;

// Monotonic nanoseconds for the send idle gap measurement; only called when stats are on
uint64 fg_UringStatsNow();
bool fg_UringStatsEnabled();
#endif

// Settles a registration's zero copy eligibility once, at its first send
void fg_UringProbePeerClass(CUringRegistration *_pRegistration);

// The ring's block reuse, shared with every buffer born from it. A dying buffer returns its
// block here instead of to the allocator, and the refill pops newest first, so the block the
// kernel copies into next is the one most recently touched — measured against fresh blocks per
// delivery, the same copies ran at ~170x the LLC misses because the working set cycled through
// the whole backpressure window cold. Consumers die on any thread while the loop refills on its
// own, hence the lock; it outlives the ring through the buffers' own references, and the ring's
// teardown marks it dead so late returns fall through to the allocator
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

// One receive buffer, as the thing that keeps itself alive: the kernel fills it, the segment
// carries a shared reference to it up the stack, and whoever drops the last reference — an app
// callback, a view slice, a send queue it was forwarded into, a discarded actor job — frees it
// right there and releases its backpressure charge. There is no return protocol to forget: the
// destructor is the release event, and it runs exactly once on whichever thread that happens
struct CUringStreamBuffer final : NMib::CVirtualDestroyBase
{
	~CUringStreamBuffer() override;

	uint8 *m_pData = nullptr;
	umint m_nDataBytes = 0;
	NMib::NStorage::TCSharedPointer<CUringBufferRecycler> m_pRecycler;
	NMib::NStorage::TCSharedPointer<NMib::NSys::CIoStreamBackpressure> m_pBackpressure;

	// The capacity this buffer was charged against the stream's accounting — the allocation, not
	// the payload, because memory is what the limit bounds
	umint m_nCharged = 0;
};

// The kernel-visible side of one receive stream: the provided-buffer ring and which buffer each
// bid currently lends to the kernel. Plain registration-owned state, loop thread only — the
// buffers themselves outlive it through the references the segments carry
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

	// Incremental consumption: the kernel fills each buffer across several completions, so
	// consecutive receives land adjacently and consumers can stitch them back into single
	// views; a completion without the buf-more flag retires the buffer
	bool m_bIncremental = false;

	// The buffer each bid has with the kernel; empty while the backpressure gate holds that bid
	// back from being refilled
	NMib::NContainer::TCVector<NMib::NStorage::TCSharedPointer<CUringStreamBuffer>> m_BufferSlots;
	NMib::NContainer::TCVector<uint16> m_UnfilledBids;
	// How far the kernel has filled each bid's buffer, incremental rings only
	NMib::NContainer::TCVector<umint> m_SlotOffsets;

	// Publish order, which is also the kernel's consumption order: a bundle completion names
	// only its first bid, so which buffers the rest of its bytes landed in is answered by this
	// queue rather than by anything on the completion. Bids enter as they are lent and leave
	// as the dispatch walk retires them; a partially filled tail on an incremental ring stays
	// at the front, holding its offset
	NMib::NContainer::TCVector<uint16> m_PublishOrder;
	umint m_iOrderHead = 0;
	umint m_nOrderCount = 0;

	NMib::NStorage::TCSharedPointer<NMib::NSys::CIoStreamBackpressure> m_pBackpressure;
	NMib::NStorage::TCSharedPointer<CUringBufferRecycler> m_pRecycler;
};

// One published transfer: the bytes one f_SubmitSendVectored call handed over and the functors
// that answer for them, resolved in publish order as bundle completions cover the FIFO
struct CUringSendRecord
{
	umint m_nBytes = 0;
	umint m_nCovered = 0;
	umint m_nEntries = 0;
	NMib::NSys::FIoCompletion m_fOnComplete;
	NMib::NSys::FIoBufferReleased m_fOnBufferReleased;
};

// The kernel-visible side of a local peer's sends: a provided-buffer ring the transfers'
// spans are published into, and the record FIFO that maps completed bytes back to callers.
// Registration-owned, loop thread only
struct CUringSendRing
{
	void f_PublishSpan(void const *_pData, umint _nBytes);
	void f_CommitTail();

	void *m_pRingMem = nullptr;
	umint m_nRingMemSize = 0;
	CIoUringBuf *m_pRingEntries = nullptr;
	uint16 *m_pRingTail = nullptr;
	umint m_nRingEntries = 0;

	// Ring entries published and not yet covered by a completion; a publish may not wrap onto
	// entries the kernel still owns
	umint m_nEntriesOutstanding = 0;

	NMib::NContainer::TCLinkedList<CUringSendRecord> m_Records;

	uint16 m_Tail = 0;
	uint16 m_Bgid = 0;
};
