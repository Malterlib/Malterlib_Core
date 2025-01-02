// Copyright © 2025 Unbroken AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Container/BitArray>

namespace NMib::NThread::NPlatform
{
	struct CWindowsCrossModuleProcessInfo;

	struct CWindowsCrossModuleThreadInfo final
	{
		CWindowsThreadLocals m_ThreadLocals;
		CWindowsCrossModuleProcessInfo *m_pProcessInfo;
		mint m_ThreadID = 0;
		NAtomic::TCAtomic<mint> m_nThreadDestroys;
		DMibListLinkDS_Link(CWindowsCrossModuleThreadInfo, m_Link);
		mint m_Reserved[123] = {0};
	};

	static_assert(sizeof(CWindowsCrossModuleThreadInfo) == sizeof(void *) * 384);

	struct CWindowsCrossModuleProcessInfo final
	{
		mint m_Version = 0;
		NAtomic::TCAtomic<mint> m_RefCount;
		mint m_ThreadLocal = TCLimitsInt<mint>::mc_Max;
		NContainer::TCBitArray<CWindowsThreadLocals::mc_ThreadLocalSlots> m_ThreadLocalFreeSlots;
		DMibListLinkDS_List(CWindowsCrossModuleThreadInfo, m_Link) m_ThreadInfos;

		mint m_Reserved[124 - CWindowsThreadLocals::mc_ThreadLocalSlots / (sizeof(void *) * 8)] = {0};

		CWindowsCrossModuleProcessInfo();
		~CWindowsCrossModuleProcessInfo();

		static CWindowsCrossModuleProcessInfo *ms_pThis;

		static uint32 fs_AllocThreadLocal();
		static void fs_FreeThreadLocal(uint32 _ThreadLocalID);
		static bool fs_SetThreadLocal(uint32 _ThreadLocalID, void *_pValue);

		static CWindowsCrossModuleThreadInfo *fs_GetOrCreateThreadInfo();
		static CWindowsCrossModuleThreadInfo *fs_CreateThreadInfo(mint _ThreadID);
		static void fs_DestroyThreadInfo();
	};

	static_assert(sizeof(CWindowsCrossModuleProcessInfo) == sizeof(void *) * 128);

	void fg_Windows_InitCrossModule();
	void fg_Windows_DestroyCrossModule();

	CWindowsCrossModuleThreadInfo *fg_Windows_GetThreadInfo();
}
