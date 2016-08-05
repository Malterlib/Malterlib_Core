// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	template <bool t_bEnable, typename t_CType = void>
	struct TCEnableIf 
	{
		typedef t_CType CType;
	};

	template <typename t_CType>
	struct TCEnableIf<false, t_CType>
	{
	};

	template <bool t_bEnable, typename t_CType = void>
	using TCEnableIfType = typename TCEnableIf<t_bEnable, t_CType>::CType;

	template <bool t_bDisable, class t_CType = void>
	struct TCDisableIf 
	{
		typedef t_CType CType;
	};

	template <class t_CType>
	struct TCDisableIf<true, t_CType> 
	{
	};

	template <bool t_bFirstType, typename t_CType0, typename t_CType1>
	struct TCChooseType
	{
		typedef t_CType0 CType;
	};

	template <typename t_CType0, typename t_CType1>
	struct TCChooseType<false, t_CType0, t_CType1>
	{
		typedef t_CType1 CType;
	};

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
}
