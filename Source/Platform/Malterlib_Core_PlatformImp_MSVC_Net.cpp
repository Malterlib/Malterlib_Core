// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_PlatformImp_MSVC_Net.h"
#include "Malterlib_Core_Platform_Windows.h"

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

// *************************************************************************************************************************
// WindowsSocketContext Implementation
// *************************************************************************************************************************

#if DMibConfig_IoDebug_Enable
CSocketIoStats g_SocketIoStats;

static void fg_DumpSocketIoStats()
{
	auto fLoad = [](NAtomic::TCAtomic<uint64> const &_Value) -> uint64
		{
			return _Value.f_Load(NAtomic::gc_MemoryOrder_Relaxed);
		}
	;

	uint64 nRecvCalls = fLoad(g_SocketIoStats.m_nRecvCalls);
	uint64 nRecvBytes = fLoad(g_SocketIoStats.m_nRecvBytes);

	NSys::fg_ConsoleErrorOutput
		(
			NStr::fg_Format<NStr::CStrNonTracked>
			(
				"[io stats] readiness recv: calls={} bytes={} avg={} wouldBlock={} short={} endOfStream={}\n"
				, nRecvCalls
				, nRecvBytes
				, nRecvCalls ? nRecvBytes / nRecvCalls : 0
				, fLoad(g_SocketIoStats.m_nRecvWouldBlock)
				, fLoad(g_SocketIoStats.m_nRecvShort)
				, fLoad(g_SocketIoStats.m_nRecvEndOfStream)
			)
		)
	;

	for (umint iBucket = 0; iBucket < 33; ++iBucket)
	{
		uint64 nCount = fLoad(g_SocketIoStats.m_RecvSizeBuckets[iBucket]);
		if (!nCount)
			continue;

		NSys::fg_ConsoleErrorOutput
			(
				NStr::fg_Format<NStr::CStrNonTracked>
				(
					"[io stats] readiness recv size 2^{}: {}\n"
					, iBucket
					, nCount
				)
			)
		;
	}

	uint64 nSendCalls = fLoad(g_SocketIoStats.m_nSendCalls);
	uint64 nSendRequested = fLoad(g_SocketIoStats.m_nSendBytesRequested);

	NSys::fg_ConsoleErrorOutput
		(
			NStr::fg_Format<NStr::CStrNonTracked>
			(
				"[io stats] readiness send: calls={} bytesReq={} bytesSent={} avgReq={} wouldBlock={} short={}\n"
				, nSendCalls
				, nSendRequested
				, fLoad(g_SocketIoStats.m_nSendBytesSent)
				, nSendCalls ? nSendRequested / nSendCalls : 0
				, fLoad(g_SocketIoStats.m_nSendWouldBlock)
				, fLoad(g_SocketIoStats.m_nSendShort)
			)
		)
	;

	for (umint iBucket = 0; iBucket < 33; ++iBucket)
	{
		uint64 nCount = fLoad(g_SocketIoStats.m_SendSizeBuckets[iBucket]);
		if (!nCount)
			continue;

		NSys::fg_ConsoleErrorOutput
			(
				NStr::fg_Format<NStr::CStrNonTracked>
				(
					"[io stats] readiness send size 2^{}: {}\n"
					, iBucket
					, nCount
				)
			)
		;
	}

	NSys::fg_ConsoleErrorOutput
		(
			NStr::fg_Format<NStr::CStrNonTracked>
			(
				"[io stats] readiness arms: read={} write={} reports: read={} write={}\n"
				, fLoad(g_SocketIoStats.m_nReadinessArmsRead)
				, fLoad(g_SocketIoStats.m_nReadinessArmsWrite)
				, fLoad(g_SocketIoStats.m_nReadinessReportsRead)
				, fLoad(g_SocketIoStats.m_nReadinessReportsWrite)
			)
		)
	;
}

// Answered once for the process under an atomic guard rather than a function-local static: the
// build disables their thread-safe initialization, and two threads racing the first ask would
// read a zero-initialized value. 0 = not asked, 1 = a thread is asking, 2 = answered
constinit NAtomic::TCAtomic<uint32> g_SocketIoStatsState{0};
constinit bool g_bSocketIoStatsEnabled = false;

