// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Core/Application>

struct CApp : public NMib::CApplication
{
private:
	aint f_Main()
	{
		return 0;
	}
};

DMibAppImplement(CApp);
