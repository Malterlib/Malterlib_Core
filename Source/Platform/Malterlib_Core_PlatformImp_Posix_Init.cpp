// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

namespace NMib
{
	namespace NSys
	{
		void fg_CreateSystem();
		void fg_DestroySystem();
#if DPlatformFamily_Linux && defined(DMibInitInPreInitArray)
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

#ifdef DMibInitInPreInitArray

extern "C" void fg_InitMalterlib(int argc, char **argv, char **envp)
{
	NMib::NSys::fg_CreateSystem_Early(envp);
}

__attribute__((section(".preinit_array"), used)) static decltype(fg_InitMalterlib) *preinit_p = fg_InitMalterlib;

#endif

#else

#ifdef DMalterlibUseStaticLibCxx
extern "C" void __umodti3();
extern "C" void __udivmodti4();
#ifdef DArchitecture_x64
extern "C" void __fixunsxfti();
extern "C" void __fixxfti();
#endif
extern "C" void __divti3();
extern "C" void __modti3();
extern "C" void __udivti3();
#endif

extern "C" void __attribute__ ((constructor(-1111111111))) fg_InitMalterlib()
{
#ifdef DMalterlibUseStaticLibCxx
	__attribute__((used)) static volatile unsigned long long WorkaroundSum
		= (unsigned long long)&__umodti3
		+ (unsigned long long)&__udivmodti4
#ifdef DArchitecture_x64
		+ (unsigned long long)&__fixunsxfti
		+ (unsigned long long)&__fixxfti
#endif
		+ (unsigned long long)&__divti3
		+ (unsigned long long)&__modti3
		+ (unsigned long long)&__udivti3
	;
#endif

	NMib::NSys::fg_CreateSystem();
}

extern "C" void fg_MalterlibDestroySystem_MacOS();

extern "C" void __attribute__ ((destructor(-1111111111))) fg_DestroyMalterlib()
{
	fg_MalterlibDestroySystem_MacOS();
}

#endif
