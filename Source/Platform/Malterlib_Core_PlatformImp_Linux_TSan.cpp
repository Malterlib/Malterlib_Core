// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Core_PlatformImp_Linux.h"

#include <unistd.h>
#include <sanitizer/tsan_interface.h>

namespace NMib
{
	extern mint g_bCreatedSystem;
}

DMibSuppressThreadSanitizer inline_never assure_used void __tsan_check_forked_parent_or_child()
{
	if (!NMib::g_bCreatedSystem)
		return;

	auto &Sys = *((CSystemLinux *)NMib::fg_GetSys());
	void *Current = pthread_getspecific(Sys.m_ThreadDestructionHook);
	if (Current)
	{
		if (Current == (void *)(mint)getpid())
			__tsan_forked_parent();
		else
			__tsan_forked_child();
	}
}

void fg_Malterlib_MakeActive_TSan()
{
}

