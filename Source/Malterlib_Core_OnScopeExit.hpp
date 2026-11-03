// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib
{
	template <typename t_FOnExitFunctor, bool t_bNoExcept>
	inline_always TCOnScopeExit<t_FOnExitFunctor, t_bNoExcept>::TCOnScopeExit(t_FOnExitFunctor &&_fOnExitFunctor)
		: mp_fOnExitFunctor(fg_Move(_fOnExitFunctor))
		, mp_bIsValid(true)
	{
	}

	template <typename t_FOnExitFunctor, bool t_bNoExcept>
	inline_always TCOnScopeExit<t_FOnExitFunctor, t_bNoExcept>::TCOnScopeExit(TCOnScopeExit &&_Other)
		: mp_fOnExitFunctor(fg_Move(_Other.mp_fOnExitFunctor))
		, mp_bIsValid(_Other.mp_bIsValid)
	{
		_Other.mp_bIsValid = false;
	}

	template <typename t_FOnExitFunctor, bool t_bNoExcept>
	TCOnScopeExit<t_FOnExitFunctor, t_bNoExcept> &TCOnScopeExit<t_FOnExitFunctor, t_bNoExcept>::operator = (TCOnScopeExit &&_Other)
	{
		mp_fOnExitFunctor = fg_Move(_Other.mp_fOnExitFunctor);
		mp_bIsValid = _Other.mp_bIsValid;
		_Other.mp_bIsValid = false;
		return *this;
	}

	template <typename t_FOnExitFunctor, bool t_bNoExcept>
	inline_always TCOnScopeExit<t_FOnExitFunctor, t_bNoExcept>::~TCOnScopeExit() noexcept(t_bNoExcept)
	{
		if (mp_bIsValid)
		{
			mp_bIsValid = false;
			mp_fOnExitFunctor();
		}
	}

	template <typename t_FOnExitFunctor, bool t_bNoExcept>
	template <typename ...tfp_CParam>
	void TCOnScopeExit<t_FOnExitFunctor, t_bNoExcept>::operator () (tfp_CParam && ...p_Params)
	{
		if (mp_bIsValid)
		{
			mp_bIsValid = false;
			mp_fOnExitFunctor(fg_Forward<tfp_CParam>(p_Params)...);
		}
	}

	template <typename t_FOnExitFunctor, bool t_bNoExcept>
	inline_always void TCOnScopeExit<t_FOnExitFunctor, t_bNoExcept>::f_Clear()
	{
		mp_bIsValid = false;
	}

	template <typename t_FOnExitFunctor, bool t_bNoExcept>
	inline_always void TCOnScopeExit<t_FOnExitFunctor, t_bNoExcept>::f_ClearFunctor()
	{
		mp_bIsValid = false;
		mp_fOnExitFunctor.f_Clear();
	}

	template <typename t_FOnExitFunctor, bool t_bNoExcept>
	inline_always bool TCOnScopeExit<t_FOnExitFunctor, t_bNoExcept>::f_IsValid() const
	{
		return mp_bIsValid;
	}

	template <typename tf_FOnExitFunctor>
	inline_always TCOnScopeExit<NTraits::TCRemoveReferenceStorable<tf_FOnExitFunctor>> fg_OnScopeExit(tf_FOnExitFunctor &&_fOnExitFunctor)
	{
		return TCOnScopeExit<NTraits::TCRemoveReferenceStorable<tf_FOnExitFunctor>>(fg_Forward<tf_FOnExitFunctor>(_fOnExitFunctor));
	}

	template <typename tf_FOnExitFunctor>
	inline_always TCOnScopeExit<NTraits::TCRemoveReferenceStorable<tf_FOnExitFunctor>> COnScopeExitHelper::operator / (tf_FOnExitFunctor &&_fOnExitFunctor) const
	{
		return TCOnScopeExit<NTraits::TCRemoveReferenceStorable<tf_FOnExitFunctor>>(fg_Forward<tf_FOnExitFunctor>(_fOnExitFunctor));
	}

	template <typename tf_FOnExitFunctor>
	inline_always TCOnScopeExit<NTraits::TCRemoveReferenceStorable<tf_FOnExitFunctor>, false> COnScopeExitHelperWithException::operator / (tf_FOnExitFunctor &&_fOnExitFunctor) const
	{
		return TCOnScopeExit<NTraits::TCRemoveReferenceStorable<tf_FOnExitFunctor>, false>(fg_Forward<tf_FOnExitFunctor>(_fOnExitFunctor));
	}
}
