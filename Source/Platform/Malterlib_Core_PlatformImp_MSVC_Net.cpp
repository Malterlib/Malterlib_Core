// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_PlatformImp_MSVC_Net.h"
#include "Malterlib_Core_Platform_Windows.h"
#include "Malterlib_Core_Platform_Windows_IoLoop_Iocp_Internal.h"

#include <AclAPI.h>
#include <mstcpip.h>

// Apply Unix socket permission flags as Windows ACLs on the socket file
static void fg_ApplyUnixSocketPermissions(CUnixAddress const &_UnixAddress)
{
	if (!_UnixAddress.m_Permissions)
		return;

	using namespace NMib::NStr;
	using namespace NMib::NFile;

	// Map Unix "everyone" permission flags to Windows generic access rights
	DWORD EveryoneAccess = 0;
	if (_UnixAddress.m_Permissions & EFileAttrib_EveryoneRead)
		EveryoneAccess |= GENERIC_READ;
	if (_UnixAddress.m_Permissions & EFileAttrib_EveryoneWrite)
		EveryoneAccess |= GENERIC_WRITE;
	if (_UnixAddress.m_Permissions & EFileAttrib_EveryoneExecute)
		EveryoneAccess |= GENERIC_EXECUTE;

	if (!EveryoneAccess)
		return;

	// Create SID for Everyone (S-1-1-0)
	SID_IDENTIFIER_AUTHORITY WorldAuthority = SECURITY_WORLD_SID_AUTHORITY;
	PSID pEveryoneSid = nullptr;
	if (!AllocateAndInitializeSid(&WorldAuthority, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &pEveryoneSid))
	{
		DMibErrorNet
			(
				"Could not allocate Everyone SID for unix socket permissions on '{}', windows returned: {}"_f
				<< _UnixAddress.f_GetPath()
				<< NMib::NPlatform::fg_Win32_GetLastErrorStr(GetLastError())
			)
		;
	}

	auto SidCleanup = g_OnScopeExit / [&]
		{
			FreeSid(pEveryoneSid);
		}
	;

	EXPLICIT_ACCESS_A ExplicitAccess = {};
	ExplicitAccess.grfAccessPermissions = EveryoneAccess;
	ExplicitAccess.grfAccessMode = SET_ACCESS;
	ExplicitAccess.grfInheritance = NO_INHERITANCE;
	ExplicitAccess.Trustee.TrusteeForm = TRUSTEE_IS_SID;
	ExplicitAccess.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
	ExplicitAccess.Trustee.ptstrName = (LPSTR)pEveryoneSid;

	// Get existing DACL on the socket file
	PACL pOldDacl = nullptr;
	PSECURITY_DESCRIPTOR pSD = nullptr;
	DWORD Result = GetNamedSecurityInfoA(_UnixAddress.f_GetPath(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &pOldDacl, nullptr, &pSD);
	if (Result != ERROR_SUCCESS)
		DMibErrorNet(("Could not get security info for unix socket '{}', windows returned: {}"_f << _UnixAddress.f_GetPath() << NMib::NPlatform::fg_Win32_GetLastErrorStr(Result)));

	auto SDCleanup = g_OnScopeExit / [&]
		{
			if (pSD) LocalFree(pSD);
		}
	;

	// Merge new ACE into existing DACL
	PACL pNewDacl = nullptr;
	Result = SetEntriesInAclA(1, &ExplicitAccess, pOldDacl, &pNewDacl);
	if (Result != ERROR_SUCCESS)
		DMibErrorNet(("Could not build ACL for unix socket '{}', windows returned: {}"_f << _UnixAddress.f_GetPath() << NMib::NPlatform::fg_Win32_GetLastErrorStr(Result)));

	auto DaclCleanup = g_OnScopeExit / [&]
		{
			if (pNewDacl) LocalFree(pNewDacl);
		}
	;

	// Apply updated DACL to the socket file
	Result = SetNamedSecurityInfoA(const_cast<char *>(_UnixAddress.f_GetPath()), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, pNewDacl, nullptr);
	if (Result != ERROR_SUCCESS)
		DMibErrorNet(("Could not set permissions on unix socket '{}', windows returned: {}"_f << _UnixAddress.f_GetPath() << NMib::NPlatform::fg_Win32_GetLastErrorStr(Result)));
}

// *************************************************************************************************************************
// CWindowsSocket Implementation
// *************************************************************************************************************************

CWindowsSocket::CUnixListenState::~CUnixListenState()
{
	try
	{
		if (m_UnixFile.f_IsValid())
		{
			m_UnixFile.f_Close();
			CFile::fs_DeleteFile(m_UnixFileName);
		}
	}
	catch (NFile::CExceptionFile const &)
	{
	}
}


#if DMibConfig_IoDebug_Enable
// Null when the statistics are off, so a recording site asks and finds the counters in one read
NSys::CSocketIoStats *fg_SocketIoStats()
{
	auto &Io = NSys::fg_IoSubSystem();
	if (!Io.f_StatsEnabled())
		return nullptr;

	return &Io.m_SocketIoStats;
}

static void fg_SocketIoStatsCountSend(umint _nRequested, umint _nSent, bool _bWouldBlock)
{
	auto *pStats = fg_SocketIoStats();
	if (!pStats)
		return;

	pStats->m_nSendCalls.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
	pStats->m_nSendBytesRequested.f_FetchAdd(_nRequested, NAtomic::gc_MemoryOrder_Relaxed);
	pStats->m_nSendBytesSent.f_FetchAdd(_nSent, NAtomic::gc_MemoryOrder_Relaxed);
	if (_nRequested)
		pStats->m_SendSizeBuckets[fg_GetHighestBitSet(_nRequested)].f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
	if (_bWouldBlock)
		pStats->m_nSendWouldBlock.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
	else if (_nSent < _nRequested)
		pStats->m_nSendShort.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
}
#endif

CWindowsSocketContext::CWindowsSocketContext()
{
	mp_pIo = &fg_IoSubSystem_Windows();
	mp_bInitFailed = false;

	{
		WORD wVersionRequested;
		WSADATA wsaData;
		aint err;

		wVersionRequested = MAKEWORD( 2, 2 );

		err = WSAStartup( wVersionRequested, &wsaData );

		if ( err != 0 )
		{
			mp_bInitFailed = true;
		}

		if (LOBYTE( wsaData.wVersion ) != 2 ||
			HIBYTE( wsaData.wVersion ) != 2 )
		{
			WSACleanup( );
			mp_bInitFailed = true;
		}
	}

	// Create the shared loop before its thread; failure must surface before sockets can use it.
	mp_PollerThread.mp_pLoop = fg_CreatePlatformIoLoop();
	if (mp_PollerThread.mp_pLoop)
		mp_PollerThread.f_Start(EExecutionPriority_Highest);
	else
		mp_bInitFailed = true;
}

CWindowsSocketContext::~CWindowsSocketContext()
{
	// Other modules may still own sockets; this subsystem must not tear down their Winsock provider.
	if (mp_PollerThread.mp_pLoop)
	{
		// The poller's exit drain acknowledges the last removals; a socket still open past it
		// is an error in its owner's teardown order, which the loop's destruction checks
		mp_PollerThread.f_Stop(true);
		NSys::fg_DestroyIoLoop(mp_PollerThread.mp_pLoop);
		mp_PollerThread.mp_pLoop = nullptr;
	}
}

void CWindowsSocketContext::f_CheckFailed()
{
	if (mp_bInitFailed)
		DMibErrorNet("Initziation of WinSock has faild, cannot use net");
}


// *************************************************************************************************************************
// WindowsSocketContext Address Methods
// *************************************************************************************************************************

CWindowsAddress* CWindowsSocketContext::f_CreateAddress(NMib::NNetwork::ENetAddressType _Type, void const* _pData, umint _nDataBytes)
{
	switch(_Type)
	{

		case NMib::NNetwork::ENetAddressType_TCPv4:
			{
				if (_nDataBytes != sizeof(NMib::NNetwork::CNetAddressTCPv4))
					return nullptr;

				sockaddr_in NativeAddr;
				fp_ToNative(*(NMib::NNetwork::CNetAddressTCPv4*)_pData, NativeAddr);

				return fg_ConstructObject<CWindowsAddress>(NMemory::CDefaultAllocator(), NativeAddr);
			}
			break;

		case NMib::NNetwork::ENetAddressType_TCPv6:
			{
				if (_nDataBytes != sizeof(NMib::NNetwork::CNetAddressTCPv6))
					return nullptr;

				sockaddr_in6 NativeAddr;
				fp_ToNative(*(NMib::NNetwork::CNetAddressTCPv6*)_pData, NativeAddr);

				return fg_ConstructObject<CWindowsAddress>(NMemory::CDefaultAllocator(), NativeAddr);
			}
			break;

		case NMib::NNetwork::ENetAddressType_Unix:
		default:
		{
			return nullptr;
		}
	}
}

CWindowsAddress* CWindowsSocketContext::f_DuplicateAddress(CWindowsAddress* _Address)
{
	TCUniquePointer<CWindowsAddress> pNew = fg_Construct(*_Address);
	return pNew.f_Detach();
}

NMib::NNetwork::ENetAddressType CWindowsSocketContext::f_GetAddressType(CWindowsAddress const &_Address)
{
	return _Address.f_GetType();
}

bool CWindowsSocketContext::f_GetAddressRaw(CWindowsAddress const &_Address, NMib::NNetwork::ENetAddressType _ExpectedType, void* _opRawData, umint _nDataBytes)
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
				return false;
			}
	}
}

CWindowsAddress* CWindowsSocketContext::f_SetAddressRaw(CWindowsAddress* _pAddress, ::NMib::NNetwork::ENetAddressType _Type, void const* _pRawData, umint _nDataBytes)
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
				f_FreeAddress(_pAddress);
				return nullptr;
			}
	}
}

