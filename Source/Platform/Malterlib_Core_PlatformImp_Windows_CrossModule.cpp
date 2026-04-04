// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_PlatformImp_Windows_CrossModule.h"

#include <Mib/Core/PlatformSpecific/WindowsOptional>

namespace NMib::NThread::NPlatform
{
	constinit CWindowsCrossModuleProcessInfo *CWindowsCrossModuleProcessInfo::ms_pThis = nullptr;

	using namespace NMib::NStr;

#ifndef DMibWindowsUseArbitraryUserPointerForThreadLocals
	uint32 CWindowsThreadLocals::ms_ThreadLocalsExtendedLocactionOffset = 0;
#endif

	umint CWindowsThreadLocals::ms_ThreadLocalsMinOffset = 0;
	umint CWindowsThreadLocals::ms_ThreadLocalsMaxOffset = 0;

	void fg_Windows_InitCrossModule()
	{
		NLocal::g_OptionalFunctions.m_fRtlAcquirePebLock();
		auto Cleanup = g_OnScopeExit / [&]
			{
				NLocal::g_OptionalFunctions.m_fRtlReleasePebLock();
			}
		;

		if (!FindAtom(str_utf16("MibCrossModuleAtom")))
		{
			void *pMemory = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(CWindowsCrossModuleProcessInfo));

			CWindowsCrossModuleProcessInfo::ms_pThis = new (pMemory) CWindowsCrossModuleProcessInfo();

			umint Pointer = (umint)CWindowsCrossModuleProcessInfo::ms_pThis;
			for (umint i = 0; i < sizeof(umint) * 8; ++i)
			{
				if (Pointer & (umint(1) << i))
				{
					AddAtomW(CFWStr128(CFWStr128::CFormat(str_utf16("MibCrossModuleAtom{}")) << i));
				}
			}
			AddAtomW(str_utf16("MibCrossModuleAtom"));
		}
		else
		{
			umint Pointer = 0;
			// This needs to be named exactly like this to be compatible with old versions of library (when Malterlib was named Ids)
			if (FindAtomW(str_utf16("MibCrossModuleAtom")))
			{
				for (umint i = 0; i < sizeof(umint) * 8; ++i)
				{
					if (FindAtomW(CFWStr128(CFWStr128::CFormat(str_utf16("MibCrossModuleAtom{}")) << i)))
						Pointer |= (umint(1) << i);
				}
			}

			CWindowsCrossModuleProcessInfo::ms_pThis = static_cast<CWindowsCrossModuleProcessInfo *>((void *)Pointer);
		}

		CWindowsCrossModuleProcessInfo::ms_pThis->m_RefCount.f_FetchAdd(1);

#ifndef DMibWindowsUseArbitraryUserPointerForThreadLocals
		CWindowsThreadLocals::ms_ThreadLocalsExtendedLocactionOffset = CWindowsCrossModuleProcessInfo::ms_pThis->m_ThreadLocal;
#endif

		CWindowsThreadLocals::ms_ThreadLocalsMinOffset = NLocal::fg_TlsIndexToTebOffset(0);
		CWindowsThreadLocals::ms_ThreadLocalsMaxOffset = NLocal::fg_TlsIndexToTebOffset(63);
	}

	void fg_Windows_DestroyCrossModule()
	{
		NLocal::g_OptionalFunctions.m_fRtlAcquirePebLock();
		auto Cleanup = g_OnScopeExit / [&]
			{
				NLocal::g_OptionalFunctions.m_fRtlReleasePebLock();
			}
		;
		DMibFastCheck(CWindowsCrossModuleProcessInfo::ms_pThis);

		if (CWindowsCrossModuleProcessInfo::ms_pThis->m_RefCount.f_FetchSub(1) == 1)
		{
			umint Pointer = (umint)CWindowsCrossModuleProcessInfo::ms_pThis;
			for (umint i = 0; i < sizeof(umint) * 8; ++i)
			{
				if (Pointer & (umint(1) << i))
					DeleteAtom(FindAtomW(CFWStr128(CFWStr128::CFormat(str_utf16("MalterlibCrossModuleAtom{}")) << i)));
			}
			DeleteAtom(FindAtomW(str_utf16("MalterlibCrossModuleAtom")));

			CWindowsCrossModuleProcessInfo::ms_pThis->~CWindowsCrossModuleProcessInfo();
			HeapFree(GetProcessHeap(), 0, CWindowsCrossModuleProcessInfo::ms_pThis);
		}
	}

	CWindowsCrossModuleProcessInfo::CWindowsCrossModuleProcessInfo()
#ifndef DMibWindowsUseArbitraryUserPointerForThreadLocals
		: m_ThreadLocal(NSys::fg_Thread_AllocLocalFast())
