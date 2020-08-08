// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

using namespace NMib;

#include <pthread.h>
#include <errno.h>
#include <signal.h>

#if defined(DMibPMachKernel)
	#include <mach/mach.h>
	#include <mach/mach_init.h>
	#include <mach/thread_policy.h>
#endif // DMibPMachKernel

#ifdef DPlatformFamily_OSX
	#include <Mib/Core/PlatformSpecific/OSXQualityOfService>
#endif

#include "Malterlib_Core_PlatformImp_POSIX.h"

// *************************************************************************************************************************
// The following code makes some assumptions about the POSIX implementation it is running on.
// These static asserts check that everything is OK.
// *************************************************************************************************************************

static_assert(sizeof(pthread_key_t) <= sizeof(mint), "pthread_key_t must be the same size or smaller than a mint.");
static_assert(sizeof(pthread_t) <= sizeof(mint), "pthread_t must be the same size or smaller than a mint.");
static_assert(sizeof(pid_t) <= sizeof(mint), "pid_t must be the same size or smaller than a mint.");

// *************************************************************************************************************************
// POSIX Thread Implementation
// *************************************************************************************************************************

mint NSys::fg_Thread_AllocLocalWithDestructor(void (_pDestructor)(void*))
{
	pthread_key_t pKey = 0;
	if (pthread_key_create(&pKey, _pDestructor))
		DMibErrorSystemImp(NPlatform::fg_FormatErrno("pthread_key_create (thread local alloc with destructor)", errno));
	else if (pKey == 0)
	{
		// We don't support the key being 0, allocate the next key if this happens
		if (pthread_key_create(&pKey, _pDestructor))
			DMibErrorSystemImp(NPlatform::fg_FormatErrno("pthread_key_create (thread local alloc with destructor)", errno));
		DMibFastCheck(pKey != 0);
	}
	
	if (pthread_setspecific(pKey, nullptr))
		DMibErrorSystemImp(NPlatform::fg_FormatErrno("pthread_setspecific (thread local alloc with destructor)", errno));
	return (mint)pKey;
}

void NSys::fg_Thread_FreeLocalWithDestructor(mint _iStorage)
{
	pthread_key_t pKey = (pthread_key_t)_iStorage;
	if (pthread_key_delete(pKey))
		DMibErrorSystemImp(NPlatform::fg_FormatErrno("pthread_key_delete (thread local free)", errno));
}

#ifdef DPlatformFamily_OSX
mint NSys::g_ThreadSelfOffset = 0;
mint NSys::g_ThreadLocalOffset = 0;
namespace NMib
{
	namespace NSys
	{
		mint g_ThreadLocalOffsetPThread = 0;
	}
}
#endif

void NSys::fg_Thread_SetLocalDestructor(mint _ThreadID, mint _iStorage, void *_pData)
{
	mint ThisThread = fg_Thread_GetCurrentUID();
	if (ThisThread == _ThreadID)
	{
		pthread_key_t pKey = (pthread_key_t)_iStorage;
		if (pthread_setspecific(pKey, _pData))
			DMibErrorSystemImp(NPlatform::fg_FormatErrno("pthread_setspecific (thread local set)", errno));
		return;
	}
#ifdef DPlatformFamily_OSX
	#if defined(DMibSafeThreadLocals) && !defined(DArchitecture_ppc32) && !defined(DArchitecture_ppc64)
		
		NAtomic::TCAtomic<mint> *pThreadLocal = (NAtomic::TCAtomic<mint> *)((_ThreadID + g_ThreadLocalOffset) + _iStorage * sizeof(mint));
		pThreadLocal->f_Exchange((mint)_pData)

	#else
		#if DPlatformVersion >= 1070
			#if defined(__i386__) || defined(__x86_64__)
				NAtomic::TCAtomic<mint> *pThreadLocal = (NAtomic::TCAtomic<mint> *)((_ThreadID + 0x0) + _iStorage * sizeof(mint));
			#else
				#error "Not Implemented"
			#endif
			pThreadLocal->f_Exchange((mint)_pData);
		#elif DPlatformVersion >= 1050
			NAtomic::TCAtomic<mint> *pThreadLocal = (NAtomic::TCAtomic<mint> *)((_ThreadID + g_ThreadLocalOffset) + _iStorage * sizeof(mint));
			pThreadLocal->f_Exchange((mint)_pData);
		#else
			#error "Not Implemented"
		#endif
	#endif
#else
	DMibPDebugBreak; // Should never get here
#endif
}

