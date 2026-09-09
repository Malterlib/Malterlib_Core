// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

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
		NStorage::TCUniquePointer<CRuntimeNetAddress> pAddress;
		if (pResult->ai_family == AF_INET)
			pAddress = fg_Construct(*reinterpret_cast<sockaddr_in const *>(pResult->ai_addr));
		else if (pResult->ai_family == AF_INET6)
			pAddress = fg_Construct(*reinterpret_cast<sockaddr_in6 const *>(pResult->ai_addr));
		else
			continue;

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
