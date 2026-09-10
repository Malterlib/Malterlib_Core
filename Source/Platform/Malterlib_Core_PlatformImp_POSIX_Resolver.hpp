// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

// Returns an owned local/numeric address, or a hostname and port for DNS. No DNS is performed.
// Service database and platform-specific address operations may block.
auto CPOSIXSocketContext::f_PrepareResolveAddress(NStr::CStr const &_Address, NSys::NNetwork::CResolveAddressParameters &o_Parameters, bool _bThrowOnError)
	-> NSys::NNetwork::CAddress
{
	o_Parameters.m_Host.f_Clear();
	o_Parameters.m_Port = 0;

	CPOSIXAddress ResolvedAddress;

	if (_Address.f_StartsWith("UNIX(") || _Address.f_StartsWith("UNIX:"))
	{
		auto Address = CUnixAddress::fs_Parse(_Address, _bThrowOnError);
		if (!Address)
			return nullptr;

		ResolvedAddress.f_Set(fg_Move(*Address));
		NStorage::TCUniquePointer<CPOSIXAddress> pAddress = fg_Construct(fg_Move(ResolvedAddress));

		return pAddress.f_Detach();
	}
	else if (mp_ImpSpecific.f_ResolveAddress(ResolvedAddress, _Address, o_Parameters.m_PreferType))
	{
		NStorage::TCUniquePointer<CPOSIXAddress> pAddress = fg_Construct(fg_Move(ResolvedAddress));

		return pAddress.f_Detach();
	}

	NStr::CStr Service;
	CPOSIXAddress::fs_ParseEndpoint(_Address, o_Parameters, Service);

	addrinfo Hints{};
	Hints.ai_family = o_Parameters.m_PreferType == ENetAddressType_TCPv6 ? AF_INET6 : AF_INET;
	Hints.ai_socktype = SOCK_STREAM;
	Hints.ai_flags = AI_NUMERICHOST;
	addrinfo *pResults = nullptr;
	auto Cleanup = g_OnScopeExit / [&pResults]
		{
			if (pResults)
				freeaddrinfo(pResults);
		}
	;

	int Error = getaddrinfo(o_Parameters.m_Host.f_GetStr(), Service.f_GetStr(), &Hints, &pResults);
	if (Error && o_Parameters.m_PreferType == ENetAddressType_None)
	{
		if (pResults)
			freeaddrinfo(pResults);
		pResults = nullptr;
		Hints.ai_family = AF_INET6;
		Error = getaddrinfo(o_Parameters.m_Host.f_GetStr(), Service.f_GetStr(), &Hints, &pResults);
	}

	if (!Error && pResults)
	{
		auto Address = CPOSIXAddress::fs_FromNative(pResults->ai_addr, pResults->ai_addrlen);
		o_Parameters.m_Host.f_Clear();

		return Address.f_Detach();
	}

	if (Service.f_IsEmpty())
		return nullptr;

	if (pResults)
		freeaddrinfo(pResults);
	pResults = nullptr;
	Hints.ai_family = AF_INET;
	Hints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;
	Error = getaddrinfo(nullptr, Service.f_GetStr(), &Hints, &pResults);
	if (Error || !pResults)
	{
		o_Parameters.m_Host.f_Clear();
		if (_bThrowOnError)
			DMibErrorNet(NStr::fg_Format("Could not resolve service '{}': {}", Service, Error));

		return nullptr;
	}

	o_Parameters.m_Port = ntohs(reinterpret_cast<sockaddr_in const *>(pResults->ai_addr)->sin_port);

	return nullptr;
}

