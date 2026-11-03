// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_OnScopeExit.h"
#include "Malterlib_Core_OnScopeExitCatch.h"
#include "Malterlib_Core_OnScopeExitShared.h"

namespace NMib
{
	constexpr COnScopeExitHelper gc_OnScopeExitInit{};
	COnScopeExitHelper const &g_OnScopeExit = gc_OnScopeExitInit;

	constexpr COnScopeExitHelperWithException gc_OnScopeExitWithExceptionInit{};
	COnScopeExitHelperWithException const &g_OnScopeExitWithException = gc_OnScopeExitWithExceptionInit;

	constexpr COnScopeExitCatchHelper gc_OnScopeExitCatchInit{};
	COnScopeExitCatchHelper const &g_OnScopeExitCatch = gc_OnScopeExitCatchInit;

	constexpr COnScopeExitSharedHelper gc_OnScopeExitSharedInit{};
	COnScopeExitSharedHelper const &g_OnScopeExitShared = gc_OnScopeExitSharedInit;

	constexpr COnScopeExitSharedWithExceptionHelper gc_OnScopeExitSharedWithExceptionInit{};
	COnScopeExitSharedWithExceptionHelper const &g_OnScopeExitSharedWithException = gc_OnScopeExitSharedWithExceptionInit;
}

