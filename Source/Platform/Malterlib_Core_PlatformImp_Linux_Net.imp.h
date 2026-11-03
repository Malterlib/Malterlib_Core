// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

struct CPOSIXImpSpecificSocketContext::CInternal
{
private:
};

CPOSIXImpSpecificSocketContext::CPOSIXImpSpecificSocketContext()
{
	mp_pInternal = fg_Construct();
}

CPOSIXImpSpecificSocketContext::~CPOSIXImpSpecificSocketContext()
{
}

bool CPOSIXImpSpecificSocketContext::f_CreateAddress(CPOSIXAddress &o_Addr, NMib::NNetwork::ENetAddressType _Type, void const *_pData, umint _nDataBytes)
{
	return false;
}

bool CPOSIXImpSpecificSocketContext::f_ResolveAddress(CPOSIXAddress &o_Addr, NMib::NStr::CStr const &_Address, NMib::NNetwork::ENetAddressType _PreferType)
{
	return false;
}

bool CPOSIXImpSpecificSocketContext::f_GetAddressRaw(CPOSIXAddress const &_Address, ENetAddressType _ExpectedType, void *o_pRawData, umint _nDataBytes)
{
	return false;
}

CPOSIXAddress *CPOSIXImpSpecificSocketContext::f_SetAddressRaw(CPOSIXAddress *_Address, ::NMib::NNetwork::ENetAddressType _ExpectedType, void const *_pRawData, umint _nDataBytes)
{
	NMib::NSys::NNetwork::fg_FreeAddress(_Address);
	return nullptr;
}

bool CPOSIXImpSpecificSocketContext::f_GetSocketCreateParams(::NMib::NNetwork::ENetAddressType _ExpectedType, CSocketCreateParams &o_Params)
{
	return false;
}
