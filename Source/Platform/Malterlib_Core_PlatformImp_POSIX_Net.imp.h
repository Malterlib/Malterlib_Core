// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_PlatformImp_POSIX_Net.h"
#include <Mib/Process/Platform>

#include <netinet/tcp.h>
#include <sys/uio.h>
#include <sys/stat.h>

#if defined(DPlatformFamily_Linux)
	#include <sys/stat.h>
	#include <sys/vfs.h>
	#include <sys/syscall.h>
	#include <fcntl.h>
	#include <unistd.h>
	#include <errno.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>

	// SO_PEERPIDFD (Linux 6.5) returns a race-free pidfd pinning the connected unix socket peer
	// process. On pidfs kernels (Linux 6.9) pidfds carry a boot-unique inode naming the exact kernel
	// process object; PID_FS_MAGIC distinguishes that from the older anonymous-inode pidfds whose
	// inode numbers are shared and identify nothing. The values may be missing from the build
	// headers even when the running kernel is newer, so define the ones that are absent
	#ifndef SO_PEERPIDFD
		#define SO_PEERPIDFD 77
	#endif
	#ifndef PID_FS_MAGIC
		#define PID_FS_MAGIC 0x50494446
	#endif
	#ifndef SYS_pidfd_open
		#define SYS_pidfd_open 434
	#endif

namespace
{
	// Fills the pidfs identity of the process behind _PidFD. Returns false on kernels older than
	// 6.9 where pidfds share one anonymous inode and comparing the numbers would identify nothing
	bool fg_GetPidFSIdentity(int _PidFD, uint64 &o_Device, uint64 &o_Inode)
	{
		struct statfs FSInfo;
		if (fstatfs(_PidFD, &FSInfo) != 0)
			return false;

		if (FSInfo.f_type != PID_FS_MAGIC)
			return false;

		struct stat PidStat;
		if (fstat(_PidFD, &PidStat) != 0)
			return false;

		o_Device = uint64(PidStat.st_dev);
		o_Inode = uint64(PidStat.st_ino);

		return true;
	}

	// The peer pid read through the pidfd's own fdinfo: "Pid:" is the peer's pid as currently
	// resolvable in this pid namespace (-1 once the process is reaped, which is the same instant
	// its number becomes reusable, and 0 when the peer is not visible here), and "NSpid:" lists one
	// entry per namespace level from here down, so a second entry means the peer is in a descendant
	// namespace. The pidfd pins the process object, so unlike a /proc/<pid> lookup this cannot race
	// with pid-number reuse. Requires /proc to be mounted for this pid namespace
	bool fg_GetPeerSameNamespacePid(int _PidFD, pid_t &o_PeerPID)
	{
		char pPath[64];
		snprintf(pPath, sizeof(pPath), "/proc/self/fdinfo/%d", _PidFD);

		int FDInfoFD = open(pPath, O_RDONLY | O_CLOEXEC);
		if (FDInfoFD < 0)
			return false;

		auto CleanupFDInfoFD = g_OnScopeExit / [&]
			{
				close(FDInfoFD);
			}
		;

		char pBuffer[4096];
		auto nBytes = read(FDInfoFD, pBuffer, sizeof(pBuffer) - 1);
		if (nBytes <= 0)
			return false;

		pBuffer[nBytes] = 0;

		char const *pPid = strstr(pBuffer, "\nPid:");
		if (!pPid)
			return false;

		long PeerPID = strtol(pPid + 5, nullptr, 10);
		if (PeerPID <= 0)
			return false;

		// An absent NSpid line means the kernel is built without pid namespaces, so same-namespace
		// holds trivially
		if (char const *pNSPid = strstr(pBuffer, "\nNSpid:"))
		{
			char *pEnd = nullptr;
			long FirstNSPid = strtol(pNSPid + 7, &pEnd, 10);
			if (FirstNSPid != PeerPID)
				return false;

			while (pEnd && (*pEnd == ' ' || *pEnd == '\t'))
				++pEnd;
			if (pEnd && *pEnd >= '0' && *pEnd <= '9')
				return false;
		}

		o_PeerPID = pid_t(PeerPID);

		return true;
	}
}
#endif

