// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Atomic/Atomic>
#include <Windows.h>

extern VOID (WINAPI *g_fOrgExitProcess)(__in UINT _ExitCode);
extern BOOL (WINAPI *g_fOrgTerminateProcess)(__in HANDLE _pProcess, __in UINT _ExitCode);
extern NMib::NAtomic::TCAtomic<smint> g_bDoneMalterlibInitAll;
extern HINSTANCE g_pDllInstanceHandle;
extern bool g_bIsDll;

namespace NMib::NFile
{
	struct CUniqueFileIdentifier;
}

namespace NMib::NPlatform
{
	void fg_GenerateExcetionHandler(void *_pData, LONG (*_fCallback)(struct _EXCEPTION_POINTERS *_pExceptionInfo, void *_pData));

	inline_small bool fg_IsGoodStackPtr(void *_pAddr, umint _Len, umint _StackStart, umint _StackEnd);

	bool fg_IsVista();
	bool fg_ThisThreadOwnsDllLock();
	bool fg_IsShuttingDown();
	void fg_ReportIsShuttingDown();
	void fg_EnsureEndSessionReporting();

	bool fg_SetPosixDeleteDisposition(HANDLE _pFile);
	bool fg_GetUniqueFileIdentifier(HANDLE _pFile, NMib::NFile::CUniqueFileIdentifier &o_Identifier);
}

#include "Malterlib_Core_Platform_Windows.hpp"
