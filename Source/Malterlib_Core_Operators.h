// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Type/Traits>
#include <Mib/Core/EnableIf>

#ifdef DCompiler_MSVC_Workaround
#include <compare>
#endif

namespace NMibOperators
{
	template<typename t_CType>
	class TCDisableAutomaticOperators : public NMib::NTraits::TCCompileTimeConstant<bool, false>
	{
	};

#ifdef DCompiler_MSVC_Workaround
	template <>
	class TCDisableAutomaticOperators<std::partial_ordering> : public NMib::NTraits::TCCompileTimeConstant<bool, true>
	{
	};

	template <>
	class TCDisableAutomaticOperators<std::weak_ordering> : public NMib::NTraits::TCCompileTimeConstant<bool, true>
	{
	};

	template <>
	class TCDisableAutomaticOperators<std::strong_ordering> : public NMib::NTraits::TCCompileTimeConstant<bool, true>
	{
	};
#endif

	template <typename t_CLeft, typename t_CRight>
	constexpr inline_small typename NMib::TCDisableIf
		<
			//NMib::NTraits::TCIsComparableLessThanEqual<t_CLeft, t_CRight>::mc_Value
			//|| !NMib::NTraits::TCIsComparableLessThan<t_CLeft, t_CRight>::mc_Value
			NMib::NTraits::TCIsEnum<t_CLeft>::mc_Value
			|| NMib::NTraits::TCIsEnum<t_CRight>::mc_Value
			|| (NMib::NTraits::TCIsPointer<t_CLeft>::mc_Value && (!NMib::NTraits::TCIsString<t_CLeft>::mc_Value))
			|| (NMib::NTraits::TCIsPointer<t_CRight>::mc_Value && (!NMib::NTraits::TCIsString<t_CRight>::mc_Value))
			|| TCDisableAutomaticOperators<t_CLeft>::mc_Value
			|| TCDisableAutomaticOperators<t_CRight>::mc_Value
			, bool
		>::CType
	operator <= (const t_CLeft &_Left, const t_CRight &_Right)
	{
		return !(_Right < _Left);
	}

	template <typename t_CLeft, typename t_CRight>
	constexpr inline_small typename NMib::TCDisableIf
		<
			//NMib::NTraits::TCIsComparableGreaterThan<t_CLeft, t_CRight>::mc_Value
			//|| !NMib::NTraits::TCIsComparableLessThan<t_CLeft, t_CRight>::mc_Value
			NMib::NTraits::TCIsEnum<t_CLeft>::mc_Value
			|| NMib::NTraits::TCIsEnum<t_CRight>::mc_Value
			|| (NMib::NTraits::TCIsPointer<t_CLeft>::mc_Value && (!NMib::NTraits::TCIsString<t_CLeft>::mc_Value))
			|| (NMib::NTraits::TCIsPointer<t_CRight>::mc_Value && (!NMib::NTraits::TCIsString<t_CRight>::mc_Value))
			|| TCDisableAutomaticOperators<t_CLeft>::mc_Value
			|| TCDisableAutomaticOperators<t_CRight>::mc_Value
			, bool
		>::CType
	operator > (const t_CLeft &_Left, const t_CRight &_Right)
	{
		return _Right < _Left;
	}

	template <typename t_CLeft, typename t_CRight>
	constexpr inline_small typename NMib::TCDisableIf
		<
			//NMib::NTraits::TCIsComparableGreaterThanEqual<t_CLeft, t_CRight>::mc_Value
			//!NMib::NTraits::TCIsComparableLessThan<t_CLeft, t_CRight>::mc_Value
			NMib::NTraits::TCIsEnum<t_CLeft>::mc_Value
			|| NMib::NTraits::TCIsEnum<t_CRight>::mc_Value
			|| (NMib::NTraits::TCIsPointer<t_CLeft>::mc_Value && (!NMib::NTraits::TCIsString<t_CLeft>::mc_Value))
			|| (NMib::NTraits::TCIsPointer<t_CRight>::mc_Value && (!NMib::NTraits::TCIsString<t_CRight>::mc_Value))
			|| TCDisableAutomaticOperators<t_CLeft>::mc_Value
			|| TCDisableAutomaticOperators<t_CRight>::mc_Value
			, bool
		>::CType operator >= (const t_CLeft &_Left, const t_CRight &_Right)
	{
		return !(_Left < _Right);
	}

	template <typename t_CLeft, typename t_CRight>
	constexpr inline_small auto operator != (const t_CLeft &_Left, const t_CRight &_Right)
		-> typename NMib::TCDisableIf
		<
			//NMib::NTraits::TCIsComparableNotEqual<t_CLeft, t_CRight>::mc_Value
			//|| !NMib::NTraits::TCIsComparableEqual<t_CLeft, t_CRight>::mc_Value
			NMib::NTraits::TCIsEnum<t_CLeft>::mc_Value
			|| NMib::NTraits::TCIsEnum<t_CRight>::mc_Value
			|| (NMib::NTraits::TCIsPointer<t_CLeft>::mc_Value && (!NMib::NTraits::TCIsString<t_CLeft>::mc_Value))
			|| (NMib::NTraits::TCIsPointer<t_CRight>::mc_Value && (!NMib::NTraits::TCIsString<t_CRight>::mc_Value))
			|| TCDisableAutomaticOperators<t_CLeft>::mc_Value
			|| TCDisableAutomaticOperators<t_CRight>::mc_Value
			, bool
		>::CType
	{
		return !(_Left == _Right);
	}
}
using namespace NMibOperators;

