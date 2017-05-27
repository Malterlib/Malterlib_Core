// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NFile
	{
		namespace NPlatform
		{
			template <typename tf_CRet, typename tf_CStr>
			tf_CRet fg_ConvertToWindowsPathLocal(const tf_CStr &_Path, bool _bForceLong = false);
			template <typename tf_CWindows, typename tf_CRet, typename tf_CSource>
			tf_CRet fg_ConvertToWindowsPath(const tf_CSource &_Path, bint _bAddCurrentDir, aint _MaxLen, bool _bTryShorten);
			template <typename tf_CWindows, typename tf_CRet, typename tf_CSource>
			tf_CRet fg_ConvertToShortWindowsPath(const tf_CSource &_Path, bint _bAddCurrentDir);
			template <typename tf_CWindows, typename tf_CRet, typename tf_CSource>
			tf_CRet fg_ConvertToLongWindowsPath(const tf_CSource &_Path, bint _bAddCurrentDir);

			NStr::CWStr fg_ConvertToWindowsPath(const NStr::CStr &_Path, bint _bAddCurrentDir, aint _MaxLen = _MAX_PATH, bool _bTryShorten = true);
			template <typename tf_CWindowsStr, typename tf_CRet, typename tf_CSrc>
			tf_CRet fg_ConvertToWindowsPath(const tf_CSrc &_Path, bint _bAddCurrentDir, aint _MaxLen = _MAX_PATH, bool _bTryShorten = true);
			NStr::CStr fg_ConvertFromWindowsPath(const NStr::CWStr &_Path);
			NStr::CStr fg_ConvertFromWindowsPath(const NStr::CStr &_Path);
			template <typename tf_CWindowsStr, typename tf_CRet, typename tf_CSrc>
			tf_CRet fg_ConvertFromWindowsPath(const tf_CSrc &_Path);
			NStr::CWStr fg_ConvertToWindowsPathLocal(const NStr::CStr &_Path, bool _bForceLong = false);

			NStr::CStr fg_ConvertToDevicePath(NStr::CStr const &_In);

			NStr::CStr fg_ConvertToMinGWPath(NStr::CStr const &_In);
		}
	}
}

#include "Malterlib_Core_Platform_Windows_FilePath.hpp"
