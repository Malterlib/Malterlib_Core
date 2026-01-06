// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

namespace NMib
{
	namespace NFile
	{
		namespace NPlatform
		{
			template <typename tf_CStr>
			auto fg_ConvertToPOSIXPath(tf_CStr const &_Path, bool _bAddCurrentDir = true)
				-> TCEnableIf<sizeof(typename tf_CStr::CChar) == 1, tf_CStr>
				requires (sizeof(typename tf_CStr::CChar) == 1) // Incorrect string type
			{
				return NFile::CFile::fs_GetExpandedPath(_Path, _bAddCurrentDir);
			}
		}
	}
}
