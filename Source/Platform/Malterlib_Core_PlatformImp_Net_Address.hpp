// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

// Preserves the endpoint syntax shared by native and asynchronous DNS backends.
inline void CRuntimeNetAddress::fs_ParseEndpoint(NStr::CStr const &_Address, NSys::NNetwork::CResolveAddressParameters &o_Parameters, NStr::CStr &o_Service)
{
	using namespace NMib::NNetwork;

	auto &Host = o_Parameters.m_Host;
	auto &PreferType = o_Parameters.m_PreferType;
	Host = _Address;
	o_Parameters.m_Port = 0;
	o_Service.f_Clear();

	if (Host.f_StartsWith("IPv4:"))
	{
		PreferType = ENetAddressType_TCPv4;
		Host = Host.f_Extract(fg_StrLen("IPv4:"));
	}
	else if (Host.f_StartsWith("IPv6:"))
	{
		PreferType = ENetAddressType_TCPv6;
		Host = Host.f_Extract(fg_StrLen("IPv6:"));
	}

	bool bCanParsePort = PreferType != ENetAddressType_TCPv6
		|| Host.f_StartsWith("[") || Host.f_FindChar(':') == Host.f_FindCharReverse(':')
	;
	if (auto iService = Host.f_FindCharReverse(':'); bCanParsePort && iService >= 0)
	{
		o_Service = Host.f_Extract(iService + 1);
		Host = Host.f_Left(iService);
	}

	if (PreferType == ENetAddressType_TCPv6)
		Host = Host.f_RemovePrefix("[").f_RemoveSuffix("]");
}

inline auto CRuntimeNetAddress::fs_FromNative(void const *_pAddress, umint _Size) -> NStorage::TCUniquePointer<CRuntimeNetAddress>
{
	if (!_pAddress || _Size < sizeof(sockaddr))
		DMibErrorNet("Invalid native socket address");

	auto *pAddress = static_cast<sockaddr const *>(_pAddress);
	if (pAddress->sa_family == AF_INET && _Size >= sizeof(sockaddr_in))
		return fg_Construct(*static_cast<sockaddr_in const *>(_pAddress));
	if (pAddress->sa_family == AF_INET6 && _Size >= sizeof(sockaddr_in6))
		return fg_Construct(*static_cast<sockaddr_in6 const *>(_pAddress));

	DMibErrorNet("Unsupported native socket address");
}

template <typename tf_CAddressInfo>
auto CRuntimeNetAddress::fs_FromResolved(tf_CAddressInfo const *_pResults) -> NContainer::TCVector<NSys::NNetwork::CAddress>
{
	NContainer::TCVector<NSys::NNetwork::CAddress> Addresses;
	auto Cleanup = g_OnScopeExit / [&Addresses]
		{
			for (auto Address : Addresses)
			{
				NStorage::TCUniquePointer<CRuntimeNetAddress> pAddress = fg_Explicit(static_cast<CRuntimeNetAddress *>(Address));
			}
		}
	;

	for (auto *pResult = _pResults; pResult; pResult = pResult->ai_next)
	{
		if (pResult->ai_family != AF_INET && pResult->ai_family != AF_INET6)
			continue;

		auto pAddress = fs_FromNative(pResult->ai_addr, pResult->ai_addrlen);
		Addresses.f_Insert(pAddress.f_Get());
		pAddress.f_Detach();
	}

	Cleanup.f_Clear();

	return Addresses;
}

inline uint32 CRuntimeNetAddress::f_GetScopeID() const
{
	return mp_Type == NMib::NNetwork::ENetAddressType_TCPv6 ? f_GetTCPv6().sin6_scope_id : 0;
}
