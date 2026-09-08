// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>

#include "../Malterlib_Core_IoSubSystem.h"

#include <afunix.h>

#include "Malterlib_Core_PlatformImp_Net.h"
#include "Malterlib_Core_Platform_Windows_IoLoop.h"

using CWindowsAddress = CRuntimeNetAddress;

enum EWindowsSocketEvent
{
	EWindowsSocketEvent_Read	= 1 << 0,
	EWindowsSocketEvent_Write	= 1 << 1,
};

enum EWindowsSocketMode
{
	EWindowsSocketMode_Connecting,
	EWindowsSocketMode_Connect,
	EWindowsSocketMode_Listen,
	EWindowsSocketMode_Datagram,
};

bool fg_WindowsTcpInfoSupported();

struct CWindowsSocket
{
	struct CUnixListenState
	{
		~CUnixListenState();

		CUnixAddress m_Address;
		CStr m_UnixFileName;
		TCBinaryStreamFile<> m_UnixFile;
	};

	uint32 m_Magic = 0x4EA11E49; // Cross-module ABI: preserve the first three member offsets for older socket layouts.
	uint32 m_Version = 0x102;

	SOCKET m_Socket;
	EWindowsSocketMode m_Mode;
	EWindowsSocketEvent m_RegisteredEvents;
	umint m_BindAddressSize = 0;
	ENetAddressType m_AddressType = ENetAddressType_None;
	TCUniquePointer<CUnixListenState> m_pUnixListen;
	CStr m_UnixListenPath; // Cleared after unlink; bind-time identity protects successor socket files.
	NFile::CUniqueFileIdentifier m_UnixListenFileIdentity;

	NMib::NSys::CIoSubSystem *m_pIo = nullptr;

	NMib::NThread::CMutual m_Lock; // Protects state shared by dispatch and the consumer.
	bool m_bInitialWriteNotification;
	NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> m_fOnStateChange;
	TCAtomic<uint32> m_StateAtomic;
	int m_CloseError;
	bool m_bShutdownCalled = false;
	bool m_bNonErrorClose = false;
	bool m_bRemoteCloseSignalled = false;

	bool m_bSendBufferDecided = false;
	umint m_nSendWindowBytes = 0; // Unreleased-byte limit; zero until explicitly configured.
	uint64 m_PathLastBytesOut = 0;
	uint64 m_PathLastStamp = 0;
	umint m_nSendBufferBytesToApply = umint(-1); // Decided before registration, applied on first completion send; all-ones preserves system policy.

	NMib::NSys::ICIoLoop *m_pOwningLoop = nullptr; // Fixed at socket start; deregistration must use the same loop.
	NMib::NSys::CIoLoopRegistration *m_pIoRegistration = nullptr; // Non-null while registered; the loop owns this opaque registration.
	bool m_bInheritable = false; // Set before start to avoid permanent completion binding; listeners pass it to accepted sockets.
	bool m_bFromInherit = false; // Handed over by another process, see CIoLoopRegisterOptions::m_bInheritedHandle

	CWindowsSocket
		(
			SOCKET _Socket
			, EWindowsSocketMode _Mode
			, EWindowsSocketEvent _Events
			, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
		)
		: m_Socket(_Socket)
		, m_Mode(_Mode)
		, m_RegisteredEvents(_Events)
		, m_bInitialWriteNotification(true)
		, m_fOnStateChange(fg_Move(_fOnStateChange))
		, m_StateAtomic(NMib::NNetwork::ENetTCPState_Write)
		, m_CloseError(0)
	{
	}
};

#if DMibConfig_IoDebug_Enable
NSys::CSocketIoStats *fg_SocketIoStats();
#endif

struct CIoSubSystem_Windows;

class CWindowsSocketContext
{
protected:
	bool mp_bInitFailed;
	bool mp_bWsaStarted = false;

	// Dedicated shared-loop thread; create the loop before starting and destroy it after stopping.
	struct CPollerThread : public NMib::NThread::CThread
	{
		NStr::CStr f_GetThreadName() override
		{
			return CStr("Socket Poller");
		}

