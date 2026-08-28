// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Atomic/Atomic>
#include <Windows.h>

extern VOID (WINAPI *g_fOrgExitProcess)(__in  UINT _ExitCode);
extern BOOL (WINAPI *g_fOrgTerminateProcess)(__in  HANDLE _hProcess, __in  UINT _ExitCode);
extern NMib::NAtomic::TCAtomic<smint> g_bDoneMalterlibInitAll;
extern HINSTANCE g_hDllInstance;
extern bool g_bIsDll;

namespace NMib::NPlatform
{
	void fg_GenerateExcetionHandler(void *_pData, LONG (*_pCallback)(struct _EXCEPTION_POINTERS *_pExceptionInfo, void *_pData));

	inline_small bool fg_IsGoodStackPtr(void *_pAddr, umint _Len, umint _StackStart, umint _StackEnd)
	{
		umint StackStart = _StackStart;
		umint StackEnd = _StackEnd;
		umint AddrStart = (umint)_pAddr;
		umint AddrEnd = AddrStart + _Len;

		if (AddrEnd < AddrStart)
			return false;

		return AddrEnd <= StackStart && AddrStart >= StackEnd;
	}

	bool fg_IsVista();
	bool fg_ThisThreadOwnsDllLock();
	bool fg_IsShuttingDown();
	void fg_ReportIsShuttingDown();

	// Starts the hidden top-level window that hears the session ending and reports it, once per
	// process; stopped with the platform. Broadcast messages never reach a message-only window,
	// so a process without a window of its own has no other way to be told
	void fg_EnsureEndSessionReporting();
	void fg_StopEndSessionReporting();
}
