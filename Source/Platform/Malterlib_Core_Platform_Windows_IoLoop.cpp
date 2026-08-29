// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Windows_IoLoop_Iocp_Internal.h"

using namespace NMib;
using namespace NMib::NMemory;
using namespace NMib::NSys;

namespace
{
#if DMibConfig_IoDebug_Enable
	bool fg_IocpEnvFlag(char const *_pName, bool _bDefault)
	{
		auto Setting = NMib::NSys::fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStrNonTracked(_pName));
		if (Setting == "0")
			return false;
		if (Setting == "1")
			return true;

		return _bDefault;
	}

	umint fg_IocpEnvCount(char const *_pName, umint _Default, umint _Min, umint _Max)
	{
		auto Setting = NMib::NSys::fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStrNonTracked(_pName));

		umint nValue = Setting.f_ToIntExact(umint(0));
		if (nValue)
			return fg_Clamp(nValue, _Min, _Max);

		return _Default;
	}

	// MalterlibIocpTrace=1 prints every trace line as it happens; =2 records them into a ring
	// and prints the ring at exit, so timing-sensitive failures are not disturbed by the console
	struct CIocpTraceEntry
	{
		int64 m_Ticks;
		char const *m_pWhat;
		void const *m_pToken;
		NSys::CIoLoopHandle m_Handle;
		uint32 m_Value;
		uint32 m_ThreadId;
	};

	constexpr umint gc_IocpTraceRingEntries = 1 << 16;
#endif

	// Everything answered once for the process: the native entry points and the io debugging
	// knobs. Constant-initialized and filled once under an atomic guard on first use — function-
	// local statics are not an option, since the build disables their thread-safe
	// initialization and two loop threads racing the first call would read a zero-initialized
	// value — and trivially destructible, so the loops and sockets torn down after static
	// destruction can still read it
	struct CIocpConfig
	{
		void f_Load()
		{
			HMODULE hNtDll = GetModuleHandleW(L"ntdll.dll");
			if (hNtDll)
			{
				(FARPROC &)m_NtFunctions.m_fNtCreateFile = GetProcAddress(hNtDll, "NtCreateFile");
				(FARPROC &)m_NtFunctions.m_fNtDeviceIoControlFile = GetProcAddress(hNtDll, "NtDeviceIoControlFile");
				(FARPROC &)m_NtFunctions.m_fNtSetInformationFile = GetProcAddress(hNtDll, "NtSetInformationFile");
				(FARPROC &)m_NtFunctions.m_fRtlNtStatusToDosError = GetProcAddress(hNtDll, "RtlNtStatusToDosError");
			}

#if DMibConfig_IoDebug_Enable
			m_bCompletionEnabled = fg_IocpEnvFlag("MalterlibIocpCompletion", true);
			m_bSkipSuccessEnabled = fg_IocpEnvFlag("MalterlibIocpSkipSuccess", false);
			m_bLoopbackFastPathEnabled = fg_IocpEnvFlag("MalterlibLoopbackFastPath", true);
			m_nSendDepth = fg_IocpEnvCount("MalterlibIocpSendDepth", gc_IocpDefaultSendDepth, 1, gc_IocpMaxSendDepth);
			m_nRecvDepth = fg_IocpEnvCount("MalterlibIocpRecvDepth", gc_IocpDefaultRecvDepth, 1, gc_IocpMaxRecvDepth);
			m_nRecvBufferBytesOverride = fg_IocpEnvCount("MalterlibIocpRecvBufferBytes", 0, 4096, umint(1) << 30);
			m_nSocketBufferBytesOverride = fg_IocpEnvCount("MalterlibSocketBufferSize", 0, 1, umint(1) << 30);
			m_nSocketSendBufferBytesOverride = fg_IocpEnvCount("MalterlibSocketSendBufferSize", umint(-1), 0, umint(1) << 30);

			auto TraceSetting = NMib::NSys::fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStrNonTracked("MalterlibIocpTrace"));
			if (TraceSetting == "1")
				m_TraceMode = 1;
			else if (TraceSetting == "2")
			{
				m_TraceMode = 2;
				m_pTraceRing = (CIocpTraceEntry *)NMemory::CAllocator_NonTrackedHeap::f_Alloc(sizeof(CIocpTraceEntry) * gc_IocpTraceRingEntries);
				atexit(&fsg_DumpTraceRing);
			}

			if (NMib::NSys::fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStrNonTracked("MalterlibIoStats")) == "1")
			{
				m_bStatsEnabled = true;
				atexit(&fsg_DumpStats);
			}
