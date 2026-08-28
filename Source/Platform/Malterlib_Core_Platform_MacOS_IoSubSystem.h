// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "../Malterlib_Core_IoSubSystem.h"

// This platform has no io knobs of its own yet; the subsystem carries the shared ones and the
// socket buffer ceiling the kernel enforces
struct CIoSubSystem_MacOS : NMib::NSys::CIoSubSystem
{
	CIoSubSystem_MacOS();

	// What sbreserve accepts: sb_max * MCLBYTES / (MSIZE + MCLBYTES), eight ninths of
	// kern.ipc.maxsockbuf, read once here
	umint m_nMaxSocketReserveBytes = 0;
};

// The subsystem, as the derived type; NSys::fg_IoSubSystem answers the same object as the base
CIoSubSystem_MacOS &fg_IoSubSystem_MacOS();
