// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#ifdef DMibStaticThreadLocals

#ifdef DArchitecture_arm64
#define DAddThreadSelfOffset + g_ThreadSelfOffset
#else
#define DAddThreadSelfOffset
#endif

using namespace NMib;

#include <unistd.h>
#include <sys/types.h>

#include <Mib/Container/BitArrayHierarchical>

// *************************************************************************************************************************
// POSIX Thread Implementation
// *************************************************************************************************************************


constexpr mint gc_nMalterlibThreadLocals = 256; // 2 KB on 64 bit platforms

constinit NThread::CLowLevelLockAggregate gc_nMalterlibThreadLocalsAllocatedLock = {DAggregateInit};
constinit NContainer::TCBitArrayHierarchical<gc_nMalterlibThreadLocals> gc_nMalterlibThreadLocalsAllocated{};

#ifdef DMibDynamicLibrary
	#ifndef DMibAssumeMalterlibHost
		__thread mint __attribute__((tls_model("initial-exec"))) g_MalterlibThreadLocals[256] = {0};
		__thread pid_t __attribute__((tls_model("initial-exec"))) g_MalterlibCurrentTID = 0;
	#endif
#else
__thread mint __attribute__((tls_model("local-exec"))) g_MalterlibThreadLocals[256] = {0};
__thread pid_t __attribute__((tls_model("local-exec"))) g_MalterlibCurrentTID = 0;
#endif

pid_t fg_Malterlib_Thread_GetTID_Local();

#if !defined(DMibDynamicLibrary)
extern "C" assure_used module_export mint fg_Malterlib_Thread_AllocLocal()
{
	return NSys::fg_Thread_AllocLocal();
}
extern "C" assure_used module_export void fg_Malterlib_Thread_FreeLocal(mint _iStorage)
{
	NSys::fg_Thread_FreeLocal(_iStorage);
}
extern "C" assure_used module_export pid_t fg_Malterlib_Thread_GetTID()
{
	return fg_Malterlib_Thread_GetTID_Local();
}
#endif

#if defined(DMibDynamicLibrary) && defined(DMibAssumeMalterlibHost)
extern "C" mint fg_Malterlib_Thread_AllocLocal();
extern "C" void fg_Malterlib_Thread_FreeLocal(mint _iStorage);
extern "C" pid_t fg_Malterlib_Thread_GetTID();

mint NSys::fg_Thread_AllocLocal()
{
	return fg_Malterlib_Thread_AllocLocal();
}

void NSys::fg_Thread_FreeLocal(mint _iStorage)
{
	return fg_Malterlib_Thread_FreeLocal(_iStorage);
}

pid_t fg_Malterlib_Thread_GetTID_Local()
{
	return fg_Malterlib_Thread_GetTID();
}

#else

pid_t fg_Malterlib_Thread_GetTID_Local()
{
	pid_t ThreadID = g_MalterlibCurrentTID;
	if (!ThreadID) [[unlikely]]
		g_MalterlibCurrentTID = ThreadID = syscall(SYS_gettid);
	return ThreadID;
}

mint NSys::fg_Thread_AllocLocal()
{
	aint iThreadLocal;
	{
		DMibLock(gc_nMalterlibThreadLocalsAllocatedLock);
		iThreadLocal = gc_nMalterlibThreadLocalsAllocated.f_FindFreeBitAndSet();
	}
	if (iThreadLocal < 0)
		DMibErrorSystemImp("Out of thread local indices");

	smint Offset = (smint)&g_MalterlibThreadLocals[iThreadLocal] - (smint)(NMib::NSys::fg_GetThreadSelf() DAddThreadSelfOffset);
	return (mint)Offset;
}

void NSys::fg_Thread_FreeLocal(mint _iStorage)
{
	smint StartOffset = (smint)&g_MalterlibThreadLocals[0] - (smint)(NMib::NSys::fg_GetThreadSelf() DAddThreadSelfOffset);
	smint iStorage = (_iStorage - StartOffset) / sizeof(mint);
	if (iStorage < 0 || iStorage >= gc_nMalterlibThreadLocals)
		DMibErrorSystemImp("Thread local index out of range");
	bool bAllocated;
	{
		DMibLock(gc_nMalterlibThreadLocalsAllocatedLock);
		bAllocated = gc_nMalterlibThreadLocalsAllocated.f_GetBit(iStorage);
		if (!bAllocated) [[unlikely]]
			DMibErrorSystemImp("Thread local index has not been allocated");
		gc_nMalterlibThreadLocalsAllocated.f_SetBit(iStorage, false);
	}
}
#endif

void NSys::fg_Thread_SetLocal(mint _iStorage, void *_pData)
{
	auto pAlloc = (void **)((uint8 *)NMib::NSys::fg_GetThreadSelf() DAddThreadSelfOffset + smint(_iStorage));
	*pAlloc = _pData;
}

void NSys::fg_Thread_SetLocal(mint _ThreadID, mint _iStorage, void *_pData)
{
	mint ThisThread = fg_Thread_GetCurrentUID();
	if (ThisThread == _ThreadID)
	{
		fg_Thread_SetLocal(_iStorage, _pData);
		return;
	}
	NAtomic::TCAtomic<mint> *pThreadLocal = (NAtomic::TCAtomic<mint> *)((uint8 *)_ThreadID DAddThreadSelfOffset + smint(_iStorage));
	pThreadLocal->f_Exchange((mint)_pData);
}

void *NSys::fg_Thread_GetLocal(mint _ThreadID, mint _iStorage)
{
	if (NSys::fg_Thread_GetCurrentUID() == _ThreadID)
		return fg_Thread_GetLocal(_iStorage);
	NAtomic::TCAtomic<mint> *pThreadLocal = (NAtomic::TCAtomic<mint> *)((uint8 *)_ThreadID DAddThreadSelfOffset + smint(_iStorage));
	return (void *)pThreadLocal->f_Load();
}

#else
__thread pid_t g_MalterlibCurrentTID = 0;
pid_t fg_Malterlib_Thread_GetTID_Local()
{
	pid_t ThreadID = g_MalterlibCurrentTID;
	if (!ThreadID) [[unlikely]]
		g_MalterlibCurrentTID = ThreadID = syscall(SYS_gettid);
	return ThreadID;
}
#endif
