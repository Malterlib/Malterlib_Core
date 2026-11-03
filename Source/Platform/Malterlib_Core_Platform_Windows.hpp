// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NPlatform
{
	inline_small bool fg_IsGoodStackPtr(void *_pAddr, umint _Len, umint _StackStart, umint _StackEnd)
	{
		umint StackStart = _StackStart;
		umint StackEnd = _StackEnd;
		umint AddrStart = (umint)_pAddr;
		umint AddrEnd = AddrStart + _Len;

		if (AddrEnd < AddrStart)
			return false;

		return AddrEnd <= StackStart && AddrStart >= StackEnd;
	}
}
