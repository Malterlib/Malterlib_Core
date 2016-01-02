// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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
			NMib::NSys::NNet::fg_FreeAddress(m_Address);
		}

		NStr::CStr m_Name;

		NThread::CMutual m_Lock;
			EFlag m_Flags;
			NMib::NSys::NNet::CAddress m_Address;
			NMib::NFunction::TCFunction<void ()> m_fOnFinish;
			NMib::NStr::CStr m_ErrorString;
			NMib::NNet::ENetAddressType m_PreferType;

		// Protected by CResolveThread::mp_Lock.
		DMibListLinkS_Link(CResolveRequest, m_Link);
	};	

	NThread::CMutual mp_Lock;
		DMibListLinkS_List(CResolveRequest, m_Link) mp_PendingList;
		DMibListLinkS_List(CResolveRequest, m_Link) mp_DoneOrInProgressList;

	NMib::NThread::CEventAutoResetReportable mp_TerminateEvent;
	NMib::NThread::CEventAutoResetReportable mp_WakeEvent;

	typedef NPtr::TCUniquePointer<NThread::CThreadObject, NMem::CDefaultAllocator, TCDynamicPtr<typename NMem::CDefaultAllocator::CPtrHolder, NThread::CThreadObject>, void>  CThreadPointer;

	enum
	{
		nWorkerThreads = 2, // TODO
	};

	NContainer::TCVector<CThreadPointer> mp_lThreads;

	aint fp_ResolveWorker(NThread::CThreadObject* _pThread);

public:
	CAddressResolver();
	~CAddressResolver();

	void* f_Open(NMib::NStr::CStr const& _Name, ::NMib::NNet::ENetAddressType _PreferType, NMib::NFunction::TCFunction<void ()>&& _fOnFinish);
	bint f_GetResult(void *_pResolver, NMib::NSys::NNet::CAddress& _oAddress, NMib::NStr::CStr &_Error);
	void f_Close(void* _pResolver);

	bint f_IsEmpty();
};

struct CRuntimeNetAddress
{
private:
	NMib::NNet::ENetAddressType mp_Type;
	TCVector<uint8> mp_lData;

public:
	CRuntimeNetAddress()
		: mp_Type(NMib::NNet::ENetAddressType_None)
	{}

	CRuntimeNetAddress(CRuntimeNetAddress const& _ToCopy)
		: mp_Type(_ToCopy.mp_Type)
		, mp_lData(_ToCopy.mp_lData)
	{
	}

	CRuntimeNetAddress(NMib::NNet::ENetAddressType _Type, void const* _pData, mint _nDataBytes)
		: mp_Type(NMib::NNet::ENetAddressType_None)
	{
		f_Set(_Type, _pData, _nDataBytes);
	}

	CRuntimeNetAddress(sockaddr_in const& _TCPv4)
		: mp_Type(NMib::NNet::ENetAddressType_None)
	{
		f_Set(_TCPv4);
	}

	CRuntimeNetAddress(sockaddr_in6 const& _TCPv6)
		: mp_Type(NMib::NNet::ENetAddressType_None)
	{
		f_Set(_TCPv6);
	}

	NMib::NNet::ENetAddressType f_GetType() const
	{
		return mp_Type;
	}

	void f_Set(NMib::NNet::ENetAddressType _Type, void const* _pData, mint _nDataBytes)
	{
		mp_Type = _Type;
		mp_lData.f_SetLen(_nDataBytes);
		fg_MemCopy(mp_lData.f_GetArray(), _pData, _nDataBytes);
	}
	
	void *f_GetForWrite(NMib::NNet::ENetAddressType _Type, mint _nDataBytes)
	{
		mp_Type = _Type;
		mp_lData.f_SetLen(_nDataBytes);
		return mp_lData.f_GetArray();
	}

	void f_Set(sockaddr_in const& _TCPv4)
	{
		f_Set(NMib::NNet::ENetAddressType_TCPv4, &_TCPv4, sizeof(sockaddr_in));
	}

	void f_Set(sockaddr_in6 const& _TCPv6)
	{
		f_Set(NMib::NNet::ENetAddressType_TCPv6, &_TCPv6, sizeof(sockaddr_in6));
	}

	mint f_GetLen() const { return mp_lData.f_GetLen(); }
	void const* f_Get() const { return mp_lData.f_GetArray(); }

	sockaddr_in const& f_GetTCPv4() const { return *(sockaddr_in const*)mp_lData.f_GetArray(); }
	sockaddr_in6 const& f_GetTCPv6() const { return *(sockaddr_in6 const*)mp_lData.f_GetArray(); }

	sockaddr_in & f_GetTCPv4() { return *(sockaddr_in *)mp_lData.f_GetArray(); }
	sockaddr_in6 & f_GetTCPv6() { return *(sockaddr_in6 *)mp_lData.f_GetArray(); }

	template<typename t_CAddrType>
	t_CAddrType const& f_GetAsType(NMib::NNet::ENetAddressType _ExpectedType) const
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

