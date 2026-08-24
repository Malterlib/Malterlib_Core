// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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
// glibc exports these libthread_db descriptors (the private struct pthread thread-local layout) in its dynamic symbol
// table only from glibc 2.34 onwards (the release that merged libpthread into libc; Ubuntu 21.10+, first LTS 22.04);
// before 2.34 they were local symbols in libpthread.so. They are resolved dynamically against the system glibc in
// NLocal::fg_GetSymbols (see Malterlib_Core_PlatformImp_Linux.cpp), so a build against an older SDK glibc still links.
// The NLocal::g_p_thread_db_* pointers stay null on systems whose glibc does not export them, in which case
// fg_Glibc_ValidateThreadLocalLayout reports an unsupported-layout error.

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
			!NLocal::g_p_thread_db_pthread_specific
			|| !NLocal::g_p_thread_db_pthread_key_data_data
			|| !NLocal::g_p_thread_db_pthread_key_data_seq
			|| !NLocal::g_p_thread_db_pthread_key_data_level2_data
			|| !NLocal::g_p_thread_db_sizeof_pthread_key_data
		)
		{
			DMibErrorSystemImp("This glibc does not export the _thread_db_* thread-local layout descriptors (needs glibc 2.34+ / Ubuntu 21.10+)");
		}

		if
		(
			NLocal::g_p_thread_db_pthread_specific[EGlibcThreadDbDescriptor_SizeBits] % 8 != 0
			|| NLocal::g_p_thread_db_pthread_key_data_data[EGlibcThreadDbDescriptor_SizeBits] != sizeof(mint) * 8
			|| NLocal::g_p_thread_db_pthread_key_data_seq[EGlibcThreadDbDescriptor_SizeBits] != sizeof(mint) * 8
			|| NLocal::g_p_thread_db_pthread_key_data_level2_data[EGlibcThreadDbDescriptor_SizeBits] != *NLocal::g_p_thread_db_sizeof_pthread_key_data * 8
			|| !NLocal::g_p_thread_db_pthread_key_data_level2_data[EGlibcThreadDbDescriptor_nElements]
			|| NLocal::g_p_thread_db_pthread_specific[EGlibcThreadDbDescriptor_SizeBits] / 8 % sizeof(mint) != 0
		)
		{
			DMibErrorSystemImp("Unsupported glibc thread local layout");
		}
	}

	uint8 *fg_Glibc_GetThreadLocalLevel2(mint _ThreadID, mint _iStorage, bool _bCreate)
	{
		fg_Glibc_ValidateThreadLocalLayout();

		mint nPerLevel2 = NLocal::g_p_thread_db_pthread_key_data_level2_data[EGlibcThreadDbDescriptor_nElements];
		mint nLevel2 = NLocal::g_p_thread_db_pthread_specific[EGlibcThreadDbDescriptor_SizeBits] / 8 / sizeof(mint);
		mint iLevel2 = _iStorage / nPerLevel2;
		if (iLevel2 >= nLevel2)
			DMibErrorSystemImp("glibc thread local index out of range");

		auto pLevel2Pointers = reinterpret_cast<NAtomic::TCAtomic<mint> *>
			(
				reinterpret_cast<uint8 *>(_ThreadID)
				+ NLocal::g_p_thread_db_pthread_specific[EGlibcThreadDbDescriptor_Offset]
			)
		;
		auto &pLevel2Pointer = pLevel2Pointers[iLevel2];
		mint pLevel2 = pLevel2Pointer.f_Load(NAtomic::EMemoryOrder_Acquire);
		if (pLevel2 || !_bCreate)
			return reinterpret_cast<uint8 *>(pLevel2);

		if (!iLevel2)
			DMibErrorSystemImp("glibc first thread local block is not initialized");

		void *pNewLevel2 = calloc(nPerLevel2, *NLocal::g_p_thread_db_sizeof_pthread_key_data);
		if (!pNewLevel2)
			DMibErrorSystemImp(NMib::NPlatform::fg_FormatErrno("calloc (thread local block)", errno));

		mint Expected = 0;
		if
		(
			pLevel2Pointer.f_CompareExchangeStrong
				(
					Expected
					, reinterpret_cast<mint>(pNewLevel2)
					, NAtomic::EMemoryOrder_AcquireRelease
					, NAtomic::EMemoryOrder_Acquire
				)
		)
		{
			return reinterpret_cast<uint8 *>(pNewLevel2);
		}

		free(pNewLevel2);
		return reinterpret_cast<uint8 *>(Expected);
	}

	NAtomic::TCAtomic<mint> *fg_Glibc_GetThreadLocalField(uint8 *_pLevel2, mint _iStorage, uint32 const *_Descriptor)
	{
		mint nPerLevel2 = NLocal::g_p_thread_db_pthread_key_data_level2_data[EGlibcThreadDbDescriptor_nElements];
		mint iLevel2 = _iStorage % nPerLevel2;
		return reinterpret_cast<NAtomic::TCAtomic<mint> *>
			(
				_pLevel2
				+ iLevel2 * *NLocal::g_p_thread_db_sizeof_pthread_key_data
				+ _Descriptor[EGlibcThreadDbDescriptor_Offset]
			)
		;
	}

	mint fg_Glibc_GetThreadLocalSequence(mint _iStorage)
	{
		uint8 Probe = 0;
		void *pOldData = pthread_getspecific((pthread_key_t)_iStorage);
		if (auto ErrNo = pthread_setspecific((pthread_key_t)_iStorage, &Probe))
			DMibErrorSystemImp(NMib::NPlatform::fg_FormatErrno("pthread_setspecific (thread local sequence probe)", ErrNo));

		uint8 *pLevel2 = fg_Glibc_GetThreadLocalLevel2(NSys::fg_Thread_GetCurrentUID(), _iStorage, false);
		bool bValid = false;
		mint Sequence = 0;
		if (pLevel2)
		{
			Sequence = fg_Glibc_GetThreadLocalField(pLevel2, _iStorage, NLocal::g_p_thread_db_pthread_key_data_seq)->f_Load(NAtomic::EMemoryOrder_Acquire);
			bValid = fg_Glibc_GetThreadLocalField(pLevel2, _iStorage, NLocal::g_p_thread_db_pthread_key_data_data)->f_Load(NAtomic::EMemoryOrder_Acquire) == reinterpret_cast<mint>(&Probe);
		}

		if (auto ErrNo = pthread_setspecific((pthread_key_t)_iStorage, pOldData))
			DMibErrorSystemImp(NMib::NPlatform::fg_FormatErrno("pthread_setspecific (thread local sequence restore)", ErrNo));
		if (!bValid)
			DMibErrorSystemImp("Unsupported glibc thread local layout");

		return Sequence;
	}

	void fg_Glibc_Thread_SetLocal(mint _ThreadID, mint _iStorage, void *_pData)
	{
		uint8 *pLevel2 = fg_Glibc_GetThreadLocalLevel2(_ThreadID, _iStorage, false);
		if (!pLevel2 && !_pData)
			return;

		mint Sequence = fg_Glibc_GetThreadLocalSequence(_iStorage);
		if (!pLevel2)
			pLevel2 = fg_Glibc_GetThreadLocalLevel2(_ThreadID, _iStorage, true);

		if (_pData)
		{
			auto pSpecificUsed = reinterpret_cast<NAtomic::TCAtomic<uint8> *>
				(
					reinterpret_cast<uint8 *>(_ThreadID)
					+ NLocal::g_p_thread_db_pthread_specific[EGlibcThreadDbDescriptor_Offset]
					+ NLocal::g_p_thread_db_pthread_specific[EGlibcThreadDbDescriptor_SizeBits] / 8
				)
			;
			pSpecificUsed->f_Store(1, NAtomic::EMemoryOrder_Release);
		}

		fg_Glibc_GetThreadLocalField(pLevel2, _iStorage, NLocal::g_p_thread_db_pthread_key_data_seq)->f_Store(Sequence, NAtomic::EMemoryOrder_Release);
		fg_Glibc_GetThreadLocalField(pLevel2, _iStorage, NLocal::g_p_thread_db_pthread_key_data_data)->f_Exchange(reinterpret_cast<mint>(_pData));
	}

	#ifndef DMibStaticThreadLocals
	void *fg_Glibc_Thread_GetLocal(mint _ThreadID, mint _iStorage)
	{
		uint8 *pLevel2 = fg_Glibc_GetThreadLocalLevel2(_ThreadID, _iStorage, false);
		if (!pLevel2)
			return nullptr;

		auto pData = fg_Glibc_GetThreadLocalField(pLevel2, _iStorage, NLocal::g_p_thread_db_pthread_key_data_data);
		mint Data = pData->f_Load(NAtomic::EMemoryOrder_Acquire);
		if (!Data)
			return nullptr;

		mint Sequence = fg_Glibc_GetThreadLocalField(pLevel2, _iStorage, NLocal::g_p_thread_db_pthread_key_data_seq)->f_Load(NAtomic::EMemoryOrder_Acquire);
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

static_assert(sizeof(pthread_key_t) <= sizeof(mint), "pthread_key_t must be the same size or smaller than a mint.");
static_assert(sizeof(pthread_t) <= sizeof(mint), "pthread_t must be the same size or smaller than a mint.");
static_assert(sizeof(pid_t) <= sizeof(mint), "pid_t must be the same size or smaller than a mint.");

// *************************************************************************************************************************
// POSIX Thread Implementation
// *************************************************************************************************************************

mint NSys::fg_Thread_AllocLocalWithDestructor(void (_pDestructor)(void*))
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
	return (mint)pKey;
}

