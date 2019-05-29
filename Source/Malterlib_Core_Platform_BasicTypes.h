// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

// Constant
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	define constant_int64(_Number) _Number##LL
#	define constant_uint64(_Number) _Number##ULL
#	define str_utf8(d_StringLiteral) d_StringLiteral
#	define str_utf16(d_StringLiteral) u##d_StringLiteral
#	define str_utf32(d_StringLiteral) U##d_StringLiteral
#elif defined(DCompiler_MSVC)
#	define constant_int64(_Number) _Number##i64
#	define constant_uint64(_Number) _Number##ui64
#	define str_utf8(d_StringLiteral) u8##d_StringLiteral
#	define str_utf16(d_StringLiteral) L##d_StringLiteral
#	define str_utf32(d_StringLiteral) U##d_StringLiteral
#else
#	error "Implement this"
#endif


// Basic types

typedef float pfp32;
#define DMibPCanDo_fp32
static_assert(sizeof(pfp32)*8 == 32, "fp32 not supported");

typedef double pfp64;
#define DMibPCanDo_fp64
static_assert(sizeof(pfp64)*8 == 64, "fp64 not supported");

#ifndef DCompiler_MSVC
typedef long double pfp80;
#define DMibPCanDo_fp80
static_assert(sizeof(pfp80)*8 >= 80, "fp80 not supported");
#endif

#if defined(DPlatformFamily_OSX)
#	if DMibPPtrBits >= 64
		typedef signed char int8;
		#define DMibPCanDo_int8

		typedef signed short int16;
		#define DMibPCanDo_int16

		typedef signed int int32;
		#define DMibPCanDo_int32

		typedef signed long long int64;
		#define DMibPCanDo_int64

		typedef signed __int128 int128;
		#define DMibPCanDo_int128

		typedef unsigned char uint8;
		#define DMibPCanDo_uint8

		typedef unsigned short uint16;
		#define DMibPCanDo_uint16

		typedef unsigned int uint32;
		#define DMibPCanDo_uint32

		typedef unsigned long long uint64; // Nope msvs cant do this
		#define DMibPCanDo_uint64

		typedef unsigned __int128 uint128; // Nope msvs cant do this
		#define DMibPCanDo_uint128

		typedef unsigned long int mint; // Memory size unsigned int
		typedef signed long int smint; // Memory size unsigned int
		typedef signed long long aint; // Architecture size int
		typedef unsigned long long uaint; // Architecture size unsigned int

		typedef char ch8;
		typedef char16_t ch16;
		typedef char32_t ch32;
		//#define int Error

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
		typedef char int8;
		#define DMibPCanDo_int8

		typedef short int16;
		#define DMibPCanDo_int16

		typedef int int32;
		#define DMibPCanDo_int32

		typedef signed long long int64;
		#define DMibPCanDo_int64

		typedef unsigned char uint8;
		#define DMibPCanDo_uint8

		typedef unsigned short uint16;
		#define DMibPCanDo_uint16

		typedef unsigned int uint32;
		#define DMibPCanDo_uint32

		typedef unsigned long long uint64; // Nope msvs cant do this
		#define DMibPCanDo_uint64

		typedef unsigned long mint; // Memory size unsigned int
		typedef signed long smint; // Memory size unsigned int
		typedef int aint; // Architecture size int
		typedef unsigned int uaint; // Architecture size unsigned int

		typedef char ch8;
		typedef char16_t ch16;
		typedef char32_t ch32;
		//#define int Error

		//#define DMibPUniqueType_int
		//#define DMibPUniqueType_uint
		#define DMibPUniqueType_mint
		#define DMibPUniqueType_smint
		//#define DMibPUniqueType_aint
		//#define DMibPUniqueType_uaint
		//#define DMibPUniqueType_ch8
		#define DMibPUniqueType_ch16
		#define DMibPUniqueType_ch32

		#define DMibPSignedType_ch8
		//#define DMibPSignedType_ch16
		//#define DMibPSignedType_ch32
