// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#if defined(DCompiler_clang) || defined(DCompiler_gcc)
namespace NMib::NPlatformHelpers
{
	constexpr inline_always int fg_GetHighestBitSet32(uint32 _Value)
	{
		if (!_Value)
			return -1;
		return 31 - __builtin_clz(_Value);
	}

	constexpr inline_always int fg_GetHighestBitSet32NoZero(uint32 _Value)
	{
		return 31 - __builtin_clz(_Value);
	}

	constexpr inline_always int fg_GetLowestBitSet32(uint32 _Value)
	{
		if (!_Value)
			return -1;
		return __builtin_ctz(_Value);
	}

	constexpr inline_always int fg_GetLowestBitSet32NoZero(uint32 _Value)
	{
		return __builtin_ctz(_Value);
	}

#if DMibPPtrBits >= 64
	constexpr inline_always int fg_GetHighestBitSet64(uint64 _Value)
	{
		if (!_Value)
			return -1;
		return 63 - __builtin_clzll(_Value);
	}

	constexpr inline_always int fg_GetHighestBitSet64NoZero(uint64 _Value)
	{
		return 63 - __builtin_clzll(_Value);
	}

	constexpr inline_always int fg_GetLowestBitSet64(uint64 _Value)
	{
		if (!_Value)
			return -1;
		return __builtin_ctzll(_Value);
	}

	constexpr inline_always int fg_GetLowestBitSet64NoZero(uint64 _Value)
	{
		return __builtin_ctzll(_Value);
	}
#endif
}
#endif
