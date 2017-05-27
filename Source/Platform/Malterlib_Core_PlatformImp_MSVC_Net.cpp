// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_PlatformImp_MSVC_Net.h"

// *************************************************************************************************************************
// CWindowsSocket Implementation
// *************************************************************************************************************************

CWindowsSocket::CWindowsSocket()
{
	m_StateAtomic = 0;
	m_pSocket = nullptr;
#ifdef DTCPDelayEmulation
	m_DelayedData = 0;
	m_bDelayedStuffed = 0;
#endif

	m_Magic = 0x4EA11E49;
	m_Version = 0x101;
}

CWindowsSocket::~CWindowsSocket()
{
	m_OnStateChange.f_Clear();
#ifdef DTCPDelayEmulation
	m_DelayedPackets.f_DeleteAll();
#endif
	if (m_pSocket && !(m_StateAtomic.f_Load() & DMibBit(31)))
		closesocket((SOCKET)m_pSocket);
}

#ifdef DTCPDelayEmulation

void CWindowsSocket::f_UpdateDelayedSend(const NTime::CTime &_Now)
{
	mint LastDelayedData;
	mint NewDelayedData;
	bint bDelayedStuffed;
	{
		DMibLock(m_DelayedLock);
		LastDelayedData = m_DelayedData;
		CDelayedPacket *pPacket = m_DelayedPackets.f_GetFirst();
		while (pPacket)
		{
			if (_Now > pPacket->m_SendTime)
			{
				mint Data = pPacket->m_Data.f_GetLen() - pPacket->m_SentData;
				int Ret = send((SOCKET)m_pSocket, (const char *)pPacket->m_Data.f_GetArray() + pPacket->m_SentData, Data, 0);

				if (Ret == SOCKET_ERROR)
				{
					break;
				}
				else
					pPacket->m_SentData += Ret;

				if (pPacket->m_SentData == pPacket->m_Data.f_GetLen())
				{
					m_DelayedData -= pPacket->m_SentData;
					delete pPacket;
					pPacket = m_DelayedPackets.f_GetFirst();
				}
				else
					break;
			}
			else
				break;
		}
		NewDelayedData = m_DelayedData;
		bDelayedStuffed = m_bDelayedStuffed;
	}
	if (bDelayedStuffed && NewDelayedData < DTCPDelayEmulation_MaxQueue)
	{
		DMibLockTyped(NThread::CMutual, mp_Lock);
		m_State |= NMib::NNet::ENetTCPState_Write;
		if (m_OnStateChange)
			m_OnStateChange(NMib::NNet::ENetTCPState_Write);
	}
}

#endif

// *************************************************************************************************************************
// WindowsSocketContext Implementation
// *************************************************************************************************************************

CWindowsSocketContext::CWindowsSocketContext()
{
	mp_ThreadStartEvent.f_ResetSignaled();
	mp_bInitFailed = false;
	mp_hThread = nullptr;
	mp_hReportWnd = nullptr;
	mp_ThreadID = -1;

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
}

CWindowsSocketContext::~CWindowsSocketContext()
{
	f_StopThread();
}


void CWindowsSocketContext::f_StopThread()
{
	DMibLock(mp_ThreadStartLock);
	if (NMib::NThread::CThread::f_GetState() == EThreadState_Running)
	{
		PostThreadMessage(mp_ThreadID, WM_QUIT, 0, 0);
		f_Stop();
		mp_ThreadStartEvent.f_ResetSignaled();
	}
}

void CWindowsSocketContext::f_StartThread()
{
	DMibLock(mp_ThreadStartLock);
	if (NMib::NThread::CThread::f_GetState() != EThreadState_Running)
	{
		f_Start(EThreadPriority_High);
		mp_ThreadStartEvent.f_Wait();
	}
}

NStr::CStr CWindowsSocketContext::f_GetThreadName()
{
	return "Malterlib_Core_PlatformImp_WindowsSocketContext";
}

void CWindowsSocketContext::f_CheckFailed()
{
	if (mp_bInitFailed)
		DMibErrorNet("Initziation of WinSock has faild, cannot use net");
}

void CWindowsSocketContext::f_CheckDestroy()
{
	bint bCanDestroy = true;
	{
		DMibLockTyped(NMib::NThread::CMutual, mp_Lock);
		if (!mp_SocketTree.f_IsEmpty())
			bCanDestroy = false;
	}
	{
		if (!mp_Resolver.f_IsEmpty())
			bCanDestroy = false;
	}

	if (bCanDestroy)
		f_StopThread();
}

LRESULT WINAPI CWindowsSocketContext::fsp_SocketWindowProc(HWND _hWnd, UINT _Message, WPARAM _wParam, LPARAM _lParam)
{
	return DefWindowProc(_hWnd, _Message, _wParam, _lParam);
}

