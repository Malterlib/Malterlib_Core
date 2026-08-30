// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Windows_IoLoop_Iocp_Internal.h"

using namespace NMib;
using namespace NMib::NSys;

#if DMibConfig_IoDebug_Enable

void fg_DumpIocpStats(NMib::NSys::CIoSubSystem &_Io)
{
	auto &Stats = static_cast<CIoSubSystem_Windows &>(_Io).m_IocpStats;

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
				"[io stats] recv: segments={} bytes={} avg={} allocs={} reuses={} posts={} inline={} parks={} resumes={} errors={}\n"
				, nSegments
				, nBytes
				, nSegments ? nBytes / nSegments : 0
				, fLoad(Stats.m_nRecvBufferAllocs)
				, fLoad(Stats.m_nRecvBufferReuses)
				, fLoad(Stats.m_nRecvPosts)
				, fLoad(Stats.m_nRecvInline)
				, fLoad(Stats.m_nStreamParks)
				, fLoad(Stats.m_nStreamResumes)
				, fLoad(Stats.m_nRecvErrors)
			)
		)
	;

	for (umint iBucket = 0; iBucket < 33; ++iBucket)
	{
		uint64 nCount = fLoad(Stats.m_RecvSizeBuckets[iBucket]);
		if (!nCount)
			continue;

		NSys::fg_ConsoleErrorOutput(NStr::fg_Format<NStr::CStrNonTracked>("[io stats] recv size 2^{}: {}\n", iBucket, nCount));
	}

	uint64 nSendRequested = fLoad(Stats.m_nSendBytesRequested);
	uint64 nSendOps = fLoad(Stats.m_nSendOps);

	NSys::fg_ConsoleErrorOutput
		(
			NStr::fg_Format<NStr::CStrNonTracked>
			(
				"[io stats] send: ops={} pendingAtIssue={} inline={} deferred={} maxInFlight={} maxBytesInFlight={} short={} bytesReq={} bytesSent={} avgReq={} errors={}\n"
				, nSendOps
				, fLoad(Stats.m_nSendPendingAtIssue)
				, fLoad(Stats.m_nSendInline)
				, fLoad(Stats.m_nSendDeferred)
				, fLoad(Stats.m_nSendMaxInFlight)
				, fLoad(Stats.m_nSendMaxBytesInFlight)
				, fLoad(Stats.m_nSendShort)
				, nSendRequested
				, fLoad(Stats.m_nSendBytesSent)
				, nSendOps ? nSendRequested / nSendOps : 0
				, fLoad(Stats.m_nSendErrors)
			)
		)
	;

	for (umint iBucket = 0; iBucket < 33; ++iBucket)
	{
		uint64 nCount = fLoad(Stats.m_SendSizeBuckets[iBucket]);
		if (!nCount)
			continue;

		NSys::fg_ConsoleErrorOutput(NStr::fg_Format<NStr::CStrNonTracked>("[io stats] send size 2^{}: {}\n", iBucket, nCount));
	}

	uint64 nIdleGaps = fLoad(Stats.m_nSendIdleGaps);
	uint64 nLagOps = fLoad(Stats.m_nSendSubmitLagOps);
	uint64 nSyncOps = fLoad(Stats.m_nSendPacketLagSyncOps);
	uint64 nPendingOps = fLoad(Stats.m_nSendPacketLagPendingOps);

	NSys::fg_ConsoleErrorOutput
		(
			NStr::fg_Format<NStr::CStrNonTracked>
			(
				"[io stats] send idle: gaps={} totalNs={} avgNs={} submitLagOps={} submitLagNs={} submitLagAvgNs={} packetLagSyncOps={} packetLagSyncAvgNs={} packetLagPendingOps={} packetLagPendingAvgNs={}\n"
				, nIdleGaps
				, fLoad(Stats.m_nSendIdleNs)
				, nIdleGaps ? fLoad(Stats.m_nSendIdleNs) / nIdleGaps : 0
				, nLagOps
				, fLoad(Stats.m_nSendSubmitLagNs)
				, nLagOps ? fLoad(Stats.m_nSendSubmitLagNs) / nLagOps : 0
				, nSyncOps
				, nSyncOps ? fLoad(Stats.m_nSendPacketLagSyncNs) / nSyncOps : 0
				, nPendingOps
				, nPendingOps ? fLoad(Stats.m_nSendPacketLagPendingNs) / nPendingOps : 0
			)
		)
	;

	NSys::fg_ConsoleErrorOutput
		(
			NStr::fg_Format<NStr::CStrNonTracked>
			(
				"[io stats] loop: registrations={} skipSuccess={} pollArms={} pollCancels={} pollEvents={} waits={} packets={} wakePosts={}\n"
				, fLoad(Stats.m_nRegistrations)
				, fLoad(Stats.m_nSkipSuccessSockets)
				, fLoad(Stats.m_nPollArms)
				, fLoad(Stats.m_nPollCancels)
				, fLoad(Stats.m_nPollEvents)
				, fLoad(Stats.m_nWaits)
				, fLoad(Stats.m_nPackets)
				, fLoad(Stats.m_nWakePosts)
			)
		)
	;}

#endif
