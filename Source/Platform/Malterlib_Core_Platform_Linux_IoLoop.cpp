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

NMib::NSys::ICIoLoop *fg_CreatePlatformIoLoop()
{
#if DMibConfig_IoDebug_Enable
	// The exit report registers on the first ask; asking at loop creation makes a run whose
	// transfers stayed on readiness report the completion counters' zeros alongside
	fg_UringStatsEnabled();
#endif

	auto &Io = fg_IoSubSystem_Linux();
	if (Io.m_bUringAvailable && Io.m_bUringMemlockFits)
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
