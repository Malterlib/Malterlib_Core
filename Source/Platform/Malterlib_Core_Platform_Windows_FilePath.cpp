// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_Platform_Windows_FilePath.h"
#include <Mib/Core/PlatformSpecific/WindowsString>

namespace NMib
{
	namespace NFile
	{
		namespace NPlatform
		{
			NStr::CWStr fg_ConvertToWindowsPathLocal(const NStr::CStr &_Path, bool _bForceLong)
			{
			#ifdef DMibAlwaysUseLongWindowsPaths
				_bForceLong = true;
			#endif
				if (_bForceLong)
					return fg_ConvertToWindowsPath(_Path, true, -1);
				else
					return fg_ConvertToWindowsPath(_Path, true, _MAX_PATH, false);
			}

			NStr::CWStr fg_ConvertToShortWindowsPath(const NStr::CStr &_Path, bint _bAddCurrentDir)
			{
				return fg_ConvertToShortWindowsPath<NStr::CWStr, NStr::CWStr>(_Path, _bAddCurrentDir);
			}


			NStr::CWStr fg_ConvertToLongWindowsPath(const NStr::CStr &_Path, bint _bAddCurrentDir)
			{
				return fg_ConvertToLongWindowsPath<NStr::CWStr, NStr::CWStr>(_Path, _bAddCurrentDir);
			}

			NStr::CWStr fg_ConvertToWindowsPath(const NStr::CStr &_Path, bint _bAddCurrentDir, aint _MaxLen)
			{
				return fg_ConvertToWindowsPath<NStr::CWStr, NStr::CWStr>(_Path, _bAddCurrentDir, _MaxLen, true);
			}

			NStr::CWStr fg_ConvertToWindowsPath(const NStr::CStr &_Path, bint _bAddCurrentDir, aint _MaxLen, bool _bTryShorten)
			{
				return fg_ConvertToWindowsPath<NStr::CWStr, NStr::CWStr>(_Path, _bAddCurrentDir, _MaxLen, _bTryShorten);
			}


			NStr::CStr fg_ConvertFromWindowsPath(const NStr::CWStr &_Path)
			{
				NStr::CStr ToRet = NStr::NPlatform::fg_StrFromWindows(_Path);
				return fg_ConvertFromWindowsPath(ToRet);
			}

			NStr::CStr fg_ConvertFromWindowsPath(const NStr::CStr &_Path)
			{
				return fg_ConvertFromWindowsPathInternal<NStr::CStr>(fg_ConvertToWindowsPath<NStr::CWStr, NStr::CWStr, NStr::CStr>(fg_ConvertFromWindowsPathInternal<NStr::CStr>(_Path), false, -1));
			}
		}
	}
}