#ifndef DMibStaticThreadLocals

mint NSys::fg_Thread_AllocLocal()
{
	pthread_key_t pKey = 0;
	if (pthread_key_create(&pKey, nullptr))
		DMibErrorSystemImp(NPlatform::fg_FormatErrno("pthread_key_create (thread local alloc)", errno));
	else if (pKey == 0)
	{
		if (pthread_key_create(&pKey, nullptr))
			DMibErrorSystemImp(NPlatform::fg_FormatErrno("pthread_key_create (thread local alloc)", errno));
		DMibFastCheck(pKey != 0);
	}
	if (pthread_setspecific(pKey, nullptr))
		DMibErrorSystemImp(NPlatform::fg_FormatErrno("pthread_setspecific (thread local alloc)", errno));
	return (mint)pKey;
}

void NSys::fg_Thread_FreeLocal(mint _iStorage)
{
	pthread_key_t pKey = (pthread_key_t)_iStorage;
	if (pthread_key_delete(pKey))
		DMibErrorSystemImp(NPlatform::fg_FormatErrno("pthread_key_delete (thread local free)", errno));
}

void NSys::fg_Thread_SetLocal(mint _iStorage, void *_pData)
{
	pthread_key_t pKey = (pthread_key_t)_iStorage;
	if (pthread_setspecific(pKey, _pData))
		DMibErrorSystemImp(NPlatform::fg_FormatErrno("pthread_setspecific (thread local set)", errno));
}

void NSys::fg_Thread_SetLocal(mint _ThreadID, mint _iStorage, void *_pData)
{
	mint ThisThread = fg_Thread_GetCurrentUID();
	if (ThisThread == _ThreadID)
	{
		fg_Thread_SetLocal(_iStorage, _pData);
		return;
	}
#ifdef DPlatformFamily_OSX
	#if defined(DMibSafeThreadLocals) && !defined(DArchitecture_ppc32) && !defined(DArchitecture_ppc64)
		NAtomic::TCAtomic<mint> *pThreadLocal = (NAtomic::TCAtomic<mint> *)((_ThreadID + g_ThreadLocalOffset) + _iStorage * sizeof(mint));
		pThreadLocal->f_Exchange((mint)_pData)

	#else
		#if DPlatformVersion >= 1070
			#if defined(__i386__) || defined(__x86_64__)
				NAtomic::TCAtomic<mint> *pThreadLocal = (NAtomic::TCAtomic<mint> *)((_ThreadID + 0x0) + _iStorage * sizeof(mint));
			#else
				#error "Not Implemented"
			#endif
			pThreadLocal->f_Exchange((mint)_pData);
		#elif DPlatformVersion >= 1050
			NAtomic::TCAtomic<mint> *pThreadLocal = (NAtomic::TCAtomic<mint> *)((_ThreadID + g_ThreadLocalOffset) + _iStorage * sizeof(mint));
			pThreadLocal->f_Exchange((mint)_pData);
		#else
			#error "Not Implemented"
		#endif
	#endif
#else
	DMibPDebugBreak; // Should never get here
#endif
}

void *NSys::fg_Thread_GetLocal(mint _ThreadID, mint _iStorage)
{
	if (NSys::fg_Thread_GetCurrentUID() == _ThreadID)
		return fg_Thread_GetLocal(_iStorage);
  
#if defined(DPlatformFamily_OSX)
	NAtomic::TCAtomic<mint> *pThreadLocal = (NAtomic::TCAtomic<mint> *)((_ThreadID + g_ThreadLocalOffsetPThread) + _iStorage * sizeof(mint));
	return (void *)pThreadLocal->f_Load();
#else
	DMibPDebugBreak; // Should never get here
#endif
	
	return nullptr;
}

#endif

#ifdef DMibDebuggerHelpers
assure_used void *fg_Debug_GetThreadLocal(mint _iStorage)
{
	return NSys::fg_Thread_GetLocal(_iStorage);
}
#endif

void *NSys::fg_Thread_GetLocalFast(mint _ThreadID, mint _iStorage)
{
	return fg_Thread_GetLocal(_ThreadID, _iStorage);
}

