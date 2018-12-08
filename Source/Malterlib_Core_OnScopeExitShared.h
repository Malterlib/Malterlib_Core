// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Core_General.h"
#include <Mib/Function/Function>

namespace NMib
{
	typedef NStorage::TCSharedPointer<TCOnScopeExit<NFunction::TCFunctionMovable<void ()>>> COnScopeExitShared;

	inline_always COnScopeExitShared fg_OnScopeExitShared(NFunction::TCFunctionMovable<void ()> &&_fOnExitFunctor) 
	{ 
		return fg_Construct<TCOnScopeExit<NFunction::TCFunctionMovable<void ()>>>(fg_Move(_fOnExitFunctor)); 
	}

	struct COnScopeExitSharedHelper
	{
		template <typename tf_FOnScopeExit>
		COnScopeExitShared operator >(tf_FOnScopeExit &&_fOnExitFunctor) const 
		{ 
			return fg_Construct<TCOnScopeExit<NFunction::TCFunctionMovable<void ()>>>(fg_Move(_fOnExitFunctor)); 
		}
	};
	extern COnScopeExitSharedHelper const &g_OnScopeExitShared;
}
