// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_Platform_Windows.h"
#include <Windows.h>
#include <Mib/Core/PlatformSpecific/WindowsOptional>

namespace NMib
{
	namespace NPlatform
	{
		bint fg_IsVista()
		{
			if (NLocal::g_VersionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT && 
					(NLocal::g_VersionInfo.dwMajorVersion >= 6 ) )
				return TRUE;

			return FALSE;
		}
	}
}
