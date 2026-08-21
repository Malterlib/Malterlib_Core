// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

using namespace NMib;

#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>

#include <pthread.h>

#ifdef DPlatformFamily_macOS
	#include <sys/syscall.h>
	#include <unistd.h>
#endif
#ifdef DPlatformFamily_Linux
	#include <sys/syscall.h>
	#include <unistd.h>
#endif

#if defined(DMibPMachKernel)
	#include <mach/mach.h>
	#include <mach/mach_init.h>
	#include <mach/thread_policy.h>
#endif // DMibPMachKernel

#ifdef DPlatformFamily_macOS
	#include <Mib/Core/PlatformSpecific/MacOSQualityOfService>
#endif

#include "Malterlib_Core_PlatformImp_POSIX.h"

#if defined(DPlatformFamily_Linux) && defined(DMibAssumeGlibc)
// glibc exports these descriptors for libthread_db so debuggers do not need to hard-code the private struct pthread layout.
extern "C"
{
	extern uint32 const _thread_db_pthread_specific[3];
	extern uint32 const _thread_db_pthread_key_data_data[3];
	extern uint32 const _thread_db_pthread_key_data_seq[3];
	extern uint32 const _thread_db_pthread_key_data_level2_data[3];
	extern uint32 const _thread_db_sizeof_pthread_key_data;
}

namespace
{
	enum EGlibcThreadDbDescriptor
	{
		EGlibcThreadDbDescriptor_SizeBits
		, EGlibcThreadDbDescriptor_nElements
		, EGlibcThreadDbDescriptor_Offset
	};

	void fg_Glibc_ValidateThreadLocalLayout()
	{
		if
		(
			_thread_db_pthread_specific[EGlibcThreadDbDescriptor_SizeBits] % 8 != 0
			|| _thread_db_pthread_key_data_data[EGlibcThreadDbDescriptor_SizeBits] != sizeof(umint) * 8
			|| _thread_db_pthread_key_data_seq[EGlibcThreadDbDescriptor_SizeBits] != sizeof(umint) * 8
			|| _thread_db_pthread_key_data_level2_data[EGlibcThreadDbDescriptor_SizeBits] != _thread_db_sizeof_pthread_key_data * 8
			|| !_thread_db_pthread_key_data_level2_data[EGlibcThreadDbDescriptor_nElements]
			|| _thread_db_pthread_specific[EGlibcThreadDbDescriptor_SizeBits] / 8 % sizeof(umint) != 0
		)
		{
			DMibErrorSystemImp("Unsupported glibc thread local layout");
		}
	}

	uint8 *fg_Glibc_GetThreadLocalLevel2(umint _ThreadID, umint _iStorage, bool _bCreate)
	{
		fg_Glibc_ValidateThreadLocalLayout();

		umint nPerLevel2 = _thread_db_pthread_key_data_level2_data[EGlibcThreadDbDescriptor_nElements];
		umint nLevel2 = _thread_db_pthread_specific[EGlibcThreadDbDescriptor_SizeBits] / 8 / sizeof(umint);
		umint iLevel2 = _iStorage / nPerLevel2;
		if (iLevel2 >= nLevel2)
			DMibErrorSystemImp("glibc thread local index out of range");

		auto pLevel2Pointers = reinterpret_cast<NAtomic::TCAtomic<umint> *>
			(
				reinterpret_cast<uint8 *>(_ThreadID)
				+ _thread_db_pthread_specific[EGlibcThreadDbDescriptor_Offset]
			)
		;
		auto &pLevel2Pointer = pLevel2Pointers[iLevel2];
		umint pLevel2 = pLevel2Pointer.f_Load(NAtomic::gc_MemoryOrder_Acquire);
		if (pLevel2 || !_bCreate)
			return reinterpret_cast<uint8 *>(pLevel2);

		if (!iLevel2)
			DMibErrorSystemImp("glibc first thread local block is not initialized");

		void *pNewLevel2 = calloc(nPerLevel2, _thread_db_sizeof_pthread_key_data);
		if (!pNewLevel2)
			DMibErrorSystemImp(NMib::NPlatform::fg_FormatErrno("calloc (thread local block)", errno));

		umint Expected = 0;
		if
		(
			pLevel2Pointer.f_CompareExchangeStrong
				(
					Expected
					, reinterpret_cast<umint>(pNewLevel2)
					, NAtomic::gc_MemoryOrder_AcquireRelease
					, NAtomic::gc_MemoryOrder_Acquire
				)
		)
		{
			return reinterpret_cast<uint8 *>(pNewLevel2);
		}

		free(pNewLevel2);
		return reinterpret_cast<uint8 *>(Expected);
	}