void NSys::fg_Thread_FreeLocalWithDestructor(mint _iStorage)
{
	pthread_key_t pKey = (pthread_key_t)_iStorage;
	if (auto ErrNo = pthread_key_delete(pKey))
		DMibErrorSystemImp(NPlatform::fg_FormatErrno("pthread_key_delete (thread local free)", ErrNo));
}

#ifdef DPlatformFamily_macOS
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
		if (auto ErrNo = pthread_setspecific(pKey, _pData))
			DMibErrorSystemImp(NPlatform::fg_FormatErrno("pthread_setspecific (thread local set)", ErrNo));
		return;
	}
#ifdef DPlatformFamily_macOS
	// The thread UID is the pthread_t and the thread specific data lives at
	// g_ThreadLocalOffsetPThread from it, matching fg_Thread_GetLocal
	NAtomic::TCAtomic<mint> *pThreadLocal = (NAtomic::TCAtomic<mint> *)((_ThreadID + g_ThreadLocalOffsetPThread) + _iStorage * sizeof(mint));
	pThreadLocal->f_Exchange((mint)_pData);
#elif defined(DPlatformFamily_Linux) && defined(DMibAssumeGlibc)
	fg_Glibc_Thread_SetLocal(_ThreadID, _iStorage, _pData);
#else
	DMibPDebugBreak; // Should never get here
