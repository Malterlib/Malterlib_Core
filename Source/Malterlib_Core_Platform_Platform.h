// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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
		static CCodeAddressFunction* ms_pFunction;
		static CCodeAddressFile* ms_pFile;
		static CCodeAddressLine* ms_pLine;
	};
	typedef CMibCodeAddressType* CMibCodeAddress;
#else
	typedef void* CMibCodeAddress;
#endif


// Error line formatting
#if defined(DPlatformFamily_OSX) || defined(DPlatformFamily_Linux)
#	define DMibPFileLineFormat "{}:{}:"
#	define DMibPFileLineFormatIndent ""
#	define DMibPFileLineColumnFormat "{}:{}:{}:"
#	define DMibPFormatIDELocation_Helper1(d_Value) #d_Value
#	define DMibPFormatIDELocation_Helper0(d_Value) DMibPFormatIDELocation_Helper1(d_Value)
#	define DMibPFormatIDELocation(d_Value) DMibPFile "(" DMibPFormatIDELocation_Helper0(DMibPLine) "): " d_Value
#elif defined(DPlatformFamily_Windows)
#	define DMibPFileLineFormat "{}({}):"
#	define DMibPFileLineFormatIndent "\t"
#	define DMibPFileLineColumnFormat "{}({},{}):"
#	define DMibPFormatIDELocation_Helper1(d_Value) #d_Value
#	define DMibPFormatIDELocation_Helper0(d_Value) DMibPFormatIDELocation_Helper1(d_Value)
#	define DMibPFormatIDELocation(d_Value) DMibPFile "(" DMibPFormatIDELocation_Helper0(DMibPLine) "): " d_Value
#else
#	error "Implement this"
#endif



// Main
#if defined(DPlatformFamily_OSX) || defined(DPlatformFamily_Linux) || defined(DPlatformFamily_Emscripten)
#	define DMibPMain int main(){return NMib::fg_GetSys()->f_RunApplication();}
#elif defined(DPlatformFamily_Windows)
#	ifdef DPConsole
#		define DMibPMain int __cdecl wmain(int argc, wchar_t *argv[], wchar_t *envp[]){return NMib::fg_GetSys()->f_RunApplication();} int __stdcall wWinMain(struct HINSTANCE__ * hInstance, struct HINSTANCE__ * hPrevInstance, wchar_t *lpCmdLine,int nShowCmd){;} int __cdecl main(int argc, wchar_t *argv[]){} int __stdcall WinMain(struct HINSTANCE__ * hInstance, struct HINSTANCE__ * hPrevInstance, char *lpCmdLine,int nShowCmd){;}
#	else
#		define DMibPMain int __stdcall wWinMain(struct HINSTANCE__ * hInstance, struct HINSTANCE__ * hPrevInstance, wchar_t *lpCmdLine,int nShowCmd){return NMib::fg_GetSys()->f_RunApplication();} int __cdecl wmain(int argc, wchar_t *argv[], wchar_t *envp[]){} int __cdecl main(int argc, wchar_t *argv[]){} int __stdcall WinMain(struct HINSTANCE__ * hInstance, struct HINSTANCE__ * hPrevInstance, char *lpCmdLine,int nShowCmd){;}
#	endif
#else
#	error "Implement this"
#endif


// New line
#if defined(DPlatformFamily_OSX) || defined(DPlatformFamily_Linux) || defined(DPlatformFamily_Emscripten)
#	define DMibNewLine "\n"
#elif defined(DPlatformFamily_Windows)
#	define DMibNewLine "\r\n"
#else
#	error "Implement this"
#endif


