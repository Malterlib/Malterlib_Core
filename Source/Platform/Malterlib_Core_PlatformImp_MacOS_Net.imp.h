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

bool CPOSIXImpSpecificSocketContext::f_GetSocketCreateParams(::NMib::NNetwork::ENetAddressType _ExpectedType, CSocketCreateParams &o_Params)
{
	if ((uint32)_ExpectedType == 0x100)
	{
		o_Params.m_Domain = PF_SYSTEM;
		o_Params.m_Type = SOCK_DGRAM;
		o_Params.m_Protocol = SYSPROTO_CONTROL;
		return true;
	}
	else if ((uint32)_ExpectedType == 0x101)
	{
		o_Params.m_Domain = PF_SYSTEM;
		o_Params.m_Type = SOCK_STREAM;
		o_Params.m_Protocol = SYSPROTO_CONTROL;
		return true;
	}

	return false;
}

bool CPOSIXImpSpecificSocketContext::f_ResolveAddress(CPOSIXAddress &o_Addr, NMib::NStr::CStr const &_Address, NMib::NNetwork::ENetAddressType _PreferType)
{
	if (_Address.f_StartsWith("KERN_DGRAM:") || _Address.f_StartsWith("KERN_STREAM:"))
	{
		CStr Address;
		int SocketType;
		uint32 MalterlibSocketType;
		if (_Address.f_StartsWith("KERN_DGRAM:"))
		{
			Address = _Address.f_Extract(fg_StrLen("KERN_DGRAM:"));
			SocketType = SOCK_DGRAM;
			MalterlibSocketType = 0x100;
		}
		else
		{
			Address = _Address.f_Extract(fg_StrLen("KERN_STREAM:"));
			SocketType = SOCK_STREAM;
			MalterlibSocketType = 0x101;
		}

		sockaddr_ctl SockAddr;
		fg_MemClear(SockAddr);

		int Socket = socket(PF_SYSTEM, SocketType, SYSPROTO_CONTROL);
		if (Socket == -1)
			return false;

		auto Cleanup = fg_OnScopeExit
			(
				[&]()
				{
					close(Socket);
				}
			)
		;

		SockAddr.sc_len = sizeof(SockAddr);
		SockAddr.sc_family = AF_SYSTEM;
		SockAddr.ss_sysaddr = AF_SYS_CONTROL;
		ctl_info CtlInfo;
		fg_MemClear(CtlInfo);
		fg_StrCopy(CtlInfo.ctl_name, Address.f_GetStr(), sizeof(CtlInfo.ctl_name));

		if (ioctl(Socket, CTLIOCGINFO, &CtlInfo))
			return false;

		SockAddr.sc_id = CtlInfo.ctl_id;
		SockAddr.sc_unit = 0;

		o_Addr.f_Set((NMib::NNetwork::ENetAddressType)MalterlibSocketType, &SockAddr, sizeof(SockAddr));

		return true;
	}

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