	NAtomic::TCAtomic<umint> *fg_Glibc_GetThreadLocalField(uint8 *_pLevel2, umint _iStorage, uint32 const (&_Descriptor)[3])
	{
		umint nPerLevel2 = _thread_db_pthread_key_data_level2_data[EGlibcThreadDbDescriptor_nElements];
		umint iLevel2 = _iStorage % nPerLevel2;
		return reinterpret_cast<NAtomic::TCAtomic<umint> *>
			(
				_pLevel2
				+ iLevel2 * _thread_db_sizeof_pthread_key_data
				+ _Descriptor[EGlibcThreadDbDescriptor_Offset]
			)
		;
	}

	umint fg_Glibc_GetThreadLocalSequence(umint _iStorage)
	{
		uint8 Probe = 0;
		void *pOldData = pthread_getspecific((pthread_key_t)_iStorage);
		if (auto ErrNo = pthread_setspecific((pthread_key_t)_iStorage, &Probe))
			DMibErrorSystemImp(NMib::NPlatform::fg_FormatErrno("pthread_setspecific (thread local sequence probe)", ErrNo));

		uint8 *pLevel2 = fg_Glibc_GetThreadLocalLevel2(NSys::fg_Thread_GetCurrentUID(), _iStorage, false);
		bool bValid = false;
		umint Sequence = 0;
		if (pLevel2)
		{
			Sequence = fg_Glibc_GetThreadLocalField(pLevel2, _iStorage, _thread_db_pthread_key_data_seq)->f_Load(NAtomic::gc_MemoryOrder_Acquire);
			bValid = fg_Glibc_GetThreadLocalField(pLevel2, _iStorage, _thread_db_pthread_key_data_data)->f_Load(NAtomic::gc_MemoryOrder_Acquire) == reinterpret_cast<umint>(&Probe);
		}

		if (auto ErrNo = pthread_setspecific((pthread_key_t)_iStorage, pOldData))
			DMibErrorSystemImp(NMib::NPlatform::fg_FormatErrno("pthread_setspecific (thread local sequence restore)", ErrNo));
		if (!bValid)
			DMibErrorSystemImp("Unsupported glibc thread local layout");

		return Sequence;
	}

	void fg_Glibc_Thread_SetLocal(umint _ThreadID, umint _iStorage, void *_pData)
	{
		uint8 *pLevel2 = fg_Glibc_GetThreadLocalLevel2(_ThreadID, _iStorage, false);
		if (!pLevel2 && !_pData)
			return;

		umint Sequence = fg_Glibc_GetThreadLocalSequence(_iStorage);
		if (!pLevel2)
			pLevel2 = fg_Glibc_GetThreadLocalLevel2(_ThreadID, _iStorage, true);

		if (_pData)
		{
			auto pSpecificUsed = reinterpret_cast<NAtomic::TCAtomic<uint8> *>
				(
					reinterpret_cast<uint8 *>(_ThreadID)
					+ _thread_db_pthread_specific[EGlibcThreadDbDescriptor_Offset]
					+ _thread_db_pthread_specific[EGlibcThreadDbDescriptor_SizeBits] / 8
				)
			;
			pSpecificUsed->f_Store(1, NAtomic::gc_MemoryOrder_Release);
		}

		fg_Glibc_GetThreadLocalField(pLevel2, _iStorage, _thread_db_pthread_key_data_seq)->f_Store(Sequence, NAtomic::gc_MemoryOrder_Release);
		fg_Glibc_GetThreadLocalField(pLevel2, _iStorage, _thread_db_pthread_key_data_data)->f_Exchange(reinterpret_cast<umint>(_pData));
	}

