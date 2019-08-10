// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Platform>
#include <Mib/Bit/Static>

namespace NMib
{
	enum EHeapDebugFlag : uint32
	{
		EHeapDebugFlag_None = 0
		, EHeapDebugFlag_Ignore		= DMibBit(0)
		, EHeapDebugFlag_Internal	= DMibBit(1)
	};

	enum EAllocationFlag : uint32
	{
		EAllocationFlag_None = 0
		, EAllocationFlag_NoCommit = DMibBit(1)
		, EAllocationFlag_LargePages = DMibBit(2)
		, EAllocationFlag_LocationUp = DMibBit(3)
		, EAllocationFlag_LocationDown = DMibBit(4)
		, EAllocationFlag_WillFreeWithSize = DMibBit(5)
		, EAllocationFlag_MainHeap = DMibBit(6)
		, EAllocationFlag_NonTrackedMainHeap = DMibBit(7)
		, EAllocationFlag_SizeNotNeeded = DMibBit(8)
	};
	
	enum ENumaNode : int32
	{
		ENumaNode_Default = -1
	};

	enum EThreadPriority
	{
		 EThreadPriority_Lowest			= 0x0000
		,EThreadPriority_Low			= 0x2000
		,EThreadPriority_BelowNormal	= 0x4000
		,EThreadPriority_Normal			= 0x8000
		,EThreadPriority_AboveNormal	= 0xc000
		,EThreadPriority_High			= 0xe000
		,EThreadPriority_Highest		= 0x10000

		,EThreadPriority_Count			= 7
	};

	class CStackTraceInfo
	{
	public:
		const ch8 *m_pFunctionName;
		const ch8 *m_pModuleName;
		const ch8 *m_pSourceFileName;
		aint m_SourceLine;
		void *m_pContext; // Opaque pointer used by system implementation
	};

	enum EProcessorArchitecture
	{
		EProcessorArchitecture_Unknown = 0
		,EProcessorArchitecture_x86
		,EProcessorArchitecture_x86_64
		,EProcessorArchitecture_Itanium
		,EProcessorArchitecture_ARMv5
		,EProcessorArchitecture_ARMv6
		,EProcessorArchitecture_ARMv7
		,EProcessorArchitecture_ARMv8
		,EProcessorArchitecture_ppc32
		,EProcessorArchitecture_ppc64
		,EProcessorArchitecture_le32 // General purpose 32 bit little endian
		// MIPS ?
		// MIPS64 ?
	};

	enum EProcessorFeature
	{
		EProcessorFeature_None		= 0
		,EProcessorFeature_MMX		= DMibBit(0)	// x86, x86_64 (SIMD)
		,EProcessorFeature_SSE		= DMibBit(1)	// x86, x86_64 (SIMD)
		,EProcessorFeature_SSE2		= DMibBit(2)	// x86, x86_64 (SIMD)
		,EProcessorFeature_SSE3		= DMibBit(3)	// x86, x86_64 (SIMD)
		,EProcessorFeature_SSSE3	= DMibBit(4)	// x86, x86_64 (SIMD)
		,EProcessorFeature_SSE4_1	= DMibBit(5)	// x86, x86_64 (SIMD)
		,EProcessorFeature_SSE4_2	= DMibBit(6)	// x86, x86_64 (SIMD)
		,EProcessorFeature_NEON		= DMibBit(7)	// Armv7+ (SIMD)
		,EProcessorFeature_VFP		= DMibBit(8)	// Arm* (FPU)

		/* Other interesting flags to add:			
				AVX
				POPCNT
				CMPXCHG16B				
				3DNow(?)
		*/

		,EProcessorFeature_HyperVisor = DMibBit(15)	// CPU is running in a hypervisor (emu)

	};

	struct CProcessorInfo
	{
		EProcessorArchitecture m_Architecture;
		EProcessorFeature m_Features;

		/*
		Maybe extend with:
			Brand / Identity strings
			Cache sizes
		*/
	};

	struct CVirtualMachineInfo
	{
		bool m_bDetected;
		ch8 const* m_pName;		
	};