auto CPOSIXSocketContext::f_ResolveAddresses(NStr::CStr const &_Address, NMib::NNetwork::ENetAddressType _PreferType, bool _bThrowOnError)
	-> NContainer::TCVector<NSys::NNetwork::CAddress>
{
	NSys::NNetwork::CResolveAddressParameters Parameters;
	Parameters.m_PreferType = _PreferType;
	NStorage::TCUniquePointer<CPOSIXAddress> Address = fg_Explicit(static_cast<CPOSIXAddress *>(f_PrepareResolveAddress(_Address, Parameters, _bThrowOnError)));
	if (Address)
	{
		NContainer::TCVector<NSys::NNetwork::CAddress> Addresses{Address.f_Get()};
		Address.f_Detach();

		return Addresses;
	}
	if (Parameters.m_Host.f_IsEmpty())
		return {};

	_PreferType = Parameters.m_PreferType;
	auto const &AddressStr = Parameters.m_Host;
	CStr Service = NStr::fg_Format("{}", Parameters.m_Port);
	addrinfo AddrHint{};
	AddrHint.ai_family = _PreferType == ENetAddressType_TCPv6 ? AF_INET6 : AF_INET;
	AddrHint.ai_socktype = SOCK_STREAM;
	AddrHint.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV;

	addrinfo* pAddresses = nullptr;

	auto Cleanup = fg_OnScopeExit(
			[&]()
			{
				if (pAddresses != nullptr)
					freeaddrinfo(pAddresses);
			}
		);

	int Result = getaddrinfo(AddressStr.f_GetStr(), Service.f_GetStr(), &AddrHint, &pAddresses);

	// Try TCPv4 first, then v6.
	if (_PreferType == ENetAddressType_None && Result != 0)
	{
		freeaddrinfo(pAddresses);
		pAddresses = nullptr;

		AddrHint.ai_family = AF_INET6;
		Result = getaddrinfo(AddressStr.f_GetStr(), Service.f_GetStr(), &AddrHint, &pAddresses);
	}

	if
		(
			Result != 0
			&&
			(
				_Address == NMib::NProcess::NPlatform::fg_Process_GetComputerAddress()
				|| _Address == NMib::NProcess::NPlatform::fg_Process_GetHostName()
				|| _Address == NMib::NProcess::NPlatform::fg_Process_GetFullyQualiedHostName()
			)
		)
		Result = getaddrinfo("localhost", Service.f_GetStr(), &AddrHint, &pAddresses);

	if (Result != 0)
	{
		if (_bThrowOnError)
			DMibErrorNet(::fg_FormatGAI<CStr>("getaddrinfo('{}', '{}')"_f << AddressStr << Service, Result));
		else
			return {};
	}

	// Keep the first supported native result first, matching the single-address API.
	addrinfo *pChosenAddress = pAddresses;
	while
	(
		pChosenAddress
		&& pChosenAddress->ai_family != AF_INET
		&& pChosenAddress->ai_family != AF_INET6
	)
	{
		pChosenAddress = pChosenAddress->ai_next;
	}

	if (!pChosenAddress)
	{
		if (_bThrowOnError)
			DMibErrorNet("No supported valid address found");

		return {};
	}

	return CPOSIXAddress::fs_FromResolved(pChosenAddress);
}

auto CPOSIXSocketContext::f_ResolveHost(NStr::CStr const &_Host, NMib::NNetwork::ENetAddressType _PreferType) -> NContainer::TCVector<NSys::NNetwork::CAddress>
{
	using namespace NMib::NNetwork;

	addrinfo Hints{};
	Hints.ai_family = _PreferType == ENetAddressType_TCPv4 ? AF_INET : _PreferType == ENetAddressType_TCPv6 ? AF_INET6 : AF_UNSPEC;
	Hints.ai_socktype = SOCK_STREAM;

	addrinfo *pResults = nullptr;
	int Error = getaddrinfo(_Host.f_GetStr(), nullptr, &Hints, &pResults);
	if (Error)
		DMibErrorNet(NStr::fg_Format("Could not resolve '{}': {}", _Host, Error));

	auto Cleanup = g_OnScopeExit / [pResults]
		{
			freeaddrinfo(pResults);
		}
	;

	auto Addresses = CPOSIXAddress::fs_FromResolved(pResults);

	if (Addresses.f_IsEmpty())
		DMibErrorNet("Name resolution returned no IP addresses");

	return Addresses;
}

void CPOSIXSocketContext::f_AsyncResolveAddress_CloseAsync(void *_pResolver, NMib::NFunction::TCFunctionMovable<void ()> &&_fOnClosed)
{
	mp_Resolver.f_CloseAsync(_pResolver, fg_Move(_fOnClosed));
}