	#ifndef DMibStaticThreadLocals
	void *fg_Glibc_Thread_GetLocal(umint _ThreadID, umint _iStorage)
	{
		uint8 *pLevel2 = fg_Glibc_GetThreadLocalLevel2(_ThreadID, _iStorage, false);
		if (!pLevel2)
			return nullptr;

		auto pData = fg_Glibc_GetThreadLocalField(pLevel2, _iStorage, _thread_db_pthread_key_data_data);
		umint Data = pData->f_Load(NAtomic::gc_MemoryOrder_Acquire);
		if (!Data)
			return nullptr;

		umint Sequence = fg_Glibc_GetThreadLocalField(pLevel2, _iStorage, _thread_db_pthread_key_data_seq)->f_Load(NAtomic::gc_MemoryOrder_Acquire);
		if (Sequence != fg_Glibc_GetThreadLocalSequence(_iStorage))
		{
			pData->f_Exchange(0);
			return nullptr;
		}

		return reinterpret_cast<void *>(Data);
	}
	#endif
}
#endif

// *************************************************************************************************************************
// The following code makes some assumptions about the POSIX implementation it is running on.
// These static asserts check that everything is OK.
// *************************************************************************************************************************

static_assert(sizeof(pthread_key_t) <= sizeof(umint), "pthread_key_t must be the same size or smaller than a umint.");
static_assert(sizeof(pthread_t) <= sizeof(umint), "pthread_t must be the same size or smaller than a umint.");
static_assert(sizeof(pid_t) <= sizeof(umint), "pid_t must be the same size or smaller than a umint.");

// *************************************************************************************************************************
// POSIX Thread Implementation
// *************************************************************************************************************************

umint NSys::fg_Thread_AllocLocalWithDestructor(void (_pDestructor)(void*))
{
	pthread_key_t pKey = 0;
	if (auto ErrNo = pthread_key_create(&pKey, _pDestructor))
		DMibErrorSystemImp(NPlatform::fg_FormatErrno("pthread_key_create (thread local alloc with destructor)", ErrNo));
	else if (pKey == 0)
	{
		// We don't support the key being 0, allocate the next key if this happens
		if (auto ErrNo = pthread_key_create(&pKey, _pDestructor))
			DMibErrorSystemImp(NPlatform::fg_FormatErrno("pthread_key_create (thread local alloc with destructor)", ErrNo));
		DMibFastCheck(pKey != 0);
	}

	if (auto ErrNo = pthread_setspecific(pKey, nullptr))
		DMibErrorSystemImp(NPlatform::fg_FormatErrno("pthread_setspecific (thread local alloc with destructor)", ErrNo));
	return (umint)pKey;
}

void NSys::fg_Thread_FreeLocalWithDestructor(umint _iStorage)
{
	pthread_key_t pKey = (pthread_key_t)_iStorage;
	if (auto ErrNo = pthread_key_delete(pKey))
		DMibErrorSystemImp(NPlatform::fg_FormatErrno("pthread_key_delete (thread local free)", ErrNo));
}

#ifdef DPlatformFamily_macOS
umint NSys::g_ThreadSelfOffset = 0;
umint NSys::g_ThreadLocalOffset = 0;
namespace NMib
{
	namespace NSys
	{
		umint g_ThreadLocalOffsetPThread = 0;
	}
}
#endif

void NSys::fg_Thread_SetLocalDestructor(umint _ThreadID, umint _iStorage, void *_pData)
{
	umint ThisThread = fg_Thread_GetCurrentUID();
	if (ThisThread == _ThreadID)
	{
		pthread_key_t pKey = (pthread_key_t)_iStorage;
		if (auto ErrNo = pthread_setspecific(pKey, _pData))
			DMibErrorSystemImp(NPlatform::fg_FormatErrno("pthread_setspecific (thread local set)", ErrNo));
		return;
	}
#ifdef DPlatformFamily_macOS
	// The thread UID is the pthread_t and the thread specific data lives at
	// g_ThreadLocalOffsetPThread from it, matching fg_Thread_GetLocal
	NAtomic::TCAtomic<umint> *pThreadLocal = (NAtomic::TCAtomic<umint> *)((_ThreadID + g_ThreadLocalOffsetPThread) + _iStorage * sizeof(umint));
	pThreadLocal->f_Exchange((umint)_pData);
#elif defined(DPlatformFamily_Linux) && defined(DMibAssumeGlibc)
	fg_Glibc_Thread_SetLocal(_ThreadID, _iStorage, _pData);
#else
	DMibPDebugBreak; // Should never get here
#endif
}