#endif
	{
	}

	CWindowsCrossModuleProcessInfo::~CWindowsCrossModuleProcessInfo()
	{
		auto pProcessHeap = GetProcessHeap();
		while (auto *pThreadInfo = m_ThreadInfos.f_GetFirst())
		{
			pThreadInfo->~CWindowsCrossModuleThreadInfo();
			HeapFree(pProcessHeap, 0, pThreadInfo);
		}

#ifndef DMibWindowsUseArbitraryUserPointerForThreadLocals
		NSys::fg_Thread_FreeLocalFast(m_ThreadLocal);
#endif
	}

	CWindowsCrossModuleThreadInfo *CWindowsCrossModuleProcessInfo::fs_GetOrCreateThreadInfo()
	{
		auto *pThreadLocals = NSys::NPrivate::fg_GetTebData<CWindowsCrossModuleThreadInfo *>(CWindowsThreadLocals::ms_ThreadLocalsExtendedLocactionOffset);
		if (pThreadLocals) [[likely]]
			return pThreadLocals;

		NLocal::g_OptionalFunctions.m_fRtlAcquirePebLock();
		auto Cleanup = g_OnScopeExit / [&]
			{
				NLocal::g_OptionalFunctions.m_fRtlReleasePebLock();
			}
		;

		pThreadLocals = CWindowsCrossModuleProcessInfo::fs_CreateThreadInfo(NSys::fg_Thread_GetCurrentUID());

		CWindowsCrossModuleProcessInfo::ms_pThis->m_ThreadInfos.f_Insert(pThreadLocals);

#ifdef DMibWindowsUseArbitraryUserPointerForThreadLocals
		auto pTeb = fg_GetTEB();
		pTeb->Tib.ArbitraryUserPointer = pThreadLocals;
#else
		NSys::fg_Thread_SetLocalFast(CWindowsCrossModuleProcessInfo::ms_pThis->m_ThreadLocal, pThreadLocals);
#endif

		return pThreadLocals;
	}

	CWindowsCrossModuleThreadInfo *CWindowsCrossModuleProcessInfo::fs_CreateThreadInfo(umint _ThreadID)
	{
		void *pMemory = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(CWindowsCrossModuleThreadInfo));

		auto *pThreadInfo = new (pMemory) CWindowsCrossModuleThreadInfo();

		pThreadInfo->m_ThreadID = _ThreadID;

		return pThreadInfo;
	}

	void CWindowsCrossModuleProcessInfo::fs_DestroyThreadInfo()
	{
		NLocal::g_OptionalFunctions.m_fRtlAcquirePebLock();
		auto Cleanup = g_OnScopeExit / [&]
			{
				NLocal::g_OptionalFunctions.m_fRtlReleasePebLock();
			}
		;

		auto *pThreadLocals = NSys::NPrivate::fg_GetTebData<CWindowsCrossModuleThreadInfo *>(CWindowsThreadLocals::ms_ThreadLocalsExtendedLocactionOffset);
		if (!pThreadLocals)
			return;

		auto nDestoryed = ++pThreadLocals->m_nThreadDestroys;

		DMibFastCheck(nDestoryed <= CWindowsCrossModuleProcessInfo::ms_pThis->m_RefCount.f_Load());

		if (nDestoryed != CWindowsCrossModuleProcessInfo::ms_pThis->m_RefCount.f_Load())
			return;

#ifdef DMibWindowsUseArbitraryUserPointerForThreadLocals
		auto pTeb = fg_GetTEB();
		pTeb->Tib.ArbitraryUserPointer = nullptr;
#else
		NSys::fg_Thread_SetLocalFast(CWindowsCrossModuleProcessInfo::ms_pThis->m_ThreadLocal, nullptr);
#endif

		pThreadLocals->~CWindowsCrossModuleThreadInfo();

		HeapFree(GetProcessHeap(), 0, pThreadLocals);
	}

	uint32 CWindowsCrossModuleProcessInfo::fs_AllocThreadLocal()
	{
		NLocal::g_OptionalFunctions.m_fRtlAcquirePebLock();
		auto Cleanup = g_OnScopeExit / [&]
			{
				NLocal::g_OptionalFunctions.m_fRtlReleasePebLock();
			}
		;

		DMibFastCheck(CWindowsCrossModuleProcessInfo::ms_pThis);

		auto Index = CWindowsCrossModuleProcessInfo::ms_pThis->m_ThreadLocalFreeSlots.f_FindFreeBitAndSet();
		if (Index < 0)
		{
			SetLastError(ERROR_NO_MORE_ITEMS);
			return TLS_OUT_OF_INDEXES;
		}

		auto *pThreadLocals = fs_GetOrCreateThreadInfo();
		if (!pThreadLocals)
		{
			SetLastError(ERROR_NOT_ENOUGH_MEMORY);
			CWindowsCrossModuleProcessInfo::ms_pThis->m_ThreadLocalFreeSlots.f_SetBit<false>(Index);
			return TLS_OUT_OF_INDEXES;
		}

		pThreadLocals->m_ThreadLocals.m_ThreadLocals[Index] = nullptr;

		return Index;
	}

	void CWindowsCrossModuleProcessInfo::fs_FreeThreadLocal(uint32 _ThreadLocalID)
	{
		NLocal::g_OptionalFunctions.m_fRtlAcquirePebLock();
		auto Cleanup = g_OnScopeExit / [&]
			{
				NLocal::g_OptionalFunctions.m_fRtlReleasePebLock();
			}
		;

		DMibFastCheck(CWindowsCrossModuleProcessInfo::ms_pThis);
		DMibFastCheck(CWindowsCrossModuleProcessInfo::ms_pThis->m_ThreadLocalFreeSlots.f_GetBit(_ThreadLocalID));

		CWindowsCrossModuleProcessInfo::ms_pThis->m_ThreadLocalFreeSlots.f_SetBit<false>(_ThreadLocalID);
	}

	bool CWindowsCrossModuleProcessInfo::fs_SetThreadLocal(uint32 _ThreadLocalID, void *_pValue)
	{
		DMibFastCheck(CWindowsCrossModuleProcessInfo::ms_pThis->m_ThreadLocalFreeSlots.f_GetBit(_ThreadLocalID));
		auto *pThreadLocals = fs_GetOrCreateThreadInfo();
		if (!pThreadLocals)
		{
			SetLastError(ERROR_NOT_ENOUGH_MEMORY);
			return false;
		}
		pThreadLocals->m_ThreadLocals.m_ThreadLocals[_ThreadLocalID] = _pValue;

		return true;
	}
}
