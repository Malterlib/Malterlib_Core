// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Windows_String.h"
#include <Windows.h>

namespace NMib
{
	namespace NStr
	{
		namespace NPlatform
		{
			CStr fg_StrFromWindowsAnsi(CAnsiStr const &_Str)
			{
				CStr To;
				NStr::NPlatform::fg_SystemDecodeAnsiStr(_Str, To);
				return To;
			}

			CWStr fg_StrToWindows(CStr const &_Str)
			{
				CWStr Ret = _Str;
				return Ret;
			}

			CWStr fg_StrToWindows(CWStr const &_Str)
			{
				CWStr Ret = _Str;
				return Ret;
			}

			CWStr fg_StrToWindows(CUStr const &_Str)
			{
				CWStr Ret = _Str;
				return Ret;
			}

			CStr fg_StrFromWindows(CWStr const &_Str)
			{
				return _Str;
			}

			// The name in upper case as Windows compares the names of its environment and its registry, regardless of case also of
			// letters outside ASCII: LCMapStringEx's invariant upper case of its UTF-16.
			CStr fg_WindowsUpperCaseName(CStr const &_Name)
			{
				CWStr Wide = fg_StrToWindows(_Name);
				int nUnits = int(Wide.f_GetLen());
				if (!nUnits)
					return _Name;

				NContainer::TCVector<ch16> Upper;
				Upper.f_SetLen(umint(nUnits));
				int nUpper = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, (LPCWSTR)Wide.f_GetStr(), nUnits, (LPWSTR)Upper.f_GetArray(), nUnits, nullptr, nullptr, 0);
				if (nUpper <= 0)
					return _Name.f_UpperCase();

				CWStr UpperWide;
				UpperWide.f_AddStr(Upper.f_GetArray(), nUpper);

				return fg_StrFromWindows(UpperWide);
			}
		}
	}
}

