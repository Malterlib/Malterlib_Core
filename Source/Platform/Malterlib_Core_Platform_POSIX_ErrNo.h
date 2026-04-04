// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib
{
	namespace NPlatform
	{
		template <typename tf_CStr>
		tf_CStr fg_FormatErrno(typename tf_CStr::CFormat &&_Desc, int _Err);

		NStr::CFStr256 fg_FormatErrno(const ch8 *_pDesc, int _Err);
		NStr::CFStr256 fg_FormatErrno(int _Err);
		NStr::CStr fg_FormatErrno(NStr::CStr::CFormat &&_Desc, int _Err);
		NStr::CStrNonTracked fg_FormatErrno(NStr::CStrNonTracked::CFormat &&_Desc, int _Err);
		NStr::CFStr256 fg_FormatErrno(NStr::CFStr256::CFormat &&_Desc, int _Err);
		NStr::CFStr256 fg_FormatErrno(const ch8 *_pDesc, int _Err);
	}
}

#include "Malterlib_Core_Platform_POSIX_ErrNo.hpp"
