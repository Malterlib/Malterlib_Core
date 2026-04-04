// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

// Preprocessor
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	define DMibPFile __FILE__
#	define DMibPLine __LINE__
#	define DMibPFunction __FUNCTION__
#	define DMibPFunctionSignature __PRETTY_FUNCTION__
#	define DMibPFunctionUndecorated __FUNCTION__
#elif defined(DCompiler_MSVC)
#	define DMibPFile __FILE__
#	define DMibPLine __LINE__
#	define DMibPFunction __FUNCTION__
#	define DMibPFunctionSignature __FUNCSIG__
#	define DMibPFunctionUndecorated __FUNCDNAME__
#else
#	error "Implement this"
#endif

// Compiler message
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	define DMibCompilerMessage(d_Message)
#elif defined(DCompiler_MSVC)
#	define DMibCompilerMessage(d_Message) __pragma(message(d_Message))
#else
#	error "Implement this"
#endif

// Deprecation
#if defined(DCompiler_clang)
#	define DMibDeprecated __attribute__((deprecated))
#	define DMibDeprecatedSuppressStart	_Pragma("clang diagnostic push") \
										_Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"")
#	define DMibDeprecatedSuppressStop	_Pragma("clang diagnostic pop")
#else
#	define DMibDeprecated
#	define DMibDeprecatedSuppressStart
#	define DMibDeprecatedSuppressStop
#endif

#ifdef DCompiler_clang
#define if_consteval if consteval
#define if_not_consteval if !consteval
#else
#ifdef DMalterlibUseStaticLibCxx
#	include <__type_traits/is_constant_evaluated.h>
#else
#	include <type_traits>
#endif
#define if_consteval if (std::is_constant_evaluated())
#define if_not_consteval if (!std::is_constant_evaluated())
#endif


#if DMibPSupportExceptions
#	ifdef DMalterlibUseStaticLibCxx
#		include <__exception/exception.h>
#	else
#		include <exception>
#	endif
#endif

#if DMalterlibUseLibCxx

_LIBCPP_BEGIN_NAMESPACE_STD
	class weak_equality;
	class strong_equality;
	class partial_ordering;
	class weak_ordering;
	class strong_ordering;
_LIBCPP_END_NAMESPACE_STD

#	if __has_feature(cxx_rtti)
#		ifdef DMalterlibUseStaticLibCxx
		namespace std
		{
			class _LIBCPP_EXPORTED_FROM_ABI type_info;
		}
#		else
#			include <typeinfo>
#		endif
		namespace NMib
		{
			namespace NTypeInfo
			{
				using CTypeInfo = std::type_info;
			}
		}
#	endif
#elif defined(DCompiler_MSVC) || defined(DCompiler_clang_cl)

namespace std
{
	struct weak_equality;
	struct strong_equality;
	struct partial_ordering;
	struct weak_ordering;
	struct strong_ordering;
}

#	if DMibPSupportTypeinfo
#		include <typeinfo>
		namespace NMib
		{
			namespace NTypeInfo
			{
				using CTypeInfo = type_info;
			}
		}
#	endif

#else
#	error "Implement this"
#endif

// Arglist
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	include <stdarg.h>
	using CMibArgList = __builtin_va_list;
#	define DMibPArgListStart(_ArgList, _PrevArg) va_start(_ArgList, _PrevArg)
#	define DMibPArgListNextArg(_ArgList, _ArgType) va_arg(_ArgList, _ArgType)
#	define DMibPArgListEnd(_ArgList) va_end(_ArgList)
#elif defined(DCompiler_MSVC)
#	if defined(DArchitecture_x86)
#		define DMibPArglistIntSizeOf(n)   ( (sizeof(n) + sizeof(int) - 1) & (~(sizeof(int) - 1)) )
#		define DMibPArglistIntAlign(n)   ( (n + sizeof(int) - 1) & (~(sizeof(int) - 1)) )
		using CMibArgList = void *;