aint CWindowsSocketContext::f_Main()
{
	mp_hThread = GetCurrentThread();
	mp_ThreadID = GetCurrentThreadId();

	mint ProcessId = (mint)GetCurrentProcessId();

	CFStr256 FormatFormat;
	FormatFormat = CFStr256::CFormat("NMib" DMibSystemManagerPrefix "PID_0x{nfh,sf0,sj*2}_THIS_0x{nfh,sf0,sj*2}") << ProcessId << (mint)this << sizeof(ProcessId) * 2 ;

	CFStr256 ClassName = CFStr256::CFormat("MalterlibSocketReportClass_{}") << FormatFormat;

	WNDCLASSA WndClass;
	memset(&WndClass, 0, sizeof(WndClass));
	WndClass.lpszClassName = ClassName ;
	WndClass.lpfnWndProc = fsp_SocketWindowProc;
	WndClass.hInstance = g_hDllInstance;
	if (!RegisterClassA(&WndClass))
	{
		mp_bInitFailed = true;
		mp_ThreadStartEvent.f_SetSignaled();
		return 0;
	}

	mp_hReportWnd = CreateWindowA(ClassName, ClassName, 0, 0, 0, 0, 0, HWND_MESSAGE, 0, 0, 0);

	if (!mp_hReportWnd)
	{
		UnregisterClassA(ClassName, g_hDllInstance);
		mp_bInitFailed = true;
		mp_ThreadStartEvent.f_SetSignaled();
		return 0;
	}

#ifdef DTCPDelayEmulation
	SetTimer(nullptr, 1, 10, nullptr);
#endif
	mp_ThreadStartEvent.f_SetSignaled();

	while (NThread::CThread::f_GetState() != NThread::EThreadState_EventWantQuit)
	{
		int32 Ret;

		MSG Message;

		Ret = GetMessage( &Message, nullptr, 0, 0 );
		if (Ret == -1 || Ret == 0 || Message.message == WM_QUIT)
		{
			// handle the error and possibly exit
			goto ExitThread;
		}
		else if (Message.message == WM_TIMER)
		{
			// Kickstart buggy drivers
			CWindowsSocket *pSocket;
			NTime::CTime Now = NTime::CTime::fs_NowUTC();
			{
				DMibLockTyped(NThread::CMutual, mp_Lock);
				auto Iter = mp_SocketTree.f_GetIterator();
				while (Iter)
				{
					pSocket = Iter;
#ifdef DTCPDelayEmulation
					pSocket->f_UpdateDelayedSend(Now);
#endif
					++Iter;
				}
			}
		}
		else if (Message.message == WM_USER)
		{
			void *hSocket = (void *)Message.wParam;

			CWindowsSocket *pSocket;
			{
				DMibLockTyped(NThread::CMutual, mp_Lock);
				pSocket = mp_SocketTree.f_FindEqual(hSocket);

				if (pSocket)
				{
					DMibLockTyped(NThread::CMutual, pSocket->m_Lock);
					{

						DMibUnlockTyped(NThread::CMutual, mp_Lock);
//									mint Error = WSAGETSELECTERROR(Message.lParam);
						mint Event = WSAGETSELECTEVENT(Message.lParam);

						{
							NMib::NNet::ENetTCPState StateAdded = NMib::NNet::ENetTCPState_None;
							if (Event & FD_READ)
								StateAdded |= NMib::NNet::ENetTCPState_Read;
							if (Event & FD_WRITE)
							{
#ifndef DTCPDelayEmulation
								StateAdded |= NMib::NNet::ENetTCPState_Write;
#else
								if (!bDTCPDelayEmulation)
									StateAdded |= NMib::NNet::ENetTCPState_Write;
#endif

							}
							if (Event & FD_ACCEPT)
								StateAdded |= NMib::NNet::ENetTCPState_Connection;
							if (Event & FD_CONNECT)
							{
								int Error = WSAGETSELECTERROR(Message.lParam);
								if (Error)
								{
									StateAdded |= NMib::NNet::ENetTCPState_Closed;
									pSocket->m_CloseReason = NMib::NPlatform::fg_Win32_GetLastErrorStr(Error);
								}
								else
								{
									StateAdded |= NMib::NNet::ENetTCPState_Connected;
								}
							}
							if (Event & FD_CLOSE)
							{
								int Error = WSAGETSELECTERROR(Message.lParam);
								StateAdded |= NMib::NNet::ENetTCPState_Closed;
								if (Error)
									pSocket->m_CloseReason = NMib::NPlatform::fg_Win32_GetLastErrorStr(Error);
								else
									pSocket->m_CloseReason = "Connection gracefully disconnected.";
							}

							if (StateAdded)
							{
								pSocket->m_StateAtomic |= StateAdded;
								if (pSocket->m_OnStateChange)
									pSocket->m_OnStateChange(StateAdded);
							}
						}
					}
				}

			}
		}
		else
		{
			TranslateMessage(&Message); 
			DispatchMessage(&Message); 
		}
	}

ExitThread:
	if (mp_hReportWnd)
	{
		DestroyWindow(mp_hReportWnd);
		mp_hReportWnd = nullptr;
	}

	UnregisterClassA(ClassName, g_hDllInstance);
	
	return 0;
}

bint CWindowsSocketContext::f_IsEmpty()
{
	return mp_SocketTree.f_IsEmpty();
}

// *************************************************************************************************************************
// WindowsSocketContext Address Methods
// *************************************************************************************************************************

