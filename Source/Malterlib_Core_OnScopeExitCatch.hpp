// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib
{
	template <typename tf_FOnExitFunctor>
	TCOnScopeExit<NFunction::TCFunction<void ()>> fg_OnScopeExitCatch(tf_FOnExitFunctor &&_fOnExitFunctor)
	{
		return TCOnScopeExit<NFunction::TCFunction<void ()>>
			(
				[_fOnExitFunctor]()
				{
					try
					{
						_fOnExitFunctor();
					}
					catch (NException::CException const &)
					{
					}
				}
			)
		;
	}

	template <typename tf_FOnExitFunctor>
	TCOnScopeExit<NFunction::TCFunction<void ()>> COnScopeExitCatchHelper::operator / (tf_FOnExitFunctor &&_fOnExitFunctor) const
	{
		return TCOnScopeExit<NFunction::TCFunction<void ()>>
			(
				[_fOnExitFunctor]()
				{
					try
					{
						_fOnExitFunctor();
					}
					catch (NException::CException const &)
					{
					}
				}
			)
		;
	}
}
