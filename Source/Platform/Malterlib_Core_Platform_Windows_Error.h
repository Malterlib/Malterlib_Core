// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NPlatform
	{
		NStr::CFStr256 fg_Win32_GetLastErrorStr(uint32 _Error = 0);
		template <typename tf_CStr>
		tf_CStr fg_ErrnoString(int _Err);

		extern template NStr::CStr fg_ErrnoString<NStr::CStr>(int _Err);
	}
}

