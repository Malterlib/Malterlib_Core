// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#ifndef DCompiler
#	if defined(__clang__)
#		define DCompiler clang
#		define DCompiler_clang
#	elif defined (_MSC_VER)
#		define DCompiler MSVC
#		define DCompiler_MSVC
#		if defined __EDG__
#			define DCompiler_MSVC_EDG
#		endif
#	elif defined (__GNUC__)
#		define DCompiler gcc
#		define DCompiler_gcc
#	else
#		error "Compiler not detected"
#	endif
#endif


#ifndef DPlatformFamily
#	if defined(__APPLE__)
#		define DPlatformFamily macOS
#		define DPlatformFamily_macOS
#	elif defined(__linux__)
#		define DPlatformFamily Linux
#		define DPlatformFamily_Linux
#	elif defined(__WIN32__) || defined(_MSC_VER)
#		define DPlatformFamily Windows
#		define DPlatformFamily_Windows
#	elif defined(EMSCRIPTEN)
#		define DPlatformFamily Emscripten
#		define DPlatformFamily_Emscripten
#	else
#		error "Platform family not detected"
#	endif
#endif

#ifndef DArchitecture
#	if defined(__ARM_ARCH_6K__)
#		define DArchitecture armv6
#		define DArchitecture_armv6
#	elif defined(__ppc64__)
#		define DArchitecture ppc64
#		define DArchitecture_ppc64
#	elif defined(__ppc__)
#		define DArchitecture ppc32
#		define DArchitecture_ppc32
#	elif defined(__i386__)
#		define DArchitecture x86
#		define DArchitecture_x86
#	elif defined(__amd64__) || defined(__x86_64__)
#		define DArchitecture x64
#		define DArchitecture_x64
#	elif defined(DPlatformFamily_Emscripten)
#		define DArchitecture le32
#		define DArchitecture_le32
#	else
#		error "Architecture not detected"
#	endif
#endif


// Enable SSE on x64 and x86
#if defined(DArchitecture_x86) || defined(DArchitecture_x64)
#	ifndef DArchitectureExtension_SSE
#		define DArchitectureExtension_SSE 1
#	endif
#endif


// Default debugger if not defined
#ifndef DDebugger
#	if defined(DCompiler_clang)
#		define DDebugger lldb
#		define DDebugger_lldb
#	elif defined(DCompiler_MSVC)
#		define DDebugger VisualStudio
#		define DDebugger_VisualStudio
#	endif
#endif


// Debug mode
#if defined(DCompiler_clang) || defined(DCompiler_gcc) || defined(DCompiler_MSVC)
#	ifdef _DEBUG
#		define DMibDebug
#	endif
#else
#	error "Implement this"
#endif


#if defined(DArchitecture_x86) || defined(DArchitecture_armv6) || defined(DArchitecture_le32) || defined(DArchitecture_ppc32)
#	define DMibPPtrBits 32
#	define DMibPBits_int 32
#	define DMibPBits_uint 32
#	define DMibPBits_mint 32
#elif  defined(DArchitecture_x64) || defined(DArchitecture_ppc64) || defined(DArchitecture_arm64) || defined(DArchitecture_arm64e)
#	define DMibPPtrBits 64
#	define DMibPBits_int 64
#	define DMibPBits_uint 64
#	define DMibPBits_mint 64
#else
#	error "Implement this"
#endif


// Endian
#if defined(DArchitecture_x64)  || defined(DArchitecture_x86) || defined(DArchitecture_armv6) || defined(DArchitecture_le32) || defined(DArchitecture_arm64) || defined(DArchitecture_arm64e)
#	define DMibPLittleEndian
#elif defined(DArchitecture_ppc64) || defined(DArchitecture_ppc32)
	// Big endian
#else
#	error "Implement this"
#endif


// Cacheline size
#if defined(DArchitecture_x64)  || defined(DArchitecture_x86) || defined(DArchitecture_armv6) || defined(DArchitecture_le32) || defined(DArchitecture_arm64) || defined(DArchitecture_arm64e)
#	define DMibPMemoryCacheLineSize 64
#elif defined(DArchitecture_ppc32)
#	define DMibPMemoryCacheLineSize 32
#elif defined(DArchitecture_ppc64)
#	define DMibPMemoryCacheLineSize 128
#else
#	error "Implement this"
#endif


// Compiler version
#if defined(DCompiler_clang)
#	define DMibCompilerVersion __clang_major__##__clang_minor__##__clang_patchlevel__
#elif defined(DCompiler_gcc)
#	define DMibCompilerVersion __GNUC__##__GNUC_MINOR__##__GNUC_PATCHLEVEL__
#elif defined(DCompiler_MSVC)
#	define DMibCompilerVersion _MSC_VER
#else
#	error "Implement this"
#endif
