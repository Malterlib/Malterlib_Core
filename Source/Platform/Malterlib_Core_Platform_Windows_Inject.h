// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Windows.h>

namespace NMib
{
	namespace NPlatform
	{
		HRESULT fg_PatchIAT(HMODULE _Module, CHAR const *_pImportedModuleName, CHAR const *_pImportedProcName, void *_pHookingProc, void **_pOriginalProc);
		HRESULT fg_DumpIATs(HMODULE _Module);
		HRESULT fg_PatchDIAT(HMODULE _Module, CHAR const *_pImportedModuleName, CHAR const *_pImportedProcName, void *_pHookingProc, void **_pOriginalProc);
#if !defined(DArchitecture_arm64)
		enum EInjectDllResult
		{
			EInjectDllResult_Failed = 0
			, EInjectDllResult_Done
			, EInjectDllResult_Delayed
		};

		EInjectDllResult fg_InjectDLL(DWORD _ProcessID, DWORD _ThreadID, WCHAR const *_pDLLName, NStr::CStr &_Error);
		EInjectDllResult fg_InjectDLL(HANDLE _Process, HANDLE _Thread, WCHAR const *_pDLLName, NStr::CStr &_Error);
		BOOL fg_DejectDLL(DWORD _ProcessID, WCHAR const *_pDLLName, NStr::CStr &_Error);
#endif
	}
}
