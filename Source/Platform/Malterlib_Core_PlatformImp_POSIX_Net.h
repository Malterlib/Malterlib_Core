// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>

#include "Malterlib_Core_PlatformImp_Net.h"

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
	mint m_BindAddressSize = 0;
	ENetAddressType m_AddressType = ENetAddressType_None;
	NStr::CStr m_UnixFilePath;
	NStr::CStr m_PeerUnixFilePath;

	// This is stull that will be changed or use by the poller etc...
	NMib::NThread::CMutual m_Lock;
	bool m_bInitialWriteNotification;
	NMib::NFunction::TCFunctionMovable<void (NMib::NNetwork::ENetTCPState _StateAdded)> m_fOnStateChange;
	NAtomic::TCAtomic<uint32> m_StateAtomic;
	int m_CloseError;
	bool m_bShutdownCalled = false;
	bool m_bNonErrorClose = false;
	bool m_bRemoteCloseSignalled = false;
	bool m_bIsRegistered = false;

	// This is set once, used and then cleared.
	NMib::NAtomic::TCAtomic<NMib::NThread::CEvent*> m_pDestructionReportTo;

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
		, m_pDestructionReportTo(nullptr)
		, m_fOnStateChange(fg_Move(_fOnStateChange))
	{}

	~CPOSIXSocket()
	{
//		DMibSafeCheck(m_FD == -1, "FD not closed upon socket destruction");
	}

};

typedef CRuntimeNetAddress CPOSIXAddress;

class CPOSIXImpSpecificSocketPoller
{
private:
	class CInternal;
	NMib::NStorage::TCUniquePointer<CInternal> mp_pInternal;

public:
	CPOSIXImpSpecificSocketPoller();
	~CPOSIXImpSpecificSocketPoller();

	// Called from any thread:
	void f_RegisterSocket(CPOSIXSocket* _pSocket);
	void f_DeregisterSocket(CPOSIXSocket* _pSocket);

	// Called from a dedicated poller thread:
	void f_Run(NThread::CThread* _pThread);

	// Called from any thread. Causes the f_Run loop to check the state of the thread.
	void f_Break();
};

class CPOSIXImpSpecificSocketContext
{
private:
	class CInternal;
	NMib::NStorage::TCUniquePointer<CInternal> mp_pInternal;

public:
	CPOSIXImpSpecificSocketContext();
	~CPOSIXImpSpecificSocketContext();

	bool f_CreateAddress(CPOSIXAddress& _oAddr, NMib::NNetwork::ENetAddressType _Type, void const* _pData, mint _nDataBytes);
	bool f_ResolveAddress(CPOSIXAddress& _oAddr, const NMib::NStr::CStr &_Address, NMib::NNetwork::ENetAddressType _PreferType = NMib::NNetwork::ENetAddressType_None);
	bool f_GetAddressRaw(CPOSIXAddress const &_Address, ENetAddressType _ExpectedType, void* _opRawData, mint _nDataBytes);
	CPOSIXAddress* f_SetAddressRaw(CPOSIXAddress* _Address, ::NMib::NNetwork::ENetAddressType _ExpectedType, void const* _pRawData, mint _nDataBytes);

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
		CPOSIXAddress* f_CreateAddress(NMib::NNetwork::ENetAddressType _Type, void const* _pData, mint _nDataBytes);

		CPOSIXAddress* f_DuplicateAddress(CPOSIXAddress const &_Address);

		NMib::NNetwork::ENetAddressType f_GetAddressType(CPOSIXAddress const& _pAddress);
		bool f_GetAddressRaw(CPOSIXAddress const &_Address, NMib::NNetwork::ENetAddressType _ExpectedType, void* _opRawData, mint _nDataBytes);
		CPOSIXAddress* f_SetAddressRaw(CPOSIXAddress* _Address, ::NMib::NNetwork::ENetAddressType _ExpectedType, void const* _opRawData, mint _nDataBytes);

		CPOSIXAddress* f_ResolveAddress(const NMib::NStr::CStr &_Address, NMib::NNetwork::ENetAddressType _PreferType, bool _bThrowOnError);
		CPOSIXAddress* f_ResolveAddress(const NMib::NStr::CStr &_Address, NMib::NNetwork::ENetAddressType _PreferType = NMib::NNetwork::ENetAddressType_None);

		void *f_AsyncResolveAddress_Open(const NMib::NStr::CStr &_Address, ::NMib::NNetwork::ENetAddressType _PreferType, NMib::NFunction::TCFunction<void ()> &&_fOnFinish);
		bool f_AsyncResolveAddress_GetResult(void *_pResolver, CPOSIXAddress*& _opAddress, NMib::NStr::CStr &_Error);
		void f_AsyncResolveAddress_Close(void *_pResolver);

		int f_CompareAddresses(CPOSIXAddress const& _pFirst, CPOSIXAddress const& _pSecond);

		void f_FreeAddress(CPOSIXAddress* _pAddress); // It is OK to free a nullptr address.

		NMib::NStr::CStr f_GetAddressString(CPOSIXAddress const &_Address, bool _bIncludeType);

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

		mint f_Receive(CPOSIXSocket *_pSocket, void *_pData, mint _DataLen);
		mint f_Send(CPOSIXSocket *_pSocket, const void *_pData, mint _DataLen);
		mint f_SendDatagram(CPOSIXSocket *_pSocket, CPOSIXAddress const &_Address, const void *_pData, mint _DataLen);
		mint f_ReceiveDatagram(CPOSIXSocket *_pSocket, CPOSIXAddress &_Address, void *_pData, mint _DataLen);

	// Socket Properties & State

		void f_SetOnStateChange(CPOSIXSocket* _pSocket, NMib::NFunction::TCFunctionMovable<void (NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange);

		NMib::NNetwork::ENetTCPState f_GetState(CPOSIXSocket *_pSocket);

		NMib::NStr::CStr f_GetCloseReason(CPOSIXSocket* _pSocket);

		CPOSIXSocket* f_InheritHandle2(void *_pOSSocket, NMib::NFunction::TCFunctionMovable<void (NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange);
		void *f_GiveUpForInherit(CPOSIXSocket *_pSocket);
		void *f_GetOSSocket(CPOSIXSocket *_pSocket);
		
		CPOSIXAddress* f_GetPeerAddress(CPOSIXSocket *_pSocket);
		uint32 f_GetListenPort(CPOSIXSocket *_pSocket);
		
private:

	struct CPollerThread : public NMib::NThread::CThread
	{
		CPOSIXImpSpecificSocketPoller mp_Poller;

		NStr::CStr f_GetThreadName() override
		{
			return CStr("POSIX SocketContext Poller Thread");
		}

		aint f_Main() override
		{
			mp_Poller.f_Run(this);
			return 0;
		}

		mint f_Stop(bool _bBlock) override
		{
			mp_Poller.f_Break();
			return NMib::NThread::CThread::f_Stop(_bBlock);
		}
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
