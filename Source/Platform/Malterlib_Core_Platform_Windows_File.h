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
			NTime::CTimeSpan fg_Win32_FileTimeToMalterlibTimeSpan(FILETIME const &_FileTime);
			NTime::CTime fg_Win32_FileTimeToMalterlibTime(FILETIME const &_FileTime);
			void fg_Win32_MalterlibTimeToFileTime(NTime::CTime const &_Time, FILETIME &o_FileTime);
			void fg_MalterlibTimeToSystemTime(NTime::CTimeConvert::CDateTime const &_DateTime, SYSTEMTIME &o_SysTime);
			void fg_MalterlibTimeToSystemTime(NTime::CTime const &_Time, SYSTEMTIME &o_SysTime);
			NTime::CTime fg_SystemTimeToMalterlibTime(SYSTEMTIME const &_SysTime);
		}
	}
}

