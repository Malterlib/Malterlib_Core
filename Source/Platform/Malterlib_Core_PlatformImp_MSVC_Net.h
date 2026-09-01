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

// Whether the kernel answers SIO_TCP_INFO (Windows 10 1703, build 15063), which is what sizes a
// zero copy send pipeline to its path; without it TCP sends keep their socket buffer
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

	// Cross-module identification: the old Malterlib project hands its sockets over to this
	// module, built against an older layout, so the first three members stay where every version
	// had them. Newer modules give up their sockets within themselves
	uint32 m_Magic = 0x4EA11E49;
	uint32 m_Version = 0x102;

	// This info is set once and never changed. The socket becomes invalid when it is given up
	// for another owner to inherit
	SOCKET m_Socket;
	EWindowsSocketMode m_Mode;
	EWindowsSocketEvent m_RegisteredEvents;
	umint m_BindAddressSize = 0;
	ENetAddressType m_AddressType = ENetAddressType_None;
	TCUniquePointer<CUnixListenState> m_pUnixListen;

	// The io subsystem, cached at creation so the socket paths need no getter
	NMib::NSys::CIoSubSystem *m_pIo = nullptr;

	// Changed by the loop's dispatch and the consumer alike, under the lock
	NMib::NThread::CMutual m_Lock;
	bool m_bInitialWriteNotification;
	NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> m_fOnStateChange;
	TCAtomic<uint32> m_StateAtomic;
	int m_CloseError;
	bool m_bShutdownCalled = false;
	bool m_bNonErrorClose = false;
	bool m_bRemoteCloseSignalled = false;

	bool m_bSendBufferDecided = false;

	// The bytes the connection may have in flight on its sends, 0 until set; what a configured
	// window sizes the kernel buffers to, and what bounds the unacknowledged bytes of sends that
	// go without a send buffer
	umint m_nSendWindowBytes = 0;

	// What the last path query saw, for the rate the next one derives
	uint64 m_PathLastBytesOut = 0;
	uint64 m_PathLastStamp = 0;

	// The send buffer the first completion send applies, decided at registration (see
	// f_StartSocket); ~umint(0) leaves the system default
	umint m_nSendBufferBytesToApply = umint(-1);

	// The loop this socket was registered with, which is where it has to be deregistered again.
	// Set once when the socket is started and never changed, so a socket never migrates between
	// loops and no handshake is needed to move it
	NMib::NSys::ICIoLoop *m_pOwningLoop = nullptr;

	// The socket's registration with its owning loop, non-null exactly while registered. The
	// handle is the loop's identity for the socket; the loop never dereferences the socket itself
	NMib::NSys::CIoLoopRegistration *m_pIoRegistration = nullptr;

	// Created to be given up to an owner that cannot rebind a handle — a backend without a
	// completion port, or a system without the native replace: registered readiness-only, so the
	// handle is never bound. Set before the socket starts; a listen socket passes it to what it
	// accepts. A loop of this kind takes over bound handles as well, so the flag is only for
	// those receivers
	bool m_bInheritable = false;

	CWindowsSocket(SOCKET _Socket, EWindowsSocketMode _Mode, EWindowsSocketEvent _Events, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange)
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
// Null when the statistics are off; the counters live on the io subsystem (m_SocketIoStats)
NSys::CSocketIoStats *fg_SocketIoStats();
#endif

struct CIoSubSystem_Windows;

class CWindowsSocketContext
{
protected:
	bool mp_bInitFailed;
	bool mp_bWsaStarted = false;

	// The process wide shared loop for sockets nobody claimed, hosted on a dedicated thread.
	// Created by the context before the thread starts and destroyed after it stops
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

	// The io subsystem the socket policies read their knobs from, cached at construction
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

	// Final teardown once no loop holds the socket: closes the handle, removes any unix socket
	// file and frees the object. Runs on the closing thread for poller-owned sockets and on the
	// loop's thread for the asynchronous path
	void fp_DestroySocket(CWindowsSocket *_pSocket);

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

		// Closes the socket and runs the continuation once the handle is closed and any unix
		// socket file removed: on the loop's thread for a socket on a created loop, inline
		// otherwise. The platform socket is consumed on the call
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