#ifndef DMibStaticThreadLocals

umint NSys::fg_Thread_AllocLocal()
{
	pthread_key_t pKey = 0;
	if (auto ErrNo = pthread_key_create(&pKey, nullptr))
		DMibErrorSystemImp(NPlatform::fg_FormatErrno("pthread_key_create (thread local alloc)", ErrNo));
	else if (pKey == 0)
	{
		if (auto ErrNo = pthread_key_create(&pKey, nullptr))
			DMibErrorSystemImp(NPlatform::fg_FormatErrno("pthread_key_create (thread local alloc)", ErrNo));
		DMibFastCheck(pKey != 0);
	}
	if (auto ErrNo = pthread_setspecific(pKey, nullptr))
		DMibErrorSystemImp(NPlatform::fg_FormatErrno("pthread_setspecific (thread local alloc)", ErrNo));
	return (umint)pKey;
}

void NSys::fg_Thread_FreeLocal(umint _iStorage)
{
	pthread_key_t pKey = (pthread_key_t)_iStorage;
	if (auto ErrNo = pthread_key_delete(pKey))
		DMibErrorSystemImp(NPlatform::fg_FormatErrno("pthread_key_delete (thread local free)", ErrNo));
}

void NSys::fg_Thread_SetLocal(umint _iStorage, void *_pData)
{
	pthread_key_t pKey = (pthread_key_t)_iStorage;
	if (auto ErrNo = pthread_setspecific(pKey, _pData))
		DMibErrorSystemImp(NPlatform::fg_FormatErrno("pthread_setspecific (thread local set)", ErrNo));
}

void NSys::fg_Thread_SetLocal(umint _ThreadID, umint _iStorage, void *_pData)
{
	umint ThisThread = fg_Thread_GetCurrentUID();
	if (ThisThread == _ThreadID)
	{
		fg_Thread_SetLocal(_iStorage, _pData);
		return;
	}
#ifdef DPlatformFamily_macOS
	// The thread UID is the pthread_t and the thread specific data lives at
	// g_ThreadLocalOffsetPThread from it, matching fg_Thread_GetLocal
	NAtomic::TCAtomic<umint> *pThreadLocal = (NAtomic::TCAtomic<umint> *)((_ThreadID + g_ThreadLocalOffsetPThread) + _iStorage * sizeof(umint));
	pThreadLocal->f_Exchange((umint)_pData);
#elif defined(DPlatformFamily_Linux) && defined(DMibAssumeGlibc)
	fg_Glibc_Thread_SetLocal(_ThreadID, _iStorage, _pData);
#else
	DMibPDebugBreak; // Should never get here
#endif
}

void *NSys::fg_Thread_GetLocal(umint _ThreadID, umint _iStorage)
{
	if (NSys::fg_Thread_GetCurrentUID() == _ThreadID)
		return fg_Thread_GetLocal(_iStorage);

#if defined(DPlatformFamily_macOS)
	NAtomic::TCAtomic<umint> *pThreadLocal = (NAtomic::TCAtomic<umint> *)((_ThreadID + g_ThreadLocalOffsetPThread) + _iStorage * sizeof(umint));
	return (void *)pThreadLocal->f_Load();
#elif defined(DPlatformFamily_Linux) && defined(DMibAssumeGlibc)
	return fg_Glibc_Thread_GetLocal(_ThreadID, _iStorage);
#else
	DMibPDebugBreak; // Should never get here
#endif

	return nullptr;
}

#endif

void *NSys::fg_Thread_GetLocalFast(umint _ThreadID, umint _iStorage)
{
	return fg_Thread_GetLocal(_ThreadID, _iStorage);
}

void *NSys::fg_Thread_GetLocalAlwaysSet(umint _ThreadID, umint _iStorage)
{
	return fg_Thread_GetLocal(_ThreadID, _iStorage);
}
void *NSys::fg_Thread_GetLocalAlwaysSetFast(umint _ThreadID, umint _iStorage)
{
	return fg_Thread_GetLocal(_ThreadID, _iStorage);
}

