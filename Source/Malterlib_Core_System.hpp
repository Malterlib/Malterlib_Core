// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib
{
	static inline_small CSystem *fg_GetSys()
	{
#ifdef DMibPAutomaticSystemCreation
		DMibFastCheck(g_bCreatingSystemDone);
		return (CSystem *)g_SystemMemory;
#else

		if (!g_pSys)
			NSys::fg_CreateSystem();

		return g_pSys;
#endif
	}

	static inline_small CSystemModule *fg_GetModule()
	{
		return &g_SystemModule;
	}
}
