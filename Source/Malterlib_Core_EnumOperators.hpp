// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

template <typename tf_CEnum>
DMibSuppressUndefinedSanitizer constexpr inline_small tf_CEnum operator | (tf_CEnum _Left, tf_CEnum _Right) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum>)
{
	using CUnderlyingType = NMib::NTraits::TCEnumUnderlyingType<tf_CEnum>;
	return static_cast<tf_CEnum>(static_cast<CUnderlyingType>(_Left) | static_cast<CUnderlyingType>(_Right));
}

template <typename tf_CEnum>
DMibSuppressUndefinedSanitizer inline_small tf_CEnum &operator |= (tf_CEnum &_Left, tf_CEnum _Right) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum>)
{
	using CUnderlyingType = NMib::NTraits::TCEnumUnderlyingType<tf_CEnum>;
	_Left = static_cast<tf_CEnum>(static_cast<CUnderlyingType>(_Left) | static_cast<CUnderlyingType>(_Right));
	return _Left;
}

template <typename tf_CEnum>
DMibSuppressUndefinedSanitizer constexpr inline_small tf_CEnum operator & (tf_CEnum _Left, tf_CEnum _Right) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum>)
{
	using CUnderlyingType = NMib::NTraits::TCEnumUnderlyingType<tf_CEnum>;
	return static_cast<tf_CEnum>(static_cast<CUnderlyingType>(_Left) & static_cast<CUnderlyingType>(_Right));
}

template <typename tf_CEnum>
DMibSuppressUndefinedSanitizer inline_small tf_CEnum &operator &= (tf_CEnum &_Left, tf_CEnum _Right)
	requires (NMib::NTraits::cIsEnum<tf_CEnum>)
{
	using CUnderlyingType = NMib::NTraits::TCEnumUnderlyingType<tf_CEnum>;
	_Left = static_cast<tf_CEnum>((static_cast<CUnderlyingType>(_Left) & static_cast<CUnderlyingType>(_Right)));
	return _Left;
}

template <typename tf_CEnum>
DMibSuppressUndefinedSanitizer constexpr inline_small tf_CEnum operator ^ (tf_CEnum _Left, tf_CEnum _Right) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum>)
{
	using CUnderlyingType = NMib::NTraits::TCEnumUnderlyingType<tf_CEnum>;
	return static_cast<tf_CEnum>(static_cast<CUnderlyingType>(_Left) ^ static_cast<CUnderlyingType>(_Right));
}

template <typename tf_CEnum>
DMibSuppressUndefinedSanitizer inline_small tf_CEnum &operator ^= (tf_CEnum &_Left, tf_CEnum _Right) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum>)
{
	using CUnderlyingType = NMib::NTraits::TCEnumUnderlyingType<tf_CEnum>;
	_Left = static_cast<tf_CEnum>(static_cast<CUnderlyingType>(_Left) ^ static_cast<CUnderlyingType>(_Right));
	return _Left;
}

template <typename tf_CEnum>
DMibSuppressUndefinedSanitizer constexpr inline_small tf_CEnum operator ~ (tf_CEnum _Left) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum>)
{
	using CUnderlyingType = NMib::NTraits::TCEnumUnderlyingType<tf_CEnum>;
	return static_cast<tf_CEnum>(~static_cast<CUnderlyingType>(_Left));
}

template <typename tf_CEnum, typename tf_CShift>
DMibSuppressUndefinedSanitizer constexpr inline_small tf_CEnum operator << (tf_CEnum _Left, tf_CShift _nPlaces) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum>)
{
	using CUnderlyingType = NMib::NTraits::TCEnumUnderlyingType<tf_CEnum>;
	return static_cast<tf_CEnum>(static_cast<CUnderlyingType>(_Left) << _nPlaces);
}

template <typename tf_CEnum, typename tf_CShift>
DMibSuppressUndefinedSanitizer constexpr inline_small tf_CEnum operator >> (tf_CEnum _Left, tf_CShift _nPlaces) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum>)
{
	using CUnderlyingType = NMib::NTraits::TCEnumUnderlyingType<tf_CEnum>;
	return static_cast<tf_CEnum>(static_cast<CUnderlyingType>(_Left) >> _nPlaces);
}

