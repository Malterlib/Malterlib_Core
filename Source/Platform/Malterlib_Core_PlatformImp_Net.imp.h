// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_PlatformImp_Net.h"

// NOTE: 	The order in which the locks are taken is critical to avoiding deadlocks!

CAddressResolver::CAddressResolver()
{

	mp_lThreads.f_SetLen(nWorkerThreads);
	for (auto TIter = mp_lThreads.f_GetIterator()
		;TIter
		;++TIter)
	{
		(*TIter) = NThread::CThreadObject::fs_StartThread(
				[&](NThread::CThreadObject* _pThread) -> aint
				{
					return fp_ResolveWorker(_pThread);
				}
				, "Async Resolver Worker"
			);
	}
}

CAddressResolver::~CAddressResolver()
{
	for (auto TIter = mp_lThreads.f_GetIterator()
		;TIter
		;++TIter)
	{
		(*TIter)->f_Stop(true);
	}

	DMibLock(mp_Lock);
		mp_PendingList.f_DeleteAllDefiniteType();
		mp_DoneOrInProgressList.f_DeleteAllDefiniteType();
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

	mp_WakeEvent.f_Signal();

	return pRet;
}

bint CAddressResolver::f_GetResult(void *_pResolver, NMib::NSys::NNetwork::CAddress& _oAddress, NMib::NStr::CStr &_Error)
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
	delete pReq;
}

aint CAddressResolver::fp_ResolveWorker(NThread::CThreadObject* _pThread)
{
	mp_WakeEvent.f_ReportTo(&_pThread->m_EventWantQuit);

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

bint CAddressResolver::f_IsEmpty()
{
	DMibLock(mp_Lock);
	return mp_PendingList.f_IsEmpty() && mp_DoneOrInProgressList.f_IsEmpty();
}
