// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Windows_IoLoop_Iocp_Internal.h"

using namespace NMib;
using namespace NMib::NMemory;
using namespace NMib::NSys;

#if DMibConfig_IoDebug_Enable
static void fg_DumpIocpTraceRing();
#endif

namespace
{
	constinit TCSubSystem<CIoSubSystem_Windows, ESubSystemDestruction_BeforeMemoryManager> g_IoSubSystem = {DAggregateInit};
}

NMib::NSys::CIoSubSystem &NMib::NSys::fg_IoSubSystem()
{
	return *g_IoSubSystem;
}

CIoSubSystem_Windows &fg_IoSubSystem_Windows()
{
	return *g_IoSubSystem;
}

CIoSubSystem_Windows::CIoSubSystem_Windows()
{
#if DMibConfig_IoDebug_Enable
	m_bCompletionEnabled = fsp_EnvFlag("MalterlibIocpCompletion", true);
	m_bSkipSuccessEnabled = fsp_EnvFlag("MalterlibIocpSkipSuccess", false);
	m_bLoopbackFastPathEnabled = fsp_EnvFlag("MalterlibLoopbackFastPath", true);
	m_bDirectSendEnabled = fsp_EnvFlag("MalterlibIocpDirectSend", true);
	m_nSendDepth = fsp_EnvCount("MalterlibIocpSendDepth", gc_IocpDefaultSendDepth, 1, gc_IocpMaxSendDepth);
	m_nRecvDepth = fsp_EnvCount("MalterlibIocpRecvDepth", gc_IocpDefaultRecvDepth, 1, gc_IocpMaxRecvDepth);
	m_nRecvBufferBytesOverride = fsp_EnvCount("MalterlibIocpRecvBufferBytes", 0, 4096, umint(1) << 30);

	auto TraceSetting = NMib::NSys::fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStrNonTracked("MalterlibIocpTrace"));
	if (TraceSetting == "1")
		m_TraceMode = 1;
	else if (TraceSetting == "2")
	{
		m_TraceMode = 2;
		m_pTraceRing = (CIocpTraceEntry *)NMemory::CAllocator_NonTrackedHeap::f_Alloc(sizeof(CIocpTraceEntry) * gc_IocpTraceRingEntries);
		f_RegisterStatsDump(&fg_DumpIocpTraceRing);
	}

	if (f_StatsEnabled())
		f_RegisterStatsDump(&fg_DumpIocpStats);
#endif
}

#if DMibConfig_IoDebug_Enable

namespace
{
	void fg_IocpTracePrint(CIocpTraceEntry const &_Entry)
	{
		int64 Frequency = NTime::CSystem_Time::fs_TimerFrequency();
		int64 Seconds = _Entry.m_Ticks / Frequency;
		int64 Microseconds = ((_Entry.m_Ticks % Frequency) * 1000000) / Frequency;

		NSys::fg_ConsoleErrorOutput
			(
				NStr::fg_Format<NStr::CStrNonTracked>
				(
					"[iocp {}.{}] {} socket=0x{} handle={} value=0x{} thread={}\n"
					, NStr::fg_FormatMinLength<5>(NStr::fg_FormatIntFormat<10>(Seconds))
					, NStr::fg_FormatMinLength<6>(NStr::fg_FormatFillOut<'0'>(NStr::fg_FormatIntFormat<10>(Microseconds)))
					, _Entry.m_pWhat
					, NStr::fg_FormatIntFormat<16>((umint)_Entry.m_pToken)
					, _Entry.m_Handle
					, NStr::fg_FormatIntFormat<16>(_Entry.m_Value)
					, _Entry.m_ThreadId
				)
			)
		;
	}
}

static void fg_DumpIocpTraceRing()
{
	auto &Io = fg_IoSubSystem_Windows();
	uint64 nTotal = Io.m_nTraceNext.f_Load(NAtomic::gc_MemoryOrder_Acquire);
	uint64 iFirst = nTotal > gc_IocpTraceRingEntries ? nTotal - gc_IocpTraceRingEntries : 0;
	for (uint64 iEntry = iFirst; iEntry < nTotal; ++iEntry)
		fg_IocpTracePrint(Io.m_pTraceRing[iEntry % gc_IocpTraceRingEntries]);
}

void CIoSubSystem_Windows::f_Trace(char const *_pWhat, void const *_pToken, NSys::CIoLoopHandle _Handle, uint32 _Value)
{
	CIocpTraceEntry Entry{NTime::CSystem_Time::fs_GetTimerValue(), _pWhat, _pToken, _Handle, _Value, GetCurrentThreadId()};

	if (m_TraceMode == 2)
	{
		uint64 iEntry = m_nTraceNext.f_FetchAdd(1, NAtomic::gc_MemoryOrder_AcquireRelease);
		m_pTraceRing[iEntry % gc_IocpTraceRingEntries] = Entry;
		return;
	}

	fg_IocpTracePrint(Entry);
}

bool fg_IocpTraceEnabled()
{
	return fg_IoSubSystem_Windows().f_TraceEnabled();
}

void fg_IocpTrace(char const *_pWhat, void const *_pToken, NSys::CIoLoopHandle _Handle, uint32 _Value)
{
	fg_IoSubSystem_Windows().f_Trace(_pWhat, _pToken, _Handle, _Value);
}

#endif
