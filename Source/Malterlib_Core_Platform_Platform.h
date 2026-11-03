// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

// Code address
#ifdef DDebugger_lldb
struct CMibCodeAddressType
{
	struct CCodeAddressFunction
	{
	};

	struct CCodeAddressFile
	{
	};

	struct CCodeAddressLine
	{
	};

	static CCodeAddressFunction *fs_Debug_Function();
	static CCodeAddressFile *fs_Debug_File();
	static CCodeAddressLine *fs_Debug_Line();
};

using CMibCodeAddress = CMibCodeAddressType *;
#else
using CMibCodeAddress = void *;
#endif

// Error line formatting
#if defined(DPlatformFamily_macOS) || defined(DPlatformFamily_Linux)
#	define DMibPFileLineFormat "{}:{}:"
#	define DMibPFileLineFormatIndent ""
#	define DMibPFileLineColumnFormat "{}:{}:{}:"
#	define DMibPFormatIDELocation_Helper1(d_Value) #d_Value
#	define DMibPFormatIDELocation_Helper0(d_Value) DMibPFormatIDELocation_Helper1(d_Value)
#	define DMibPFormatIDELocation(d_Value) DMibPFile "(" DMibPFormatIDELocation_Helper0(DMibPLine) "): " d_Value
#elif defined(DPlatformFamily_Windows)
#	define DMibPFileLineFormat "{}({}):"
#	define DMibPFileLineFormatIndent "    "
#	define DMibPFileLineColumnFormat "{}({},{}):"
#	define DMibPFormatIDELocation_Helper1(d_Value) #d_Value
#	define DMibPFormatIDELocation_Helper0(d_Value) DMibPFormatIDELocation_Helper1(d_Value)
#	define DMibPFormatIDELocation(d_Value) DMibPFile "(" DMibPFormatIDELocation_Helper0(DMibPLine) "): " d_Value
#else
#	error "Implement this"
#endif

// Main
#if defined(DPlatformFamily_macOS) || defined(DPlatformFamily_Linux) || defined(DPlatformFamily_Emscripten)
#	define DMibPMain int main(){return NMib::fg_GetSys()->f_RunApplication();}
#elif defined(DPlatformFamily_Windows)
#	ifdef DPConsole
#		define DMibPMain int __cdecl wmain(int _nArguments, wchar_t *_Arguments[], wchar_t *_Environment[]){return NMib::fg_GetSys()->f_RunApplication();}
#	else
#		define DMibPMain \
			/* NOLINTNEXTLINE(malterlib-type-naming): the name in the Windows headers */ \
			int __stdcall wWinMain(struct HINSTANCE__ *_pInstance, struct HINSTANCE__ *_pPreviousInstance, wchar_t *_pCommandLine, int _ShowCommand) \
			{ \
				return NMib::fg_GetSys()->f_RunApplication(); \
			}
#	endif
#else
#	error "Implement this"
#endif

// New line
#if defined(DPlatformFamily_macOS) || defined(DPlatformFamily_Linux) || defined(DPlatformFamily_Emscripten)
#	define DMibNewLine "\n"
#elif defined(DPlatformFamily_Windows)
#	define DMibNewLine "\r\n"
#else
#	error "Implement this"
#endif