#endif
}

#ifndef DMibStaticThreadLocals

mint NSys::fg_Thread_AllocLocal()
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
	return (mint)pKey;
}

void NSys::fg_Thread_FreeLocal(mint _iStorage)
{
	pthread_key_t pKey = (pthread_key_t)_iStorage;
	if (auto ErrNo = pthread_key_delete(pKey))
		DMibErrorSystemImp(NPlatform::fg_FormatErrno("pthread_key_delete (thread local free)", ErrNo));
}

void NSys::fg_Thread_SetLocal(mint _iStorage, void *_pData)
{
	pthread_key_t pKey = (pthread_key_t)_iStorage;
	if (auto ErrNo = pthread_setspecific(pKey, _pData))
		DMibErrorSystemImp(NPlatform::fg_FormatErrno("pthread_setspecific (thread local set)", ErrNo));
}

void NSys::fg_Thread_SetLocal(mint _ThreadID, mint _iStorage, void *_pData)
{
	mint ThisThread = fg_Thread_GetCurrentUID();
	if (ThisThread == _ThreadID)
	{
		fg_Thread_SetLocal(_iStorage, _pData);
		return;
	}
#ifdef DPlatformFamily_macOS
	// The thread UID is the pthread_t and the thread specific data lives at
	// g_ThreadLocalOffsetPThread from it, matching fg_Thread_GetLocal
	NAtomic::TCAtomic<mint> *pThreadLocal = (NAtomic::TCAtomic<mint> *)((_ThreadID + g_ThreadLocalOffsetPThread) + _iStorage * sizeof(mint));
	pThreadLocal->f_Exchange((mint)_pData);
#elif defined(DPlatformFamily_Linux) && defined(DMibAssumeGlibc)
	fg_Glibc_Thread_SetLocal(_ThreadID, _iStorage, _pData);
#else
	DMibPDebugBreak; // Should never get here
#endif
}

