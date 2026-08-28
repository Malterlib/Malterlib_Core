// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Core/Platform>

#include "Malterlib_Core_IoSubSystem.h"

#if DMibConfig_IoDebug_Enable

using namespace NMib;

// The readiness-path socket report; every platform records into the base subsystem, so the
// report is shared too
void fg_DumpSocketIoStats(NMib::NSys::CIoSubSystem &_Io)
{
	auto &Stats = _Io.m_SocketIoStats;

	auto fLoad = [](NAtomic::TCAtomic<uint64> const &_Value) -> uint64
		{
			return _Value.f_Load(NAtomic::gc_MemoryOrder_Relaxed);
		}
	;

	uint64 nRecvCalls = fLoad(Stats.m_nRecvCalls);
	uint64 nRecvBytes = fLoad(Stats.m_nRecvBytes);

	NSys::fg_ConsoleErrorOutput
		(
			NStr::fg_Format<NStr::CStrNonTracked>
			(
				"[io stats] readiness recv: calls={} bytes={} avg={} wouldBlock={} short={} endOfStream={}\n"
				, nRecvCalls
				, nRecvBytes
				, nRecvCalls ? nRecvBytes / nRecvCalls : 0
				, fLoad(Stats.m_nRecvWouldBlock)
				, fLoad(Stats.m_nRecvShort)
				, fLoad(Stats.m_nRecvEndOfStream)
			)
		)
	;

	for (umint iBucket = 0; iBucket < 33; ++iBucket)
	{
		uint64 nCount = fLoad(Stats.m_RecvSizeBuckets[iBucket]);
		if (!nCount)
			continue;

		NSys::fg_ConsoleErrorOutput
			(
				NStr::fg_Format<NStr::CStrNonTracked>
				(
					"[io stats] readiness recv size 2^{}: {}\n"
					, iBucket
					, nCount
				)
			)
		;
	}

	uint64 nSendCalls = fLoad(Stats.m_nSendCalls);
	uint64 nSendRequested = fLoad(Stats.m_nSendBytesRequested);

	NSys::fg_ConsoleErrorOutput
		(
			NStr::fg_Format<NStr::CStrNonTracked>
			(
				"[io stats] readiness send: calls={} bytesReq={} bytesSent={} avgReq={} wouldBlock={} short={}\n"
				, nSendCalls
				, nSendRequested
				, fLoad(Stats.m_nSendBytesSent)
				, nSendCalls ? nSendRequested / nSendCalls : 0
				, fLoad(Stats.m_nSendWouldBlock)
				, fLoad(Stats.m_nSendShort)
			)
		)
	;

	for (umint iBucket = 0; iBucket < 33; ++iBucket)
	{
		uint64 nCount = fLoad(Stats.m_SendSizeBuckets[iBucket]);
		if (!nCount)
			continue;

		NSys::fg_ConsoleErrorOutput
			(
				NStr::fg_Format<NStr::CStrNonTracked>
				(
					"[io stats] readiness send size 2^{}: {}\n"
					, iBucket
					, nCount
				)
			)
		;
	}

	NSys::fg_ConsoleErrorOutput
		(
			NStr::fg_Format<NStr::CStrNonTracked>
			(
				"[io stats] readiness arms: read={} write={} reports: read={} write={}\n"
				, fLoad(Stats.m_nReadinessArmsRead)
				, fLoad(Stats.m_nReadinessArmsWrite)
				, fLoad(Stats.m_nReadinessReportsRead)
				, fLoad(Stats.m_nReadinessReportsWrite)
			)
		)
	;}