template <typename tf_CEnum, typename tf_CShift>
DMibSuppressUndefinedSanitizer inline_small tf_CEnum &operator <<= (tf_CEnum &_Left, tf_CShift _nPlaces) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum>)
{
	using CUnderlyingType = NMib::NTraits::TCEnumUnderlyingType<tf_CEnum>;
	_Left = static_cast<tf_CEnum>(static_cast<CUnderlyingType>(_Left) << _nPlaces);
	return _Left;
}

template <typename tf_CEnum, typename tf_CShift>
DMibSuppressUndefinedSanitizer inline_small tf_CEnum &operator >>= (tf_CEnum &_Left, tf_CShift _nPlaces) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum>)
{
	using CUnderlyingType = NMib::NTraits::TCEnumUnderlyingType<tf_CEnum>;
	_Left = static_cast<tf_CEnum>(static_cast<CUnderlyingType>(_Left) >> _nPlaces);
	return _Left;
}

template <typename tf_CEnum0, typename tf_CEnum1>
constexpr inline_small bool operator == (tf_CEnum0 const &_Left, tf_CEnum1 const &_Right) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum0> && NMib::NTraits::cIsEnum<tf_CEnum1>)
{
	static_assert(NMib::NTraits::cIsSame<tf_CEnum0, tf_CEnum1>, "Comparison of two different enum types is unsafe");
	return _Left == _Right;
}

template <typename tf_CEnum0, typename tf_CEnum1>
constexpr inline_small bool operator != (tf_CEnum0 const &_Left, tf_CEnum1 const &_Right) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum0> && NMib::NTraits::cIsEnum<tf_CEnum1>)
{
	static_assert(NMib::NTraits::cIsSame<tf_CEnum0, tf_CEnum1>, "Comparison of two different enum types is unsafe");
	return _Left != _Right;
}

template <typename tf_CEnum0, typename tf_CEnum1>
constexpr inline_small bool operator < (tf_CEnum0 const &_Left, tf_CEnum1 const &_Right) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum0> && NMib::NTraits::cIsEnum<tf_CEnum1>)
{
	static_assert(NMib::NTraits::cIsSame<tf_CEnum0, tf_CEnum1>, "Comparison of two different enum types is unsafe");
	return _Left < _Right;
}

template <typename tf_CEnum0, typename tf_CEnum1>
constexpr inline_small bool operator > (tf_CEnum0 const &_Left, tf_CEnum1 const &_Right) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum0> && NMib::NTraits::cIsEnum<tf_CEnum1>)
{
	static_assert(NMib::NTraits::cIsSame<tf_CEnum0, tf_CEnum1>, "Comparison of two different enum types is unsafe");
	return _Left > _Right;
}

template <typename tf_CEnum0, typename tf_CEnum1>
constexpr inline_small bool operator <= (tf_CEnum0 const &_Left, tf_CEnum1 const &_Right) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum0> && NMib::NTraits::cIsEnum<tf_CEnum1>)
{
	static_assert(NMib::NTraits::cIsSame<tf_CEnum0, tf_CEnum1>, "Comparison of two different enum types is unsafe");
	return _Left <= _Right;
}

template <typename tf_CEnum0, typename tf_CEnum1>
constexpr inline_small bool operator >= (tf_CEnum0 const &_Left, tf_CEnum1 const &_Right) noexcept
	requires (NMib::NTraits::cIsEnum<tf_CEnum0> && NMib::NTraits::cIsEnum<tf_CEnum1>)
{
	static_assert(NMib::NTraits::cIsSame<tf_CEnum0, tf_CEnum1>, "Comparison of two different enum types is unsafe");
	return _Left >= _Right;
}

namespace NMib
{
	template <typename tf_CType>
	constexpr inline_always bool fg_IsSet(tf_CType _Value, tf_CType _Flags) noexcept
		requires (NTraits::cIsScopedEnum<tf_CType>)
	{
		return (_Value & _Flags) != tf_CType(0);
	}
}
