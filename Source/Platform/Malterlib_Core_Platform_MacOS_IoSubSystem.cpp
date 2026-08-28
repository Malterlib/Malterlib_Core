// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

#include "Malterlib_Core_Platform_MacOS_IoSubSystem.h"

#include <sys/sysctl.h>

using namespace NMib;

namespace
{
	constinit TCSubSystem<CIoSubSystem_MacOS, ESubSystemDestruction_BeforeMemoryManager> g_IoSubSystem = {DAggregateInit};
}

NMib::NSys::CIoSubSystem &NMib::NSys::fg_IoSubSystem()
{
	return *g_IoSubSystem;
}

CIoSubSystem_MacOS &fg_IoSubSystem_MacOS()
{
	return *g_IoSubSystem;
}

CIoSubSystem_MacOS::CIoSubSystem_MacOS()
{
	uint64 nMax = 0;
	size_t nSize = sizeof(nMax);
	if (sysctlbyname("kern.ipc.maxsockbuf", &nMax, &nSize, nullptr, 0) != 0 || !nMax)
		nMax = 8 * 1024 * 1024;

	m_nMaxSocketReserveBytes = umint(nMax) / 9 * 8;
}
