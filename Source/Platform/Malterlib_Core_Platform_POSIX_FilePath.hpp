// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NFile::NPlatform
{
	template <typename tf_CStr>
	auto fg_ConvertToPOSIXPath(tf_CStr const &_Path, bool _bAddCurrentDir) -> TCEnableIf<sizeof(typename tf_CStr::CChar) == 1, tf_CStr>
		requires (sizeof(typename tf_CStr::CChar) == 1) // Incorrect string type
	{
		return NFile::CFile::fs_GetExpandedPath(_Path, _bAddCurrentDir);
	}
}
