// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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