#endif
		}

		static void fsg_DumpTraceRing();
		static void fsg_DumpStats();

		CIocpNtFunctions m_NtFunctions;

#if DMibConfig_IoDebug_Enable
		bool m_bCompletionEnabled = true;
		bool m_bSkipSuccessEnabled = false;
		bool m_bLoopbackFastPathEnabled = true;
		bool m_bStatsEnabled = false;
		int m_TraceMode = 0;
		umint m_nSendDepth = gc_IocpDefaultSendDepth;
		umint m_nRecvDepth = gc_IocpDefaultRecvDepth;
		umint m_nRecvBufferBytesOverride = 0;
		umint m_nSocketBufferBytesOverride = 0;
		umint m_nSocketSendBufferBytesOverride = umint(-1);
		CIocpTraceEntry *m_pTraceRing = nullptr;
		NAtomic::TCAtomic<uint64> m_nTraceNext{0};
#endif
	};

	constinit CIocpConfig g_IocpConfig;

	// 0 = not loaded, 1 = a thread is loading, 2 = loaded
	constinit NAtomic::TCAtomic<uint32> g_IocpConfigState{0};

	CIocpConfig &fg_IocpConfig()
	{
		uint32 State = g_IocpConfigState.f_Load(NAtomic::gc_MemoryOrder_Acquire);
		if (State == 2) [[likely]]
			return g_IocpConfig;

		uint32 Expected = 0;
		if (State == 0 && g_IocpConfigState.f_CompareExchangeStrong(Expected, 1, NAtomic::gc_MemoryOrder_AcquireRelease, NAtomic::gc_MemoryOrder_Acquire))
		{
			g_IocpConfig.f_Load();
			g_IocpConfigState.f_Store(2, NAtomic::gc_MemoryOrder_Release);
			return g_IocpConfig;
		}

		while (g_IocpConfigState.f_Load(NAtomic::gc_MemoryOrder_Acquire) != 2)
			NSys::fg_Thread_Yield();

		return g_IocpConfig;
	}
}

CIocpNtFunctions const &fg_IocpNtFunctions()
{
	return fg_IocpConfig().m_NtFunctions;
}

#if DMibConfig_IoDebug_Enable
bool fg_IocpCompletionEnabled()
{
	return fg_IocpConfig().m_bCompletionEnabled;
}

bool fg_IocpSkipSuccessEnabled()
{
	return fg_IocpConfig().m_bSkipSuccessEnabled;
}

bool fg_IocpLoopbackFastPathEnabled()
{
	return fg_IocpConfig().m_bLoopbackFastPathEnabled;
}

umint fg_IocpSendDepth()
{
	return fg_IocpConfig().m_nSendDepth;
}

umint fg_IocpRecvDepth()
{
	return fg_IocpConfig().m_nRecvDepth;
}

umint fg_IocpRecvBufferBytesOverride()
{
	return fg_IocpConfig().m_nRecvBufferBytesOverride;
}

umint fg_IocpSocketBufferBytesOverride()
{
	return fg_IocpConfig().m_nSocketBufferBytesOverride;
}

// -1 leaves the socket's send buffer alone; 0 makes AFD transmit straight from the locked user
// pages of an overlapped send instead of copying them into its own buffer first
umint fg_IocpSocketSendBufferBytesOverride()
{
	return fg_IocpConfig().m_nSocketSendBufferBytesOverride;
}

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

void CIocpConfig::fsg_DumpTraceRing()
{
	auto &Config = fg_IocpConfig();
	uint64 nTotal = Config.m_nTraceNext.f_Load(NAtomic::gc_MemoryOrder_Acquire);
	uint64 iFirst = nTotal > gc_IocpTraceRingEntries ? nTotal - gc_IocpTraceRingEntries : 0;
	for (uint64 iEntry = iFirst; iEntry < nTotal; ++iEntry)
		fg_IocpTracePrint(Config.m_pTraceRing[iEntry % gc_IocpTraceRingEntries]);
}

bool fg_IocpTraceEnabled()
{
	return fg_IocpConfig().m_TraceMode != 0;
}