void NSys::fg_Thread_Sleep(fp32 _Seconds)
{
#ifdef DPlatformFamily_macOS
	if (g_bForking)
		return; // System needs to handle atfork before this works
#endif

	// Sleeping may be interrupted via EINTR so we ensure we sleep the whole time period.
	struct timespec Timeout;
	Timeout.tv_sec = _Seconds.f_ToInt();
	Timeout.tv_nsec = ((_Seconds - (fp32)Timeout.tv_sec) * 1000000000.0).f_ToInt();

	while (nanosleep(&Timeout, &Timeout) != 0 && errno == EINTR)
		;
}

#ifndef DMibConfig_FutexImplemented
	#include "Malterlib_Core_PlatformImp_FutexEmulation.hpp"
#endif

#include "Malterlib_Core_PlatformImp_FutexSync.hpp"

#ifndef DPlatformFamily_Emscripten

struct CThreadStartParams
{
	FThreadProc *m_pThreadProc;
	void *m_pThreadParam;
	umint m_ParentThreadID;
	CStrNonTracked m_ThreadName;
};

void *fg_ThreadStartRoutine(void *_pParams)
{
	signal(SIGPIPE,SIG_IGN);
	signal(SIGHUP,SIG_IGN);

	NStorage::TCUniquePointer<CThreadStartParams, CAllocator_NonTrackedHeap> pThreadParams = fg_Explicit((CThreadStartParams *)_pParams);

	CThreadStartParams StartParams = *pThreadParams;

	fg_GetSys()->f_ThreadLocalCreateThread(NSys::fg_Thread_GetCurrentUID(), StartParams.m_ParentThreadID);

	pThreadParams.f_Clear();

#ifdef DPlatformFamily_Linux
	if (NLocal::g_f_pthread_setname_np)
		NLocal::g_f_pthread_setname_np(pthread_self(), StartParams.m_ThreadName.f_GetStr());
#elif defined(DPlatformFamily_macOS)
#if DPlatformVersion < 1060
	if (CSystem::ms_PlatformVersion >= 10'06'00)
#endif
		pthread_setname_np(StartParams.m_ThreadName.f_GetStr());
#elif defined(DPlatformFamily_Emscripten)
#else
#	error "Implement this"
#endif

	aint ReturnCode	= StartParams.m_pThreadProc(StartParams.m_pThreadParam);

	return (void *)ReturnCode;

}

namespace
{
	struct CPrioMap
	{
		umint m_MalterlibPriority;
		int m_Scheduler;
	};

	static const CPrioMap gc_MalterlibToPOSIXPriorityMap[] =
	{
#if defined(DMibPLinuxKernel)
			{ 0x0000, SCHED_IDLE }
		,	{ 0x1FFF, SCHED_OTHER }
		,	{ 0xe000, SCHED_RR }
		,	{ 0xe001, SCHED_FIFO }
		,	{ 0x10000, SCHED_FIFO }
#elif defined(DMibPMachKernel)
			{ 0x0000, SCHED_OTHER }
		,	{ 0x4000, SCHED_RR }
		,	{ 0xe000, SCHED_FIFO }
		,	{ 0x10000, SCHED_FIFO }
#endif
		,	{ ~umint(0), 0 }
	};
};

static void fg_POSIX_MapThreadPriority(EExecutionPriority _Priority, int& _oSched, int& _oPrio)
{
	CPrioMap const* pCurEntry = & gc_MalterlibToPOSIXPriorityMap[0];
	CPrioMap const* pNextEntry;

	while (pCurEntry->m_MalterlibPriority != ~umint(0))
	{
		pNextEntry = pCurEntry + 1;

		if (_Priority < pNextEntry->m_MalterlibPriority)
		{
			_oSched = pCurEntry->m_Scheduler;

			int MinPrio = sched_get_priority_min(_oSched);
			int MaxPrio = sched_get_priority_max(_oSched);

			if (MaxPrio > MinPrio)
			{
				_oPrio
					= MinPrio
					+ (
						(fp64(_Priority) / fp64(EExecutionPriority_Highest))
						* fp64(MaxPrio - MinPrio)
					).f_ToInt()
				;
			}
			else
				_oPrio = MinPrio;
//			DMibLog(Info, "Prio found: {} = {} = {} -> {} CurEntry: {}", _Priority, _oPrio, MinPrio, MaxPrio, pCurEntry->m_MalterlibPriority);

			return;
		}

		++pCurEntry;
	}

	_oSched = SCHED_OTHER;
	int MinPrio = sched_get_priority_min(_oSched);
	int MaxPrio = sched_get_priority_max(_oSched);
	_oPrio = (MaxPrio - MinPrio) >> 1;
//	DMibLog(Info, "Prio not found: {}", _oPrio);
}

