// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

// Constant
#if defined(DCompiler_MSVC) || defined(DCompiler_clang_cl)
#	define constant_int64(_Number) _Number##i64
#	define constant_uint64(_Number) _Number##ui64
#	define str_utf8(d_StringLiteral) d_StringLiteral
#	define str_utf16(d_StringLiteral) L##d_StringLiteral
#	define str_utf32(d_StringLiteral) U##d_StringLiteral
#elif defined(DCompiler_clang) || defined(DCompiler_gcc)
#	define constant_int64(_Number) _Number##LL
#	define constant_uint64(_Number) _Number##ULL
#	define str_utf8(d_StringLiteral) d_StringLiteral
#	define str_utf16(d_StringLiteral) u##d_StringLiteral
#	define str_utf32(d_StringLiteral) U##d_StringLiteral
#else
#	error "Implement this"
#endif


// Basic types

using pfp32 = float;
#define DMibPCanDo_fp32
static_assert(sizeof(pfp32)*8 == 32, "fp32 not supported");

using pfp64 = double;
#define DMibPCanDo_fp64
static_assert(sizeof(pfp64)*8 == 64, "fp64 not supported");

#if !defined(DCompiler_MSVC) && !defined(DCompiler_clang_cl) && __LDBL_MANT_DIG__ == 64
using pfp80 = long double;
#define DMibPCanDo_fp80
static_assert(sizeof(pfp80)*8 >= 80, "fp80 not supported");
#endif

#if defined(DPlatformFamily_macOS)
#	if DMibPPtrBits >= 64
		#define DMibPCanDo_int8
		#define DMibPCanDo_int16
		#define DMibPCanDo_int32
		#define DMibPCanDo_int64
		#define DMibPCanDo_int128
		#define DMibPCanDo_uint8
		#define DMibPCanDo_uint16
		#define DMibPCanDo_uint32
		#define DMibPCanDo_uint64
		#define DMibPCanDo_uint128

		using int8 = signed char;
		using int16 = signed short;
		using int32 = signed int;
		using int64 = signed long long;
		using int128 = signed __int128;
		using uint8 = unsigned char;
		using uint16 = unsigned short;
		using uint32 = unsigned int;
		using uint64 = unsigned long long;
		using uint128 = unsigned __int128;

		using umint = unsigned long int;
		using smint = signed long int;
		using aint = signed long long;
		using uaint = unsigned long long;

		using ch8 = char;
		using ch16 = char16_t;
		using ch32 = char32_t;

		//#define DMibPUniqueType_int
		//#define DMibPUniqueType_uint
		#define DMibPUniqueType_mint
		#define DMibPUniqueType_smint
		//#define DMibPUniqueType_aint
		//#define DMibPUniqueType_uaint
		#define DMibPUniqueType_ch8
		#define DMibPUniqueType_ch16
		#define DMibPUniqueType_ch32

		#define DMibPSignedType_ch8
		//#define DMibPSignedType_ch16
		//#define DMibPSignedType_ch32
#	else
		#define DMibPCanDo_int8
		#define DMibPCanDo_int16
		#define DMibPCanDo_int32
		#define DMibPCanDo_int64
		#define DMibPCanDo_uint8
		#define DMibPCanDo_uint16
		#define DMibPCanDo_uint32
		#define DMibPCanDo_uint64

		using int8 = signed char;
		using int16 = signed short;
		using int32 = signed int;
		using int64 = signed long long;
		using uint8 = unsigned char;
		using uint16 = unsigned short;
		using uint32 = unsigned int;
		using uint64 = unsigned long long;

		using umint = unsigned long;
		using smint = signed long;
		using aint = int;
		using uaint = unsigned int;

		using ch8 = char;
		using ch16 = char16_t;
		using ch32 = char32_t;

		//#define DMibPUniqueType_int
		//#define DMibPUniqueType_uint
		#define DMibPUniqueType_mint
		#define DMibPUniqueType_smint
		//#define DMibPUniqueType_aint
		//#define DMibPUniqueType_uaint
		#define DMibPUniqueType_ch8
		#define DMibPUniqueType_ch16
		#define DMibPUniqueType_ch32

		#define DMibPSignedType_ch8
		//#define DMibPSignedType_ch16
		//#define DMibPSignedType_ch32
#	endif
#elif defined(DPlatformFamily_Linux)
#	if DMibPPtrBits >= 64
		#define DMibPCanDo_int8
		#define DMibPCanDo_int16
		#define DMibPCanDo_int32
		#define DMibPCanDo_int64
		#define DMibPCanDo_int128
		#define DMibPCanDo_uint8
		#define DMibPCanDo_uint16
		#define DMibPCanDo_uint32
		#define DMibPCanDo_uint64
		#define DMibPCanDo_uint128

		using int8 = signed char;
		using int16 = signed short;
		using int32 = signed int;
		using int64 = signed long long;
		using int128 = signed __int128;
		using uint8 = unsigned char;
		using uint16 = unsigned short;
		using uint32 = unsigned int;
		using uint64 = unsigned long long;
		using uint128 = unsigned __int128;

		using umint = unsigned long int;
		using smint = signed long int;
		using aint = signed long long;
		using uaint = unsigned long long;

		using ch8 = char;
		using ch16 = char16_t;
		using ch32 = char32_t;

		//#define DMibPUniqueType_int
		//#define DMibPUniqueType_uint
		#define DMibPUniqueType_mint
		#define DMibPUniqueType_smint
		//#define DMibPUniqueType_aint
		//#define DMibPUniqueType_uaint
		#define DMibPUniqueType_ch8
		#define DMibPUniqueType_ch16
		#define DMibPUniqueType_ch32

#if !defined(DArchitecture_arm64)
		#define DMibPSignedType_ch8
