// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Windows_String.h"

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
		}
	}
}

