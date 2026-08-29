// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>

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

struct CWindowsSocket
{
	struct CUnixListenState
	{
		~CUnixListenState();

		CUnixAddress m_Address;
		CStr m_UnixFileName;
		TCBinaryStreamFile<> m_UnixFile;
	};

	// Cross-module identification: another module handing a socket over may have been built
	// against an older layout, so the first three members stay where every version had them
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

	// Changed by the loop's dispatch and the consumer alike, under the lock
	NMib::NThread::CMutual m_Lock;
	bool m_bInitialWriteNotification;
	NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> m_fOnStateChange;
	TCAtomic<uint32> m_StateAtomic;
	int m_CloseError;
	bool m_bShutdownCalled = false;
	bool m_bNonErrorClose = false;
	bool m_bRemoteCloseSignalled = false;

	// Whether the socket's completion port is provably this loop's. A handle keeps the port of
	// its first association for its lifetime, so a handle adopted from a give-up is only capable
	// of completion transfers on the loop that first registered it — or after the system let the
	// loop rebind it. Readiness works regardless, since polls are issued on the loop's own AFD
	// handle
	bool m_bInheritedFromOwningLoop = false;
	bool m_bSendBufferOverrideApplied = false;

	// The loop this socket was registered with, which is where it has to be deregistered again.
	// Set once when the socket is started and never changed, so a socket never migrates between
	// loops and no handshake is needed to move it
	NMib::NSys::ICIoLoop *m_pOwningLoop = nullptr;

	// The socket's registration with its owning loop, non-null exactly while registered. The
	// handle is the loop's identity for the socket; the loop never dereferences the socket itself
	NMib::NSys::CIoLoopRegistration *m_pIoRegistration = nullptr;

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

// Cumulative readiness-path socket statistics, reported at process exit when MalterlibIoStats=1:
// the readiness counterpart of the IOCP completion counters, so the two transfer paths can be
// compared on the same terms — how many transfer calls it took, how big they were, how often
// they met an empty queue or a full buffer, and how many readiness arms and reports drove them.
// Relaxed atomics: every loop and every calling thread writes them, exactness per counter is not
// the point. Everything here and every recording site exists only in builds carrying the io
// debugging overrides
#if DMibConfig_IoDebug_Enable
struct CSocketIoStats
{
	NAtomic::TCAtomic<uint64> m_nRecvCalls = 0;
	NAtomic::TCAtomic<uint64> m_nRecvBytes = 0;
	NAtomic::TCAtomic<uint64> m_nRecvWouldBlock = 0;
	NAtomic::TCAtomic<uint64> m_nRecvShort = 0;
	NAtomic::TCAtomic<uint64> m_nRecvEndOfStream = 0;
	NAtomic::TCAtomic<uint64> m_RecvSizeBuckets[33] = {};
	NAtomic::TCAtomic<uint64> m_nSendCalls = 0;
	NAtomic::TCAtomic<uint64> m_nSendBytesRequested = 0;
	NAtomic::TCAtomic<uint64> m_nSendBytesSent = 0;
	NAtomic::TCAtomic<uint64> m_nSendWouldBlock = 0;
	NAtomic::TCAtomic<uint64> m_nSendShort = 0;
	NAtomic::TCAtomic<uint64> m_SendSizeBuckets[33] = {};
	NAtomic::TCAtomic<uint64> m_nReadinessArmsRead = 0;
	NAtomic::TCAtomic<uint64> m_nReadinessArmsWrite = 0;
	NAtomic::TCAtomic<uint64> m_nReadinessReportsRead = 0;
	NAtomic::TCAtomic<uint64> m_nReadinessReportsWrite = 0;
};

extern CSocketIoStats g_SocketIoStats;

// Registers the exit report the first time it answers true. The socket context asks at startup,
// so a run whose transfers never reached a counted site still reports its zeros
bool fg_SocketIoStatsEnabled();
#endif

class CWindowsSocketContext
{
protected:
	bool mp_bInitFailed;

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

		NMib::NSys::ICIoLoop *f_GetPollerLoop() const
		{
			return mp_PollerThread.mp_pLoop;
		}
};
