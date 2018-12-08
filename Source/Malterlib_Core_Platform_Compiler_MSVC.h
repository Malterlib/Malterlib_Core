// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#if defined(__EDG__)
// This is the intellisense compiler
#elif _MSC_VER == 1916
#	if _MSC_FULL_VER < 191627024
#		pragma message("Current compiler version: " DMibStringize(_MSC_FULL_VER))
#		error "Compiler version is not 191627024, please install the approprita service pack of Visual Studio 2017"
#	endif
#else
#	pragma message("Compiler version " DMibStringize(_MSC_VER) "  not supported")
#	error "Compiler version is not supported"
#endif


#define _SCL_SECURE_NO_WARNINGS 1
#define _SECURE_SCL 0
#define _HAS_ITERATOR_DEBUGGING 0
#define _CRT_SECURE_NO_WARNINGS 1
#ifndef _CPPUNWIND
#	define _CPPUNWIND 1
#endif
#define _SECURE_SCL 0
#define _HAS_ITERATOR_DEBUGGING 0


#define __has_feature(d_Feature) 0


#define _XKEYCHECK_H

#define DMibNoAggregateConstexpr

#define DCompiler_MSVC_Workaround 1

#ifdef _DEBUG
#pragma warning(disable:4714) // force inline not inlined
#else
#pragma warning(disable:4714) // force inline not inlined
//#pragma warning(default:4714) // force inline not inlined
//#pragma warning(2:4714) // force inline not inlined
#endif
#pragma warning(disable:4100) // unreferenced formal parameter
#pragma warning(disable:4127) // conditional expression is constant
#pragma warning(disable:4244) // conversion from 'mint' to 'const unsigned int', possible loss of data
#pragma warning(disable:4146) // unary minus operator applied to unsigned type, result still unsigned
#pragma warning(disable:4200) // nonstandard extension used : zero-sized array in struct/union
#pragma warning(disable:4211) // nonstandard extension used : redefined extern to static
#pragma warning(disable:4505) // unreferenced local function has been removed
#pragma warning(disable:4221) // no public symbols found; archive member will be inaccessible
#pragma warning(disable:4702) // unreachable code
#pragma warning(disable:4503) // decorated name length exceeded, name was truncated
#pragma warning(disable:4512) // assignment operator could not be generated
#pragma warning(disable:4800) // forcing value to bool 'true' or 'false' (performance warning)
#pragma warning(disable:4201) // nonstandard extension used : nameless struct/union
#pragma warning(disable:4481) // nonstandard extension used: override specifier 'override'
#pragma warning(disable:4706) // assignment within conditional expression
#pragma warning(disable:4530) // exceptions
#pragma warning(disable:4355) // this used in member initialization list
#pragma warning(disable:4521) // multiple copy constructors specified
#pragma warning(disable:4522) // multiple assignment operators specified
#pragma warning(disable:4822) // Undefined functions in local classes
#pragma warning(disable:4456)
#pragma warning(disable:4456) // Hides previous local declaration
#pragma warning(disable:4595) // non-member operator new or delete functions may not be declared inline
#pragma warning(disable:4742) // '`string'' has different alignment in -- To workaround seeming bug in microsoft PGO linker


#ifdef __EDG__

#pragma warning(disable:1684)
#pragma warning(disable:383)
#pragma warning(disable:981)
#pragma warning(disable:1599)
#pragma warning(disable:424)
#pragma warning(disable:1418)
#pragma warning(disable:444)
#pragma warning(disable:193)


#endif

#if _MSC_FULL_VER == 170051025
#pragma warning(disable:4554)
#endif

