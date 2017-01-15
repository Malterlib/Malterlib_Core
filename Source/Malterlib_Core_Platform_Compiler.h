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
#	define DMibDeprecatedSupressStart	_Pragma("clang diagnostic push") \
										_Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"")
#	define DMibDeprecatedSupressStop	_Pragma("clang diagnostic pop")
#else
#	define DMibDeprecated
#	define DMibDeprecatedSupressStart
#	define DMibDeprecatedSupressStop
#endif

// Type traits
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	define DMibPHasAssignmentOperator(_Type) __has_assign(_Type)
#	define DMibPHasCopyConstructor(_Type) __has_copy(_Type)
#	define DMibPHasNothrowAssignmentOperator(_Type) __has_nothrow_assign(_Type)
#	define DMibPHasNothrowDefaultConstructor(_Type) __has_nothrow_constructor(_Type)
#	define DMibPHasNothrowCopyConstructor(_Type) __has_nothrow_copy(_Type)
#	define DMibPHasTrivialAssignmentOperator(_Type) __has_trivial_assign(_Type)
#	define DMibPHasTrivialDefaultConstructor(_Type) __has_trivial_constructor(_Type)
#	define DMibPHasTrivialCopyConstructor(_Type) __has_trivial_copy(_Type)
#	define DMibPHasTrivialDestructor(_Type) __has_trivial_destructor(_Type)
#	define DMibPHasUserDestructor(_Type) __has_user_destructor(_Type)
#	define DMibPHasVirtualDestructor(_Type) __has_virtual_destructor(_Type)

#	define DMibPIsAbstractType(_Type) __is_abstract(_Type)
#	define DMibPIsBaseOfType(_BaseType, _DerivedType) __is_base_of(_BaseType,_DerivedType)
#	define DMibPIsClassType(_Type) __is_class(_Type)
#	define DMibPIsTypeConvertibleToType(_FromType, _ToType) __is_convertible_to(_FromType, _ToType)
#	define DMibPIsEmptyType(_Type) __is_empty(_Type)
#	define DMibPIsEnumType(_Type) __is_enum(_Type)
#	define DMibPIsPODType(_Type) __is_pod(_Type)
#	define DMibPIsPolymorphicType(_Type) __is_polymorphic(_Type)
#	define DMibPIsUnionType(_Type) __is_union(_Type)

#	define DMibPAlignmentOf(d_Type) __alignof__(d_Type)

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
#	define DMibPIsPODType(d_Type) __is_pod(d_Type)
#	define DMibPIsPolymorphicType(d_Type) __is_polymorphic(d_Type)
#	define DMibPIsUnionType(d_Type) __is_union(d_Type)

#	define DMibPAlignmentOf(d_Type) __alignof(d_Type)
#else
#	error "Implement this"
#endif



// Workaround for MSVC constexr missing
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	define constenum(d_Enum) (d_Enum)
#elif defined(DCompiler_MSVC)
#	ifndef constenum
#		define constenum(d_Enum) ((int)d_Enum)
#	endif
#else
#	error "Implement this"
#endif



#if defined(DCompiler_clang) || defined(DCompiler_gcc)
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





// Offset
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	define DMibPOffsetOf(_Type, _Member) ((aint)(&((_Type *)4)->_Member)-4)
#elif defined(DCompiler_MSVC)
#	define DMibPOffsetOf(_Type, _Member) ((aint)(&((_Type *)0)->_Member))
#else
#	error "Implement this"
#endif


// C++14

#if __cplusplus >= 201402L || _MSC_VER >= 1900
#	define DMib_Cxx14 1
#	define DMib_EnableIfDefault
#else
#	define DMib_Cxx14 0
#	define DMib_EnableIfDefault = nullptr
#endif

#ifdef __cpp_noexcept_function_type
#	define DMib_NoexceptFunctionType 1
#else
#	define DMib_NoexceptFunctionType 0
#endif

// Workarounds
#if defined(DCompiler_MSVC)
#	define DMibDecltypeThis
#else
#	define DMibDecltypeThis this->
#endif

