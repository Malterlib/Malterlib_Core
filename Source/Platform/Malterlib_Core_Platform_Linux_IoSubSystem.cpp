// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Linux_IoSubSystem.h"

using namespace NMib;

namespace
{
	constinit TCSubSystem<CIoSubSystem_Linux, ESubSystemDestruction_BeforeMemoryManager> g_IoSubSystem = {DAggregateInit};
}

NMib::NSys::CIoSubSystem &NMib::NSys::fg_IoSubSystem()
{
	return *g_IoSubSystem;
}

CIoSubSystem_Linux &fg_IoSubSystem_Linux()
{
	return *g_IoSubSystem;
}

CIoSubSystem_Linux::CIoSubSystem_Linux()
{
#if DMibConfig_IoDebug_Enable
	m_bTraceEnabled = fsp_EnvFlag("MalterlibUringTrace", false);
	m_nSendDepth = fsp_EnvCount("MalterlibIoUringSendDepth", gc_UringDefaultSendDepth, 1, gc_UringMaxSendDepth);
	m_nReceiveBuffersOverride = fsp_EnvCount("MalterlibIoUringReceiveBuffers", 0, 2, gc_UringMaxReceiveBuffers);
	m_nReceiveBufferBytesOverride = fsp_EnvCount("MalterlibIoUringRecvBufferBytes", 0, 4096, umint(1) << 30);

	switch (fsp_EnvKnob("MalterlibIoUringZeroCopyLocal"))
	{
	case NSys::EIoKnob::mc_Off:
		m_ZeroCopyOverride = EUringZeroCopyOverride::mc_Never;
		break;

	case NSys::EIoKnob::mc_On:
		m_ZeroCopyOverride = EUringZeroCopyOverride::mc_Always;
		break;

	case NSys::EIoKnob::mc_Default:
		break;
	}

	if (f_StatsEnabled())
		f_RegisterStatsDump(&fg_DumpUringStats);
#endif
}
