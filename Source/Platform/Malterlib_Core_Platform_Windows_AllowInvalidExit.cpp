// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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