	namespace NSys
	{
		enum EColor 
		{ 
			EColor_Default, 
			EColor_Green, 
			EColor_Yellow, 
			EColor_Red
		};

		/***************************************************************************************************\
		|¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯|
		| Hardware																							|
		|___________________________________________________________________________________________________|
		\***************************************************************************************************/

		void fg_HW_GetProcessorInfo(CProcessorInfo& _Info);
		bool fg_HW_GetVirtualMachineInfo(CVirtualMachineInfo& _Info);
		bool fg_HW_MeetsMinimumRequirements();

		/***************************************************************************************************\
		|¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯|
		| Memory Allocation																					|
		|___________________________________________________________________________________________________|
		\***************************************************************************************************/

		void fg_Mem_DisableLazyReturnCheckout();	
		void fg_Mem_EnableLazyReturnCheckout();	

		void fg_Mem_PrepareFork();
		void fg_Mem_ForkedChild();
		void fg_Mem_ForkedParent();

		void *fg_Mem_VirtualAllocInRange(mint &_Size, uint8 *_pLower, uint8 *_pUpper, EAllocationFlag _AllocFlags, ENumaNode _NumaNode = ENumaNode_Default, mint _Alignment = 0);
		void *fg_Mem_VirtualAlloc(mint &_Size, EAllocationFlag _AllocFlags, ENumaNode _NumaNode = ENumaNode_Default, mint _Alignment = 0);
		void *fg_Mem_VirtualRealloc(void *_pMem, mint &_Size, mint _OldSize, EAllocationFlag _AllocFlags, ENumaNode _NumaNode = ENumaNode_Default);
		void *fg_Mem_VirtualResize(void *_pMem, mint &_Size, mint _OldSize, EAllocationFlag _AllocFlags, ENumaNode _NumaNode = ENumaNode_Default);
		mint fg_Mem_VirtualGranularityAlloc(bool _bLargePages);
		mint fg_Mem_VirtualGranularityCommit(bool _bLargePages);
		mint fg_Mem_VirtualGranularityProtect(bool _bLargePages);
		void fg_Mem_VirtualCommit(void *_pMem, mint _Size);
		void fg_Mem_VirtualProtect(void *_pMem, mint _Size, uaint _Protect);
		void fg_Mem_VirtualDecommit(void *_pMem, mint _Size);
		void fg_Mem_VirtualFree(void *_pMem, mint _Size);
		mint fg_Mem_VirtualSize(const void *_pMem);
		mint fg_Mem_VirtualTrySize(const void *_pMem);
		fp32 fg_Mem_VirtualOverhead(void const *_pMem);
		constexpr bool fg_Mem_VirtualCanCommit();
		bool fg_Mem_VirtualCanProtect();
		void fg_Mem_VirtualFlushInstructionCache(void *_pMem, mint _Size);

		/***************************************************************************************************\
		|¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯|
		| Threading																							|
		|___________________________________________________________________________________________________|
		\***************************************************************************************************/
		
		void *fg_Semaphore_Alloc(mint _InitialCount, mint _MaximumCount);
		void *fg_Semaphore_Duplicate(void * _pSemaphore);
		void fg_Semaphore_ForkedChild(void * _pSemaphore);
		void fg_Semaphore_Free(void * _pSemaphore);
		void fg_Semaphore_Increase(void * _pSemaphore, mint _Count);
		void fg_Semaphore_Wait(void * _pSemaphore);
		bool fg_Semaphore_WaitTimeout(void * _pSemaphore, fp64 _Timeout);
		bool fg_Semaphore_TryWait(void * _pSemaphore);

		void *fg_Event_Alloc(bool _InitialSignal);
		void fg_Event_Free(void *_pEvent);
		void fg_Event_PrepareFork(void *_pEvent);
		void fg_Event_ForkedChild(void *_pEvent);
		void fg_Event_ForkedParent(void *_pEvent);
		void fg_Event_SetSignaled(void * _pEvent);
		void fg_Event_ResetSignaled(void * _pEvent);
		void fg_Event_Wait(void * _pEvent);
		bool fg_Event_WaitTimeout(void * _pEvent, fp64 _Timeout);
		bool fg_Event_TryWait(void * _pEvent);

