// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NLocal
{
	inline_always static uint32 fg_TlsIndexToTebOffset(uint32 _Index)
	{
#if defined(DArchitecture_arm64)
		return _Index * 8 + 0x1480;
#elif defined(DArchitecture_x64)
		return _Index * 8 + 0x1480;
#else
		return _Index * 4 + 0xe10;
#endif
	}

	inline_always static uint32 fg_TebOffsetToTlsIndex(uint32 _Offset)
	{
#if defined(DArchitecture_arm64)
		return (_Offset - 0x1480) / 8;
#elif defined(DArchitecture_x64)
		return (_Offset - 0x1480) / 8;
#else
		return (_Offset - 0xe10) / 4;
#endif
	}
}