CPOSIXSocketContext::CPOSIXSocketContext()
{
	mp_PollerThread.f_Start(EExecutionPriority_Highest);
	signal(SIGPIPE, SIG_IGN);
}

CPOSIXSocketContext::~CPOSIXSocketContext()
{
	mp_PollerThread.f_Stop(true);
}

CPOSIXAddress* CPOSIXSocketContext::f_CreateAddress(NMib::NNetwork::ENetAddressType _Type, void const* _pData, umint _nDataBytes)
{
	switch(_Type)
	{
	case NMib::NNetwork::ENetAddressType_TCPv4:
		{
			if (_nDataBytes != sizeof(NMib::NNetwork::CNetAddressTCPv4))
				return nullptr;

			sockaddr_in NativeAddr;
			fp_ToNative(*(NMib::NNetwork::CNetAddressTCPv4*)_pData, NativeAddr);

			return fg_ConstructObject<CPOSIXAddress>(NMemory::CDefaultAllocator(), NativeAddr);
		}
		break;
	case NMib::NNetwork::ENetAddressType_TCPv6:
		{
			if (_nDataBytes != sizeof(NMib::NNetwork::CNetAddressTCPv6))
				return nullptr;

			sockaddr_in6 NativeAddr;
			fp_ToNative(*(NMib::NNetwork::CNetAddressTCPv6*)_pData, NativeAddr);

			return fg_ConstructObject<CPOSIXAddress>(NMemory::CDefaultAllocator(), NativeAddr);
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

bool CPOSIXSocketContext::f_GetAddressRaw(CPOSIXAddress const &_Address, NMib::NNetwork::ENetAddressType _ExpectedType, void* _opRawData, umint _nDataBytes)
{
	NMib::NNetwork::ENetAddressType Type = _Address.f_GetType();

	if (Type != _ExpectedType)
		return false;

	switch(Type)
	{

		case NMib::NNetwork::ENetAddressType_TCPv4:
			{
				if (_nDataBytes != sizeof(NMib::NNetwork::CNetAddressTCPv4))
					return false;

				NMib::NNetwork::CNetAddressTCPv4& Addr = *(NMib::NNetwork::CNetAddressTCPv4*)_opRawData;
				fp_FromNative(_Address.f_GetTCPv4(), Addr);

				return true;
			}
			break;

		case NMib::NNetwork::ENetAddressType_TCPv6:
			{
				if (_nDataBytes != sizeof(NMib::NNetwork::CNetAddressTCPv6))
					return false;

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

CPOSIXAddress* CPOSIXSocketContext::f_SetAddressRaw(CPOSIXAddress* _pAddress, ::NMib::NNetwork::ENetAddressType _Type, void const* _pRawData, umint _nDataBytes)
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
		auto Address = CUnixAddress::fs_Parse(_Address, _bThrowOnError);
		if (!Address)
			return nullptr;

		pAddress->f_Set(fg_Move(*Address));
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

void *CPOSIXSocketContext::f_AsyncResolveAddress_Open(const NMib::NStr::CStr &_Address, ::NMib::NNetwork::ENetAddressType _PreferType, NMib::NFunction::TCFunctionMutable<void ()> &&_fOnFinish)
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
	if (!_pAddress)
		return;

	fg_DeleteObject(NMemory::CDefaultAllocator(), _pAddress);
}

#ifdef DPlatformFamily_macOS
#	ifndef s6_addr16
#		define s6_addr16 __u6_addr.__u6_addr16
#	endif
#	ifndef s6_addr32
#		define s6_addr32 __u6_addr.__u6_addr32
#	endif
#endif

NMib::NStr::CStr CPOSIXSocketContext::f_GetAddressString(CPOSIXAddress const &_Address, ENetAddressStringFlag _Flags)
{
	using namespace NMib::NStr;

	CStr AddressStr;

	switch(_Address.f_GetType())
	{
		case NMib::NNetwork::ENetAddressType_TCPv4:
			{
				auto &Address = _Address.f_GetTCPv4();

				if (_Flags & ENetAddressStringFlag_IncludeType)
					AddressStr += "TCPv4:";

				AddressStr
					+= "{}.{}.{}.{}"_f
					<< ((ntohl(Address.sin_addr.s_addr) >> 24) & 0xFF)
					<< ((ntohl(Address.sin_addr.s_addr) >> 16) & 0xFF)
					<< ((ntohl(Address.sin_addr.s_addr) >> 8) & 0xFF)
					<< ((ntohl(Address.sin_addr.s_addr) >> 0) & 0xFF)
				;

				if (_Flags & ENetAddressStringFlag_IncludePort)
					AddressStr += ":{}"_f << ntohs(Address.sin_port);
			}
			break;

		case NMib::NNetwork::ENetAddressType_TCPv6:
			{
				auto &Address = _Address.f_GetTCPv6();
				if (_Flags & ENetAddressStringFlag_IncludeType)
					AddressStr += "TCPv6:";

				AddressStr
					+= "[{nfh,sj4,sf0}:{nfh,sj4,sf0}:{nfh,sj4,sf0}:{nfh,sj4,sf0}:{nfh,sj4,sf0}:{nfh,sj4,sf0}:{nfh,sj4,sf0}:{nfh,sj4,sf0}]"_f
					<< ntohs(Address.sin6_addr.s6_addr16[0])
					<< ntohs(Address.sin6_addr.s6_addr16[1])
					<< ntohs(Address.sin6_addr.s6_addr16[2])
					<< ntohs(Address.sin6_addr.s6_addr16[3])
					<< ntohs(Address.sin6_addr.s6_addr16[4])
					<< ntohs(Address.sin6_addr.s6_addr16[5])
					<< ntohs(Address.sin6_addr.s6_addr16[6])
					<< ntohs(Address.sin6_addr.s6_addr16[7])
				;

				if (_Flags & ENetAddressStringFlag_IncludePort)
					AddressStr += ":{}"_f << ntohs(Address.sin6_port);
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

				if (_Flags & ENetAddressStringFlag_IncludeType)
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
				if (_Flags & ENetAddressStringFlag_IncludeType)
					AddressStr += "Kernel:";

				AddressStr += "{}.{}.{}.{}"_f << (Address.sin_addr.s_addr & 0xFF);
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
	OpenFlags |= SOCK_CLOEXEC;
#endif
	return OpenFlags;
}

void fg_SetUnixSocketOptions(int _File)
{
#if defined(DPlatformFamily_Linux)
	// SOCK_CLOEXEC is passed at socket creation via fg_GetUnixSocketFlags
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
	umint Retries = 32;
	int FD;
	bool bConnected;
	while (Retries)
	{
		bConnected = false;
		ENetAddressType AddressType = _Address.f_GetType();

		CPOSIXImpSpecificSocketContext::CSocketCreateParams SocketCreateParams;
		if (!fp_GetSocketCreateParams(AddressType, SocketCreateParams))
			DMibErrorNet("Unsupported address type");

		{
			auto SocketFlags = fg_GetUnixSocketFlags();
			if (!SocketFlags)
				NMib::NPlatform::fg_ForkLock().f_Lock();
			auto CleanupLock = g_OnScopeExit / [SocketFlags]
				{
					if (!SocketFlags)
						NMib::NPlatform::fg_ForkLock().f_Unlock();
				}
			;

			FD = socket(SocketCreateParams.m_Domain, SocketCreateParams.m_Type | SocketFlags, SocketCreateParams.m_Protocol);

			if (FD == -1)
			{
				int Error = errno;
				DMibErrorNet(NMib::NPlatform::fg_FormatErrno("socket (connect)", Error));
			}

			fg_SetUnixSocketOptions(FD);
		}
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

		// Remove a stale socket file from a previous listener. A file already gone is success:
		// a concurrent teardown of the previous socket can unlink it between any existence
		// check and the unlink, so ENOENT must not fail the new listen
		NStr::CStr UnixFilePath = UnixAddress.f_GetPath();
		NStr::CStr PosixPath = fg_ConvertToPOSIXPath(UnixFilePath);
		if (unlink(PosixPath.f_GetStr()) != 0 && errno != ENOENT)
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno(NStr::CStr::CFormat("unlink('{}') when removing stale unix socket") << UnixFilePath, errno));
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

	int FD;
	{
		auto SocketFlags = fg_GetUnixSocketFlags();
		if (!SocketFlags)
			NMib::NPlatform::fg_ForkLock().f_Lock();
		auto CleanupLock = g_OnScopeExit / [SocketFlags]
			{
				if (!SocketFlags)
					NMib::NPlatform::fg_ForkLock().f_Unlock();
			}
		;

		FD = socket(SocketCreateParams.m_Domain, SocketCreateParams.m_Type | SocketFlags, SocketCreateParams.m_Protocol);

		if (FD == -1)
		{
			int Error = errno;
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("socket (listen)", Error));
		}

		fg_SetUnixSocketOptions(FD);
	}

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

	int FD;
	{
		auto SocketFlags = fg_GetUnixSocketFlags();
		if (!SocketFlags)
			NMib::NPlatform::fg_ForkLock().f_Lock();
		auto CleanupLock = g_OnScopeExit / [SocketFlags]
			{
				if (!SocketFlags)
					NMib::NPlatform::fg_ForkLock().f_Unlock();
			}
		;

		FD = socket(SocketCreateParams.m_Domain, SocketCreateParams.m_Type | SocketFlags, SocketCreateParams.m_Protocol);

		if (FD == -1)
		{
			int Error = errno;
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("socket (listen)", Error));
		}

		fg_SetUnixSocketOptions(FD);
	}

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

		if (ResultFD == -1)
		{
			int Error = errno;
			if (Error == EAGAIN || Error == EWOULDBLOCK)
				return nullptr;

			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("accept", Error));
		}
	}
	else
#endif
	{
		DMibLock(NMib::NPlatform::fg_ForkLock());

		ResultFD = accept(_pSocket->m_FD, NULL, NULL);

		if (ResultFD == -1)
		{
			int Error = errno;
			if (Error == EAGAIN || Error == EWOULDBLOCK)
				return nullptr;

			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("accept", Error));
		}

		fg_SetUnixSocketOptions(ResultFD);
	}

	auto Cleanup = g_OnScopeExit / [&]
		{
			close(ResultFD);
		}
	;

	if (_pSocket->m_AddressType == ENetAddressType_TCPv4 || _pSocket->m_AddressType == ENetAddressType_TCPv6)
	{
		int bNoDelay = 1;

		if (setsockopt(ResultFD, IPPROTO_TCP, TCP_NODELAY, &bNoDelay, sizeof(bNoDelay)) != 0)
		{
			int Error = errno;
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("setsockopt (accept)", Error));
		}
	}

	{
		int Flags;
		if ((Flags = fcntl(ResultFD, F_GETFL)) == -1 || fcntl(ResultFD, F_SETFL, Flags | O_NONBLOCK) == -1)
		{
			int Error = errno;
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("fcntl (accept set non blocking)", Error));
		}
	}

	Cleanup.f_Clear();

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

#if defined(DPlatformFamily_Linux)
	if (_pSocket->m_LocalPidFD != -1)
		close(_pSocket->m_LocalPidFD);
	if (_pSocket->m_PeerPidFD != -1)
		close(_pSocket->m_PeerPidFD);
#endif

	fg_DeleteObject(NMemory::CDefaultAllocator(), _pSocket);

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

umint CPOSIXSocketContext::f_Receive(CPOSIXSocket *_pSocket, void *_pData, umint _DataLen)
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

umint CPOSIXSocketContext::f_Send(CPOSIXSocket *_pSocket, const void *_pData, umint _DataLen)
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

umint CPOSIXSocketContext::f_SendVectored(CPOSIXSocket *_pSocket, NMib::NSys::CIoSpan const *_pSpans, umint _nSpans)
{
	constexpr umint c_MaxVectors = 64;
	iovec IoVectors[c_MaxVectors];
	umint nVectors = 0;
	for (umint iSpan = 0; iSpan < _nSpans && nVectors < c_MaxVectors; ++iSpan)
	{
		if (!_pSpans[iSpan].m_nBytes)
			continue;

		IoVectors[nVectors].iov_base = (void *)_pSpans[iSpan].m_pData;
		IoVectors[nVectors].iov_len = _pSpans[iSpan].m_nBytes;
		++nVectors;
	}

	if (!nVectors)
		return 0;

	msghdr Header = {};
	Header.msg_iov = IoVectors;
	Header.msg_iovlen = (decltype(Header.msg_iovlen))nVectors;

	int Flags = 0;
#ifdef DPlatformFamily_Linux
	Flags |= MSG_NOSIGNAL;
#endif
	auto Result = sendmsg(_pSocket->m_FD, &Header, Flags);

	if (Result == -1)
	{
		if (errno == EAGAIN)
		{
			Result = 0;
		}
		else
		{
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("sendmsg (send to socket)", errno));
		}
	}

	return Result;
}