void *NSys::fg_Thread_GetLocalAlwaysSet(mint _ThreadID, mint _iStorage)
{
	return fg_Thread_GetLocal(_ThreadID, _iStorage);
}
void *NSys::fg_Thread_GetLocalAlwaysSetFast(mint _ThreadID, mint _iStorage)
{
	return fg_Thread_GetLocal(_ThreadID, _iStorage);
}

void NSys::fg_Thread_Sleep(fp32 _Seconds)
{
	// Sleeping may be interrupted via EINTR so we ensure we sleep the whole time period.
	struct timespec Timeout;
	Timeout.tv_sec = _Seconds.f_ToInt();
	Timeout.tv_nsec = ((_Seconds - (fp32)Timeout.tv_sec) * 1000000000.0).f_ToInt();
	while (		nanosleep(&Timeout, &Timeout) != 0
			&& 	errno == EINTR)
	{};
}

#ifndef DMibConfig_SemaphoreImplemented

class CImpSemaphore
{
public:
	pthread_mutex_t m_Lock;
	pthread_cond_t m_Condition;
	mint m_Value;
	mint m_Maximum;
	
	pthread_mutex_t *fp_GetMutex()
	{
		return (pthread_mutex_t *)&m_Lock;
	}

	CImpSemaphore(mint _Value, mint _Maximum)
	{
		f_Init();
		m_Value = _Value;
		m_Maximum = _Maximum;
	}

	~CImpSemaphore()
	{
		int ErrNo;
		if ((ErrNo = pthread_mutex_lock(fp_GetMutex())) != 0)
			DMibError(NPlatform::fg_FormatErrno("pthread_mutex_lock (semaphore destructor)", ErrNo));
		if ((ErrNo = pthread_mutex_unlock(fp_GetMutex())) != 0)
			DMibError(NPlatform::fg_FormatErrno("pthread_mutex_unlock (semaphore destructor)", ErrNo));

		int ErrNo;
		if ((ErrNo = pthread_cond_destroy(&m_Condition)) != 0)
			DMibError(NPlatform::fg_FormatErrno("pthread_cond_destroy (semaphore destructor)", ErrNo));
		if ((ErrNo = pthread_mutex_destroy(fp_GetMutex())) != 0)
			DMibError(NPlatform::fg_FormatErrno("pthread_mutex_destroy (semaphore destructor)", ErrNo));
	}
	
