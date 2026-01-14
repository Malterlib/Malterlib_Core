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

			NTime::CTimeSpan fg_Win32_FileTimeToMalterlibTimeSpan(FILETIME const &_FileTime)
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

			NTime::CTime fg_Win32_FileTimeToMalterlibTime(FILETIME const &_FileTime)
			{
				NTime::CTime BaseTime = g_FileTimeBase;

				return BaseTime + fg_Win32_FileTimeToMalterlibTimeSpan(_FileTime);
			}

			void fg_Win32_MalterlibTimeToFileTime(NTime::CTime const &_Time, FILETIME &o_FileTime)
			{
				NTime::CTime BaseTime = g_FileTimeBase;
				NTime::CTimeSpan FileTimeSpan = _Time - BaseTime;

				LARGE_INTEGER Temp;
				Temp.QuadPart = FileTimeSpan.f_GetSeconds() * 10000000;
				Temp.QuadPart += (FileTimeSpan.f_GetFraction() * 10000000.0).f_ToInt();

				o_FileTime.dwHighDateTime = Temp.HighPart;
				o_FileTime.dwLowDateTime = Temp.LowPart;
			}

			void fg_MalterlibTimeToSystemTime(NTime::CTimeConvert::CDateTime const &_DateTime, SYSTEMTIME &o_SysTime)
			{
				DMibRequire
					(
						_DateTime.m_Fraction >= 0.0
						&& _DateTime.m_Fraction < 1.0
						&& (_DateTime.m_Fraction * 1000.0).f_ToIntRound() <= 999
					)
					(_DateTime.m_Fraction)
				;

				o_SysTime.wYear = _DateTime.m_Year;
				o_SysTime.wMonth = _DateTime.m_Month;
				o_SysTime.wDayOfWeek = _DateTime.m_DayOfWeek;
				o_SysTime.wDay = _DateTime.m_DayOfMonth;
				o_SysTime.wHour = _DateTime.m_Hour;
				o_SysTime.wMinute = _DateTime.m_Minute;
				o_SysTime.wSecond = _DateTime.m_Second;
				o_SysTime.wMilliseconds = fg_Clamp((_DateTime.m_Fraction * 1000.0).f_ToIntRound(), 0, 999);
			}

			void fg_MalterlibTimeToSystemTime(NTime::CTime const &_Time, SYSTEMTIME &o_SysTime)
			{
				// Round to nearest millisecond with proper rollover to next second
				NTime::CTime RoundedTime = _Time;
				fp64 Fraction = RoundedTime.f_GetFraction();
				int32 Milliseconds = (Fraction * 1000.0).f_ToIntRound();
				if (Milliseconds >= 1000)
				{
					RoundedTime.f_SetSeconds(RoundedTime.f_GetSeconds() + 1);
					RoundedTime.f_SetFraction(0.0);
				}
				else
					RoundedTime.f_SetFraction(fp64(Milliseconds) / 1000.0);

				return fg_MalterlibTimeToSystemTime(NTime::CTimeConvert(RoundedTime).f_ExtractDateTime(), o_SysTime);
			}

			NTime::CTime fg_SystemTimeToMalterlibTime(SYSTEMTIME const &_SysTime)
			{
				return NTime::CTimeConvert::fs_CreateTime
					(
						_SysTime.wYear
						, _SysTime.wMonth
						, _SysTime.wDay
						, _SysTime.wHour
						, _SysTime.wMinute
						, _SysTime.wSecond
						, fp64(_SysTime.wMilliseconds) / 1000.0
					)
				;
			}
		}
	}
}