#if defined(DMibPMachKernel)
#include <mach/mach_time.h>
bool fg_SetMachPriority(void *_pThread, EExecutionPriority _Priority)
{
	if (_Priority == EExecutionPriority_Lowest)
	{
		thread_precedence_policy Policy;
		Policy.importance = 0;

		auto Ret = thread_policy_set(pthread_mach_thread_np((pthread_t)_pThread), THREAD_PRECEDENCE_POLICY, (integer_t *)&Policy, THREAD_PRECEDENCE_POLICY_COUNT);
		if (Ret == KERN_SUCCESS)
		{
			return true;
		}
		else
		{
			DMibDTrace("thread_policy_set failed: {}\n", Ret);
		}
	}
	if (_Priority == EExecutionPriority_Highest)
	{
		thread_time_constraint_policy Policy;
		fg_MemClear(Policy);
		struct mach_timebase_info TimeBaseInfo;
		mach_timebase_info(&TimeBaseInfo);
		uint64_t AbsTime = (uint64(1000000000) * TimeBaseInfo.denom) / TimeBaseInfo.numer;
		Policy.period = AbsTime/240; // HZ/160
		Policy.computation = AbsTime/480; // HZ/3300;
		Policy.constraint = AbsTime/241; // HZ/2200;
		Policy.preemptible = 1;

		auto Ret = thread_policy_set(pthread_mach_thread_np((pthread_t)_pThread), THREAD_TIME_CONSTRAINT_POLICY, (integer_t *)&Policy, THREAD_TIME_CONSTRAINT_POLICY_COUNT);
		if (Ret == KERN_SUCCESS)
		{
			return true;
		}
		else
		{
			DMibDTrace("thread_policy_set failed: {}\n", mach_error_string(Ret));
		}
	}
	return false;
}
#endif