#		define DMibPArgListStart(_ArgList, _PrevArg) { _ArgList = ((char *)&_PrevArg + DMibPArglistIntSizeOf(_PrevArg)); }
		namespace NMib
		{
			namespace NHelpers
			{
				__forceinline static void * ArgListNextArg(CMibArgList &_ArgList, int _ArgSize)
				{
					void *pToReturn = _ArgList;
					_ArgList = (((char *)_ArgList) + DMibPArglistIntAlign(_ArgSize));
					return pToReturn;
				}
			}
		}
#		define DMibPArgListNextArg(_ArgList, _ArgType) (*((_ArgType *)NMib::NHelpers::ArgListNextArg(_ArgList, sizeof(_ArgType))))
#		define DMibPArgListEnd(_ArgList) ((void)0)
#	elif defined(DArchitecture_x64)
		using CMibArgList = char *;
		extern "C" void __cdecl __va_start(CMibArgList *, ...);
#		define DMibPArgListStart(_ArgList, _PrevArg) ( __va_start(&_ArgList, _PrevArg) )
#		define DMibPArgListNextArg(_ArgList, _ArgType) \
			( ( sizeof(_ArgType) > sizeof(__int64) || ( sizeof(_ArgType) & (sizeof(_ArgType) - 1) ) != 0 ) \
				? **(_ArgType **)( ( _ArgList += sizeof(__int64) ) - sizeof(__int64) ) \
				:  *(_ArgType  *)( ( _ArgList += sizeof(__int64) ) - sizeof(__int64) ) )
#		define DMibPArgListEnd(_ArgList) ((void)0)
#	else
#		include <stdarg.h>
		using CMibArgList = va_list;
#		define DMibPArgListStart(_ArgList, _PrevArg) va_start(_ArgList, _PrevArg)
#		define DMibPArgListNextArg(_ArgList, _ArgType) va_arg(_ArgList, _ArgType)
#		define DMibPArgListEnd(_ArgList) va_end(_ArgList)
#	endif
#else
#	error "Implement this"
#endif

#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	define DMibSuppressUndefinedSanitizer __attribute__((no_sanitize("undefined")))
#elif defined(DCompiler_MSVC)
#	define DMibSuppressUndefinedSanitizer
#else
#	error "Implement this"
#endif

#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	define DMibSuppressThreadSanitizer __attribute__((no_sanitize("thread")))
#elif defined(DCompiler_MSVC)
#	define DMibSuppressThreadSanitizer
#else
#	error "Implement this"
#endif

#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	define DMibRelaxConstexpr(d_Expression) __builtin_constant_p(d_Expression) ? d_Expression : d_Expression
#elif defined(DCompiler_MSVC)
#	define DMibRelaxConstexpr(d_Expression) d_Expression
#else
#	error "Implement this"
#endif

// Offset
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	define DMibPOffsetOf(_Type, _Member) __builtin_offsetof(_Type, _Member)
#elif defined(DCompiler_MSVC)
#	define DMibPOffsetOf(_Type, _Member) __builtin_offsetof(_Type, _Member)
//#	define DMibPOffsetOf(_Type, _Member) ((aint)(&((_Type *)0)->_Member))
#else
#	error "Implement this"
#endif

#if defined(DCompiler_clang_cl)
#	define DMibNoUniqueAddress [[msvc::no_unique_address]]
#elif defined(DCompiler_MSVC)
#	define DMibNoUniqueAddress [[msvc::no_unique_address]]
#else
#	define DMibNoUniqueAddress [[no_unique_address]]
#endif

// Used for working around bugs in UBSAN
#if defined(DArchitecture_arm64) || defined(DArchitecture_arm64e) || !defined(DPlatformFamily_Linux) || !defined(DMibSanitizerEnabled_UndefinedBehavior)
	#define DMibWorkaroundUBSanSectionErrors
	#define DMibWorkaroundUBSanSectionErrorsDisable
