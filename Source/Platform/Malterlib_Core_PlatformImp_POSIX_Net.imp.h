// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_PlatformImp_POSIX_Net.h"

CPOSIXSocketContext::CPOSIXSocketContext()
{
	mp_PollerThread.f_Start(EThreadPriority_Highest);
	signal(SIGPIPE, SIG_IGN);
}

CPOSIXSocketContext::~CPOSIXSocketContext()
{
	mp_PollerThread.f_Stop(true);
}

CPOSIXAddress* CPOSIXSocketContext::f_CreateAddress(NMib::NNet::ENetAddressType _Type, void const* _pData, mint _nDataBytes)
{
	switch(_Type)
	{

		case NMib::NNet::ENetAddressType_TCPv4:
			{
				if (_nDataBytes != sizeof(NMib::NNet::CNetAddressTCPv4))
					return nullptr;

				sockaddr_in NativeAddr;
				fp_ToNative(*(NMib::NNet::CNetAddressTCPv4*)_pData, NativeAddr);

				return DMibNew CPOSIXAddress(NativeAddr);
			}
			break;

		case NMib::NNet::ENetAddressType_TCPv6:
			{
				if (_nDataBytes != sizeof(NMib::NNet::CNetAddressTCPv6))
					return nullptr;

				sockaddr_in6 NativeAddr;
				fp_ToNative(*(NMib::NNet::CNetAddressTCPv6*)_pData, NativeAddr);

				return DMibNew CPOSIXAddress(NativeAddr);
			}
			break;

		case NMib::NNet::ENetAddressType_Unix:
		default:
		{
			NMib::NPtr::TCUniquePointer<CPOSIXAddress> pAddress = fg_Construct();

			if (mp_ImpSpecific.f_CreateAddress(*pAddress, _Type, _pData, _nDataBytes))
			{
				return pAddress.f_Detach();
			}
			else
			{
				return nullptr;
			}
		}
	}
}

CPOSIXAddress* CPOSIXSocketContext::f_DuplicateAddress(CPOSIXAddress const& _Address)
{
	NMib::NPtr::TCUniquePointer<CPOSIXAddress> pNewAddress = fg_Construct(_Address);
	return pNewAddress.f_Detach();
}


NMib::NNet::ENetAddressType CPOSIXSocketContext::f_GetAddressType(CPOSIXAddress const& _Address)
{
	return _Address.f_GetType();
}

bint CPOSIXSocketContext::f_GetAddressRaw(CPOSIXAddress const& _Address, NMib::NNet::ENetAddressType _ExpectedType, void* _opRawData, mint _nDataBytes)
{
	NMib::NNet::ENetAddressType Type = _Address.f_GetType();

	if (Type != _ExpectedType)
		return false;

	switch(Type)
	{

		case NMib::NNet::ENetAddressType_TCPv4:
			{
				NMib::NNet::CNetAddressTCPv4& Addr = *(NMib::NNet::CNetAddressTCPv4*)_opRawData;
				fp_FromNative(_Address.f_GetTCPv4(), Addr);

				return true;
			}
			break;

		case NMib::NNet::ENetAddressType_TCPv6:
			{
				NMib::NNet::CNetAddressTCPv6& Addr = *(NMib::NNet::CNetAddressTCPv6*)_opRawData;
				fp_FromNative(_Address.f_GetTCPv6(), Addr);

				return true;
			}
			break;

		case NMib::NNet::ENetAddressType_Unix:
		default:
			{
				return mp_ImpSpecific.f_GetAddressRaw(_Address, _ExpectedType, _opRawData, _nDataBytes);
			}
	}
}

CPOSIXAddress* CPOSIXSocketContext::f_SetAddressRaw(CPOSIXAddress* _pAddress, ::NMib::NNet::ENetAddressType _Type, void const* _pRawData, mint _nDataBytes)
{
	if (_Type != _pAddress->f_GetType())
	{
		f_FreeAddress(_pAddress);
		return f_CreateAddress(_Type, _pRawData, _nDataBytes);
	}

	NMib::NNet::ENetAddressType Type = _pAddress->f_GetType();

	switch(Type)
	{

		case NMib::NNet::ENetAddressType_TCPv4:
			{
				NMib::NNet::CNetAddressTCPv4 const& Addr = *(NMib::NNet::CNetAddressTCPv4 const*)_pRawData;
				fp_ToNative(Addr, _pAddress->f_GetTCPv4());

				return _pAddress;
			}
			break;

		case NMib::NNet::ENetAddressType_TCPv6:
			{
				NMib::NNet::CNetAddressTCPv6 const& Addr = *(NMib::NNet::CNetAddressTCPv6 const*)_pRawData;
				fp_ToNative(Addr, _pAddress->f_GetTCPv6());

				return _pAddress;
			}
			break;

		case NMib::NNet::ENetAddressType_Unix:
		default:
			{
				return mp_ImpSpecific.f_SetAddressRaw(_pAddress, _Type, _pRawData, _nDataBytes);
			}
	}
}

