// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Core_General.h"
#include <Mib/Function/Function>

namespace NMib
{
	using COnScopeExitShared = NStorage::TCSharedPointer<TCOnScopeExit<NFunction::TCFunctionMovable<void ()>>>;
	using COnScopeExitSharedWithException = NStorage::TCSharedPointer<TCOnScopeExit<NFunction::TCFunctionMovable<void ()>, false>>;

	inline_always COnScopeExitShared fg_OnScopeExitShared(NFunction::TCFunctionMovable<void ()> &&_fOnExitFunctor) 
	{ 
		return fg_Construct<TCOnScopeExit<NFunction::TCFunctionMovable<void ()>>>(fg_Move(_fOnExitFunctor)); 
	}

	struct COnScopeExitSharedHelper
	{
		template <typename tf_FOnScopeExit>
		COnScopeExitShared operator / (tf_FOnScopeExit &&_fOnExitFunctor) const
		{
			return fg_Construct<TCOnScopeExit<NFunction::TCFunctionMovable<void ()>>>(fg_Move(_fOnExitFunctor));
		}
	};
	extern COnScopeExitSharedHelper const &g_OnScopeExitShared;

	struct COnScopeExitSharedWithExceptionHelper
	{
		template <typename tf_FOnScopeExit>
		COnScopeExitSharedWithException operator / (tf_FOnScopeExit &&_fOnExitFunctor) const
		{
			return fg_Construct<TCOnScopeExit<NFunction::TCFunctionMovable<void ()>>, false>(fg_Move(_fOnExitFunctor));
		}
	};
	extern COnScopeExitSharedWithExceptionHelper const &g_OnScopeExitSharedWithException;
}
