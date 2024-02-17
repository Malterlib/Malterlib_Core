// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once


#include <Mib/Type/Traits>
#include <Mib/Core/EnableIf>

template <typename t_CEnum>
DMibSuppressUndefinedSanitizer constexpr inline_small t_CEnum operator | ( t_CEnum _Left, t_CEnum _Right)
	requires (NMib::NTraits::TCIsEnum<t_CEnum>::mc_Value)
{
	using CUnderlyingType = NMib::NTraits::TCEnumUnderlyingType<t_CEnum>;
	return static_cast<t_CEnum>(static_cast<CUnderlyingType>(_Left) | static_cast<CUnderlyingType>(_Right));
}

template <typename t_CEnum>
DMibSuppressUndefinedSanitizer inline_small t_CEnum &operator |= (t_CEnum &_Left, t_CEnum _Right)
	requires (NMib::NTraits::TCIsEnum<t_CEnum>::mc_Value)
{
	using CUnderlyingType = NMib::NTraits::TCEnumUnderlyingType<t_CEnum>;
	_Left = static_cast<t_CEnum>(static_cast<CUnderlyingType>(_Left) | static_cast<CUnderlyingType>(_Right));
	return _Left;
}

template <typename t_CEnum>
DMibSuppressUndefinedSanitizer constexpr inline_small t_CEnum operator & (t_CEnum _Left, t_CEnum _Right)
	requires (NMib::NTraits::TCIsEnum<t_CEnum>::mc_Value)
{
	using CUnderlyingType = NMib::NTraits::TCEnumUnderlyingType<t_CEnum>;
	return static_cast<t_CEnum>(static_cast<CUnderlyingType>(_Left) & static_cast<CUnderlyingType>(_Right));
}

template <typename t_CEnum>
DMibSuppressUndefinedSanitizer inline_small t_CEnum &operator &= (t_CEnum &_Left, t_CEnum _Right)
	requires (NMib::NTraits::TCIsEnum<t_CEnum>::mc_Value)
{
	using CUnderlyingType = NMib::NTraits::TCEnumUnderlyingType<t_CEnum>;
	_Left = static_cast<t_CEnum>((static_cast<CUnderlyingType>(_Left) & static_cast<CUnderlyingType>(_Right)));
	return _Left;
}

template <typename t_CEnum>
DMibSuppressUndefinedSanitizer constexpr inline_small t_CEnum operator ^ (t_CEnum _Left, t_CEnum _Right)
	requires (NMib::NTraits::TCIsEnum<t_CEnum>::mc_Value)
{
	using CUnderlyingType = NMib::NTraits::TCEnumUnderlyingType<t_CEnum>;
	return static_cast<t_CEnum>(static_cast<CUnderlyingType>(_Left) ^ static_cast<CUnderlyingType>(_Right));
}

template <typename t_CEnum>
DMibSuppressUndefinedSanitizer inline_small t_CEnum &operator ^= (t_CEnum &_Left, t_CEnum _Right)
	requires (NMib::NTraits::TCIsEnum<t_CEnum>::mc_Value)
{
	using CUnderlyingType = NMib::NTraits::TCEnumUnderlyingType<t_CEnum>;
	_Left = static_cast<t_CEnum>(static_cast<CUnderlyingType>(_Left) ^ static_cast<CUnderlyingType>(_Right));
	return _Left;
}

template <typename t_CEnum>
DMibSuppressUndefinedSanitizer constexpr inline_small t_CEnum operator ~ (t_CEnum _Left)
	requires (NMib::NTraits::TCIsEnum<t_CEnum>::mc_Value)
{
	using CUnderlyingType = NMib::NTraits::TCEnumUnderlyingType<t_CEnum>;
	return static_cast<t_CEnum>(~static_cast<CUnderlyingType>(_Left));
}

template <typename t_CEnum, typename t_CShift>
DMibSuppressUndefinedSanitizer constexpr inline_small t_CEnum operator << (t_CEnum _Left, t_CShift _nPlaces)
	requires (NMib::NTraits::TCIsEnum<t_CEnum>::mc_Value)
{
	using CUnderlyingType = NMib::NTraits::TCEnumUnderlyingType<t_CEnum>;
	return static_cast<t_CEnum>(static_cast<CUnderlyingType>(_Left) << _nPlaces);
}

template <typename t_CEnum, typename t_CShift>
DMibSuppressUndefinedSanitizer constexpr inline_small t_CEnum operator >> (t_CEnum _Left, t_CShift _nPlaces)
	requires (NMib::NTraits::TCIsEnum<t_CEnum>::mc_Value)
{
	using CUnderlyingType = NMib::NTraits::TCEnumUnderlyingType<t_CEnum>;
	return static_cast<t_CEnum>(static_cast<CUnderlyingType>(_Left) >> _nPlaces);
}

