// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

namespace NMib::NPlatform
{
	namespace
	{
		struct CInit
		{
			CInit()
			{
				fg_GetSys();
			}
		};

		CInit g_Init;
	}
}
