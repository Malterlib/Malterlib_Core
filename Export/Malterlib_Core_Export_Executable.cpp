// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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