		void *fg_Thread_GetCurrent();
		mint fg_Thread_GetCurrentUID();
		mint fg_Thread_GetCurrentUIDAlternate();
		void fg_Thread_Yield();

		/***************************************************************************************************\
		|¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯|
		| Thread local storage																				|
		|___________________________________________________________________________________________________|
		\***************************************************************************************************/

		mint fg_Thread_AllocLocal();

	#ifdef DMibPSupportThreadLocalDestructors
		mint fg_Thread_AllocLocalWithDestructor(void (_pDestructor)(void*));
		void fg_Thread_FreeLocalWithDestructor(mint _iStorage);
	#endif
		
		void fg_Thread_FreeLocal(mint _iStorage);
		void fg_Thread_SetLocal(mint _iStorage, void *_pData);
		void fg_Thread_SetLocalDestructor(mint _ThreadID, mint _iStorage, void *_pData);
		void fg_Thread_SetLocal(mint _ThreadID, mint _iStorage, void *_pData);
		void *fg_Thread_GetLocal(mint _ThreadID, mint _iStorage);
		void *fg_Thread_GetLocal(mint _iStorage);
		void *fg_Thread_GetLocalAlwaysSet(mint _iStorage);
		void *fg_Thread_GetLocalAlwaysSet(mint _ThreadID, mint _iStorage);

		mint fg_Thread_AllocLocalFast();
		void fg_Thread_FreeLocalFast(mint _iStorage);
		void fg_Thread_SetLocalFast(mint _iStorage, void *_pData);
		void fg_Thread_SetLocalFast(mint _ThreadID, mint _iStorage, void *_pData);
		void *fg_Thread_GetLocalFast(mint _iStorage);
		void *fg_Thread_GetLocalFast(mint _ThreadID, mint _iStorage);
		void *fg_Thread_GetLocalAlwaysSetFast(mint _iStorage);
		void *fg_Thread_GetLocalAlwaysSetFast(mint _ThreadID, mint _iStorage);

		/***************************************************************************************************\
		|¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯|
		| Deadlock detector																					|
		|___________________________________________________________________________________________________|
		\***************************************************************************************************/

		void fg_Debug_PauseDeadlockDetector();
		void fg_Debug_ResumeDeadlockDetector();

		/***************************************************************************************************\
		|¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯|
		| Compiler																								|
		|___________________________________________________________________________________________________|
		\***************************************************************************************************/

		inline_never bool fg_Compiler_AlwaysFalse();
		bool fg_Compiler_MakeActive(const void *_Reference);
		bool fg_Compiler_MakeActive(int _Dummy, ...);
/*		template <typename t_CAny>
		static bool fg_Compiler_MakeActive(t_CAny &&_Other)
		{
			return fg_Compiler_MakeActive((const void * &)_Other);
		}*/

		/***************************************************************************************************\
		|¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯|
		| System
		|___________________________________________________________________________________________________|
		\***************************************************************************************************/
        
		void fg_CreateSystem();
		void fg_DestroySystem();
		void fg_PreDestroyHeap();
		
		void fg_Security_GenerateHighEntropyData(uint8 *_pData, mint _nBytes);

		/***************************************************************************************************\
		|¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯|
		| Debug																								|
		|___________________________________________________________________________________________________|
		\***************************************************************************************************/

		CStackTraceInfo *fg_Debug_AquireStackTraceInfo(CMibCodeAddress _Address);
		bool fg_Debug_AquireStackTraceInfo(CStackTraceInfo & _oInfo, CMibCodeAddress _Address, bool _bCanAllocNonTracked);
		void fg_Debug_ReleaseStackTraceInfo(CStackTraceInfo *_pInfo);

		void fg_DebugOutput(const ch8 *_pToOutput);
		void fg_DebugOutput(const ch16 *_pToOutput);
		void fg_DebugOutput(const ch32 *_pToOutput);
		void fg_DebugOutput(const NMib::NStr::CStrNonTracked &_Output);
		void fg_DebugOutput(const NMib::NStr::CWStrNonTracked &_Output);
		void fg_DebugOutput(const NMib::NStr::CUStrNonTracked &_Output);

	}

}

