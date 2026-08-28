// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Linux_IoSubSystem.h"

using namespace NMib;

#if DMibConfig_IoDebug_Enable

void fg_DumpUringStats(NMib::NSys::CIoSubSystem &_Io)
{
	auto &Stats = static_cast<CIoSubSystem_Linux &>(_Io).m_UringStats;

	auto fLoad = [](NAtomic::TCAtomic<uint64> const &_Value) -> uint64
		{
			return _Value.f_Load(NAtomic::gc_MemoryOrder_Relaxed);
		}
	;

	uint64 nSegments = fLoad(Stats.m_nRecvSegments);
	uint64 nBytes = fLoad(Stats.m_nRecvBytes);

	NSys::fg_ConsoleErrorOutput
		(
			NStr::fg_Format<NStr::CStrNonTracked>
			(
				"[io stats] recv: segments={} bytes={} avg={} allocs={} reuses={} allocBytes={} arms={} enobufs={} parks={} resumes={}\n"
				, nSegments
				, nBytes
				, nSegments ? nBytes / nSegments : 0
				, fLoad(Stats.m_nRecvBufferAllocs)
				, fLoad(Stats.m_nRecvBufferReuses)
				, fLoad(Stats.m_nRecvBufferAllocBytes)
				, fLoad(Stats.m_nStreamArms)
				, fLoad(Stats.m_nStreamEnobufs)
				, fLoad(Stats.m_nStreamParks)
				, fLoad(Stats.m_nStreamResumes)
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
					"[io stats] recv size 2^{}: {}\n"
					, iBucket
					, nCount
				)
			)
		;
	}

	uint64 nSendRequested = fLoad(Stats.m_nSendBytesRequested);
	uint64 nSendOps = fLoad(Stats.m_nSendOps);

	NSys::fg_ConsoleErrorOutput
		(
			NStr::fg_Format<NStr::CStrNonTracked>
			(
				"[io stats] send: ops={} publishes={} zc={} short={} bytesReq={} bytesSent={} avgReq={} notifs={} maxInFlight={} maxBytesInFlight={} notifLagAvgUs={} notifLagMaxUs={}\n"
				, nSendOps
				, fLoad(Stats.m_nSendPublishes)
				, fLoad(Stats.m_nSendZcOps)
				, fLoad(Stats.m_nSendShort)
				, nSendRequested
				, fLoad(Stats.m_nSendBytesSent)
				, nSendOps ? nSendRequested / nSendOps : 0
				, fLoad(Stats.m_nSendNotifs)
				, fLoad(Stats.m_nSendMaxInFlight)
				, fLoad(Stats.m_nSendMaxBytesInFlight)
				, fLoad(Stats.m_nSendNotifLagOps) ? fLoad(Stats.m_nSendNotifLagNs) / fLoad(Stats.m_nSendNotifLagOps) / 1000 : 0
				, fLoad(Stats.m_nSendNotifLagMaxNs) / 1000
			)
		)
	;

	for (umint iBucket = 0; iBucket < 33; ++iBucket)
	{
		uint64 nPlain = fLoad(Stats.m_SendSizeBuckets[iBucket]);
		uint64 nZc = fLoad(Stats.m_SendZcSizeBuckets[iBucket]);
		if (!nPlain && !nZc)
			continue;

		NSys::fg_ConsoleErrorOutput
			(
				NStr::fg_Format<NStr::CStrNonTracked>
				(
					"[io stats] send size 2^{}: plain={} zc={}\n"
					, iBucket
					, nPlain
					, nZc
				)
			)
		;
	}

	uint64 nIdleGaps = fLoad(Stats.m_nSendIdleGaps);

	NSys::fg_ConsoleErrorOutput
		(
			NStr::fg_Format<NStr::CStrNonTracked>
			(
				"[io stats] send idle: gaps={} totalNs={} avgNs={} submitLagOps={} submitLagNs={} submitLagAvgNs={}\n"
				, nIdleGaps
				, fLoad(Stats.m_nSendIdleNs)
				, nIdleGaps ? fLoad(Stats.m_nSendIdleNs) / nIdleGaps : 0
				, fLoad(Stats.m_nSendSubmitLagOps)
				, fLoad(Stats.m_nSendSubmitLagNs)
				, fLoad(Stats.m_nSendSubmitLagOps) ? fLoad(Stats.m_nSendSubmitLagNs) / fLoad(Stats.m_nSendSubmitLagOps) : 0
			)
		)
	;

	NSys::fg_ConsoleErrorOutput
		(
			NStr::fg_Format<NStr::CStrNonTracked>
			(
				"[io stats] errors: send={} recv={}\n"
				, fLoad(Stats.m_nSendErrors)
				, fLoad(Stats.m_nRecvErrors)
			)
		)
	;}

#endif
