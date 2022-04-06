#pragma once

#include "Malterlib_Core_General.h"
#include <Mib/Function/Function>

namespace NMib
{
	template<typename tf_FOnExitFunctor>
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

	struct COnScopeExitCatchHelper
	{
		template<typename tf_FOnExitFunctor>
		TCOnScopeExit<NFunction::TCFunction<void ()>> operator / (tf_FOnExitFunctor &&_fOnExitFunctor) const
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
	};

	extern COnScopeExitCatchHelper const &g_OnScopeExitCatch;
}