		aint f_Main() override
		{
			mp_pLoop->f_SetOwnerThreadToCurrent();

			while (mp_bStop.f_Load() == 0 && f_GetState() != NMib::NThread::EThreadState_EventWantQuit)
				mp_pLoop->f_WaitAndDispatch();

			mp_pLoop->f_DrainForShutdown();

			return 0;
		}

		umint f_Stop(bool _bBlock) override
		{
			mp_bStop.f_Store(1);
			mp_pLoop->f_Wake();
			return NMib::NThread::CThread::f_Stop(_bBlock);
		}

		NMib::NSys::ICIoLoop *mp_pLoop = nullptr;
		NMib::NAtomic::TCAtomic<smint> mp_bStop{0};
	};

	CIoSubSystem_Windows *mp_pIo = nullptr;
	CPollerThread mp_PollerThread;

	CAddressResolver mp_Resolver;

	void fp_ToNative(NMib::NNetwork::CNetAddressTCPv4 const& _InAddr, sockaddr_in& _OutAddr) const
	{
		fg_MemClear(_OutAddr);
		_OutAddr.sin_family = AF_INET;

		_OutAddr.sin_addr.s_addr = _InAddr.m_IP[3] << 24 | _InAddr.m_IP[2] << 16 | _InAddr.m_IP[1] << 8 |	_InAddr.m_IP[0];

		_OutAddr.sin_port = htons(_InAddr.m_Port);
	}

	void fp_ToNative(NMib::NNetwork::CNetAddressTCPv6 const& _InAddr, sockaddr_in6& _OutAddr) const
	{
		fg_MemClear(_OutAddr);
		_OutAddr.sin6_family = AF_INET6;

		fg_MemCopy(&_OutAddr.sin6_addr.s6_addr, &_InAddr.m_IP, sizeof(uint8) * 16);

		_OutAddr.sin6_port = htons(_InAddr.m_Port);
	}

	void fp_FromNative(sockaddr_in const& _InAddr, NMib::NNetwork::CNetAddressTCPv4 & _OutAddr) const
	{
		uint32 IP = _InAddr.sin_addr.s_addr;
		_OutAddr.m_IP[0] = 0xFF & (IP);
		_OutAddr.m_IP[1] = 0xFF & (IP >> 8);
		_OutAddr.m_IP[2] = 0xFF & (IP >> 16);
		_OutAddr.m_IP[3] = 0xFF & (IP >> 24);
		_OutAddr.m_Port = ntohs(_InAddr.sin_port);
	}

	void fp_FromNative(sockaddr_in6 const& _InAddr, NMib::NNetwork::CNetAddressTCPv6 & _OutAddr) const
	{
		fg_MemCopy(&_OutAddr.m_IP, &_InAddr.sin6_addr.s6_addr, sizeof(uint8) * 16);
		_OutAddr.m_Port = ntohs(_InAddr.sin6_port);
	}

	CWindowsSocket *fp_Connect
		(
			CWindowsAddress const &_Address
			, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
			, CWindowsAddress const *_pBindAddress
		)
	;

	CWindowsSocket *fp_CreateSocket
		(
			SOCKET _Socket
			, EWindowsSocketMode _Mode
			, EWindowsSocketEvent _Events
			, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
			, bool _bFromInherit = false
		)
	;
	void fp_DestroySocket(CWindowsSocket *_pSocket);
	void fp_RemoveUnixListenFile(CWindowsSocket *_pSocket);

	TCUniquePointer<CWindowsSocket::CUnixListenState> fp_PrepareUnixListen(CWindowsAddress &o_Address);

public:
	CWindowsSocketContext();
	~CWindowsSocketContext();

	void f_CheckFailed();

	// Address
		CWindowsAddress* f_CreateAddress(NMib::NNetwork::ENetAddressType _Type, void const* _pData, umint _nDataBytes);
		CWindowsAddress* f_DuplicateAddress(CWindowsAddress* _Address);

		NMib::NNetwork::ENetAddressType f_GetAddressType(CWindowsAddress const& _pAddress);
		bool f_GetAddressRaw(CWindowsAddress const &_Address, NMib::NNetwork::ENetAddressType _ExpectedType, void* _opRawData, umint _nDataBytes);
		CWindowsAddress* f_SetAddressRaw(CWindowsAddress* _pAddress, ::NMib::NNetwork::ENetAddressType _Type, void const* _opRawData, umint _nDataBytes);

