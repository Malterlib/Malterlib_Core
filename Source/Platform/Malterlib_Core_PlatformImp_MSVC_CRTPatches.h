// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#ifdef __cplusplus

#include <windows.h>

void __cdecl fg_CreateMalterlib();
void __cdecl fg_DestroyMalterlib();
void __cdecl fg_DestroyMalterlibAggregates();
void __cdecl fg_ValidExitProcess();
void __cdecl fg_ValidDestroyModule();
void __cdecl fg_FixFunctionPointers();
void __cdecl fg_InitMalterlibAll(void *_pInstance);

void * __cdecl fg_MalterlibAllocNonTracked(size_t _Size);
void __cdecl fg_MalterlibFreeNonTracked(void *_pMem);

extern "C" BOOL NTAPI fg_MalterlibDllMainCallback(void *_pInstance, DWORD _Reason, void *_pReserved);


namespace NMib
{
	namespace NSys
	{
		bool fg_Compiler_MakeActive(int _Dummy, ...);
	}
}


#define DMibCRTPatches

#endif
