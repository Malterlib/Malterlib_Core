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
	EWindowsSocketEvent_Read    = 1 << 0
	, EWindowsSocketEvent_Write = 1 << 1
};

enum EWindowsSocketMode
{
	EWindowsSocketMode_Connecting
	, EWindowsSocketMode_Connect
	, EWindowsSocketMode_Listen
	, EWindowsSocketMode_Datagram
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

	CWindowsSocket
		(
			SOCKET _Socket
			, EWindowsSocketMode _Mode
			, EWindowsSocketEvent _Events
			, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
		)
	;

	uint32 m_Magic = 0x4EA11E49; // Cross-module ABI: preserve the first three member offsets for older socket layouts.
	uint32 m_Version = 0x102;

	SOCKET m_Socket;
	EWindowsSocketMode m_Mode;
	EWindowsSocketEvent m_RegisteredEvents;
	umint m_BindAddressSize = 0;
	ENetAddressType m_AddressType = ENetAddressType_None;
	TCUniquePointer<CUnixListenState> m_pUnixListen;
	CStr m_UnixListenPath;              // Cleared after unlink; bind-time identity protects successor socket files.
	NFile::CUniqueFileIdentifier m_UnixListenFileIdentity;
	CStr m_PeerUnixFilePath;            // The path connected to, or the listener's path for accepted sockets
	bool m_bPeerIsUnixListener = false; // Connected sockets: the kernel names the listener by the staged path it bound

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
	umint m_nSendWindowBytes = 0;                // Unreleased-byte limit; zero until explicitly configured.
	uint64 m_PathLastBytesOut = 0;
	uint64 m_PathLastStamp = 0;
	umint m_nSendBufferBytesToApply = umint(-1); // Decided before registration, applied on first completion send; all-ones preserves system policy.

	NMib::NSys::ICIoLoop *m_pOwningLoop = nullptr;                // Fixed at socket start; deregistration must use the same loop.
	NMib::NSys::CIoLoopRegistration *m_pIoRegistration = nullptr; // Non-null while registered; the loop owns this opaque registration.
	bool m_bInheritable = false;                                  // Set before start to avoid permanent completion binding; listeners pass it to accepted sockets.
	bool m_bFromInherit = false;                                  // Handed over by another process, see CIoLoopRegisterOptions::m_bInheritedHandle
};

#if DMibConfig_IoDebug_Enable
NSys::CSocketIoStats *fg_SocketIoStats();
#endif

struct CIoSubSystem_Windows;

struct CWindowsSocketContext
{
	CWindowsSocketContext();
	~CWindowsSocketContext();

	void f_CheckFailed();

	// Address
	CWindowsAddress *f_CreateAddress(NMib::NNetwork::ENetAddressType _Type, void const *_pData, umint _nDataBytes);
	CWindowsAddress *f_DuplicateAddress(CWindowsAddress *_pAddress);

	NMib::NNetwork::ENetAddressType f_GetAddressType(CWindowsAddress const &_Address);
	bool f_GetAddressRaw(CWindowsAddress const &_Address, NMib::NNetwork::ENetAddressType _ExpectedType, void *o_pRawData, umint _nDataBytes);
	CWindowsAddress *f_SetAddressRaw(CWindowsAddress *_pAddress, ::NMib::NNetwork::ENetAddressType _Type, void const *_pRawData, umint _nDataBytes);

	CWindowsAddress *f_ResolveAddress(NMib::NStr::CStr const &_Address, NMib::NNetwork::ENetAddressType _PreferType = NMib::NNetwork::ENetAddressType_None);
	CWindowsAddress *f_ResolveAddress(NMib::NStr::CStr const &_Address, NMib::NNetwork::ENetAddressType _PreferType, bool _bThrowOnError);

	NSys::NNetwork::CAddress f_PrepareResolveAddress(NStr::CStr const &_Address, NSys::NNetwork::CResolveAddressParameters &o_Parameters, bool _bThrowOnError = true);

	auto f_ResolveAddresses(NStr::CStr const &_Address, NMib::NNetwork::ENetAddressType _PreferType, bool _bThrowOnError = true) -> NContainer::TCVector<NSys::NNetwork::CAddress>;

	NContainer::TCVector<NSys::NNetwork::CAddress> f_ResolveHost(NStr::CStr const &_Host, NMib::NNetwork::ENetAddressType _PreferType);

	void *f_AsyncResolveAddress_Open(NMib::NStr::CStr const &_Address, ::NMib::NNetwork::ENetAddressType _PreferType, NMib::NFunction::TCFunctionMutable<void ()> &&_fOnFinish);
	bool f_AsyncResolveAddress_GetResult(void *_pResolver, CWindowsAddress * &o_pAddress, NMib::NStr::CStr &_Error);
	void f_AsyncResolveAddress_Close(void *_pResolver);
	void f_AsyncResolveAddress_CloseAsync(void *_pResolver, NMib::NFunction::TCFunctionMovable<void ()> &&_fOnClosed);