umint CPOSIXSocketContext::f_SendDatagram(CPOSIXSocket *_pSocket, CPOSIXAddress const &_Address, const void *_pData, umint _DataLen)
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

umint CPOSIXSocketContext::f_ReceiveDatagram(CPOSIXSocket *_pSocket, CPOSIXAddress &_Address, void *_pData, umint _DataLen)
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

	return (void*)(umint)FD;
}

void *CPOSIXSocketContext::f_GetOSSocket(CPOSIXSocket *_pSocket)
{
	return (void*)(umint)_pSocket->m_FD;
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

bool CPOSIXSocketContext::f_GetProcessIdentity(CPOSIXSocket *_pSocket, NMib::NSys::NNetwork::CProcessIdentity &o_LocalIdentity, NMib::NSys::NNetwork::CProcessIdentity &o_PeerIdentity)
{
	// The pidfs fields are only assigned on the pidfs path, so clear the outputs up front: stale
	// values in a reused output must never masquerade as an exact identity
	o_LocalIdentity = {};
	o_PeerIdentity = {};

	// Only a connected unix domain socket carries a kernel-authenticated peer process id
	sockaddr_storage PeerAddr;
	socklen_t nAddrBytes = sizeof(PeerAddr);

	if (getpeername(_pSocket->m_FD, (struct sockaddr *)&PeerAddr, &nAddrBytes) != 0)
		return false;

	if (PeerAddr.ss_family != AF_UNIX)
		return false;

	pid_t LocalPID = getpid();
	pid_t PeerPID = 0;

#if defined(DPlatformFamily_Linux)
	// SO_PEERCRED reports the credentials captured when the peer's listening socket called listen
	// (or when the peer called connect). A listener handed to another process (pre-fork accept,
	// socket activation) therefore advertises the listen caller's pid, which the accepting process
	// cannot honestly sign, and the authenticated unix handshake fails closed by design for such
	// topologies; they must use the TLS transport instead (see the CSocket_AuthenticatedUnix
	// documentation). This function only reports the kernel facts
	struct ucred Credentials;
	socklen_t nCredentialBytes = sizeof(Credentials);

	if (getsockopt(_pSocket->m_FD, SOL_SOCKET, SO_PEERCRED, &Credentials, &nCredentialBytes) != 0)
		return false;

	if (nCredentialBytes != sizeof(Credentials))
		return false;

	PeerPID = Credentials.pid;
#else
	pid_t PeerPIDValue = 0;
	socklen_t nPeerPIDBytes = sizeof(PeerPIDValue);

	if (getsockopt(_pSocket->m_FD, SOL_LOCAL, LOCAL_PEERPID, &PeerPIDValue, &nPeerPIDBytes) != 0)
		return false;

	if (nPeerPIDBytes != sizeof(PeerPIDValue))
		return false;

	PeerPID = PeerPIDValue;
#endif

	if (LocalPID <= 0)
		return false;

#if defined(DPlatformFamily_Linux)
	// SO_PEERCRED translates the peer pid into this namespace (0 when the peer is not visible
	// here), so the numeric pid is only a genuine identity once the peer is known to share this pid
	// namespace. SO_PEERPIDFD pins the connect-time peer process: on pidfs kernels its boot-unique
	// inode names the exact kernel process object, independent of pid namespaces and pid-number
	// recycling, so the handshake binds against that and cross-namespace peers are supported. On
	// 6.5 up to before 6.9 the pidfd's fdinfo confirms the peer shares this namespace and supplies
	// its current pid. A kernel without SO_PEERPIDFD (ENOPROTOOPT) leaves the numeric binding
	// standing alone with the namespace hole as an accepted risk; once the capability is present,
	// any failure fails closed rather than silently downgrading
	int PeerPidFD = -1;
	socklen_t nPeerPidFDBytes = sizeof(PeerPidFD);
	if (getsockopt(_pSocket->m_FD, SOL_SOCKET, SO_PEERPIDFD, &PeerPidFD, &nPeerPidFDBytes) != 0)
	{
		if (errno != ENOPROTOOPT)
			return false;

		if (PeerPID <= 0)
			return false;
	}
	else
	{
		if (PeerPidFD < 0)
			return false;

		// The pidfds are kept open on the socket for the connection lifetime rather than closed
		// here: they pin both process objects, and a 32-bit kernel recycles a process's pidfs inode
		// once its last pidfd closes, so releasing them before the peer completes its own identity
		// checks could make the same process present two different identities
		if (_pSocket->m_PeerPidFD != -1)
			close(_pSocket->m_PeerPidFD);
		_pSocket->m_PeerPidFD = PeerPidFD;

		uint64 PeerDevice = 0;
		uint64 PeerInode = 0;
		if (fg_GetPidFSIdentity(PeerPidFD, PeerDevice, PeerInode))
		{
			// Both endpoints run the same kernel, so the local side always has pidfs when the peer
			// side does
			int LocalPidFD = int(syscall(SYS_pidfd_open, LocalPID, 0));
			if (LocalPidFD < 0)
				return false;

			if (_pSocket->m_LocalPidFD != -1)
				close(_pSocket->m_LocalPidFD);
			_pSocket->m_LocalPidFD = LocalPidFD;

			uint64 LocalDevice = 0;
			uint64 LocalInode = 0;
			if (!fg_GetPidFSIdentity(LocalPidFD, LocalDevice, LocalInode))
				return false;

			o_LocalIdentity.m_PidFSDevice = LocalDevice;
			o_LocalIdentity.m_PidFSInode = LocalInode;
			o_PeerIdentity.m_PidFSDevice = PeerDevice;
			o_PeerIdentity.m_PidFSInode = PeerInode;
		}
		else
		{
			pid_t FDInfoPeerPID = 0;
			if (!fg_GetPeerSameNamespacePid(PeerPidFD, FDInfoPeerPID))
				return false;

			PeerPID = FDInfoPeerPID;
		}
	}
#else
	if (PeerPID <= 0)
		return false;
#endif

	o_LocalIdentity.m_ProcessID = uint64(LocalPID);
	o_PeerIdentity.m_ProcessID = PeerPID > 0 ? uint64(PeerPID) : 0;

	return true;
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
