// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib
{
	template <typename tf_CType>
	constexpr tf_CType fg_CheckOrdering(tf_CType _Order)
	{
		static_assert(cIsOrderType<tf_CType>);
		return _Order;
	}

	template <typename t_CType0, typename t_CType1, typename t_CPrio>
	constexpr inline_small t_CType0 TCConvert<t_CType0, t_CType1, t_CPrio>::fs_Convert(t_CType1 const &_From)
	{
		return t_CType0(_From);
	}

	namespace NConvertPrivate
	{
		template <typename tf_CType0, typename tf_CType1>
		constexpr inline_small TCEnableIf<cIsDefault<tf_CType0, tf_CType1, NMib::NTraits::TCCompileTimeConstant<int, 2>>, tf_CType0> fg_ConvertPrivate(tf_CType1 const &_From)
		{
			return TCConvert<tf_CType0, tf_CType1, NMib::NTraits::TCCompileTimeConstant<int, 2>>::fs_Convert(_From);
		}

		template <typename tf_CType0, typename tf_CType1>
		constexpr inline_small auto fg_ConvertPrivate(tf_CType1 const &_From)
			-> TCEnableIf
			<
				cIsDefault<tf_CType0, tf_CType1, NMib::NTraits::TCCompileTimeConstant<int, 1>> && !cIsDefault<tf_CType0, tf_CType1, NMib::NTraits::TCCompileTimeConstant<int, 2>>
				, tf_CType0
			>
		{
			return TCConvert<tf_CType0, tf_CType1, NMib::NTraits::TCCompileTimeConstant<int, 1>>::fs_Convert(_From);
		}

		template <typename tf_CType0, typename tf_CType1>
		constexpr inline_small auto fg_ConvertPrivate(tf_CType1 const &_From)
			-> TCEnableIf
			<
				!cIsDefault<tf_CType0, tf_CType1, NMib::NTraits::TCCompileTimeConstant<int, 1>> && !cIsDefault<tf_CType0, tf_CType1, NMib::NTraits::TCCompileTimeConstant<int, 2>>
				, tf_CType0
			>
		{
			return TCConvert<tf_CType0, tf_CType1, NMib::NTraits::TCCompileTimeConstant<int, 0>>::fs_Convert(_From);
		}
	}

	template <typename tf_CType0, typename tf_CType1>
	constexpr inline_small TCDisableIf<NTraits::cIsSame<tf_CType0, tf_CType1>, tf_CType0> fg_Convert(tf_CType1 const &_From)
	{
		return NConvertPrivate::fg_ConvertPrivate<tf_CType0, tf_CType1>(_From);
	}

	template <typename tf_CType0, typename tf_CType1>
	constexpr inline_small TCEnableIf<NTraits::cIsSame<tf_CType0, tf_CType1>, tf_CType0> fg_Convert(tf_CType1 const &_From)
	{
		return _From;
	}
}
