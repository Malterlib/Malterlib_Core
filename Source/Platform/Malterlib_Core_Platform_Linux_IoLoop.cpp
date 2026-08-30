// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Linux_IoLoop.h"
#include "Malterlib_Core_Platform_Linux_IoLoop_Epoll.h"
#include "Malterlib_Core_Platform_Linux_IoLoop_Uring.h"
#include "Malterlib_Core_Platform_Linux_IoLoop_Uring_Internal.h"
#include "Malterlib_Core_Platform_Linux_IoSubSystem.h"

using namespace NMib;
using namespace NMib::NMemory;

#if DMibConfig_IoDebug_Enable
bool fg_UringTraceEnabled()
{
	return fg_IoSubSystem_Linux().f_TraceEnabled();
}

void fg_UringTrace(char const *_pWhat, void const *_pToken, NMib::NSys::CIoLoopHandle _Handle, uint32 _Value)
{
	int64 Ticks = NTime::CSystem_Time::fs_GetTimerValue();
	int64 Frequency = NTime::CSystem_Time::fs_TimerFrequency();
	int64 Seconds = Ticks / Frequency;
	int64 Microseconds = ((Ticks % Frequency) * 1000000) / Frequency;

	NSys::fg_ConsoleErrorOutput
		(
			NStr::fg_Format<NStr::CStrNonTracked>
			(
				"[uring {}.{}] {} socket=0x{} fd={} value=0x{}\n"
				, NStr::fg_FormatMinLength<5>(NStr::fg_FormatIntFormat<10>(Seconds))
				, NStr::fg_FormatMinLength<6>(NStr::fg_FormatFillOut<'0'>(NStr::fg_FormatIntFormat<10>(Microseconds)))
				, _pWhat
				, NStr::fg_FormatIntFormat<16>((umint)_pToken)
				, _Handle
				, NStr::fg_FormatIntFormat<16>(_Value)
			)
		)
	;
}
#endif

// The kernel charges every ring's memory, and the pinned pages of every zero copy send, to the
// user's locked memory limit (RLIMIT_MEMLOCK). Its legacy default of 8 MiB is less than the rings
// of one loop per pool thread on a machine with a few dozen cores, which left the first zero copy
// send failing with ENOMEM and the transport dropping the connection. Decided once, before the
// first loop: the rings of every loop the process may create — the actor pools take one per
// thread per priority — plus a margin for sends in flight have to fit what the limit still has,
// measured against the kernel's own accounting so that the rings other processes of the same
// user hold count too. Otherwise every loop is an epoll loop, which pins nothing, and the log
// says why (fg_UringLogMemlockFallback)
namespace
{
	constexpr umint gc_UringLoopsPerCore = 3;
	constexpr umint gc_UringExtraLoops = 4;
	constexpr umint gc_UringSendMarginBytes = 16 * 1024 * 1024;

	struct CUringMemlockDecision
	{
		bool m_bFits = true;
		umint m_nLimitBytes = 0;
		umint m_nLoops = 0;
		umint m_nRingBytes = 0;
	};

	CUringMemlockDecision const &fg_UringMemlockDecision()
	{
		static CUringMemlockDecision s_Decision =
			(
				[]() -> CUringMemlockDecision
				{
					CUringMemlockDecision Decision;
					Decision.m_nLoops = NSys::fg_Thread_GetVirtualCores() * gc_UringLoopsPerCore + gc_UringExtraLoops;
					Decision.m_nRingBytes = CIoUringRing::fs_RingBytes(gc_UringLoopSqEntries, gc_UringLoopCqEntries);

					umint nNeededBytes = Decision.m_nLoops * Decision.m_nRingBytes + gc_UringSendMarginBytes;
					Decision.m_bFits = CIoUringRing::fs_MemlockFits(nNeededBytes, Decision.m_nLimitBytes);

					return Decision;
				}
				()
			)
		;

		return s_Decision;
	}
}

void fg_UringLogMemlockFallback()
{
	static NAtomic::TCAtomic<bool> s_bLogged = false;

	CUringMemlockDecision const &Decision = fg_UringMemlockDecision();
	if (Decision.m_bFits || s_bLogged.f_Exchange(true))
		return;

	DMibLogWithCategory
		(
			Mib/Core/IoLoop
			, Warning
			, "io_uring disabled: the locked memory limit (RLIMIT_MEMLOCK, {} KiB) cannot hold the rings of {} io loops ({} KiB) plus {} KiB for zero copy sends. "
				"Sockets use epoll instead; raise the limit (ulimit -l) to enable io_uring"
			, Decision.m_nLimitBytes / 1024
			, Decision.m_nLoops
			, Decision.m_nLoops * Decision.m_nRingBytes / 1024
			, gc_UringSendMarginBytes / 1024
		);
}

NMib::NSys::ICIoLoop *fg_CreatePlatformIoLoop()
{
#if DMibConfig_IoDebug_Enable
	// The exit report registers on the first ask; asking at loop creation makes a run whose
	// transfers stayed on readiness report the completion counters' zeros alongside
	fg_UringStatsEnabled();
#endif

	if (CIoUringRing::fs_Available() && fg_UringMemlockDecision().m_bFits)
	{
		// Ring creation can still fail where the probe passed (descriptor or memory pressure at
		// the mmaps); the epoll backend takes over then
		auto *pUring = fg_ConstructObject<CIoLoop_IoUring>(CAllocator_NonTrackedHeap());
		if (pUring->f_IsRingCreated())
			return pUring;

		fg_DeleteObject(CAllocator_NonTrackedHeap(), pUring);
	}

	return fg_ConstructObject<CIoLoop_Epoll>(CAllocator_NonTrackedHeap());
}
