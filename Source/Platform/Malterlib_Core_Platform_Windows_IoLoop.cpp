// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Windows_IoLoop_Iocp_Internal.h"
#include "Malterlib_Core_Platform_Windows_Optional.h"

using namespace NMib;
using namespace NMib::NMemory;
using namespace NMib::NSys;

#if DMibConfig_IoDebug_Enable
static void fg_DumpIocpTraceRing();
static void fg_DumpIocpStats();
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

// A view of the entry points COptionalFunctions resolved at platform start
CIocpNtFunctions fg_IocpNtFunctions()
{
	auto const &Functions = NLocal::g_OptionalFunctions;

	CIocpNtFunctions Nt;
	Nt.m_fNtCreateFile = Functions.m_fNtCreateFile;
	Nt.m_fNtDeviceIoControlFile = Functions.m_fNtDeviceIoControlFile;
	Nt.m_fNtSetInformationFile = Functions.m_fNtSetInformationFile;
	Nt.m_fRtlNtStatusToDosError = Functions.m_fRtlNtStatusToDosError;

	return Nt;
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

bool fg_IocpTraceEnabled()
{
	return fg_IoSubSystem_Windows().f_TraceEnabled();
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

void fg_IocpTrace(char const *_pWhat, void const *_pToken, NSys::CIoLoopHandle _Handle, uint32 _Value)
{
	fg_IoSubSystem_Windows().f_Trace(_pWhat, _pToken, _Handle, _Value);
}

CIocpStats g_IocpStats;

uint64 fg_IocpStatsNow()
{
	int64 Ticks = NTime::CSystem_Time::fs_GetTimerValue();
	int64 Frequency = NTime::CSystem_Time::fs_TimerFrequency();
	return (uint64)((Ticks / Frequency) * 1000000000 + ((Ticks % Frequency) * 1000000000) / Frequency);
}

static void fg_DumpIocpStats()
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
				"[io stats] send: ops={} pendingAtIssue={} inline={} deferred={} maxInFlight={} maxBytesInFlight={} short={} bytesReq={} bytesSent={} avgReq={} errors={}\n"
				, nSendOps
				, fLoad(g_IocpStats.m_nSendPendingAtIssue)
				, fLoad(g_IocpStats.m_nSendInline)
				, fLoad(g_IocpStats.m_nSendDeferred)
				, fLoad(g_IocpStats.m_nSendMaxInFlight)
				, fLoad(g_IocpStats.m_nSendMaxBytesInFlight)
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
	uint64 nSyncOps = fLoad(g_IocpStats.m_nSendPacketLagSyncOps);
	uint64 nPendingOps = fLoad(g_IocpStats.m_nSendPacketLagPendingOps);

	NSys::fg_ConsoleErrorOutput
		(
			NStr::fg_Format<NStr::CStrNonTracked>
			(
				"[io stats] send idle: gaps={} totalNs={} avgNs={} submitLagOps={} submitLagNs={} submitLagAvgNs={} packetLagSyncOps={} packetLagSyncAvgNs={} packetLagPendingOps={} packetLagPendingAvgNs={}\n"
				, nIdleGaps
				, fLoad(g_IocpStats.m_nSendIdleNs)
				, nIdleGaps ? fLoad(g_IocpStats.m_nSendIdleNs) / nIdleGaps : 0
				, nLagOps
				, fLoad(g_IocpStats.m_nSendSubmitLagNs)
				, nLagOps ? fLoad(g_IocpStats.m_nSendSubmitLagNs) / nLagOps : 0
				, nSyncOps
				, nSyncOps ? fLoad(g_IocpStats.m_nSendPacketLagSyncNs) / nSyncOps : 0
				, nPendingOps
				, nPendingOps ? fLoad(g_IocpStats.m_nSendPacketLagPendingNs) / nPendingOps : 0
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
	return fg_IoSubSystem_Windows().f_StatsEnabled();
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
