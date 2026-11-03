// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Type/Traits>
#include <Mib/Core/EnableIf>

template <typename tf_CEnum>
DMibSuppressUndefinedSanitizer constexpr inline_small tf_CEnum operator | (tf_CEnum _Left, tf_CEnum _Right) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum>)
;

template <typename tf_CEnum>
DMibSuppressUndefinedSanitizer inline_small tf_CEnum &operator |= (tf_CEnum &_Left, tf_CEnum _Right) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum>)
;

template <typename tf_CEnum>
DMibSuppressUndefinedSanitizer constexpr inline_small tf_CEnum operator & (tf_CEnum _Left, tf_CEnum _Right) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum>)
;

template <typename tf_CEnum>
DMibSuppressUndefinedSanitizer inline_small tf_CEnum &operator &= (tf_CEnum &_Left, tf_CEnum _Right)
	requires (NMib::NTraits::cIsEnum<tf_CEnum>)
;

template <typename tf_CEnum>
DMibSuppressUndefinedSanitizer constexpr inline_small tf_CEnum operator ^ (tf_CEnum _Left, tf_CEnum _Right) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum>)
;

template <typename tf_CEnum>
DMibSuppressUndefinedSanitizer inline_small tf_CEnum &operator ^= (tf_CEnum &_Left, tf_CEnum _Right) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum>)
;

template <typename tf_CEnum>
DMibSuppressUndefinedSanitizer constexpr inline_small tf_CEnum operator ~ (tf_CEnum _Left) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum>)
;

template <typename tf_CEnum, typename tf_CShift>
DMibSuppressUndefinedSanitizer constexpr inline_small tf_CEnum operator << (tf_CEnum _Left, tf_CShift _nPlaces) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum>)
;

template <typename tf_CEnum, typename tf_CShift>
DMibSuppressUndefinedSanitizer constexpr inline_small tf_CEnum operator >> (tf_CEnum _Left, tf_CShift _nPlaces) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum>)
;

template <typename tf_CEnum, typename tf_CShift>
DMibSuppressUndefinedSanitizer inline_small tf_CEnum &operator <<= (tf_CEnum &_Left, tf_CShift _nPlaces) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum>)
;

template <typename tf_CEnum, typename tf_CShift>
DMibSuppressUndefinedSanitizer inline_small tf_CEnum &operator >>= (tf_CEnum &_Left, tf_CShift _nPlaces) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum>)
;

// TODO: Make it an error to compare enum and non emum as well

//#ifndef DCompiler_clang

// This happens all the time in constant expressions that compare different values. Maybe make OK for anonymous enums somehow

template <typename tf_CEnum0, typename tf_CEnum1>
constexpr inline_small bool operator == (tf_CEnum0 const &_Left, tf_CEnum1 const &_Right) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum0> && NMib::NTraits::cIsEnum<tf_CEnum1>)
;

template <typename tf_CEnum0, typename tf_CEnum1>
constexpr inline_small bool operator != (tf_CEnum0 const &_Left, tf_CEnum1 const &_Right) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum0> && NMib::NTraits::cIsEnum<tf_CEnum1>)
;

template <typename tf_CEnum0, typename tf_CEnum1>
constexpr inline_small bool operator < (tf_CEnum0 const &_Left, tf_CEnum1 const &_Right) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum0> && NMib::NTraits::cIsEnum<tf_CEnum1>)
;

template <typename tf_CEnum0, typename tf_CEnum1>
constexpr inline_small bool operator > (tf_CEnum0 const &_Left, tf_CEnum1 const &_Right) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum0> && NMib::NTraits::cIsEnum<tf_CEnum1>)
;

template <typename tf_CEnum0, typename tf_CEnum1>
constexpr inline_small bool operator <= (tf_CEnum0 const &_Left, tf_CEnum1 const &_Right) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum0> && NMib::NTraits::cIsEnum<tf_CEnum1>)
;

template <typename tf_CEnum0, typename tf_CEnum1>
constexpr inline_small bool operator >= (tf_CEnum0 const &_Left, tf_CEnum1 const &_Right) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum0> && NMib::NTraits::cIsEnum<tf_CEnum1>)
;

//#endif

namespace NMib
{
	template <typename tf_CType>
	constexpr inline_always bool fg_IsSet(tf_CType _Value, tf_CType _Flags) noexcept
		requires (NTraits::cIsScopedEnum<tf_CType>)
	;
}

#include "Malterlib_Core_EnumOperators.hpp"