bool fg_SocketIoStatsEnabled()
{
	uint32 State = g_SocketIoStatsState.f_Load(NAtomic::gc_MemoryOrder_Acquire);
	if (State == 2) [[likely]]
		return g_bSocketIoStatsEnabled;

	uint32 Expected = 0;
	if (State == 0 && g_SocketIoStatsState.f_CompareExchangeStrong(Expected, 1, NAtomic::gc_MemoryOrder_AcquireRelease, NAtomic::gc_MemoryOrder_Acquire))
	{
		if (NMib::NSys::fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStrNonTracked("MalterlibIoStats")) == "1")
		{
			atexit(&fg_DumpSocketIoStats);
			g_bSocketIoStatsEnabled = true;
		}

		g_SocketIoStatsState.f_Store(2, NAtomic::gc_MemoryOrder_Release);

		return g_bSocketIoStatsEnabled;
	}

	while (g_SocketIoStatsState.f_Load(NAtomic::gc_MemoryOrder_Acquire) != 2)
		NSys::fg_Thread_Yield();

	return g_bSocketIoStatsEnabled;
}

static void fg_SocketIoStatsCountSend(umint _nRequested, umint _nSent, bool _bWouldBlock)
{
	if (!fg_SocketIoStatsEnabled())
		return;

	g_SocketIoStats.m_nSendCalls.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
	g_SocketIoStats.m_nSendBytesRequested.f_FetchAdd(_nRequested, NAtomic::gc_MemoryOrder_Relaxed);
	g_SocketIoStats.m_nSendBytesSent.f_FetchAdd(_nSent, NAtomic::gc_MemoryOrder_Relaxed);
	if (_nRequested)
		g_SocketIoStats.m_SendSizeBuckets[fg_GetHighestBitSet(_nRequested)].f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
	if (_bWouldBlock)
		g_SocketIoStats.m_nSendWouldBlock.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
	else if (_nSent < _nRequested)
		g_SocketIoStats.m_nSendShort.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
}
#endif

CWindowsSocketContext::CWindowsSocketContext()
{
#if DMibConfig_IoDebug_Enable
	// The exit reports register on the first ask; asking here makes every run report
	fg_SocketIoStatsEnabled();
#endif

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

	// A networking process listens for the session ending, as the socket window used to try to
	// before the loops replaced it
	NMib::NPlatform::fg_EnsureEndSessionReporting();

	// The shared loop exists before the thread that hosts it, so the thread body never checks.
	// Every socket nobody claimed is serviced from this loop; a process that cannot create one
	// has no working sockets, which the init failure reports on first use
	mp_PollerThread.mp_pLoop = fg_CreatePlatformIoLoop();
	if (mp_PollerThread.mp_pLoop)
		mp_PollerThread.f_Start(EExecutionPriority_Highest);
	else
		mp_bInitFailed = true;
}

