// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Time/Stopwatch>

// SDK target-version guards may hide the Windows 10 1703 TCP-info ABI; older kernels reject the query.
#if !defined(SIO_TCP_INFO)
#	define SIO_TCP_INFO _WSAIORW(IOC_VENDOR, 39) // NOLINT(malterlib-macro-naming)
#endif

struct CWindowsTcpInfoV0
{
	DWORD m_State;
	ULONG m_Mss;
	ULONG64 m_ConnectionTimeMs;
	BOOLEAN m_bTimestampsEnabled;
	ULONG m_RttUs;
	ULONG m_MinRttUs;
	ULONG m_BytesInFlight;
	ULONG m_Cwnd;
	ULONG m_SndWnd;
	ULONG m_RcvWnd;
	ULONG m_RcvBuf;
	ULONG64 m_BytesOut;
	ULONG64 m_BytesIn;
	ULONG m_BytesReordered;
	ULONG m_BytesRetrans;
	ULONG m_FastRetrans;
	ULONG m_DupAcksIn;
	ULONG m_TimeoutEpisodes;
	UCHAR m_SynRetrans;
};

bool fg_Windows_QueryPathDeliveryRate(SOCKET _Socket, uint64 &o_LastBytesOut, uint64 &o_LastStamp, umint &o_nBytesPerSecond, bool &o_bAppLimited);
