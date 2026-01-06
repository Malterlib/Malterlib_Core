// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_OnScopeExit.h"
#include "Malterlib_Core_OnScopeExitCatch.h"
#include "Malterlib_Core_OnScopeExitShared.h"

namespace NMib
{
	constexpr COnScopeExitHelper g_OnScopeExitInit{};
	COnScopeExitHelper const &g_OnScopeExit = g_OnScopeExitInit;

	constexpr COnScopeExitHelperWithException g_OnScopeExitWithExceptionInit{};
	COnScopeExitHelperWithException const &g_OnScopeExitWithException = g_OnScopeExitWithExceptionInit;

	constexpr COnScopeExitCatchHelper g_OnScopeExitCatchInit{};
	COnScopeExitCatchHelper const &g_OnScopeExitCatch = g_OnScopeExitCatchInit;

	constexpr COnScopeExitSharedHelper g_OnScopeExitSharedInit{};
	COnScopeExitSharedHelper const &g_OnScopeExitShared = g_OnScopeExitSharedInit;

	constexpr COnScopeExitSharedWithExceptionHelper g_OnScopeExitSharedWithExceptionInit{};
	COnScopeExitSharedWithExceptionHelper const &g_OnScopeExitSharedWithException = g_OnScopeExitSharedWithExceptionInit;
}

