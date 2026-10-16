// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib
{
	namespace NStr
	{
		namespace NPlatform
		{
			template <typename tf_CRet, typename tf_CSrc>
			tf_CRet fg_StrToWindows(tf_CSrc const &_Str);

			CStr fg_StrFromWindowsAnsi(CAnsiStr const &_Str);
			CWStr fg_StrToWindows(CStr const &_Str);
			CWStr fg_StrToWindows(CWStr const &_Str);
			CWStr fg_StrToWindows(CUStr const &_Str);
			CStr fg_StrFromWindows(CWStr const &_Str);

			static constexpr umint gc_MaxWindowsEnvVarLength = 32768;
		}
	}
}

#include "Malterlib_Core_Platform_Windows_String.hpp"
