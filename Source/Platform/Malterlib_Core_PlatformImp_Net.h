// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>

// A simple async name resolver.
struct CAddressResolver
{
	CAddressResolver();
	~CAddressResolver();

	void f_Stop();

	void *f_Open(NMib::NStr::CStr const &_Name, ::NMib::NNetwork::ENetAddressType _PreferType, NMib::NFunction::TCFunctionMutable<void ()> &&_fOnFinish);
	bool f_GetResult(void *_pResolver, NMib::NSys::NNetwork::CAddress &o_pAddress, NMib::NStr::CStr &_Error);
	void f_Close(void *_pResolver);
	void f_CloseAsync(void *_pResolver, NMib::NFunction::TCFunctionMovable<void ()> &&_fOnClosed);

	bool f_IsEmpty();

private:
	enum EFlag
	{
		EFlag_None      = 0
		, EFlag_Done    = DMibBit(0)
		, EFlag_Error   = DMibBit(1)
		, EFlag_Pending = DMibBit(2)
		, EFlag_Running = DMibBit(3)
		, EFlag_Closing = DMibBit(4)
	};

	// Request state and list membership are protected by CAddressResolver::mp_Lock.
	struct CResolveRequest
	{
		~CResolveRequest();

		NStr::CStr m_Name;
		NMib::NSys::NNetwork::CAddress m_pAddress = nullptr;
		NMib::NFunction::TCFunctionMutable<void ()> m_fOnFinish;
		NMib::NFunction::TCFunctionMovable<void ()> m_fOnClosed;
		NMib::NStr::CStr m_ErrorString;

		DMibListLinkS_Link(CResolveRequest, m_Link);
		NMib::NNetwork::ENetAddressType m_PreferType;
		EFlag m_Flags = EFlag_None;
	};

	static void fsp_CloseRequest(CResolveRequest *_pRequest);

	aint fp_ResolveWorker(NThread::CThreadObject *_pThread);

	NThread::CMutual mp_Lock;
	DMibListLinkS_List(CResolveRequest, m_Link) mp_PendingList;
	DMibListLinkS_List(CResolveRequest, m_Link) mp_DoneOrInProgressList;

	NStorage::TCUniquePointer<NThread::CThreadObject> mp_pThread;
	bool mp_bStopped = false;
};

struct CUnixAddress
{
	ch8 const *f_GetPath() const;

	static NStorage::TCOptional<CUnixAddress> fs_Parse(NMib::NStr::CStr const &_Address, bool _bThrowOnError);
	CUnixAddress f_GetStaged() const;

	static constexpr umint mc_MaxAddressLength = sizeof(sockaddr_un::sun_path) - 1;

	sockaddr_un m_UnixAddress;
	NFile::EFileAttrib m_Permissions = NFile::EFileAttrib_None;
};

struct CRuntimeNetAddress
{
	CRuntimeNetAddress();
	CRuntimeNetAddress(CRuntimeNetAddress const &_ToCopy);
	CRuntimeNetAddress(NMib::NNetwork::ENetAddressType _Type, void const *_pData, umint _nDataBytes);
	CRuntimeNetAddress(sockaddr_in const &_TCPv4);
	CRuntimeNetAddress(sockaddr_in6 const &_TCPv6);
	CRuntimeNetAddress(CUnixAddress const &_Unix);

	static void fs_ParseEndpoint(NStr::CStr const &_Address, NSys::NNetwork::CResolveAddressParameters &o_Parameters, NStr::CStr &o_Service);

	static NStorage::TCUniquePointer<CRuntimeNetAddress> fs_FromNative(void const *_pAddress, umint _Size);

	template <typename tf_CAddressInfo>
	static auto fs_FromResolved(tf_CAddressInfo const *_pResults) -> NContainer::TCVector<NSys::NNetwork::CAddress>;

	NMib::NNetwork::ENetAddressType f_GetType() const;
	void f_Set(NMib::NNetwork::ENetAddressType _Type, void const *_pData, umint _nDataBytes);
	void *f_GetForWrite(NMib::NNetwork::ENetAddressType _Type, umint _nDataBytes);
	void f_Set(sockaddr_in const &_TCPv4);
	void f_Set(sockaddr_in6 const &_TCPv6);
	void f_Set(CUnixAddress const &_Unix);

	uint32 f_GetScopeID() const;

	umint f_GetFullDataLen() const;
	umint f_GetSockAddrLen() const;
	void const *f_Get() const;
	sockaddr_in const &f_GetTCPv4() const;
	sockaddr_in6 const &f_GetTCPv6() const;
	CUnixAddress const &f_GetUnix() const;
	sockaddr_in &f_GetTCPv4();
	sockaddr_in6 &f_GetTCPv6();
	CUnixAddress &f_GetUnix();

	template <typename tf_CAddrType>
	tf_CAddrType const &f_GetAsType(NMib::NNetwork::ENetAddressType _ExpectedType) const;

	int f_Compare(CRuntimeNetAddress const &_Other) const;

private:
	NMib::NNetwork::ENetAddressType mp_Type;
	CByteVector mp_Data;
};

#include "Malterlib_Core_PlatformImp_Net_Address.hpp"
#include "Malterlib_Core_PlatformImp_Net.hpp"
