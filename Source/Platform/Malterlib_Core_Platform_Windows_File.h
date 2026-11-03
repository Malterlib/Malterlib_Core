// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Windows.h>

namespace NMib
{
	namespace NFile
	{
		namespace NPlatform
		{
			// NOLINTBEGIN(malterlib-conceptual-prefix): FILETIME is a struct, which the check takes for a function type by its name
			NTime::CTimeSpan fg_Win32_FileTimeToMalterlibTimeSpan(FILETIME const &_FileTime);
			NTime::CTime fg_Win32_FileTimeToMalterlibTime(FILETIME const &_FileTime);
			void fg_Win32_MalterlibTimeToFileTime(NTime::CTime const &_Time, FILETIME &o_FileTime);
			// NOLINTEND(malterlib-conceptual-prefix)
			void fg_MalterlibTimeToSystemTime(NTime::CTimeConvert::CDateTime const &_DateTime, SYSTEMTIME &o_SysTime);
			void fg_MalterlibTimeToSystemTime(NTime::CTime const &_Time, SYSTEMTIME &o_SysTime);
			NTime::CTime fg_SystemTimeToMalterlibTime(SYSTEMTIME const &_SysTime);
		}
	}
}

