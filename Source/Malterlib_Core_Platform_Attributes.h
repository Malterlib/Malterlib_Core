// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

// Struct packing
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	define DMibPStartPackedStruct
#	define DMibPPackedStruct __attribute__((packed))
#	define DMibPEndPackedStruct
#elif defined(DCompiler_MSVC)
#	define DMibPStartPackedStruct __pragma(pack(push, 1))
#	define DMibPPackedStruct 
#	define DMibPEndPackedStruct __pragma(pack(pop))
#else
#	error "Implement this"
#endif

// Calling convention
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	ifdef DArchitecture_x86
#		define calling_convention_c __attribute__((cdecl))
#	else
#		define calling_convention_c
#	endif
#elif defined(DCompiler_MSVC)
#	define calling_convention_c __cdecl
#else
#	error "Implement this"
#endif

// Module export
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	define module_export __attribute__ ((__visibility__("default")))
#	define module_import
#elif defined(DCompiler_MSVC)
#	define module_export __declspec(dllexport)
#	define module_import __declspec(dllimport)
#else
#	error "Implement this"
#endif

// Force linking
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	define assure_used __attribute__((used))
#elif defined(DCompiler_MSVC)
#	define assure_used
#else
#	error "Implement this"
#endif

// Disable unused warning for function
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	define allow_unused __attribute__((unused)) 
#elif defined(DCompiler_MSVC)
#	define allow_unused
#else
#	error "Implement this"
#endif



// Likely
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	define likely(d_Expression) __builtin_expect(!!(d_Expression), 1)
#	define unlikely(d_Expression) __builtin_expect(!!(d_Expression), 0)
#elif defined(DCompiler_MSVC)
#	define likely(d_Expression) d_Expression
#	define unlikely(d_Expression) d_Expression
#else
#	error "Implement this"
#endif

// Inline always
#if !defined(_DEBUG) || defined(DConfig_DebugInlined)
#	define DMibPInlineActive 1
#else
#	define DMibPInlineActive 0
#endif

#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	if DMibPInlineActive
#		define inline_always inline __attribute__((always_inline))
#	else
#		define inline_always inline
#	endif
#elif defined(DCompiler_MSVC)
#	if DMibPInlineActive
#		define inline_always __forceinline
#	else
#		define inline_always inline
#	endif
#else
#	error "Implement this"
#endif

// Other inlining
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	define inline_always_debug inline __attribute__((always_inline))
#	define inline_never __attribute__((noinline))
#elif defined(DCompiler_MSVC)
#	define inline_always_debug __forceinline
#	define inline_never __declspec(noinline)
#	ifdef DConfig_DebugInlined
//#		pragma auto_inline(off)
#		pragma inline_recursion(off)
//#		pragma inline_depth(3)
#	else
#		pragma inline_depth(255)
#	endif
#else
#	error "Implement this"
#endif

// Inlinelevel
#ifndef DMibPInlineLevel
#	define DMibPInlineLevel 3
#endif

#if DMibPInlineLevel > 1
#	define inline_small inline_always
#else
#	define inline_small 
#endif

#if DMibPInlineLevel > 2
#	define inline_medium inline_always
#else
#	define inline_medium
#endif

#if DMibPInlineLevel > 3
#	define inline_large inline_always
#else
#	define inline_large
#endif

#if DMibPInlineLevel > 4
#	define inline_extralarge inline_always
#else
#	define inline_extralarge 
#endif


// Aliasing
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	define only_parameters_aliased 
#	define malloc_like __attribute__((__malloc__))
#	define return_not_aliased 
#	define function_does_not_return __attribute__((noreturn))
#	define variable_not_aliased __restrict__

#elif defined(DCompiler_MSVC)
#	define only_parameters_aliased __declspec(noalias)
#	define return_not_aliased __declspec(restrict)
#	define malloc_like __declspec(restrict)
#	define function_does_not_return __declspec(noreturn)
#	define variable_not_aliased __restrict
#else
#	error "Implement this"
#endif


// Alignment
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	define DMibPAlignType(_Type, _Alignment) __attribute__ ((aligned (_Alignment))) _Type
#	define align_cacheline __attribute__ ((aligned (DMibPMemoryCacheLineSize)))
#	define DMibPMaxAlign 8192
#elif defined(DCompiler_MSVC)
#	define DMibPAlignType(d_Type, d_Alignment) __declspec(align(d_Alignment)) d_Type
#	define align_cacheline __declspec(align(DMibPMemoryCacheLineSize))
#	define DMibPMaxAlign 8192
#else
#	error "Implement this"
#endif

