// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	enum EEndian
	{
		EEndian_Native = 0,
		EEndian_Little = 1,
		EEndian_Big	= 2			
	};
}

#include "Malterlib_Core_Platform_Detection.h"
#include "Malterlib_Core_Platform_Config.h"
#include "Malterlib_Core_Platform_BasicTypes.h"

// Compiler
#ifdef DCompiler_MSVC
#	include "Malterlib_Core_Platform_Compiler_MSVC.h"
#endif

#include "Malterlib_Core_Platform_Thread.h"
#include "Malterlib_Core_Platform_Attributes.h"
#include "Malterlib_Core_Platform_Intrinsics.h"
#include "Malterlib_Core_Platform_Compiler.h"
#include "Malterlib_Core_Platform_Platform.h"


// Shortcuts
#ifndef DMibPNoShortCuts
#	define DPtrBits DMibPPtrBits
#	define DInlineActive DMibPInlineActive
#	define DDebugBreak DMibPDebugBreak
#	define DArglistStart DMibPArglistStart
#	define DArglistGetArgPtr DMibPArglistGetArgPtr
#	define DArglistNextArg DMibPArglistNextArg
#	define DMemoryCacheLineSize DMibPMemoryCacheLineSize
#	define DFile DMibPFile
#	define DLine DMibPLine
#	ifdef DMibDebug
#		define DDebug DMibDebug
#	endif
#endif


// Sanity checks
static_assert(sizeof(ch8) == 1);
static_assert(sizeof(ch16) == 2);
static_assert(sizeof(ch32) == 4);
static_assert(sizeof(int8) == 1);
static_assert(sizeof(int8) == 1);
static_assert(sizeof(int16) == 2);
static_assert(sizeof(int32) == 4);
static_assert(sizeof(int64) == 8);
static_assert(sizeof(uint8) == 1);
static_assert(sizeof(uint16) == 2);
static_assert(sizeof(uint32) == 4);
static_assert(sizeof(uint64) == 8);
static_assert(sizeof(void*) == sizeof(mint));
static_assert(sizeof(void*) == sizeof(smint));


