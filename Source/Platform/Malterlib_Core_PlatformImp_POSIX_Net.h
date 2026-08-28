// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>

#include "Malterlib_Core_PlatformImp_Net.h"
#include "Malterlib_Core_Platform_POSIX_IoLoop.h"

static const uint32 ENetAddressType_Kernel = 3;

using namespace NMib;
using namespace NMib::NMemory;
using namespace NMib::NStr;
using namespace NMib::NNetwork;

enum EPOSIXSocketEvent
{
	EPOSIXSocketEvent_Read	= 1 << 0,
	EPOSIXSocketEvent_Write	= 1 << 1,
};

enum EPOSIXSocketMode
{
	EPOSIXSocketMode_Connecting,
	EPOSIXSocketMode_Connect,
	EPOSIXSocketMode_Listen,
	EPOSIXSocketMode_Datagram,
};

struct CPOSIXSocket
{
	// This info is set once and never changed.
	int m_FD;
	EPOSIXSocketMode m_Mode;
	EPOSIXSocketEvent m_RegisteredEvents;
	umint m_BindAddressSize = 0;
	ENetAddressType m_AddressType = ENetAddressType_None;
	NStr::CStr m_UnixFilePath;
	NStr::CStr m_PeerUnixFilePath;

#if defined(DPlatformFamily_Linux)
	// Kernel process identity handles pinned for the connection lifetime: a 32-bit kernel recycles
	// a process's pidfs inode once its last pidfd closes, so releasing these before the peer
	// completes its own identity checks could make one process present two different identities
	int m_LocalPidFD = -1;
	int m_PeerPidFD = -1;
#endif

	// This is stull that will be changed or use by the poller etc...
	NMib::NThread::CMutual m_Lock;
	bool m_bInitialWriteNotification;
	NMib::NFunction::TCFunctionMovable<void (NMib::NNetwork::ENetTCPState _StateAdded)> m_fOnStateChange;
	NAtomic::TCAtomic<uint32> m_StateAtomic;
	int m_CloseError;
	bool m_bShutdownCalled = false;
	bool m_bNonErrorClose = false;
	bool m_bRemoteCloseSignalled = false;

	// Whether the peer is on another machine, probed once from the first completion-support
	// query after the connection settles: local peers default to readiness transfers — measured
	// at best at parity on loopback and unix transports while costing more CPU — and the
	// completion machinery's wins only pay on a real link. 0 = not probed yet (a connecting
	// socket has no peer to ask), 1 = local, 2 = remote
	int8 m_CompletionPeerClass = 0;

	// The loop this socket was registered with, which is where it has to be deregistered again.
	// Set once when the socket is started and never changed, so a socket never migrates between
	// loops and no handshake is needed to move it
	NMib::NSys::ICIoLoop *m_pOwningLoop = nullptr;

	// The socket's registration with its owning loop, non-null exactly while registered. The
	// handle is the loop's identity for the socket; the loop never dereferences the socket itself
	NMib::NSys::CIoLoopRegistration *m_pIoRegistration = nullptr;

	// This is protected by the context m_Lock.
	NMib::NIntrusive::TCAVLLink<> m_FDToSocketLink;

	class CAVLCompare_CPOSIXSocket
	{
	public:
		inline_small int operator () (CPOSIXSocket const& _Sock) const
		{
			return _Sock.m_FD;
		}
	};

	CPOSIXSocket(int _FD, EPOSIXSocketMode _Mode, EPOSIXSocketEvent _Events, NMib::NFunction::TCFunctionMovable<void (NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange)
		: m_FD(_FD)
		, m_Mode(_Mode)
		, m_RegisteredEvents(_Events)
		, m_bInitialWriteNotification(true)
		, m_StateAtomic(NMib::NNetwork::ENetTCPState_Write)
		, m_CloseError(0)
		, m_fOnStateChange(fg_Move(_fOnStateChange))
	{}

	~CPOSIXSocket()
	{
//		DMibSafeCheck(m_FD == -1, "FD not closed upon socket destruction");
	}

};

using CPOSIXAddress = CRuntimeNetAddress;


class CPOSIXImpSpecificSocketContext
{
private:
	class CInternal;
	NMib::NStorage::TCUniquePointer<CInternal> mp_pInternal;

public:
	CPOSIXImpSpecificSocketContext();
	~CPOSIXImpSpecificSocketContext();

