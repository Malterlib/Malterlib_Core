// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Core_Platform_Linux_IoLoop_Uring_Internal.h"

// The Linux io subsystem: this platform's io knobs, decided once in the constructor; without
// the io debugging overrides the accessors answer their compile time defaults as constants

// The kernel charges every ring's memory, and the pinned pages of every zero copy send, to the
// user's locked memory limit (RLIMIT_MEMLOCK). Its legacy default of 8 MiB is less than the rings
// of one loop per pool thread on a machine with a few dozen cores, which left the first zero copy
// send failing with ENOMEM and the transport dropping the connection. Decided once, in the
// constructor: the rings of every loop the process may create — the actor pools take one per
// thread per priority — plus a margin for sends in flight have to fit under the limit. The limit
// is charged per user across all their processes and the amount already charged cannot be read
// back, so the margin also stands in for that; the check is a heuristic either way, since other
// processes can take the budget at any later point. Otherwise every loop is an epoll loop,
// which pins nothing, and the log says why (fg_UringLogMemlockFallback)
constexpr umint gc_UringLoopsPerCore = 3;
constexpr umint gc_UringExtraLoops = 4;
constexpr umint gc_UringSendMarginBytes = 32 * 1024 * 1024;

struct CIoSubSystem_Linux : NMib::NSys::CIoSubSystem
{
	CIoSubSystem_Linux();

	// How many sends the loop keeps with the kernel at once for one descriptor; one reproduces
	// the unpipelined path exactly
	inline umint f_SendDepth() const;

	// Debug override for the number of buffers in a receive stream's provided ring,
	// 0 = derive the count from the socket's own buffer size
	inline umint f_ReceiveBuffersOverride() const;

	// Debug override for the size of each receive stream buffer, 0 = use the socket's own size
	inline umint f_ReceiveBufferBytesOverride() const;

	inline EUringZeroCopyOverride f_ZeroCopyOverride() const;
	inline bool f_TraceEnabled() const;

	// Probed once at construction: whether io_uring works here at all, and whether the loops
	// the process may create fit the locked memory limit. The loops read these members instead
	// of probing per creation
	umint m_nUringMemlockLimitBytes = 0;
	umint m_nUringMemlockLoops = 0;
	umint m_nUringMemlockRingBytes = 0;
	bool m_bUringAvailable = false;
	bool m_bUringMemlockFits = true;

	// What the kernel's io_uring supports, filled by the same probe
	CIoUringCaps m_UringCaps;

#if DMibConfig_IoDebug_Enable
	umint m_nSendDepth = gc_UringDefaultSendDepth;
	umint m_nReceiveBuffersOverride = 0;
	umint m_nReceiveBufferBytesOverride = 0;
	EUringZeroCopyOverride m_ZeroCopyOverride = EUringZeroCopyOverride::mc_None;
	bool m_bTraceEnabled = false;
#endif
};

// The subsystem, as the derived type; NSys::fg_IoSubSystem answers the same object as the base
CIoSubSystem_Linux &fg_IoSubSystem_Linux();

#include "Malterlib_Core_Platform_Linux_IoSubSystem.hpp"