void *NSys::fg_Thread_GetLocal(mint _ThreadID, mint _iStorage)
{
	if (NSys::fg_Thread_GetCurrentUID() == _ThreadID)
		return fg_Thread_GetLocal(_iStorage);

#if defined(DPlatformFamily_macOS)
	NAtomic::TCAtomic<mint> *pThreadLocal = (NAtomic::TCAtomic<mint> *)((_ThreadID + g_ThreadLocalOffsetPThread) + _iStorage * sizeof(mint));
	return (void *)pThreadLocal->f_Load();
#elif defined(DPlatformFamily_Linux) && defined(DMibAssumeGlibc)
	return fg_Glibc_Thread_GetLocal(_ThreadID, _iStorage);
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

#ifdef DPlatformFamily_macOS
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

constinit NMemory::TCPoolAggregate<CImpSemaphore, 128, NThread::CLowLevelLockAggregate, CPoolType_Freeable, CAllocator_VirtualNoTracking> g_ImpSemaphorePool = {DAggregateInit};

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
	[[maybe_unused]] CImpSemaphore *pSemaphore = (CImpSemaphore *)_pSemaphore;
#ifdef DMibSanitizerEnabled_Thread
	DMibLock(g_ImpSemaphorePool);
	pSemaphore->~CImpSemaphore();
#else
	g_ImpSemaphorePool.f_Delete(pSemaphore);
#endif
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

class CEventEmulation
{
public:

	uint32 m_nWantLock;
	uint32 m_State:1;
	uint32 m_LockSequence;
	NMib::NThread::CSemaphore m_Semaphore;
	NMib::NThread::CLowLevelLock m_Lock;

	CEventEmulation(bool _bStartState)
		: m_Semaphore(0, TCLimitsInt<aint>::mc_Max)
	{
		m_State = _bStartState;
		m_LockSequence = 0;
		m_nWantLock = 0;
	}

	~CEventEmulation()
	{
		DMibLock(m_Lock);
	}

	void f_PrepareFork()
	{
		m_Lock.f_Lock();
		m_Semaphore.f_PrepareFork();
	}

	void f_ForkedChild()
	{
		m_Semaphore.f_ForkedChild();
		m_Lock.f_ForkedChildLocked();
		m_Lock.f_Unlock();
	}

	void f_ForkedParent()
	{
		m_Semaphore.f_ForkedParent();
		m_Lock.f_Unlock();
	}

	bool f_TryWait()
	{
		DMibLock(m_Lock);

		if (m_State)
			return true;
		return false;
	}

	void f_Wait()
	{
		{
			DMibLock(m_Lock);

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
			DMibLock(m_Lock);

			if (m_State)
				return false;

			LockSequence = m_LockSequence;

			++m_nWantLock;
		}

		if (m_Semaphore.f_WaitTimeout(_Timeout))
			return true;

		{
			DMibLock(m_Lock);

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
			DMibLock(m_Lock);

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
			DMibLock(m_Lock);

			if (!m_State)
				return;

			m_State = 0;
		}
	}
};


NMemory::TCPoolAggregate<CEventEmulation, 128, NThread::CLowLevelLockAggregate, CPoolType_Freeable, CAllocator_VirtualNoTracking> g_EventEmulationPool = {};

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

mint NSys::fg_Thread_GetCurrentUIDAlternate()
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

mint NSys::fg_GetThreadSelf_Safe()
{
	return (mint)pthread_self();
}

mint NSys::fg_GetThreadLocal_Safe(mint _iVariable)
{
	return (mint)pthread_getspecific((pthread_key_t)_iVariable);
}

#endif
