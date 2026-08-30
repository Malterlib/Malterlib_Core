// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Core_Platform_Linux_IoLoop_Uring_Internal.h"

// The Linux io subsystem: this platform's io knobs, decided once in the constructor; without
// the io debugging overrides the accessors answer their compile time defaults as constants
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

#if DMibConfig_IoDebug_Enable
	bool m_bTraceEnabled = false;
	EUringZeroCopyOverride m_ZeroCopyOverride = EUringZeroCopyOverride::mc_None;
	umint m_nSendDepth = gc_UringDefaultSendDepth;
	umint m_nReceiveBuffersOverride = 0;
	umint m_nReceiveBufferBytesOverride = 0;
#endif
};

// The subsystem, as the derived type; NSys::fg_IoSubSystem answers the same object as the base
CIoSubSystem_Linux &fg_IoSubSystem_Linux();

#include "Malterlib_Core_Platform_Linux_IoSubSystem.hpp"
