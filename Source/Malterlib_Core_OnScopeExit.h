// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Core_General.h"

namespace NMib
{
	// Utility to call perform an action at the end of scope:
	// {
	//   auto handle = OpenThingie();
	//   auto cleanup = fg_OnScopeExit( [&]{CloseThingie(handle);})
	//   ... Do stuff
	// } // CloseThinie called here

	template <typename t_FOnExitFunctor, bool t_bNoExcept = true>
	struct [[nodiscard]] TCOnScopeExit
	{
		TCOnScopeExit(TCOnScopeExit const &_Other) = delete;
		TCOnScopeExit(t_FOnExitFunctor const &_fOnExitFunctor) = delete;

		inline_always TCOnScopeExit(t_FOnExitFunctor &&_fOnExitFunctor);
		inline_always TCOnScopeExit(TCOnScopeExit &&_Other);

		inline_always ~TCOnScopeExit() noexcept(t_bNoExcept);

		TCOnScopeExit operator = (TCOnScopeExit const &_Other) = delete;
		TCOnScopeExit &operator = (TCOnScopeExit &&_Other);

		template <typename ...tfp_CParam>
		void operator () (tfp_CParam && ...p_Params);

		inline_always void f_Clear();
		inline_always void f_ClearFunctor();
		inline_always bool f_IsValid() const;

	private:
		t_FOnExitFunctor mp_fOnExitFunctor;
		bool mp_bIsValid;
	};

	template <typename tf_FOnExitFunctor>
	inline_always TCOnScopeExit<NTraits::TCRemoveReferenceStorable<tf_FOnExitFunctor>> fg_OnScopeExit(tf_FOnExitFunctor &&_fOnExitFunctor);

	struct COnScopeExitHelper
	{
		template <typename tf_FOnExitFunctor>
		inline_always TCOnScopeExit<NTraits::TCRemoveReferenceStorable<tf_FOnExitFunctor>> operator / (tf_FOnExitFunctor &&_fOnExitFunctor) const;
	};

	extern COnScopeExitHelper const &g_OnScopeExit;

	struct COnScopeExitHelperWithException
	{
		template <typename tf_FOnExitFunctor>
		inline_always TCOnScopeExit<NTraits::TCRemoveReferenceStorable<tf_FOnExitFunctor>, false> operator / (tf_FOnExitFunctor &&_fOnExitFunctor) const;
	};

	extern COnScopeExitHelperWithException const &g_OnScopeExitWithException;
}

#include "Malterlib_Core_OnScopeExit.hpp"