CWindowsSocketContext::~CWindowsSocketContext()
{
	// No WSACleanup: another module in the process may still own sockets, and a cleanup here
	// would fail their later closes. Process exit reclaims the provider
	if (mp_PollerThread.mp_pLoop)
	{
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
	// The loopback fast path bypasses most of the TCP stack for connections between two sockets
	// on this machine that both opted in; on newer systems loopback is already short-circuited
	// and the option is a no-op. MalterlibLoopbackFastPath=0 leaves it off for measurement
	void fg_EnableLoopbackFastPath(SOCKET _Socket)
	{
		if (!fg_IocpLoopbackFastPathEnabled())
			return;

		int bEnable = 1;
		DWORD nBytes = 0;
		WSAIoctl(_Socket, SIO_LOOPBACK_FAST_PATH, &bEnable, sizeof(bEnable), nullptr, 0, &nBytes, nullptr, nullptr);
	}

	bool fg_IsLoopbackAddress(CWindowsAddress const &_Address)
	{
		if (_Address.f_GetType() == ENetAddressType_TCPv4)
		{
			auto const &Native = *(sockaddr_in const *)_Address.f_Get();
			return ((uint8 const *)&Native.sin_addr.s_addr)[0] == 127;
		}

		if (_Address.f_GetType() == ENetAddressType_TCPv6)
		{
			auto const &Native = *(sockaddr_in6 const *)_Address.f_Get();
			if (IN6_IS_ADDR_LOOPBACK(&Native.sin6_addr))
				return true;
			if (IN6_IS_ADDR_V4MAPPED(&Native.sin6_addr))
				return Native.sin6_addr.s6_addr[12] == 127;
		}

		return false;
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

	// Where a handle given up for inheritance was registered, keyed by the handle. A handle keeps
	// its first completion port for its lifetime, so the adopter has to know whether the loop it
	// registers with is the one that port belongs to; handles cannot be recycled while the given
	// up socket stays open, and it stays open until adopted or closed through this module
	NThread::CMutual g_GivenUpHandlesLock;
	NContainer::TCMap<umint, NSys::ICIoLoop *> g_GivenUpHandles;

	void fg_RecordGivenUpHandle(SOCKET _Socket, NSys::ICIoLoop *_pOwningLoop)
	{
		DMibLock(g_GivenUpHandlesLock);
		if (auto *pEntry = g_GivenUpHandles.f_FindEqual((umint)_Socket))
			*pEntry = _pOwningLoop;
		else
			g_GivenUpHandles.f_Insert((umint)_Socket) = _pOwningLoop;
	}

	// Removes the record and answers the loop it named, null when the handle is unknown here
	NSys::ICIoLoop *fg_TakeGivenUpHandle(SOCKET _Socket, bool &o_bKnown)
	{
		DMibLock(g_GivenUpHandlesLock);
		auto *pEntry = g_GivenUpHandles.f_FindEqual((umint)_Socket);
		o_bKnown = pEntry != nullptr;
		if (!pEntry)
			return nullptr;

		NSys::ICIoLoop *pLoop = *pEntry;
		g_GivenUpHandles.f_Remove((umint)_Socket);
		return pLoop;
	}
}

// The one decoder from loop readiness to socket state: the loop reports normalized EIoLoopEvent
// bits and this is the only place that knows what they mean for a socket. Runs on the thread
// driving the socket's loop, at the point in the pass where the event was reaped, so state
// callbacks keep their ordering against everything else the pass dispatches
static void fg_DispatchSocketIoEvent(void *_pToken, NSys::EIoLoopEvent _Events, int _Error)
{
	CWindowsSocket *pSocket = (CWindowsSocket *)_pToken;

	DMibLock(pSocket->m_Lock);

	if (_Events == NSys::EIoLoopEvent::mc_None)
	{
		// The registration has been applied: report the state that accumulated before the loop
		// could deliver anything, so a connection with pre-registration readiness is not stalled
		// waiting for a fresh edge
		if (pSocket->m_fOnStateChange)
			pSocket->m_fOnStateChange((ENetTCPState)pSocket->m_StateAtomic.f_Load());

		return;
	}

#if DMibConfig_IoDebug_Enable
	if (fg_SocketIoStatsEnabled())
	{
		if (fg_IsSet(_Events, NSys::EIoLoopEvent::mc_Read))
			g_SocketIoStats.m_nReadinessReportsRead.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		if (fg_IsSet(_Events, NSys::EIoLoopEvent::mc_Write))
			g_SocketIoStats.m_nReadinessReportsWrite.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
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
		// The connection is gone; an abortive close carries its reason in the socket error
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
		// The peer's half close after this side's own is the full close — what epoll reports as
		// a hangup once both halves are shut. AFD has no event for the pair, so the socket keeps
		// the score: a shutdown already called makes this the end of the connection, graceful
		// unless the socket error says otherwise. Like the hangup it stands in for, it is
		// reported on its own; any readable bytes ahead of the close are drained by the consumer
		if (pSocket->m_bShutdownCalled)
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

	if (_bFromInherit)
		pNewSocket->m_bInitialWriteNotification = false;

	NMib::NNetwork::ENetTCPState StateAdded = NMib::NNetwork::ENetTCPState_Read | NMib::NNetwork::ENetTCPState_Write; // Kickstart

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
			fg_EnableLoopbackFastPath(hSock);
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
	// Kernel default socket buffers quantize a bulk stream into buffer-sized bursts with a wake
	// handoff between each, which caps per-socket throughput at roughly buffer size over wake
	// round-trip time. The override is a debugging aid for measuring that effect
	if (umint nBufferBytes = fg_IocpSocketBufferBytesOverride(); nBufferBytes && _pSocket->m_Socket != INVALID_SOCKET)
	{
		int BufferSize = (int)fg_Min(nBufferBytes, umint(INT_MAX));
		setsockopt(_pSocket->m_Socket, SOL_SOCKET, SO_SNDBUF, (char const *)&BufferSize, sizeof(BufferSize));
		setsockopt(_pSocket->m_Socket, SOL_SOCKET, SO_RCVBUF, (char const *)&BufferSize, sizeof(BufferSize));
	}

	if (_pSocket->m_pIoRegistration)
		DMibErrorNet("Windows socket already registered");

	NSys::EIoLoopEvent EventMask = fg_IoLoopMaskFromSocketEvents(_pSocket->m_RegisteredEvents);
	if (EventMask == NSys::EIoLoopEvent::mc_None)
		DMibErrorNet("Failed to register Windows socket.");

	NSys::ICIoLoop *pThreadLoop = NSys::fg_GetThreadIoLoop();
	_pSocket->m_pOwningLoop = pThreadLoop ? pThreadLoop : mp_PollerThread.mp_pLoop;

	// The registration-applied notification is the read kickstart: connections whose readable
	// state predates the registration get it reported once the add lands
	_pSocket->m_pIoRegistration = _pSocket->m_pOwningLoop->f_Register
		(
			(NSys::CIoLoopHandle)_pSocket->m_Socket
			, _pSocket
			, EventMask
			, &fg_DispatchSocketIoEvent
			, fg_IsSet(EventMask, NSys::EIoLoopEvent::mc_Read) != 0
		)
	;
}

// A would-block observation is the only point where requesting the next readiness report means
// anything: the single-shot polls arm exactly here. Short stream transfers count as would-block
// — a short recv proves the receive queue was emptied and a short send proves the buffer filled
// — so consumers that stop at a short result without driving on to would-block cannot strand.
// End of stream deliberately does not count: nothing further is coming, and the close events
// report it
static void fg_RequestSocketReadiness(CWindowsSocket *_pSocket, NSys::EIoLoopEvent _EventMask)
{
	if (!_pSocket->m_pOwningLoop || !_pSocket->m_pIoRegistration)
		return;

#if DMibConfig_IoDebug_Enable
	if (fg_SocketIoStatsEnabled())
	{
		if (fg_IsSet(_EventMask, NSys::EIoLoopEvent::mc_Read))
			g_SocketIoStats.m_nReadinessArmsRead.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		if (fg_IsSet(_EventMask, NSys::EIoLoopEvent::mc_Write))
			g_SocketIoStats.m_nReadinessArmsWrite.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
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

	// Null for a socket serviced by the shared poller thread: only created loops are bindings a
	// caller can restore, and the shared poller is what a null binding already means
	NSys::ICIoLoop *pOwningLoop = pSocket->m_pOwningLoop;
	if (!pOwningLoop || !pOwningLoop->m_bCreatedAsLoop)
		return nullptr;

	return pOwningLoop;
}

namespace
{
	// Whether transfers submitted against this socket complete on its loop's port. A fresh
	// handle is bound at registration; an adopted one keeps its first port, and is capable only
	// if that port is this loop's — the give-up record says which loop that was — or the loop
	// managed to rebind it
	bool fg_CompletionPortIsOwningLoops(CWindowsSocket *_pSocket)
	{
		if (!_pSocket->m_pIoRegistration)
			return false;

		if (_pSocket->m_bInheritedFromOwningLoop)
			return true;

		return static_cast<CIocpRegistration const *>(_pSocket->m_pIoRegistration)->m_bAssociated;
	}
}

// Completion transfers for every stream socket on a loop that offers them, local peers included:
// readiness on this platform costs a poll request per arm on top of the transfer call, while an
// overlapped transfer is one submission, so the parity argument that keeps POSIX local peers on
// readiness does not hold here
bool NSys::NNetwork::fg_SupportsCompletionIo(void *_pSocket)
{
	CWindowsSocket *pSocket = (CWindowsSocket *)_pSocket;

	// Streams only: datagram and listen sockets have no byte stream to complete into, and the
	// connecting mode is included because it settles into connect without changing loops
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

bool NSys::NNetwork::fg_SubmitSendVectored(void *_pSocket, NSys::CIoSpan const *_pSpans, umint _nSpans, NSys::FIoCompletion &&_fOnComplete, NSys::FIoBufferReleased &&_fOnBufferReleased)
{
	CWindowsSocket *pSocket = (CWindowsSocket *)_pSocket;
	if (!pSocket->m_pOwningLoop || !pSocket->m_pIoRegistration)
		return false;

#if DMibConfig_IoDebug_Enable
	// Applied at the first completion send rather than at start: a zero send buffer makes the
	// readiness path's non-blocking send unable to queue anything until the peer has a receive
	// pending, which strands a handshake. From here on every send is an overlapped one
	if (umint nSendBufferBytes = fg_IocpSocketSendBufferBytesOverride(); nSendBufferBytes != umint(-1) && !pSocket->m_bSendBufferOverrideApplied)
	{
		pSocket->m_bSendBufferOverrideApplied = true;
		int BufferSize = (int)fg_Min(nSendBufferBytes, umint(TCLimitsInt<int>::mc_Max));
		setsockopt(pSocket->m_Socket, SOL_SOCKET, SO_SNDBUF, (char const *)&BufferSize, sizeof(BufferSize));
	}
#endif

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
		fg_EnableLoopbackFastPath(hSock);

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
	}

	// Explicit rather than trusting inheritance from the listener
	fg_SetNonBlocking(hSock, "accept");

	Cleanup.f_Clear();

	auto *pSocket = fp_CreateSocket(hSock, EWindowsSocketMode_Connect, EWindowsSocketEvent(EWindowsSocketEvent_Read | EWindowsSocketEvent_Write), fg_Move(_fOnStateChange));
	pSocket->m_AddressType = _pSocket->m_AddressType;

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

		// The peer's half close was reported before this shutdown, so the shutdown completes the
		// pair — and AFD reports nothing for that. The close class is asked for again; the loop
		// answers with the disconnect it already holds, which the dispatch reads as the full
		// close now that the shutdown is on record. Decided under the lock the dispatch holds, so
		// a disconnect landing concurrently sees the shutdown itself and exactly one of the two
		// paths reports the close
		if (bRemoteCloseSignalled)
			fg_RequestSocketReadiness(_pSocket, NSys::EIoLoopEvent::mc_ReadClosed);
	}

	return true;
}

void CWindowsSocketContext::fp_DestroySocket(CWindowsSocket *_pSocket)
{
	if (_pSocket->m_Socket != INVALID_SOCKET)
	{
		DMibLock(_pSocket->m_Lock);
		closesocket(_pSocket->m_Socket);
		_pSocket->m_Socket = INVALID_SOCKET;
	}

	// The unix listen state removes its port file with it
	_pSocket->m_pUnixListen.f_Clear();

	fg_DeleteObject(NMemory::CDefaultAllocator(), _pSocket);
}

bool CWindowsSocketContext::f_Close(CWindowsSocket *_pSocket)
{
	// The synchronous form is legal only where blocking on the acknowledgement is: the shared
	// poller runs on a thread of its own that is always responsive. A socket on a pool-hosted
	// loop must use the asynchronous form — a blocking wait here could deadlock two pool threads
	// deregistering into each other's loops, and quietly deferring the close would leave the
	// caller believing the handle and a listener's socket file are gone when they are not
	auto *pOwningLoop = _pSocket->m_pOwningLoop;
	if (pOwningLoop && _pSocket->m_pIoRegistration && pOwningLoop != mp_PollerThread.mp_pLoop && _pSocket->m_Socket != INVALID_SOCKET)
		DMibErrorNet("Synchronous close on a pool-hosted loop; use the asynchronous form");

	f_CloseAsync(_pSocket, {});

	return true;
}

void CWindowsSocketContext::f_CloseAsync(CWindowsSocket *_pSocket, NMib::NFunction::TCFunctionMovable<void ()> &&_fOnClosed)
{
	auto *pOwningLoop = _pSocket->m_pOwningLoop;

	if (pOwningLoop && pOwningLoop != mp_PollerThread.mp_pLoop && _pSocket->m_Socket != INVALID_SOCKET)
	{
		// The socket belongs to a loop hosted on a pool thread, and pool threads never block, so
		// the removal cannot be waited for: whoever hosts the loop may be closing a socket of
		// this thread's loop at the same time, and the acknowledgement may need actor jobs to run
		// before it can be produced. No callback fires after the clear below, and the loop
		// destroys the socket once the removal has been applied. The handle, and a listener's
		// socket file with it, are gone only when the continuation runs, so an owner that reuses
		// the name must wait for it
		{
			DMibLock(_pSocket->m_Lock);
			_pSocket->m_fOnStateChange.f_Clear();
		}

		if (_pSocket->m_pIoRegistration)
		{
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

		fp_DestroySocket(_pSocket);
		if (_fOnClosed)
			_fOnClosed();

		return;
	}

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
	if (_fOnClosed)
		_fOnClosed();
}

umint CWindowsSocketContext::f_Receive(CWindowsSocket *_pSocket, void *_pData, umint _DataLen, bool &o_bEndOfStream)
{
	int Ret = recv(_pSocket->m_Socket, (char *)_pData, (int)fg_Min(_DataLen, umint(INT_MAX)), 0);

	o_bEndOfStream = Ret == 0 && _DataLen != 0;

#if DMibConfig_IoDebug_Enable
	if (fg_SocketIoStatsEnabled())
	{
		g_SocketIoStats.m_nRecvCalls.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		if (Ret > 0)
		{
			g_SocketIoStats.m_nRecvBytes.f_FetchAdd((umint)Ret, NAtomic::gc_MemoryOrder_Relaxed);
			g_SocketIoStats.m_RecvSizeBuckets[fg_GetHighestBitSet((umint)Ret)].f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
			if ((umint)Ret < _DataLen)
				g_SocketIoStats.m_nRecvShort.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		}
		else if (Ret == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
			g_SocketIoStats.m_nRecvWouldBlock.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
		else if (o_bEndOfStream)
			g_SocketIoStats.m_nRecvEndOfStream.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);
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

		// WSABUF lengths are 32-bit: an oversized span is sent as a clamped prefix and ends
		// the gather, since buffers after a partial span would go out on the wire out of order.
		// The caller's partial-send handling resumes with the remainder
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

	// Only a connected unix domain socket carries a kernel-authenticated peer process id
	sockaddr_storage PeerAddr;
	int nAddrBytes = sizeof(PeerAddr);

	if (getpeername(_pSocket->m_Socket, (struct sockaddr *)&PeerAddr, &nAddrBytes) != 0)
		return false;

	if (PeerAddr.ss_family != AF_UNIX)
		return false;

	// SIO_AF_UNIX_GETPEERPID reports the pid captured by the kernel when the peer connected. A
	// Windows build too old to support the ioctl fails here, and the authenticated unix
	// handshake fails closed with it. Success is judged by the call result and a nonzero pid
	// only: some supported builds (Windows 10 1903) fill in the pid but leave the returned
	// byte count at zero
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
			return "Connection gracefully disconnected";
		else
			return "End of file encountered";
	}

	if (CloseError == -1)
		return "Connection closed with an unknown error";

	return NStr::CStr(NMib::NPlatform::fg_Win32_GetLastErrorStr((uint32)CloseError));
}

CWindowsSocket* CWindowsSocketContext::f_InheritHandle2(void *_pOSSocket, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange)
{
	DMibRequire(!!_pOSSocket);
	f_CheckFailed();

	SOCKET Socket = (SOCKET)_pOSSocket;

	// A handle from an older module may still carry a window or event selection; clearing it is
	// also what puts the handle into the non-blocking mode a fresh selection would have
	WSAEventSelect(Socket, nullptr, 0);
	fg_SetNonBlocking(Socket, "inherit");

	bool bKnown = false;
	NSys::ICIoLoop *pPreviousLoop = fg_TakeGivenUpHandle(Socket, bKnown);

	auto *pSocket = fp_CreateSocket(Socket, EWindowsSocketMode_Connect, EWindowsSocketEvent(EWindowsSocketEvent_Read | EWindowsSocketEvent_Write), fg_Move(_fOnStateChange), true);

	// Settled at start, once the owning loop is known: the handle's port is that loop's if the
	// give-up happened from the same loop
	NSys::ICIoLoop *pThreadLoop = NSys::fg_GetThreadIoLoop();
	NSys::ICIoLoop *pOwningLoop = pThreadLoop ? pThreadLoop : mp_PollerThread.mp_pLoop;
	pSocket->m_bInheritedFromOwningLoop = bKnown && pPreviousLoop == pOwningLoop;

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

// The 0x101 layout: a module built before the io loop hands its sockets over with the handle at
// this offset and the give-up flag in the state word's top bit
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
		pSocket->m_State |= DMibBit(31); // Hopefully this is good enough. Old versions needed this to be locked
		return pSocketHandle;
	}

	// No callback fires after this clear. The handle is extracted only after the registration is
	// fully removed, so the loop never holds a reference to a socket whose handle belongs to the
	// caller
	{
		DMibLock(_pSocket->m_Lock);
		_pSocket->m_fOnStateChange.f_Clear();
	}

	auto *pOwningLoop = _pSocket->m_pOwningLoop;
	if (pOwningLoop && _pSocket->m_pIoRegistration)
	{
		// The synchronous form is legal only where blocking on the acknowledgement is: the
		// shared poller runs on a thread of its own that is always responsive. A socket on a
		// pool-hosted loop must use the asynchronous form — a blocking wait here could deadlock
		// two pool threads deregistering into each other's loops
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

	if (Socket != INVALID_SOCKET)
		fg_RecordGivenUpHandle(Socket, pOwningLoop);

	return (void *)Socket;
}

void CWindowsSocketContext::f_GiveUpForInheritAsync(CWindowsSocket *_pSocket, NMib::NFunction::TCFunctionMovable<void (void *_pSocketHandle)> &&_fOnHandle)
{
	// Acknowledge-first handoff: the caller receives the handle only after the loop has removed
	// the registration and nothing loop-side references the socket, so the new owner may close,
	// reuse, or re-register it freely. The platform socket is consumed here — the caller must
	// not touch it after this call, and the continuation destroys it
	{
		DMibLock(_pSocket->m_Lock);
		_pSocket->m_fOnStateChange.f_Clear();
	}

	auto *pOwningLoop = _pSocket->m_pOwningLoop;
	if (pOwningLoop && _pSocket->m_pIoRegistration && pOwningLoop != mp_PollerThread.mp_pLoop)
	{
		auto *pRegistration = _pSocket->m_pIoRegistration;
		_pSocket->m_pIoRegistration = nullptr;

		pOwningLoop->f_DeregisterAsync
			(
				pRegistration
				, [this, _pSocket, pOwningLoop, _fOnHandle = fg_Move(_fOnHandle)]() mutable
				{
					SOCKET Socket = INVALID_SOCKET;
					{
						DMibLock(_pSocket->m_Lock);
						Socket = _pSocket->m_Socket;
						_pSocket->m_Socket = INVALID_SOCKET;
					}

					if (Socket != INVALID_SOCKET)
						fg_RecordGivenUpHandle(Socket, pOwningLoop);

					fp_DestroySocket(_pSocket);
					_fOnHandle((void *)Socket);
				}
			)
		;

		return;
	}

	// Shared poller or never registered: the blocking removal is safe here and the handle is
	// produced on the calling thread
	if (pOwningLoop && _pSocket->m_pIoRegistration)
	{
		pOwningLoop->f_Deregister(_pSocket->m_pIoRegistration);
		_pSocket->m_pIoRegistration = nullptr;
	}

	SOCKET Socket = INVALID_SOCKET;
	{
		DMibLock(_pSocket->m_Lock);
		Socket = _pSocket->m_Socket;
		_pSocket->m_Socket = INVALID_SOCKET;
	}

	if (Socket != INVALID_SOCKET)
		fg_RecordGivenUpHandle(Socket, pOwningLoop);

	fp_DestroySocket(_pSocket);
	_fOnHandle((void *)Socket);
}

void CWindowsSocketContext::f_CloseSocketHandle(void *_pSocketHandle)
{
	SOCKET Socket = (SOCKET)_pSocketHandle;

	bool bKnown = false;
	fg_TakeGivenUpHandle(Socket, bKnown);

	closesocket(Socket);
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
