// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Platform>
#include <Mib/Core/BuildMetaData>
#include <Mib/Bit/Static>

namespace NMib
{
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
		,EProcessorArchitecture_armv5
		,EProcessorArchitecture_armv6
		,EProcessorArchitecture_armv7
		,EProcessorArchitecture_armv8
		,EProcessorArchitecture_ppc32
		,EProcessorArchitecture_ppc64
		,EProcessorArchitecture_le32 // General purpose 32 bit little endian
		,EProcessorArchitecture_arm64
		,EProcessorArchitecture_arm64e
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

		void *fg_Mem_VirtualAllocInRange(umint &_Size, uint8 *_pLower, uint8 *_pUpper, EAllocationFlag _AllocFlags, ENumaNode _NumaNode = ENumaNode_Default, umint _Alignment = 0);
		void *fg_Mem_VirtualAlloc(umint &_Size, EAllocationFlag _AllocFlags, ENumaNode _NumaNode = ENumaNode_Default, umint _Alignment = 0);
		void *fg_Mem_VirtualRealloc(void *_pMem, umint &_Size, umint _OldSize, EAllocationFlag _AllocFlags, ENumaNode _NumaNode = ENumaNode_Default);
		void *fg_Mem_VirtualResize(void *_pMem, umint &_Size, umint _OldSize, EAllocationFlag _AllocFlags, ENumaNode _NumaNode = ENumaNode_Default);
		umint fg_Mem_VirtualGranularityAlloc(bool _bLargePages);
		umint fg_Mem_VirtualGranularityCommit(bool _bLargePages);
		umint fg_Mem_VirtualGranularityProtect(bool _bLargePages);
		void fg_Mem_VirtualCommit(void *_pMem, umint _Size);
		void fg_Mem_VirtualProtect(void *_pMem, umint _Size, uaint _Protect);
		uaint fg_Mem_VirtualGetProtect(void const *_pMem);
		void fg_Mem_VirtualDecommit(void *_pMem, umint _Size);
		void fg_Mem_VirtualFree(void *_pMem, umint _Size);
		umint fg_Mem_VirtualSize(const void *_pMem);
		umint fg_Mem_VirtualTrySize(const void *_pMem);
		fp32 fg_Mem_VirtualOverhead(void const *_pMem);
		constexpr bool fg_Mem_VirtualCanCommit();
		bool fg_Mem_VirtualCanProtect();
		void fg_Mem_VirtualFlushInstructionCache(void *_pMem, umint _Size);
		umint fg_Mem_PageSize();

		/***************************************************************************************************\
		|¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯|
		| Threading																							|
		|___________________________________________________________________________________________________|
		\***************************************************************************************************/

		// Futex-style wait/wake on a 32-bit word. _pAddress must be 4-byte aligned.
		// A wait blocks only while *_pAddress == _Expected and may return spuriously;
		// callers must re-check their predicate in a loop.
		// Wakes never dereference _pAddress in user space, so they are safe on
		// addresses whose object has already been destroyed; the cost is a
		// possible spurious wake of an unrelated waiter that reuses the address,
		// which the wait contract already requires callers to tolerate.
		// _Timeout is relative real-time seconds without time-speed scaling; a wait
		// with _Timeout <= 0 returns immediately. Returns true if the wait timed out.
		void fg_Futex_Wait(uint32 volatile *_pAddress, uint32 _Expected);
		bool fg_Futex_WaitTimeout(uint32 volatile *_pAddress, uint32 _Expected, fp64 _Timeout);
		void fg_Futex_WakeOne(uint32 volatile *_pAddress);
		// Wakes at most _nToWake waiters; a single syscall where the OS supports a
		// wake count (Linux), otherwise a loop of single wakes
		void fg_Futex_WakeCount(uint32 volatile *_pAddress, uint32 _nToWake);
		void fg_Futex_WakeAll(uint32 volatile *_pAddress);

		void *fg_Thread_GetCurrent();
		umint fg_Thread_GetCurrentUID();
		umint fg_Thread_GetCurrentUIDAlternate();
		void fg_Thread_Yield();

		/***************************************************************************************************\
		|¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯|
		| Thread local storage																				|
		|___________________________________________________________________________________________________|
		\***************************************************************************************************/

		umint fg_Thread_AllocLocal();

	#ifdef DMibPSupportThreadLocalDestructors
		umint fg_Thread_AllocLocalWithDestructor(void (_pDestructor)(void*));
		void fg_Thread_FreeLocalWithDestructor(umint _iStorage);
	#endif

		void fg_Thread_FreeLocal(umint _iStorage);
		void fg_Thread_SetLocal(umint _iStorage, void *_pData);
		void fg_Thread_SetLocalDestructor(umint _ThreadID, umint _iStorage, void *_pData);
		void fg_Thread_SetLocal(umint _ThreadID, umint _iStorage, void *_pData);
		void *fg_Thread_GetLocal(umint _ThreadID, umint _iStorage);
		void *fg_Thread_GetLocal(umint _iStorage);
		void *fg_Thread_GetLocalAlwaysSet(umint _iStorage);
		void *fg_Thread_GetLocalAlwaysSet(umint _ThreadID, umint _iStorage);

		umint fg_Thread_AllocLocalFast();
		void fg_Thread_FreeLocalFast(umint _iStorage);
		void fg_Thread_SetLocalFast(umint _iStorage, void *_pData);
		void fg_Thread_SetLocalFast(umint _ThreadID, umint _iStorage, void *_pData);
		void *fg_Thread_GetLocalFast(umint _iStorage);
		void *fg_Thread_GetLocalFast(umint _ThreadID, umint _iStorage);
		void *fg_Thread_GetLocalAlwaysSetFast(umint _iStorage);
		void *fg_Thread_GetLocalAlwaysSetFast(umint _ThreadID, umint _iStorage);

	#if defined(DPlatformFamily_macOS) && defined(DMibConfig_PThreadIntrospection)
		bool fg_Thread_GetLocalsDestroyed(umint _iPerThread);
	#endif

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

		void fg_Security_GenerateHighEntropyData(uint8 *_pData, umint _nBytes);

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

		CBuildMetadata fg_GetBuildMetadata();
	}
}
