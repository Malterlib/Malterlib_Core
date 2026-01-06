// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NStr
	{
		namespace NPlatform
		{
			template <typename tf_CRet, typename tf_CSrc>
			tf_CRet fg_StrToWindows(const tf_CSrc &_Str);

			CStr fg_StrFromWindowsAnsi(const CAnsiStr &_Str);
			CWStr fg_StrToWindows(const CStr &_Str);
			CWStr fg_StrToWindows(const CWStr &_Str);
			CWStr fg_StrToWindows(const CUStr &_Str);
			CStr fg_StrFromWindows(const CWStr &_Str);

			static constexpr mint gc_MaxWindowsEnvVarLength = 32768;
		}
	}
}

#include "Malterlib_Core_Platform_Windows_String.hpp"
