// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib
{
	inline_always COnScopeExitShared fg_OnScopeExitShared(NFunction::TCFunctionMovable<void ()> &&_fOnExitFunctor)
	{
		return fg_Construct<TCOnScopeExit<NFunction::TCFunctionMovable<void ()>>>(fg_Move(_fOnExitFunctor));
	}

	template <typename tf_FOnScopeExit>
	COnScopeExitShared COnScopeExitSharedHelper::operator / (tf_FOnScopeExit &&_fOnExitFunctor) const
	{
		return fg_Construct<TCOnScopeExit<NFunction::TCFunctionMovable<void ()>>>(fg_Move(_fOnExitFunctor));
	}

	template <typename tf_FOnScopeExit>
	COnScopeExitSharedWithException COnScopeExitSharedWithExceptionHelper::operator / (tf_FOnScopeExit &&_fOnExitFunctor) const
	{
		return fg_Construct<TCOnScopeExit<NFunction::TCFunctionMovable<void ()>>, false>(fg_Move(_fOnExitFunctor));
	}
}
