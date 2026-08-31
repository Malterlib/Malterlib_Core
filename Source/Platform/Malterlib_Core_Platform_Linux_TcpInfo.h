// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
// Linux measures both halves itself: tcpi_delivery_rate is the rate the peer has been
// acknowledging at, tcpi_min_rtt the least round trip seen, and it says when the rate was
// held back by the sender rather than the path. The C library's tcp_info stops short of
// those, so the kernel's layout is spelled out here as far as the delivery rate; the kernel
// returns as much as it has, and a shorter answer means an older kernel without them
struct CLinuxTcpInfo
{
	uint8 m_State;
	uint8 m_CaState;
	uint8 m_Retransmits;
	uint8 m_Probes;
	uint8 m_Backoff;
	uint8 m_Options;
	uint8 m_WindowScales;
	uint8 m_Flags; // bit 0: the delivery rate was limited by the application
	uint32 m_Rto;
	uint32 m_Ato;
	uint32 m_SndMss;
	uint32 m_RcvMss;
	uint32 m_Unacked;
	uint32 m_Sacked;
	uint32 m_Lost;
	uint32 m_Retrans;
	uint32 m_Fackets;
	uint32 m_LastDataSent;
	uint32 m_LastAckSent;
	uint32 m_LastDataRecv;
	uint32 m_LastAckRecv;
	uint32 m_Pmtu;
	uint32 m_RcvSsthresh;
	uint32 m_Rtt;
	uint32 m_Rttvar;
	uint32 m_SndSsthresh;
	uint32 m_SndCwnd;
	uint32 m_Advmss;
	uint32 m_Reordering;
	uint32 m_RcvRtt;
	uint32 m_RcvSpace;
	uint32 m_TotalRetrans;
	uint64 m_PacingRate;
	uint64 m_MaxPacingRate;
	uint64 m_BytesAcked;
	uint64 m_BytesReceived;
	uint32 m_SegsOut;
	uint32 m_SegsIn;
	uint32 m_NotsentBytes;
	uint32 m_MinRtt;
	uint32 m_DataSegsIn;
	uint32 m_DataSegsOut;
	uint64 m_DeliveryRate;
};
static_assert(offsetof(CLinuxTcpInfo, m_MinRtt) == 148 && offsetof(CLinuxTcpInfo, m_DeliveryRate) == 160, "The layout must be the kernel's");

inline bool fg_Linux_QueryPathBandwidthDelay(int _Fd, umint &o_nBytes, bool &o_bAppLimited)
{
	CLinuxTcpInfo Info;
	NMib::NMemory::fg_MemClear(&Info, sizeof(Info));
	socklen_t nInfo = sizeof(Info);
	if (getsockopt(_Fd, IPPROTO_TCP, TCP_INFO, &Info, &nInfo) != 0)
		return false;
	if (nInfo < sizeof(Info) || !Info.m_DeliveryRate || !Info.m_MinRtt)
		return false;

	o_nBytes = umint(Info.m_DeliveryRate * uint64(Info.m_MinRtt) / 1000000);
	o_bAppLimited = (Info.m_Flags & 1) != 0;

	return true;
}
