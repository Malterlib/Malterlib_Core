// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib
{
	template <typename t_CReturn>
	template <typename ...tfp_CParam>
	t_CReturn TCVoidFunctor<t_CReturn>::operator () (tfp_CParam && ...p_Params) const volatile
	{
		return t_CReturn();
	}

	template <typename ...tfp_CParam>
	void TCVoidFunctor<void>::operator () (tfp_CParam && ...p_Params) const volatile
	{
	}

	template <typename tf_CStr>
	void CVoidTag::f_Format(tf_CStr &o_Str) const
	{
		o_Str += "void";
	}
}