void fg_IocpTrace(char const *_pWhat, void const *_pToken, NSys::CIoLoopHandle _Handle, uint32 _Value)
{
	CIocpTraceEntry Entry{NTime::CSystem_Time::fs_GetTimerValue(), _pWhat, _pToken, _Handle, _Value, GetCurrentThreadId()};

	auto &Config = fg_IocpConfig();
	if (Config.m_TraceMode == 2)
	{
		uint64 iEntry = Config.m_nTraceNext.f_FetchAdd(1, NAtomic::gc_MemoryOrder_AcquireRelease);
		Config.m_pTraceRing[iEntry % gc_IocpTraceRingEntries] = Entry;
		return;
	}

	fg_IocpTracePrint(Entry);
}

CIocpStats g_IocpStats;

uint64 fg_IocpStatsNow()
{
	int64 Ticks = NTime::CSystem_Time::fs_GetTimerValue();
	int64 Frequency = NTime::CSystem_Time::fs_TimerFrequency();
	return (uint64)((Ticks / Frequency) * 1000000000 + ((Ticks % Frequency) * 1000000000) / Frequency);
}

void CIocpConfig::fsg_DumpStats()
{
	auto fLoad = [](NAtomic::TCAtomic<uint64> const &_Value) -> uint64
		{
			return _Value.f_Load(NAtomic::gc_MemoryOrder_Relaxed);
		}
	;

	uint64 nSegments = fLoad(g_IocpStats.m_nRecvSegments);
	uint64 nBytes = fLoad(g_IocpStats.m_nRecvBytes);

	NSys::fg_ConsoleErrorOutput
		(
			NStr::fg_Format<NStr::CStrNonTracked>
			(
				"[io stats] recv: segments={} bytes={} avg={} allocs={} reuses={} posts={} inline={} parks={} resumes={} errors={}\n"
				, nSegments
				, nBytes
				, nSegments ? nBytes / nSegments : 0
				, fLoad(g_IocpStats.m_nRecvBufferAllocs)
				, fLoad(g_IocpStats.m_nRecvBufferReuses)
				, fLoad(g_IocpStats.m_nRecvPosts)
				, fLoad(g_IocpStats.m_nRecvInline)
				, fLoad(g_IocpStats.m_nStreamParks)
				, fLoad(g_IocpStats.m_nStreamResumes)
				, fLoad(g_IocpStats.m_nRecvErrors)
			)
		)
	;

	for (umint iBucket = 0; iBucket < 33; ++iBucket)
	{
		uint64 nCount = fLoad(g_IocpStats.m_RecvSizeBuckets[iBucket]);
		if (!nCount)
			continue;

		NSys::fg_ConsoleErrorOutput(NStr::fg_Format<NStr::CStrNonTracked>("[io stats] recv size 2^{}: {}\n", iBucket, nCount));
	}

	uint64 nSendRequested = fLoad(g_IocpStats.m_nSendBytesRequested);
	uint64 nSendOps = fLoad(g_IocpStats.m_nSendOps);

	NSys::fg_ConsoleErrorOutput
		(
			NStr::fg_Format<NStr::CStrNonTracked>
			(
				"[io stats] send: ops={} inline={} deferred={} short={} bytesReq={} bytesSent={} avgReq={} errors={}\n"
				, nSendOps
				, fLoad(g_IocpStats.m_nSendInline)
				, fLoad(g_IocpStats.m_nSendDeferred)
				, fLoad(g_IocpStats.m_nSendShort)
				, nSendRequested
				, fLoad(g_IocpStats.m_nSendBytesSent)
				, nSendOps ? nSendRequested / nSendOps : 0
				, fLoad(g_IocpStats.m_nSendErrors)
			)
		)
	;

	for (umint iBucket = 0; iBucket < 33; ++iBucket)
	{
		uint64 nCount = fLoad(g_IocpStats.m_SendSizeBuckets[iBucket]);
		if (!nCount)
			continue;

		NSys::fg_ConsoleErrorOutput(NStr::fg_Format<NStr::CStrNonTracked>("[io stats] send size 2^{}: {}\n", iBucket, nCount));
	}

	uint64 nIdleGaps = fLoad(g_IocpStats.m_nSendIdleGaps);
	uint64 nLagOps = fLoad(g_IocpStats.m_nSendSubmitLagOps);

	NSys::fg_ConsoleErrorOutput
		(
			NStr::fg_Format<NStr::CStrNonTracked>
			(
				"[io stats] send idle: gaps={} totalNs={} avgNs={} submitLagOps={} submitLagNs={} submitLagAvgNs={}\n"
				, nIdleGaps
				, fLoad(g_IocpStats.m_nSendIdleNs)
				, nIdleGaps ? fLoad(g_IocpStats.m_nSendIdleNs) / nIdleGaps : 0
				, nLagOps
				, fLoad(g_IocpStats.m_nSendSubmitLagNs)
				, nLagOps ? fLoad(g_IocpStats.m_nSendSubmitLagNs) / nLagOps : 0
			)
		)
	;

	NSys::fg_ConsoleErrorOutput
		(
			NStr::fg_Format<NStr::CStrNonTracked>
			(
				"[io stats] loop: registrations={} skipSuccess={} pollArms={} pollCancels={} pollEvents={} waits={} packets={} wakePosts={}\n"
				, fLoad(g_IocpStats.m_nRegistrations)
				, fLoad(g_IocpStats.m_nSkipSuccessSockets)
				, fLoad(g_IocpStats.m_nPollArms)
				, fLoad(g_IocpStats.m_nPollCancels)
				, fLoad(g_IocpStats.m_nPollEvents)
				, fLoad(g_IocpStats.m_nWaits)
				, fLoad(g_IocpStats.m_nPackets)
				, fLoad(g_IocpStats.m_nWakePosts)
			)
		)
	;
}

