// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_PlatformImp_POSIX_Net.h"
#include <Mib/Process/Platform>

#include <netinet/tcp.h>

CPOSIXSocketContext::CPOSIXSocketContext()
{
	mp_PollerThread.f_Start(EExecutionPriority_Highest);
	signal(SIGPIPE, SIG_IGN);
}

CPOSIXSocketContext::~CPOSIXSocketContext()
{
	mp_PollerThread.f_Stop(true);
}

CPOSIXAddress* CPOSIXSocketContext::f_CreateAddress(NMib::NNetwork::ENetAddressType _Type, void const* _pData, mint _nDataBytes)
{
	switch(_Type)
	{
	case NMib::NNetwork::ENetAddressType_TCPv4:
		{
			if (_nDataBytes != sizeof(NMib::NNetwork::CNetAddressTCPv4))
				return nullptr;

			sockaddr_in NativeAddr;
			fp_ToNative(*(NMib::NNetwork::CNetAddressTCPv4*)_pData, NativeAddr);

			return DMibNew CPOSIXAddress(NativeAddr);
		}
		break;
	case NMib::NNetwork::ENetAddressType_TCPv6:
		{
			if (_nDataBytes != sizeof(NMib::NNetwork::CNetAddressTCPv6))
				return nullptr;

			sockaddr_in6 NativeAddr;
			fp_ToNative(*(NMib::NNetwork::CNetAddressTCPv6*)_pData, NativeAddr);

			return DMibNew CPOSIXAddress(NativeAddr);
		}
		break;
	case NMib::NNetwork::ENetAddressType_Unix:
	default:
		{
			NMib::NStorage::TCUniquePointer<CPOSIXAddress> pAddress = fg_Construct();

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

CPOSIXAddress* CPOSIXSocketContext::f_DuplicateAddress(CPOSIXAddress const &_Address)
{
	NMib::NStorage::TCUniquePointer<CPOSIXAddress> pNewAddress = fg_Construct(_Address);
	return pNewAddress.f_Detach();
}


NMib::NNetwork::ENetAddressType CPOSIXSocketContext::f_GetAddressType(CPOSIXAddress const &_Address)
{
	return _Address.f_GetType();
}

bool CPOSIXSocketContext::f_GetAddressRaw(CPOSIXAddress const &_Address, NMib::NNetwork::ENetAddressType _ExpectedType, void* _opRawData, mint _nDataBytes)
{
	NMib::NNetwork::ENetAddressType Type = _Address.f_GetType();

	if (Type != _ExpectedType)
		return false;

	switch(Type)
	{

		case NMib::NNetwork::ENetAddressType_TCPv4:
			{
				NMib::NNetwork::CNetAddressTCPv4& Addr = *(NMib::NNetwork::CNetAddressTCPv4*)_opRawData;
				fp_FromNative(_Address.f_GetTCPv4(), Addr);

				return true;
			}
			break;

		case NMib::NNetwork::ENetAddressType_TCPv6:
			{
				NMib::NNetwork::CNetAddressTCPv6& Addr = *(NMib::NNetwork::CNetAddressTCPv6*)_opRawData;
				fp_FromNative(_Address.f_GetTCPv6(), Addr);

				return true;
			}
			break;

		case NMib::NNetwork::ENetAddressType_Unix:
		default:
			{
				return mp_ImpSpecific.f_GetAddressRaw(_Address, _ExpectedType, _opRawData, _nDataBytes);
			}
	}
}

CPOSIXAddress* CPOSIXSocketContext::f_SetAddressRaw(CPOSIXAddress* _pAddress, ::NMib::NNetwork::ENetAddressType _Type, void const* _pRawData, mint _nDataBytes)
{
	if (_Type != _pAddress->f_GetType())
	{
		f_FreeAddress(_pAddress);
		return f_CreateAddress(_Type, _pRawData, _nDataBytes);
	}

	NMib::NNetwork::ENetAddressType Type = _pAddress->f_GetType();

	switch(Type)
	{

		case NMib::NNetwork::ENetAddressType_TCPv4:
			{
				NMib::NNetwork::CNetAddressTCPv4 const& Addr = *(NMib::NNetwork::CNetAddressTCPv4 const*)_pRawData;
				fp_ToNative(Addr, _pAddress->f_GetTCPv4());

				return _pAddress;
			}
			break;

		case NMib::NNetwork::ENetAddressType_TCPv6:
			{
				NMib::NNetwork::CNetAddressTCPv6 const& Addr = *(NMib::NNetwork::CNetAddressTCPv6 const*)_pRawData;
				fp_ToNative(Addr, _pAddress->f_GetTCPv6());

				return _pAddress;
			}
			break;

		case NMib::NNetwork::ENetAddressType_Unix:
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
tf_CStr fg_FormatGAI(typename tf_CStr::CFormat &&_Desc, int _Err)
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

CPOSIXAddress* CPOSIXSocketContext::f_ResolveAddress(const NMib::NStr::CStr &_Address, ENetAddressType _PreferType, bool _bThrowOnError)
{
	NMib::NStorage::TCUniquePointer<CPOSIXAddress> pAddress = fg_Construct();

	if (_Address.f_StartsWith("UNIX(") || _Address.f_StartsWith("UNIX:"))
	{
		using namespace NMib::NFile;

		EFileAttrib Permissions = EFileAttrib_None;
		CStr Address;

		if (_Address.f_StartsWith("UNIX(") )
		{
			auto *pParse = _Address.f_GetStr() + 5;
			bool bFailed = false;
			uint32 UnixPermissions = fg_StrToIntParse(pParse, uint32(01000), "):", false, EStrToIntParseMode_Octal, &bFailed);

			if (bFailed || pParse[0] != ')' || pParse[1] != ':')
			{
				if (_bThrowOnError)
					DMibErrorNet("Failed to parse unix permissions");
				else
					return nullptr;
			}

			pParse += 2;

			if (UnixPermissions >= uint32(01000))
			{
				if (_bThrowOnError)
					DMibErrorNet("Invalid permissions specified");
				else
					return nullptr;
			}

			if (UnixPermissions & 0100)
				Permissions |= EFileAttrib_UserExecute;
			if (UnixPermissions & 0200)
				Permissions |= EFileAttrib_UserWrite;
			if (UnixPermissions & 0400)
				Permissions |= EFileAttrib_UserRead;

			if (UnixPermissions & 010)
				Permissions |= EFileAttrib_GroupExecute;
			if (UnixPermissions & 020)
				Permissions |= EFileAttrib_GroupWrite;
			if (UnixPermissions & 040)
				Permissions |= EFileAttrib_GroupRead;

			if (UnixPermissions & 01)
				Permissions |= EFileAttrib_EveryoneExecute;
			if (UnixPermissions & 02)
				Permissions |= EFileAttrib_EveryoneWrite;
			if (UnixPermissions & 04)
				Permissions |= EFileAttrib_EveryoneRead;

			Address = CStr{pParse};
		}
		else
			Address = _Address.f_Extract(fg_StrLen("UNIX:"));

		if (Address.f_GetLen() > CUnixAddress::mc_MaxAddressLength)
		{
			if (_bThrowOnError)
				DMibErrorNet(fg_Format("Unix sockets support a maximum path length of {} characters. Invalid path '{}'", CUnixAddress::mc_MaxAddressLength, Address));
			else
				return nullptr;
		}

		CUnixAddress AddressWithPermissions;
		AddressWithPermissions.m_Permissions = Permissions;
		sockaddr_un &AddressUn = AddressWithPermissions.m_UnixAddress;

		AddressUn.sun_family = AF_UNIX;
		NMib::NStr::fg_StrCopy(AddressUn.sun_path, Address, CUnixAddress::mc_MaxAddressLength + 1);
#if !defined(DPlatformFamily_Linux)
		AddressUn.sun_len = sizeof(AddressUn);
#endif
		pAddress->f_Set(AddressWithPermissions);
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
		Result = getaddrinfo("localhost", nullptr, &AddrHint, &pAddresses);

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

void *CPOSIXSocketContext::f_AsyncResolveAddress_Open(const NMib::NStr::CStr &_Address, ::NMib::NNetwork::ENetAddressType _PreferType, NMib::NFunction::TCFunction<void ()> &&_fOnFinish)
{
	return mp_Resolver.f_Open(_Address, _PreferType, fg_Move(_fOnFinish));
}

bool CPOSIXSocketContext::f_AsyncResolveAddress_GetResult(void *_pResolver, CPOSIXAddress*& _opAddress, NMib::NStr::CStr &_Error)
{
	return mp_Resolver.f_GetResult(_pResolver, (NMib::NSys::NNetwork::CAddress&)_opAddress, _Error);
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

#ifdef DPlatformFamily_macOS
#	ifndef s6_addr16
#		define s6_addr16 __u6_addr.__u6_addr16
#	endif
#	ifndef s6_addr32
#		define s6_addr32 __u6_addr.__u6_addr32
#	endif
#endif

NMib::NStr::CStr CPOSIXSocketContext::f_GetAddressString(CPOSIXAddress const &_Address, bool _bIncludeType)
{
	NMib::NStr::CStr AddressStr;

	switch(_Address.f_GetType())
	{
		case NMib::NNetwork::ENetAddressType_TCPv4:
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

		case NMib::NNetwork::ENetAddressType_TCPv6:
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
		case NMib::NNetwork::ENetAddressType_Unix:
			{
				auto &Address = _Address.f_GetUnix();

				using namespace NMib::NFile;

				EFileAttrib Permissions = Address.m_Permissions;

				uint32 UnixPermissions = 0;

				if (Permissions & EFileAttrib_UserExecute)
					UnixPermissions |= 0100;
				if (Permissions & EFileAttrib_UserWrite)
					UnixPermissions |= 0200;
				if (Permissions & EFileAttrib_UserRead)
					UnixPermissions |= 0400;

				if (Permissions & EFileAttrib_GroupExecute)
					UnixPermissions |= 010;
				if (Permissions & EFileAttrib_GroupWrite)
					UnixPermissions |= 020;
				if (Permissions & EFileAttrib_GroupRead)
					UnixPermissions |= 040;

				if (Permissions & EFileAttrib_EveryoneExecute)
					UnixPermissions |= 01;
				if (Permissions & EFileAttrib_EveryoneWrite)
					UnixPermissions |= 02;
				if (Permissions & EFileAttrib_EveryoneRead)
					UnixPermissions |= 04;

				if (_bIncludeType)
				{
					if (UnixPermissions)
						AddressStr += fg_Format("UNIX({nfo,sj3,sf0}):", UnixPermissions);
					else
						AddressStr += "UNIX:";
				}

				AddressStr += Address.f_GetPath();
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

bool CPOSIXSocketContext::fp_GetSocketCreateParams(NMib::NNetwork::ENetAddressType _AddressType, CPOSIXImpSpecificSocketContext::CSocketCreateParams &o_Params)
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

int fg_GetUnixSocketFlags()
{
	int OpenFlags = 0;
#if defined(DPlatformFamily_Linux)
	if (NMib::CSystem::ms_PlatformVersion >= 2'006'027)
		OpenFlags |= SOCK_CLOEXEC;
#endif
	return OpenFlags;
}

void fg_SetUnixSocketOptions(int _File)
{
#if defined(DPlatformFamily_Linux)
	if (NMib::CSystem::ms_PlatformVersion >= 2'006'027)
		return;
#endif
	// Set CloseOnExec so that child processes do not get our open files.
	int FDFlags = fcntl(_File, F_GETFD);
	if (FDFlags != -1)
	{
		FDFlags |= FD_CLOEXEC;

		if (fcntl(_File, F_SETFD, FDFlags) == -1)
		{
			// We let this go deliberately. Nothing overly bad can happen.
		}
	}
	else
	{
		// We let this go deliberately. Nothing overly bad can happen.
	}
}

CPOSIXSocket* CPOSIXSocketContext::fp_Connect
	(
	 	CPOSIXAddress const &_Address
	 	, NMib::NFunction::TCFunctionMovable<void (NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
	 	, CPOSIXAddress const *_pBindAddress
	)
{
	mint Retries = 32;
	int FD;
	bool bConnected;
	while (Retries)
	{
		bConnected = false;
		ENetAddressType AddressType = _Address.f_GetType();

		CPOSIXImpSpecificSocketContext::CSocketCreateParams SocketCreateParams;
		if (!fp_GetSocketCreateParams(AddressType, SocketCreateParams))
			DMibErrorNet("Unsupported address type");

		FD = socket(SocketCreateParams.m_Domain, SocketCreateParams.m_Type | fg_GetUnixSocketFlags(), SocketCreateParams.m_Protocol);

		if (FD == -1)
		{
			int Error = errno;
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("socket (connect)", Error));
		}

		fg_SetUnixSocketOptions(FD);

		auto Cleanup = g_OnScopeExit / [&]
			{
				close(FD);
			}
		;

		if (AddressType == ENetAddressType_TCPv4 || AddressType == ENetAddressType_TCPv6)
		{
			int bNoDelay = 1;

			if (setsockopt(FD, IPPROTO_TCP, TCP_NODELAY, &bNoDelay, sizeof(bNoDelay)) != 0)
			{
				int Error = errno;
				DMibErrorNet(NMib::NPlatform::fg_FormatErrno("setsockopt (connect)", Error));
			}
		}

		if (_pBindAddress)
		{
			int bReuse = 1;
			setsockopt(FD, SOL_SOCKET, SO_REUSEADDR, &bReuse, sizeof(bReuse));

			int Result = bind(FD, (sockaddr const*)_pBindAddress->f_Get(), _pBindAddress->f_GetSockAddrLen());
			if (Result != 0)
			{
				int Error = errno;
				DMibErrorNet(NMib::NPlatform::fg_FormatErrno("bind (connect)", Error));
			}
		}

		int Flags;
		if ((Flags = fcntl(FD, F_GETFL)) == -1 || fcntl(FD, F_SETFL, Flags | O_NONBLOCK) == -1)
		{
			int Error = errno;
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("fcntl (connect set async non blocking)", Error));
		}

		int Result = connect(FD, (sockaddr const*)_Address.f_Get(), _Address.f_GetSockAddrLen());

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
				--Retries;
				continue;
			}
			else
			{
				ENetAddressType AddressType = _Address.f_GetType();
				if (AddressType == ENetAddressType_Unix)
				{
					auto &Unix = _Address.f_GetUnix();
					DMibErrorNet(NMib::NPlatform::fg_FormatErrno(fg_Format("connect ({}, connect)", Unix.f_GetPath()), Error));
				}
				else
					DMibErrorNet(NMib::NPlatform::fg_FormatErrno("connect (connect)", Error));
			}
		}
		else
			bConnected = true;

		Cleanup.f_Clear();

		break;
	}

	auto *pSocket = fp_CreateSocket(FD, bConnected ? EPOSIXSocketMode_Connect : EPOSIXSocketMode_Connecting, EPOSIXSocketEvent_Read | EPOSIXSocketEvent_Write, fg_Move(_fOnStateChange));

	ENetAddressType AddressType = _Address.f_GetType();
	pSocket->m_AddressType = AddressType;

	if (AddressType == ENetAddressType_Unix)
	{
		auto &Unix = _Address.f_GetUnix();
		pSocket->m_PeerUnixFilePath = Unix.f_GetPath();
	}

	return pSocket;
}

void CPOSIXSocketContext::fp_PrepareUnixListen(CPOSIXAddress const &_Address)
{
	if ( _Address.f_GetType() == ENetAddressType_Unix)
	{
		CUnixAddress const &UnixAddress = _Address.f_GetUnix();

		NStr::CStr UnixFilePath = UnixAddress.f_GetPath();
		if (NFile::CFile::fs_FileExists(UnixFilePath))
			NFile::CFile::fs_DeleteFile(UnixFilePath);
		auto Directory = NFile::CFile::fs_GetPath(UnixFilePath);
		if (!NFile::CFile::fs_FileExists(Directory))
		{
			// If user want other permissions it needs to make sure that the directory is created beforehand
			NFile::CFile::fs_CreateDirectory(Directory);
			NFile::CFile::fs_SetAttributes
				(
					Directory
					, NFile::EFileAttrib_UnixAttributesValid
					| NFile::EFileAttrib_UserExecute
					| NFile::EFileAttrib_UserRead
					| NFile::EFileAttrib_UserWrite
				)
			;
		}
	}
}

void CPOSIXSocketContext::fp_SetUnixListenAddress(CPOSIXSocket *_pSocket, CPOSIXAddress const &_Address)
{
	ENetAddressType AddressType = _Address.f_GetType();
	if (AddressType == ENetAddressType_Unix)
	{
		auto &Unix = _Address.f_GetUnix();
		_pSocket->m_UnixFilePath = Unix.f_GetPath();
	}
	_pSocket->m_AddressType = AddressType;
}

CPOSIXSocket* CPOSIXSocketContext::f_AsyncConnect
	(
	 	CPOSIXAddress const &_Address
	 	, NMib::NFunction::TCFunctionMovable<void (NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
	 	, CPOSIXAddress const *_pBindAddress
	)
{
	return fp_Connect(_Address, fg_Move(_fOnStateChange), _pBindAddress);
}

void CPOSIXSocketContext::f_StartSocket(CPOSIXSocket *_pSocket)
{
	mp_PollerThread.mp_Poller.f_RegisterSocket(_pSocket);
}

CPOSIXSocket* CPOSIXSocketContext::f_Listen
	(
	 	CPOSIXAddress const &_Address
	 	, NMib::NFunction::TCFunctionMovable<void (NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
	 	, NMib::NNetwork::ENetFlag _Flags
	)
{
	ENetAddressType AddressType = _Address.f_GetType();

	CPOSIXImpSpecificSocketContext::CSocketCreateParams SocketCreateParams;
	if (!fp_GetSocketCreateParams(AddressType, SocketCreateParams))
		DMibErrorNet("Unsupported address type");

	fp_PrepareUnixListen(_Address);

	int FD = socket(SocketCreateParams.m_Domain, SocketCreateParams.m_Type | fg_GetUnixSocketFlags(), SocketCreateParams.m_Protocol);

	if (FD == -1)
	{
		int Error = errno;
		DMibErrorNet(NMib::NPlatform::fg_FormatErrno("socket (listen)", Error));
	}

	fg_SetUnixSocketOptions(FD);

	auto Cleanup = g_OnScopeExit / [&]
		{
			close(FD);
		}
	;

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

	{
		int bReuse = 1;
		setsockopt(FD, SOL_SOCKET, SO_REUSEADDR, &bReuse, sizeof(bReuse));
	}

#ifdef DPlatformFamily_macOS
	if (_Flags & NNetwork::ENetFlag_ReusePort)
	{
		int bReuse = 1;
		setsockopt(FD, SOL_SOCKET, SO_REUSEPORT, &bReuse, sizeof(bReuse));
	}
#endif

	int Result = bind(FD, (sockaddr const*)_Address.f_Get(), _Address.f_GetSockAddrLen());

	if (Result != 0)
	{
		int Error = errno;
		DMibErrorNet(NMib::NPlatform::fg_FormatErrno("bind (listen)", Error));
	}

	if (AddressType == ENetAddressType_Unix)
	{
		auto &UnixAddress = _Address.f_GetUnix();
		if (UnixAddress.m_Permissions)
			NMib::NFile::CFile::fs_SetAttributes(UnixAddress.f_GetPath(), UnixAddress.m_Permissions | NFile::EFileAttrib_UnixAttributesValid);
	}

	Result = listen(FD, SOMAXCONN);

	if (Result != 0)
	{
		int Error = errno;
		DMibErrorNet(NMib::NPlatform::fg_FormatErrno("listen (listen)", Error));
	}

	Cleanup.f_Clear();

	auto pSocket = fp_CreateSocket(FD, EPOSIXSocketMode_Listen, EPOSIXSocketEvent_Read, fg_Move(_fOnStateChange));
	fp_SetUnixListenAddress(pSocket, _Address);
	return pSocket;
}

CPOSIXSocket* CPOSIXSocketContext::f_ListenDatagram
	(
	 	CPOSIXAddress const &_Address
	 	, NMib::NFunction::TCFunctionMovable<void (NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
	 	, NMib::NNetwork::ENetFlag _Flags
	)
{
	ENetAddressType AddressType = _Address.f_GetType();

	CPOSIXImpSpecificSocketContext::CSocketCreateParams SocketCreateParams;
	if (!fp_GetSocketCreateParams(AddressType, SocketCreateParams))
		DMibErrorNet("Unsupported address type");

	fp_PrepareUnixListen(_Address);

	int FD = socket(SocketCreateParams.m_Domain, SocketCreateParams.m_Type | fg_GetUnixSocketFlags(), SocketCreateParams.m_Protocol);

	if (FD == -1)
	{
		int Error = errno;
		DMibErrorNet(NMib::NPlatform::fg_FormatErrno("socket (listen)", Error));
	}

	fg_SetUnixSocketOptions(FD);

	auto Cleanup = g_OnScopeExit / [&]
		{
			close(FD);
		}
	;

	{
		int Flags;
		if ((Flags = fcntl(FD, F_GETFL)) == -1 || fcntl(FD, F_SETFL, Flags | O_NONBLOCK) == -1)
		{
			int Error = errno;
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("fcntl (listen set non blocking)", Error));
		}
	}
	{
		int bReuse = 1;
		setsockopt(FD, SOL_SOCKET, SO_REUSEADDR, &bReuse, sizeof(bReuse));
	}

#ifdef DPlatformFamily_macOS
	if (_Flags & NNetwork::ENetFlag_ReusePort)
	{
		int bReuse = 1;
		setsockopt(FD, SOL_SOCKET, SO_REUSEPORT, &bReuse, sizeof(bReuse));
	}
#endif

	int Result = bind(FD, (sockaddr const*)_Address.f_Get(), _Address.f_GetSockAddrLen());

	if (Result != 0)
	{
		int Error = errno;
		DMibErrorNet(NMib::NPlatform::fg_FormatErrno("bind (listen)", Error));
	}

	if (AddressType == ENetAddressType_Unix)
	{
		auto &UnixAddress = _Address.f_GetUnix();
		if (UnixAddress.m_Permissions)
			NMib::NFile::CFile::fs_SetAttributes(UnixAddress.f_GetPath(), UnixAddress.m_Permissions | NFile::EFileAttrib_UnixAttributesValid);
	}

	Cleanup.f_Clear();

	auto pSocket = fp_CreateSocket(FD, EPOSIXSocketMode_Datagram, EPOSIXSocketEvent_Read | EPOSIXSocketEvent_Write, fg_Move(_fOnStateChange));

	fp_SetUnixListenAddress(pSocket, _Address);

	pSocket->m_BindAddressSize = _Address.f_GetSockAddrLen();

	return pSocket;
}

CPOSIXSocket* CPOSIXSocketContext::f_Accept(CPOSIXSocket *_pSocket, NMib::NFunction::TCFunctionMovable<void (NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange)
{
	int ResultFD;
#ifdef DPlatformFamily_Linux
	if (NLocal::g_f_accept4)
	{
		ResultFD = NLocal::g_f_accept4(_pSocket->m_FD, NULL, NULL, SOCK_CLOEXEC);
	}
	else
#endif
		ResultFD = accept(_pSocket->m_FD, NULL, NULL);

	if (ResultFD == -1)
	{
		int Error = errno;
		if (Error == EAGAIN || Error == EWOULDBLOCK)
			return nullptr;

		DMibErrorNet(NMib::NPlatform::fg_FormatErrno("accept", Error));
	}

	if (_pSocket->m_AddressType == ENetAddressType_TCPv4 || _pSocket->m_AddressType == ENetAddressType_TCPv6)
	{
		int bNoDelay = 1;

		if (setsockopt(ResultFD, IPPROTO_TCP, TCP_NODELAY, &bNoDelay, sizeof(bNoDelay)) != 0)
		{
			int Error = errno;
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("setsockopt (accept)", Error));
		}
	}

	fg_SetUnixSocketOptions(ResultFD);

	{
		int Flags;
		if ((Flags = fcntl(ResultFD, F_GETFL)) == -1 || fcntl(ResultFD, F_SETFL, Flags | O_NONBLOCK) == -1)
		{
			close(ResultFD);

			int Error = errno;
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("fcntl (accept set non blocking)", Error));
		}
	}

	auto *pSocket = fp_CreateSocket(ResultFD, EPOSIXSocketMode_Connect, EPOSIXSocketEvent_Read | EPOSIXSocketEvent_Write, fg_Move(_fOnStateChange));
	pSocket->m_AddressType = _pSocket->m_AddressType;

	{
		sockaddr_storage SocketName;
		socklen_t nAddrBytes = sizeof(SocketName);
		int Ret = getsockname(_pSocket->m_FD, (struct sockaddr *)&SocketName, &nAddrBytes);
		if (Ret == 0 && SocketName.ss_family == AF_UNIX)
		{
			auto &Unix = *((sockaddr_un *)&SocketName);
			pSocket->m_PeerUnixFilePath = (ch8 const *)Unix.sun_path;
		}
	}

	return pSocket;
}

void CPOSIXSocketContext::f_SetOnStateChange(CPOSIXSocket* _pSocket, NMib::NFunction::TCFunctionMovable<void (NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange)
{
	{
		DMibLock(_pSocket->m_Lock);
		_pSocket->m_fOnStateChange = fg_Move(_fOnStateChange);
	}
}

bool CPOSIXSocketContext::f_Close(CPOSIXSocket* _pSocket)
{
	if (_pSocket->m_FD != -1)
	{
		mp_PollerThread.mp_Poller.f_DeregisterSocket(_pSocket);

		{
			DMibLock(_pSocket->m_Lock);
			_pSocket->m_fOnStateChange.f_Clear();
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
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("shutdown", errno));
	}
	else
	{
		DMibLock(_pSocket->m_Lock);
		_pSocket->m_bShutdownCalled = true;
	}
}

NMib::NNetwork::ENetTCPState CPOSIXSocketContext::f_GetState(CPOSIXSocket *_pSocket)
{
	return (NMib::NNetwork::ENetTCPState)_pSocket->m_StateAtomic.f_Exchange(0);
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
	int Flags = 0;
#ifdef DPlatformFamily_Linux
	Flags |= MSG_NOSIGNAL;
#endif
	int Result = send(_pSocket->m_FD, _pData, _DataLen, Flags);

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
	int Flags = 0;
#ifdef DPlatformFamily_Linux
	Flags |= MSG_NOSIGNAL;
#endif
	int Result = sendto(_pSocket->m_FD, _pData, _DataLen, Flags, (sockaddr const*)_Address.f_Get(), _Address.f_GetSockAddrLen());

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
	int Result = recvfrom(_pSocket->m_FD, _pData, _DataLen, 0, (sockaddr *)_Address.f_GetForWrite(_pSocket->m_AddressType, Len), &Len);

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
	bool bGracefulClose = false;

	{
		DMibLock(_pSocket->m_Lock);
		CloseReason = _pSocket->m_CloseError;
		bGracefulClose = _pSocket->m_bShutdownCalled && _pSocket->m_bNonErrorClose;
	}

	if (CloseReason == 0)
	{
		if (bGracefulClose)
			return "Connection gracefully disconnected";
		else
			return "End of file encountered";
	}

	return NMib::NPlatform::fg_FormatErrno("", CloseReason);
}

CPOSIXSocket* CPOSIXSocketContext::f_InheritHandle2(void* _pOSSocket, NMib::NFunction::TCFunctionMovable<void (NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange)
{
	return fp_CreateSocket(int(aint(_pOSSocket)), EPOSIXSocketMode_Connect, EPOSIXSocketEvent_Read | EPOSIXSocketEvent_Write, fg_Move(_fOnStateChange), true);
}

void *CPOSIXSocketContext::f_GiveUpForInherit(CPOSIXSocket *_pSocket)
{
	int FD = -1;

	mp_PollerThread.mp_Poller.f_DeregisterSocket(_pSocket);

	{
		DMibLock(_pSocket->m_Lock);
		_pSocket->m_fOnStateChange.f_Clear();
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
		NStorage::TCUniquePointer<CPOSIXAddress> pAddress = fg_Construct(*(sockaddr_in const*)&PeerAddr);
		return pAddress.f_Detach();
	}
	else if (PeerAddr.ss_family == AF_INET6)
	{
		NStorage::TCUniquePointer<CPOSIXAddress> pAddress = fg_Construct(*(sockaddr_in6 const*)&PeerAddr);
		return pAddress.f_Detach();
	}
	else if (PeerAddr.ss_family == AF_UNIX)
	{
		auto UnixAddress = *(sockaddr_un const*)&PeerAddr;
		if (nAddrBytes <= sizeof(UnixAddress.sun_family))
			UnixAddress.sun_path[0] = 0;
		if (fg_StrLen((ch8 const *)UnixAddress.sun_path) == 0 && !_pSocket->m_PeerUnixFilePath.f_IsEmpty())
		{

			CUnixAddress Address;
			Address.m_UnixAddress.sun_family = AF_UNIX;
#if !defined(DPlatformFamily_Linux)
			Address.m_UnixAddress.sun_len = sizeof(Address.m_UnixAddress);
#endif
			fg_StrCopy(Address.m_UnixAddress.sun_path, _pSocket->m_PeerUnixFilePath, CUnixAddress::mc_MaxAddressLength + 1);

			NStorage::TCUniquePointer<CPOSIXAddress> pAddress = fg_Construct(Address);
			return pAddress.f_Detach();
		}

		CUnixAddress Address;
		Address.m_UnixAddress = UnixAddress;
		NStorage::TCUniquePointer<CPOSIXAddress> pAddress = fg_Construct(Address);
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

CPOSIXSocket* CPOSIXSocketContext::fp_CreateSocket
	(
	 	int _FD
	 	, EPOSIXSocketMode _Mode
	 	, EPOSIXSocketEvent _Events
	 	, NMib::NFunction::TCFunctionMovable<void (NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
	 	, bool _bFromInherit
	)
{
#ifdef DPlatformFamily_macOS
	{
		// Disable sigpipe from being sent to program. We handle this in exceptions
		int set = 1;
		setsockopt(_FD, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
	}
#endif

	NMib::NStorage::TCUniquePointer<CPOSIXSocket> pNewSocket = fg_Construct(_FD, _Mode, _Events, fg_Move(_fOnStateChange));

	if (_bFromInherit)
	{
		pNewSocket->m_bInitialWriteNotification = false;
	}

	NMib::NNetwork::ENetTCPState StateAdded = NMib::NNetwork::ENetTCPState_Read | NMib::NNetwork::ENetTCPState_Write; // Kickstart

	if (_Mode == EPOSIXSocketMode_Connect)
		StateAdded |= NMib::NNetwork::ENetTCPState_Connected;
	else if (_Mode == EPOSIXSocketMode_Connecting)
		pNewSocket->m_bInitialWriteNotification = false;

	pNewSocket->m_StateAtomic.f_FetchOr(StateAdded);

	return pNewSocket.f_Detach();
}

#include "Malterlib_Core_PlatformImp_Net.imp.h"