void *NSys::fg_Thread_Create
	(
		FThreadProc *_pThreadProc
		, void *_pParam
		, EExecutionPriority _Priority
		, umint _StackSize
		, bool _bSuspended
		, const ch8 *_pThreadName
		, umint _Affinity
		, umint &_ThreadID
	)
{
	int Result;

	struct CData
	{
	private:
		bool mp_bThreadAttribsRequired = false;
		pthread_attr_t mp_ThreadAttribs;

	public:
		CData()
			: mp_bThreadAttribsRequired(false)
		{}

		~CData()
		{
			if (mp_bThreadAttribsRequired)
				pthread_attr_destroy(&mp_ThreadAttribs);
		}

		pthread_attr_t* f_UseThreadAttribs()
		{
			if (!mp_bThreadAttribsRequired)
			{
				mp_bThreadAttribsRequired = true;

				int Result = pthread_attr_init(&mp_ThreadAttribs);
				if (Result != 0)
					DMibError(NPlatform::fg_FormatErrno("pthread_attr_init (create thread)", Result));
			}

			return &mp_ThreadAttribs;
		}

		pthread_attr_t* f_GetThreadAttribs()
		{
			return mp_bThreadAttribsRequired ? &mp_ThreadAttribs : nullptr;
		}
	};

	CData Data;

/*
	Windows style basic thread priorities don't really map to the POSIX scheduling model.
	This is the mapping we use currently.
		0x0000	Lowest		(SCHED_IDLE)
		0x0001				(SCHED_OTHER, Min Prio)

		0x8000	Normal		(SCHED_OTHER, ? Prio)

		0x8001				(SCHED_RR, normal prio)
		0xe000	_RT_High	(SCHED_RR, high prio)

		0xe001	_RT_Highest (SCHED_FIFO)
		0x10000	_RT_Highest (SCHED_FIFO)
*/

	bool bAlreadySetPriority = false;
#ifdef DPlatformFamily_macOS
	if (&pthread_attr_set_qos_class_np)
	{
		int RelativePriority;
		auto QosClass = NMib::NPlatform::fg_PriorityToQualityOfService(_Priority, RelativePriority);
		if (!pthread_attr_set_qos_class_np(Data.f_UseThreadAttribs(), QosClass, RelativePriority))
			bAlreadySetPriority = true;
	}
#endif

	if (!bAlreadySetPriority)
	{
		int Scheduler = SCHED_OTHER;
		sched_param ScheduleParams;
		pthread_attr_getschedparam(Data.f_UseThreadAttribs(), &ScheduleParams); // We need to do this to get the correct quantum

		fg_POSIX_MapThreadPriority(_Priority, Scheduler, ScheduleParams.sched_priority);

		pthread_attr_setschedpolicy(Data.f_UseThreadAttribs(), Scheduler);
		pthread_attr_setschedparam(Data.f_UseThreadAttribs(), &ScheduleParams);
	}

#if defined DMibSanitizerEnabled_Address
	if (_StackSize == 0)
		_StackSize = 512 * 1024;
	_StackSize *= 4;
#endif

#if DMibPPtrBits == 32
	// The OS default stack (RLIMIT_STACK, usually 8 MiB on Linux) exhausts the
	// 32-bit address space when many threads are created, so use a smaller
	// default
	if (_StackSize == 0)
		_StackSize = 1024 * 1024;
#endif

	if (_StackSize != 0)
		pthread_attr_setstacksize (Data.f_UseThreadAttribs(), _StackSize);

#if defined(DMibPLinuxKernel)
	if (_Affinity)
	{
		// It is likely that we could just pass _Affinity as the cpuset but that would not be super portable.
		// For processors with > 32 (or 64) CPUs we will need to switch to a dynamic cpu_set_t
		cpu_set_t CPUSet;
		CPU_ZERO(&CPUSet);

		umint const nBits = sizeof(_Affinity) * 8;
		for (umint iB = 0
			;iB < nBits
			;++iB)
		{
			if (_Affinity & (1 << iB))
				CPU_SET(iB, &CPUSet);
		}

		Result = pthread_attr_setaffinity_np(Data.f_UseThreadAttribs(), sizeof(cpu_set_t), &CPUSet);

		if (Result != 0)
			DMibError(NPlatform::fg_FormatErrno("pthread_attr_setaffinity_np (create thread)", Result));
	}
#endif // DMibPLinuxKernel

	if (_bSuspended)
	{
		// Implement by waiting for a event in thread

	}

	NStorage::TCUniquePointer<CThreadStartParams, CAllocator_NonTrackedHeap> pThreadParams = fg_Construct();
	pThreadParams->m_pThreadProc = _pThreadProc;
	pThreadParams->m_pThreadParam = _pParam;
	pThreadParams->m_ParentThreadID = NSys::fg_Thread_GetCurrentUID();
	pThreadParams->m_ThreadName = _pThreadName;
#ifdef DPlatformFamily_Linux
	pThreadParams->m_ThreadName = pThreadParams->m_ThreadName.f_Left(15);
#endif

	pthread_t ThreadID;

	Result = pthread_create(&ThreadID, Data.f_GetThreadAttribs(), &fg_ThreadStartRoutine, pThreadParams.f_Get());
	if (Result != 0)
		DMibError(NPlatform::fg_FormatErrno("pthread_create (create thread)", Result));

	pThreadParams.f_Detach();

#if defined(DMibPMachKernel)
	if (!bAlreadySetPriority)
		fg_SetMachPriority(ThreadID, _Priority);
	if (_Affinity)
	{
		mach_port_t MachThread = pthread_mach_thread_np(ThreadID);
		thread_affinity_policy Policy;
		Policy.affinity_tag = _Affinity;
		thread_policy_set(MachThread, THREAD_AFFINITY_POLICY, (integer_t *)&Policy, THREAD_AFFINITY_POLICY_COUNT);
	}
#endif // DMibPMachKernel

	_ThreadID = (umint)ThreadID;
	return (void *)ThreadID;
}