#else
	#define DMibWorkaroundUBSanSectionErrors __attribute__ ((section("ubsan_broken_section")))
	#define DMibWorkaroundUBSanSectionErrorsDisable __attribute__((no_sanitize("undefined")))
#endif

#ifdef DMibSanitizerEnabled_Thread
	#include <sanitizer/tsan_interface.h>

	#define DMibSanitizerAnnotate_MutexCreate(...) __tsan_mutex_create(__VA_ARGS__)
	#define DMibSanitizerAnnotate_MutexDestroy(...) __tsan_mutex_destroy(__VA_ARGS__)
	#define DMibSanitizerAnnotate_MutexPreLock(...) __tsan_mutex_pre_lock(__VA_ARGS__)
	#define DMibSanitizerAnnotate_MutexPostLock(...) __tsan_mutex_post_lock(__VA_ARGS__)
	#define DMibSanitizerAnnotate_MutexPreUnlock(...) __tsan_mutex_pre_unlock(__VA_ARGS__)
	#define DMibSanitizerAnnotate_MutexPostUnlock(...) __tsan_mutex_post_unlock(__VA_ARGS__)
	#define DMibSanitizerAnnotate_MutexPreSignal(...) __tsan_mutex_pre_signal(__VA_ARGS__)
	#define DMibSanitizerAnnotate_MutexPostSignal(...) __tsan_mutex_post_signal(__VA_ARGS__)
	#define DMibSanitizerAnnotate_MutexPreDivert(...) __tsan_mutex_pre_divert(__VA_ARGS__)
	#define DMibSanitizerAnnotate_MutexPostDivert(...) __tsan_mutex_post_divert(__VA_ARGS__)

	#define DMibSanitizerAnnotate_Acquire(...) __tsan_acquire(__VA_ARGS__)
	#define DMibSanitizerAnnotate_Release(...) __tsan_release(__VA_ARGS__)
#else
	#define DMibSanitizerAnnotate_MutexCreate(...)
	#define DMibSanitizerAnnotate_MutexDestroy(...)
	#define DMibSanitizerAnnotate_MutexPreLock(...)
	#define DMibSanitizerAnnotate_MutexPostLock(...)
	#define DMibSanitizerAnnotate_MutexPreUnlock(...)
	#define DMibSanitizerAnnotate_MutexPostUnlock(...)
	#define DMibSanitizerAnnotate_MutexPreSignal(...)
	#define DMibSanitizerAnnotate_MutexPostSignal(...)
	#define DMibSanitizerAnnotate_MutexPreDivert(...)
	#define DMibSanitizerAnnotate_MutexPostDivert(...)

	#define DMibSanitizerAnnotate_Acquire(...)
	#define DMibSanitizerAnnotate_Release(...)
#endif

#ifdef DMibSanitizerEnabled_Address
	#ifndef DCompiler_MSVC
		#include <sanitizer/asan_interface.h>
	#endif

	#define DMibSanitizerAnnotate_PoisonMemoryRegion(...) __asan_poison_memory_region(__VA_ARGS__);
	#define DMibSanitizerAnnotate_UnpoisonMemoryRegion(...) __asan_unpoison_memory_region(__VA_ARGS__);
#else
	#define DMibSanitizerAnnotate_PoisonMemoryRegion(...) (void)0
	#define DMibSanitizerAnnotate_UnpoisonMemoryRegion(...) (void)0
#endif

#ifdef __clang_analyzer__
	void fg_UnconsumeUndefined(void *);
	template <typename tf_CObject>
	void fg_Unconsume(tf_CObject &o_Value)
	{
		fg_UnconsumeUndefined(&o_Value);
	}
	#define DMibMovedFromValid(d_Value) fg_Unconsume(d_Value)
	#define DMibClangAnalyzerWorkaround
#else
	#define DMibMovedFromValid(d_Value)
#endif

#if defined(__apple_build_version__) && (__apple_build_version__ < 17000000)
#define DCompiler_Workaround_Apple_clang
#endif
