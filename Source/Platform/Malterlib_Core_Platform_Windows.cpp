// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_Platform_Windows.h"
#include <Windows.h>
#include <Mib/Core/PlatformSpecific/WindowsOptional>

#include <utility>

[[noreturn]] void NMib::fg_NoReturn()
{
	std::unreachable();
}

namespace NMib::NPlatform
{
	bool fg_IsVista()
	{
		if (NLocal::g_VersionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT &&
				(NLocal::g_VersionInfo.dwMajorVersion >= 6 ) )
			return TRUE;

		return FALSE;
	}

	constinit static NAtomic::TCAtomic<bool> gs_ShuttingDown = false;

	void fg_ReportIsShuttingDown()
	{
		gs_ShuttingDown.f_Exchange(true);
	}

	bool fg_IsShuttingDown()
	{
		if (gs_ShuttingDown.f_Load())
			return true;

		return GetSystemMetrics(SM_SHUTTINGDOWN) != 0;
	}
}
