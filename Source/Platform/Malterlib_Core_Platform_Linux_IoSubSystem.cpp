// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Linux_IoSubSystem.h"

#include <sys/resource.h>

using namespace NMib;

// The warning the epoll fallback prints once when io_uring was there but did not fit
void fg_UringLogMemlockFallback()
{
	static NAtomic::TCAtomic<bool> s_bLogged = false;

	auto &Io = fg_IoSubSystem_Linux();
	if (!Io.m_bUringAvailable || Io.m_bUringMemlockFits || s_bLogged.f_Exchange(true))
		return;

	DMibLogWithCategory
		(
			Mib/Core/IoLoop
			, Warning
			, "io_uring disabled: the locked memory limit (RLIMIT_MEMLOCK, {} KiB) cannot hold the rings of {} io loops ({} KiB) plus {} KiB for zero copy sends. "
				"Sockets use epoll instead; raise the limit (ulimit -l) to enable io_uring"
			, Io.m_nUringMemlockLimitBytes / 1024
			, Io.m_nUringMemlockLoops
			, Io.m_nUringMemlockLoops * Io.m_nUringMemlockRingBytes / 1024
			, gc_UringSendMarginBytes / 1024
		)
	;
}

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
	m_bUringAvailable = CIoUringRing::fs_Available(m_UringCaps);
	if (m_bUringAvailable)
	{
		m_nUringMemlockLoops = NSys::fg_Thread_GetVirtualCores() * gc_UringLoopsPerCore + gc_UringExtraLoops;
		m_nUringMemlockRingBytes = CIoUringRing::fs_RingBytes(gc_UringLoopSqEntries, gc_UringLoopCqEntries);

		umint nNeededBytes = m_nUringMemlockLoops * m_nUringMemlockRingBytes + gc_UringSendMarginBytes;

		rlimit Limit;
		if (getrlimit(RLIMIT_MEMLOCK, &Limit) == 0 && Limit.rlim_cur != RLIM_INFINITY)
		{
			m_nUringMemlockLimitBytes = (umint)Limit.rlim_cur;
			m_bUringMemlockFits = nNeededBytes <= m_nUringMemlockLimitBytes;
		}
	}

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
