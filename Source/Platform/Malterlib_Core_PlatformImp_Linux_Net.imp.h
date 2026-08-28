// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

class CPOSIXImpSpecificSocketContext::CInternal
{
private:
public:
};

CPOSIXImpSpecificSocketContext::CPOSIXImpSpecificSocketContext()
{
	mp_pInternal = fg_Construct();
}

CPOSIXImpSpecificSocketContext::~CPOSIXImpSpecificSocketContext()
{
}

bool CPOSIXImpSpecificSocketContext::f_CreateAddress(CPOSIXAddress& _oAddr, NMib::NNetwork::ENetAddressType _Type, void const* _pData, umint _nDataBytes)
{
	return false;
}

bool CPOSIXImpSpecificSocketContext::f_ResolveAddress(CPOSIXAddress& _oAddr, const NMib::NStr::CStr &_Address, NMib::NNetwork::ENetAddressType _PreferType)
{
	return false;
}

bool CPOSIXImpSpecificSocketContext::f_GetAddressRaw(CPOSIXAddress const &_Address, ENetAddressType _ExpectedType, void* _opRawData, umint _nDataBytes)
{
	return false;
}

CPOSIXAddress* CPOSIXImpSpecificSocketContext::f_SetAddressRaw(CPOSIXAddress* _Address, ::NMib::NNetwork::ENetAddressType _ExpectedType, void const* _pRawData, umint _nDataBytes)
{
	NMib::NSys::NNetwork::fg_FreeAddress(_Address);
	return nullptr;
}

bool CPOSIXImpSpecificSocketContext::f_GetSocketCreateParams(::NMib::NNetwork::ENetAddressType _ExpectedType, CSocketCreateParams &_oParams)
{
	return false;
}
