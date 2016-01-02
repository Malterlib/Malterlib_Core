// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#if defined(DPlatformFamily_OSX)

#elif defined(DPlatformFamily_Linux)
#elif defined(DPlatformFamily_Windows)
#	if DMibPPtrBits >= 64
#		if !defined(_KERNEL32_)
#			define WINBASEAPI DECLSPEC_IMPORT
#		else
#			define WINBASEAPI
#		endif
#		ifndef NTDDI_VERSION
#			define _WIN32_WINNT _WIN32_WINNT_WS03
#			define NTDDI_VERSION NTDDI_WS03
#		endif
#	else
#		ifndef NTDDI_VERSION
#			define _WIN32_WINNT _WIN32_WINNT_WINXP
#			define NTDDI_VERSION NTDDI_WINXPSP2
#		endif
#	endif
#elif defined(DPlatformFamily_Emscripten)
#	include <emscripten.h>
#else
#	error "Implement this"
#endif

#if defined(DPlatformFamily_OSX) || defined(DPlatformFamily_Linux)
#	ifndef DMibNewOverride
#		define DMibNewOverride
#	endif
#endif


// Optimal char size
#if defined(DPlatformFamily_OSX)
#	define DMibOptimalSystemCharSize 1
#elif defined(DPlatformFamily_Emscripten)
#	define DMibOptimalSystemCharSize 1
#elif defined(DPlatformFamily_Linux)
#	define DMibOptimalSystemCharSize 1
#elif defined(DPlatformFamily_Windows)
#	define DMibOptimalSystemCharSize 2
#else
#	error "Implement this"
#endif


// System creation
#if defined(DPlatformFamily_OSX) || defined(DPlatformFamily_Linux) || defined(DPlatformFamily_Windows)
#	define DMibPAutomaticSystemCreation
#else
#	error "Implement this"
#endif


// Safe timer
#if defined(DPlatformFamily_OSX) || defined(DPlatformFamily_Linux) || defined(DPlatformFamily_Emscripten)
	// Not supported or needed
#elif defined(DPlatformFamily_Windows)
#	define DMibSafeTimerAvailable
#else
#	error "Implement this"
#endif


// Typeinfo and exceptions
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	define DMibPSupportExceptions 1
#	if __has_feature(cxx_rtti)
#		define DMibPSupportTypeinfo 1
#	else
#		define DMibPSupportTypeinfo 0
#	endif
#elif defined(DCompiler_MSVC)
#	ifdef _DDK_DRIVER_
#		define DMibPSupportExceptions 0
#		define DMibPSupportTypeinfo 0
#	else
#		define DMibPSupportExceptions 1
#		define DMibPSupportTypeinfo 1
#	endif
#else
#	error "Implement this"
#endif


// Boost
//#define BOOST_NO_STD_TYPEINFO
#define BOOST_ALL_NO_LIB


// Float implementation
#define DMibPFloat_StdLib



// New override
#if defined(DPlatformFamily_OSX) || defined(DPlatformFamily_Linux)
#	include <new>
#	define DMibPOverrideOperatorNew
#elif defined(DPlatformFamily_Windows)
#	ifndef _DEBUG
#		define DMibPOverrideOperatorNew
#	endif
#else
#	error "Implement this"
#endif
