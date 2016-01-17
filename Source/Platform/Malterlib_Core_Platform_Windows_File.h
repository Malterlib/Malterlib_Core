// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Windows.h>

namespace NMib
{
	namespace NFile
	{
		namespace NPlatform
		{
			NTime::CTimeSpan fg_Win32_FileTimeToMalterlibTimeSpan(FILETIME &_FileTime);
			NTime::CTime fg_Win32_FileTimeToMalterlibTime(FILETIME &_FileTime);
			void fg_Win32_MalterlibTimeToFileTime(const NTime::CTime &_Time, FILETIME &_FileTime);
			void fg_MalterlibTimeToSystemTime(const NTime::CTime &_Time, SYSTEMTIME &_SysTime);
		}
	}
}

