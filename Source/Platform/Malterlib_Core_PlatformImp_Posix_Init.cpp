// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

namespace NMib
{
	namespace NSys
	{
		void fg_CreateSystem();
		void fg_DestroySystem();
#if DPlatformFamily_Linux
		void fg_CreateSystem_Early(char **envp);
#endif
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

extern "C" void fg_InitMalterlib(int argc, char **argv, char **envp)
{
	NMib::NSys::fg_CreateSystem_Early(envp);
}

__attribute__((section(".preinit_array"), used)) static decltype(fg_InitMalterlib) *preinit_p = fg_InitMalterlib;

#else

extern "C" void __umodti3();
extern "C" void __udivmodti4();
extern "C" void __fixunsxfti();
extern "C" void __fixxfti();
extern "C" void __divti3();
extern "C" void __modti3();
extern "C" void __udivti3();

extern "C" void __attribute__ ((constructor(-1111111111))) fg_InitMalterlib()
{
	__attribute__((used)) static volatile unsigned long long WorkaroundSum
		= (unsigned long long)&__umodti3
		+ (unsigned long long)&__udivmodti4
		+ (unsigned long long)&__fixunsxfti
		+ (unsigned long long)&__fixxfti
		+ (unsigned long long)&__divti3
		+ (unsigned long long)&__modti3
		+ (unsigned long long)&__udivti3
	;

	NMib::NSys::fg_CreateSystem();
}

#endif