bool fg_IocpStatsEnabled()
{
	return fg_IocpConfig().m_bStatsEnabled;
}
#else
void CIocpConfig::fsg_DumpTraceRing()
{
}

void CIocpConfig::fsg_DumpStats()
{
}
#endif

// Newest first: the most recently returned block is the hottest
uint8 *CIocpBufferRecycler::f_TryTake()
{
	DMibLock(m_Lock);
	if (m_FreeBlocks.f_IsEmpty())
		return nullptr;

	return m_FreeBlocks.f_Pop();
}

CIocpBufferRecycler::~CIocpBufferRecycler()
{
	for (uint8 *pBlock : m_FreeBlocks)
		NMemory::CDefaultAllocator::f_Free(pBlock, m_nBufferBytes);
}

// Returns the block to the stack, or refuses — full or dead — and the caller frees
bool CIocpBufferRecycler::f_TryReturn(uint8 *_pBlock)
{
	DMibLock(m_Lock);
	if (m_bDead || m_FreeBlocks.f_GetLen() >= m_nMaxFree)
		return false;

	m_FreeBlocks.f_InsertLast(_pBlock);

	return true;
}

void CIocpBufferRecycler::f_Die()
{
	NContainer::TCVector<uint8 *> Blocks;
	{
		DMibLock(m_Lock);
		m_bDead = true;
		Blocks = fg_Move(m_FreeBlocks);
	}

	for (uint8 *pBlock : Blocks)
		NMemory::CDefaultAllocator::f_Free(pBlock, m_nBufferBytes);
}

CIocpStreamBuffer::~CIocpStreamBuffer()
{
	// The block goes back to its stream's recycle stack while the stream wants it — the next
	// post reuses it cache-hot — and to the allocator once the stack is full or the stream is
	// gone. An exact sized raw allocation, headerless on purpose
	if (m_pData)
	{
		if (!m_pRecycler || !m_pRecycler->f_TryReturn(m_pData))
			NMemory::CDefaultAllocator::f_Free(m_pData, m_nDataBytes);
	}

	if (!m_pBackpressure)
		return;

	auto &Backpressure = *m_pBackpressure;
	umint Previous = Backpressure.m_nOutstandingBytes.f_FetchSub(m_nCharged, NAtomic::gc_MemoryOrder_AcquireRelease);
	umint Now = Previous - m_nCharged;

	// The release that crosses the resume threshold while the stream is parked claims the
	// flag — the exchange dedups concurrent releases to a single resume — and reschedules
	// through the stream's owner. Lock free, and safe on any thread at any time: the functor
	// is immutable once the stream has started and reaches its owner through a weak
	// reference of its own
	if (Now <= Backpressure.m_nResumeBytes && Backpressure.m_bParked.f_Load(NAtomic::gc_MemoryOrder_Acquire))
	{
		if (Backpressure.m_bParked.f_Exchange(0, NAtomic::gc_MemoryOrder_AcquireRelease))
		{
			if (Backpressure.m_fResume)
				Backpressure.m_fResume();
		}
	}
}

NMib::NSys::ICIoLoop *fg_CreatePlatformIoLoop()
{
	if (!fg_IocpNtFunctions().f_IsComplete())
		return nullptr;

	auto *pLoop = fg_ConstructObject<CIoLoop_Iocp>(CAllocator_NonTrackedHeap());
	if (pLoop->f_IsCreated())
		return pLoop;

	fg_DeleteObject(CAllocator_NonTrackedHeap(), pLoop);

	return nullptr;
}
