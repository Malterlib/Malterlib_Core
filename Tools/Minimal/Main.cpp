// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

#if 1
int main()
{
	return 0;
}
#else

#include <Mib/Core/Application>

class CMinimal : public CApplication
{
	aint f_Main()
	{
		return 0;
	}
};

DAppImplement(CMinimal);

#endif