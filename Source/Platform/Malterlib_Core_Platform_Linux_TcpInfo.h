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

	// The limiter accounting (4.10): microseconds spent busy sending, held by the peer's
	// receive window, and held by the send buffer — who the bottleneck was, measured by the
	// kernel itself. Zero on kernels that answer with fewer bytes
	uint64 m_BusyTime;
	uint64 m_RwndLimited;
	uint64 m_SndbufLimited;
	uint32 m_Delivered;
	uint32 m_DeliveredCe;
	uint64 m_BytesSent;
	uint64 m_BytesRetrans;
	uint32 m_DsackDups;
	uint32 m_ReordSeen;
	uint32 m_RcvOoopack;
	uint32 m_SndWnd;
};
static_assert(offsetof(CLinuxTcpInfo, m_MinRtt) == 148 && offsetof(CLinuxTcpInfo, m_DeliveryRate) == 160, "The layout must be the kernel's");
static_assert(offsetof(CLinuxTcpInfo, m_BusyTime) == 168 && offsetof(CLinuxTcpInfo, m_SndWnd) == 228, "The layout must be the kernel's");

inline bool fg_Linux_QueryPathBandwidthDelay(int _Fd, umint &o_nBytes, bool &o_bAppLimited)
{
	CLinuxTcpInfo Info;
	NMib::NMemory::fg_MemClear(&Info, sizeof(Info));
	socklen_t nInfo = sizeof(Info);
	if (getsockopt(_Fd, IPPROTO_TCP, TCP_INFO, &Info, &nInfo) != 0)
		return false;

	// Everything through the delivery rate has to be answered; the limiter accounting past it
	// stays zero on an older kernel and simply does not contribute
	if (nInfo < offsetof(CLinuxTcpInfo, m_BusyTime) || !Info.m_DeliveryRate || !Info.m_MinRtt)
		return false;

	umint nRateDelay = umint(Info.m_DeliveryRate * uint64(Info.m_MinRtt) / 1000000);

	// The congestion window is the kernel's own answer for what belongs in flight, and fq
	// pacing keeps it honest here. The delivery-rate product alone locks a zero copy sender
	// under line rate: its pages release at the acknowledgement, a delay the least round trip
	// does not represent, and the rate the low window then achieves feeds the next sample —
	// a fixed point below the link. The congestion window covers that release latency by
	// construction
	umint nCwndBytes = umint(Info.m_SndCwnd) * umint(Info.m_SndMss);

	o_nBytes = NMib::fg_Max(nRateDelay, nCwndBytes);
	o_bAppLimited = (Info.m_Flags & 1) != 0;

	return true;
}
