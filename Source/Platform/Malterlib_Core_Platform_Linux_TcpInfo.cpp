// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include "Malterlib_Core_Platform_Linux_TcpInfo.h"

// Returns peer acknowledgement rate; release latency is measured separately.
bool fg_Linux_QueryPathDeliveryRate(int _Fd, umint &o_nBytesPerSecond, bool &o_bAppLimited)
{
	CLinuxTcpInfo Info;
	NMib::NMemory::fg_MemClear(&Info, sizeof(Info));
	socklen_t nInfo = sizeof(Info);
	if (getsockopt(_Fd, IPPROTO_TCP, TCP_INFO, &Info, &nInfo) != 0)
		return false;

	// Everything through the delivery rate has to be answered; the limiter accounting past it
	// stays zero on an older kernel and simply does not contribute
	if (nInfo < offsetof(CLinuxTcpInfo, m_BusyTime) || !Info.m_DeliveryRate)
		return false;

	o_nBytesPerSecond = umint(Info.m_DeliveryRate);
	o_bAppLimited = (Info.m_Flags & 1) != 0;

	return true;
}
