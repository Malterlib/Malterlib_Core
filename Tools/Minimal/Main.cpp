// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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