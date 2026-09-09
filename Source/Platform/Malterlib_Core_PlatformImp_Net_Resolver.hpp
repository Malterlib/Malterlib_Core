// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

void CAddressResolver::fs_CloseRequest(CResolveRequest *_pRequest)
{
	NStorage::TCUniquePointer<CResolveRequest> pRequest = fg_Explicit(_pRequest);
	auto fOnClosed = fg_Move(pRequest->m_fOnClosed);
	pRequest.f_Clear();

	if (fOnClosed)
		fOnClosed();
}

void CAddressResolver::f_CloseAsync(void *_pResolver, NMib::NFunction::TCFunctionMovable<void ()> &&_fOnClosed)
{
	auto *pReq = static_cast<CResolveRequest *>(_pResolver);

	{
		DMibLock(mp_Lock);
		pReq->m_fOnClosed = fg_Move(_fOnClosed);
		pReq->m_Flags |= EFlag_Closing;

		// The worker owns a running request until both the lookup and its callback return.
		if (pReq->m_Flags & EFlag_Running)
			return;

		if (pReq->m_Flags & EFlag_Pending)
			mp_PendingList.f_Remove(pReq);
		else
			mp_DoneOrInProgressList.f_Remove(pReq);
	}

	fs_CloseRequest(pReq);
}