CWindowsAddress* CWindowsSocketContext::f_ResolveAddress(const NMib::NStr::CStr &_Address, NMib::NNetwork::ENetAddressType _PreferType)
{
	return f_ResolveAddress(_Address, _PreferType, true);
}

CWindowsAddress* CWindowsSocketContext::f_ResolveAddress(const NMib::NStr::CStr &_Address, NMib::NNetwork::ENetAddressType _PreferType, bool _bThrowOnError)
{
	f_CheckFailed();

	NMib::NStorage::TCUniquePointer<CWindowsAddress> pAddress = fg_Construct();

	if (_Address.f_StartsWith("UNIX(") || _Address.f_StartsWith("UNIX:"))
	{
		auto Address = CUnixAddress::fs_Parse(_Address, _bThrowOnError);
		if (!Address)
			return nullptr;

		pAddress->f_Set(fg_Move(*Address));
		return pAddress.f_Detach();
	}

	ADDRINFOW AddrHint;
	fg_MemClear(AddrHint);

	AddrHint.ai_family = AF_INET;

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

	CWStr Service;

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

	ADDRINFOW* pAddresses = nullptr;

	CWStr AddressStrWin = NStr::NPlatform::fg_StrToWindows(AddressStr);

	int Result = GetAddrInfoW(AddressStrWin.f_GetStr(), Service.f_GetStr(), &AddrHint, &pAddresses);

	// Try TCPv4 first, then v6.
	if (_PreferType == ENetAddressType_None && Result != 0)
	{
		AddrHint.ai_family = AF_INET6;
		Result = GetAddrInfoW(AddressStrWin.f_GetStr(), Service.f_GetStr(), &AddrHint, &pAddresses);
	}

	if (Result != 0)
	{
		if (!_bThrowOnError && Result == EAI_NONAME)
		{
			return nullptr;
		}
		else
		{
			uint32 Error = WSAGetLastError();
			DMibErrorNet((CStr::CFormat("Could not resolve address, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
		}
	}

	// Just use the first address of the correct family returned (all should be of the correct family).
	ADDRINFOW *pChosenAddress = pAddresses;
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

	FreeAddrInfoW(pAddresses);

	return pAddress.f_Detach();
}

void *CWindowsSocketContext::f_AsyncResolveAddress_Open(const NMib::NStr::CStr &_Address, ::NMib::NNetwork::ENetAddressType _PreferType, NMib::NFunction::TCFunctionMutable<void ()> &&_fOnFinish)
{
	return mp_Resolver.f_Open(_Address, _PreferType, fg_Move(_fOnFinish));
}

bool CWindowsSocketContext::f_AsyncResolveAddress_GetResult(void *_pResolver, CWindowsAddress*& _opAddress, NMib::NStr::CStr &_Error)
{
	return mp_Resolver.f_GetResult(_pResolver, (NMib::NSys::NNetwork::CAddress&)_opAddress, _Error);
}

void CWindowsSocketContext::f_AsyncResolveAddress_Close(void *_pResolver)
{
	return mp_Resolver.f_Close(_pResolver);
}

int CWindowsSocketContext::f_CompareAddresses(CWindowsAddress const& _First, CWindowsAddress const& _Second)
{
	return _First.f_Compare(_Second);
}

void CWindowsSocketContext::f_FreeAddress(CWindowsAddress* _pAddress) // It is OK to free a nullptr address.
{
	if (!_pAddress)
		return;

	fg_DeleteObject(NMemory::CDefaultAllocator(), _pAddress);
}

NMib::NStr::CStr CWindowsSocketContext::f_GetAddressString(CWindowsAddress const &_Address, ENetAddressStringFlag _Flags)
{
	using namespace NMib::NStr;

	CStr AddressStr;

	switch(_Address.f_GetType())
	{
	case ENetAddressType_TCPv4:
		{
			if (_Flags & ENetAddressStringFlag_IncludeType)
				AddressStr += "TCPv4:";

			CNetAddressTCPv4 TCPv4;
			f_GetAddressRaw(_Address, ENetAddressType_TCPv4, &TCPv4, sizeof(TCPv4));

			AddressStr += "{}.{}.{}.{}"_f << TCPv4.m_IP[0] << TCPv4.m_IP[1] << TCPv4.m_IP[2] << TCPv4.m_IP[3];

			if (_Flags & ENetAddressStringFlag_IncludePort)
				AddressStr += ":{}"_f << TCPv4.m_Port;

			break;
		}
	case ENetAddressType_TCPv6:
		{
			if (_Flags & ENetAddressStringFlag_IncludeType)
				AddressStr += "TCPv6:";

			CNetAddressTCPv6 TCPv6;
			f_GetAddressRaw(_Address, ENetAddressType_TCPv6, &TCPv6, sizeof(TCPv6));

			AddressStr += "[{nfh,sj2,sf0}{nfh,sj2,sf0}:{nfh,sj2,sf0}{nfh,sj2,sf0}:{nfh,sj2,sf0}{nfh,sj2,sf0}:{nfh,sj2,sf0}{nfh,sj2,sf0}:"
				"{nfh,sj2,sf0}{nfh,sj2,sf0}:{nfh,sj2,sf0}{nfh,sj2,sf0}:{nfh,sj2,sf0}{nfh,sj2,sf0}:{nfh,sj2,sf0}{nfh,sj2,sf0}]"_f
				<< TCPv6.m_IP[0]
				<< TCPv6.m_IP[1]
				<< TCPv6.m_IP[2]
				<< TCPv6.m_IP[3]
				<< TCPv6.m_IP[4]
				<< TCPv6.m_IP[5]
				<< TCPv6.m_IP[6]
				<< TCPv6.m_IP[7]
				<< TCPv6.m_IP[8]
				<< TCPv6.m_IP[9]
				<< TCPv6.m_IP[10]
				<< TCPv6.m_IP[11]
				<< TCPv6.m_IP[12]
				<< TCPv6.m_IP[13]
				<< TCPv6.m_IP[14]
				<< TCPv6.m_IP[15]
			;

			if (_Flags & ENetAddressStringFlag_IncludePort)
				AddressStr += ":{}"_f << TCPv6.m_Port;

			break;
		}
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
					AddressStr += "UNIX({nfo,sj3,sf0}):"_f << UnixPermissions;
				else
					AddressStr += "UNIX:";
			}

			AddressStr += Address.f_GetPath();
			break;
		}

		case ENetAddressType_None:
			break;
		default:
			break;
	}

	return AddressStr;
}
// *************************************************************************************************************************
// WindowsSocketContext Connection Operations
// *************************************************************************************************************************

static bool fg_UnixSocketsSupported()
{
	return CSystem::ms_PlatformVersion >= 10'0'017063;
}

namespace
{
	// Both loopback endpoints must opt in; newer kernels may already provide this fast path.
	void fg_EnableLoopbackFastPath(CIoSubSystem_Windows *_pIo, SOCKET _Socket)
	{
		if (!_pIo->f_LoopbackFastPathEnabled())
			return;

		int bEnable = 1;
		DWORD nBytes = 0;
		WSAIoctl(_Socket, SIO_LOOPBACK_FAST_PATH, &bEnable, sizeof(bEnable), nullptr, 0, &nBytes, nullptr, nullptr);
	}

	bool fg_IsLoopbackSockAddr(sockaddr const *_pAddress)
	{
		if (_pAddress->sa_family == AF_INET)
		{
			auto const &Native = *(sockaddr_in const *)_pAddress;
			return ((uint8 const *)&Native.sin_addr.s_addr)[0] == 127;
		}

		if (_pAddress->sa_family == AF_INET6)
		{
			auto const &Native = *(sockaddr_in6 const *)_pAddress;
			if (IN6_IS_ADDR_LOOPBACK(&Native.sin6_addr))
				return true;
			if (IN6_IS_ADDR_V4MAPPED(&Native.sin6_addr))
				return Native.sin6_addr.s6_addr[12] == 127;
		}

		return false;
	}

	bool fg_IsLoopbackAddress(CWindowsAddress const &_Address)
	{
		if (_Address.f_GetType() != ENetAddressType_TCPv4 && _Address.f_GetType() != ENetAddressType_TCPv6)
			return false;

		return fg_IsLoopbackSockAddr((sockaddr const *)_Address.f_Get());
	}

	bool fg_IsLoopbackPeer(SOCKET _Socket)
	{
		sockaddr_storage Peer;
		int nBytes = sizeof(Peer);
		if (getpeername(_Socket, (sockaddr *)&Peer, &nBytes) != 0)
			return false;

		return fg_IsLoopbackSockAddr((sockaddr const *)&Peer);
	}

	// Loopback lacks receive-window autotuning; larger buffers avoid wake-sized bursts.
	// Do not apply to remote peers, where fixed receive buffers disable needed autotuning.
	constexpr int gc_LoopbackTcpSocketBufferBytes = 1024 * 1024;

	void fg_SizeLoopbackTcpBuffers(SOCKET _Socket)
	{
		int BufferSize = gc_LoopbackTcpSocketBufferBytes;
		if
		(
			setsockopt(_Socket, SOL_SOCKET, SO_SNDBUF, (char const *)&BufferSize, sizeof(BufferSize)) != 0
			|| setsockopt(_Socket, SOL_SOCKET, SO_RCVBUF, (char const *)&BufferSize, sizeof(BufferSize)) != 0
		)
		{
			uint32 Error = WSAGetLastError();
			DMibErrorNet((CStr::CFormat("Could not size the buffers of a loopback TCP socket, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
		}
	}

	void fg_SetNonBlocking(SOCKET _Socket, char const *_pWhat)
	{
		u_long bNonBlocking = 1;
		if (ioctlsocket(_Socket, FIONBIO, &bNonBlocking) != 0)
		{
			uint32 Error = WSAGetLastError();
			DMibErrorNet((CStr::CFormat("Could not set socket non-blocking ({}), windows returned: {}") << _pWhat << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
		}
	}

	constexpr NSys::EIoLoopEvent fg_IoLoopMaskFromSocketEvents(EWindowsSocketEvent _Events)
	{
		return
			((_Events & EWindowsSocketEvent_Read) ? NSys::EIoLoopEvent::mc_Read : NSys::EIoLoopEvent::mc_None)
			| ((_Events & EWindowsSocketEvent_Write) ? NSys::EIoLoopEvent::mc_Write : NSys::EIoLoopEvent::mc_None)
		;
	}
}

// Decode on the driving thread so readiness preserves ordering with the rest of the dispatch pass.
static void fg_DispatchSocketIoEvent(void *_pToken, NSys::EIoLoopEvent _Events, int _Error)
{
	CWindowsSocket *pSocket = (CWindowsSocket *)_pToken;

	DMibLock(pSocket->m_Lock);

	if (_Events == NSys::EIoLoopEvent::mc_None)
	{
		// Replay readiness predating registration; no new edge may arrive.
		if (pSocket->m_fOnStateChange)
			pSocket->m_fOnStateChange((ENetTCPState)pSocket->m_StateAtomic.f_Load());

		return;
	}

#if DMibConfig_IoDebug_Enable
	if (auto *pStats = fg_SocketIoStats())
	{
		if (fg_IsSet(_Events, NSys::EIoLoopEvent::mc_Read))
			pStats->m_nReadinessReportsRead.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		if (fg_IsSet(_Events, NSys::EIoLoopEvent::mc_Write))
			pStats->m_nReadinessReportsWrite.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
	}
#endif

	if (pSocket->m_CloseError || pSocket->m_bNonErrorClose)
		return;

	ENetTCPState AddedState = ENetTCPState_None;

	auto fAddState = [&]
		{
			if (AddedState)
			{
				pSocket->m_StateAtomic.f_FetchOr(AddedState);
				if (pSocket->m_fOnStateChange)
					pSocket->m_fOnStateChange(AddedState);
			}
		}
	;

	auto fSocketError = [&]() -> int
		{
			int Error = 0;
			int ErrorLen = sizeof(Error);
			if (getsockopt(pSocket->m_Socket, SOL_SOCKET, SO_ERROR, (char *)&Error, &ErrorLen) != 0)
				return WSAGetLastError();

			return Error;
		}
	;

	if (fg_IsSet(_Events, NSys::EIoLoopEvent::mc_Error))
	{
		if (_Error)
			pSocket->m_CloseError = _Error;
		else
		{
			// The backend has no error value for this event; the socket error answers, with -1
			// standing in when even that is empty so the close still reads as an error close
			pSocket->m_CloseError = fSocketError();
			if (!pSocket->m_CloseError)
				pSocket->m_CloseError = -1;
		}

		AddedState |= ENetTCPState_Closed;
		fAddState();

		return;
	}

	if (fg_IsSet(_Events, NSys::EIoLoopEvent::mc_WriteClosed))
	{
		int Error = fSocketError();
		if (Error)
			pSocket->m_CloseError = Error;
		else
			pSocket->m_bNonErrorClose = true;

		AddedState |= ENetTCPState_Closed;
		fAddState();

		return;
	}

	if (fg_IsSet(_Events, NSys::EIoLoopEvent::mc_Hup))
	{
		int Error = fSocketError();
		if (Error)
			pSocket->m_CloseError = Error;
		else
			pSocket->m_bNonErrorClose = true;

		AddedState |= ENetTCPState_Closed;
		fAddState();

		return;
	}

	if (fg_IsSet(_Events, NSys::EIoLoopEvent::mc_ReadClosed))
	{
		// AFD has no event for the completed pair of half-closes. Track local shutdown and report readable too,
		// so consumers drain any final payload before observing EOF.
		if (pSocket->m_bShutdownCalled)
		{
			int Error = fSocketError();
			if (Error)
				pSocket->m_CloseError = Error;
			else
				pSocket->m_bNonErrorClose = true;

			AddedState |= ENetTCPState_Closed | ENetTCPState_Read;
			fAddState();

			return;
		}

		if (!pSocket->m_bRemoteCloseSignalled)
		{
			pSocket->m_bRemoteCloseSignalled = true;
			AddedState |= ENetTCPState_RemoteClosed | ENetTCPState_Read;
		}
	}

	if (fg_IsSet(_Events, NSys::EIoLoopEvent::mc_Read))
	{
		if (pSocket->m_Mode == EWindowsSocketMode_Connect || pSocket->m_Mode == EWindowsSocketMode_Datagram)
			AddedState |= ENetTCPState_Read;
		else if (pSocket->m_Mode == EWindowsSocketMode_Listen)
			AddedState |= ENetTCPState_Connection;
	}

	if (fg_IsSet(_Events, NSys::EIoLoopEvent::mc_Write))
	{
		if (pSocket->m_Mode == EWindowsSocketMode_Connect || pSocket->m_Mode == EWindowsSocketMode_Datagram)
			AddedState |= ENetTCPState_Write;
		else if (pSocket->m_Mode == EWindowsSocketMode_Connecting)
		{
			// Writability arrives for a settled connect either way; the socket error is what
			// distinguishes success from failure
			int Error = fSocketError();
			if (Error)
			{
				pSocket->m_CloseError = Error;
				AddedState |= ENetTCPState_Closed;
			}
			else
			{
				pSocket->m_Mode = EWindowsSocketMode_Connect;
				AddedState |= ENetTCPState_Connected;
			}
		}
	}

	fAddState();
}

static ENetAddressType fsg_AddressTypeOfSocket(SOCKET _Socket)
{
	sockaddr_storage Address;
	int nAddress = sizeof(Address);
	fg_MemClear(&Address, sizeof(Address));

	if (getsockname(_Socket, (sockaddr *)&Address, &nAddress) != 0)
		return ENetAddressType_None;

	if (Address.ss_family == AF_UNIX)
		return ENetAddressType_Unix;
	if (Address.ss_family == AF_INET)
		return ENetAddressType_TCPv4;
	if (Address.ss_family == AF_INET6)
		return ENetAddressType_TCPv6;

	return ENetAddressType_None;
}

CWindowsSocket *CWindowsSocketContext::fp_CreateSocket
	(
		SOCKET _Socket
		, EWindowsSocketMode _Mode
		, EWindowsSocketEvent _Events
		, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
		, bool _bFromInherit
	)
{
	NMib::NStorage::TCUniquePointer<CWindowsSocket> pNewSocket = fg_Construct(_Socket, _Mode, _Events, fg_Move(_fOnStateChange));
	pNewSocket->m_pIo = mp_pIo;

	if (_bFromInherit)
	{
		pNewSocket->m_bInitialWriteNotification = false;
		pNewSocket->m_bFromInherit = true;
	}

	NMib::NNetwork::ENetTCPState StateAdded = NMib::NNetwork::ENetTCPState_Read | NMib::NNetwork::ENetTCPState_Write;

	if (_Mode == EWindowsSocketMode_Connect)
		StateAdded |= NMib::NNetwork::ENetTCPState_Connected;
	else if (_Mode == EWindowsSocketMode_Connecting)
		pNewSocket->m_bInitialWriteNotification = false;

	pNewSocket->m_StateAtomic.f_FetchOr(StateAdded);

	return pNewSocket.f_Detach();
}

CWindowsSocket *CWindowsSocketContext::fp_Connect
	(
		CWindowsAddress const &_Address
		, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
		, CWindowsAddress const *_pBindAddress
	)
{
	f_CheckFailed();

	CWindowsAddress Address = _Address;

	if (Address.f_GetType() == ENetAddressType_Unix)
	{
		CUnixAddress const &UnixAddress = Address.f_GetUnix();

		if (!fg_UnixSocketsSupported())
		{
			CStr UnixFileName = UnixAddress.f_GetPath();
			if (!CFile::fs_FileExists(UnixFileName))
				DMibErrorNet(fg_Format("Unix socket '{}' does not exist", UnixFileName));

			uint16 Port;

			try
			{
				TCBinaryStreamFile<> UnixFile;
				UnixFile.f_Open(UnixFileName, EFileOpen_Read | EFileOpen_NoLocalCache | EFileOpen_ShareRead | EFileOpen_ShareWrite);
				UnixFile >> Port;
			}
			catch (CException const &_Exception)
			{
				DMibErrorNet(fg_Format("Failed to get port for unix socket: {}", _Exception.f_GetErrorStr()));
			}

			CNetAddressTCPv4 ConnectAddress{{127, 0, 0, 1}, Port};

			NNetwork::CNetAddress NetAddress{ConnectAddress};

			Address = *((CWindowsAddress *)NetAddress.f_AccessRaw());
		}
	}

	ENetAddressType AddressType = Address.f_GetType();
	SOCKET hSock = INVALID_SOCKET;

	if
	(
		AddressType != ENetAddressType_TCPv4
		&& AddressType != ENetAddressType_TCPv6
		&& AddressType != ENetAddressType_Unix
	)
	{
		DMibErrorNet("Invalid address type.");
	}

	int Family = (AddressType == ENetAddressType_TCPv4) ? PF_INET : PF_INET6;
	if (AddressType == ENetAddressType_Unix)
		Family = PF_UNIX;

	hSock = socket(Family, SOCK_STREAM, 0);

	if (hSock == INVALID_SOCKET)
	{
		uint32 Error = WSAGetLastError();
		DMibErrorNet((CStr::CFormat("Could not create a socket for connection, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
	}

	auto SocketCleanup = g_OnScopeExit / [&]
		{
			closesocket(hSock);
		}
	;

	if (AddressType != ENetAddressType_Unix)
	{
		BOOL NoDelay = true;
		if (setsockopt(hSock, IPPROTO_TCP, TCP_NODELAY, (char *)&NoDelay, sizeof(NoDelay)))
		{
			uint32 Error = WSAGetLastError();
			DMibErrorNet((CStr::CFormat("Could not set connect socket NoDelay setting, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
		}

		if (fg_IsLoopbackAddress(Address))
		{
			fg_EnableLoopbackFastPath(mp_pIo, hSock);
			fg_SizeLoopbackTcpBuffers(hSock);
		}
	}

	if (_pBindAddress)
	{
		int Result = bind(hSock, (sockaddr const*)_pBindAddress->f_Get(), _pBindAddress->f_GetSockAddrLen());
		if (Result != 0)
		{
			uint32 Error = WSAGetLastError();
			DMibErrorNet((CStr::CFormat("Could not bind socket, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
		}
	}

	fg_SetNonBlocking(hSock, "connect");

	EWindowsSocketMode Mode = EWindowsSocketMode_Connect;

	int Result = connect(hSock, (sockaddr const*)Address.f_Get(), Address.f_GetSockAddrLen());
	if (Result != 0)
	{
		uint32 Error = WSAGetLastError();

		if (Error != WSAEWOULDBLOCK)
		{
			if (_Address.f_GetType() == ENetAddressType_Unix)
			{
				auto &Unix = _Address.f_GetUnix();
				DMibErrorNet((CStr::CFormat("Could not connect socket ('{}'), windows returned: {}") << Unix.f_GetPath() << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
			}
			else
				DMibErrorNet((CStr::CFormat("Could not connect socket, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
		}

		Mode = EWindowsSocketMode_Connecting;
	}

	SocketCleanup.f_Clear();

	auto *pSocket = fp_CreateSocket(hSock, Mode, EWindowsSocketEvent(EWindowsSocketEvent_Read | EWindowsSocketEvent_Write), fg_Move(_fOnStateChange));
	pSocket->m_AddressType = AddressType;

	return pSocket;
}

CWindowsSocket *CWindowsSocketContext::f_AsyncConnect
	(
		CWindowsAddress const &_Address
		, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
		, CWindowsAddress const *_pBindAddress
	)
{
	return fp_Connect(_Address, fg_Move(_fOnStateChange), _pBindAddress);
}

void CWindowsSocketContext::f_StartSocket(CWindowsSocket *_pSocket)
{
	if (umint nBufferBytes = mp_pIo->f_SocketBufferBytesOverride(); nBufferBytes && _pSocket->m_Socket != INVALID_SOCKET)
	{
		int BufferSize = (int)fg_Min(nBufferBytes, umint(INT_MAX));
		if
		(
			setsockopt(_pSocket->m_Socket, SOL_SOCKET, SO_SNDBUF, (char const *)&BufferSize, sizeof(BufferSize)) != 0
			|| setsockopt(_pSocket->m_Socket, SOL_SOCKET, SO_RCVBUF, (char const *)&BufferSize, sizeof(BufferSize)) != 0
		)
		{
			uint32 Error = WSAGetLastError();
			DMibErrorNet((CStr::CFormat("Could not apply MalterlibSocketBufferSize, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
		}
	}

	if (_pSocket->m_pIoRegistration)
		DMibErrorNet("Windows socket already registered");

	NSys::EIoLoopEvent EventMask = fg_IoLoopMaskFromSocketEvents(_pSocket->m_RegisteredEvents);
	if (EventMask == NSys::EIoLoopEvent::mc_None)
		DMibErrorNet("Failed to register Windows socket.");

	NSys::ICIoLoop *pThreadLoop = NSys::fg_GetThreadIoLoop();
	_pSocket->m_pOwningLoop = pThreadLoop ? pThreadLoop : mp_PollerThread.mp_pLoop;

	NSys::CIoLoopRegisterOptions RegisterOptions;
	RegisterOptions.m_bReadinessOnly = _pSocket->m_bInheritable;
	RegisterOptions.m_bInheritedHandle = _pSocket->m_bFromInherit;

	// Decide release semantics before publishing registration: its callback can immediately activate completion I/O.
	// Apply SO_SNDBUF only on the first completion send so readiness handshakes retain buffering.
	_pSocket->m_nSendBufferBytesToApply = mp_pIo->f_SocketSendBufferBytesOverride();
	// Unbuffered TCP needs SIO_TCP_INFO for window sizing. Inheritable sockets retain buffers for readiness sends.
	if
	(
		_pSocket->m_nSendBufferBytesToApply == umint(-1)
		&& !_pSocket->m_bInheritable
		&& mp_pIo->f_DirectSendEnabled()
		&& _pSocket->m_Mode != EWindowsSocketMode_Datagram
		&& (_pSocket->m_AddressType == ENetAddressType_Unix || fg_WindowsTcpInfoSupported())
	)
	{
		_pSocket->m_nSendBufferBytesToApply = 0;
	}
	if (_pSocket->m_nSendBufferBytesToApply == 0 && _pSocket->m_AddressType != ENetAddressType_Unix)
		RegisterOptions.m_bSendCompletesOnAck = true;

	_pSocket->m_pIoRegistration = _pSocket->m_pOwningLoop->f_Register
		(
			(NSys::CIoLoopHandle)_pSocket->m_Socket
			, _pSocket
			, EventMask
			, &fg_DispatchSocketIoEvent
			, fg_IsSet(EventMask, NSys::EIoLoopEvent::mc_Read) != 0
			, RegisterOptions
		)
	;

	if (_pSocket->m_nSendWindowBytes)
		_pSocket->m_pOwningLoop->f_SetSendWindow(_pSocket->m_pIoRegistration, _pSocket->m_nSendWindowBytes);
}

// Arm after would-block or a short stream transfer, which proves the queue exhausted.
// EOF must not rearm; datagrams require an actual would-block observation.
static void fg_RequestSocketReadiness(CWindowsSocket *_pSocket, NSys::EIoLoopEvent _EventMask)
{
	if (!_pSocket->m_pOwningLoop || !_pSocket->m_pIoRegistration)
		return;

#if DMibConfig_IoDebug_Enable
	if (auto *pStats = fg_SocketIoStats())
	{
		if (fg_IsSet(_EventMask, NSys::EIoLoopEvent::mc_Read))
			pStats->m_nReadinessArmsRead.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		if (fg_IsSet(_EventMask, NSys::EIoLoopEvent::mc_Write))
			pStats->m_nReadinessArmsWrite.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
	}
#endif

	_pSocket->m_pOwningLoop->f_RequestReadiness(_pSocket->m_pIoRegistration, _EventMask);
}

void NSys::NNetwork::fg_RequestReadiness(void *_pSocket, bool _bRead, bool _bWrite)
{
	CWindowsSocket *pSocket = (CWindowsSocket *)_pSocket;

	NSys::EIoLoopEvent EventMask =
		(_bRead ? NSys::EIoLoopEvent::mc_Read : NSys::EIoLoopEvent::mc_None)
		| (_bWrite ? NSys::EIoLoopEvent::mc_Write : NSys::EIoLoopEvent::mc_None)
	;
	if (EventMask != NSys::EIoLoopEvent::mc_None)
		fg_RequestSocketReadiness(pSocket, EventMask);
}

NSys::ICIoLoop *NSys::NNetwork::fg_GetOwningIoLoop(void *_pSocket)
{
	CWindowsSocket *pSocket = (CWindowsSocket *)_pSocket;

	// Only caller-created loops are restorable bindings; nullptr already selects the shared poller.
	NSys::ICIoLoop *pOwningLoop = pSocket->m_pOwningLoop;
	if (!pOwningLoop || !pOwningLoop->m_bCreatedAsLoop)
		return nullptr;

	return pOwningLoop;
}

namespace
{
	// Submissions follow registration changes on the loop queue, so binding precedes kernel submission.
	// Read only immutable registration options here; failed binding cancels pending work.
	bool fg_CompletionPortIsOwningLoops(CWindowsSocket *_pSocket)
	{
		if (!_pSocket->m_pIoRegistration)
			return false;

		return !static_cast<CIocpRegistration const *>(_pSocket->m_pIoRegistration)->m_Options.m_bReadinessOnly;
	}
}

// Windows readiness requires an extra poll per arm; completion I/O also benefits local streams.
bool NSys::NNetwork::fg_SupportsCompletionIo(void *_pSocket)
{
	CWindowsSocket *pSocket = (CWindowsSocket *)_pSocket;

	if (pSocket->m_Mode != EWindowsSocketMode_Connect && pSocket->m_Mode != EWindowsSocketMode_Connecting)
		return false;

	if (!pSocket->m_pOwningLoop || !pSocket->m_pOwningLoop->f_SupportsCompletionIo())
		return false;

	return fg_CompletionPortIsOwningLoops(pSocket);
}

bool NSys::NNetwork::fg_SupportsReceiveStream(void *_pSocket)
{
	CWindowsSocket *pSocket = (CWindowsSocket *)_pSocket;

	if (pSocket->m_Mode != EWindowsSocketMode_Connect && pSocket->m_Mode != EWindowsSocketMode_Connecting)
		return false;

	if (!pSocket->m_pOwningLoop || !pSocket->m_pOwningLoop->f_SupportsReceiveStream())
		return false;

	return fg_CompletionPortIsOwningLoops(pSocket);
}

bool NSys::NNetwork::fg_SendReleaseIsPrompt(void *_pSocket)
{
	CWindowsSocket *pSocket = (CWindowsSocket *)_pSocket;
	if (!pSocket->m_pOwningLoop || !pSocket->m_pIoRegistration)
		return true;

	return pSocket->m_pOwningLoop->f_SendReleaseIsPrompt(pSocket->m_pIoRegistration);
}

bool NSys::NNetwork::fg_StartReceiveStream(void *_pSocket, umint _nBufferBytes, NStorage::TCSharedPointer<NSys::CIoStreamBackpressure> _pBackpressure, NSys::FIoStreamSink &&_fSink)
{
	CWindowsSocket *pSocket = (CWindowsSocket *)_pSocket;
	if (!pSocket->m_pOwningLoop || !pSocket->m_pIoRegistration)
		return false;

	return pSocket->m_pOwningLoop->f_StartReceiveStream(pSocket->m_pIoRegistration, _nBufferBytes, fg_Move(_pBackpressure), fg_Move(_fSink));
}

void NSys::NNetwork::fg_ResumeReceiveStream(void *_pSocket)
{
	CWindowsSocket *pSocket = (CWindowsSocket *)_pSocket;
	if (!pSocket->m_pOwningLoop || !pSocket->m_pIoRegistration)
		return;

	pSocket->m_pOwningLoop->f_ResumeReceiveStream(pSocket->m_pIoRegistration);
}

// Set before starting: readiness-only registration avoids permanent completion-port binding for the next owner.
void NSys::NNetwork::fg_SetInheritable(void *_pSocket)
{
	CWindowsSocket *pSocket = (CWindowsSocket *)_pSocket;
	if (pSocket->m_pIoRegistration)
		DMibErrorNet("A socket can only be made inheritable before it is started");

	pSocket->m_bInheritable = true;
}

// Transport upgrades retain the existing registration and kernel connection.
void NSys::NNetwork::fg_ReownSocket(void *_pSocket, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange)
{
	CWindowsSocket *pSocket = (CWindowsSocket *)_pSocket;
	{
		DMibLock(pSocket->m_Lock);
		pSocket->m_fOnStateChange = fg_Move(_fOnStateChange);
		pSocket->m_bInitialWriteNotification = false;
	}

	pSocket->m_StateAtomic.f_FetchOr(NMib::NNetwork::ENetTCPState_Read | NMib::NNetwork::ENetTCPState_Write | NMib::NNetwork::ENetTCPState_Connected);
	fg_RequestReadiness(_pSocket, true, true);
}

void NSys::NNetwork::fg_SetAbortOnClose(void *_pSocket)
{
	CWindowsSocket *pSocket = (CWindowsSocket *)_pSocket;
	if (pSocket->m_Socket == INVALID_SOCKET || pSocket->m_AddressType == ENetAddressType_Unix)
		return;

	linger Linger{1, 0};
	setsockopt(pSocket->m_Socket, SOL_SOCKET, SO_LINGER, (char const *)&Linger, sizeof(Linger));
}

// Fixed buffers disable Windows autotuning; apply only explicit windows and preserve send-buffer overrides.
void NSys::NNetwork::fg_SetSendWindow(void *_pSocket, umint _nBytes, bool _bConfigured)
{
	CWindowsSocket *pSocket = (CWindowsSocket *)_pSocket;
	if (pSocket->m_Socket == INVALID_SOCKET || !_nBytes)
		return;

	pSocket->m_nSendWindowBytes = _nBytes;
	if (pSocket->m_pOwningLoop && pSocket->m_pIoRegistration)
		pSocket->m_pOwningLoop->f_SetSendWindow(pSocket->m_pIoRegistration, _nBytes);

	if (!_bConfigured || pSocket->m_AddressType == ENetAddressType_Unix)
		return;

	int BufferSize = (int)fg_Min(_nBytes, umint(TCLimitsInt<int>::mc_Max));
	if
	(
		(
			pSocket->m_nSendBufferBytesToApply == umint(-1)
			&& setsockopt(pSocket->m_Socket, SOL_SOCKET, SO_SNDBUF, (char const *)&BufferSize, sizeof(BufferSize)) != 0
		)
		|| setsockopt(pSocket->m_Socket, SOL_SOCKET, SO_RCVBUF, (char const *)&BufferSize, sizeof(BufferSize)) != 0
	)
	{
		uint32 Error = WSAGetLastError();
		DMibErrorNet((CStr::CFormat("Could not size the socket buffers to the send window, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
	}
}

#include "Malterlib_Core_Platform_Windows_TcpInfo.h"

bool NSys::NNetwork::fg_QueryPathDeliveryRate(void *_pSocket, umint &o_nBytes, bool &o_bAppLimited)
{
	CWindowsSocket *pSocket = (CWindowsSocket *)_pSocket;
	if (pSocket->m_Socket == INVALID_SOCKET || pSocket->m_AddressType == ENetAddressType_Unix)
		return false;

	return fg_Windows_QueryPathDeliveryRate(pSocket->m_Socket, pSocket->m_PathLastBytesOut, pSocket->m_PathLastStamp, o_nBytes, o_bAppLimited);
}

bool NSys::NNetwork::fg_IsSendWindowFull(void *_pSocket, umint _nUnreleasedBytes, umint _nStartBytes)
{
	CWindowsSocket *pSocket = (CWindowsSocket *)_pSocket;
	if (!pSocket->m_pOwningLoop || !pSocket->m_pIoRegistration)
		return false;

	return pSocket->m_pOwningLoop->f_IsSendWindowFull(pSocket->m_pIoRegistration, _nUnreleasedBytes, _nStartBytes);
}

umint NSys::NNetwork::fg_SubmitSendVectored(void *_pSocket, NSys::CIoSpan const *_pSpans, umint _nSpans, NSys::FIoCompletion &&_fOnComplete, NSys::FIoBufferReleased &&_fOnBufferReleased)
{
	CWindowsSocket *pSocket = (CWindowsSocket *)_pSocket;
	if (!pSocket->m_pOwningLoop || !pSocket->m_pIoRegistration)
		return 0;

	// Defer SO_SNDBUF=0 until all sends are overlapped; unbuffered nonblocking sends can stall a readiness handshake.
	if (!pSocket->m_bSendBufferDecided)
	{
		pSocket->m_bSendBufferDecided = true;

		umint nSendBufferBytes = pSocket->m_nSendBufferBytesToApply;
		if (nSendBufferBytes != umint(-1))
		{
			// Release semantics were published at registration; retaining an unexpected buffer would violate them.
			int BufferSize = (int)fg_Min(nSendBufferBytes, umint(TCLimitsInt<int>::mc_Max));
			if (setsockopt(pSocket->m_Socket, SOL_SOCKET, SO_SNDBUF, (char const *)&BufferSize, sizeof(BufferSize)) != 0)
			{
				uint32 Error = WSAGetLastError();
				DMibErrorNet((CStr::CFormat("Could not apply the send buffer policy, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
			}
		}
	}

	return pSocket->m_pOwningLoop->f_SubmitSendVectored(pSocket->m_pIoRegistration, _pSpans, _nSpans, fg_Move(_fOnComplete), fg_Move(_fOnBufferReleased));
}

TCUniquePointer<CWindowsSocket::CUnixListenState> CWindowsSocketContext::fp_PrepareUnixListen(CWindowsAddress &o_Address)
{
	if (o_Address.f_GetType() == ENetAddressType_Unix)
	{
		CUnixAddress const &UnixAddress = o_Address.f_GetUnix();

		NStr::CStr UnixFilePath = UnixAddress.f_GetPath();
		if (NFile::CFile::fs_FileExists(UnixFilePath))
			NFile::CFile::fs_DeleteFile(UnixFilePath);
		auto Directory = NFile::CFile::fs_GetPath(UnixFilePath);
		if (!NFile::CFile::fs_FileExists(Directory))
			NFile::CFile::fs_CreateDirectory(Directory);

		if (!fg_UnixSocketsSupported())
		{
			TCUniquePointer<CWindowsSocket::CUnixListenState> pListenState = fg_Construct();

			pListenState->m_Address = UnixAddress;
			pListenState->m_UnixFileName = UnixFilePath;
			pListenState->m_UnixFile.f_Open(pListenState->m_UnixFileName, EFileOpen_Write | EFileOpen_NoLocalCache | EFileOpen_ShareRead);

			CNetAddressTCPv4 ListenAddress{ {127, 0, 0, 1}, 0 };

			NNetwork::CNetAddress NetAddress{ ListenAddress };

			o_Address = *((CWindowsAddress*)NetAddress.f_AccessRaw());

			return pListenState;
		}
	}

	return {};
}

CWindowsSocket *CWindowsSocketContext::f_Listen
	(
		CWindowsAddress const &_Address
		, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
		, NNetwork::ENetFlag _Flags
	)
{
	f_CheckFailed();

	CWindowsAddress Address = _Address;
	auto pUnixListen = fp_PrepareUnixListen(Address);

	ENetAddressType AddressType = Address.f_GetType();

	if
		(
			AddressType != ENetAddressType_TCPv4
			&& AddressType != ENetAddressType_TCPv6
			&& AddressType != ENetAddressType_Unix
		)
	{
		DMibErrorNet("Invalid address type for listening");
	}

	int Family = (AddressType == ENetAddressType_TCPv4) ? AF_INET : AF_INET6;
	if (AddressType == ENetAddressType_Unix)
		Family = AF_UNIX;

	SOCKET hSock = socket(Family, SOCK_STREAM, 0);

	if (hSock == INVALID_SOCKET)
		DMibErrorNet("Could not create a socket for listening");

	auto Cleanup = g_OnScopeExit / [&]
		{
			closesocket(hSock);
		}
	;

	if (_Flags & NNetwork::ENetFlag_ReusePort)
	{
		int bReuse = 1;
		setsockopt(hSock, SOL_SOCKET, SO_REUSEADDR, (char const*)&bReuse, sizeof(bReuse));
	}

	int Result = bind(hSock, (sockaddr const*)Address.f_Get(), Address.f_GetSockAddrLen());

	if (Result != 0)
	{
		uint32 Error = WSAGetLastError();
		DMibErrorNet((CStr::CFormat("Could not bind socket, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
	}

	if (_Address.f_GetType() == ENetAddressType_Unix)
		fg_ApplyUnixSocketPermissions(_Address.f_GetUnix());

	// Accepted sockets inherit the fast path from the listener; only loopback peers that opted
	// in themselves take it
	if (AddressType != ENetAddressType_Unix)
		fg_EnableLoopbackFastPath(mp_pIo, hSock);

	fg_SetNonBlocking(hSock, "listen");

	Result = listen(hSock, SOMAXCONN);

	if (Result != 0)
	{
		uint32 Error = WSAGetLastError();
		DMibErrorNet((CStr::CFormat("Could not listen on socket, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
	}

	Cleanup.f_Clear();

	auto *pSocket = fp_CreateSocket(hSock, EWindowsSocketMode_Listen, EWindowsSocketEvent_Read, fg_Move(_fOnStateChange));
	pSocket->m_AddressType = AddressType;
	pSocket->m_pUnixListen = fg_Move(pUnixListen);

	if (pSocket->m_pUnixListen)
	{
		uint16 ListenPort = f_GetListenPort(pSocket);
		auto &UnixListen = *pSocket->m_pUnixListen;
		UnixListen.m_UnixFile << ListenPort;
	}

	return pSocket;
}

CWindowsSocket *CWindowsSocketContext::f_ListenDatagram
	(
		CWindowsAddress const &_Address
		, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
		, NNetwork::ENetFlag _Flags
	)
{
	f_CheckFailed();

	CWindowsAddress Address = _Address;
	auto pUnixListen = fp_PrepareUnixListen(Address);

	ENetAddressType AddressType = Address.f_GetType();

	if
		(
			AddressType != ENetAddressType_TCPv4
			&& AddressType != ENetAddressType_TCPv6
			&& AddressType != ENetAddressType_Unix
		)
	{
		DMibErrorNet("Invalid address type for listening");
	}

	int Family = (AddressType == ENetAddressType_TCPv4) ? AF_INET : AF_INET6;
	if (AddressType == ENetAddressType_Unix)
		Family = AF_UNIX;

	SOCKET hSock = socket(Family, SOCK_DGRAM, 0);

	if (hSock == INVALID_SOCKET)
		DMibErrorNet("Could not create a socket for listening");

	auto Cleanup = g_OnScopeExit / [&]
		{
			closesocket(hSock);
		}
	;

	if (_Flags & NNetwork::ENetFlag_ReusePort)
	{
		int bReuse = 1;
		setsockopt(hSock, SOL_SOCKET, SO_REUSEADDR, (char const*)&bReuse, sizeof(bReuse));
	}

	int Result = bind(hSock, (sockaddr const*)Address.f_Get(), Address.f_GetSockAddrLen());

	if (Result != 0)
	{
		uint32 Error = WSAGetLastError();
		DMibErrorNet((CStr::CFormat("Could not bind socket, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
	}

	if (_Address.f_GetType() == ENetAddressType_Unix)
		fg_ApplyUnixSocketPermissions(_Address.f_GetUnix());

	fg_SetNonBlocking(hSock, "listen datagram");

	Cleanup.f_Clear();

	auto *pSocket = fp_CreateSocket(hSock, EWindowsSocketMode_Datagram, EWindowsSocketEvent(EWindowsSocketEvent_Read | EWindowsSocketEvent_Write), fg_Move(_fOnStateChange));
	pSocket->m_AddressType = AddressType;
	pSocket->m_BindAddressSize = Address.f_GetSockAddrLen();
	pSocket->m_pUnixListen = fg_Move(pUnixListen);

	if (pSocket->m_pUnixListen)
	{
		uint16 ListenPort = f_GetListenPort(pSocket);
		auto &UnixListen = *pSocket->m_pUnixListen;
		UnixListen.m_UnixFile << ListenPort;
	}

	return pSocket;
}

CWindowsSocket *CWindowsSocketContext::f_Accept(CWindowsSocket *_pSocket, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange)
{
	f_CheckFailed();

	SOCKET hSock = accept(_pSocket->m_Socket, nullptr, 0);
	if (hSock == INVALID_SOCKET)
	{
		int LastError = WSAGetLastError();
		if (LastError == WSAEWOULDBLOCK)
		{
			fg_RequestSocketReadiness(_pSocket, NSys::EIoLoopEvent::mc_Read);
			return nullptr;
		}

		DMibErrorNet((CStr::CFormat("Could not accept socket, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(LastError)).f_GetStr());
	}

	auto Cleanup = g_OnScopeExit / [&]
		{
			closesocket(hSock);
		}
	;

	if (_pSocket->m_AddressType != ENetAddressType_Unix)
	{
		BOOL NoDelay = true;
		if (setsockopt(hSock, IPPROTO_TCP, TCP_NODELAY, (char *)&NoDelay, sizeof(NoDelay)))
		{
			uint32 Error = WSAGetLastError();
			DMibErrorNet((CStr::CFormat("Could not accept socket (TCP_NODELAY), windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
		}

		if (fg_IsLoopbackPeer(hSock))
			fg_SizeLoopbackTcpBuffers(hSock);
	}

	// Explicit rather than trusting inheritance from the listener
	fg_SetNonBlocking(hSock, "accept");

	Cleanup.f_Clear();

	auto *pSocket = fp_CreateSocket(hSock, EWindowsSocketMode_Connect, EWindowsSocketEvent(EWindowsSocketEvent_Read | EWindowsSocketEvent_Write), fg_Move(_fOnStateChange));
	pSocket->m_AddressType = _pSocket->m_AddressType;
	pSocket->m_bInheritable = _pSocket->m_bInheritable;

	return pSocket;
}

bool CWindowsSocketContext::f_Shutdown(CWindowsSocket *_pSocket)
{
	int Ret = shutdown(_pSocket->m_Socket, SD_SEND);

	if (Ret == SOCKET_ERROR)
	{
		int Error = WSAGetLastError();
		if (Error != WSAEWOULDBLOCK && Error != WSAENOTCONN)
			DMibErrorNet((CStr::CFormat("Could not shutdown socket, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
	}
	else
	{
		bool bRemoteCloseSignalled;
		{
			DMibLock(_pSocket->m_Lock);
			_pSocket->m_bShutdownCalled = true;
			bRemoteCloseSignalled = _pSocket->m_bRemoteCloseSignalled;
		}

		// AFD emits no event when local shutdown completes a prior peer half-close. Re-request held close state under the dispatch lock
		// so exactly one path reports full closure.
		if (bRemoteCloseSignalled)
			fg_RequestSocketReadiness(_pSocket, NSys::EIoLoopEvent::mc_ReadClosed);
	}

	return true;
}

#if DMibConfig_IoDebug_Enable
static void fsg_DumpTcpInfoAtClose(CWindowsSocket *_pSocket)
{
	if
	(
		!_pSocket->m_pIo->f_StatsEnabled()
		|| _pSocket->m_AddressType == ENetAddressType_Unix
		|| _pSocket->m_Mode == EWindowsSocketMode_Datagram
		|| _pSocket->m_Mode == EWindowsSocketMode_Listen
	)
	{
		return;
	}

	DWORD Version = 0;
	CWindowsTcpInfoV0 Info;
	DWORD nBytes = 0;
	if (WSAIoctl(_pSocket->m_Socket, SIO_TCP_INFO, &Version, sizeof(Version), &Info, sizeof(Info), &nBytes, nullptr, nullptr) != 0)
		return;

	if (Info.m_BytesOut < 1024 * 1024)
		return;

	NSys::fg_ConsoleErrorOutput
		(
			NStr::fg_Format<NStr::CStrNonTracked>
			(
				"[tcp info] out={} in={} rttUs={} minRttUs={} cwnd={} sndWnd={} inFlight={} mss={} retransBytes={} fastRetrans={} timeouts={} dupAcks={} reorderedBytes={} timeMs={}\n"
				, Info.m_BytesOut
				, Info.m_BytesIn
				, Info.m_RttUs
				, Info.m_MinRttUs
				, Info.m_Cwnd
				, Info.m_SndWnd
				, Info.m_BytesInFlight
				, Info.m_Mss
				, Info.m_BytesRetrans
				, Info.m_FastRetrans
				, Info.m_TimeoutEpisodes
				, Info.m_DupAcksIn
				, Info.m_BytesReordered
				, Info.m_ConnectionTimeMs
			)
		)
	;
}
#endif

// Run only after no loop reference remains; closes the handle and frees the socket.
void CWindowsSocketContext::fp_DestroySocket(CWindowsSocket *_pSocket)
{
	if (_pSocket->m_Socket != INVALID_SOCKET)
	{
		DMibLock(_pSocket->m_Lock);
#if DMibConfig_IoDebug_Enable
		fsg_DumpTcpInfoAtClose(_pSocket);
#endif
		closesocket(_pSocket->m_Socket);
		_pSocket->m_Socket = INVALID_SOCKET;
	}

	_pSocket->m_pUnixListen.f_Clear();

	fg_DeleteObject(NMemory::CDefaultAllocator(), _pSocket);
}

bool CWindowsSocketContext::f_Close(CWindowsSocket *_pSocket)
{
	// Only shared-poller sockets may close synchronously. Cross-waits between pool-hosted loops can deadlock; use asynchronous close.
	auto *pOwningLoop = _pSocket->m_pOwningLoop;
	if (pOwningLoop && _pSocket->m_pIoRegistration && pOwningLoop != mp_PollerThread.mp_pLoop && _pSocket->m_Socket != INVALID_SOCKET)
		DMibErrorNet("Synchronous close on a pool-hosted loop; use the asynchronous form");

	if (_pSocket->m_Socket != INVALID_SOCKET)
	{
		if (pOwningLoop && _pSocket->m_pIoRegistration)
		{
			pOwningLoop->f_Deregister(_pSocket->m_pIoRegistration);
			_pSocket->m_pIoRegistration = nullptr;
		}

		DMibLock(_pSocket->m_Lock);
		_pSocket->m_fOnStateChange.f_Clear();
	}

	fp_DestroySocket(_pSocket);

	return true;
}

// Consumes the socket. Registered sockets complete on their loop after removal and handle close; unregistered sockets complete inline.
void CWindowsSocketContext::f_CloseAsync(CWindowsSocket *_pSocket, NMib::NFunction::TCFunctionMovable<void ()> &&_fOnClosed)
{
	auto *pOwningLoop = _pSocket->m_pOwningLoop;

	if (pOwningLoop && _pSocket->m_pIoRegistration && _pSocket->m_Socket != INVALID_SOCKET)
	{
		// Always defer registered sockets through their loop; pool threads cannot block on each other's deregistrations.
		// The handle and socket file are unavailable for reuse until the continuation runs.
		{
			DMibLock(_pSocket->m_Lock);
			_pSocket->m_fOnStateChange.f_Clear();
		}

		pOwningLoop->f_DeregisterAsync
			(
				_pSocket->m_pIoRegistration
				, [this, _pSocket, _fOnClosed = fg_Move(_fOnClosed)]() mutable
				{
					fp_DestroySocket(_pSocket);
					if (_fOnClosed)
						_fOnClosed();
				}
			)
		;

		return;
	}

	{
		DMibLock(_pSocket->m_Lock);
		_pSocket->m_fOnStateChange.f_Clear();
	}

	fp_DestroySocket(_pSocket);
	if (_fOnClosed)
		_fOnClosed();
}

umint CWindowsSocketContext::f_Receive(CWindowsSocket *_pSocket, void *_pData, umint _DataLen, bool &o_bEndOfStream)
{
	int Ret = recv(_pSocket->m_Socket, (char *)_pData, (int)fg_Min(_DataLen, umint(INT_MAX)), 0);

	o_bEndOfStream = Ret == 0 && _DataLen != 0;

#if DMibConfig_IoDebug_Enable
	if (auto *pStats = fg_SocketIoStats())
	{
		pStats->m_nRecvCalls.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		if (Ret > 0)
		{
			pStats->m_nRecvBytes.f_FetchAdd((umint)Ret, NAtomic::gc_MemoryOrder_Relaxed);
			pStats->m_RecvSizeBuckets[fg_GetHighestBitSet((umint)Ret)].f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
			if ((umint)Ret < _DataLen)
				pStats->m_nRecvShort.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		}
		else if (Ret == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
			pStats->m_nRecvWouldBlock.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		else if (o_bEndOfStream)
			pStats->m_nRecvEndOfStream.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
	}
#endif

	if (Ret == SOCKET_ERROR)
	{
		int Error = WSAGetLastError();
		if (Error != WSAEWOULDBLOCK)
			DMibErrorNet((CStr::CFormat("Could not revc from socket, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());

		fg_RequestSocketReadiness(_pSocket, NSys::EIoLoopEvent::mc_Read);
		return 0;
	}

	if (Ret > 0 && (umint)Ret < _DataLen)
		fg_RequestSocketReadiness(_pSocket, NSys::EIoLoopEvent::mc_Read);

	return Ret;
}

umint CWindowsSocketContext::f_Send(CWindowsSocket *_pSocket, const void *_pData, umint _DataLen)
{
	int Ret = send(_pSocket->m_Socket, (const char *)_pData, (int)fg_Min(_DataLen, umint(INT_MAX)), 0);

#if DMibConfig_IoDebug_Enable
	fg_SocketIoStatsCountSend(_DataLen, Ret > 0 ? (umint)Ret : 0, Ret == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK);
#endif

	if (Ret == SOCKET_ERROR)
	{
		int Error = WSAGetLastError();
		if (Error != WSAEWOULDBLOCK)
			DMibErrorNet((CStr::CFormat("Could not sendfrom socket, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());

		Ret = 0;
	}

	if ((umint)Ret < _DataLen)
		fg_RequestSocketReadiness(_pSocket, NSys::EIoLoopEvent::mc_Write);

	return Ret;
}

umint CWindowsSocketContext::f_SendVectored(CWindowsSocket *_pSocket, NMib::NSys::CIoSpan const *_pSpans, umint _nSpans)
{
	WSABUF Buffers[NMib::NSys::gc_IoLoopMaxSubmitSpans];
	umint nBuffers = 0;
	umint nSubmittedBytes = 0;
	for (umint iSpan = 0; iSpan < _nSpans && nBuffers < NMib::NSys::gc_IoLoopMaxSubmitSpans; ++iSpan)
	{
		if (!_pSpans[iSpan].m_nBytes)
			continue;

		// A clamped 32-bit WSABUF must end the gather or later spans would overtake its unsent suffix.
		umint nSpanBytes = _pSpans[iSpan].m_nBytes;
		bool bClamped = nSpanBytes > umint(TCLimitsInt<ULONG>::mc_Max);
		if (bClamped)
			nSpanBytes = umint(TCLimitsInt<ULONG>::mc_Max);

		Buffers[nBuffers].buf = (CHAR *)_pSpans[iSpan].m_pData;
		Buffers[nBuffers].len = (ULONG)nSpanBytes;
		nSubmittedBytes += nSpanBytes;
		++nBuffers;

		if (bClamped)
			break;
	}

	if (!nBuffers)
		return 0;

	DWORD nBytesSent = 0;
	int Ret = WSASend(_pSocket->m_Socket, Buffers, (DWORD)nBuffers, &nBytesSent, 0, nullptr, nullptr);

#if DMibConfig_IoDebug_Enable
	fg_SocketIoStatsCountSend(nSubmittedBytes, Ret == SOCKET_ERROR ? 0 : (umint)nBytesSent, Ret == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK);
#endif

	if (Ret == SOCKET_ERROR)
	{
		int Error = WSAGetLastError();
		if (Error != WSAEWOULDBLOCK)
			DMibErrorNet((CStr::CFormat("Could not send on socket, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());

		nBytesSent = 0;
	}

	// Would-block and a short send both prove the buffer filled; measured against what was
	// actually handed over, since spans past the buffer cap were never submitted
	if ((umint)nBytesSent < nSubmittedBytes)
		fg_RequestSocketReadiness(_pSocket, NSys::EIoLoopEvent::mc_Write);

	return nBytesSent;
}

// The ioctl ships in afunix.h in recent Windows SDKs; define it for older SDK headers
#ifndef SIO_AF_UNIX_GETPEERPID
	#define SIO_AF_UNIX_GETPEERPID _WSAIOR(IOC_VENDOR, 256)
#endif

bool CWindowsSocketContext::f_GetProcessIdentity(CWindowsSocket *_pSocket, NMib::NSys::NNetwork::CProcessIdentity &o_LocalIdentity, NMib::NSys::NNetwork::CProcessIdentity &o_PeerIdentity)
{
	o_LocalIdentity = {};
	o_PeerIdentity = {};

	sockaddr_storage PeerAddr;
	int nAddrBytes = sizeof(PeerAddr);

	if (getpeername(_pSocket->m_Socket, (struct sockaddr *)&PeerAddr, &nAddrBytes) != 0)
		return false;

	if (PeerAddr.ss_family != AF_UNIX)
		return false;

	// Some supported kernels return a PID with zero bytes returned; validate the ioctl result and nonzero PID only.
	ULONG PeerPid = 0;
	DWORD nBytesReturned = 0;
	if (WSAIoctl(_pSocket->m_Socket, SIO_AF_UNIX_GETPEERPID, nullptr, 0, &PeerPid, sizeof(PeerPid), &nBytesReturned, nullptr, nullptr) != 0)
		return false;

	if (!PeerPid)
		return false;

	o_LocalIdentity.m_ProcessID = GetCurrentProcessId();
	o_PeerIdentity.m_ProcessID = PeerPid;

	return true;
}

umint CWindowsSocketContext::f_SendDatagram(CWindowsSocket *_pSocket, CWindowsAddress const&_Address, const void *_pData, umint _DataLen)
{
	int Ret = sendto(_pSocket->m_Socket, (const char *)_pData, (int)fg_Min(_DataLen, umint(INT_MAX)), 0, (sockaddr const*)_Address.f_Get(), _Address.f_GetSockAddrLen());

	if (Ret == SOCKET_ERROR)
	{
		int Error = WSAGetLastError();
		if (Error != WSAEWOULDBLOCK)
			DMibErrorNet((CStr::CFormat("Could not sendfrom socket, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());

		// Would-block only: a datagram result says nothing about queue occupancy short of it
		fg_RequestSocketReadiness(_pSocket, NSys::EIoLoopEvent::mc_Write);
		return 0;
	}

	return Ret;
}

umint CWindowsSocketContext::f_ReceiveDatagram(CWindowsSocket *_pSocket, CWindowsAddress &_Address, void *_pData, umint _DataLen)
{
	socklen_t Len = (socklen_t)_pSocket->m_BindAddressSize;
	int Ret = recvfrom(_pSocket->m_Socket, (char *)_pData, (int)fg_Min(_DataLen, umint(INT_MAX)), 0, (sockaddr *)_Address.f_GetForWrite(_pSocket->m_AddressType, Len), &Len);

	if (Ret == SOCKET_ERROR)
	{
		int Error = WSAGetLastError();
		if (Error != WSAEWOULDBLOCK)
			DMibErrorNet((CStr::CFormat("Could not sendfrom socket, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());

		// Would-block only: a short datagram read is a truncated datagram, not an empty queue
		fg_RequestSocketReadiness(_pSocket, NSys::EIoLoopEvent::mc_Read);
		return 0;
	}

	return Ret;
}

// *************************************************************************************************************************
// WindowsSocketContext Socket Properties & State Methods
// *************************************************************************************************************************

void CWindowsSocketContext::f_SetOnStateChange(CWindowsSocket *_pSocket, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange)
{
	DMibLock(_pSocket->m_Lock);
	_pSocket->m_fOnStateChange = fg_Move(_fOnStateChange);
}

NMib::NNetwork::ENetTCPState CWindowsSocketContext::f_GetState(CWindowsSocket *_pSocket)
{
	return (NMib::NNetwork::ENetTCPState)_pSocket->m_StateAtomic.f_Exchange(0);
}

NStr::CStr CWindowsSocketContext::f_GetCloseReason(CWindowsSocket *_pSocket)
{
	int CloseError = 0;
	bool bGracefulClose = false;

	{
		DMibLock(_pSocket->m_Lock);
		CloseError = _pSocket->m_CloseError;
		bGracefulClose = _pSocket->m_bShutdownCalled && _pSocket->m_bNonErrorClose;
	}

	if (CloseError == 0)
	{
		if (bGracefulClose)
			return gc_Str<"Connection gracefully disconnected">.m_Str;
		else
			return gc_Str<"End of file encountered">.m_Str;
	}

	if (CloseError == -1)
		return gc_Str<"Connection closed with an unknown error">.m_Str;

	return NStr::CStr(NMib::NPlatform::fg_Win32_GetLastErrorStr((uint32)CloseError));
}

CWindowsSocket* CWindowsSocketContext::f_InheritHandle2(void *_pOSSocket, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange)
{
	DMibRequire(!!_pOSSocket);
	f_CheckFailed();

	SOCKET Socket = (SOCKET)_pOSSocket;

	// Inherited handles may retain window/event selection; clearing it also establishes nonblocking mode.
	if (WSAEventSelect(Socket, nullptr, 0) != 0)
	{
		uint32 Error = WSAGetLastError();
		DMibErrorNet
			(
				"Could not clear the event selection of an inherited socket handle, windows returned: {}"_f
				<< NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)
			)
		;
	}
	fg_SetNonBlocking(Socket, "inherit");

	// Adopt before registration; reject an incompatible permanent binding rather than send completions to an unserviced port.
	NSys::ICIoLoop *pThreadLoop = NSys::fg_GetThreadIoLoop();
	NSys::ICIoLoop *pOwningLoop = pThreadLoop ? pThreadLoop : mp_PollerThread.mp_pLoop;
	int AdoptError = 0;
	if (pOwningLoop && !pOwningLoop->f_AdoptHandle((NSys::CIoLoopHandle)Socket, AdoptError))
	{
		closesocket(Socket);
		DMibErrorNet
			(
				"Cannot inherit a socket handle bound to a completion port this system will not replace; the previous owner must create it inheritable. Windows returned: {}"_f
				<< NMib::NPlatform::fg_Win32_GetLastErrorStr(AdoptError)
			)
		;
	}

	auto *pSocket = fp_CreateSocket(Socket, EWindowsSocketMode_Connect, EWindowsSocketEvent(EWindowsSocketEvent_Read | EWindowsSocketEvent_Write), fg_Move(_fOnStateChange), true);

	// Inherited sockets lack address metadata; query the family before applying TCP-only buffer policy.
	if (pSocket)
		pSocket->m_AddressType = fsg_AddressTypeOfSocket(Socket);

	return pSocket;
}

struct CWindowsSocket_OldVersion
{
	struct CLock
	{
		umint m_nLocked;
		umint m_ThreadID;
		aint m_nRecurse;
		void* m_Event;
	};

	CLock m_Lock;
	void* m_pSocket;
	void* m_pLink0;
	void* m_pLink1;

	void *m_pReportTo;

	TCAtomic<uint32> m_State;
};

// Version 0x101 ABI: the handle offset and high-bit give-up flag must match cross-module callers.
struct CWindowsSocket_V101
{
	uint32 m_Magic;
	uint32 m_Version;
	void *m_pSocket;
	TCAtomic<uint32> m_StateAtomic;
};

void *CWindowsSocketContext::f_GiveUpForInherit(CWindowsSocket *_pSocket)
{
	DMibRequire(!!_pSocket);

	if (_pSocket->m_Magic == 0x4EA11E49)
	{
		if (_pSocket->m_Version == 0x101)
		{
			auto *pOld = (CWindowsSocket_V101 *)_pSocket;
			pOld->m_StateAtomic |= DMibBit(31);
			return pOld->m_pSocket;
		}

		if (_pSocket->m_Version != 0x102)
			DMibErrorNet(fg_Format("Unsupported socket version: {nfh}", _pSocket->m_Version));
	}
	else
	{
		CWindowsSocket_OldVersion* pSocket = (CWindowsSocket_OldVersion*)_pSocket;
		auto *pSocketHandle = pSocket->m_pSocket;
		pSocket->m_State |= DMibBit(31);
		return pSocketHandle;
	}

	// Stop callbacks before removal; extract the handle only after no loop reference remains.
	{
		DMibLock(_pSocket->m_Lock);
		_pSocket->m_fOnStateChange.f_Clear();
	}

	auto *pOwningLoop = _pSocket->m_pOwningLoop;
	if (pOwningLoop && _pSocket->m_pIoRegistration)
	{
		// Pool-hosted loops require asynchronous handoff to avoid cross-thread deregistration deadlocks.
		if (pOwningLoop != mp_PollerThread.mp_pLoop)
			DMibErrorNet("Synchronous inherit handoff on a pool-hosted loop; use the asynchronous form");

		pOwningLoop->f_Deregister(_pSocket->m_pIoRegistration);
		_pSocket->m_pIoRegistration = nullptr;
	}

	SOCKET Socket = INVALID_SOCKET;
	{
		DMibLock(_pSocket->m_Lock);
		Socket = _pSocket->m_Socket;
		_pSocket->m_Socket = INVALID_SOCKET;
	}

	return (void *)Socket;
}

void CWindowsSocketContext::f_GiveUpForInheritAsync(CWindowsSocket *_pSocket, NMib::NFunction::TCFunctionMovable<void (void *_pSocketHandle)> &&_fOnHandle)
{
	// Consume the platform socket now; hand out its handle only after removal acknowledgement makes reuse safe.
	{
		DMibLock(_pSocket->m_Lock);
		_pSocket->m_fOnStateChange.f_Clear();
	}

	auto *pOwningLoop = _pSocket->m_pOwningLoop;
	if (pOwningLoop && _pSocket->m_pIoRegistration)
	{
		auto *pRegistration = _pSocket->m_pIoRegistration;
		_pSocket->m_pIoRegistration = nullptr;

		pOwningLoop->f_DeregisterAsync
			(
				pRegistration
				, [this, _pSocket, _fOnHandle = fg_Move(_fOnHandle)]() mutable
				{
					SOCKET Socket = INVALID_SOCKET;
					{
						DMibLock(_pSocket->m_Lock);
						Socket = _pSocket->m_Socket;
						_pSocket->m_Socket = INVALID_SOCKET;
					}

					fp_DestroySocket(_pSocket);
					_fOnHandle((void *)Socket);
				}
			)
		;

		return;
	}

	// Never registered: no loop to defer to, so the handle is produced on the calling thread

	SOCKET Socket = INVALID_SOCKET;
	{
		DMibLock(_pSocket->m_Lock);
		Socket = _pSocket->m_Socket;
		_pSocket->m_Socket = INVALID_SOCKET;
	}

	fp_DestroySocket(_pSocket);
	_fOnHandle((void *)Socket);
}

void CWindowsSocketContext::f_CloseSocketHandle(void *_pSocketHandle)
{
	closesocket((SOCKET)_pSocketHandle);
}

void *CWindowsSocketContext::f_GetOSSocket(CWindowsSocket *_pSocket)
{
	DMibRequire(!!_pSocket);
	return (void *)_pSocket->m_Socket;
}

CWindowsAddress* CWindowsSocketContext::f_GetPeerAddress(CWindowsSocket *_pSocket)
{
	sockaddr_storage PeerAddr;

	socklen_t nAddrBytes = sizeof(PeerAddr);

	int Ret = getpeername(_pSocket->m_Socket, (struct sockaddr *)&PeerAddr, &nAddrBytes);

	if (Ret != 0)
	{
		uint32 Error = WSAGetLastError();
		DMibErrorNet((CStr::CFormat("Could not get peer address, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
	}

	if (PeerAddr.ss_family == AF_INET)
	{
		NStorage::TCUniquePointer<CWindowsAddress> pAddress = fg_Construct(*(sockaddr_in const*)&PeerAddr);
		return pAddress.f_Detach();
	}
	else if (PeerAddr.ss_family == AF_INET6)
	{
		NStorage::TCUniquePointer<CWindowsAddress> pAddress = fg_Construct(*(sockaddr_in6 const*)&PeerAddr);
		return pAddress.f_Detach();
	}
	else if (PeerAddr.ss_family == AF_UNIX)
	{
		auto UnixAddress = *(sockaddr_un const*)&PeerAddr;
		if (nAddrBytes <= sizeof(UnixAddress.sun_family))
			UnixAddress.sun_path[0] = 0;

		CUnixAddress Address;
		Address.m_UnixAddress = UnixAddress;
		NStorage::TCUniquePointer<CWindowsAddress> pAddress = fg_Construct(Address);
		return pAddress.f_Detach();
	}
	else
	{
		return nullptr;
	}
}

uint32 CWindowsSocketContext::f_GetListenPort(CWindowsSocket *_pSocket)
{
	sockaddr_storage PeerAddr;

	socklen_t nAddrBytes = sizeof(PeerAddr);

	int Ret = getsockname(_pSocket->m_Socket, (struct sockaddr *)&PeerAddr, &nAddrBytes);

	if (Ret != 0)
	{
		uint32 Error = WSAGetLastError();
		DMibErrorNet((CStr::CFormat("Could not get socket address, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
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

umint NSys::NNetwork::fg_GetMaxUnixSocketNameLength()
{
	return CUnixAddress::mc_MaxAddressLength;
}

#include "Malterlib_Core_PlatformImp_Net.imp.h"
