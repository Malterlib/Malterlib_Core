// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Core_General.h"
#include <Mib/Function/Function>

namespace NMib
{
	typedef NPtr::TCSharedPointer<TCOnScopeExit<NFunction::TCFunction<void ()>>> COnScopeExitShared;

	template <typename tf_FOnExitFunctor>
	NPtr::TCSharedPointer<TCOnScopeExit<NFunction::TCFunction<void ()>>> fg_OnScopeExitShared(tf_FOnExitFunctor &&_fOnExitFunctor) 
	{ 
		return fg_Construct<TCOnScopeExit<NFunction::TCFunction<void ()>>>(fg_Forward<tf_FOnExitFunctor>(_fOnExitFunctor)); 
	}

}