	int f_CompareAddresses(CWindowsAddress const &_First, CWindowsAddress const &_Second);

	void f_FreeAddress(CWindowsAddress *_pAddress); // It is OK to free a nullptr address.

	NMib::NStr::CStr f_GetAddressString(CWindowsAddress const &_Address, ENetAddressStringFlag _Flags);

	// Connection Operations
	auto f_AsyncConnect
		(
			CWindowsAddress const &_Address
			, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
			, CWindowsAddress const *_pBindAddress
		)
		-> CWindowsSocket *
	;

	void f_StartSocket(CWindowsSocket *_pSocket);

	auto f_Listen(CWindowsAddress const &_Address, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange, NMib::NNetwork::ENetFlag _Flags)
		-> CWindowsSocket *
	;
	auto f_ListenDatagram
		(
			CWindowsAddress const &_Address
			, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
			, NMib::NNetwork::ENetFlag _Flags
		)
		-> CWindowsSocket *
	;
	CWindowsSocket *f_Accept(CWindowsSocket *_pSocket, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange);

	bool f_Close(CWindowsSocket *_pSocket);
	void f_CloseAsync(CWindowsSocket *_pSocket, NMib::NFunction::TCFunctionMovable<void ()> &&_fOnClosed);
	bool f_Shutdown(CWindowsSocket *_pSocket);

	umint f_Receive(CWindowsSocket *_pSocket, void *_pData, umint _DataLen, bool &o_bEndOfStream);
	umint f_Send(CWindowsSocket *_pSocket, void const *_pData, umint _DataLen);
	umint f_SendVectored(CWindowsSocket *_pSocket, NMib::NSys::CIoSpan const *_pSpans, umint _nSpans);
	bool f_GetProcessIdentity(CWindowsSocket *_pSocket, NMib::NSys::NNetwork::CProcessIdentity &o_LocalIdentity, NMib::NSys::NNetwork::CProcessIdentity &o_PeerIdentity);
	umint f_SendDatagram(CWindowsSocket *_pSocket, CWindowsAddress const &_Address, void const *_pData, umint _DataLen);
	umint f_ReceiveDatagram(CWindowsSocket *_pSocket, CWindowsAddress &_Address, void *_pData, umint _DataLen);

	// Socket Properties & State

	void f_SetOnStateChange(CWindowsSocket *_pSocket, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange);

	NMib::NNetwork::ENetTCPState f_GetState(CWindowsSocket *_pSocket);

	NMib::NStr::CStr f_GetCloseReason(CWindowsSocket *_pSocket);

	CWindowsSocket *f_InheritHandle2(void *_pOSSocket, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange);
	void *f_GiveUpForInherit(CWindowsSocket *_pSocket);
	void f_GiveUpForInheritAsync(CWindowsSocket *_pSocket, NMib::NFunction::TCFunctionMovable<void (void *_pSocketHandle)> &&_fOnHandle);
	void f_CloseSocketHandle(void *_pSocketHandle);
	void *f_GetOSSocket(CWindowsSocket *_pSocket);

	CWindowsAddress *f_GetPeerAddress(CWindowsSocket *_pSocket);
	uint32 f_GetListenPort(CWindowsSocket *_pSocket);

protected:
	void fp_ToNative(NMib::NNetwork::CNetAddressTCPv4 const &_InAddr, sockaddr_in &_OutAddr) const;
	void fp_ToNative(NMib::NNetwork::CNetAddressTCPv6 const &_InAddr, sockaddr_in6 &_OutAddr) const;
	void fp_FromNative(sockaddr_in const &_InAddr, NMib::NNetwork::CNetAddressTCPv4 &_OutAddr) const;
	void fp_FromNative(sockaddr_in6 const &_InAddr, NMib::NNetwork::CNetAddressTCPv6 &_OutAddr) const;

	auto fp_Connect
		(
			CWindowsAddress const &_Address
			, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
			, CWindowsAddress const *_pBindAddress
		)
		-> CWindowsSocket *
	;

	auto fp_CreateSocket
		(
			SOCKET _Socket
			, EWindowsSocketMode _Mode
			, EWindowsSocketEvent _Events
			, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
			, bool _bFromInherit = false
		)
		-> CWindowsSocket *
	;
	void fp_DestroySocket(CWindowsSocket *_pSocket);
	void fp_RemoveUnixListenFile(CWindowsSocket *_pSocket);

	TCUniquePointer<CWindowsSocket::CUnixListenState> fp_PrepareUnixListen(CWindowsAddress &o_Address);

	bool mp_bInitFailed;
	bool mp_bWsaStarted = false;

	CIoSubSystem_Windows *mp_pIo = nullptr;

	CAddressResolver mp_Resolver;
};