CWindowsAddress* CWindowsSocketContext::f_CreateAddress(NMib::NNet::ENetAddressType _Type, void const* _pData, mint _nDataBytes)
{
	switch(_Type)
	{

		case NMib::NNet::ENetAddressType_TCPv4:
			{
				if (_nDataBytes != sizeof(NMib::NNet::CNetAddressTCPv4))
					return nullptr;

				sockaddr_in NativeAddr;
				fp_ToNative(*(NMib::NNet::CNetAddressTCPv4*)_pData, NativeAddr);

				return DMibNew CWindowsAddress(NativeAddr);
			}
			break;

		case NMib::NNet::ENetAddressType_TCPv6:
			{
				if (_nDataBytes != sizeof(NMib::NNet::CNetAddressTCPv6))
					return nullptr;

				sockaddr_in6 NativeAddr;
				fp_ToNative(*(NMib::NNet::CNetAddressTCPv6*)_pData, NativeAddr);

				return DMibNew CWindowsAddress(NativeAddr);
			}
			break;

		case NMib::NNet::ENetAddressType_Unix:
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

NMib::NNet::ENetAddressType CWindowsSocketContext::f_GetAddressType(CWindowsAddress const& _Address)
{
	return _Address.f_GetType();
}

bint CWindowsSocketContext::f_GetAddressRaw(CWindowsAddress const& _Address, NMib::NNet::ENetAddressType _ExpectedType, void* _opRawData, mint _nDataBytes)
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
				return false; 
			}
	}
}

CWindowsAddress* CWindowsSocketContext::f_SetAddressRaw(CWindowsAddress* _pAddress, ::NMib::NNet::ENetAddressType _Type, void const* _pRawData, mint _nDataBytes)
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
				f_FreeAddress(_pAddress);
				return nullptr;
			}
	}
}

CWindowsAddress* CWindowsSocketContext::f_ResolveAddress(const NMib::NStr::CStr &_Address, NMib::NNet::ENetAddressType _PreferType)
{
	return f_ResolveAddress(_Address, _PreferType, true);
}

