// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

auto CPOSIXSocketContext::f_ResolveAddresses(NStr::CStr const &_Address, NMib::NNetwork::ENetAddressType _PreferType, bool _bThrowOnError)
	-> NContainer::TCVector<NSys::NNetwork::CAddress>
{
	CPOSIXAddress ResolvedAddress;

	if (_Address.f_StartsWith("UNIX(") || _Address.f_StartsWith("UNIX:"))
	{
		auto Address = CUnixAddress::fs_Parse(_Address, _bThrowOnError);
		if (!Address)
			return {};

		ResolvedAddress.f_Set(fg_Move(*Address));
		NStorage::TCUniquePointer<CPOSIXAddress> pAddress = fg_Construct(fg_Move(ResolvedAddress));
		NContainer::TCVector<NSys::NNetwork::CAddress> Addresses{pAddress.f_Get()};
		pAddress.f_Detach();

		return Addresses;
	}
	else if (mp_ImpSpecific.f_ResolveAddress(ResolvedAddress, _Address, _PreferType))
	{
		NStorage::TCUniquePointer<CPOSIXAddress> pAddress = fg_Construct(fg_Move(ResolvedAddress));
		NContainer::TCVector<NSys::NNetwork::CAddress> Addresses{pAddress.f_Get()};
		pAddress.f_Detach();

		return Addresses;
	}

	addrinfo AddrHint;
	fg_MemClear(AddrHint);

	CStr AddressStr = _Address;

	if (_Address.f_StartsWith("IPv4:"))
	{
		_PreferType = ENetAddressType_TCPv4;
		AddressStr = _Address.f_Extract(fg_StrLen("IPv4:"));
	}
	else if (_Address.f_StartsWith("IPv6:"))
	{
		_PreferType = ENetAddressType_TCPv6;
		AddressStr = _Address.f_Extract(fg_StrLen("IPv6:"));
	}

	CStr Service;

	bool bCanParsePort;
	if (_PreferType == ENetAddressType_TCPv6)
	{
		if (AddressStr.f_StartsWith("["))
			bCanParsePort = true;
		else if (AddressStr.f_FindChar(':') == AddressStr.f_FindCharReverse(':'))
			bCanParsePort = true;
		else
			bCanParsePort = false;
	}
	else
		bCanParsePort = true;

	if (auto iService = AddressStr.f_FindCharReverse(':'); bCanParsePort && iService >= 0)
	{
		Service = AddressStr.f_Extract(iService + 1);
		AddressStr = AddressStr.f_Left(iService);
	}

	if (_PreferType == ENetAddressType_TCPv6)
		AddressStr = AddressStr.f_RemovePrefix("[").f_RemoveSuffix("]");

	if (_PreferType == ENetAddressType_TCPv6)
		AddrHint.ai_family = AF_INET6;
	else
		AddrHint.ai_family = AF_INET;

	AddrHint.ai_socktype = SOCK_STREAM;

	AddrHint.ai_flags = AI_ADDRCONFIG;

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
