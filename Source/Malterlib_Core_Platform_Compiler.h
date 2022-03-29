// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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

// Type traits
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	define DMibPHasAssignmentOperator(d_Type) __has_assign(d_Type)
#	define DMibPHasCopyConstructor(d_Type) __has_copy(d_Type)
#	define DMibPHasNothrowAssignmentOperator(d_Type) __has_nothrow_assign(d_Type)
#	define DMibPHasNothrowDefaultConstructor(d_Type) __has_nothrow_constructor(d_Type)
#	define DMibPHasNothrowCopyConstructor(d_Type) __has_nothrow_copy(d_Type)
#	define DMibPHasTrivialAssignmentOperator(d_Type) __has_trivial_assign(d_Type)
#	define DMibPHasTrivialDefaultConstructor(d_Type) __has_trivial_constructor(d_Type)
#	define DMibPHasTrivialCopyConstructor(d_Type) __has_trivial_copy(d_Type)
#	define DMibPHasTrivialDestructor(d_Type) __has_trivial_destructor(d_Type)
#	define DMibPHasUserDestructor(d_Type) __has_user_destructor(d_Type)
#	define DMibPHasVirtualDestructor(d_Type) __has_virtual_destructor(d_Type)

#	define DMibPIsAbstractType(d_Type) __is_abstract(d_Type)
#	define DMibPIsBaseOfType(_BaseType, _DerivedType) __is_base_of(_BaseType,_DerivedType)
#	define DMibPIsClassType(d_Type) __is_class(d_Type)
#	define DMibPIsTypeConvertibleToType(_FromType, _ToType) __is_convertible_to(_FromType, _ToType)
#	define DMibPIsEmptyType(d_Type) __is_empty(d_Type)
#	define DMibPIsEnumType(d_Type) __is_enum(d_Type)
#	define DMibPUnderlyingType(d_Type) __underlying_type(d_Type)
#	define DMibPIsPODType(d_Type) __is_pod(d_Type)
#	define DMibPIsPolymorphicType(d_Type) __is_polymorphic(d_Type)
#	define DMibPIsUnionType(d_Type) __is_union(d_Type)

#elif defined(DCompiler_MSVC)
#	define DMibPHasAssignmentOperator(d_Type) __has_assign(d_Type)
#	define DMibPHasCopyConstructor(d_Type) __has_copy(d_Type)
#	define DMibPHasNothrowAssignmentOperator(d_Type) __has_nothrow_assign(d_Type)
#	define DMibPHasNothrowDefaultConstructor(d_Type) __has_nothrow_constructor(d_Type)
#	define DMibPHasNothrowCopyConstructor(d_Type) __has_nothrow_copy(d_Type)
#	define DMibPHasTrivialAssignmentOperator(d_Type) __has_trivial_assign(d_Type)
#	define DMibPHasTrivialDefaultConstructor(d_Type) __has_trivial_constructor(d_Type)
#	define DMibPHasTrivialCopyConstructor(d_Type) __has_trivial_copy(d_Type)
#	define DMibPHasTrivialDestructor(d_Type) __has_trivial_destructor(d_Type)
#	define DMibPHasUserDestructor(d_Type) __has_user_destructor(d_Type)
#	define DMibPHasVirtualDestructor(d_Type) __has_virtual_destructor(d_Type)

#	define DMibPIsAbstractType(d_Type) __is_abstract(d_Type)
#	define DMibPIsBaseOfType(d_BaseType, d_DerivedType) __is_base_of(d_BaseType, d_DerivedType)
#	define DMibPIsClassType(d_Type) __is_class(d_Type)
#	define DMibPIsTypeConvertibleToType(d_FromType, d_ToType) __is_convertible_to(d_FromType, d_ToType)
#	define DMibPIsEmptyType(d_Type) __is_empty(d_Type)
#	define DMibPIsEnumType(d_Type) __is_enum(d_Type)
#	define DMibPUnderlyingType(d_Type) __underlying_type(d_Type)
#	define DMibPIsPODType(d_Type) __is_pod(d_Type)
#	define DMibPIsPolymorphicType(d_Type) __is_polymorphic(d_Type)
#	define DMibPIsUnionType(d_Type) __is_union(d_Type)

#else
#	error "Implement this"
#endif

#if defined(DCompiler_clang) || defined(DCompiler_gcc)

_LIBCPP_BEGIN_NAMESPACE_STD
	class weak_equality;
	class strong_equality;
	class partial_ordering;
	class weak_ordering;
	class strong_ordering;
_LIBCPP_END_NAMESPACE_STD