void NSys::fg_Thread_SetAffinity(void *_pThread, umint _Affinity)
{
	pthread_t ThreadID = (pthread_t)_pThread;

#if defined(DMibPMachKernel)

	struct thread_affinity_policy Policy;
	Policy.affinity_tag = _Affinity;
	thread_policy_set(pthread_mach_thread_np(ThreadID), THREAD_AFFINITY_POLICY, (integer_t *)&Policy, THREAD_AFFINITY_POLICY_COUNT);

#elif defined(DMibPLinuxKernel)

	// It is likely that we could just pass _Affinity as the cpuset but that would not be super portable.
	// For processors with > 32 (or 64) CPUs we will need to switch to a dynamic cpu_set_t
	cpu_set_t CPUSet;
	CPU_ZERO(&CPUSet);

	umint const nBits = sizeof(_Affinity) * 8;
	for (umint iB = 0
		;iB < nBits
		;++iB)
	{
		if (_Affinity & (1 << iB))
			CPU_SET(iB, &CPUSet);
	}

	pthread_setaffinity_np(ThreadID, sizeof(cpu_set_t), &CPUSet);

#endif // DMibPLinuxKernel
}


void *NSys::fg_Thread_BeginDestroy(void *_pThread)
{
	return _pThread;
}

void NSys::fg_Thread_WillNotBlockUntilExit(void *_pThreadDestroyContext)
{
	if (_pThreadDestroyContext != nullptr)
		pthread_detach((pthread_t)_pThreadDestroyContext);
}

void NSys::fg_Thread_BlockUntilExit(void *_pThreadDestroyContext)
{
	if (_pThreadDestroyContext != nullptr)
		pthread_join((pthread_t)_pThreadDestroyContext,nullptr);
}

void NSys::fg_Thread_EndDestroy(void *_pThreadDestroyContext)
{

}

void NSys::fg_Thread_SetPriority(void *_pThread, EExecutionPriority _Priority)
{
#ifdef DPlatformFamily_macOS
	if (&pthread_set_qos_class_self_np && _pThread == NSys::fg_Thread_GetCurrent())
	{
		int RelativePriority;
		auto QosClass = NMib::NPlatform::fg_PriorityToQualityOfService(_Priority, RelativePriority);
		int ErrNo = pthread_set_qos_class_self_np(QosClass, RelativePriority);
		if (ErrNo)
			DMibError(NPlatform::fg_FormatErrno("pthread_set_qos_class_self_np (set thread priority)", ErrNo));
		return;
	}
#endif

#if defined(DMibPMachKernel)
	if (fg_SetMachPriority(_pThread, _Priority))
		return;
#endif
	int Scheduler = SCHED_OTHER;
	sched_param ScheduleParams;
	pthread_getschedparam((pthread_t)_pThread, &Scheduler, &ScheduleParams); // We need to do this to get the correct quantum

	fg_POSIX_MapThreadPriority(_Priority, Scheduler, ScheduleParams.sched_priority);

	int Result = pthread_setschedparam((pthread_t)_pThread, Scheduler, &ScheduleParams);

	if (Result != 0)
		DMibError(NPlatform::fg_FormatErrno("pthread_setschedparam (set thread priority)", Result));
}

void NSys::fg_Thread_Destroy(void *_pThread)
{

}

umint NSys::fg_Thread_GetCurrentUIDAlternate()
{
#ifdef DPlatformFamily_macOS
	if (&pthread_threadid_np)
	{
		uint64_t ThreadID;
		pthread_threadid_np(pthread_self(), &ThreadID);
		return ThreadID;
	}
#if DPlatformVersion < 1060
	else
		return fg_Thread_GetCurrentUID();
#endif
#elif defined(DPlatformFamily_Linux)
	return syscall(SYS_gettid);
#elif defined(DPlatformFamily_Emscripten)
	return fg_Thread_GetCurrentUID();
#else
	#error "Implement this";
#endif
}

umint NSys::fg_GetThreadSelf_Safe()
{
	return (umint)pthread_self();
}

umint NSys::fg_GetThreadLocal_Safe(umint _iVariable)
{
	return (umint)pthread_getspecific((pthread_key_t)_iVariable);
}

#endif
