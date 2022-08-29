// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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
	class TCOnScopeExit
	{
		t_FOnExitFunctor mp_fOnExitFunctor;
		bool mp_bIsValid;
	public:
		TCOnScopeExit(TCOnScopeExit const&) = delete;
		TCOnScopeExit operator=(TCOnScopeExit const&) = delete;
		TCOnScopeExit(t_FOnExitFunctor const &_fOnExitFunctor) = delete;
		
		inline_always TCOnScopeExit(t_FOnExitFunctor &&_fOnExitFunctor)
			: mp_fOnExitFunctor(fg_Move(_fOnExitFunctor))
			, mp_bIsValid(true)
		{
		}
		inline_always TCOnScopeExit(TCOnScopeExit &&_Other)
			: mp_fOnExitFunctor(fg_Move(_Other.mp_fOnExitFunctor))
			, mp_bIsValid(_Other.mp_bIsValid)
		{
			_Other.mp_bIsValid = false;
		}
		TCOnScopeExit &operator = (TCOnScopeExit &&_Other) 
		{
			mp_fOnExitFunctor = fg_Move(_Other.mp_fOnExitFunctor);
			mp_bIsValid = _Other.mp_bIsValid;
			_Other.mp_bIsValid = false;
			return *this;
		}
		inline_always ~TCOnScopeExit() noexcept(t_bNoExcept)
		{ 
			if (mp_bIsValid)
			{
				mp_bIsValid = false;
				mp_fOnExitFunctor();
			}
		}

		template <typename ...tfp_CParam>
		void operator () (tfp_CParam &&...p_Params)
		{
			if (mp_bIsValid)
			{
				mp_bIsValid = false;
				mp_fOnExitFunctor(fg_Forward<tfp_CParam>(p_Params)...);
			}
		}
		inline_always void f_Clear()
		{
			mp_bIsValid = false;
		}
	};
	
	template<typename tf_FOnExitFunctor>
	inline_always TCOnScopeExit<typename NTraits::TCRemoveReferenceStorable<tf_FOnExitFunctor>::CType> fg_OnScopeExit(tf_FOnExitFunctor &&_fOnExitFunctor)
	{ 
		return TCOnScopeExit<typename NTraits::TCRemoveReferenceStorable<tf_FOnExitFunctor>::CType>(fg_Forward<tf_FOnExitFunctor>(_fOnExitFunctor)); 
	}
	
	struct COnScopeExitHelper
	{
		template<typename tf_FOnExitFunctor>
		inline_always TCOnScopeExit<typename NTraits::TCRemoveReferenceStorable<tf_FOnExitFunctor>::CType> operator / (tf_FOnExitFunctor &&_fOnExitFunctor) const
		{
			return TCOnScopeExit<typename NTraits::TCRemoveReferenceStorable<tf_FOnExitFunctor>::CType>(fg_Forward<tf_FOnExitFunctor>(_fOnExitFunctor));
		}
	};
	extern COnScopeExitHelper const &g_OnScopeExit;

	struct COnScopeExitHelperWithException
	{
		template<typename tf_FOnExitFunctor>
		inline_always TCOnScopeExit<typename NTraits::TCRemoveReferenceStorable<tf_FOnExitFunctor>::CType, false> operator / (tf_FOnExitFunctor &&_fOnExitFunctor) const
		{
			return TCOnScopeExit<typename NTraits::TCRemoveReferenceStorable<tf_FOnExitFunctor>::CType, false>(fg_Forward<tf_FOnExitFunctor>(_fOnExitFunctor));
		}
	};
	extern COnScopeExitHelperWithException const &g_OnScopeExitWithException;
}