template <typename t_CEnum, typename t_CShift>
DMibSuppressUndefinedSanitizer inline_small t_CEnum &operator <<= (t_CEnum &_Left, t_CShift _nPlaces)
	requires (NMib::NTraits::TCIsEnum<t_CEnum>::mc_Value)
{
	using CUnderlyingType = NMib::NTraits::TCEnumUnderlyingType<t_CEnum>;
	_Left = static_cast<t_CEnum>(static_cast<CUnderlyingType>(_Left) << _nPlaces);
	return _Left;
}

template <typename t_CEnum, typename t_CShift>
DMibSuppressUndefinedSanitizer inline_small t_CEnum &operator >>= (t_CEnum &_Left, t_CShift _nPlaces)
	requires (NMib::NTraits::TCIsEnum<t_CEnum>::mc_Value)
{
	using CUnderlyingType = NMib::NTraits::TCEnumUnderlyingType<t_CEnum>;
	_Left = static_cast<t_CEnum>(static_cast<CUnderlyingType>(_Left) >> _nPlaces);
	return _Left;
}

// TODO: Make it an error to compare enum and non emum as well

//#ifndef DCompiler_clang

// This happens all the time in constant expressions that compare different values. Maybe make OK for anonymous enums somehow

template <typename t_CEnum0, typename t_CEnum1>
constexpr inline_small bool operator == (t_CEnum0 const &_Left, t_CEnum1 const &_Right)
	requires (NMib::NTraits::TCIsEnum<t_CEnum0>::mc_Value && NMib::NTraits::TCIsEnum<t_CEnum1>::mc_Value)
{
	static_assert(NMib::NTraits::TCIsSame<t_CEnum0, t_CEnum1>::mc_Value, "Comparison of two different enum types is unsafe");
	return _Left == _Right;
}

template <typename t_CEnum0, typename t_CEnum1>
constexpr inline_small bool operator != (t_CEnum0 const &_Left, t_CEnum1 const &_Right)
	requires (NMib::NTraits::TCIsEnum<t_CEnum0>::mc_Value && NMib::NTraits::TCIsEnum<t_CEnum1>::mc_Value)
{
	static_assert(NMib::NTraits::TCIsSame<t_CEnum0, t_CEnum1>::mc_Value, "Comparison of two different enum types is unsafe");
	return _Left != _Right;
}

template <typename t_CEnum0, typename t_CEnum1>
constexpr inline_small bool operator < (t_CEnum0 const &_Left, t_CEnum1 const &_Right)
	requires (NMib::NTraits::TCIsEnum<t_CEnum0>::mc_Value && NMib::NTraits::TCIsEnum<t_CEnum1>::mc_Value)
{
	static_assert(NMib::NTraits::TCIsSame<t_CEnum0, t_CEnum1>::mc_Value, "Comparison of two different enum types is unsafe");
	return _Left < _Right;
}

template <typename t_CEnum0, typename t_CEnum1>
constexpr inline_small bool operator > (t_CEnum0 const &_Left, t_CEnum1 const &_Right)
	requires (NMib::NTraits::TCIsEnum<t_CEnum0>::mc_Value && NMib::NTraits::TCIsEnum<t_CEnum1>::mc_Value)
{
	static_assert(NMib::NTraits::TCIsSame<t_CEnum0, t_CEnum1>::mc_Value, "Comparison of two different enum types is unsafe");
	return _Left > _Right;
}

template <typename t_CEnum0, typename t_CEnum1>
constexpr inline_small bool operator <= (t_CEnum0 const &_Left, t_CEnum1 const &_Right)
	requires (NMib::NTraits::TCIsEnum<t_CEnum0>::mc_Value && NMib::NTraits::TCIsEnum<t_CEnum1>::mc_Value)
{
	static_assert(NMib::NTraits::TCIsSame<t_CEnum0, t_CEnum1>::mc_Value, "Comparison of two different enum types is unsafe");
	return _Left <= _Right;
}

template <typename t_CEnum0, typename t_CEnum1>
constexpr inline_small bool operator >= (t_CEnum0 const &_Left, t_CEnum1 const &_Right)
	requires (NMib::NTraits::TCIsEnum<t_CEnum0>::mc_Value && NMib::NTraits::TCIsEnum<t_CEnum1>::mc_Value)
{
	static_assert(NMib::NTraits::TCIsSame<t_CEnum0, t_CEnum1>::mc_Value, "Comparison of two different enum types is unsafe");
	return _Left >= _Right;
}

//#endif

namespace NMib
{
	template <typename tf_CType>
	inline_always bool fg_IsSet(tf_CType _Value, tf_CType _Flags)
		requires (NTraits::cIsScopedEnum<tf_CType>)
	{
		return (_Value & _Flags) != tf_CType(0);
	}
}
