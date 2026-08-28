// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Core_Platform_Linux_IoLoop_Uring_Internal.h"


// RLIMIT_MEMLOCK covers rings and pinned sends across the user's processes. Reserve estimated loop capacity plus send headroom;
// fall back to epoll when it cannot fit. Other processes can consume the budget later, so this is only a startup heuristic.
constexpr umint gc_UringLoopsPerCore = 3;
constexpr umint gc_UringExtraLoops = 4;
constexpr umint gc_UringSendMarginBytes = 32 * 1024 * 1024;

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
	NMib::NAtomic::TCAtomic<uint64> m_nSendMaxInFlight = 0; // Maximum generations on one loop awaiting zero-copy release notifications.
	NMib::NAtomic::TCAtomic<uint64> m_nSendMaxBytesInFlight = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendNotifLagNs = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendNotifLagOps = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendNotifLagMaxNs = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendErrors = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendIdleGaps = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendIdleNs = 0;
	NMib::NAtomic::TCAtomic<uint64> m_SendSizeBuckets[33] = {};
	NMib::NAtomic::TCAtomic<uint64> m_SendZcSizeBuckets[33] = {};
	NMib::NAtomic::TCAtomic<uint64> m_nRecvErrors = 0;
};
#endif

struct CIoSubSystem_Linux : NMib::NSys::CIoSubSystem
{
	CIoSubSystem_Linux();

	inline umint f_SendDepth() const;

	inline umint f_ReceiveBuffersOverride() const;
	inline umint f_ReceiveBufferBytesOverride() const;

	inline EUringZeroCopyOverride f_ZeroCopyOverride() const;
	inline bool f_TraceEnabled() const;

	umint m_nUringMemlockLimitBytes = 0; // Startup estimate; other processes can consume the user's memlock budget later.
	umint m_nUringMemlockLoops = 0;
	umint m_nUringMemlockRingBytes = 0;
	bool m_bUringAvailable = false;
	bool m_bUringMemlockFits = true;
	CIoUringCaps m_UringCaps;

#if DMibConfig_IoDebug_Enable
	umint m_nSendDepth = gc_UringDefaultSendDepth;
	umint m_nReceiveBuffersOverride = 0;
	umint m_nReceiveBufferBytesOverride = 0;

	CUringStats m_UringStats;
	EUringZeroCopyOverride m_ZeroCopyOverride = EUringZeroCopyOverride::mc_None;
	bool m_bTraceEnabled = false;
#endif
};

CIoSubSystem_Linux &fg_IoSubSystem_Linux();

#if DMibConfig_IoDebug_Enable
void fg_DumpUringStats(NMib::NSys::CIoSubSystem &_Io);
#endif

#include "Malterlib_Core_Platform_Linux_IoSubSystem.hpp"
