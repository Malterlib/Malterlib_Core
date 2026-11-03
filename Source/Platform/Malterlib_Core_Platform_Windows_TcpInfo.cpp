// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// The IOCP loop headers bring in the Windows socket API that the TCP info header uses.
#include "Malterlib_Core_Platform_Windows_IoLoop_Iocp_Internal.h"
#include "Malterlib_Core_Platform_Windows_TcpInfo.h"

// Derive delivery rate from successive cumulative byte samples. Below both congestion and receive windows,
// the sender is application-limited; that rate must not shrink the send window.
bool fg_Windows_QueryPathDeliveryRate(SOCKET _Socket, uint64 &o_LastBytesOut, uint64 &o_LastStamp, umint &o_nBytesPerSecond, bool &o_bAppLimited)
{
	DWORD Version = 0;
	CWindowsTcpInfoV0 Info;
	DWORD nBytes = 0;
	if (WSAIoctl(_Socket, SIO_TCP_INFO, &Version, sizeof(Version), &Info, sizeof(Info), &nBytes, nullptr, nullptr) != 0)
		return false;

	uint64 Stamp = uint64(NMib::NTime::NPlatform::fg_TimerRaw_PreciseGet());
	uint64 Frequency = uint64(NMib::NTime::NPlatform::fg_TimerRaw_PreciseFrequency());
	uint64 LastBytesOut = o_LastBytesOut;
	uint64 LastStamp = o_LastStamp;
	o_LastBytesOut = Info.m_BytesOut;
	o_LastStamp = Stamp;

	if (!LastStamp || Stamp <= LastStamp || Info.m_BytesOut <= LastBytesOut)
		return false;

	o_nBytesPerSecond = umint((Info.m_BytesOut - LastBytesOut) * Frequency / (Stamp - LastStamp));

	ULONG nWireWindow = Info.m_Cwnd < Info.m_SndWnd ? Info.m_Cwnd : Info.m_SndWnd;
	o_bAppLimited = Info.m_BytesInFlight + Info.m_Mss < nWireWindow;

	return true;
}
