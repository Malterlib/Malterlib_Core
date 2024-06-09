// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_PlatformImp_Net.h"

// NOTE:	The order in which the locks are taken is critical to avoiding deadlocks!

CAddressResolver::CAddressResolver()
{
	mp_pThread = NThread::CThreadObject::fs_StartThread
		(
			[this](NThread::CThreadObject* _pThread) -> aint
			{
				return fp_ResolveWorker(_pThread);
			}
			, "Async Resolver Worker"
		)
	;
}

CAddressResolver::~CAddressResolver()
{
	if (mp_pThread)
	{
		mp_pThread->f_Stop(true);
		mp_pThread.f_Clear();
	}

	{
		DMibLock(mp_Lock);
		mp_PendingList.f_DeleteAllDefiniteType();
		mp_DoneOrInProgressList.f_DeleteAllDefiniteType();
	}
}

void* CAddressResolver::f_Open(NMib::NStr::CStr const& _Name, ::NMib::NNetwork::ENetAddressType _PreferType, NMib::NFunction::TCFunction<void ()> &&_fOnFinish)
{
	NStorage::TCUniquePointer<CResolveRequest> pReq = fg_Construct();

	pReq->m_Name = _Name;
	pReq->m_Flags = EFlag_Pending;
	pReq->m_fOnFinish = fg_Move(_fOnFinish);
	pReq->m_Address = nullptr;
	pReq->m_PreferType = _PreferType;

	CResolveRequest* pRet = nullptr;

	{
		DMibLock(mp_Lock);
		mp_PendingList.f_Push(pRet = pReq.f_Detach());
	}

	mp_pThread->m_EventWantQuit.f_Signal();

	return pRet;
}

bool CAddressResolver::f_GetResult(void *_pResolver, NMib::NSys::NNetwork::CAddress& _oAddress, NMib::NStr::CStr &_Error)
{
	CResolveRequest* pReq = (CResolveRequest*)_pResolver;

	DMibLock(pReq->m_Lock);
	if (pReq->m_Flags & EFlag_Done)
	{		
		_oAddress = pReq->m_Address;
		pReq->m_Address = nullptr;
		return true;
	}
	else if (pReq->m_Flags & EFlag_Error)
	{
		_Error = pReq->m_ErrorString;
		return false;
	}
	else
	{
		_Error = "Name resolution not yet complete.";
		return false;
	}
}

void CAddressResolver::f_Close(void* _pResolver)
{
	CResolveRequest* pReq = (CResolveRequest*)_pResolver;

	{
		DMibLock(mp_Lock);
		NMib::NThread::TCScopeLock<decltype(pReq->m_Lock)> ReqLocker(pReq->m_Lock);

		if (pReq->m_Flags & EFlag_Pending)
			mp_PendingList.f_Remove(pReq);
		else
			mp_DoneOrInProgressList.f_Remove(pReq);
	}
	fg_DeleteObject(NMemory::CDefaultAllocator(), pReq);
}

aint CAddressResolver::fp_ResolveWorker(NThread::CThreadObject* _pThread)
{
	while(_pThread->f_GetState() != NThread::EThreadState_EventWantQuit)
	{		
		CResolveRequest* pReq;

		while (1)
		{
			{
				DMibLock(mp_Lock);
				pReq = mp_PendingList.f_Pop();

				if (!pReq)
					break;

				mp_DoneOrInProgressList.f_Push(pReq);
				pReq->m_Flags &= ~EFlag_Pending;
				pReq->m_Lock.f_Lock();
			}

			try
			{
				pReq->m_Address = NMib::NSys::NNetwork::fg_ResolveAddress(pReq->m_Name, pReq->m_PreferType);
			}
			catch(NMib::NNetwork::CExceptionNet const &_Error)
			{
				pReq->m_ErrorString = _Error.f_GetErrorStr();
				pReq->m_Address = nullptr;
			}

			pReq->m_Flags |= pReq->m_Address ? EFlag_Done : EFlag_Error;

			if (pReq->m_fOnFinish)
				pReq->m_fOnFinish();

			pReq->m_Lock.f_Unlock();
		}

		_pThread->m_EventWantQuit.f_Wait();
	}

	return 0;
}

bool CAddressResolver::f_IsEmpty()
{
	DMibLock(mp_Lock);
	return mp_PendingList.f_IsEmpty() && mp_DoneOrInProgressList.f_IsEmpty();
}


NStorage::TCOptional<CUnixAddress> CUnixAddress::fs_Parse(NMib::NStr::CStr const &_Address, bool _bThrowOnError)
{
	using namespace NMib::NFile;

	EFileAttrib Permissions = EFileAttrib_None;
	CStr Address;

	if (_Address.f_StartsWith("UNIX("))
	{
		auto *pParse = _Address.f_GetStr() + 5;
		{
			bool bFailed = false;
			uint32 UnixPermissions = fg_StrToIntParse(pParse, uint32(01000), "),:", false, EStrToIntParseMode_Octal, &bFailed);

			if (bFailed)
			{
				if (_bThrowOnError)
					DMibErrorNet("Failed to parse unix permissions");
				else
					return {};
			}

			if (UnixPermissions >= uint32(01000))
			{
				if (_bThrowOnError)
					DMibErrorNet("Invalid permissions specified");
				else
					return {};
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
		}

		{
			if (pParse[0] != ')' || pParse[1] != ':')
			{
				if (_bThrowOnError)
					DMibErrorNet("Failed to parse unix permissions");
				else
					return {};
			}

			pParse += 2;
			Address = CStr{pParse};
		}
	}
	else
		Address = _Address.f_Extract(fg_StrLen("UNIX:"));

	if (Address.f_GetLen() > CUnixAddress::mc_MaxAddressLength)
	{
		if (_bThrowOnError)
			DMibErrorNet(fg_Format("Unix sockets support a maximum path length of {} characters. Invalid path '{}'", CUnixAddress::mc_MaxAddressLength, Address));
		else
			return {};
	}

	CUnixAddress AddressWithPermissions;
	AddressWithPermissions.m_Permissions = Permissions;

	auto &AddressUn = AddressWithPermissions.m_UnixAddress;

	AddressUn.sun_family = AF_UNIX;
	NMib::NStr::fg_StrCopy(AddressUn.sun_path, Address, CUnixAddress::mc_MaxAddressLength + 1);

#if defined(DPlatformFamily_macOS)
	AddressUn.sun_len = sizeof(AddressUn);
#endif

	return fg_Move(AddressWithPermissions);
}
