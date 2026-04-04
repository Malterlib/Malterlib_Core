// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

extern bool g_bAllowInvalidExit;

namespace NMib::NPlatform
{
	namespace
	{
		struct CAllowInvalidExit
		{
			CAllowInvalidExit()
			{
				g_bAllowInvalidExit = true;
			}
		};

		CAllowInvalidExit g_AllowInvalidExit;
	}
}
