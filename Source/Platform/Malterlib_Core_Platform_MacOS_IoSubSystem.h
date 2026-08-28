// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "../Malterlib_Core_IoSubSystem.h"

struct CIoSubSystem_MacOS : NMib::NSys::CIoSubSystem
{
	CIoSubSystem_MacOS();

	umint m_nMaxSocketReserveBytes = 0; // sbreserve ceiling: eight ninths of kern.ipc.maxsockbuf.
};

CIoSubSystem_MacOS &fg_IoSubSystem_MacOS();