#	endif
#elif defined(DPlatformFamily_Linux)
#	if DMibPPtrBits >= 64
		typedef signed char int8;
		#define DMibPCanDo_int8

		typedef signed short int16;
		#define DMibPCanDo_int16

		typedef signed int int32;
		#define DMibPCanDo_int32

		typedef signed long long int64;
		#define DMibPCanDo_int64

		typedef signed __int128 int128;
		#define DMibPCanDo_int128

		typedef unsigned char uint8;
		#define DMibPCanDo_uint8

		typedef unsigned short uint16;
		#define DMibPCanDo_uint16

		typedef unsigned int uint32;
		#define DMibPCanDo_uint32

		typedef unsigned long long uint64; // Nope msvs cant do this
		#define DMibPCanDo_uint64

		typedef unsigned __int128 uint128;
		#define DMibPCanDo_uint128

		typedef unsigned long int mint; // Memory size unsigned int
		typedef signed long int smint; // Memory size unsigned int
		typedef signed long long aint; // Architecture size int
		typedef unsigned long long uaint; // Architecture size unsigned int

		typedef char ch8;
		typedef char16_t ch16;
		typedef char32_t ch32;
		//#define int Error

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
		typedef char int8;
		#define DMibPCanDo_int8

		typedef short int16;
		#define DMibPCanDo_int16

		typedef long int32;
		#define DMibPCanDo_int32

		typedef signed long long int64;
		#define DMibPCanDo_int64

		typedef unsigned char uint8;
		#define DMibPCanDo_uint8

		typedef unsigned short uint16;
		#define DMibPCanDo_uint16

		typedef unsigned long uint32;
		#define DMibPCanDo_uint32

		typedef unsigned long long uint64; // Nope msvs cant do this
		#define DMibPCanDo_uint64

		typedef unsigned int mint; // Memory size unsigned int
		typedef signed int smint; // Memory size unsigned int
		typedef int aint; // Architecture size int
		typedef unsigned int uaint; // Architecture size unsigned int

		typedef char ch8;
		typedef char16_t ch16;
		typedef char32_t ch32;
		//#define int Error

		//#define DMibPUniqueType_int
		//#define DMibPUniqueType_uint
		#define DMibPUniqueType_mint
		#define DMibPUniqueType_smint
		//#define DMibPUniqueType_aint
		//#define DMibPUniqueType_uaint
		//#define DMibPUniqueType_ch8
		#define DMibPUniqueType_ch16
		#define DMibPUniqueType_ch32

		#define DMibPSignedType_ch8
		//#define DMibPSignedType_ch16
		//#define DMibPSignedType_ch32
#	endif
#elif defined(DPlatformFamily_Windows)
#	if DMibPPtrBits >= 64
		typedef char int8;
		#define DMibPCanDo_int8

		typedef short int16;
		#define DMibPCanDo_int16

		typedef long int32;
		#define DMibPCanDo_int32

		typedef __int64 int64;
		#define DMibPCanDo_int64

		typedef unsigned char uint8;
		#define DMibPCanDo_uint8

		typedef unsigned short uint16;
		#define DMibPCanDo_uint16

		typedef unsigned long uint32;
		#define DMibPCanDo_uint32

		typedef unsigned __int64 uint64; // Nope msvc cant do this
		#define DMibPCanDo_uint64

		typedef __int64 smint; // Memory size unsigned int
		typedef unsigned __int64 mint; // Memory size signed int
		typedef __int64 aint; // Architecture size int
		typedef unsigned __int64 uaint; // Architecture size unsigned int

		typedef int8 ch8;
		typedef wchar_t ch16;
		typedef char32_t ch32;
		//#define int Error

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
		typedef char int8;
		#define DMibPCanDo_int8

		typedef short int16;
		#define DMibPCanDo_int16

		typedef long int32;
		#define DMibPCanDo_int32

		typedef __int64 int64;
		#define DMibPCanDo_int64

		typedef unsigned char uint8;
		#define DMibPCanDo_uint8

		typedef unsigned short uint16;
		#define DMibPCanDo_uint16

		typedef unsigned long uint32;
		#define DMibPCanDo_uint32

		typedef unsigned __int64 uint64; // Nope msvc cant do this
		#define DMibPCanDo_uint64

		typedef unsigned int mint; // Memory size unsigned int
		typedef int smint; // Memory size signed int
		typedef int aint; // Architecture size int
		typedef unsigned int uaint; // Architecture size unsigned int

		typedef int8 ch8;
		typedef wchar_t ch16;
		typedef char32_t ch32;
		//#define int Error

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
	typedef char int8;
	#define DMibPCanDo_int8

	typedef short int16;
	#define DMibPCanDo_int16

	typedef long int32;
	#define DMibPCanDo_int32

	typedef signed long long int64;
	#define DMibPCanDo_int64

	typedef unsigned char uint8;
	#define DMibPCanDo_uint8

	typedef unsigned short uint16;
	#define DMibPCanDo_uint16

	typedef unsigned long uint32;
	#define DMibPCanDo_uint32

	typedef unsigned long long uint64; // Nope msvs cant do this
	#define DMibPCanDo_uint64

	typedef unsigned int mint; // Memory size unsigned int
	typedef signed int smint; // Memory size unsigned int
	typedef int aint; // Architecture size int
	typedef unsigned int uaint; // Architecture size unsigned int

	typedef char ch8;
	typedef char16_t ch16;
	typedef char32_t ch32;
	//#define int Error

	//#define DMibPUniqueType_int
	//#define DMibPUniqueType_uint
	#define DMibPUniqueType_mint
	#define DMibPUniqueType_smint
	//#define DMibPUniqueType_aint
	//#define DMibPUniqueType_uaint
	//#define DMibPUniqueType_ch8
	#define DMibPUniqueType_ch16
	#define DMibPUniqueType_ch32

	#define DMibPSignedType_ch8
	//#define DMibPSignedType_ch16
	//#define DMibPSignedType_ch32
#else
#	error "Implement this"
#endif

#if defined(DPlatformFamily_OSX) || defined(DPlatformFamily_Linux) || defined(DPlatformFamily_Windows) || defined(DPlatformFamily_Emscripten)
	typedef int64 CMibFilePos;
#else
#	error "Implement this"
#endif


//#define DMibPAlignedDataAccess