	bool f_CreateAddress(CPOSIXAddress& _oAddr, NMib::NNetwork::ENetAddressType _Type, void const* _pData, umint _nDataBytes);
	bool f_ResolveAddress(CPOSIXAddress& _oAddr, const NMib::NStr::CStr &_Address, NMib::NNetwork::ENetAddressType _PreferType = NMib::NNetwork::ENetAddressType_None);
	bool f_GetAddressRaw(CPOSIXAddress const &_Address, ENetAddressType _ExpectedType, void* _opRawData, umint _nDataBytes);
	CPOSIXAddress* f_SetAddressRaw(CPOSIXAddress* _Address, ::NMib::NNetwork::ENetAddressType _ExpectedType, void const* _pRawData, umint _nDataBytes);

	struct CSocketCreateParams
	{
		int m_Domain;
		int m_Type;
		int m_Protocol;
	};

	bool f_GetSocketCreateParams(::NMib::NNetwork::ENetAddressType _ExpectedType, CSocketCreateParams &_oParams);
};

class CPOSIXSocketContext
{

public:
	CPOSIXSocketContext();
	~CPOSIXSocketContext();

	// Address
		CPOSIXAddress* f_CreateAddress(NMib::NNetwork::ENetAddressType _Type, void const* _pData, umint _nDataBytes);

		CPOSIXAddress* f_DuplicateAddress(CPOSIXAddress const &_Address);

		NMib::NNetwork::ENetAddressType f_GetAddressType(CPOSIXAddress const& _pAddress);
		bool f_GetAddressRaw(CPOSIXAddress const &_Address, NMib::NNetwork::ENetAddressType _ExpectedType, void* _opRawData, umint _nDataBytes);
		CPOSIXAddress* f_SetAddressRaw(CPOSIXAddress* _Address, ::NMib::NNetwork::ENetAddressType _ExpectedType, void const* _opRawData, umint _nDataBytes);

		CPOSIXAddress* f_ResolveAddress(const NMib::NStr::CStr &_Address, NMib::NNetwork::ENetAddressType _PreferType, bool _bThrowOnError);
		CPOSIXAddress* f_ResolveAddress(const NMib::NStr::CStr &_Address, NMib::NNetwork::ENetAddressType _PreferType = NMib::NNetwork::ENetAddressType_None);

		void *f_AsyncResolveAddress_Open(const NMib::NStr::CStr &_Address, ::NMib::NNetwork::ENetAddressType _PreferType, NMib::NFunction::TCFunctionMutable<void ()> &&_fOnFinish);
		bool f_AsyncResolveAddress_GetResult(void *_pResolver, CPOSIXAddress*& _opAddress, NMib::NStr::CStr &_Error);
		void f_AsyncResolveAddress_Close(void *_pResolver);

		int f_CompareAddresses(CPOSIXAddress const& _pFirst, CPOSIXAddress const& _pSecond);

		void f_FreeAddress(CPOSIXAddress* _pAddress); // It is OK to free a nullptr address.

		NMib::NStr::CStr f_GetAddressString(CPOSIXAddress const &_Address, ENetAddressStringFlag _Flags);