	void f_Init()
	{
		int ErrNo;
		pthread_mutexattr_t Attributes;
		pthread_mutexattr_init(&Attributes);
		pthread_mutexattr_settype(&Attributes, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutexattr_setpshared(&Attributes, PTHREAD_PROCESS_PRIVATE);
		if ((ErrNo = pthread_mutex_init(fp_GetMutex(), &Attributes)) != 0)
			DMibError(NPlatform::fg_FormatErrno("pthread_mutex_init (semaphore init)", ErrNo));
		if ((ErrNo = pthread_cond_init(&m_Condition, nullptr)) != 0)
			DMibError(NPlatform::fg_FormatErrno("pthread_cond_init (semaphore init)", ErrNo));
	}
	
	void f_Signal(mint _Count)
	{
		int ErrNo;
		if ((ErrNo = pthread_mutex_lock(fp_GetMutex())) != 0)
			DMibError(NPlatform::fg_FormatErrno("pthread_mutex_lock (semaphore signal)", ErrNo));
		if (m_Value + _Count > m_Maximum)
			_Count = m_Maximum - m_Value;
		m_Value += _Count;
		if (_Count < 16)
		{
			while (_Count--)
				pthread_cond_signal(&m_Condition);
		}
		else
			pthread_cond_broadcast(&m_Condition);
		if ((ErrNo = pthread_mutex_unlock(fp_GetMutex())) != 0)
			DMibError(NPlatform::fg_FormatErrno("pthread_mutex_unlock (semaphore signal)", ErrNo));
	}

	bool f_TryWait()
	{
		int ErrNo;
		bool bRet = false;
		if ((ErrNo = pthread_mutex_lock(fp_GetMutex())) != 0)
			DMibError(NPlatform::fg_FormatErrno("pthread_mutex_lock (semaphore try wait)", ErrNo));
		if (m_Value > 0)
		{
			--m_Value;
			bRet = true;
		}
		if ((ErrNo = pthread_mutex_unlock(fp_GetMutex())) != 0)
			DMibError(NPlatform::fg_FormatErrno("pthread_mutex_unlock (semaphore try wait)", ErrNo));
		return bRet;
	}

	void f_Wait()
	{
		int ErrNo;
		if ((ErrNo = pthread_mutex_lock(fp_GetMutex())) != 0)
			DMibError(NPlatform::fg_FormatErrno("pthread_mutex_lock (semaphore wait)", ErrNo));
		while (m_Value <= 0)
		{
			if ((ErrNo = pthread_cond_wait(&m_Condition, fp_GetMutex())) != 0)
			{
				if ((ErrNo = pthread_mutex_unlock(fp_GetMutex())) != 0)
					DMibError(NPlatform::fg_FormatErrno("pthread_mutex_unlock (semaphore after pthread_cond_wait failed)", ErrNo));
				DMibError(NPlatform::fg_FormatErrno("pthread_cond_wait (semaphore wait)", ErrNo));
			}
		}
		--m_Value;
		if ((ErrNo = pthread_mutex_unlock(fp_GetMutex())) != 0)
			DMibError(NPlatform::fg_FormatErrno("pthread_mutex_unlock (semaphore wait)", ErrNo));
	}

	bool f_WaitTimeout(fp32 _Timeout)
	{
		int ErrNo;
		bool bRet = true;
		CClockRaw TimeWait;
		TimeWait.f_Start();
		if ((ErrNo = pthread_mutex_lock(fp_GetMutex())) != 0)
			DMibError(NPlatform::fg_FormatErrno("pthread_mutex_lock (semaphore wait timeout)", ErrNo));
		fp64 Time = TimeWait.f_GetTime();
		while (Time < _Timeout)
		{
			if (m_Value <= 0)
			{
				timespec ToWait;
				
#ifdef DPlatformFamily_OSX
				struct timeval TimeVal;
				gettimeofday(&TimeVal, NULL);
				ToWait.tv_sec = TimeVal.tv_sec + 0;
				ToWait.tv_nsec = TimeVal.tv_usec * 1000;
#else
				clock_gettime(CLOCK_REALTIME, &ToWait);
#endif
				fp64 ToWaitLeft = (_Timeout - Time) + fp32(ToWait.tv_nsec) * (fp32(1.0f) / fp32(1000000000.0f));
				fp64 nSec = ToWaitLeft.f_Floor();
				ToWait.tv_sec += nSec.f_ToInt();
				ToWait.tv_nsec = ((ToWaitLeft - nSec)*fp32(1000000000.0f)).f_ToInt();
				
				int RetW = pthread_cond_timedwait(&m_Condition, fp_GetMutex(), &ToWait);
				if (RetW == ETIMEDOUT)
					break;
				else if (RetW)
				{
					if ((ErrNo = pthread_mutex_unlock(fp_GetMutex())) != 0)
						DMibError(NPlatform::fg_FormatErrno("pthread_mutex_unlock (semaphore after pthread_cond_timedwait failed)", ErrNo));
					DMibError(NPlatform::fg_FormatErrno("pthread_cond_timedwait (semaphore wait timeout)", RetW));
				}
			}
			else
			{
				bRet = false;
				--m_Value;
				break;
			}
			Time = TimeWait.f_GetTime();
		}
		if ((ErrNo = pthread_mutex_unlock(fp_GetMutex())) != 0)
			DMibError(NPlatform::fg_FormatErrno("pthread_mutex_unlock (semaphore wait timeout)", ErrNo));
		return bRet;
	}	
};

constinit NMemory::TCPoolAggregate<CImpSemaphore, 128, NThread::CSpinLockAggregate, CPoolType_Freeable, CAllocator_VirtualNoTracking> g_ImpSemaphorePool = {DAggregateInit};

void *NSys::fg_Semaphore_Alloc(mint _InitialCount, mint _MaximumCount)
{
	CImpSemaphore *pSemaphore = g_ImpSemaphorePool.f_New(_InitialCount, _MaximumCount);
	return pSemaphore;
}

void NSys::fg_Semaphore_ForkedChild(void * _pSemaphore)
{
	CImpSemaphore *pSemaphore = (CImpSemaphore *)_pSemaphore;
	pSemaphore->f_Init();
}

void NSys::fg_Semaphore_Free(void *_pSemaphore)
{
	CImpSemaphore *pSemaphore = (CImpSemaphore *)_pSemaphore;
	g_ImpSemaphorePool.f_Delete(pSemaphore);
}

void NSys::fg_Semaphore_Increase(void * _pSemaphore, mint _Count)
{
	CImpSemaphore *pSemaphore = (CImpSemaphore *)_pSemaphore;
	pSemaphore->f_Signal(_Count);
}

void NSys::fg_Semaphore_Wait(void * _pSemaphore)
{
	CImpSemaphore *pSemaphore = (CImpSemaphore *)_pSemaphore;
	pSemaphore->f_Wait();
}

bool NSys::fg_Semaphore_WaitTimeout(void * _pSemaphore, fp64 _Timeout)
{
	CImpSemaphore *pSemaphore = (CImpSemaphore *)_pSemaphore;
	return pSemaphore->f_WaitTimeout(_Timeout * CSystem_Time::fs_GetTimeSpeedReciprocal());
}

bool NSys::fg_Semaphore_TryWait(void * _pSemaphore)
{
	CImpSemaphore *pSemaphore = (CImpSemaphore *)_pSemaphore;
	return pSemaphore->f_TryWait();
	
}
#endif

#ifndef DPlatformFamily_Emscripten

struct CThreadStartParams
{
	FThreadProc *m_pThreadProc;
	void *m_pThreadParam;
	mint m_ParentThreadID;
	CStrNonTracked m_ThreadName;
};

void *fg_ThreadStartRoutine(void *_pParams)
{
	signal(SIGPIPE,SIG_IGN);

	NStorage::TCUniquePointer<CThreadStartParams, CAllocator_NonTrackedHeap> pThreadParams = fg_Explicit((CThreadStartParams *)_pParams);
	
	CThreadStartParams StartParams = *pThreadParams;
	
	fg_GetSys()->f_ThreadLocalCreateThread(NSys::fg_Thread_GetCurrentUID(), StartParams.m_ParentThreadID);

	pThreadParams.f_Clear();

#ifdef DPlatformFamily_Linux
	if (NLocal::g_f_pthread_setname_np)
		NLocal::g_f_pthread_setname_np(pthread_self(), StartParams.m_ThreadName.f_GetStr());
#elif defined(DPlatformFamily_OSX)
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
		mint m_MalterlibPriority;
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
		,	{ ~mint(0), 0 }
	};
};

