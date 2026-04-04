// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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
		umint m_ThreadID = 0;
		NAtomic::TCAtomic<umint> m_nThreadDestroys;
		DMibListLinkDS_Link(CWindowsCrossModuleThreadInfo, m_Link);
		umint m_Reserved[123] = {0};
	};

	static_assert(sizeof(CWindowsCrossModuleThreadInfo) == sizeof(void *) * 384);

	struct CWindowsCrossModuleProcessInfo final
	{
		umint m_Version = 0;
		NAtomic::TCAtomic<umint> m_RefCount;
		umint m_ThreadLocal = TCLimitsInt<umint>::mc_Max;
		NContainer::TCBitArray<CWindowsThreadLocals::mc_ThreadLocalSlots> m_ThreadLocalFreeSlots;
		DMibListLinkDS_List(CWindowsCrossModuleThreadInfo, m_Link) m_ThreadInfos;

		umint m_Reserved[124 - CWindowsThreadLocals::mc_ThreadLocalSlots / (sizeof(void *) * 8)] = {0};

		CWindowsCrossModuleProcessInfo();
		~CWindowsCrossModuleProcessInfo();

		static CWindowsCrossModuleProcessInfo *ms_pThis;

		static uint32 fs_AllocThreadLocal();
		static void fs_FreeThreadLocal(uint32 _ThreadLocalID);
		static bool fs_SetThreadLocal(uint32 _ThreadLocalID, void *_pValue);

		static CWindowsCrossModuleThreadInfo *fs_GetOrCreateThreadInfo();
		static CWindowsCrossModuleThreadInfo *fs_CreateThreadInfo(umint _ThreadID);
		static void fs_DestroyThreadInfo();
	};

	static_assert(sizeof(CWindowsCrossModuleProcessInfo) == sizeof(void *) * 128);

	void fg_Windows_InitCrossModule();
	void fg_Windows_DestroyCrossModule();

	CWindowsCrossModuleThreadInfo *fg_Windows_GetThreadInfo();
}
