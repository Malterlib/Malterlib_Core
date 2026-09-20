// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#if defined(DPlatformFamily_macOS)

#elif defined(DPlatformFamily_Linux)
#elif defined(DPlatformFamily_Windows)
#	ifndef NTDDI_VERSION
#		define _WIN32_WINNT _WIN32_WINNT_WIN8
#		define NTDDI_VERSION NTDDI_WIN8
#	endif
#	if DMibPPtrBits >= 64
#		if !defined(_KERNEL32_)
#			define WINBASEAPI DECLSPEC_IMPORT
#		else
#			define WINBASEAPI
#		endif
#	endif
#elif defined(DPlatformFamily_Emscripten)
#	include <emscripten.h>
#else
#	error "Implement this"
#endif

#if defined(DPlatformFamily_macOS) || defined(DPlatformFamily_Linux)
#	if !defined(_LIBCPP_DISABLE_NEW_DELETE) && !defined(DMibDefaultToolset) && defined(DMalterlibUseStaticLibCxx)
#		define _LIBCPP_DISABLE_NEW_DELETE
#	endif
#endif

// Optimal char size
#if defined(DPlatformFamily_macOS)
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
#if defined(DPlatformFamily_macOS) || defined(DPlatformFamily_Linux) || defined(DPlatformFamily_Windows)
#	define DMibPAutomaticSystemCreation
#else
#	error "Implement this"
#endif


// Safe timer
#if defined(DPlatformFamily_macOS) || defined(DPlatformFamily_Linux) || defined(DPlatformFamily_Emscripten)
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
#ifndef BOOST_ALL_NO_LIB
#	define BOOST_ALL_NO_LIB
#endif


// Float implementation
#define DMibPFloat_StdLib

// Sized destruction
//
// The compiler sets this feature where it compiles with
// -fmalterlib-sized-destructors, and every deleting destructor then returns the
// size and the address of the complete object instead of freeing it when
// __builtin_malterlib_destroy calls it. The attribute marks the functions that
// construct such objects for an allocator that later frees them with that size,
// so that the linker can prove their classes were compiled the same way.
#if defined(__has_feature)
#	if __has_feature(malterlib_sized_destructors)
#		define DMibPSizedDestructors
#	endif
#endif

#ifdef DMibPSizedDestructors
#	define DMibSizedConstruction [[malterlib::sized_construction]]
#else
#	define DMibSizedConstruction
#endif

// New override
#if defined(DPlatformFamily_macOS) || defined(DPlatformFamily_Linux)
#ifdef DMalterlibUseStaticLibCxx
#	include <stddef.h>
#else
#	include <new>
#endif
#	define DMibPOverrideOperatorNew
#elif defined(DPlatformFamily_Windows)
#	define DMibPOverrideOperatorNew
#else
#	error "Implement this"
#endif
