// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once


#include <Mib/Type/Traits>
#include <Mib/Core/EnableIf>

template <typename t_CEnum>
constexpr inline_small typename NMib::TCEnableIf<NMib::NTraits::TCIsEnum<t_CEnum>::mc_Value, t_CEnum>::CType operator | ( t_CEnum _Left, t_CEnum _Right)
{
	return (t_CEnum)(uint32(_Left) | uint32(_Right));
}

template <typename t_CEnum>
inline_small typename NMib::TCEnableIf<NMib::NTraits::TCIsEnum<t_CEnum>::mc_Value, t_CEnum>::CType &operator |= (t_CEnum &_Left, const t_CEnum &_Right)
{
	_Left = _Left | _Right;
	return _Left;
}

template <typename t_CEnum>
constexpr inline_small typename NMib::TCEnableIf<NMib::NTraits::TCIsEnum<t_CEnum>::mc_Value, t_CEnum>::CType operator & (t_CEnum _Left, t_CEnum _Right)
{
	return (t_CEnum)(uint32(_Left) & uint32(_Right));
}

template <typename t_CEnum>
inline_small typename NMib::TCEnableIf<NMib::NTraits::TCIsEnum<t_CEnum>::mc_Value, t_CEnum>::CType &operator &= (t_CEnum &_Left, const t_CEnum &_Right)
{
	_Left = _Left & _Right;
	return _Left;
}

template <typename t_CEnum>
constexpr inline_small typename NMib::TCEnableIf<NMib::NTraits::TCIsEnum<t_CEnum>::mc_Value, t_CEnum>::CType operator ^ (t_CEnum _Left, t_CEnum _Right)
{
	return (t_CEnum)(uint32(_Left) ^ uint32(_Right));
}

template <typename t_CEnum>
inline_small typename NMib::TCEnableIf<NMib::NTraits::TCIsEnum<t_CEnum>::mc_Value, t_CEnum>::CType &operator ^= (t_CEnum &_Left, const t_CEnum &_Right)
{
	_Left = _Left ^ _Right;
	return _Left;
}

template <typename t_CEnum>
constexpr inline_small typename NMib::TCEnableIf<NMib::NTraits::TCIsEnum<t_CEnum>::mc_Value, t_CEnum>::CType operator ~ (t_CEnum _Left)
{
	return (t_CEnum)(~uint32(_Left));
}

template <typename t_CEnum, typename t_CShift>
constexpr inline_small typename NMib::TCEnableIf<NMib::NTraits::TCIsEnum<t_CEnum>::mc_Value, t_CEnum>::CType operator << (t_CEnum _Left, t_CShift _nPlaces)
{
	return (t_CEnum)(uint32(_Left) << _nPlaces);
}

template <typename t_CEnum, typename t_CShift>
constexpr inline_small typename NMib::TCEnableIf<NMib::NTraits::TCIsEnum<t_CEnum>::mc_Value, t_CEnum>::CType operator >> (t_CEnum _Left, t_CShift _nPlaces)
{
	return (t_CEnum)(uint32(_Left) >> _nPlaces);
}

template <typename t_CEnum, typename t_CShift>
inline_small typename NMib::TCEnableIf<NMib::NTraits::TCIsEnum<t_CEnum>::mc_Value, t_CEnum>::CType &operator <<= (t_CEnum &_Left, const t_CShift &_nPlaces)
{
	_Left = (t_CEnum)(uint32(_Left) << _nPlaces);
	return _Left;
}

template <typename t_CEnum, typename t_CShift>
inline_small typename NMib::TCEnableIf<NMib::NTraits::TCIsEnum<t_CEnum>::mc_Value, t_CEnum>::CType &operator >>= (t_CEnum &_Left, const t_CShift &_nPlaces)
{
	_Left = (t_CEnum)(uint32(_Left) >> _nPlaces);
	return _Left;
}

// TODO: Make it an error to compare enum and non emum as well

#ifndef DCompiler_clang

// This happens all the time in constant expressions that compare different values. Maybe make OK for anonymous enums somehow

template <typename t_CEnum0, typename t_CEnum1>
constexpr inline_small typename NMib::TCEnableIf<NMib::NTraits::TCIsEnum<t_CEnum0>::mc_Value && NMib::NTraits::TCIsEnum<t_CEnum1>::mc_Value, bool>::CType operator == (t_CEnum0 const &_Left, t_CEnum1 const &_Right)
{
	static_assert(NMib::NTraits::TCIsSame<t_CEnum0, t_CEnum1>::mc_Value, "Comparison of two different enum types is unsafe");
	return _Left == _Right;
}

template <typename t_CEnum0, typename t_CEnum1>
constexpr inline_small typename NMib::TCEnableIf<NMib::NTraits::TCIsEnum<t_CEnum0>::mc_Value && NMib::NTraits::TCIsEnum<t_CEnum1>::mc_Value, bool>::CType operator != (t_CEnum0 const &_Left, t_CEnum1 const &_Right)
{
	static_assert(NMib::NTraits::TCIsSame<t_CEnum0, t_CEnum1>::mc_Value, "Comparison of two different enum types is unsafe");
	return _Left != _Right;
}

template <typename t_CEnum0, typename t_CEnum1>
constexpr inline_small typename NMib::TCEnableIf<NMib::NTraits::TCIsEnum<t_CEnum0>::mc_Value && NMib::NTraits::TCIsEnum<t_CEnum1>::mc_Value, bool>::CType operator < (t_CEnum0 const &_Left, t_CEnum1 const &_Right)
{
	static_assert(NMib::NTraits::TCIsSame<t_CEnum0, t_CEnum1>::mc_Value, "Comparison of two different enum types is unsafe");
	return _Left < _Right;
}

template <typename t_CEnum0, typename t_CEnum1>
constexpr inline_small typename NMib::TCEnableIf<NMib::NTraits::TCIsEnum<t_CEnum0>::mc_Value && NMib::NTraits::TCIsEnum<t_CEnum1>::mc_Value, bool>::CType operator > (t_CEnum0 const &_Left, t_CEnum1 const &_Right)
{
	static_assert(NMib::NTraits::TCIsSame<t_CEnum0, t_CEnum1>::mc_Value, "Comparison of two different enum types is unsafe");
	return _Left > _Right;
}

template <typename t_CEnum0, typename t_CEnum1>
constexpr inline_small typename NMib::TCEnableIf<NMib::NTraits::TCIsEnum<t_CEnum0>::mc_Value && NMib::NTraits::TCIsEnum<t_CEnum1>::mc_Value, bool>::CType operator <= (t_CEnum0 const &_Left, t_CEnum1 const &_Right)
{
	static_assert(NMib::NTraits::TCIsSame<t_CEnum0, t_CEnum1>::mc_Value, "Comparison of two different enum types is unsafe");
	return _Left <= _Right;
}

template <typename t_CEnum0, typename t_CEnum1>
constexpr inline_small typename NMib::TCEnableIf<NMib::NTraits::TCIsEnum<t_CEnum0>::mc_Value && NMib::NTraits::TCIsEnum<t_CEnum1>::mc_Value, bool>::CType operator >= (t_CEnum0 const &_Left, t_CEnum1 const &_Right)
{
	static_assert(NMib::NTraits::TCIsSame<t_CEnum0, t_CEnum1>::mc_Value, "Comparison of two different enum types is unsafe");
	return _Left >= _Right;
}
#endif
