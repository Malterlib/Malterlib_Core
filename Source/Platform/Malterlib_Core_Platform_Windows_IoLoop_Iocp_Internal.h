// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Core_Platform_Windows_IoSubSystem.h"

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

#if DMibConfig_IoDebug_Enable
// Monotonic nanoseconds for the send lag measurements; only called when stats are on
uint64 fg_IocpStatsNow();
#endif
