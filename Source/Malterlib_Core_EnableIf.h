// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <type_traits>

namespace NMib
{
	template <bool t_bEnable, typename t_CType = void>
	using TCEnableIf = typename std::enable_if<t_bEnable, t_CType>::type;

	template <bool t_bDisable, typename t_CType = void>
	using TCDisableIf = typename std::enable_if<!t_bDisable, t_CType>::type;

	template <bool t_bFirstType, typename t_CType0, typename t_CType1>
	struct TCConditionalHelper
	{
		using CType = t_CType0;
	};

	template <typename t_CType0, typename t_CType1>
	struct TCConditionalHelper<false, t_CType0, t_CType1>
	{
		using CType = t_CType1;
	};

	template <bool t_bFirstType, typename t_CType0, typename t_CType1>
	using TCConditional = typename TCConditionalHelper<t_bFirstType, t_CType0, t_CType1>::CType;

	template <typename t_CInt, bool t_bFirstType, t_CInt t_Val0, t_CInt t_Val1>
	struct TCChooseInt
	{
		static constexpr t_CInt mc_Value = t_Val0;
	};

	template <typename t_CInt, t_CInt t_Val0, t_CInt t_Val1>
	struct TCChooseInt<t_CInt, false, t_Val0, t_Val1>
	{
		static constexpr t_CInt mc_Value = t_Val1;
	};

	template <bool t_bFirstType, int t_Val0, int t_Val1>
	struct TCChooseInt<int, t_bFirstType, t_Val0, t_Val1>
	{
		enum
		{
			mc_Value = t_Val0
		};
	};

	template <int t_Val0, int t_Val1>
	struct TCChooseInt<int, false, t_Val0, t_Val1>
	{
		enum
		{
			mc_Value = t_Val1
		};
	};

	template <bool t_bFirstType, bool t_Val0, bool t_Val1>
	struct TCChooseInt<bool, t_bFirstType, t_Val0, t_Val1>
	{
		enum
		{
			mc_Value = t_Val0
		};
	};

	template <int t_Val0, int t_Val1>
	struct TCChooseInt<bool, false, t_Val0, t_Val1>
	{
		enum
		{
			mc_Value = t_Val1
		};
	};

	template <auto t_fFunction>
	struct TCInstantiateValue
	{
		static constexpr bool mc_Value = t_fFunction != nullptr;
	};
}
