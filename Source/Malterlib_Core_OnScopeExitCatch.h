// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Core_General.h"
#include <Mib/Function/Function>

namespace NMib
{
	template <typename tf_FOnExitFunctor>
	TCOnScopeExit<NFunction::TCFunction<void ()>> fg_OnScopeExitCatch(tf_FOnExitFunctor &&_fOnExitFunctor);

	struct COnScopeExitCatchHelper
	{
		template <typename tf_FOnExitFunctor>
		TCOnScopeExit<NFunction::TCFunction<void ()>> operator / (tf_FOnExitFunctor &&_fOnExitFunctor) const;
	};

	extern COnScopeExitCatchHelper const &g_OnScopeExitCatch;
}

#include "Malterlib_Core_OnScopeExitCatch.hpp"
