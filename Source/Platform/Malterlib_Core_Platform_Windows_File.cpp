// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_Platform_Windows_File.h"
#include <Windows.h>

namespace NMib
{
	namespace NFile
	{
		namespace NPlatform
		{
			namespace
			{
				constexpr static NMib::NTime::CTime const g_FileTimeBase = NMib::NTime::CTime::fs_Create(237148610522659200, 0);
			}

			NTime::CTimeSpan fg_Win32_FileTimeToMalterlibTimeSpan(FILETIME &_FileTime)
			{
				LARGE_INTEGER Temp;
				Temp.HighPart = _FileTime.dwHighDateTime;
				Temp.LowPart = _FileTime.dwLowDateTime;

				uint64 Nano100 = Temp.QuadPart;
				uint64 nSeconds = Nano100 / 10000000;
				NTime::CTimeSpan FileTimeSpan;
				FileTimeSpan.f_SetSeconds(nSeconds);
				FileTimeSpan.f_SetFraction(fp64(Nano100 % 10000000) / fp64(10000000.0));

				return FileTimeSpan;
			}

			NTime::CTime fg_Win32_FileTimeToMalterlibTime(FILETIME &_FileTime)
			{
				NTime::CTime BaseTime = g_FileTimeBase;

				return BaseTime + fg_Win32_FileTimeToMalterlibTimeSpan(_FileTime);
			}

			void fg_Win32_MalterlibTimeToFileTime(const NTime::CTime &_Time, FILETIME &_FileTime)
			{
				NTime::CTime BaseTime = g_FileTimeBase;
				NTime::CTimeSpan FileTimeSpan = _Time - BaseTime;

				LARGE_INTEGER Temp;
				Temp.QuadPart = FileTimeSpan.f_GetSeconds() * 10000000;
				Temp.QuadPart += (FileTimeSpan.f_GetFraction() * 10000000.0).f_ToInt();

				_FileTime.dwHighDateTime = Temp.HighPart;
				_FileTime.dwLowDateTime = Temp.LowPart;
			}

			void fg_MalterlibTimeToSystemTime(const NTime::CTime &_Time, SYSTEMTIME &_SysTime)
			{
				NTime::CTimeConvert::CDateTime DateTime;
				NTime::CTimeConvert(_Time).f_ExtractDateTime(DateTime);
	
				_SysTime.wYear = DateTime.m_Year;
				_SysTime.wMonth = DateTime.m_Month;
				_SysTime.wDayOfWeek = DateTime.m_DayOfWeek;
				_SysTime.wDay = DateTime.m_DayOfMonth;
				_SysTime.wHour = DateTime.m_Hour;
				_SysTime.wMinute = DateTime.m_Minute;
				_SysTime.wSecond = DateTime.m_Second;
				_SysTime.wMilliseconds = (DateTime.m_Fraction * 1000.0).f_ToInt();
			}
		}
	}
}
