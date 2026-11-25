// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Core/PlatformSpecific/PosixErrNo>
#include <Mib/Core/PlatformSpecific/PosixFork>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>

namespace
{
	NMib::NNetwork::CNetAddressIPv4 fg_ToAddress(sockaddr const *_pAddress)
	{
		uint8 const *pBytes = (uint8 const *)&((sockaddr_in const *)_pAddress)->sin_addr.s_addr;
		return NMib::NNetwork::CNetAddressIPv4(pBytes[0], pBytes[1], pBytes[2], pBytes[3]);
	}
}

// Throws CExceptionNet when the interfaces can't be listed
NMib::NContainer::TCVector<NMib::NNetwork::CNetInterfaceAddressIPv4> NMib::NSys::NNetwork::fg_GetInterfaceAddressesIPv4()
{
	ifaddrs *pAddresses = nullptr;
	if (getifaddrs(&pAddresses) != 0)
		DMibErrorNet(NMib::NPlatform::fg_FormatErrno("getifaddrs (list the interface addresses)", errno));

	auto FreeAddresses = g_OnScopeExit / [pAddresses]
		{
			freeifaddrs(pAddresses);
		}
	;

	NContainer::TCVector<NMib::NNetwork::CNetInterfaceAddressIPv4> Addresses;
	for (ifaddrs *pAddress = pAddresses; pAddress; pAddress = pAddress->ifa_next)
	{
		if (!pAddress->ifa_addr || pAddress->ifa_addr->sa_family != AF_INET)
			continue;

		auto &Address = Addresses.f_Insert();
		Address.m_InterfaceName = pAddress->ifa_name;
		Address.m_Address = fg_ToAddress(pAddress->ifa_addr);
		if (pAddress->ifa_netmask)
			Address.m_Mask = fg_ToAddress(pAddress->ifa_netmask);
		else
			Address.m_Mask = NMib::NNetwork::CNetAddressIPv4(255, 255, 255, 255);

		// ifa_dstaddr is the broadcast address on interfaces that are not point-to-point
		if ((pAddress->ifa_flags & IFF_POINTOPOINT) && pAddress->ifa_dstaddr && pAddress->ifa_dstaddr->sa_family == AF_INET)
		{
			Address.m_PeerAddress = fg_ToAddress(pAddress->ifa_dstaddr);
			Address.m_bPointToPoint = true;
		}
	}

	return Addresses;
}

// Returns a non-blocking routing socket handle that the caller owns, from which the kernel delivers a message for each change of the IPv4
// routes and addresses of the host, for fg_IsInterfaceAddressChange to tell the address changes apart. A full socket drops messages without
// reporting it, and the kernel sends none for an address added or removed on a network whose route another address or a static route
// provides, so callers also list the addresses periodically. Throws CExceptionNet when the socket can't be opened
void *NMib::NSys::NNetwork::fg_OpenInterfaceAddressChangesIPv4()
{
	int File;
	{
		// macOS can't create sockets with close-on-exec set, so a process launched before it is set would inherit the descriptor
		DMibLock(NMib::NPlatform::fg_ForkLock());

		File = socket(PF_ROUTE, SOCK_RAW, AF_INET);
		if (File == -1)
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("socket (open a routing socket)", errno));

		if (fcntl(File, F_SETFD, FD_CLOEXEC) == -1)
		{
			int Error = errno;
			close(File);
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("fcntl (set close on exec of a routing socket)", Error));
		}
	}

	int Flags = fcntl(File, F_GETFL);
	if (Flags == -1 || fcntl(File, F_SETFL, Flags | O_NONBLOCK) == -1)
	{
		int Error = errno;
		close(File);
		DMibErrorNet(NMib::NPlatform::fg_FormatErrno("fcntl (set a routing socket non blocking)", Error));
	}

	return (void *)umint(File);
}

// Whether a message read from a handle of fg_OpenInterfaceAddressChangesIPv4 reports an address that was added or removed
bool NMib::NSys::NNetwork::fg_IsInterfaceAddressChange(void const *_pMessage, umint _nBytes)
{
	if (_nBytes <= offsetof(rt_msghdr, rtm_type))
		return false;

	uint8 Type = ((uint8 const *)_pMessage)[offsetof(rt_msghdr, rtm_type)];
	return Type == RTM_NEWADDR || Type == RTM_DELADDR;
}
