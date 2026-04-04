// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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
				uint64 Nano100Remainder = Nano100 % 10000000;

				// Convert 100-nanosecond units to internal fraction using integer arithmetic
				// FractionInt = Nano100Remainder * mc_FractionDividend / 10,000,000
				//             = Nano100Remainder * (Divisor * 10,000,000 + Remainder) / 10,000,000
				//             = Nano100Remainder * Divisor + Nano100Remainder * Remainder / 10,000,000
				constexpr uint64 TenMillion = 10'000'000;
				constexpr uint64 Divisor = NTime::NPrivate::CConst::mc_FractionDividend / TenMillion;
				constexpr uint64 Remainder = NTime::NPrivate::CConst::mc_FractionDividend % TenMillion;

				uint64 FractionInt = Nano100Remainder * Divisor + Nano100Remainder * Remainder / TenMillion;

				NTime::CTimeSpan FileTimeSpan;
				FileTimeSpan.f_SetSecondsNoFraction(nSeconds);
				FileTimeSpan.f_SetFractionInt(FractionInt);

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

				// Convert internal fraction to 100-nanosecond units using integer arithmetic with rounding
				// Nano100 = FractionInt * 10,000,000 / mc_FractionDividend
				//         = (FractionInt + Divisor/2) / Divisor  (with rounding)
				constexpr uint64 TenMillion = 10'000'000;
				constexpr uint64 Divisor = NTime::NPrivate::CConst::mc_FractionDividend / TenMillion;

				uint64 FractionInt = FileTimeSpan.f_GetFractionInt();
				uint64 Nano100 = (FractionInt + Divisor / 2) / Divisor;

				LARGE_INTEGER Temp;
				Temp.QuadPart = FileTimeSpan.f_GetSeconds() * TenMillion;

				// Handle overflow if Nano100 rounds to TenMillion
				if (Nano100 >= TenMillion)
					Temp.QuadPart += TenMillion;
				else
					Temp.QuadPart += Nano100;

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