CWindowsAddress* CWindowsSocketContext::f_ResolveAddress(const NMib::NStr::CStr &_Address, NMib::NNet::ENetAddressType _PreferType, bint _bThrowOnError)
{
	f_CheckFailed();

	NMib::NPtr::TCUniquePointer<CWindowsAddress> pAddress = fg_Construct();

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

		if (Address.f_GetLen() > (CUnixAddress::mc_MaxLength - 1))
		{
			if (_bThrowOnError)
				DMibErrorNet(fg_Format("Unix sockets support a maximum path length of {} characters", (CUnixAddress::mc_MaxLength - 1)));
			else
				return nullptr;
		}
			
		CUnixAddress AddressWithPermissions;
		AddressWithPermissions.m_Permissions = Permissions;

		NMib::NStr::fg_StrCopy(AddressWithPermissions.m_FilePath, Address, CUnixAddress::mc_MaxLength);
		pAddress->f_Set(AddressWithPermissions);
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
	
	if (_PreferType == ENetAddressType_TCPv6)
		AddrHint.ai_family = AF_INET6;
	else
		AddrHint.ai_family = AF_INET;

	AddrHint.ai_socktype = SOCK_STREAM;

	AddrHint.ai_flags = AI_ADDRCONFIG; 

	ADDRINFOW* pAddresses = nullptr;

	CWStr AddressStrWin = NStr::NPlatform::fg_StrToWindows(AddressStr);

	int Result = GetAddrInfoW(AddressStrWin.f_GetStr(), nullptr, &AddrHint, &pAddresses);

	// Try TCPv4 first, then v6.
	if (_PreferType == ENetAddressType_None && Result != 0)
	{
		AddrHint.ai_family = AF_INET6;
		Result = GetAddrInfoW(AddressStrWin.f_GetStr(), nullptr, &AddrHint, &pAddresses);
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

void *CWindowsSocketContext::f_AsyncResolveAddress_Open(const NMib::NStr::CStr &_Address, ::NMib::NNet::ENetAddressType _PreferType, NMib::NFunction::TCFunction<void ()>&& _fOnFinish)
{
	return mp_Resolver.f_Open(_Address, _PreferType, fg_Move(_fOnFinish));
}

bint CWindowsSocketContext::f_AsyncResolveAddress_GetResult(void *_pResolver, CWindowsAddress*& _opAddress, NMib::NStr::CStr &_Error)
{
	return mp_Resolver.f_GetResult(_pResolver, (NMib::NSys::NNet::CAddress&)_opAddress, _Error);
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
	delete _pAddress;
}

NMib::NStr::CStr CWindowsSocketContext::f_GetAddressString(CWindowsAddress const& _Address, bint _bIncludeType)
{
	NMib::NStr::CStr AddressStr;

	switch(_Address.f_GetType())
	{
	case ENetAddressType_TCPv4:
		{
			if (_bIncludeType)
				AddressStr += "TCPv4:";

			CNetAddressTCPv4 TCPv4;
			f_GetAddressRaw(_Address, ENetAddressType_TCPv4, &TCPv4, sizeof(TCPv4));

			AddressStr += NMib::NStr::CStr::CFormat("{}.{}.{}.{}:{}") << 
													TCPv4.m_IP[0] << TCPv4.m_IP[1] << TCPv4.m_IP[2] << TCPv4.m_IP[3] <<
													TCPv4.m_Port;
			break;
		}
	case ENetAddressType_TCPv6:
		{
			if (_bIncludeType)
				AddressStr += "TCPv6:";

			CNetAddressTCPv6 TCPv6;
			f_GetAddressRaw(_Address, ENetAddressType_TCPv6, &TCPv6, sizeof(TCPv6));

			AddressStr += NMib::NStr::CStr::CFormat("{nfh}{nfh}:{nfh}{nfh}:{nfh}{nfh}:{nfh}{nfh}:{nfh}{nfh}:{nfh}{nfh}:{nfh}{nfh}:{nfh}{nfh}:{}") <<
													TCPv6.m_IP[0] << TCPv6.m_IP[1] << TCPv6.m_IP[2] << TCPv6.m_IP[3] <<
													TCPv6.m_IP[4] << TCPv6.m_IP[5] << TCPv6.m_IP[6] << TCPv6.m_IP[7] <<
													TCPv6.m_IP[8] << TCPv6.m_IP[9] << TCPv6.m_IP[10] << TCPv6.m_IP[11] <<
													TCPv6.m_IP[12] << TCPv6.m_IP[13] << TCPv6.m_IP[14] << TCPv6.m_IP[15] <<
													TCPv6.m_Port;
			break;
		}
	case NMib::NNet::ENetAddressType_Unix:
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
				
			AddressStr += Address.m_FilePath; 
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

CWindowsSocket *CWindowsSocketContext::fp_Connect(CWindowsAddress const& _Address, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange, bint _bAsyncConnect, CWindowsAddress const *_pBindAddress)
{
	f_CheckFailed();

	CWindowsAddress Address = _Address;

	if (Address.f_GetType() == ENetAddressType_Unix)
	{
		CUnixAddress const &UnixAddress = Address.f_GetUnix();

		CStr UnixFileName = UnixAddress.m_FilePath;
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

		NNet::CNetAddress NetAddress{ConnectAddress};

		Address = *((CWindowsAddress *)NetAddress.f_AccessRaw());
	}

	ENetAddressType AddressType = Address.f_GetType();
	SOCKET hSock = INVALID_SOCKET;
	bint bConnected = false;

	if (	AddressType == ENetAddressType_TCPv4
		||	AddressType == ENetAddressType_TCPv6)
	{
		int Family = (AddressType == ENetAddressType_TCPv4) ? PF_INET : PF_INET6;

		hSock = socket(Family, SOCK_STREAM, 0);

		if (hSock == INVALID_SOCKET)
		{	
			uint32 Error = WSAGetLastError();
			f_CheckDestroy();
			DMibErrorNet((CStr::CFormat("Could not create a socket for connection, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
		}

		auto Cleanup = g_OnScopeExit > [&]
			{
				f_CheckDestroy();
			}
		;

		auto SocketCleanup = g_OnScopeExit > [&]
			{
				closesocket(hSock);
			}
		;

		int Buf = EDefaultSocketBufSize;
		if (Buf > 0)
		{
			if (setsockopt(hSock, SOL_SOCKET, SO_RCVBUF, (char *)&Buf, sizeof(Buf)))
			{
				uint32 Error = WSAGetLastError();
				DMibErrorNet((CStr::CFormat("Could not set connect socket receive buffer size, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
			}

			if (setsockopt(hSock, SOL_SOCKET, SO_SNDBUF, (char *)&Buf, sizeof(Buf)))
			{
				uint32 Error = WSAGetLastError();
				DMibErrorNet((CStr::CFormat("Could not set connect socket send buffer size, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
			}
		}

		BOOL NoDelay = true;
		if (setsockopt(hSock, IPPROTO_TCP, TCP_NODELAY, (char *)&NoDelay, sizeof(NoDelay)))
		{
			uint32 Error = WSAGetLastError();
			DMibErrorNet((CStr::CFormat("Could not set connect socket NoDelay setting, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
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

		TCUniquePointer<CWindowsSocket> pSocket;

		if (_bAsyncConnect)
		{
			pSocket = fg_Construct();

			pSocket->m_OnStateChange = fg_Move(_OnStateChange);
			pSocket->m_pSocket = (void *)hSock;
			SocketCleanup.f_Clear();
			{
				DMibLockTyped(NMib::NThread::CMutual, mp_Lock);
				mp_SocketTree.f_Insert(pSocket.f_Get());
			}

			f_StartThread();

			if (WSAAsyncSelect(hSock, mp_hReportWnd, WM_USER, FD_READ | FD_WRITE | FD_CLOSE | FD_CONNECT))
			{
				uint32 Error = WSAGetLastError();
				{
					DMibLockTyped(NMib::NThread::CMutual, mp_Lock);
					mp_SocketTree.f_Remove(pSocket.f_Get());
				}
				pSocket = nullptr;
				DMibErrorNet((CStr::CFormat("Could not set socket async mode, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
			}
		}

		int Result = connect(hSock, (sockaddr const*)Address.f_Get(), Address.f_GetSockAddrLen());
		if (Result != 0)
		{		
			uint32 Error = WSAGetLastError();

			if (_bAsyncConnect)
			{
				if (Error != WSAEWOULDBLOCK)
				{
					{
						DMibLockTyped(NMib::NThread::CMutual, mp_Lock);
						mp_SocketTree.f_Remove(pSocket.f_Get());
					}
					pSocket = nullptr;
					DMibErrorNet((CStr::CFormat("Could not connect socket, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());				
				}
			}
			else
			{
				DMibErrorNet((CStr::CFormat("Could not connect socket, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
			}
		}
		else
		{
			if (_bAsyncConnect)
			{
				pSocket->m_StateAtomic |= NMib::NNet::ENetTCPState_Connection;

				if (pSocket->m_OnStateChange)
					pSocket->m_OnStateChange(NMib::NNet::ENetTCPState_Connection);
			}
		}

		if (!_bAsyncConnect)
		{
			pSocket = fg_Construct();

			pSocket->m_OnStateChange = fg_Move(_OnStateChange);
			pSocket->m_pSocket = (void *)hSock;
			SocketCleanup.f_Clear();
			{
				DMibLockTyped(NMib::NThread::CMutual, mp_Lock);
				mp_SocketTree.f_Insert(pSocket.f_Get());
			}

			f_StartThread();

			if (WSAAsyncSelect(hSock, mp_hReportWnd, WM_USER, FD_READ | FD_WRITE | FD_CLOSE))
			{
				uint32 Error = WSAGetLastError();
				{
					DMibLockTyped(NMib::NThread::CMutual, mp_Lock);
					mp_SocketTree.f_Remove(pSocket.f_Get());
				}
				pSocket = nullptr;
				DMibErrorNet((CStr::CFormat("Could not set socket async mode, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
			}

		}
		Cleanup.f_Clear();

		return pSocket.f_Detach();

	}
	else
	{
		DMibErrorNet("Invalid address type.");
//		return nullptr;
	}
}

CWindowsSocket *CWindowsSocketContext::f_Connect(CWindowsAddress const& _Address, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange, CWindowsAddress const *_pBindAddress)
{
	return fp_Connect(_Address, fg_Move(_OnStateChange), false, _pBindAddress);
}

CWindowsSocket *CWindowsSocketContext::f_AsyncConnect(CWindowsAddress const& _Address, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange, CWindowsAddress const *_pBindAddress)
{
	return fp_Connect(_Address, fg_Move(_OnStateChange), true, _pBindAddress);
}

TCUniquePointer<CWindowsSocket::CUnixListenState> CWindowsSocketContext::fp_PrepareUnixListen(CWindowsAddress &o_Address)
{
	if (o_Address.f_GetType() == ENetAddressType_Unix) 
	{
		CUnixAddress const &UnixAddress = o_Address.f_GetUnix();
		
		NStr::CStr UnixFilePath = UnixAddress.m_FilePath;
		if (NFile::CFile::fs_FileExists(UnixFilePath))
			NFile::CFile::fs_DeleteFile(UnixFilePath);
		auto Directory = NFile::CFile::fs_GetPath(UnixFilePath);
		if (!NFile::CFile::fs_FileExists(Directory))
			NFile::CFile::fs_CreateDirectory(Directory);

		TCUniquePointer<CWindowsSocket::CUnixListenState> pListenState = fg_Construct();

		pListenState->m_Address = UnixAddress;
		pListenState->m_UnixFileName = UnixFilePath;
		pListenState->m_UnixFile.f_Open(pListenState->m_UnixFileName, EFileOpen_Write | EFileOpen_NoLocalCache | EFileOpen_ShareRead);

		CNetAddressTCPv4 ListenAddress{{127, 0, 0, 1}, 0};

		NNet::CNetAddress NetAddress{ListenAddress};

		o_Address = *((CWindowsAddress *)NetAddress.f_AccessRaw());

		return pListenState;
	}

	return {};
}

CWindowsSocket *CWindowsSocketContext::f_Listen(CWindowsAddress const&_Address, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange, NNet::ENetFlag _Flags)
{
	CWindowsAddress Address = _Address;
	auto pUnixListen = fp_PrepareUnixListen(Address);

	ENetAddressType AddressType = Address.f_GetType();

	if (	AddressType != ENetAddressType_TCPv4
		&&	AddressType != ENetAddressType_TCPv6)
	{
		DMibErrorNet("Invalid address type for listening");
	}

	int Family = (AddressType == ENetAddressType_TCPv4) ? AF_INET : AF_INET6;

	SOCKET hSock = socket(Family, SOCK_STREAM, 0);

	if (hSock == INVALID_SOCKET )
	{
		DMibErrorNet("Could not create a socket for listening");
	}

	auto Cleanup = g_OnScopeExit > [&]
		{
			closesocket(hSock);
			f_CheckDestroy();
		}
	;

	if (_Flags & NNet::ENetFlag_ReusePort)
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

	Result = listen(hSock, SOMAXCONN);

	if (Result != 0)
	{
		uint32 Error = WSAGetLastError();
		DMibErrorNet((CStr::CFormat("Could not listen on socket, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
	}

	TCUniquePointer<CWindowsSocket> pSocket = fg_Construct();

	pSocket->m_OnStateChange = fg_Move(_OnStateChange);
	pSocket->m_pSocket = (void *)hSock;
	pSocket->m_pUnixListen = fg_Move(pUnixListen);

	if (pSocket->m_pUnixListen)
	{
		uint16 ListenPort = f_GetListenPort(pSocket.f_Get());
		auto &UnixListen = *pSocket->m_pUnixListen;
		UnixListen.m_UnixFile << ListenPort;
	}

	Cleanup.f_Clear();
	{
		DMibLockTyped(NMib::NThread::CMutual, mp_Lock);
		mp_SocketTree.f_Insert(pSocket.f_Get());
	}

	f_StartThread();

	if (WSAAsyncSelect(hSock, mp_hReportWnd, WM_USER, FD_ACCEPT | FD_CLOSE))
	{
		uint32 Error = WSAGetLastError();
		{
			DMibLockTyped(NMib::NThread::CMutual, mp_Lock);
			mp_SocketTree.f_Remove(pSocket.f_Get());
		}
		pSocket = nullptr;
		f_CheckDestroy();
		DMibErrorNet((CStr::CFormat("Could not set socket async mode, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
	}

	return pSocket.f_Detach();
}

CWindowsSocket *CWindowsSocketContext::f_ListenDatagram(CWindowsAddress const&_Address, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange, NNet::ENetFlag _Flags)
{
	CWindowsAddress Address = _Address;
	auto pUnixListen = fp_PrepareUnixListen(Address);

	ENetAddressType AddressType = Address.f_GetType();

	if (	AddressType != ENetAddressType_TCPv4
		&&	AddressType != ENetAddressType_TCPv6)
	{
		DMibErrorNet("Invalid address type for listening");
	}

	int Family = (AddressType == ENetAddressType_TCPv4) ? AF_INET : AF_INET6;

	SOCKET hSock = socket(Family, SOCK_DGRAM, 0);

	if (hSock == INVALID_SOCKET )
	{
		DMibErrorNet("Could not create a socket for listening");
	}

	auto Cleanup = g_OnScopeExit > [&]
		{
			closesocket(hSock);
			f_CheckDestroy();
		}
	;

	if (_Flags & NNet::ENetFlag_ReusePort)
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

	TCUniquePointer<CWindowsSocket> pSocket = fg_Construct();

	pSocket->m_OnStateChange = fg_Move(_OnStateChange);
	pSocket->m_pSocket = (void *)hSock;

	pSocket->m_pUnixListen = fg_Move(pUnixListen);

	if (pSocket->m_pUnixListen)
	{
		uint16 ListenPort = f_GetListenPort(pSocket.f_Get());
		auto &UnixListen = *pSocket->m_pUnixListen;
		UnixListen.m_UnixFile << ListenPort;
	}

	Cleanup.f_Clear();
	{
		DMibLockTyped(NMib::NThread::CMutual, mp_Lock);
		mp_SocketTree.f_Insert(pSocket.f_Get());
		pSocket->m_BindAddressSize = Address.f_GetSockAddrLen();
		pSocket->m_BindAddressType = AddressType;
	}

	f_StartThread();

	if (WSAAsyncSelect(hSock, mp_hReportWnd, WM_USER, FD_READ | FD_WRITE | FD_CLOSE))
	{
		uint32 Error = WSAGetLastError();
		{
			DMibLockTyped(NMib::NThread::CMutual, mp_Lock);
			mp_SocketTree.f_Remove(pSocket.f_Get());
		}
		pSocket = nullptr;
		f_CheckDestroy();
		DMibErrorNet((CStr::CFormat("Could not set socket async mode, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
	}

	return pSocket.f_Detach();
}

CWindowsSocket *CWindowsSocketContext::f_Accept(CWindowsSocket *_pSocket, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange)
{			
	f_CheckFailed();

	SOCKET hSock = accept((SOCKET)_pSocket->m_pSocket, nullptr, 0);
	if (hSock == INVALID_SOCKET)
	{
		int LastError = WSAGetLastError();
		if (LastError == WSAEWOULDBLOCK)
			return nullptr;

		DMibErrorNet((CStr::CFormat("Could not accpept socket, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(LastError)).f_GetStr());
	}

	int Buf = EDefaultSocketBufSize;
	if (Buf > 0)
	{
		if (setsockopt(hSock, SOL_SOCKET, SO_RCVBUF, (char *)&Buf, sizeof(Buf)))
		{
			uint32 Error = WSAGetLastError();
			closesocket(hSock);
			DMibErrorNet((CStr::CFormat("Could not connect socket, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
		}

		if (setsockopt(hSock, SOL_SOCKET, SO_SNDBUF, (char *)&Buf, sizeof(Buf)))
		{
			uint32 Error = WSAGetLastError();
			closesocket(hSock);
			DMibErrorNet((CStr::CFormat("Could not connect socket, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
		}
	}

	BOOL NoDelay = true;

	if (setsockopt(hSock, IPPROTO_TCP, TCP_NODELAY, (char *)&NoDelay, sizeof(NoDelay)))
	{
		uint32 Error = WSAGetLastError();
		closesocket(hSock);
		DMibErrorNet((CStr::CFormat("Could not connect socket, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
	}

	TCUniquePointer<CWindowsSocket> pSocket = fg_Construct();

	pSocket->m_OnStateChange = fg_Move(_OnStateChange);
	pSocket->m_pSocket = (void *)hSock;
	{
		DMibLockTyped(NMib::NThread::CMutual, mp_Lock);
		mp_SocketTree.f_Insert(pSocket.f_Get());
	}

	f_StartThread();

	if (WSAAsyncSelect(hSock, mp_hReportWnd, WM_USER, FD_READ | FD_WRITE | FD_CLOSE))
	{
		uint32 Error = WSAGetLastError();
		{
			DMibLockTyped(NMib::NThread::CMutual, mp_Lock);
			mp_SocketTree.f_Remove(pSocket.f_Get());
		}
		pSocket = nullptr;
		f_CheckDestroy();
		DMibErrorNet((CStr::CFormat("Could not set socket async mode, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
	}
	return pSocket.f_Detach();
}

bint CWindowsSocketContext::f_Shutdown(CWindowsSocket *_pSocket)
{
	int Ret = shutdown((SOCKET)_pSocket->m_pSocket, SD_SEND);

	if (Ret == SOCKET_ERROR)
	{
		int Error = WSAGetLastError();
		DMibErrorNet((CStr::CFormat("Could not shutdown socket, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
	}
	return true;
}

bint CWindowsSocketContext::f_Close(CWindowsSocket *_pSocket)
{
	{
		DMibLockTyped(NMib::NThread::CMutual, mp_Lock);
		if (_pSocket->m_TreeLink.f_IsInTree())
			mp_SocketTree.f_Remove(_pSocket);
	}

	// Make sure that the report thread isn't using our socket
	{
		DMibLockTyped(NMib::NThread::CMutual, _pSocket->m_Lock);
	}

	delete _pSocket;

	f_CheckDestroy();

	return true;
}

mint CWindowsSocketContext::f_Receive(CWindowsSocket *_pSocket, void *_pData, mint _DataLen)
{
	int Ret = recv((SOCKET)_pSocket->m_pSocket, (char *)_pData, _DataLen, 0);

	if (Ret == SOCKET_ERROR)
	{
		int Error = WSAGetLastError();
		if (Error != WSAEWOULDBLOCK)
		{
			DMibErrorNet((CStr::CFormat("Could not revc from socket, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
		}
		else
			return 0;
	}

	return Ret;
}

mint CWindowsSocketContext::f_Send(CWindowsSocket *_pSocket, const void *_pData, mint _DataLen)
{
#ifdef DTCPDelayEmulation
	if (bDTCPDelayEmulation)
	{
		DMibLock(_pSocket->m_DelayedLock);
		if (_pSocket->m_DelayedData >= DTCPDelayEmulation_MaxQueue)
		{
			_pSocket->m_bDelayedStuffed = true;
			return 0;
		}

		CWindowsSocket::CDelayedPacket *pNewPacket = DMibNew CWindowsSocket::CDelayedPacket;
		pNewPacket->m_Data.f_Insert((uint8 *)_pData, _DataLen);
		NTime::CTime Now = NTime::CTime::fs_NowUTC();
		NTime::CTime SendTime = Now + NTime::CTimeSpanConvert::fs_CreateSpanFromSeconds(DTCPDelayEmulation_MinDelay);
		NTime::CTime SendTimeRateLimit;
		SendTimeRateLimit = Now + NTime::CTimeSpanConvert::fs_CreateSpanFromSeconds(fp64(_pSocket->m_DelayedData)/fp64(DTCPDelayEmulation_Rate));
		if (SendTimeRateLimit > SendTime)
			pNewPacket->m_SendTime = SendTimeRateLimit;
		else
			pNewPacket->m_SendTime = SendTime;
		_pSocket->m_DelayedPackets.f_Insert(pNewPacket);
		_pSocket->m_DelayedData += _DataLen;
		return _DataLen;
	}
#endif

	int Ret = send((SOCKET)_pSocket->m_pSocket, (const char *)_pData, _DataLen, 0);

	if (Ret == SOCKET_ERROR)
	{
		int Error = WSAGetLastError();
		if (Error != WSAEWOULDBLOCK)
		{
			DMibErrorNet((CStr::CFormat("Could not sendfrom socket, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
		}
		else
			return 0;
	}

	return Ret;
}

mint CWindowsSocketContext::f_SendDatagram(CWindowsSocket *_pSocket, CWindowsAddress const&_Address, const void *_pData, mint _DataLen)
{
	int Ret = sendto((SOCKET)_pSocket->m_pSocket, (const char *)_pData, _DataLen, 0, (sockaddr const*)_Address.f_Get(), _Address.f_GetSockAddrLen());

	if (Ret == SOCKET_ERROR)
	{
		int Error = WSAGetLastError();
		if (Error != WSAEWOULDBLOCK)
		{
			DMibErrorNet((CStr::CFormat("Could not sendfrom socket, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
		}
		else
			return 0;
	}

	return Ret;
}

mint CWindowsSocketContext::f_ReceiveDatagram(CWindowsSocket *_pSocket, CWindowsAddress &_Address, void *_pData, mint _DataLen)
{
	socklen_t Len = _pSocket->m_BindAddressSize;
	int Ret = recvfrom((SOCKET)_pSocket->m_pSocket, (char *)_pData, _DataLen, 0, (sockaddr *)_Address.f_GetForWrite(_pSocket->m_BindAddressType, Len), &Len);

	if (Ret == SOCKET_ERROR)
	{
		int Error = WSAGetLastError();
		if (Error != WSAEWOULDBLOCK)
		{
			DMibErrorNet((CStr::CFormat("Could not sendfrom socket, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
		}
		else
			return 0;
	}

	return Ret;
}

// *************************************************************************************************************************
// WindowsSocketContext Socket Properties & State Methods
// *************************************************************************************************************************

void CWindowsSocketContext::f_SetOnStateChange(CWindowsSocket *_pSocket, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange)
{
	{
		DMibLockTyped(NMib::NThread::CMutual, _pSocket->m_Lock);
		_pSocket->m_OnStateChange = fg_Move(_OnStateChange);

		// Signal once so the new report to gets to update
		if (_pSocket->m_OnStateChange)
			_pSocket->m_OnStateChange(::NMib::NNet::ENetTCPState_None);
	}
}

NMib::NNet::ENetTCPState CWindowsSocketContext::f_GetState(CWindowsSocket *_pSocket)
{
	uint32 OldState = _pSocket->m_StateAtomic.f_FetchAnd(~((uint32)DMibBitRange(0, 30)));
	uint32 State = OldState & DMibBitRange(0, 30);
	return (NMib::NNet::ENetTCPState)State;
}

NStr::CStr CWindowsSocketContext::f_GetCloseReason(CWindowsSocket *_pSocket)
{
	NStr::CStr Ret;
	{
		DMibLockTyped(NThread::CMutual, _pSocket->m_Lock);
		Ret = _pSocket->m_CloseReason;
	}
	return Ret;
}

CWindowsSocket* CWindowsSocketContext::f_InheritHandle2(void *_pSocket, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange)
{
	DMibRequire(!!_pSocket);
	CWindowsSocket *pReturn = DMibNew CWindowsSocket;

	pReturn->m_OnStateChange = fg_Move(_OnStateChange);
	pReturn->m_pSocket = (void *)_pSocket;
	pReturn->m_StateAtomic |= NMib::NNet::ENetTCPState_Read | NMib::NNet::ENetTCPState_Write;

	f_StartThread();

	if (WSAAsyncSelect((SOCKET)pReturn->m_pSocket, mp_hReportWnd, WM_USER, FD_READ | FD_WRITE | FD_CLOSE))
	{
		uint32 Error = WSAGetLastError();
		delete pReturn;
		f_CheckDestroy();
		DMibErrorNet((CStr::CFormat("Could not set socket async mode, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
	}

	{
		DMibLockTyped(NMib::NThread::CMutual, mp_Lock);
		CWindowsSocket *pSocket = mp_SocketTree.f_FindEqual(_pSocket);
		if(pSocket) 
		{
			DMibCheck(pSocket->m_pSocket == _pSocket);
			mp_SocketTree.f_Remove(pSocket);
		}
		mp_SocketTree.f_Insert(pReturn);
	}
	if (pReturn->m_OnStateChange)
		pReturn->m_OnStateChange(::NMib::NNet::ENetTCPState_None);

	return pReturn;
}

struct CWindowsSocket_OldVersion
{
	struct CLock
	{
		mint m_nLocked;
		mint m_ThreadID;
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

void *CWindowsSocketContext::f_GiveUpForInherit(CWindowsSocket *_pSocket)
{
	DMibRequire(!!_pSocket);
	if (_pSocket->m_Magic == 0x4EA11E49)
	{
		// Versioned
		if (_pSocket->m_Version != 0x101)
			DMibErrorNet(fg_Format("Unsupported socket version: {nfh}", _pSocket->m_Version));
		_pSocket->m_StateAtomic |= DMibBit(31);
		return _pSocket->m_pSocket;
	}

	CWindowsSocket_OldVersion* pSocket = (CWindowsSocket_OldVersion*)_pSocket;
	auto *pSocketHandle = pSocket->m_pSocket;
	pSocket->m_State |= DMibBit(31); // Hopefully this is good enough. Old versions needed this to be locked
	return pSocketHandle;
}

void *CWindowsSocketContext::f_GetOSSocket(CWindowsSocket *_pSocket)
{
	DMibRequire(!!_pSocket);
	return _pSocket->m_pSocket;
}

CWindowsAddress* CWindowsSocketContext::f_GetPeerAddress(CWindowsSocket *_pSocket)
{
	sockaddr_storage PeerAddr;

	socklen_t nAddrBytes = sizeof(PeerAddr);

	int Ret = getpeername( (SOCKET)_pSocket->m_pSocket, (struct sockaddr *)&PeerAddr, &nAddrBytes);

	if (Ret != 0)
	{
		uint32 Error = WSAGetLastError();
		DMibErrorNet((CStr::CFormat("Could not get peer address, windows returned: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
	}

	if (PeerAddr.ss_family == AF_INET)
	{
		NPtr::TCUniquePointer<CWindowsAddress> pAddress = fg_Construct(*(sockaddr_in const*)&PeerAddr);
		return pAddress.f_Detach();
	}
	else if (PeerAddr.ss_family == AF_INET6)
	{
		NPtr::TCUniquePointer<CWindowsAddress> pAddress = fg_Construct(*(sockaddr_in6 const*)&PeerAddr);
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

	int Ret = getsockname((SOCKET)_pSocket->m_pSocket, (struct sockaddr *)&PeerAddr, &nAddrBytes);

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

mint NSys::NNet::fg_GetMaxUnixSocketNameLength()
{
	return CUnixAddress::mc_MaxLength - 1;
}

#include "Malterlib_Core_PlatformImp_Net.imp.h"
