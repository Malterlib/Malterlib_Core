// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>

// A simple async name resolver.
class CAddressResolver
{
private:
	enum EFlag
	{
		EFlag_None		= 0,
		EFlag_Done		= DMibBit(0),
		EFlag_Error		= DMibBit(1),
		EFlag_Pending	= DMibBit(2),
	};

	struct CResolveRequest
	{
		CResolveRequest()
			: m_Flags(EFlag_None)
			, m_Address(nullptr)
		{}

		~CResolveRequest()
		{
			NMib::NSys::NNetwork::fg_FreeAddress(m_Address);
		}

		NStr::CStr m_Name;

		NThread::CMutual m_Lock;
			EFlag m_Flags;
			NMib::NSys::NNetwork::CAddress m_Address;
			NMib::NFunction::TCFunctionMutable<void ()> m_fOnFinish;
			NMib::NStr::CStr m_ErrorString;
			NMib::NNetwork::ENetAddressType m_PreferType;

		// Protected by CResolveThread::mp_Lock.
		DMibListLinkS_Link(CResolveRequest, m_Link);
	};

	NThread::CMutual mp_Lock;
		DMibListLinkS_List(CResolveRequest, m_Link) mp_PendingList;
		DMibListLinkS_List(CResolveRequest, m_Link) mp_DoneOrInProgressList;

	NStorage::TCUniquePointer<NThread::CThreadObject> mp_pThread;

	aint fp_ResolveWorker(NThread::CThreadObject* _pThread);

public:
	CAddressResolver();
	~CAddressResolver();

	// Stops and joins the worker and drops every request; what the destructor does, for an owner
	// that must have the worker gone before its own teardown continues
	void f_Stop();

	void* f_Open(NMib::NStr::CStr const& _Name, ::NMib::NNetwork::ENetAddressType _PreferType, NMib::NFunction::TCFunctionMutable<void ()> &&_fOnFinish);
	bool f_GetResult(void *_pResolver, NMib::NSys::NNetwork::CAddress& _oAddress, NMib::NStr::CStr &_Error);
	void f_Close(void* _pResolver);

	bool f_IsEmpty();
};

struct CUnixAddress
{
	static constexpr umint mc_MaxAddressLength = sizeof(sockaddr_un::sun_path) - 1;

	ch8 const *f_GetPath() const
	{
		return m_UnixAddress.sun_path;
	}

	static NStorage::TCOptional<CUnixAddress> fs_Parse(NMib::NStr::CStr const &_Address, bool _bThrowOnError);

	sockaddr_un m_UnixAddress;
	NFile::EFileAttrib m_Permissions = NFile::EFileAttrib_None;
};

struct CRuntimeNetAddress
{
private:
	NMib::NNetwork::ENetAddressType mp_Type;
	CByteVector mp_lData;

public:
	CRuntimeNetAddress()
		: mp_Type(NMib::NNetwork::ENetAddressType_None)
	{}

	CRuntimeNetAddress(CRuntimeNetAddress const& _ToCopy)
		: mp_Type(_ToCopy.mp_Type)
		, mp_lData(_ToCopy.mp_lData)
	{
	}

	CRuntimeNetAddress(NMib::NNetwork::ENetAddressType _Type, void const* _pData, umint _nDataBytes)
		: mp_Type(NMib::NNetwork::ENetAddressType_None)
	{
		f_Set(_Type, _pData, _nDataBytes);
	}

	CRuntimeNetAddress(sockaddr_in const& _TCPv4)
		: mp_Type(NMib::NNetwork::ENetAddressType_None)
	{
		f_Set(_TCPv4);
	}

	CRuntimeNetAddress(sockaddr_in6 const& _TCPv6)
		: mp_Type(NMib::NNetwork::ENetAddressType_None)
	{
		f_Set(_TCPv6);
	}

	CRuntimeNetAddress(CUnixAddress const &_Unix)
		: mp_Type(NMib::NNetwork::ENetAddressType_None)
	{
		f_Set(_Unix);
	}

	NMib::NNetwork::ENetAddressType f_GetType() const
	{
		return mp_Type;
	}

	void f_Set(NMib::NNetwork::ENetAddressType _Type, void const* _pData, umint _nDataBytes)
	{
		mp_Type = _Type;
		mp_lData.f_SetLen(_nDataBytes);
		fg_MemCopy(mp_lData.f_GetArray(), _pData, _nDataBytes);
	}

	void *f_GetForWrite(NMib::NNetwork::ENetAddressType _Type, umint _nDataBytes)
	{
		mp_Type = _Type;
		mp_lData.f_SetLen(_nDataBytes);
		return mp_lData.f_GetArray();
	}

	void f_Set(sockaddr_in const& _TCPv4)
	{
		f_Set(NMib::NNetwork::ENetAddressType_TCPv4, &_TCPv4, sizeof(sockaddr_in));
	}

	void f_Set(sockaddr_in6 const& _TCPv6)
	{
		f_Set(NMib::NNetwork::ENetAddressType_TCPv6, &_TCPv6, sizeof(sockaddr_in6));
	}

	void f_Set(CUnixAddress const &_Unix)
	{
		f_Set(NMib::NNetwork::ENetAddressType_Unix, &_Unix, sizeof(CUnixAddress));
	}

	umint f_GetFullDataLen() const { return mp_lData.f_GetLen(); }
	umint f_GetSockAddrLen() const
	{
		using namespace NMib::NNetwork;
		switch (mp_Type)
		{
			case ENetAddressType_TCPv4: return sizeof(sockaddr_in);
			case ENetAddressType_TCPv6: return sizeof(sockaddr_in6);
			case ENetAddressType_Unix: return sizeof(sockaddr_un);
			default: return f_GetFullDataLen();
		}
	}
	void const* f_Get() const { return mp_lData.f_GetArray(); }

	sockaddr_in const& f_GetTCPv4() const { return *(sockaddr_in const*)mp_lData.f_GetArray(); }
	sockaddr_in6 const& f_GetTCPv6() const { return *(sockaddr_in6 const*)mp_lData.f_GetArray(); }
	CUnixAddress const& f_GetUnix() const { return *(CUnixAddress const*)mp_lData.f_GetArray(); }

	sockaddr_in & f_GetTCPv4() { return *(sockaddr_in *)mp_lData.f_GetArray(); }
	sockaddr_in6 & f_GetTCPv6() { return *(sockaddr_in6 *)mp_lData.f_GetArray(); }
	CUnixAddress &f_GetUnix()  { return *(CUnixAddress *)mp_lData.f_GetArray(); }

	template<typename t_CAddrType>
	t_CAddrType const& f_GetAsType(NMib::NNetwork::ENetAddressType _ExpectedType) const
	{
		DMibSafeCheck(mp_Type == _ExpectedType, "Address is not of the expected type.");
		return *(t_CAddrType const*)mp_lData.f_GetArray();
	}

	int f_Compare(CRuntimeNetAddress const& _Other) const
	{
		if (mp_Type < _Other.mp_Type)
			return -1;
		else if (mp_Type > _Other.mp_Type)
			return 1;
		else if (mp_lData.f_GetLen() < _Other.mp_lData.f_GetLen())
			return -1;
		else if (mp_lData.f_GetLen() > _Other.mp_lData.f_GetLen())
			return 1;
		else
			return fg_MemCmp(mp_lData.f_GetArray(), _Other.mp_lData.f_GetArray(), _Other.mp_lData.f_GetLen());
	}
};