#	if __has_feature(cxx_rtti)
#		define DMibPTypeName(x) (typeid(x).name())
#		include <typeinfo>
		namespace NMib
		{
			namespace NTypeInfo
			{
				
				typedef std::type_info CTypeInfo;
				typedef std::bad_cast CExceptionBadCast;
 				typedef std::bad_typeid CExceptionTypeId;
			}
		}
#	endif
#elif defined(DCompiler_MSVC)

namespace std
{
	struct weak_equality;
	struct strong_equality;
	struct partial_ordering;
	struct weak_ordering;
	struct strong_ordering;
}

#	if DMibPSupportExceptions
#		include <exception>
#	endif

#	if DMibPSupportTypeinfo
#		include <typeinfo>
		namespace NMib
		{
			namespace NTypeInfo
			{
				typedef type_info CTypeInfo;
#		if DMibPSupportExceptions
				typedef std::bad_typeid CExceptionTypeId;
				typedef std::bad_cast CExceptionBadCast;
#		endif
			}
		}
#	endif

#else
#	error "Implement this"
#endif

// Assume
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	define assume(x) ((void)0)
#elif defined(DCompiler_MSVC)
#	define assume(d_ToAssume) (__assume(d_ToAssume))
#else
#	error "Implement this"
#endif

// Arglist
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	include <stdarg.h>
	typedef va_list CMibArgList;
#	define DMibPArgListStart(_ArgList, _PrevArg) va_start(_ArgList, _PrevArg)
#	define DMibPArgListNextArg(_ArgList, _ArgType) va_arg(_ArgList, _ArgType)
#	define DMibPArgListEnd(_ArgList) va_end(_ArgList)
#elif defined(DCompiler_MSVC)
#	if defined(DArchitecture_x86)
#		define DMibPArglistIntSizeOf(n)   ( (sizeof(n) + sizeof(int) - 1) & (~(sizeof(int) - 1)) )
#		define DMibPArglistIntAlign(n)   ( (n + sizeof(int) - 1) & (~(sizeof(int) - 1)) )
		typedef void * CMibArgList;
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
		typedef char * CMibArgList;
		extern "C" void __cdecl __va_start(CMibArgList *, ...);
#		define DMibPArgListStart(_ArgList, _PrevArg) ( __va_start(&_ArgList, _PrevArg) )
#		define DMibPArgListNextArg(_ArgList, _ArgType) \
			( ( sizeof(_ArgType) > sizeof(__int64) || ( sizeof(_ArgType) & (sizeof(_ArgType) - 1) ) != 0 ) \
				? **(_ArgType **)( ( _ArgList += sizeof(__int64) ) - sizeof(__int64) ) \
				:  *(_ArgType  *)( ( _ArgList += sizeof(__int64) ) - sizeof(__int64) ) )
#		define DMibPArgListEnd(_ArgList) ((void)0)
#	else
#		include <stdarg.h>
		typedef va_list CMibArgList;
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

// Used for working around bugs in UBSAN
#if (defined(DCompiler_clang) || defined(DCompiler_gcc)) && defined(DPlatformFamily_Linux)
#	define DMibSuppressUndefinedSanitizerLinux __attribute__((no_sanitize("undefined")))
#else
#	define DMibSuppressUndefinedSanitizerLinux
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

#if defined(__has_feature)
#	if __has_feature(undefined_behavior_sanitizer)
#		define DMibSanitizerEnabled_UndefinedBehavior
#		define DMibSanitizerEnabled
#	endif
#	if __has_feature(address_sanitizer)
#		define DMibSanitizerEnabled_Address
#		define DMibSanitizerEnabled
#	endif
#	if __has_feature(thread_sanitizer)
#		define DMibSanitizerEnabled_Thread
#		define DMibSanitizerEnabled
#	endif
#endif

#if defined(DArchitecture_arm64) || defined(DArchitecture_arm64e) || !defined(DPlatformFamily_Linux) || !defined(DMibSanitizerEnabled_UndefinedBehavior)
	#define DMibWorkaroundUBSanSectionErrors
#else
	#define DMibWorkaroundUBSanSectionErrors __attribute__ ((section("ubsan_broken_section")))
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
	#include <sanitizer/asan_interface.h>

	#define DMibSanitizerAnnotate_PoisonMemoryRegion(...) __asan_poison_memory_region(__VA_ARGS__);
	#define DMibSanitizerAnnotate_UnpoisonMemoryRegion(...) __asan_unpoison_memory_region(__VA_ARGS__);
#else
	#define DMibSanitizerAnnotate_PoisonMemoryRegion(...) (void)0
	#define DMibSanitizerAnnotate_UnpoisonMemoryRegion(...) (void)0
#endif