// The socket-actor level report; the Network module records the counters, the base subsystem
// carries them like the ssl and websocket knobs
void fg_DumpNetIoStats(NMib::NSys::CIoSubSystem &_Io)
{
	auto &Stats = _Io.m_NetIoStats;

	auto fLoad = [](NAtomic::TCAtomic<uint64> const &_Value) -> uint64
		{
			return _Value.f_Load(NAtomic::gc_MemoryOrder_Relaxed);
		}
	;

	NSys::fg_ConsoleErrorOutput
		(
			NStr::fg_Format<NStr::CStrNonTracked>
				(
					"[net stats] send: readiness={} readinessBytes={} submits={} blocked={} maxOutstanding={} syncParked={} continuations={}\n"
					, fLoad(Stats.m_nSendReadinessCalls)
					, fLoad(Stats.m_nSendReadinessBytes)
					, fLoad(Stats.m_nSendSubmits)
					, fLoad(Stats.m_nSendBlocked)
					, fLoad(Stats.m_nSendMaxOutstanding)
					, fLoad(Stats.m_nSendSyncParked)
					, fLoad(Stats.m_nSendContinuations)
				)
		)
	;

	uint64 nShared = fLoad(Stats.m_nRecvSharedDeliveries);
	uint64 nCopy = fLoad(Stats.m_nRecvCopyDeliveries);

	NSys::fg_ConsoleErrorOutput
		(
			NStr::fg_Format<NStr::CStrNonTracked>
				(
					"[net stats] recv: readiness={} readinessBytes={} shared={} sharedBytes={} copy={} copyBytes={} sslSegments={} sslNoProgress={} sslCompacts={}\n"
					, fLoad(Stats.m_nRecvReadinessCalls)
					, fLoad(Stats.m_nRecvReadinessBytes)
					, nShared
					, fLoad(Stats.m_nRecvSharedBytes)
					, nCopy
					, fLoad(Stats.m_nRecvCopyBytes)
					, fLoad(Stats.m_nSslSegments)
					, fLoad(Stats.m_nSslNoProgress)
					, fLoad(Stats.m_nSslCompacts)
				)
		)
	;

	NSys::fg_ConsoleErrorOutput
		(
			NStr::fg_Format<NStr::CStrNonTracked>
				(
					"[net stats] storage copies: range={} feed={} feedConst={} consume={}\n"
					, NStream::g_BinaryStorageRangeCopyBytes.f_Load(NAtomic::gc_MemoryOrder_Relaxed)
					, NStream::g_BinaryStorageFeedCopyBytes.f_Load(NAtomic::gc_MemoryOrder_Relaxed)
					, NStream::g_BinaryStorageFeedConstCopyBytes.f_Load(NAtomic::gc_MemoryOrder_Relaxed)
					, NStream::g_BinaryStorageConsumeCopyBytes.f_Load(NAtomic::gc_MemoryOrder_Relaxed)
				)
		)
	;

	NSys::fg_ConsoleErrorOutput
		(
			NStr::fg_Format<NStr::CStrNonTracked>
				(
					"[net stats] ssl pump: submits={} inFlight={} beginRefused={} kernelRefused={} lastRefusal: pending={} pinned={} canBegin={} ops={}/{}\n"
					, fLoad(Stats.m_nPumpSubmits)
					, fLoad(Stats.m_nPumpInFlight)
					, fLoad(Stats.m_nPumpBeginRefused)
					, fLoad(Stats.m_nPumpKernelRefused)
					, fLoad(Stats.m_LastPumpPending)
					, fLoad(Stats.m_LastPumpPinned)
					, fLoad(Stats.m_LastPumpCanBegin)
					, fLoad(Stats.m_LastPumpOpsUnresolved)
					, fLoad(Stats.m_LastPumpOpsInUse)
				)
		)
	;
	NSys::fg_ConsoleErrorOutput
		(
			NStr::fg_Format<NStr::CStrNonTracked>
				(
					"[net stats] ssl pins: max={} maxBytes={} cap: max={} bdp={} queries={}\n"
					, fLoad(Stats.m_nSslMaxPinned)
					, fLoad(Stats.m_nSslMaxPinnedBytes)
					, fLoad(Stats.m_nSslWindowMax)
					, fLoad(Stats.m_nSslWindowBandwidthDelay)
					, fLoad(Stats.m_nSslWindowQueries)
				)
		)
	;
}

#endif
