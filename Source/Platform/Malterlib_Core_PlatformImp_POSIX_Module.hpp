// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

using namespace NMib;

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mman.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#include <uuid/uuid.h>
#include <poll.h>

#include <sys/param.h>

#include "Malterlib_Core_PlatformImp_POSIX.h"


// *************************************************************************************************************************
// POSIX Dynamic Library Implementation
// *************************************************************************************************************************

using FMalterlibLibraryFunc = void calling_convention_c ();

namespace
{
#if defined(DMibSanitizerEnabled_Address)
	static constexpr int g_ExtraDlOpenFlags = RTLD_NODELETE;
#else
	static constexpr int g_ExtraDlOpenFlags = 0;
#endif
}

void * NSys::fg_LoadLibrary(CFStr256 const& _Library)
{
	if (!_Library.f_IsEmpty())
	{
		auto Library = _Library;
		void *pRet = dlopen(Library.f_GetStr(), RTLD_NOW | RTLD_LOCAL | g_ExtraDlOpenFlags);
		if (pRet)
		{
			void (*pMalterlibLoadLibraryExternal)();
			pMalterlibLoadLibraryExternal = (FMalterlibLibraryFunc*)NSys::fg_GetLibrarySymbol(pRet, "IdsLoadLibraryExternal");
			if (pMalterlibLoadLibraryExternal)
				pMalterlibLoadLibraryExternal();
		}
		//else
		//	DMibDTraceSafe("{}\n", fg_FormatErrno(CFStr256::CFormat("dlopen('{}') {}") << _Library << dlerror(), errno));
		return pRet;
	}
	else
	{
		return dlopen(nullptr, RTLD_NOW);
	}
}

void *NSys::fg_LoadLibrary(const CStr& _Library)
{
	if (!_Library.f_IsEmpty())
	{
		auto Library = _Library;
		void *pRet = dlopen(Library.f_GetStr(), RTLD_NOW | RTLD_LOCAL | g_ExtraDlOpenFlags);
		if (pRet)
		{
			void (*pMalterlibLoadLibraryExternal)();
			pMalterlibLoadLibraryExternal = (FMalterlibLibraryFunc*)NSys::fg_GetLibrarySymbol(pRet, "IdsLoadLibraryExternal");
			if (pMalterlibLoadLibraryExternal)
				pMalterlibLoadLibraryExternal();
		}
		//else
		//	DMibDTraceSafe("{}\n", fg_FormatErrno(CFStr256::CFormat("dlopen('{}') {} ") << _Library << dlerror(), errno));
		return pRet;
	}
	else
	{
		return dlopen(nullptr, RTLD_NOW);
	}
}

void *NSys::fg_LoadLibrary(const CStrNonTracked &_Library)
{
	if (!_Library.f_IsEmpty())
	{
		auto Library = _Library;
		void *pRet = dlopen(Library.f_GetStr(), RTLD_NOW | RTLD_LOCAL | g_ExtraDlOpenFlags);
		if (pRet)
		{
			void (*pMalterlibLoadLibraryExternal)();
			pMalterlibLoadLibraryExternal = (FMalterlibLibraryFunc*)NSys::fg_GetLibrarySymbol(pRet, "IdsLoadLibraryExternal");
			if (pMalterlibLoadLibraryExternal)
				pMalterlibLoadLibraryExternal();
		}
		//else
		//	DMibDTraceSafe("{}\n", fg_FormatErrno(CFStr256::CFormat("dlopen('{}') {} ") << _Library << dlerror(), errno));
		return pRet;
	}
	else
	{
		return dlopen(nullptr, RTLD_NOW);
	}
}

void NSys::fg_FreeLibrary(void *_pModule)
{
	void (*pMalterlibFreeLibraryExternal)();
	pMalterlibFreeLibraryExternal = (FMalterlibLibraryFunc*)NSys::fg_GetLibrarySymbol(_pModule, "IdsFreeLibraryExternal");
	if (pMalterlibFreeLibraryExternal)
		pMalterlibFreeLibraryExternal();

	dlclose(_pModule);
}

void* NSys::fg_GetLibrarySymbol(void* _pModule, char const* _pSymbol)
{
	return dlsym(_pModule, _pSymbol);
}

