// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

namespace NMib
{
	namespace NSys
	{
		void fg_CreateSystem();
		void fg_DestroySystem();
	}
}

#if DPlatformFamily_Linux

struct CInitMalterlib
{
	CInitMalterlib()
	{
		NMib::NSys::fg_CreateSystem();
	}
	~CInitMalterlib()
	{
		NMib::NSys::fg_DestroySystem();
	}
};

// Clang emits C++ initializers first, 101 is lowest allowed prio
CInitMalterlib g_InitMalterlib __attribute__((init_priority(101)));
/*
extern "C"
{
	// The highest priority destructors are called last
	void __attribute__ ((destructor(1111111111))) fg_DestroyMalterlib()
	{ 
		NMib::NSys::fg_DestroySystem();
	}
}
*/
#else

extern "C" void __attribute__ ((constructor(-1111111111))) fg_InitMalterlib()
{
	NMib::NSys::fg_CreateSystem();
}

#endif
