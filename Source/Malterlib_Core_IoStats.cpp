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

#endif
