// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Windows.h>

namespace NMib
{
	namespace NPlatform
	{
		HRESULT fg_PatchIAT(HMODULE _hMod, CHAR const *_pImportedModuleName, CHAR const *_pImportedProcName, void *_pHookingProc, void **_pOriginalProc);
		HRESULT fg_DumpIATs(HMODULE _hMod);
		HRESULT fg_PatchDIAT(HMODULE _hMod, CHAR const *_pImportedModuleName, CHAR const *_pImportedProcName, void *_pHookingProc, void **_pOriginalProc);
#if !defined(DArchitecture_arm64)
		enum EInjectDllResult
		{
			EInjectDllResult_Failed = 0
			, EInjectDllResult_Done
			, EInjectDllResult_Delayed
		};

		EInjectDllResult fg_InjectDLL(DWORD _ProcessID, DWORD _ThreadID, const WCHAR *_pDLLName, NStr::CStr &_Error);
		EInjectDllResult fg_InjectDLL(HANDLE _hProcess, HANDLE _hThread, const WCHAR *_pDLLName, NStr::CStr &_Error);
		BOOL fg_DejectDLL(DWORD _ProcessID, const WCHAR *_pDLLName, NStr::CStr &_Error);
#endif
	}
}