static void fg_POSIX_MapThreadPriority(EExecutionPriority _Priority, int& _oSched, int& _oPrio)
{
	CPrioMap const* pCurEntry = & gc_MalterlibToPOSIXPriorityMap[0];
	CPrioMap const* pNextEntry;

	while (pCurEntry->m_MalterlibPriority != ~mint(0))
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
		, mint _StackSize
		, bool _bSuspended
		, const ch8 *_pThreadName
		, mint _Affinity
		, mint &_ThreadID
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

		0x8000	Normal 		(SCHED_OTHER, ? Prio)

		0x8001				(SCHED_RR, normal prio)
		0xe000	_RT_High	(SCHED_RR, high prio)

		0xe001	_RT_Highest (SCHED_FIFO)	
		0x10000	_RT_Highest (SCHED_FIFO)
*/

	bool bAlreadySetPriority = false;
#ifdef DPlatformFamily_OSX
	if (pthread_attr_set_qos_class_np)
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

	if (_StackSize != 0)
		pthread_attr_setstacksize (Data.f_UseThreadAttribs(), _StackSize);

#if defined(DMibPLinuxKernel)
	if (_Affinity)
	{
		// It is likely that we could just pass _Affinity as the cpuset but that would not be super portable.
		// For processors with > 32 (or 64) CPUs we will need to switch to a dynamic cpu_set_t
		cpu_set_t CPUSet;
		CPU_ZERO(&CPUSet);

		mint const nBits = sizeof(_Affinity) * 8;
		for (mint iB = 0
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

	_ThreadID = (mint)ThreadID;
	return (void *)ThreadID;
}


void NSys::fg_Thread_SetAffinity(void *_pThread, mint _Affinity)
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

	mint const nBits = sizeof(_Affinity) * 8;
	for (mint iB = 0
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
#ifdef DPlatformFamily_OSX
	if (pthread_set_qos_class_self_np && _pThread == NSys::fg_Thread_GetCurrent())
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

class CEventEmulation
{
public:
		
	int m_nWantLock;
	int m_State:1;
	int m_LockSequence;
	NMib::NThread::CSemaphore m_Semaphore;
	NMib::NThread::CMutual m_Lock;

	CEventEmulation(bool _bStartState)
		:
		m_Semaphore(0, TCLimitsInt<aint>::mc_Max)
	{
		m_State = _bStartState;
		m_LockSequence = 0;
		m_nWantLock = 0;
	}

	~CEventEmulation()
	{
		DMibLockTyped(NMib::NThread::CMutual, m_Lock);
	}
	
	void f_PrepareFork()
	{
		m_Lock.f_Lock();
		m_Lock.f_PrepareFork();
		m_Semaphore.f_PrepareFork();
	}

	void f_ForkedChild()
	{
		m_Semaphore.f_ForkedChild();
		m_Lock.f_ForkedChild();
		m_Lock.f_Unlock();
	}
	
	void f_ForkedParent()
	{
		m_Semaphore.f_ForkedParent();
		m_Lock.f_ForkedParent();
		m_Lock.f_Unlock();
	}
	
	bool f_TryWait()
	{
		DMibLockTyped(NMib::NThread::CMutual, m_Lock);
		
		if (m_State)
			return true;
		return false;
	}
	
	void f_Wait()
	{
		{
			DMibLockTyped(NMib::NThread::CMutual, m_Lock);

			if (m_State)
				return;

			++m_nWantLock;
		}

		m_Semaphore.f_Wait();		
	}
	
	inline_small bool f_WaitTimeout(fp64 _Timeout)
	{
		int LockSequence;
		{
			DMibLockTyped(NMib::NThread::CMutual, m_Lock);

			if (m_State)
				return false;
			
			LockSequence = m_LockSequence;
			
			++m_nWantLock;			
		}

		if (m_Semaphore.f_WaitTimeout(_Timeout))
			return true;
		
		{
			DMibLockTyped(NMib::NThread::CMutual, m_Lock);
			
			if (m_State)
				return false;
			
			if (LockSequence == m_LockSequence)
				--m_nWantLock;
		}
		
		return false;
	}
		
	void f_Signal()
	{
		{
			DMibLockTyped(NMib::NThread::CMutual, m_Lock);

			if (m_State)
				return;

			m_State = 1;

			m_Semaphore.f_Signal(m_nWantLock);
			
			++m_LockSequence;

			m_nWantLock = 0;
		}
	}

	void f_Reset()
	{
		{
			DMibLockTyped(NMib::NThread::CMutual, m_Lock);

			if (!m_State)
				return;

			m_State = 0;
		}
	}
};


NMemory::TCPoolAggregate<CEventEmulation, 128, NThread::CSpinLockAggregate, CPoolType_Freeable, CAllocator_VirtualNoTracking> g_EventEmulationPool = {};

void *NSys::fg_Event_Alloc(bool _bInitialSignal)
{
	auto *pEvent = g_EventEmulationPool.f_New(_bInitialSignal);
	return pEvent;
}

void NSys::fg_Event_Free(void *_pEvent)
{
	g_EventEmulationPool.f_Delete(((CEventEmulation *)_pEvent));
}


void NSys::fg_Event_PrepareFork(void *_pEvent)
{
	((CEventEmulation *)_pEvent)->f_PrepareFork();
}

void NSys::fg_Event_ForkedChild(void *_pEvent)
{
	((CEventEmulation *)_pEvent)->f_ForkedChild();
}

void NSys::fg_Event_ForkedParent(void *_pEvent)
{
	((CEventEmulation *)_pEvent)->f_ForkedParent();
}

void NSys::fg_Event_SetSignaled(void * _pEvent)
{
	((CEventEmulation *)_pEvent)->f_Signal();
}

void NSys::fg_Event_ResetSignaled(void * _pEvent)
{
	((CEventEmulation *)_pEvent)->f_Reset();
}

void NSys::fg_Event_Wait(void * _pEvent)
{
	((CEventEmulation *)_pEvent)->f_Wait();
}

bool NSys::fg_Event_WaitTimeout(void * _pEvent, fp64 _Timeout)
{
	return ((CEventEmulation *)_pEvent)->f_WaitTimeout(_Timeout);
}

bool NSys::fg_Event_TryWait(void * _pEvent)
{
	return ((CEventEmulation *)_pEvent)->f_TryWait();
}

#endif