CPOSIXAddress* CPOSIXSocketContext::f_ResolveAddress(const NMib::NStr::CStr &_Address, ENetAddressType _PreferType)
{
	return f_ResolveAddress(_Address, _PreferType, true);
}

template <typename tf_CStr>
tf_CStr fg_FormatGAI(typename tf_CStr::CFormat &_Desc, int _Err)
{
	auto pError = gai_strerror(_Err);
	
	tf_CStr Ret;
	
	Ret = "The OS returned an error from ";
	Ret += _Desc;
	
	if (!pError)
		Ret += typename tf_CStr::CFormat(": Unknown error ({})") << _Err;
	else
		Ret += typename tf_CStr::CFormat(": {} ({})") << pError << _Err;
	return Ret;
}

CPOSIXAddress* CPOSIXSocketContext::f_ResolveAddress(const NMib::NStr::CStr &_Address, ENetAddressType _PreferType, bint _bThrowOnError)
{
	NMib::NPtr::TCUniquePointer<CPOSIXAddress> pAddress = fg_Construct();

	if (_Address.f_StartsWith("UNIX:"))
	{
		int SocketType;
		uint32 MalterlibSocketType;
		CStr Address;
		{
			Address = _Address.f_Extract(fg_StrLen("UNIX:"));
			SocketType = SOCK_STREAM;
			MalterlibSocketType = 0x101;
		}

		if (Address.f_GetLen() > (sizeof(sockaddr_un::sun_path) - 1))
		{
			if (_bThrowOnError)
				DMibErrorNet(fg_Format("Unix sockets support a maximum path length of {} characters", (sizeof(sockaddr_un::sun_path) - 1)));
			else
				return nullptr;
		}
			
		sockaddr_un AddressUn;
		
		AddressUn.sun_family = AF_UNIX;
		NMib::NStr::fg_StrCopy(AddressUn.sun_path, Address, sizeof(sockaddr_un::sun_path));
#if !defined(DPlatformFamily_Linux)
		AddressUn.sun_len = sizeof(AddressUn);
#endif
		
		pAddress->f_Set(AddressUn);
		return pAddress.f_Detach();
	}
	else if (mp_ImpSpecific.f_ResolveAddress(*pAddress, _Address, _PreferType))
	{
		return pAddress.f_Detach();
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

	int Result = getaddrinfo(AddressStr.f_GetStr(), nullptr, &AddrHint, &pAddresses);

	// Try TCPv4 first, then v6.
	if (_PreferType == ENetAddressType_None && Result != 0)
	{
		freeaddrinfo(pAddresses);
		pAddresses = nullptr;

		AddrHint.ai_family = AF_INET6;
		Result = getaddrinfo(AddressStr.f_GetStr(), nullptr, &AddrHint, &pAddresses);
	}

	if (Result != 0)
	{
		if (_bThrowOnError)
			DMibErrorNet(::fg_FormatGAI<CStr>(CStr::CFormat("getaddrinfo('{}')") << AddressStr, Result));
		else
			return nullptr;
	}

	// Just use the first address of the correct family returned (all should be of the correct family).
	addrinfo *pChosenAddress = pAddresses;
	{
		while(		pChosenAddress && pChosenAddress->ai_family != AF_INET
				&&	pChosenAddress && pChosenAddress->ai_family != AF_INET6)
			pChosenAddress = pChosenAddress->ai_next;

		if (!_bThrowOnError && !pChosenAddress)
			return nullptr;
		else if (!pChosenAddress)
			DMibErrorNet("No supported valid address found");
	}

	if (pChosenAddress->ai_family == AF_INET)
	{
		pAddress->f_Set(*(sockaddr_in const*)pChosenAddress->ai_addr);
	}
	else if (pChosenAddress->ai_family == AF_INET6)
	{
		pAddress->f_Set(*(sockaddr_in6 const*)pChosenAddress->ai_addr);
	}
	else
	{
//		DMibNeverGetHere;
		if (_bThrowOnError)
			DMibErrorNet("Address is not from a supported adress type");
		else
			return nullptr;
	}

	return pAddress.f_Detach();
}

void *CPOSIXSocketContext::f_AsyncResolveAddress_Open(const NMib::NStr::CStr &_Address, ::NMib::NNet::ENetAddressType _PreferType, NMib::NFunction::TCFunction<void ()>&& _fOnFinish)
{
	return mp_Resolver.f_Open(_Address, _PreferType, fg_Move(_fOnFinish));
}

bint CPOSIXSocketContext::f_AsyncResolveAddress_GetResult(void *_pResolver, CPOSIXAddress*& _opAddress, NMib::NStr::CStr &_Error)
{
	return mp_Resolver.f_GetResult(_pResolver, (NMib::NSys::NNet::CAddress&)_opAddress, _Error);
}

void CPOSIXSocketContext::f_AsyncResolveAddress_Close(void *_pResolver)
{
	mp_Resolver.f_Close(_pResolver);
}

int CPOSIXSocketContext::f_CompareAddresses(CPOSIXAddress const& _First, CPOSIXAddress const& _Second)
{
	return _First.f_Compare(_Second);
}

void CPOSIXSocketContext::f_FreeAddress(CPOSIXAddress* _pAddress) // It is OK to free a nullptr address.
{
	delete _pAddress;
}

#ifdef DPlatformFamily_OSX
#	ifndef s6_addr16
#		define s6_addr16 __u6_addr.__u6_addr16
#	endif
#	ifndef s6_addr32
#		define s6_addr32 __u6_addr.__u6_addr32
#	endif
#endif

NMib::NStr::CStr CPOSIXSocketContext::f_GetAddressString(CPOSIXAddress const& _Address, bint _bIncludeType)
{
	NMib::NStr::CStr AddressStr;

	switch(_Address.f_GetType())
	{
		case NMib::NNet::ENetAddressType_TCPv4:
			{
				auto &Address = _Address.f_GetTCPv4();
				if (_bIncludeType)
					AddressStr += "TCPv4:";
				AddressStr 
					+= CStr::CFormat("{}.{}.{}.{}") 
					<< ((ntohl(Address.sin_addr.s_addr) >> 24) & 0xFF)
					<< ((ntohl(Address.sin_addr.s_addr) >> 16) & 0xFF)
					<< ((ntohl(Address.sin_addr.s_addr) >> 8) & 0xFF)
					<< ((ntohl(Address.sin_addr.s_addr) >> 0) & 0xFF)
				;
			}
			break;

		case NMib::NNet::ENetAddressType_TCPv6:
			{
				auto &Address = _Address.f_GetTCPv6();
				if (_bIncludeType)
					AddressStr += "TCPv6:";
				
				AddressStr 
					+= CStr::CFormat("{}:{}:{}:{}:{}:{}:{}:{}") 
					<< ntohs(Address.sin6_addr.s6_addr16[0])
					<< ntohs(Address.sin6_addr.s6_addr16[1])
					<< ntohs(Address.sin6_addr.s6_addr16[2])
					<< ntohs(Address.sin6_addr.s6_addr16[3])
					<< ntohs(Address.sin6_addr.s6_addr16[4])
					<< ntohs(Address.sin6_addr.s6_addr16[5])
					<< ntohs(Address.sin6_addr.s6_addr16[6])
					<< ntohs(Address.sin6_addr.s6_addr16[7])
				;
			}
			break;
		case NMib::NNet::ENetAddressType_Unix:
			{
				auto &Address = _Address.f_GetUnix();
				if (_bIncludeType)
					AddressStr += "UNIX:";
				
				AddressStr += CStr::CFormat("{}") << Address.sun_path; 
			}
			break;
/*
		case ENetAddressType_Kernel:
			{
				auto &Address = _Address.f_GetKernel();
				if (_bIncludeType)
					AddressStr += "Kernel:";

				AddressStr += CStr::CFormat("{}.{}.{}.{}") << (Address.sin_addr.s_addr & 0xFF);
			}*/
		default:
			AddressStr = "Unknown or invalid address";
			break;

	}

	return AddressStr;
}

bool CPOSIXSocketContext::fp_GetSocketCreateParams(NMib::NNet::ENetAddressType _AddressType, CPOSIXImpSpecificSocketContext::CSocketCreateParams &o_Params)
{
	if (!mp_ImpSpecific.f_GetSocketCreateParams(_AddressType, o_Params))
	{
		if (_AddressType == ENetAddressType_Unix)
		{
			o_Params.m_Domain = PF_UNIX;
			o_Params.m_Type = SOCK_STREAM;
			o_Params.m_Protocol = 0;
		}
		else 
		{
			if (_AddressType != ENetAddressType_TCPv4 && _AddressType != ENetAddressType_TCPv6)
				return false;
			o_Params.m_Domain = (_AddressType == ENetAddressType_TCPv4) ? PF_INET : PF_INET6;
			o_Params.m_Type = SOCK_STREAM;
			o_Params.m_Protocol = 0;
		}
	}
	return true;
}

CPOSIXSocket* CPOSIXSocketContext::fp_Connect(CPOSIXAddress const& _Address, NMib::NFunction::TCFunction<void (NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange, bint _bAsyncConnect, CPOSIXAddress const *_pBindAddress)
{
	mint Retries = 32;
	int FD;
	bint bConnected;
	while (Retries)
	{
		bConnected = false;
		ENetAddressType AddressType = _Address.f_GetType();

		CPOSIXImpSpecificSocketContext::CSocketCreateParams SocketCreateParams;
		if (!fp_GetSocketCreateParams(AddressType, SocketCreateParams))
			return nullptr;
		
		FD = socket(SocketCreateParams.m_Domain, SocketCreateParams.m_Type, SocketCreateParams.m_Protocol);

		if (FD == -1)
		{
			int Error = errno;
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("socket (connect)", Error));
		}

		if (_pBindAddress)
		{
			int bReuse = 1;
			setsockopt(FD, SOL_SOCKET, SO_REUSEADDR, &bReuse, sizeof(bReuse));

			int Result = bind(FD, (sockaddr const*)_pBindAddress->f_Get(), _pBindAddress->f_GetLen());
			if (Result != 0)
			{
				int Error = errno;
				DMibErrorNet(NMib::NPlatform::fg_FormatErrno("bind (connect)", Error));
			}
		}

		if (_bAsyncConnect)
		{
			int Flags;
			if ((Flags = fcntl(FD, F_GETFL)) == -1 || fcntl(FD, F_SETFL, Flags | O_NONBLOCK) == -1) 
			{
				int Error = errno;
				DMibErrorNet(NMib::NPlatform::fg_FormatErrno("fcntl (connect set async non blocking)", Error));
			}
		}
		int Result = connect(FD, (sockaddr const*)_Address.f_Get(), _Address.f_GetLen());

		uint16 BindPort = 0;
		if (_pBindAddress)
		{
			CNetAddress BindAddress((void *)&_pBindAddress);
			BindPort = BindAddress.f_GetPort();
			BindAddress.f_Detach();
		}

		if (Result != 0)
		{		
			int Error = errno;

			if (Error == EINPROGRESS)
			{
				bConnected = false;
			}
			else if (Error == EADDRNOTAVAIL && _pBindAddress && BindPort == 0)
			{
				close(FD);
				--Retries;
				continue;
			}
			else
			{
				close(FD);
				ENetAddressType AddressType = _Address.f_GetType();
				if (AddressType == ENetAddressType_Unix)
				{
					auto &Unix = _Address.f_GetUnix();
					DMibErrorNet(NMib::NPlatform::fg_FormatErrno(fg_Format("connect ({}, connect)", Unix.sun_path), Error));
				}
				else
					DMibErrorNet(NMib::NPlatform::fg_FormatErrno("connect (connect)", Error));
			}
		}
		else
		{
			bConnected = true;
		}

		if (!_bAsyncConnect)
		{
			int Flags;
			if ((Flags = fcntl(FD, F_GETFL)) == -1 || fcntl(FD, F_SETFL, Flags | O_NONBLOCK) == -1) 
			{
				int Error = errno;
				DMibErrorNet(NMib::NPlatform::fg_FormatErrno("fcntl (connect set non blocking)", Error));
			}
		}
		break;
	}

	auto *pSocket = fp_CreateSocket(FD, bConnected ? EPOSIXSocketMode_Connect : EPOSIXSocketMode_Connecting, EPOSIXSocketEvent_Read | EPOSIXSocketEvent_Write, fg_Move(_OnStateChange));
	
	ENetAddressType AddressType = _Address.f_GetType();
	if (AddressType == ENetAddressType_Unix)
	{
		auto &Unix = _Address.f_GetUnix();
		pSocket->m_PeerUnixFilePath = Unix.sun_path;
	}
	
	return pSocket;
}

void CPOSIXSocketContext::fp_PrepareUnixListen(CPOSIXAddress const &_Address)
{
	if (_Address.f_GetType() == ENetAddressType_Unix)
	{
		auto &Unix = _Address.f_GetUnix();
		NStr::CStr UnixFilePath = Unix.sun_path;
		if (NFile::CFile::fs_FileExists(UnixFilePath))
			NFile::CFile::fs_DeleteFile(UnixFilePath);
	}
}

void CPOSIXSocketContext::fp_SetUnixListenAddress(CPOSIXSocket *_pSocket, CPOSIXAddress const &_Address)
{
	ENetAddressType AddressType = _Address.f_GetType();
	if (AddressType == ENetAddressType_Unix)
	{
		auto &Unix = _Address.f_GetUnix();
		_pSocket->m_UnixFilePath = Unix.sun_path;
	}
}

CPOSIXSocket* CPOSIXSocketContext::f_Connect(CPOSIXAddress const& _Address, NMib::NFunction::TCFunction<void (NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange, CPOSIXAddress const *_pBindAddress)
{
	return fp_Connect(_Address, fg_Move(_OnStateChange), false, _pBindAddress);
}

CPOSIXSocket* CPOSIXSocketContext::f_AsyncConnect(CPOSIXAddress const& _Address, NMib::NFunction::TCFunction<void (NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange, CPOSIXAddress const *_pBindAddress)
{
	return fp_Connect(_Address, fg_Move(_OnStateChange), true, _pBindAddress);
}

CPOSIXSocket* CPOSIXSocketContext::f_Listen(CPOSIXAddress const& _Address, NMib::NFunction::TCFunction<void (NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange, NMib::NNet::ENetFlag _Flags)
{
	ENetAddressType AddressType = _Address.f_GetType();

	CPOSIXImpSpecificSocketContext::CSocketCreateParams SocketCreateParams;
	if (!fp_GetSocketCreateParams(AddressType, SocketCreateParams))
		return nullptr;
	
	fp_PrepareUnixListen(_Address);
	
	int FD = socket(SocketCreateParams.m_Domain, SocketCreateParams.m_Type, SocketCreateParams.m_Protocol);

	if (FD == -1)
	{
		int Error = errno;
		DMibErrorNet(NMib::NPlatform::fg_FormatErrno("socket (listen)", Error));
	}

	{
		int Flags;
		if ((Flags = fcntl(FD, F_GETFL)) == -1 || fcntl(FD, F_SETFL, Flags | O_NONBLOCK) == -1)
		{
			int Error = errno;
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("fcntl (listen set non blocking)", Error));
		}
	}
	
	if (AddressType == ENetAddressType_TCPv6)
	{
		// Only IPV6
		int bV6Only = 1;
		setsockopt(FD, IPPROTO_IPV6, IPV6_V6ONLY, &bV6Only, sizeof(bV6Only));	
	}

	if (_Flags & NNet::ENetFlag_ReuseAddress)
	{
		int bReuse = 1;
		setsockopt(FD, SOL_SOCKET, SO_REUSEADDR, &bReuse, sizeof(bReuse));
	}
	
#ifdef DPlatformFamily_OSX
	if (_Flags & NNet::ENetFlag_ReuseAddress)
	{
		int bReuse = 1;
		setsockopt(FD, SOL_SOCKET, SO_REUSEPORT, &bReuse, sizeof(bReuse));
	}
#endif

	int Result = bind(FD, (sockaddr const*)_Address.f_Get(), _Address.f_GetLen());

	if (Result != 0)
	{
		int Error = errno;
		DMibErrorNet(NMib::NPlatform::fg_FormatErrno("bind (listen)", Error));
	}

	Result = listen(FD, 16);

	if (Result != 0)
	{
		close(FD);
		int Error = errno;
		DMibErrorNet(NMib::NPlatform::fg_FormatErrno("listen (listen)", Error));
	}
	
	auto pSocket = fp_CreateSocket(FD, EPOSIXSocketMode_Listen, EPOSIXSocketEvent_Read, fg_Move(_OnStateChange));
	fp_SetUnixListenAddress(pSocket, _Address);
	return pSocket;
}

CPOSIXSocket* CPOSIXSocketContext::f_ListenDatagram(CPOSIXAddress const& _Address, NMib::NFunction::TCFunction<void (NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange, NMib::NNet::ENetFlag _Flags)
{
	ENetAddressType AddressType = _Address.f_GetType();

	CPOSIXImpSpecificSocketContext::CSocketCreateParams SocketCreateParams;
	if (!fp_GetSocketCreateParams(AddressType, SocketCreateParams))
		return nullptr;

	fp_PrepareUnixListen(_Address);
	
	int FD = socket(SocketCreateParams.m_Domain, SocketCreateParams.m_Type, SocketCreateParams.m_Protocol);

	if (FD == -1)
	{
		int Error = errno;
		DMibErrorNet(NMib::NPlatform::fg_FormatErrno("socket (listen)", Error));
	}

	{
		int Flags;
		if ((Flags = fcntl(FD, F_GETFL)) == -1 || fcntl(FD, F_SETFL, Flags | O_NONBLOCK) == -1) 
		{
			int Error = errno;
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("fcntl (listen set non blocking)", Error));
		}
	}
	if (_Flags & NNet::ENetFlag_ReuseAddress)
	{
		int bReuse = 1;
		setsockopt(FD, SOL_SOCKET, SO_REUSEADDR, &bReuse, sizeof(bReuse));	
	}

	int Result = bind(FD, (sockaddr const*)_Address.f_Get(), _Address.f_GetLen());

	if (Result != 0)
	{
		int Error = errno;
		DMibErrorNet(NMib::NPlatform::fg_FormatErrno("bind (listen)", Error));
	}

	auto pSocket = fp_CreateSocket(FD, EPOSIXSocketMode_Datagram, EPOSIXSocketEvent_Read | EPOSIXSocketEvent_Write, fg_Move(_OnStateChange));

	fp_SetUnixListenAddress(pSocket, _Address);
	
	pSocket->m_BindAddressSize = _Address.f_GetLen();
	pSocket->m_BindAddressType = AddressType;

	return pSocket;
}

CPOSIXSocket* CPOSIXSocketContext::f_Accept(CPOSIXSocket *_pSocket, NMib::NFunction::TCFunction<void (NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange)
{
	int ResultFD = accept(_pSocket->m_FD, NULL, NULL);

	if (ResultFD == -1)
	{
		return nullptr;
	}

	{
		int Flags;
		if ((Flags = fcntl(ResultFD, F_GETFL)) == -1 || fcntl(ResultFD, F_SETFL, Flags | O_NONBLOCK) == -1) 
		{
			close(ResultFD);

			int Error = errno;
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("fcntl (accept set non blocking)", Error));
		}
	}

	auto *pSocket = fp_CreateSocket(ResultFD, EPOSIXSocketMode_Connect, EPOSIXSocketEvent_Read | EPOSIXSocketEvent_Write, fg_Move(_OnStateChange));

	{
		sockaddr_storage SocketName;
		socklen_t nAddrBytes = sizeof(SocketName);
		int Ret = getsockname(_pSocket->m_FD, (struct sockaddr *)&SocketName, &nAddrBytes);
		if (Ret == 0 && SocketName.ss_family == AF_UNIX)
		{
			auto &Unix = *((sockaddr_un *)&SocketName);
			pSocket->m_PeerUnixFilePath = Unix.sun_path;
		}
	}
	
	return pSocket;
}

void CPOSIXSocketContext::f_SetOnStateChange(CPOSIXSocket* _pSocket, NMib::NFunction::TCFunction<void (NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange)
{
	{
		DMibLock(_pSocket->m_Lock);
		_pSocket->m_OnStateChange = fg_Move(_OnStateChange);
	}
}

bint CPOSIXSocketContext::f_Close(CPOSIXSocket* _pSocket)
{
	if (_pSocket->m_FD != -1)
	{
		mp_PollerThread.mp_Poller.f_DeregisterSocket(_pSocket);

		{
			DMibLock(_pSocket->m_Lock);
			_pSocket->m_OnStateChange.f_Clear();
			close(_pSocket->m_FD);
		}
	}
	if (!_pSocket->m_UnixFilePath.f_IsEmpty())
	{
		try
		{
			if (NFile::CFile::fs_FileExists(_pSocket->m_UnixFilePath))
				NFile::CFile::fs_DeleteFile(_pSocket->m_UnixFilePath);
		}
		catch (NFile::CExceptionFile const &)
		{
		}
	}

	delete _pSocket;

	return true;
}

void CPOSIXSocketContext::f_Shutdown(CPOSIXSocket* _pSocket)
{
	int Result = shutdown(_pSocket->m_FD, SHUT_WR);

	if (Result == -1)
	{
		if (errno != EAGAIN && errno != ENOTCONN)
		{
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("shutdown", errno));
		}
	}
}

NMib::NNet::ENetTCPState CPOSIXSocketContext::f_GetState(CPOSIXSocket *_pSocket)
{
	NMib::NNet::ENetTCPState State;
	{
		DMibLock(_pSocket->m_Lock);
		State = _pSocket->m_State;
		_pSocket->m_State = NMib::NNet::ENetTCPState_None;
	}

	return State;
}

mint CPOSIXSocketContext::f_Receive(CPOSIXSocket *_pSocket, void *_pData, mint _DataLen)
{
	int Result = recv(_pSocket->m_FD, _pData, _DataLen, 0);

	if (Result == -1)
	{
		if (errno == EAGAIN)
		{
			Result = 0;
		}
		else
		{
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("recv (receive from socket)", errno));
		}
	}

	return Result;	
}

mint CPOSIXSocketContext::f_Send(CPOSIXSocket *_pSocket, const void *_pData, mint _DataLen)
{
	int Result = send(_pSocket->m_FD, _pData, _DataLen, 0);

	if (Result == -1)
	{
		if (errno == EAGAIN)
		{
			Result = 0;
		}
		else
		{
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("send (send to socket)", errno));
		}
	}

	return Result;	
}

mint CPOSIXSocketContext::f_SendDatagram(CPOSIXSocket *_pSocket, CPOSIXAddress const &_Address, const void *_pData, mint _DataLen)
{
	auto &TCP = _Address.f_GetTCPv4();
	(void)TCP;
	int Result = sendto(_pSocket->m_FD, _pData, _DataLen, 0, (sockaddr const*)_Address.f_Get(), _Address.f_GetLen());

	if (Result == -1)
	{
		if (errno == EAGAIN)
		{
			Result = 0;
		}
		else
		{
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("send (send to socket)", errno));
		}
	}

	return Result;	
}

