// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Time/Stopwatch>

// SDK target-version guards may hide the Windows 10 1703 TCP-info ABI; older kernels reject the query.
#if !defined(SIO_TCP_INFO)
	#define SIO_TCP_INFO _WSAIORW(IOC_VENDOR, 39)
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

// Derive delivery rate from successive cumulative byte samples. Below both congestion and receive windows,
// the sender is application-limited; that rate must not shrink the send window.
inline bool fg_Windows_QueryPathDeliveryRate(SOCKET _Socket, uint64 &io_LastBytesOut, uint64 &io_LastStamp, umint &o_nBytesPerSecond, bool &o_bAppLimited)
{
	DWORD Version = 0;
	CWindowsTcpInfoV0 Info;
	DWORD nBytes = 0;
	if (WSAIoctl(_Socket, SIO_TCP_INFO, &Version, sizeof(Version), &Info, sizeof(Info), &nBytes, nullptr, nullptr) != 0)
		return false;

	uint64 Stamp = uint64(NMib::NTime::NPlatform::fg_TimerRaw_PreciseGet());
	uint64 Frequency = uint64(NMib::NTime::NPlatform::fg_TimerRaw_PreciseFrequency());
	uint64 LastBytesOut = io_LastBytesOut;
	uint64 LastStamp = io_LastStamp;
	io_LastBytesOut = Info.m_BytesOut;
	io_LastStamp = Stamp;

	if (!LastStamp || Stamp <= LastStamp || Info.m_BytesOut <= LastBytesOut)
		return false;

	o_nBytesPerSecond = umint((Info.m_BytesOut - LastBytesOut) * Frequency / (Stamp - LastStamp));

	ULONG nWireWindow = Info.m_Cwnd < Info.m_SndWnd ? Info.m_Cwnd : Info.m_SndWnd;
	o_bAppLimited = Info.m_BytesInFlight + Info.m_Mss < nWireWindow;

	return true;
}
