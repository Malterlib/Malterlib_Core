// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Windows.h>

namespace NMib
{
	namespace NPlatform
	{
		HRESULT fg_PatchIAT(HMODULE _pModule, CHAR const *_pImportedModuleName, CHAR const *_pImportedProcName, void *_pHookingProc, void **_pOriginalProc);
		HRESULT fg_DumpIATs(HMODULE _pModule);
		HRESULT fg_PatchDIAT(HMODULE _pModule, CHAR const *_pImportedModuleName, CHAR const *_pImportedProcName, void *_pHookingProc, void **_pOriginalProc);
#if !defined(DArchitecture_arm64)
		enum EInjectDllResult
		{
			EInjectDllResult_Failed = 0
			, EInjectDllResult_Done
			, EInjectDllResult_Delayed
		};

		EInjectDllResult fg_InjectDLL(DWORD _ProcessID, DWORD _ThreadID, WCHAR const *_pDLLName, NStr::CStr &_Error);
		EInjectDllResult fg_InjectDLL(HANDLE _pProcess, HANDLE _pThread, WCHAR const *_pDLLName, NStr::CStr &_Error);
		BOOL fg_DejectDLL(DWORD _ProcessID, WCHAR const *_pDLLName, NStr::CStr &_Error);
#endif
	}
}