mint CPOSIXSocketContext::f_ReceiveDatagram(CPOSIXSocket *_pSocket, CPOSIXAddress &_Address, void *_pData, mint _DataLen)
{
	socklen_t Len = _pSocket->m_BindAddressSize;
	int Result = recvfrom(_pSocket->m_FD, _pData, _DataLen, 0, (sockaddr *)_Address.f_GetForWrite(_pSocket->m_BindAddressType, Len), &Len);

	if (Result == -1)
	{
		if (errno == EAGAIN)
		{
			Result = 0;
		}
		else
		{
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("send (send to socket)", errno));
		}
	}

	return Result;	
}

NMib::NStr::CStr CPOSIXSocketContext::f_GetCloseReason(CPOSIXSocket* _pSocket)
{
	int CloseReason = 0;

	{
		DMibLock(_pSocket->m_Lock);
		CloseReason = _pSocket->m_CloseError;
	}

	if (CloseReason == 0)
		return NMib::NStr::CStr();
	return NMib::NPlatform::fg_FormatErrno("", CloseReason);
}

CPOSIXSocket* CPOSIXSocketContext::f_InheritHandle2(void* _pOSSocket, NMib::NFunction::TCFunction<void (NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange)
{
	return fp_CreateSocket(int(aint(_pOSSocket)), EPOSIXSocketMode_Connect, EPOSIXSocketEvent_Read | EPOSIXSocketEvent_Write, fg_Move(_OnStateChange), true);
}

void *CPOSIXSocketContext::f_GiveUpForInherit(CPOSIXSocket *_pSocket)
{
	int FD = -1;

	mp_PollerThread.mp_Poller.f_DeregisterSocket(_pSocket);

	{
		DMibLock(_pSocket->m_Lock);
		_pSocket->m_OnStateChange.f_Clear();
		FD = _pSocket->m_FD;
		_pSocket->m_FD = -1;
	}

	return (void*)(mint)FD;
}

void *CPOSIXSocketContext::f_GetOSSocket(CPOSIXSocket *_pSocket)
{
	return (void*)(mint)_pSocket->m_FD;
}

CPOSIXAddress* CPOSIXSocketContext::f_GetPeerAddress(CPOSIXSocket *_pSocket)
{
	sockaddr_storage PeerAddr;

	socklen_t nAddrBytes = sizeof(PeerAddr);

	int Ret = getpeername(_pSocket->m_FD, (struct sockaddr *)&PeerAddr, &nAddrBytes);

	if (Ret != 0)
	{
		int Error = errno;
		DMibErrorNet(NMib::NPlatform::fg_FormatErrno("getpeername (get peer address)", Error));
	}

	if (PeerAddr.ss_family == AF_INET)
	{
		NPtr::TCUniquePointer<CPOSIXAddress> pAddress = fg_Construct(*(sockaddr_in const*)&PeerAddr);
		return pAddress.f_Detach();
	}
	else if (PeerAddr.ss_family == AF_INET6)
	{
		NPtr::TCUniquePointer<CPOSIXAddress> pAddress = fg_Construct(*(sockaddr_in6 const*)&PeerAddr);
		return pAddress.f_Detach();
	}
	else if (PeerAddr.ss_family == AF_UNIX)
	{
		auto UnixAddress = *(sockaddr_un const*)&PeerAddr;
		if (nAddrBytes <= sizeof(UnixAddress.sun_family))
			UnixAddress.sun_path[0] = 0;
		if (fg_StrLen(UnixAddress.sun_path) == 0 && !_pSocket->m_PeerUnixFilePath.f_IsEmpty())
		{
			sockaddr_un Address;
			Address.sun_family = AF_UNIX;
#if !defined(DPlatformFamily_Linux)
			Address.sun_len = sizeof(Address);
#endif
			fg_StrCopy(Address.sun_path, _pSocket->m_PeerUnixFilePath, sizeof(Address.sun_path));
			
			NPtr::TCUniquePointer<CPOSIXAddress> pAddress = fg_Construct(*(sockaddr_un const*)&Address);
			return pAddress.f_Detach();
		}			

		NPtr::TCUniquePointer<CPOSIXAddress> pAddress = fg_Construct(UnixAddress);
		return pAddress.f_Detach();
	}
	else
	{
		return nullptr;
	}
}

uint32 CPOSIXSocketContext::f_GetListenPort(CPOSIXSocket *_pSocket)
{
	sockaddr_storage PeerAddr;

	socklen_t nAddrBytes = sizeof(PeerAddr);

	int Ret = getsockname(_pSocket->m_FD, (struct sockaddr *)&PeerAddr, &nAddrBytes);

	if (Ret != 0)
	{
		int Error = errno;
		DMibErrorNet(NMib::NPlatform::fg_FormatErrno("Could not get socket address", Error));
	}

	if (PeerAddr.ss_family == AF_INET)
	{
		auto &Addr = *(sockaddr_in const*)&PeerAddr;
		return ntohs(Addr.sin_port);
	}
	else if (PeerAddr.ss_family == AF_INET6)
	{
		auto &Addr = *(sockaddr_in6 const*)&PeerAddr;
		return ntohs(Addr.sin6_port);
	}
	else
	{
		return 0;
	}
}


CPOSIXSocket* CPOSIXSocketContext::fp_CreateSocket(int _FD, EPOSIXSocketMode _Mode, EPOSIXSocketEvent _Events, NMib::NFunction::TCFunction<void (NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange, bint _bFromInherit)
{
#ifdef DPlatformFamily_OSX
	{
		// Disable sigpipe from being sent to program. We handle this in exceptions
		int set = 1;
		setsockopt(_FD, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
	}
#endif
	
	NMib::NPtr::TCUniquePointer<CPOSIXSocket> pNewSocket = fg_Construct(_FD, _Mode, _Events, fg_Move(_OnStateChange));

	if (_bFromInherit)
	{
		pNewSocket->m_bInitialWriteNotification = false;
	}
	
	NMib::NNet::ENetTCPState StateAdded = NMib::NNet::ENetTCPState_None;

	if (_Mode == EPOSIXSocketMode_Datagram)
		_Mode = EPOSIXSocketMode_Connect;
	else if (_Mode == EPOSIXSocketMode_Connect)
		StateAdded |= NMib::NNet::ENetTCPState_Connected;
	else if (_Mode == EPOSIXSocketMode_Connecting)
	{
		pNewSocket->m_bInitialWriteNotification = false;
	}

	CPOSIXSocket* pSocket = nullptr;

	mp_PollerThread.mp_Poller.f_RegisterSocket(pSocket = pNewSocket.f_Detach());

	if (StateAdded)
	{
		pSocket->m_State |= StateAdded;
		if (pSocket->m_OnStateChange)
			pSocket->m_OnStateChange(StateAdded);
	}

	return (CPOSIXSocket*)pSocket;
}

#include "Malterlib_Core_PlatformImp_Net.imp.h"