		CWindowsAddress* f_ResolveAddress(const NMib::NStr::CStr &_Address, NMib::NNetwork::ENetAddressType _PreferType = NMib::NNetwork::ENetAddressType_None);
		CWindowsAddress* f_ResolveAddress(const NMib::NStr::CStr &_Address, NMib::NNetwork::ENetAddressType _PreferType, bool _bThrowOnError);

		void *f_AsyncResolveAddress_Open(const NMib::NStr::CStr &_Address, ::NMib::NNetwork::ENetAddressType _PreferType, NMib::NFunction::TCFunctionMutable<void ()> &&_fOnFinish);
		bool f_AsyncResolveAddress_GetResult(void *_pResolver, CWindowsAddress*& _opAddress, NMib::NStr::CStr &_Error);
		void f_AsyncResolveAddress_Close(void *_pResolver);

		int f_CompareAddresses(CWindowsAddress const& _pFirst, CWindowsAddress const& _pSecond);

		void f_FreeAddress(CWindowsAddress* _pAddress); // It is OK to free a nullptr address.

		NMib::NStr::CStr f_GetAddressString(CWindowsAddress const &_Address, ENetAddressStringFlag _Flags);

	// Connection Operations
		CWindowsSocket *f_AsyncConnect
			(
				CWindowsAddress const &_Address
				, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
				, CWindowsAddress const *_pBindAddress
			)
		;

		void f_StartSocket(CWindowsSocket *_pSocket);

		CWindowsSocket *f_Listen
			(
				CWindowsAddress const &_Address
				, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
				, NMib::NNetwork::ENetFlag _Flags
			)
		;
		CWindowsSocket *f_ListenDatagram
			(
				CWindowsAddress const &_Address
				, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
				, NMib::NNetwork::ENetFlag _Flags
			)
		;
		CWindowsSocket *f_Accept(CWindowsSocket *_pSocket, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange);

		bool f_Close(CWindowsSocket* _pSocket);
		void f_CloseAsync(CWindowsSocket *_pSocket, NMib::NFunction::TCFunctionMovable<void ()> &&_fOnClosed);
		bool f_Shutdown(CWindowsSocket *_pSocket);

		umint f_Receive(CWindowsSocket *_pSocket, void *_pData, umint _DataLen, bool &o_bEndOfStream);
		umint f_Send(CWindowsSocket *_pSocket, const void *_pData, umint _DataLen);
		umint f_SendVectored(CWindowsSocket *_pSocket, NMib::NSys::CIoSpan const *_pSpans, umint _nSpans);
		bool f_GetProcessIdentity(CWindowsSocket *_pSocket, NMib::NSys::NNetwork::CProcessIdentity &o_LocalIdentity, NMib::NSys::NNetwork::CProcessIdentity &o_PeerIdentity);
		umint f_SendDatagram(CWindowsSocket *_pSocket, CWindowsAddress const &_Address, const void *_pData, umint _DataLen);
		umint f_ReceiveDatagram(CWindowsSocket *_pSocket, CWindowsAddress &_Address, void *_pData, umint _DataLen);

	// Socket Properties & State

		void f_SetOnStateChange(CWindowsSocket* _pSocket, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange);

		NMib::NNetwork::ENetTCPState f_GetState(CWindowsSocket *_pSocket);

		NMib::NStr::CStr f_GetCloseReason(CWindowsSocket* _pSocket);

		CWindowsSocket* f_InheritHandle2(void *_pOSSocket, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange);
		void *f_GiveUpForInherit(CWindowsSocket *_pSocket);
		void f_GiveUpForInheritAsync(CWindowsSocket *_pSocket, NMib::NFunction::TCFunctionMovable<void (void *_pSocketHandle)> &&_fOnHandle);
		void f_CloseSocketHandle(void *_pSocketHandle);
		void *f_GetOSSocket(CWindowsSocket *_pSocket);

		CWindowsAddress* f_GetPeerAddress(CWindowsSocket *_pSocket);
		uint32 f_GetListenPort(CWindowsSocket *_pSocket);
};
