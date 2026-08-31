// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

// SIO_TCP_INFO and its record, as the SDK declares them for Windows 10 1703 and later, spelled
// out here because the SDK hides them behind a target version this build does not set. The
// query fails on earlier systems and the path is then simply not asked
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

// Windows reports the bytes sent so far; the delivery rate is what went out between two
// readings, so the first only takes its bearings — the caller carries them. The latency the
// consumer multiplies the rate with is measured at the releases themselves, not asked of the
// path. Whether the sender held the rate back is not reported, and the query is made while
// the sender is bound, so it is taken as held back by the sender rather than the path
inline bool fg_Windows_QueryPathDeliveryRate(SOCKET _Socket, uint64 &io_LastBytesOut, uint64 &io_LastStamp, umint &o_nBytesPerSecond, bool &o_bAppLimited)
{
	DWORD Version = 0;
	CWindowsTcpInfoV0 Info;
	DWORD nBytes = 0;
	if (WSAIoctl(_Socket, SIO_TCP_INFO, &Version, sizeof(Version), &Info, sizeof(Info), &nBytes, nullptr, nullptr) != 0)
		return false;

	uint64 Stamp = (uint64)NMib::NTime::CSystem_Time::fs_GetTimerValue();
	uint64 Frequency = (uint64)NMib::NTime::CSystem_Time::fs_TimerFrequency();
	uint64 LastBytesOut = io_LastBytesOut;
	uint64 LastStamp = io_LastStamp;
	io_LastBytesOut = Info.m_BytesOut;
	io_LastStamp = Stamp;

	if (!LastStamp || Stamp <= LastStamp || Info.m_BytesOut <= LastBytesOut)
		return false;

	o_nBytesPerSecond = umint((Info.m_BytesOut - LastBytesOut) * Frequency / (Stamp - LastStamp));
	o_bAppLimited = true;

	return true;
}
