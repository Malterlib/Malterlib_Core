// Copyright © 2018 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Core/Application>

struct CApp : public NMib::CApplication
{
	aint f_Main() override
	{
		return 0;
	}
};

DMibAppImplement(CApp);
