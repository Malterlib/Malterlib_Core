// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Atomic/Atomic>
#include <Windows.h>

extern VOID (WINAPI *g_fOrgExitProcess)(__in  UINT _ExitCode);
extern BOOL (WINAPI *g_fOrgTerminateProcess)(__in  HANDLE _hProcess, __in  UINT _ExitCode);
extern NMib::NAtomic::TCAtomicAggregate<smint> g_bDoneMalterlibInitAll;
extern HINSTANCE g_hDllInstance;
extern bint g_bIsDll;

namespace NMib
{
	namespace NPlatform
	{
		void fg_GenerateExcetionHandler(void *_pData, LONG (*_pCallback)(struct _EXCEPTION_POINTERS *_pExceptionInfo, void *_pData));

		inline_small bint fg_IsGoodStackPtr(void *_pAddr, mint _Len, mint _StackStart, mint _StackEnd)
		{
			mint StackStart = _StackStart;
			mint StackEnd = _StackEnd;
			mint AddrStart = (mint)_pAddr;
			mint AddrEnd = AddrStart + _Len;

			if (AddrEnd < AddrStart)
				return false;

			return AddrEnd <= StackStart && AddrStart >= StackEnd;
		}

		bint fg_IsVista();
		bool fg_ThisThreadOwnsDllLock();

	}
}