#endif
		//#define DMibPSignedType_ch16
		//#define DMibPSignedType_ch32
#	else
		#define DMibPCanDo_int8
		#define DMibPCanDo_int16
		#define DMibPCanDo_int32
		#define DMibPCanDo_int64
		#define DMibPCanDo_uint8
		#define DMibPCanDo_uint16
		#define DMibPCanDo_uint32
		#define DMibPCanDo_uint64

		using int8 = signed char;
		using int16 = signed short;
		using int32 = signed long;
		using int64 = signed long long;
		using uint8 = unsigned char;
		using uint16 = unsigned short;
		using uint32 = unsigned long;
		using uint64 = unsigned long long;

		using umint = unsigned int;
		using smint = signed int;
		using aint = int;
		using uaint = unsigned int;

		using ch8 = char;
		using ch16 = char16_t;
		using ch32 = char32_t;

		//#define DMibPUniqueType_int
		//#define DMibPUniqueType_uint
		#define DMibPUniqueType_mint
		#define DMibPUniqueType_smint
		//#define DMibPUniqueType_aint
		//#define DMibPUniqueType_uaint
		#define DMibPUniqueType_ch8
		#define DMibPUniqueType_ch16
		#define DMibPUniqueType_ch32

		#define DMibPSignedType_ch8
		//#define DMibPSignedType_ch16
		//#define DMibPSignedType_ch32
#	endif
#elif defined(DPlatformFamily_Windows)
#	if DMibPPtrBits >= 64
		#define DMibPCanDo_int8
		#define DMibPCanDo_int16
		#define DMibPCanDo_int32
		#define DMibPCanDo_int64
		#define DMibPCanDo_uint8
		#define DMibPCanDo_uint16
		#define DMibPCanDo_uint32
		#define DMibPCanDo_uint64

		using int8 = char;
		using int16 = short;
		using int32 = long;
		using int64 = __int64;
		using uint8 = unsigned char;
		using uint16 = unsigned short;
		using uint32 = unsigned long;
		using uint64 = unsigned __int64;

		using smint = __int64;
		using umint = unsigned __int64;
		using aint = __int64;
		using uaint = unsigned __int64;

		using ch8 = int8;
		using ch16 = wchar_t;
		using ch32 = char32_t;

		#define DMibPUniqueType_int
		#define DMibPUniqueType_uint
		//#define DMibPUniqueType_mint
		//#define DMibPUniqueType_smint
		//#define DMibPUniqueType_aint
		//#define DMibPUniqueType_uaint
		//#define DMibPUniqueType_ch8
		#ifndef __EDG__
		#define DMibPUniqueType_ch16
		#endif
		#define DMibPUniqueType_ch32
		#define DMibPSignedType_ch8
		//#define DMibPSignedType_ch16
		//#define DMibPSignedType_ch32
#	else
		#define DMibPCanDo_int8
		#define DMibPCanDo_int16
		#define DMibPCanDo_int32
		#define DMibPCanDo_int64
		#define DMibPCanDo_uint8
		#define DMibPCanDo_uint16
		#define DMibPCanDo_uint32
		#define DMibPCanDo_uint64

		using int8 = char;
		using int16 = short;
		using int32 = long;
		using int64 = __int64;
		using uint8 = unsigned char;
		using uint16 = unsigned short;
		using uint32 = unsigned long;
		using uint64 = unsigned __int64;
		using umint = unsigned int;
		using smint = int;
		using aint = int;
		using uaint = unsigned int;
		using ch8 = int8;
		using ch16 = wchar_t;
		using ch32 = char32_t;

		//#define DMibPUniqueType_int
		//#define DMibPUniqueType_uint
		#define DMibPUniqueType_mint
		#define DMibPUniqueType_smint
		//#define DMibPUniqueType_aint
		//#define DMibPUniqueType_uaint
		//#define DMibPUniqueType_ch8
		#ifndef __EDG__
		#define DMibPUniqueType_ch16
		#endif
		#define DMibPUniqueType_ch32
		#define DMibPSignedType_ch8
		//#define DMibPSignedType_ch16
		//#define DMibPSignedType_ch32
#	endif
#elif defined(DPlatformFamily_Emscripten)
	#define DMibPCanDo_int8
	#define DMibPCanDo_int16
	#define DMibPCanDo_int32
	#define DMibPCanDo_int64
	#define DMibPCanDo_uint8
	#define DMibPCanDo_uint16
	#define DMibPCanDo_uint32
	#define DMibPCanDo_uint64

	using int8 = signed char;
	using int16 = signed short;
	using int32 = signed long;
	using int64 = signed long long;
	using uint8 = unsigned char;
	using uint16 = unsigned short;
	using uint32 = unsigned long;
	using uint64 = unsigned long long;
	using umint = unsigned int;
	using smint = signed int;
	using aint = int;
	using uaint = unsigned int;
	using ch8 = char;
	using ch16 = char16_t;
	using ch32 = char32_t;

	//#define DMibPUniqueType_int
	//#define DMibPUniqueType_uint
	#define DMibPUniqueType_mint
	#define DMibPUniqueType_smint
	//#define DMibPUniqueType_aint
	//#define DMibPUniqueType_uaint
	#define DMibPUniqueType_ch8
	#define DMibPUniqueType_ch16
	#define DMibPUniqueType_ch32

	#define DMibPSignedType_ch8
	//#define DMibPSignedType_ch16
	//#define DMibPSignedType_ch32
#else
#	error "Implement this"
#endif

#if defined(DPlatformFamily_macOS) || defined(DPlatformFamily_Linux) || defined(DPlatformFamily_Windows) || defined(DPlatformFamily_Emscripten)
	using CMibFilePos = int64;
#else
#	error "Implement this"
#endif
