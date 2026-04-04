// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Windows_String.h"

namespace NMib
{
	namespace NStr
	{
		namespace NPlatform
		{
			CStr fg_StrFromWindowsAnsi(const CAnsiStr &_Str)
			{
				CStr To;
				NStr::NPlatform::fg_SystemDecodeAnsiStr(_Str, To);
				return To;
			}

			CWStr fg_StrToWindows(const CStr &_Str)
			{
				CWStr Ret = _Str;
				return Ret;
			}

			CWStr fg_StrToWindows(const CWStr &_Str)
			{
				CWStr Ret = _Str;
				return Ret;
			}

			CWStr fg_StrToWindows(const CUStr &_Str)
			{
				CWStr Ret = _Str;
				return Ret;
			}

			CStr fg_StrFromWindows(const CWStr &_Str)
			{
				return _Str;
			}
		}
	}
}

