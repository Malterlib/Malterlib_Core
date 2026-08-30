// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Core_Platform_Windows_IoLoop.h"

// Completion keys the loop owns. Socket handles are associated under their registration's
// address, but only as a cross-check: the operation is what a packet names, and a registration
// is always reached through it — a stale key survives the re-registration of a handle that was
// already associated, since the association is permanent for the handle's lifetime
constexpr ULONG_PTR gc_IocpKey_Wake = 1;
constexpr ULONG_PTR gc_IocpKey_Afd = 2;

// Registrations per \Device\Afd handle. AFD keeps the polls outstanding on one device handle in
// a list it walks on every completion, so the polls are spread over a small pool of handles
// (wepoll's measured group size)
constexpr umint gc_IocpAfdGroupSize = 32;

// Packets dequeued per wait
constexpr umint gc_IocpDequeueBatch = 64;

// Inline completions dispatched per pass before the pass hands control back, so a firehose of
// synchronously completing receives cannot starve the queue the loop is hosted in
constexpr umint gc_IocpMaxInlinePerPass = 256;

// The slice a large socket buffer is cut into for the stream's receives: with the default depth
// it is what a direct send can be delivered into ahead of the receiver (a megabyte posted), small
// enough that the blocks still cycle hot through the recycler. A socket whose own buffer is
// smaller keeps it whole
constexpr umint gc_IocpReceiveSliceBytes = 256 * 1024;

// A buffer whose untouched tail is shorter than this retires instead of taking another receive:
// the kernel would fill a sliver and the delivery would pin a whole block for it
constexpr umint gc_IocpRecvMinPostBytes = 4096;

// Memory a stream's recycler keeps for reuse. Consumers assembling messages hold a pipeline's
// worth of delivered buffers and return them in bursts, so a short list drops most of a burst
// to the allocator and the next posts go cold again — measured at a million fresh allocations
// per bulk run with a list of eight blocks; a few megabytes covers the burst at any slice size
constexpr umint gc_IocpRecyclerMaxFreeBytes = 8 * 1024 * 1024;

// One \Device\Afd handle serving a bounded number of registrations' polls
struct CIocpAfdGroup
{
	HANDLE m_hAfd = nullptr;
	umint m_nRegistrations = 0;
};

// A message from a submitter to the loop thread, applied at the start of the next pass: a send
// to append to its registration's FIFO, a stream start carrying the sink and backpressure for
// the standing receives, or a stream resume answering a backpressure release
struct CIocpPendingOp
{
	CIocpRegistration *m_pRegistration = nullptr;
	CIocpSendOp *m_pSendOp = nullptr;
	NMib::NSys::FIoStreamSink m_fSink;
	NMib::NStorage::TCSharedPointer<NMib::NSys::CIoStreamBackpressure> m_pBackpressure;
	umint m_nBytes = 0;
	bool m_bStreamStart = false;
	bool m_bStreamResume = false;
	bool m_bSendWindow = false;
};

// The io debugging knobs, answered once for the process like their io_uring counterparts.
// Without the io debugging overrides every knob is its compile time answer, so the branches
// consulting it fold away
#if DMibConfig_IoDebug_Enable
bool fg_IocpCompletionEnabled();
bool fg_IocpSkipSuccessEnabled();
umint fg_IocpSendDepth();
umint fg_IocpRecvDepth();
umint fg_IocpRecvBufferBytesOverride();
bool fg_IocpTraceEnabled();
void fg_IocpTrace(char const *_pWhat, void const *_pToken, NMib::NSys::CIoLoopHandle _Handle, uint32 _Value);
#else
constexpr bool fg_IocpCompletionEnabled()
{
	return true;
}

// Off by default: with the port skipped, a stream whose data is already waiting completes its
// receives inline, and the pass reports a whole firehose of them before the hosting queue gets
// to run the jobs that consume them — measured at half the throughput and twice the round trip
// of one packet per transfer, which interleaves the two naturally
constexpr bool fg_IocpSkipSuccessEnabled()
{
	return false;
}

constexpr umint fg_IocpSendDepth()
{
	return gc_IocpDefaultSendDepth;
}

constexpr umint fg_IocpRecvDepth()
{
	return gc_IocpDefaultRecvDepth;
}

constexpr umint fg_IocpRecvBufferBytesOverride()
{
	return 0;
}
#endif

// Cumulative io statistics, reported at process exit when MalterlibIoStats=1. Relaxed atomics:
// several loops write them, exactness per counter is not the point
#if DMibConfig_IoDebug_Enable
struct CIocpStats
{
	NMib::NAtomic::TCAtomic<uint64> m_nRecvSegments = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nRecvBytes = 0;
	NMib::NAtomic::TCAtomic<uint64> m_RecvSizeBuckets[33] = {};
	NMib::NAtomic::TCAtomic<uint64> m_nRecvBufferAllocs = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nRecvBufferReuses = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nRecvPosts = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nRecvInline = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nStreamParks = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nStreamResumes = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nRecvErrors = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendOps = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendInline = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendDeferred = 0;
	// The most sends, and bytes, one registration had issued and unfinished at once
	NMib::NAtomic::TCAtomic<uint64> m_nSendMaxInFlight = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendMaxBytesInFlight = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendShort = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendBytesRequested = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendBytesSent = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendErrors = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendIdleGaps = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendIdleNs = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendSubmitLagNs = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendSubmitLagOps = 0;
	NMib::NAtomic::TCAtomic<uint64> m_SendSizeBuckets[33] = {};
	NMib::NAtomic::TCAtomic<uint64> m_nPollArms = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nPollCancels = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nPollEvents = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nWakePosts = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nWaits = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nPackets = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nRegistrations = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSkipSuccessSockets = 0;
};

extern CIocpStats g_IocpStats;

uint64 fg_IocpStatsNow();
bool fg_IocpStatsEnabled();
#endif
