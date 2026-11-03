// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NMacOSRuntime
{
	inline NMib::NStr::CStr const &CFileChangeNoticationContext::CNotification::CFileSnapshot::CCompare::operator () (CFileSnapshot const &_Node) const
	{
		return _Node.m_FileName;
	}
}