	// Connection Operations
		CPOSIXSocket *f_AsyncConnect
			(
				CPOSIXAddress const &_Address
				, NMib::NFunction::TCFunctionMovable<void (NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
				, CPOSIXAddress const *_pBindAddress
			)
		;

		CPOSIXSocket *f_Listen
			(
				CPOSIXAddress const &_Address
				, NMib::NFunction::TCFunctionMovable<void (NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
				, NMib::NNetwork::ENetFlag _Flags
			)
		;
		CPOSIXSocket *f_ListenDatagram
			(
				CPOSIXAddress const &_Address
				, NMib::NFunction::TCFunctionMovable<void (NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
				, NMib::NNetwork::ENetFlag _Flags
			)
		;
		CPOSIXSocket *f_Accept(CPOSIXSocket *_pSocket, NMib::NFunction::TCFunctionMovable<void (NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange);

		void f_StartSocket(CPOSIXSocket *_pSocket);

		bool f_Close(CPOSIXSocket* _pSocket);
		void f_Shutdown(CPOSIXSocket* _pSocket);

		umint f_Receive(CPOSIXSocket *_pSocket, void *_pData, umint _DataLen, bool &o_bEndOfStream);
		umint f_Send(CPOSIXSocket *_pSocket, const void *_pData, umint _DataLen);
		umint f_SendVectored(CPOSIXSocket *_pSocket, NMib::NSys::CIoSpan const *_pSpans, umint _nSpans);
		umint f_SendDatagram(CPOSIXSocket *_pSocket, CPOSIXAddress const &_Address, const void *_pData, umint _DataLen);
		umint f_ReceiveDatagram(CPOSIXSocket *_pSocket, CPOSIXAddress &_Address, void *_pData, umint _DataLen);

	// Socket Properties & State

		void f_SetOnStateChange(CPOSIXSocket* _pSocket, NMib::NFunction::TCFunctionMovable<void (NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange);

		NMib::NNetwork::ENetTCPState f_GetState(CPOSIXSocket *_pSocket);

		NMib::NStr::CStr f_GetCloseReason(CPOSIXSocket* _pSocket);

		CPOSIXSocket* f_InheritHandle2(void *_pOSSocket, NMib::NFunction::TCFunctionMovable<void (NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange);
		void *f_GiveUpForInherit(CPOSIXSocket *_pSocket);
		void f_GiveUpForInheritAsync(CPOSIXSocket *_pSocket, NMib::NFunction::TCFunctionMovable<void (void *_pSocketHandle)> &&_fOnHandle);
		void *f_GetOSSocket(CPOSIXSocket *_pSocket);

		CPOSIXAddress* f_GetPeerAddress(CPOSIXSocket *_pSocket);
		bool f_GetProcessIdentity(CPOSIXSocket *_pSocket, NMib::NSys::NNetwork::CProcessIdentity &o_LocalIdentity, NMib::NSys::NNetwork::CProcessIdentity &o_PeerIdentity);
		uint32 f_GetListenPort(CPOSIXSocket *_pSocket);

private:

	// Final teardown once no poller holds the socket: closes the descriptor, removes any unix
	// socket file and frees the object. Runs on the closing thread for poller-owned sockets and on
	// the loop's thread for the asynchronous path
	void fp_DestroySocket(CPOSIXSocket *_pSocket);

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

		// The process wide shared loop for sockets nobody claimed, hosted on this dedicated
		// thread. Created by the context before the thread starts and destroyed after it stops
		NMib::NSys::ICIoLoop *mp_pLoop = nullptr;
		NMib::NAtomic::TCAtomic<smint> mp_bStop{0};
	};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated"

	void fp_ToNative(NMib::NNetwork::CNetAddressTCPv4 const& _InAddr, sockaddr_in& _OutAddr) const
	{
		fg_MemClear(_OutAddr);
#ifdef DPlatformFamily_macOS
		_OutAddr.sin_len = sizeof(_OutAddr);
#endif
		_OutAddr.sin_family = AF_INET;

		_OutAddr.sin_addr.s_addr = _InAddr.m_IP[3] << 24 | _InAddr.m_IP[2] << 16 | _InAddr.m_IP[1] << 8 |	_InAddr.m_IP[0];

		_OutAddr.sin_port = htons(_InAddr.m_Port);
	}

	void fp_ToNative(NMib::NNetwork::CNetAddressTCPv6 const& _InAddr, sockaddr_in6& _OutAddr) const
	{
		fg_MemClear(_OutAddr);
#ifdef DPlatformFamily_macOS
		_OutAddr.sin6_len = sizeof(_OutAddr);
#endif
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

#pragma clang diagnostic pop

	bool fp_GetSocketCreateParams(NMib::NNetwork::ENetAddressType _AddressType, CPOSIXImpSpecificSocketContext::CSocketCreateParams &o_Params);

	CPOSIXSocket *fp_Connect
		(
			CPOSIXAddress const &_Address
			, NMib::NFunction::TCFunctionMovable<void (NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
			, CPOSIXAddress const *_pBindAddress
		)
	;
	CPOSIXSocket *fp_CreateSocket
		(
			int _FD
			, EPOSIXSocketMode _Mode
			, EPOSIXSocketEvent _Events
			, NMib::NFunction::TCFunctionMovable<void (NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
			, bool _bFromInherit = false
		)
	;
	void fp_PrepareUnixListen(CPOSIXAddress const &_Address);
	void fp_SetUnixListenAddress(CPOSIXSocket *_pSocket, CPOSIXAddress const &_Address);

	CPOSIXImpSpecificSocketContext mp_ImpSpecific;

	CPollerThread mp_PollerThread;

	// TODO: This should be able to be replaced by an imp specific version.
	CAddressResolver mp_Resolver;
};
