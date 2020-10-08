// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Cryptography/UUID>

#define _DARWIN_USE_64_BIT_INODE

#define DMibAllowCodeStandardViolations 1

#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Authorization.h>
#include <Security/Security.h>
#include <ApplicationServices/ApplicationServices.h>
#include <CoreServices/CoreServices.h>

#include <Mib/Concurrency/ThreadSafeQueue>

#include <mach/mach_time.h>
#include <sys/utsname.h>
#include <crt_externs.h>
#include <sys/clonefile.h>
#include <os/lock.h>

using namespace NMib;
using namespace NMib::NStr;
using namespace NMib::NTime;
using namespace NMib::NMemory;
using namespace NMib::NContainer;

#include <Mib/Core/PlatformSpecific/PosixErrNo>
#include <Mib/Core/PlatformSpecific/OSXOSStatus>

#ifdef DMibDynamicLibrary
bool g_bIsSharedLibrary = true;
#else
bool g_bIsSharedLibrary = false;
#endif

bool g_bRegisteredAtFork = false;

void fg_ForkPrepare();
void fg_ForkParentOrChild();
void fg_MalterlibMallocOverride_CanStartThreads();

namespace NMib
{
	namespace NSys
	{
		extern int g_OperatingSystemMajor;
		extern int g_OperatingSystemMinor;
		extern int g_OperatingSystemFix;
	}
}

extern "C"
{
	void *nontracked_malloc(size_t __size)
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
#		if DMibConfig_MalterlibMemoryManager_Debug
			return NMib::NMemory::CAllocator_NonTrackedHeap::f_AllocDebug(__size, DMibPFile, DMibPLine, EHeapDebugFlag_Ignore);
#		else
			return NMib::NMemory::CAllocator_NonTrackedHeap::f_Alloc(__size);
#		endif
	}

	void *nontracked_calloc (size_t __nmemb, size_t __size)
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
		mint Size = __nmemb * __size;
#		if DMibConfig_MalterlibMemoryManager_Debug
			auto pMem = NMib::NMemory::CAllocator_NonTrackedHeap::f_AllocDebug(Size, DMibPFile, DMibPLine, EHeapDebugFlag_Ignore);
#		else
			auto pMem = NMib::NMemory::CAllocator_NonTrackedHeap::f_Alloc(Size);
#		endif
		fg_MemClear(pMem, Size);
		return pMem;
	}

	void *nontracked_realloc (void *__ptr, size_t __size)
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
#		if DMibConfig_MalterlibMemoryManager_Debug
			return NMib::NMemory::CAllocator_NonTrackedHeap::f_ResizeDebug(__ptr, __size, 0, DMibPFile, DMibPLine, EHeapDebugFlag_Ignore);
#		else
			return NMib::NMemory::CAllocator_NonTrackedHeap::f_Resize(__ptr, __size, 0);
#		endif
	}

	void nontracked_free (void *__ptr)
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
		return NMib::NMemory::CAllocator_NonTrackedHeap::f_FreeNoSize(__ptr);
	}

	void nontracked_cfree (void *__ptr)
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
		return NMib::NMemory::CAllocator_NonTrackedHeap::f_FreeNoSize(__ptr);
	}
	void *nontracked_memalign (size_t __alignment, size_t __size)
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
#		if DMibConfig_MalterlibMemoryManager_Debug
			return NMib::NMemory::CAllocator_NonTrackedHeap::f_AllocAlignedDebug(__size, __alignment, DMibPFile, DMibPLine, EHeapDebugFlag_Ignore);
#		else
			return NMib::NMemory::CAllocator_NonTrackedHeap::f_AllocAligned(__size, __alignment);
#		endif
	}
	int nontracked_posix_memalign(void **_pOutput, size_t _Alignment, size_t _Size)
	{
		*_pOutput = nontracked_memalign(_Alignment, _Size);
		return 0;
	}
	void *nontracked_valloc (size_t __size)
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
#		if DMibConfig_MalterlibMemoryManager_Debug
			return NMib::NMemory::CAllocator_NonTrackedHeap::f_AllocAlignedDebug(__size, NMib::NSys::NPrivate::g_PageSize, DMibPFile, DMibPLine, EHeapDebugFlag_Ignore);
#		else
			return NMib::NMemory::CAllocator_NonTrackedHeap::f_AllocAligned(__size, NMib::NSys::NPrivate::g_PageSize);
#		endif
	}
	void * nontracked_pvalloc (size_t __size)
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
#		if DMibConfig_MalterlibMemoryManager_Debug
			return NMib::NMemory::CAllocator_NonTrackedHeap::f_AllocAlignedDebug(__size, NMib::NSys::NPrivate::g_PageSize, DMibPFile, DMibPLine, EHeapDebugFlag_Ignore);
#		else
			return NMib::NMemory::CAllocator_NonTrackedHeap::f_AllocAligned(__size, NMib::NSys::NPrivate::g_PageSize);
#		endif
	}
	size_t nontracked_malloc_usable_size (void *__ptr)
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
		return NMib::NMemory::CAllocator_NonTrackedHeap::f_Size(__ptr);
	}
}

// *************************************************************************************************************************
// POSIX Implementation
// *************************************************************************************************************************

#define DMibPMachKernel
#define DMibConfig_SemaphoreImplemented

#include "Malterlib_Core_PlatformImp_POSIX_PThread.hpp"
#include "Malterlib_Core_PlatformImp_POSIX.imp.h"
#include "Malterlib_Core_PlatformImp_POSIX_File.hpp"
#include "Malterlib_Core_PlatformImp_POSIX_Console.hpp"
#include "Malterlib_Core_PlatformImp_POSIX_Environment.hpp"
#include "Malterlib_Core_PlatformImp_POSIX_Module.hpp"
#include "Malterlib_Core_PlatformImp_POSIX_User.hpp"
#include "Malterlib_Core_PlatformImp_POSIX_VirtualMemory.hpp"
#include "Malterlib_Core_PlatformImp_POSIX_Net.imp.h"

// *************************************************************************************************************************
// OSX Implementation
// *************************************************************************************************************************

//#if !TARGET_OS_ASPEN
//	#include <CoreServices/CoreServices.h>
//	#include <Carbon/Carbon.h>
//#endif
extern "C"
{
	#include <sys/mman.h>
	#include <mach-o/getsect.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netdb.h>
	#include <netinet/in.h>
	#include <sys/ioctl.h>
	//#include <time.h>
	#include <crt_externs.h>
	#include <sys/kern_control.h>
	#include <sys/sys_domain.h>
	#include <sys/param.h>
	#include <sys/mount.h>
	#include <libkern/OSAtomic.h>
	#include <sys/sysctl.h>
	#include <copyfile.h>
	#include <execinfo.h>
	#include <dlfcn.h>
	#include <sys/event.h>
	#include <sys/time.h>
	#include <crt_externs.h>
	#include <uuid/uuid.h>	// For UUID gen
}


#include <mach-o/dyld.h>

#include <dlfcn.h>

#include <cxxabi.h>

//#include <files.h>

#include "Malterlib_Core_PlatformImp_MacOSX_ObjCPP.h"
#if defined(DArchitecture_x64) || defined(DArchitecture_x64)
#include <xmmintrin.h>
#endif

static inline_small class CSystemMacOSX *fg_GetLocalSys();

void NSys::fg_System_EnableFloatingPointExceptions()
{
#if defined(DArchitecture_x86) || defined(DArchitecture_x64)
	_MM_SET_EXCEPTION_MASK(_MM_MASK_INEXACT);
#endif
}

void calling_convention_c fg_Malterlib_MakeActive()
{

}

CStr fg_EscapeString(CStr _In)
{
	return _In.f_Replace("\"","\\\"");
	/*const ch8 *pStr = _In;
	CStr Ret;
	while(*pStr)
	{
		if (*pStr == '"')
		{
			Ret.f_AddStr("\\\"");
		}
		else
		{
			Ret.f_AddChar(*pStr);
		}
		++pStr;
	}
	return Ret;*/
}

class align_cacheline CImpSemaphore
{
public:

	NMib::NThread::CLowLevelLock m_Lock;

	semaphore_t m_Semaphore;

	mint m_Value;
	mint m_Maximum;

	CImpSemaphore(mint _Value, mint _Maximum)
		: m_Semaphore(MACH_PORT_NULL)
	{
		f_Init();
		m_Value = _Value;
		m_Maximum = _Maximum;
	}

	~CImpSemaphore() noexcept(false)
	{
		{
			DMibLock(m_Lock);
		}

		if (m_Semaphore != MACH_PORT_NULL)
		{
			kern_return_t Result = semaphore_destroy(mach_task_self(), m_Semaphore);
			if (Result != KERN_SUCCESS)
				DMibError((CFStr256::CFormat("semaphore_destroy failed: 0x{nfh} {}") << Result << mach_error_string(Result)).f_GetStr().f_GetStr());
		}
	}

	void f_ForkedChild()
	{
		kern_return_t Result = semaphore_create(mach_task_self(), &m_Semaphore, SYNC_POLICY_FIFO, 0);

		m_Lock.f_ForkedChildUnlocked();

		if (Result != KERN_SUCCESS)
			DMibError((CFStr256::CFormat("semaphore_create failed: 0x{nfh} {}") << Result << mach_error_string(Result)).f_GetStr().f_GetStr());
	}

	void f_Init()
	{
		kern_return_t Result = semaphore_create(mach_task_self(), &m_Semaphore, SYNC_POLICY_FIFO, 0);

		m_Lock.f_Construct();

		if (Result != KERN_SUCCESS)
			DMibError((CFStr256::CFormat("semaphore_create failed: 0x{nfh} {}") << Result << mach_error_string(Result)).f_GetStr().f_GetStr());
	}

	void f_Signal(mint _Count)
	{
		DMibLock(m_Lock);
		if (m_Value + _Count > m_Maximum)
			_Count = m_Maximum - m_Value;
		m_Value += _Count;

		while (_Count--)
			semaphore_signal(m_Semaphore);
	}

	bool f_TryWait()
	{
		bool bRet = false;
		DMibLock(m_Lock);
		if (m_Value > 0)
		{
			--m_Value;
			bRet = true;
		}
		return bRet;
	}

	void f_Wait()
	{
		DMibLock(m_Lock);
		while (m_Value <= 0)
		{
			{
				DMibUnlock(m_Lock);
				kern_return_t Result = semaphore_wait(m_Semaphore);
				if (Result != KERN_SUCCESS && Result != KERN_ABORTED)
					DMibError((CFStr256::CFormat("semaphore_wait failed: 0x{nfh} {}") << Result << mach_error_string(Result)).f_GetStr().f_GetStr());
			}
		}
		--m_Value;
	}

	bool f_WaitTimeout(fp32 _Timeout)
	{
		bool bRet = true;
		CClockRaw TimeWait;
		TimeWait.f_Start();
		DMibLock(m_Lock);
		fp64 Time = TimeWait.f_GetTime();
		while (Time < _Timeout)
		{
			if (m_Value <= 0)
			{
				mach_timespec_t ToWait;
				fp64 ToWaitLeft = _Timeout - Time;
				fp64 nSec = ToWaitLeft.f_Floor();
				ToWait.tv_sec=nSec.f_ToInt();
				ToWait.tv_nsec=((ToWaitLeft - nSec)*fp32(1000000000.0f)).f_ToInt();

				{
					DMibUnlock(m_Lock);
					kern_return_t Result = semaphore_timedwait(m_Semaphore, ToWait);
					if (Result == KERN_OPERATION_TIMED_OUT)
						;
					else if (Result != KERN_SUCCESS && Result != KERN_ABORTED)
						DMibError((CFStr256::CFormat("semaphore_timedwait failed: 0x{nfh}") << Result << mach_error_string(Result)).f_GetStr().f_GetStr());
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
	pSemaphore->f_ForkedChild();
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

void NSys::fg_System_GenerateUUID(NCryptography::CUniversallyUniqueIdentifier &_UUID)
{
	static_assert(sizeof(uuid_t) == sizeof(_UUID));
	uuid_generate((unsigned char *)&_UUID);
#	if DMibEnableSafeCheck > 0
		uuid_string_t RetStr;
		uuid_unparse((unsigned char *)&_UUID, RetStr);
#	endif
	_UUID.m_TimeLow = fg_ByteSwapBE(_UUID.m_TimeLow);
	_UUID.m_TimeMid = fg_ByteSwapBE(_UUID.m_TimeMid);
	_UUID.m_TimeHiAndVersion = fg_ByteSwapBE(_UUID.m_TimeHiAndVersion);
#	if DMibEnableSafeCheck > 0
		DMibFastCheck(_UUID.f_GetAsStaticString(NCryptography::EUniversallyUniqueIdentifierFormat_Bare).f_CmpNoCase(RetStr) == 0);
#	endif
}

NStr::CStr NSys::fg_System_GenerateUUID()
{
	uuid_t Ret;
	uuid_generate(Ret);

	uuid_string_t RetStr;
	uuid_unparse(Ret, RetStr);

	// {7EE072A7-458D-491f-ACCF-447AD4BE8DBF}
	return CStr(CStr::CFormat("{{{}}") << RetStr);
}

void NSys::fg_Security_GenerateHighEntropyData(uint8 *_pData, mint _nBytes)
{
	if (NMib::NPlatform::fg_ReadProcFS("/dev/urandom", _pData, _nBytes) != _nBytes)
		DMibPDebugBreak;
}


NMib::NSys::EDesktopEnvironment NMib::NSys::fg_DesktopEnvironment_Get()
{
	return EDesktopEnvironment_OSX;
}

class CSystemMacOSX : public CSystem
{
public:
	CSystem_POSIX m_Posix;

	pthread_key_t m_ForkThreadLocal;

	NMib::NStorage::TCAggregate<CPOSIXSocketContext, 64> m_SocketContext;

	uint64 m_TimerFrequency;

	bool m_bForkedChild = false;

	static void fs_ForkPrepare()
	{
		auto &Sys = *fg_GetLocalSys();
		if (!pthread_getspecific(Sys.m_ForkThreadLocal))
		{
			pthread_setspecific(Sys.m_ForkThreadLocal, (void *)(mint)getpid());
			Sys.m_Posix.f_GetMalterlibDisableStdErrLog(); // getenv fails on forked process, to workaround this here

			Sys.m_Posix.m_ForkLock.f_Lock();
			Sys.m_Posix.m_ForkLock.f_PrepareFork();
			Sys.f_PrepareFork();
			g_EventEmulationPool.f_Lock();
			g_ImpSemaphorePool.f_Lock();
		}
	}

	static void fs_ForkParentOrChild()
	{
		auto &Sys = *fg_GetLocalSys();
		if (Sys.m_ForkThreadLocal == TCLimitsInt<pthread_key_t>::mc_Max)
			return;
		void *Current = pthread_getspecific(Sys.m_ForkThreadLocal);
		if (Current)
		{
#ifdef DMibSanitizerEnabled_Thread
			if (Current == (void *)(mint)getpid())
				__tsan_forked_parent();
			else
				__tsan_forked_child();
#endif

			pthread_setspecific(Sys.m_ForkThreadLocal, 0);
			if (Current != (void *)(mint)getpid())
			{
				g_ImpSemaphorePool.f_ForkedChildLocked();
				g_EventEmulationPool.f_ForkedChildLocked();
				Sys.m_bForkedChild = true;
			}
			g_ImpSemaphorePool.f_Unlock();
			g_EventEmulationPool.f_Unlock();
			if (Current == (void *)(mint)getpid())
			{
				Sys.f_ForkedParent(); // Parent
				Sys.m_Posix.m_ForkLock.f_ForkedParent();
				Sys.m_Posix.m_ForkLock.f_Unlock();
			}
			else
			{
				Sys.f_ForkedChild(); // Child
				Sys.m_Posix.m_ForkLock.f_ForkedChild();
				Sys.m_Posix.m_ForkLock.f_Unlock();
			}
		}
	}
	static void fs_ForkParent()
	{
		auto &Sys = *fg_GetLocalSys();
		if (pthread_getspecific(Sys.m_ForkThreadLocal))
		{
#ifdef DMibSanitizerEnabled_Thread
			__tsan_forked_parent();
#endif
			pthread_setspecific(Sys.m_ForkThreadLocal, 0);
			g_ImpSemaphorePool.f_Unlock();
			g_EventEmulationPool.f_Unlock();
			Sys.f_ForkedParent();
			Sys.m_Posix.m_ForkLock.f_ForkedParent();
			Sys.m_Posix.m_ForkLock.f_Unlock();
		}
	}

	static void fs_ForkChild()
	{
		auto &Sys = *fg_GetLocalSys();
		if (pthread_getspecific(Sys.m_ForkThreadLocal))
		{
#ifdef DMibSanitizerEnabled_Thread
			__tsan_forked_child();
#endif
			pthread_setspecific(Sys.m_ForkThreadLocal, 0);
			Sys.m_bForkedChild = true;
			g_bCanStartThreads = false;
			g_ImpSemaphorePool.f_ForkedChildLocked();
			g_EventEmulationPool.f_ForkedChildLocked();
			g_ImpSemaphorePool.f_Unlock();
			g_EventEmulationPool.f_Unlock();
			Sys.f_ForkedChild();
			Sys.m_Posix.m_ForkLock.f_ForkedChild();
			Sys.m_Posix.m_ForkLock.f_Unlock();
			g_bCanStartThreads = true;
			Sys.f_MemoryManager_CanStartThreads();
			fg_MalterlibMallocOverride_CanStartThreads();
		}
	}

	CSystemMacOSX()
		: CSystem(g_bIsSharedLibrary)
		, m_SocketContext{DAggregateInit}
		, m_FileChangeNoticationContext{DAggregateInit}
		, m_TimerFrequency{}
		, m_ForkThreadLocal(TCLimitsInt<pthread_key_t>::mc_Max)
	{
		fg_MemClear(m_SocketContext);

		fp_InitComplete();
	}

	~CSystemMacOSX()
	{
	}

	void f_InitThreadLocal()
	{
		pthread_key_create(&m_ForkThreadLocal, nullptr);
	}

	void f_Init()
	{
		CSystem::f_Init();
	}

	void f_DestroyThreadSpecific()
	{
		CSystem::f_PreDestructThreadSpecific();

		m_Posix.f_DestroyThreadSpecific();

		if (m_FileChangeNoticationContext.f_IsConstructed())
			m_FileChangeNoticationContext.f_Destruct();

		CSystem::f_DestructThreadSpecific();
	}

	void f_Destruct()
	{
		m_Posix.f_Destruct();

		if (m_SocketContext.f_IsConstructed())
			m_SocketContext.f_Destruct();

		CSystem::f_Destruct();

		pthread_key_delete(m_ForkThreadLocal);
	}

	static void fs_ThreadDestructionHook(void* _ThreadID)
	{

		fg_GetSys()->f_ThreadLocalFreeThread();
	}

	void f_RegisterDestructionHookForThread()
	{

	}

	NMib::NStorage::TCAggregate<NMib::NOSXRuntime::CFileChangeNoticationContext, 64> m_FileChangeNoticationContext;


};

static inline_small CSystemMacOSX *fg_GetLocalSys()
{
	return (CSystemMacOSX *)fg_GetSys();
}

CSystem_POSIX *fg_GetSys_POSIX()
{
	return &fg_GetLocalSys()->m_Posix;
}

uint32 fg_CodepageToCFStringEncoding(uint32 _Codepage)
{
  switch (_Codepage)
  {
    case 1252:
      return kCFStringEncodingWindowsLatin1;

    case 10000:
    	return kCFStringEncodingMacRoman;
  }

  return kCFStringEncodingInvalidId;
}

void NMib::NSys::NStr::fg_SystemEncodeAnsiStr(NMib::NStr::CStr const &_In, NMib::NStr::CAnsiStr &_Out, ch8 _ErrorChar)
{
	fg_SystemEncodeCodePageStr(_In, _Out, 1252, _ErrorChar); // 1252 == Windows Latin 1
}

void NMib::NSys::NStr::fg_SystemEncodeAnsiStr(NMib::NStr::CStrNonTracked const &_In, NMib::NStr::CAnsiStrNonTracked &_Out, ch8 _ErrorChar)
{
	fg_SystemEncodeCodePageStr(_In, _Out, 1252, _ErrorChar); // 1252 == Windows Latin 1
}


void NMib::NSys::NStr::fg_SystemDecodeAnsiStr(NMib::NStr::CAnsiStr const &_In, NMib::NStr::CStr &_Out)
{
	fg_SystemDecodeCodePageStr(_In, _Out, 1252);  // 1252 == Windows Latin 1
}

void NMib::NSys::NStr::fg_SystemDecodeAnsiStr(ch8 const *_pIn, NMib::NStr::CStr &_Out)
{
	fg_SystemDecodeCodePageStr(_pIn, _Out, 1252);  // 1252 == Windows Latin 1
}

void NMib::NSys::NStr::fg_SystemDecodeAnsiStr(NMib::NStr::CAnsiStrNonTracked const &_In, NMib::NStr::CStrNonTracked &_Out)
{
	fg_SystemDecodeCodePageStr(_In, _Out, 1252);  // 1252 == Windows Latin 1
}

void NMib::NSys::NStr::fg_SystemDecodeAnsiStr(ch8 const *_pIn, NMib::NStr::CStrNonTracked &_Out)
{
	fg_SystemDecodeCodePageStr(_pIn, _Out, 1252);  // 1252 == Windows Latin 1
}

void NMib::NSys::NStr::fg_SystemEncodeCodePageStr(NMib::NStr::CStr const &_In, NMib::NStr::CAnsiStr &_Out, uint32 _CodePage, ch8 _ErrorChar)
{
	uint32 CodePage = fg_CodepageToCFStringEncoding(_CodePage);
	if (CodePage == kCFStringEncodingInvalidId)
		DMibError(NMib::NStr::CStrNonTracked::CFormat("Codepage {} not supported") << _CodePage);

	CFStringRef pStringRef = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)_In.f_GetStr(), _In.f_GetLen(), kCFStringEncodingUTF8, false);

	if (!pStringRef)
		DMibError(NMib::NPlatform::fg_FormatErrno("CFStringCreateWithBytes (encode code page str)", errno));

	auto Cleanup0 = g_OnScopeExit > [&]
		{
			CFRelease(pStringRef);
		}
	;

	CFDataRef pData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, pStringRef, CodePage, _ErrorChar);
	if (!pData)
		DMibError(NMib::NPlatform::fg_FormatErrno("CFStringCreateExternalRepresentation (encode code page str)", errno));

	auto Cleanup1 = g_OnScopeExit > [&]
		{
			CFRelease(pData);
		}
	;

	_Out.f_SetStr((ch8 const *)CFDataGetBytePtr(pData), CFDataGetLength(pData));
}

void NMib::NSys::NStr::fg_SystemEncodeCodePageStr(NMib::NStr::CStrNonTracked const &_In, NMib::NStr::CAnsiStrNonTracked &_Out, uint32 _CodePage, ch8 _ErrorChar)
{
	uint32 CodePage = fg_CodepageToCFStringEncoding(_CodePage);
	if (CodePage == kCFStringEncodingInvalidId)
		DMibError(NMib::NStr::CStrNonTracked::CFormat("Codepage {} not supported") << _CodePage);

	CFStringRef pStringRef = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)_In.f_GetStr(), _In.f_GetLen(), kCFStringEncodingUTF8, false);

	if (!pStringRef)
		DMibError(NMib::NPlatform::fg_FormatErrno("CFStringCreateWithBytes (encode code page str)", errno));

	auto Cleanup0 = g_OnScopeExit > [&]
		{
			CFRelease(pStringRef);
		}
	;

	CFDataRef pData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, pStringRef, CodePage, _ErrorChar);
	if (!pData)
		DMibError(NMib::NPlatform::fg_FormatErrno("CFStringCreateExternalRepresentation (encode code page str)", errno));

	auto Cleanup1 = g_OnScopeExit > [&]
		{
			CFRelease(pData);
		}
	;

	_Out.f_SetStr((ch8 const *)CFDataGetBytePtr(pData), CFDataGetLength(pData));
}

void NMib::NSys::NStr::fg_SystemDecodeCodePageStr(NMib::NStr::CAnsiStr const &_In, NMib::NStr::CStr &_Out, uint32 _CodePage)
{
	uint32 CodePage = fg_CodepageToCFStringEncoding(_CodePage);
	if (CodePage == kCFStringEncodingInvalidId)
		DMibError(NMib::NStr::CStrNonTracked::CFormat("Codepage {} not supported") << _CodePage);

	CFStringRef pStringRef = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)_In.f_GetStr(), _In.f_GetLen(), CodePage, false);

	if (!pStringRef)
		DMibError(NMib::NPlatform::fg_FormatErrno("CFStringCreateWithBytes (decode code page str)", errno));

	auto Cleanup0 = g_OnScopeExit > [&]
		{
			CFRelease(pStringRef);
		}
	;

	CFIndex nUniChars = CFStringGetLength(pStringRef);
	const UniChar * Output = CFStringGetCharactersPtr(pStringRef);

	if (Output)
	{
		NMib::NStr::CWStr UniString;
		NMib::NStr::CWStr::CChar* pDest = UniString.f_GetStr(nUniChars);

		DMibSafeCheck(pDest, "Out of memory?");

		fg_MemCopy(pDest, Output, nUniChars * sizeof(UniChar));
		pDest[nUniChars] = 0;

		_Out = UniString;
	}
	else
	{
		NMib::NStr::CWStr UniString;
		auto *pChars = UniString.f_GetStr(nUniChars);

		*pChars = NMib::NStr::CWStr::CChar(0); // We have no idea if the next call with succeed so prepare for it doing nothing.
		CFStringGetCharacters(pStringRef, CFRangeMake(0, nUniChars), (UniChar*)pChars);
		pChars[nUniChars] = NMib::NStr::CWStr::CChar(0);

		_Out = UniString;
	}
}


void NMib::NSys::NStr::fg_SystemDecodeCodePageStr(ch8 const *_pIn, NMib::NStr::CStr &_Out, uint32 _CodePage)
{
	uint32 CodePage = fg_CodepageToCFStringEncoding(_CodePage);
	if (CodePage == kCFStringEncodingInvalidId)
		DMibError(NMib::NStr::CStrNonTracked::CFormat("Codepage {} not supported") << _CodePage);

	CFStringRef pStringRef = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)_pIn, fg_StrLen(_pIn), CodePage, false);

	if (!pStringRef)
		DMibError(NMib::NPlatform::fg_FormatErrno("CFStringCreateWithBytes (decode code page str)", errno));

	auto Cleanup0 = g_OnScopeExit > [&]
		{
			CFRelease(pStringRef);
		}
	;

	CFIndex nUniChars = CFStringGetLength(pStringRef);
	const UniChar * Output = CFStringGetCharactersPtr(pStringRef);

	if (Output)
	{
		NMib::NStr::CWStr UniString;
		NMib::NStr::CWStr::CChar* pDest = UniString.f_GetStr(nUniChars);

		DMibSafeCheck(pDest, "Out of memory?");

		fg_MemCopy(pDest, Output, nUniChars * sizeof(UniChar));
		pDest[nUniChars] = 0;

		_Out = UniString;
	}
	else
	{
		NMib::NStr::CWStr UniString;
		auto *pChars = UniString.f_GetStr(nUniChars);

		*pChars = NMib::NStr::CWStr::CChar(0); // We have no idea if the next call with succeed so prepare for it doing nothing.
		CFStringGetCharacters(pStringRef, CFRangeMake(0, nUniChars), (UniChar*)pChars);
		pChars[nUniChars] = NMib::NStr::CWStr::CChar(0);

		_Out = UniString;
	}
}


void NMib::NSys::NStr::fg_SystemDecodeCodePageStr(NMib::NStr::CAnsiStrNonTracked const &_In, NMib::NStr::CStrNonTracked &_Out, uint32 _CodePage)
{
	uint32 CodePage = fg_CodepageToCFStringEncoding(_CodePage);
	if (CodePage == kCFStringEncodingInvalidId)
		DMibError(NMib::NStr::CStrNonTracked::CFormat("Codepage {} not supported") << _CodePage);

	CFStringRef pStringRef = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)_In.f_GetStr(), _In.f_GetLen(), CodePage, false);

	if (!pStringRef)
		DMibError(NMib::NPlatform::fg_FormatErrno("CFStringCreateWithBytes (decode code page str)", errno));

	auto Cleanup0 = g_OnScopeExit > [&]
		{
			CFRelease(pStringRef);
		}
	;

	CFIndex nUniChars = CFStringGetLength(pStringRef);
	const UniChar * Output = CFStringGetCharactersPtr(pStringRef);

	if (Output)
	{
		NMib::NStr::CWStrNonTracked UniString;
		NMib::NStr::CWStrNonTracked::CChar* pDest = UniString.f_GetStr(nUniChars);

		DMibSafeCheck(pDest, "Out of memory?");

		fg_MemCopy(pDest, Output, nUniChars * sizeof(UniChar));
		pDest[nUniChars] = 0;

		_Out = UniString;
	}
	else
	{
		NMib::NStr::CWStrNonTracked UniString;
		auto *pChars = UniString.f_GetStr(nUniChars);

		*pChars = NMib::NStr::CWStrNonTracked::CChar(0); // We have no idea if the next call with succeed so prepare for it doing nothing.
		CFStringGetCharacters(pStringRef, CFRangeMake(0, nUniChars), (UniChar*)pChars);
		pChars[nUniChars] = NMib::NStr::CWStrNonTracked::CChar(0);

		_Out = UniString;
	}
}

void NMib::NSys::NStr::fg_SystemDecodeCodePageStr(ch8 const *_pIn, NMib::NStr::CStrNonTracked &_Out, uint32 _CodePage)
{
	uint32 CodePage = fg_CodepageToCFStringEncoding(_CodePage);
	if (CodePage == kCFStringEncodingInvalidId)
		DMibError(NMib::NStr::CStrNonTracked::CFormat("Codepage {} not supported") << _CodePage);

	CFStringRef pStringRef = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)_pIn, fg_StrLen(_pIn), CodePage, false);

	if (!pStringRef)
		DMibError(NMib::NPlatform::fg_FormatErrno("CFStringCreateWithBytes (decode code page str)", errno));

	auto Cleanup0 = g_OnScopeExit > [&]
		{
			CFRelease(pStringRef);
		}
	;

	CFIndex nUniChars = CFStringGetLength(pStringRef);
	const UniChar * Output = CFStringGetCharactersPtr(pStringRef);

	if (Output)
	{
		NMib::NStr::CWStrNonTracked UniString;
		NMib::NStr::CWStrNonTracked::CChar* pDest = UniString.f_GetStr(nUniChars);

		DMibSafeCheck(pDest, "Out of memory?");

		fg_MemCopy(pDest, Output, nUniChars * sizeof(UniChar));
		pDest[nUniChars] = 0;

		_Out = UniString;
	}
	else
	{
		NMib::NStr::CWStrNonTracked UniString;
		auto *pChars = UniString.f_GetStr(nUniChars);

		*pChars = NMib::NStr::CWStrNonTracked::CChar(0); // We have no idea if the next call with succeed so prepare for it doing nothing.
		CFStringGetCharacters(pStringRef, CFRangeMake(0, nUniChars), (UniChar*)pChars);
		pChars[nUniChars] = NMib::NStr::CWStrNonTracked::CChar(0);

		_Out = UniString;
	}
}



NContainer::TCMap<NMib::NStr::CStr, NMib::NStr::CStr> NMib::NSys::fg_Process_GetEnvironmentVariables_NonProtected()
{
	NContainer::TCMap<NMib::NStr::CStr, NMib::NStr::CStr> Vars;

	CStr Key;

	for (char** pEnvironment = (*_NSGetEnviron())
		;*pEnvironment
		;++pEnvironment)
	{
		CStr VarStr(*pEnvironment);
		Key = NMib::NStr::fg_GetStrSep(VarStr, "=");

		Vars[Key] = VarStr;
	}

	return fg_Move(Vars);
}

inline_never bool NSys::fg_Compiler_AlwaysFalse()
{
	return false;
}

inline_never assure_used bool NSys::fg_Compiler_MakeActive(const void *_Reference)
{
	(void)_Reference;
	return true;
}


void NSys::fg_Thread_Suspend(void *_pThread)
{
	mach_port_t MachThread = pthread_mach_thread_np((pthread_t)_pThread);

	kern_return_t Result = thread_suspend(MachThread);

	if (Result != KERN_SUCCESS)
		DMibError("Failed to suspend thread.");
}

void NSys::fg_Thread_Resume(void *_pThread)
{
	mach_port_t MachThread = pthread_mach_thread_np((pthread_t)_pThread);

	kern_return_t Result = thread_resume(MachThread);

	if (Result != KERN_SUCCESS)
		DMibError("Failed to resume thread.");
}

//NStorage::TCAggregate<NThread::TCThreadLocal<zint32, NOSXDebug::CAllocator_NonTrackedHeap, NThread::EThreadLocalFlag_None>> g_DisableHeapOverride;

void NSys::fg_Thread_SetNumaAffinity(void *_pThread, ENumaNode _NumaNode)
{
	// TODO: Implement
}

mint NSys::fg_Mem_GetNumNumaNodes()
{
  // TODO: Implement
  return 0;
}

void NSys::fg_Mem_GetNumaNodes(ENumaNode *_pNodes, mint _nNodes)
{
  // TODO: Implement
}

void *NSys::fg_InterProcess_MemAlloc(ch8 const *_pName, mint _Size, void * &_pMemory)
{
	// TODO: Implement
	DMibError("Not implemented");
	return nullptr;
}

void NSys::fg_InterProcess_MemFree(void *_pHandle, void *_pMemory)
{
	// TODO: Implement
}

void NSys::fg_Thread_SmallestSleep()
{
	fg_Thread_Sleep(0.001);
}


namespace
{
	bool fg_ArgIsInternal(ch8 const* _pArg)
	{
		if (fg_StrCmp(_pArg, "--OutputPID") == 0)
			return true;
		if (fg_StrCmp(_pArg, "--NoStdErr") == 0)
			return true;
		return false;
	}
}

NMib::NStr::CStr NSys::fg_Process_GetCommandLine()
{
	NMib::NStr::CStr Return;

	int NumArgs = *_NSGetArgc();
	ch8** pArgs = *_NSGetArgv();
	if (pArgs)
	{
		for (int i = 0;i < NumArgs; ++i)
		{
            if (pArgs[i] && !fg_ArgIsInternal(pArgs[i]))
                Return += CStr("\"") + CStr(pArgs[i]) + CStr("\"");
		}
	}

	return Return;
}

void NSys::fg_Process_GetCommandLineArgs(NContainer::TCVector<NMib::NStr::CStr> &_List)
{
	_List.f_SetLen(0);

	int NumArgs = *_NSGetArgc();
	ch8** pArgs = *_NSGetArgv();
	if (pArgs)
	{
		for (int i = 0;i < NumArgs; ++i)
		{
            if (pArgs[i] && !fg_ArgIsInternal(pArgs[i]))
            {
                _List.f_Insert(CStr(pArgs[i]));
            }
		}
	}
}


NMib::NStr::CStr NSys::fg_CommandLineParameters()
{
	NMib::NStr::CStr Return;

	int NumArgs = *_NSGetArgc();
	ch8** pArgs = *_NSGetArgv();
	if (pArgs)
	{
		for (int i = 1;i < NumArgs; ++i)
		{
            if (pArgs[i] && !fg_ArgIsInternal(pArgs[i]))
            {
                if (i > 1)
                    Return += CStr(" \"") + CStr(pArgs[i]) + CStr("\"");
                else
                    Return += CStr("\"") + CStr(pArgs[i]) + CStr("\"");
            }
		}
	}
	return Return;
}

void NSys::fg_Message(const ch8 *_pMessageType, const ch8 *_pToOutput)
{

	bool bCopy = !fg_StrCmpNoCase(_pMessageType, "Copy");

	CStrVMem Message = _pToOutput;
	CStrVMem Message2;

#if 0
	if (bCopy)
	{
		Message2 = Message.f_Replace("\n", DMibNewLine);
		Message = "Test\n\n" + Message + "\n\nIf you want to put the message in clipboard press OK.";

		ClearCurrentScrap();
		ScrapRef Current;
		OSStatus ret = GetCurrentScrap(&Current);
		if ( ret == noErr )
		{
			PutScrapFlavor(Current, kScrapFlavorTypeText, 0, Message2.f_GetLen(), Message2.f_GetStr());
		}
		else
		{
			DMibTrace("Failed to copy text to the scrap\n", 0);
		}

	}
#endif

/*	if (!fg_StrCmpNoCase(_pMessageType, "Fatal Error"))
	{
	}*/


//	DWORD Answer = MessageBoxA(hWndParent, Message, _pMessageType, uType);

	DMibTrace("{}", Message);
	if (bCopy)
	{
	}
}

void NSys::fg_Message(const ch16 *_pMessageType, const ch16 *_pToOutput)
{
}

namespace NMib
{
	mint align_cacheline g_SystemMemory[sizeof(CSystemMacOSX) / sizeof(mint)];
	mint g_bCreatingSystemDone = false;
	mint g_bCanUseSystemMalloc = false;
	constinit NAtomic::TCAtomicAggregate<mint> g_bCanStartThreads = {DAggregateInit};
	mint g_bCreatedSystem = false;
}

#include <mach-o/dyld.h>

void fg_ForkPrepare()
{
	CSystemMacOSX::fs_ForkPrepare();
}

void fg_ForkParentOrChild()
{
	CSystemMacOSX::fs_ForkParentOrChild();
}

namespace NMib
{

	namespace NSys
	{
		// This allows
		void __attribute__((weak)) fg_InitBreakpad()
		{
		}
		void __attribute__((weak)) fg_DestroyBreakpad()
		{
		}
	}

} // Namespace NMib


namespace NMib
{
	namespace NSys
	{
		int g_OperatingSystemMajor = -1;
		int g_OperatingSystemMinor = -1;
		int g_OperatingSystemFix = -1;
		EOperatingSystemArch g_OperatingSystemArch = EOperatingSystemArch_Unknown;

		bool fg_System_GetOperatingSystemVersion(int& _oMajor, int& _oMinor, int& _oFix, EOperatingSystemArch& _Arch)
		{
			if (g_OperatingSystemMajor >= 0)
			{
				_oMajor = g_OperatingSystemMajor;
				_oMinor = g_OperatingSystemMinor;
				_oFix = g_OperatingSystemFix;
				_Arch = g_OperatingSystemArch;
				return g_OperatingSystemMajor != 0;
			}

			CFStr256 VersionString;

			// Arch
			{
				struct utsname un;
				int Res = uname(&un);
				if (Res >= 0)
				{
					VersionString = un.release;
					int b64bitCPU = false;

					size_t DataSize = sizeof(b64bitCPU);
					sysctlbyname("hw.cpu64bit_capable", &b64bitCPU, &DataSize, nullptr, 0);

					if (fg_StrCmpNoCase(un.machine, "i386") == 0 || fg_StrCmpNoCase(un.machine, "x86_64") == 0)
					{
						if (b64bitCPU)
							g_OperatingSystemArch = EOperatingSystemArch_x64;
						else
							g_OperatingSystemArch = EOperatingSystemArch_x86;
					}
					else if (fg_StrCmpNoCase(un.machine, "powerpc") == 0 || fg_StrCmpNoCase(un.machine, "ppc64") == 0)
					{
						if (b64bitCPU)
							g_OperatingSystemArch = EOperatingSystemArch_PPC64;
						else
							g_OperatingSystemArch = EOperatingSystemArch_PPC;
					}
					else
						g_OperatingSystemArch = EOperatingSystemArch_Unknown;
				}
				else
				{
					g_OperatingSystemArch = EOperatingSystemArch_Unknown;
					return false;
				}

				_Arch = g_OperatingSystemArch;
			}

			int Major = 0;
			int Minor = 0;
			int Fix = 0;

			Major = fg_GetStrSep(VersionString, ".").f_ToInt(0);
			Minor = fg_GetStrSep(VersionString, ".").f_ToInt(0);
			Fix = fg_GetStrSep(VersionString, ".").f_ToInt(0);

			// Try to convert from darwin version to OSX version
			g_OperatingSystemMajor = 10;
			g_OperatingSystemMinor = Major - 4;
			g_OperatingSystemFix = Minor;

			_oMajor = g_OperatingSystemMajor;
			_oMinor = g_OperatingSystemMinor;
			_oFix = g_OperatingSystemFix;

			return g_OperatingSystemMajor != 0;
    	}
	}
}

bool g_bCreatedSystemMalloc = false;

void fg_MalterlibMallocOverrideInit();
void fg_MalterlibMallocOverrideInit_ReinstallHandler();
void fg_MalterlibMallocOverride_AtExitCalled();

void NSys::fg_Process_AllowInvalidExit(bool _bAllow)
{
}

namespace NMib
{
	namespace NSys
	{
		void fg_CreateSystemMalloc(bool _bProvideDestroySystem);
		void fg_CreateSystemVersion();
		extern mint g_ThreadLocalOffsetPThread;
	}

}

void fg_DestroySystemAtExit()
{
	NSys::fg_DestroySystem();
}

void fg_DestroySystemThreadsAtExit()
{
	auto pSys = fg_GetLocalSys();
	pSys->f_DestroyThreadSpecific();
	fg_MalterlibMallocOverride_AtExitCalled();
}

//#if defined(DConfig_Release) && !defined(DConfig)

constinit NMib::NThread::CLowLevelLockAggregate g_CrashReporterLock = {DAggregateInit};
NMib::NStr::CStrNonTracked g_CrashReporterString;

extern "C"
{
	char *__crashreporter_info__ = nullptr;
	asm(".desc ___crashreporter_info__, 0x10");
}

void NSys::fg_System_ReportContractViolation(const NMib::NStr::CStrNonTracked &_Message)
{
	DMibLock(g_CrashReporterLock);
	if (__crashreporter_info__)
	{
		if (g_CrashReporterString.f_GetStr() != (ch8 const *)__crashreporter_info__)
			g_CrashReporterString = __crashreporter_info__;
		g_CrashReporterString += "\n\n";
		g_CrashReporterString += _Message;
	}
	else
		g_CrashReporterString = _Message;
	__crashreporter_info__ = (char *)g_CrashReporterString.f_GetStr();
}

void fg_ReportCurrentException()
{
	if (!__cxxabiv1::__cxa_current_primary_exception())
		return;

	try
	{
		throw;
	}
	catch (NException::CExceptionBase const& _Exception)
	{
		CStrNonTracked ExceptionInfo;
		ExceptionInfo += CStrNonTracked::CFormat(DMibPFileLineFormat " Uncaught exception of type: {}\n") << _Exception.f_GetFile() << _Exception.f_GetLine() << _Exception.f_GetClass();
		ExceptionInfo += CStrNonTracked::CFormat("\t{}\n") << _Exception.f_GetErrorStrNonTracked();
		NMib::NSys::fg_System_ReportContractViolation(ExceptionInfo);
		NSys::fg_ConsoleErrorOutput(ExceptionInfo);
	}
	catch (std::exception const& _Exception)
	{
		CStrNonTracked ExceptionInfo;
		ExceptionInfo += "Uncaught exception of type inherited from: std::exception\n";
		ExceptionInfo += CStrNonTracked::CFormat("	Error: {}\n") << _Exception.what();
		NMib::NSys::fg_System_ReportContractViolation(ExceptionInfo);
		NSys::fg_ConsoleErrorOutput(ExceptionInfo);
	}
	catch (...)
	{
		CStrNonTracked ExceptionInfo;
		ExceptionInfo += "Uncaught exception of type: Unknown\n";
		NMib::NSys::fg_System_ReportContractViolation(ExceptionInfo);
		NSys::fg_ConsoleErrorOutput(ExceptionInfo);
	}
}

static std::unexpected_handler g_DefaultUnexpectedHandler = nullptr;
void fg_UnexpectedExceptionHandler()
{
	fg_ReportCurrentException();
	if (g_DefaultUnexpectedHandler)
		g_DefaultUnexpectedHandler();
	else
		abort();
}

static std::terminate_handler g_DefaultTerminateHandler = nullptr;

void fg_TerminateHandler()
{
	fg_ReportCurrentException();
	if (g_DefaultTerminateHandler)
		g_DefaultTerminateHandler();
	else
		abort();
}

void NSys::fg_CreateSystemVersion()
{
	if (CSystem::ms_PlatformVersion != 0)
		return;

	if (g_OperatingSystemMajor < 0)
	{
		int Dummy;
		EOperatingSystemArch Arch;

		if (!fg_System_GetOperatingSystemVersion(Dummy, Dummy, Dummy, Arch))
			DMibPDebugBreak; // Not supported
	}

#if defined(__i386__)
	g_ThreadLocalOffsetPThread = 0x48;
#elif defined(__x86_64__)
	g_ThreadLocalOffsetPThread = 0x60;
#elif defined(__arm64__)
	g_ThreadLocalOffsetPThread = 0x60;
#else
	#error "Not Implemented"
#endif

	if (g_OperatingSystemMajor == 10 && g_OperatingSystemMinor >= 9)
	{
		g_ThreadLocalOffsetPThread += 16 * sizeof(void *);
	}
	else if (g_OperatingSystemMajor == 10 && g_OperatingSystemMinor >= 7)
	{
	}
	else if (g_OperatingSystemMajor == 10 && (g_OperatingSystemMinor == 5 || g_OperatingSystemMinor == 6))
	{
#if defined(__i386__)
		g_ThreadLocalOffset = 0x48;
		g_ThreadSelfOffset = 0x48;

#elif defined(__x86_64__)
		g_ThreadLocalOffset = 0x60;
		g_ThreadSelfOffset = 0x60;
#elif defined(__arm64__)
		g_ThreadLocalOffset = 0x60;
		g_ThreadSelfOffset = 0x60;
#else
#error "Not Implemented"
#endif
	}
	else
		DMibPDebugBreak; // Not supported

	CSystem::ms_PlatformVersion = g_OperatingSystemMajor * 10000 + g_OperatingSystemMinor * 100 + g_OperatingSystemFix;
}

namespace NMib::NSys::NPrivate
{
	constinit mint g_PageSize = 0;
}

void NSys::fg_CreateSystemMalloc(bool _bProvideDestroySystem)
{
	if (g_bCreatedSystemMalloc)
		return;

	NPrivate::g_PageSize = sysconf(_SC_PAGE_SIZE);

	fg_CreateSystemVersion();

	fg_MalterlibMallocOverrideInit();

	g_bCreatedSystemMalloc = true;

	g_VirtualMap.f_Construct();

	host_page_size(mach_host_self(), (vm_size_t *)&NMib::NSys::NPrivate::g_PageSize);

	g_bCreatingSystemDone = true;

	auto pSystemMemory = (void *)NMib::g_SystemMemory;
	auto pSystem = new(pSystemMemory) CSystemMacOSX();
	static_assert(NTraits::TCAlignmentOf<CSystemMacOSX>::mc_Value <= mint(DMibPMemoryCacheLineSize), "Aligment error");

	NSys::fg_Compiler_MakeActive(&pSystemMemory);
	NSys::fg_Compiler_MakeActive(&pSystem);
	DMibFastCheck((void *)pSystem == pSystemMemory);

	g_bCanUseSystemMalloc = true;

	if (!_bProvideDestroySystem)
		atexit(&fg_DestroySystemAtExit);
	else
		atexit(&fg_DestroySystemThreadsAtExit);

	pSystem->f_InitThreadLocal();

	g_DefaultTerminateHandler = std::set_terminate(&fg_TerminateHandler);
	g_DefaultUnexpectedHandler = std::set_unexpected(&fg_UnexpectedExceptionHandler);
}

extern "C" void fg_Malterlib_CreateSystem()
{
	NSys::fg_CreateSystem();
}
void NSys::fg_CreateSystem()
{
	if (g_bCreatedSystem)
		return;

	g_bCreatedSystem = true;

	fg_CreateSystemMalloc(false);

	NPrivate::fg_SetupLimits();

#if !defined(DMibMemoryOverrideDll)
	if (!g_bIsSharedLibrary) // Only use pthread_atfork in non-dylibs as atfork handlers cannot be unregistered before dlclose
#endif
	{
#ifndef F_SETNOSIGPIPE
		signal(SIGPIPE,SIG_IGN);
#endif
		if (!g_bRegisteredAtFork)
		{
			g_bRegisteredAtFork = true;
			pthread_atfork(&CSystemMacOSX::fs_ForkPrepare, &CSystemMacOSX::fs_ForkParent, &CSystemMacOSX::fs_ForkChild);
		}
	}

	//atexit(&fg_DestroySystemAtExit);

	// fg_MalterlibMallocOverrideInit_ReinstallHandler(); Breakpad does not use signal handlers on OSX, so we don't need to install handlers here

	{
		int NumArgs = *_NSGetArgc();
		ch8** pArgs = *_NSGetArgv();
		if (pArgs)
		{
			for (int i = 0;i < NumArgs; ++i)
			{
				if (pArgs[i])
				{
					if (fg_StrCmp(pArgs[i], "--OutputPID") == 0)
						NSys::fg_ConsoleOutput((CFStr256::CFormat("{nfh,sj16,sf0}") << (mint)getpid()).f_GetStr());
					else if (fg_StrCmp(pArgs[i], "--NoStdErr") == 0)
					{
						int DevNull = open("/dev/null", O_WRONLY);
						if (DevNull >= 0)
						{
							if (DevNull != 2)
								dup2(DevNull, 2);
							close(DevNull);
						}
					}
				}
			}
		}
	}

	auto pSystem = fg_GetLocalSys();

	pSystem->f_Init();
	pSystem->f_InitModule();
	fg_InitBreakpad();
	pSystem->f_InitModuleThreaded();

	setlinebuf(stdout); // Default to line buffered output
	setlinebuf(stderr); // Default to line buffered output
}

bool g_bSysDeleted = false;

void NSys::fg_PreDestroyHeap()
{
}

void NSys::fg_DestroySystem()
{
	if (g_bCreatedSystem && !g_bSysDeleted)
	{
		g_bSysDeleted = true;

		auto pSys = fg_GetLocalSys();
		pSys->f_DestroyThreadSpecific();

		pSys->f_ExitModule();

		// We need to flush these before the buffer memory is deleted
		fflush(stdout);
		fflush(stderr);

		fg_DestroyBreakpad();

		pSys->f_Destruct();
		if (pSys->m_bForkedChild)
			return; // Forked children have several problems with invalid semaphores etc, so lets just not destroy anything here
		pSys->~CSystemMacOSX();

		g_VirtualMap.f_Destruct();
		g_VirtualMapLock.f_Destruct();

		if (!g_bMemoryManagerNeededAfterDestroy)
		{
			g_EventEmulationPool.f_Destruct();
			g_ImpSemaphorePool.f_Destruct();
		}
	}
}

namespace NMib
{
	namespace NSys
	{
		void fg_MalterlibSystem_ForkPrepare()
		{
			CSystemMacOSX::fs_ForkPrepare();
		}
		void fg_MalterlibSystem_ForkParent()
		{
			CSystemMacOSX::fs_ForkParent();
		}
		void fg_MalterlibSystem_ForkChild()
		{
			CSystemMacOSX::fs_ForkChild();
		}
	}
}

#if !defined(DMibDynamicLibrary) || defined(DMibMemoryOverrideDll)
	namespace NMib
	{
		namespace NSys
		{
			void * g_pCrossModuleMemoryManagerInterface = nullptr;
		}
	}

	// Note: This needs to be named exactly like this to be compatible with old versions of library (when Malterlib was named Ids)
	extern "C" assure_used module_export void *fg_IdsGetCrossModuleMemoryManagerInterface()
	{
		return NMib::NSys::g_pCrossModuleMemoryManagerInterface;
	}
#endif

void *NSys::fg_Process_GetCrossModuleMemoryManagerInterface()
{
#if defined(DMibDynamicLibrary) && !defined(DMibMemoryOverrideDll)
	void * ( *fMalterlibGetCrossModuleMemoryManagerInterface)();
	(void * &)fMalterlibGetCrossModuleMemoryManagerInterface = dlsym(RTLD_DEFAULT, "fg_IdsGetCrossModuleMemoryManagerInterface");
	if (fMalterlibGetCrossModuleMemoryManagerInterface )
	{
		auto pRet = fMalterlibGetCrossModuleMemoryManagerInterface ();
		return pRet;
	}
	else
		return nullptr;
#else
	return NMib::NSys::g_pCrossModuleMemoryManagerInterface;
#endif
}

void NSys::fg_Process_SetCrossModuleMemoryManagerInterface(void *_pInterface)
{
#if defined(DMibDynamicLibrary) && !defined(DMibMemoryOverrideDll)
	DMibNeverGetHere;
#else
	g_pCrossModuleMemoryManagerInterface = _pInterface;

	DMibFastCheck(fg_IdsGetCrossModuleMemoryManagerInterface() == _pInterface);
#endif
}

bool NSys::fg_System_BeingDebugged()
{
	int                 junk;
	int                 mib[4];
	struct kinfo_proc   info;
	size_t              size;

	// Initialize the flags so that, if sysctl fails for some bizarre
	// reason, we get a predictable result.

	info.kp_proc.p_flag = 0;

	// Initialize mib, which tells sysctl the info we want, in this case
	// we're looking for information about a specific process ID.

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PID;
	mib[3] = getpid();

	// Call sysctl.

	size = sizeof(info);
	junk = sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);
	assert(junk == 0);

	// We're being debugged if the P_TRACED flag is set.

	return ( (info.kp_proc.p_flag & P_TRACED) != 0 );
}

////file handling

namespace NMib
{

	namespace NSys
	{

		NMib::NStr::CStr fg_MacOSX_GetApplicationSupportDirectory();
		NMib::NStr::CStr fg_MacOSX_GetCachesDirectory();
        NMib::NStr::CStr fg_MacOSX_GetUserHomeDirectory();
		NMib::NStr::CStr fg_MacOSX_GetLogDirectory();

		NMib::NStr::CStrNonTracked fg_MacOSX_GetApplicationSupportDirectoryNonTracked();
		NMib::NStr::CStrNonTracked fg_MacOSX_GetCachesDirectoryNonTracked();
        NMib::NStr::CStrNonTracked fg_MacOSX_GetUserHomeDirectoryNonTracked();
		NMib::NStr::CStrNonTracked fg_MacOSX_GetLogDirectoryNonTracked();

		NMib::NStr::CStr fg_MacOSX_GetSystemLanguage();

	} // Namespace NSys

} // Namespace NMib

namespace
{
	CStr fg_GetProgramUserName()
	{
		NMib::NStr::CStr FileName = NMib::NFile::CFile::fs_GetFileNoExt(NSys::NFile::fg_GetProgramPath());
		aint iFind = FileName.f_FindReverse("_x");
		if (iFind >= 0)
			FileName = FileName.f_Left(iFind);

		return FileName;
	}

	CStrNonTracked fg_GetProgramUserNameNonTracked()
	{
		NMib::NStr::CStrNonTracked FileName = NMib::NFile::CFile::fs_GetFileNoExt(NSys::NFile::fg_GetProgramPathNonTracked());
		aint iFind = FileName.f_FindReverse("_x");
		if (iFind >= 0)
			FileName = FileName.f_Left(iFind);

		return FileName;
	}
}

NMib::NStr::CStr NSys::NFile::fg_GetUserLocalProgramDirectory()
{
	return NMib::NFile::CFile::fs_AppendPath(NMib::NSys::fg_MacOSX_GetApplicationSupportDirectory(), fg_GetProgramUserName());
}

NMib::NStr::CStr NSys::NFile::fg_GetUserLocalProgramCacheDirectory()
{
	return NMib::NFile::CFile::fs_AppendPath(NMib::NSys::fg_MacOSX_GetCachesDirectory(), fg_GetProgramUserName());
}

NMib::NStr::CStrNonTracked NSys::NFile::fg_GetUserLocalProgramDirectoryNonTracked()
{
	return NMib::NFile::CFile::fs_AppendPath(NMib::NSys::fg_MacOSX_GetApplicationSupportDirectoryNonTracked(), fg_GetProgramUserNameNonTracked());
}

NMib::NStr::CStrNonTracked NSys::NFile::fg_GetUserLocalProgramCacheDirectoryNonTracked()
{
	return NMib::NFile::CFile::fs_AppendPath(NMib::NSys::fg_MacOSX_GetCachesDirectoryNonTracked(), fg_GetProgramUserNameNonTracked());
}

NStr::CStr NSys::NFile::fg_GetUserProgramDirectory()
{
	return NSys::NFile::fg_GetUserLocalProgramDirectory();
}

NStr::CStrNonTracked NSys::NFile::fg_GetUserProgramDirectoryNonTracked()
{
	return NSys::NFile::fg_GetUserLocalProgramDirectoryNonTracked();
}

NStr::CStr NSys::NFile::fg_GetTemporaryDirectory()
{
	NMib::NStr::CStr TmpDir = fg_GetSys()->f_GetEnvironmentVariable("TMPDIR");
	if (TmpDir.f_IsEmpty())
		TmpDir = fg_GetSys()->f_GetEnvironmentVariable("TMP");
	if (TmpDir.f_IsEmpty())
		TmpDir = fg_GetSys()->f_GetEnvironmentVariable("TEMP");
	if (TmpDir.f_IsEmpty())
		TmpDir = fg_GetSys()->f_GetEnvironmentVariable("TEMPDIR");
	if (!TmpDir.f_IsEmpty())
		return NMib::NFile::CFile::fs_AppendPath(TmpDir, fg_GetProgramUserName());
	return "/tmp";
}

NStr::CStrNonTracked NSys::NFile::fg_GetTemporaryDirectoryNonTracked()
{
	NMib::NStr::CStrNonTracked TmpDir = fg_Process_GetEnvironmentVariable_NonProtected(CStrNonTracked("TMPDIR"));
	if (TmpDir.f_IsEmpty())
		TmpDir = fg_Process_GetEnvironmentVariable_NonProtected(CStrNonTracked("TMP"));
	if (TmpDir.f_IsEmpty())
		TmpDir = fg_Process_GetEnvironmentVariable_NonProtected(CStrNonTracked("TEMP"));
	if (TmpDir.f_IsEmpty())
		TmpDir = fg_Process_GetEnvironmentVariable_NonProtected(CStrNonTracked("TEMPDIR"));
	if(!TmpDir.f_IsEmpty())
		return NMib::NFile::CFile::fs_AppendPath(TmpDir, fg_GetProgramUserNameNonTracked());
	return "/tmp";
}

NStr::CStr NSys::NFile::fg_GetRawTemporaryDirectory()
{
	NMib::NStr::CStr TmpDir = fg_GetSys()->f_GetEnvironmentVariable("TMPDIR");
	if (TmpDir.f_IsEmpty())
		TmpDir = fg_GetSys()->f_GetEnvironmentVariable("TMP");
	if (TmpDir.f_IsEmpty())
		TmpDir = fg_GetSys()->f_GetEnvironmentVariable("TEMP");
	if (TmpDir.f_IsEmpty())
		TmpDir = fg_GetSys()->f_GetEnvironmentVariable("TEMPDIR");
	if (!TmpDir.f_IsEmpty())
	{
		mint Len = TmpDir.f_GetLen();
		if (TmpDir[Len - 1] == '/')
			return TmpDir.f_Left(Len - 1);
		return TmpDir;
	}
	return "/tmp";
}

NStr::CStrNonTracked NSys::NFile::fg_GetRawTemporaryDirectoryNonTracked()
{
	NMib::NStr::CStrNonTracked TmpDir = fg_Process_GetEnvironmentVariable_NonProtected(CStrNonTracked("TMPDIR"));
	if (TmpDir.f_IsEmpty())
		TmpDir = fg_Process_GetEnvironmentVariable_NonProtected(CStrNonTracked("TMP"));
	if (TmpDir.f_IsEmpty())
		TmpDir = fg_Process_GetEnvironmentVariable_NonProtected(CStrNonTracked("TEMP"));
	if (TmpDir.f_IsEmpty())
		TmpDir = fg_Process_GetEnvironmentVariable_NonProtected(CStrNonTracked("TEMPDIR"));
	if (!TmpDir.f_IsEmpty())
	{
		mint Len = TmpDir.f_GetLen();
		if (TmpDir[Len - 1] == '/')
			return TmpDir.f_Left(Len - 1);
		return TmpDir;
	}
	return "/tmp";
}

namespace NMib
{
	namespace NSys
	{
		namespace NFile
		{
			template <typename tf_CStr>
			tf_CStr fg_GetProgramDirectoryGeneral()
			{
				static_assert(tf_CStr::mc_Type == EStrType_UTF && sizeof(typename tf_CStr::CChar) == 1);
				uint32 Len = 0;
				_NSGetExecutablePath(nullptr, &Len);
				tf_CStr Ret;
				_NSGetExecutablePath(Ret.f_GetStr(Len+1), &Len);
				return NMib::NFile::CFile::fs_GetPath(NMib::NFile::CFile::fs_GetExpandedPath(Ret));
			}
			template <typename tf_CStr>
			tf_CStr fg_GetProgramPathGeneral()
			{
				static_assert(tf_CStr::mc_Type == EStrType_UTF && sizeof(typename tf_CStr::CChar) == 1);
				uint32 Len = 0;
				_NSGetExecutablePath(nullptr, &Len);
				tf_CStr Ret;
				_NSGetExecutablePath(Ret.f_GetStr(Len+1), &Len);
				return NMib::NFile::CFile::fs_GetExpandedPath(Ret);
			}
		}
	}
}

CStr NSys::NFile::fg_GetProgramDirectory()
{
	return fg_GetProgramDirectoryGeneral<CStr>();
}

CStrNonTracked NSys::NFile::fg_GetProgramDirectoryNonTracked()
{
	return fg_GetProgramDirectoryGeneral<CStrNonTracked>();
}


CStr NSys::NFile::fg_GetProgramPath()
{
	return fg_GetProgramPathGeneral<CStr>();
}

CStrNonTracked NSys::NFile::fg_GetProgramPathNonTracked()
{
	return fg_GetProgramPathGeneral<CStrNonTracked>();
}

NMib::NStr::CStr NSys::NFile::fg_GetModulePath(void *_pCode)
{
	Dl_info ModuleInfo;
	if (dladdr(_pCode, &ModuleInfo) != 0)
		return NMib::NFile::CFile::fs_GetExpandedPath(NMib::NStr::CStr(ModuleInfo.dli_fname));
	else
		return NMib::NStr::CStr();
}

NMib::NStr::CStrNonTracked NSys::NFile::fg_GetModulePathNonTracked(void *_pCode)
{
	Dl_info ModuleInfo;
	if (dladdr(_pCode, &ModuleInfo) != 0)
		return NMib::NFile::CFile::fs_GetExpandedPath(NMib::NStr::CStrNonTracked(ModuleInfo.dli_fname));
	else
		return NMib::NStr::CStrNonTracked();
}

NMib::NStr::CStr NSys::NFile::fg_GetUserHomeDirectory()
{
    return fg_MacOSX_GetUserHomeDirectory();
}

NMib::NStr::CStrNonTracked NSys::NFile::fg_GetUserHomeDirectoryNonTracked()
{
    return fg_MacOSX_GetUserHomeDirectoryNonTracked();
}


NMib::NStr::CStr NSys::NFile::fg_GetLogDirectory()
{
	return NMib::NFile::CFile::fs_AppendPath(fg_MacOSX_GetLogDirectory(), fg_GetProgramUserName());
}


NMib::NStr::CStrNonTracked NSys::NFile::fg_GetLogDirectoryNonTracked()
{
	return NMib::NFile::CFile::fs_AppendPath(fg_MacOSX_GetLogDirectoryNonTracked(), fg_GetProgramUserNameNonTracked());
}


// see man copyfile

static int fg_CopyOrRename(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo, bool _bRename)
{
	CStr From = _FileFrom;
	CStr To = _FileTo;

	copyfile_state_t CopyState;
	errno = 0;
	CopyState = copyfile_state_alloc();
	if (!CopyState)
	{
		if (!errno)
			return 1;
		return errno;
	}

	uint32_t Flags = COPYFILE_ALL;
	if (_bRename)
		Flags |= COPYFILE_MOVE;

	errno = 0;
	int Result = copyfile(From.f_GetStr(), To.f_GetStr(), CopyState, Flags);
	if (Result != 0)
	{
		if (errno)
			Result = errno;
	}

	copyfile_state_free(CopyState);

	return Result;
}

struct CCopyFileContext
{
	NMib::NFile::CFileProgress* m_pProgress;
	CMibFilePos m_nFileSize;
	NMib::NStr::CStr m_FileFrom;
	bool m_bSizeInit = false;

	CMibFilePos f_GetSize()
	{
		if (!m_bSizeInit)
		{
			m_bSizeInit = true;
			m_nFileSize = 0;
			try
			{
				m_nFileSize = NSys::NFile::fg_GetSize(m_FileFrom);
			}
			catch (NFile::CExceptionFile const &)
			{
			}
		}
		return m_nFileSize;
	}
};

static int SingleFileCopyFileCallback(int _What, int _Stage, copyfile_state_t _State, char const* _pSrc, char const* _pDst, void* _pContext)
{
	CCopyFileContext* pContext = (CCopyFileContext*)_pContext;

	switch(_What)
	{
		case COPYFILE_COPY_DATA:
			{
				if (pContext && pContext->m_pProgress)
				{
					off_t nBytesCopied = 0;
					copyfile_state_get(_State, COPYFILE_STATE_COPIED, &nBytesCopied);

					pContext->m_pProgress->f_Progress( CMibFilePos(nBytesCopied), pContext->f_GetSize() );
				}
			}
		break;
	}

	return COPYFILE_CONTINUE;
}

static int fg_CopyOrRename(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo, NMib::NFile::CFileProgress& _Progress, bool _bRename)
{
	CStr From = _FileFrom;
	CStr To = _FileTo;

	copyfile_state_t CopyState;
	errno = 0;
	CopyState = copyfile_state_alloc();
	if (!CopyState)
	{
		if (!errno)
			return 1;
		return errno;
	}

	CCopyFileContext Context;
	if (CSystem::ms_PlatformVersion >= 10'06'00)
	{
		Context.m_pProgress = &_Progress;
		Context.m_FileFrom = _FileFrom;

		copyfile_state_set(CopyState, COPYFILE_STATE_STATUS_CB, (void*)&SingleFileCopyFileCallback);
		copyfile_state_set(CopyState, COPYFILE_STATE_STATUS_CTX, &Context);
	}

	uint32_t Flags = COPYFILE_ALL;
	if (_bRename)
		Flags |= COPYFILE_MOVE;

	errno = 0;
	int Result = copyfile(From.f_GetStr(), To.f_GetStr(), CopyState, Flags);
	if (Result != 0)
	{
		if (errno)
			Result = errno;
	}

	copyfile_state_free(CopyState);

	return Result;
}

void NSys::NFile::fg_Duplicate(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo)
{
	if (!&clonefile)
		DMibErrorFile("clonefile function not available in this version of macOS");

	if (clonefile(_FileFrom, _FileTo, 0))
		DMibErrorFile(NMib::NPlatform::fg_FormatErrno(CStr::CFormat("clonefile('{}', '{}')") << _FileFrom << _FileTo, errno));
}

bool NSys::NFile::fg_TryDuplicate(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo)
{
	if (!&clonefile)
		return false;

	if (clonefile(_FileFrom, _FileTo, 0))
		return false;

	return true;
}

void NSys::NFile::fg_Copy(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo)
{
	if (auto ErrNo = fg_CopyOrRename(_FileFrom, _FileTo, false))
		DMibErrorFile(NMib::NPlatform::fg_FormatErrno(CStr::CFormat("copyfile('{}', '{}') when copying file") << _FileFrom << _FileTo, ErrNo));
}

void NSys::NFile::fg_Rename(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo)
{
	if (NMib::NFile::CFile::fs_FileExists(_FileFrom, NMib::NFile::EFileAttrib_Directory))
	{
		if (rename(_FileFrom, _FileTo))
			DMibErrorFile(NMib::NPlatform::fg_FormatErrno(CStr::CFormat("rename('{}', '{}')") << _FileFrom << _FileTo, errno));
		return;
	}

	if (rename(_FileFrom, _FileTo))
	{
		int Error = errno;
		if (Error != EXDEV)
			DMibErrorFile(NMib::NPlatform::fg_FormatErrno(CStr::CFormat("rename('{}', '{}')") << _FileFrom << _FileTo, Error));
		if (auto ErrNo = fg_CopyOrRename(_FileFrom, _FileTo, true))
			DMibErrorFile(NMib::NPlatform::fg_FormatErrno(CStr::CFormat("copyfile('{}', '{}') when renaming file") << _FileFrom << _FileTo, ErrNo));
	}
}

void NSys::NFile::fg_Copy(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo, NMib::NFile::CFileProgress &_Progress)
{
	if (auto ErrNo = fg_CopyOrRename(_FileFrom, _FileTo, _Progress, false))
		DMibErrorFile(NMib::NPlatform::fg_FormatErrno(CStr::CFormat("copyfile('{}', '{}') when copying file") << _FileFrom << _FileTo, ErrNo));
}

void NSys::NFile::fg_Rename(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo, NMib::NFile::CFileProgress &_Progress)
{
	if (NMib::NFile::CFile::fs_FileExists(_FileFrom, NMib::NFile::EFileAttrib_Directory))
	{
		if (rename(_FileFrom, _FileTo))
			DMibErrorFile(NMib::NPlatform::fg_FormatErrno(CStr::CFormat("rename('{}', '{}')") << _FileFrom << _FileTo, errno));
		return;
	}

	if (rename(_FileFrom, _FileTo))
	{
		int Error = errno;
		if (Error != EXDEV)
			DMibErrorFile(NMib::NPlatform::fg_FormatErrno(CStr::CFormat("rename('{}', '{}')") << _FileFrom << _FileTo, Error));
		if (auto ErrNo = fg_CopyOrRename(_FileFrom, _FileTo, _Progress, true))
			DMibErrorFile(NMib::NPlatform::fg_FormatErrno(CStr::CFormat("copyfile('{}', '{}') when renaming file") << _FileFrom << _FileTo, ErrNo));
	}
}

void NSys::NFile::fg_AtomicReplace(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo)
{
	if (rename(_FileFrom, _FileTo))
		DMibErrorFile(NMib::NPlatform::fg_FormatErrno(CStr::CFormat("rename('{}', '{}')") << _FileFrom << _FileTo, errno));
}

#include <CoreServices/CoreServices.h>

void *NSys::NFile::fg_ChangeNotification_Open(const CStr &_FileName, NMib::NFile::EFileChange _OpenFlags, NMib::NThread::CSemaphoreReportableAggregate *_pReportTo)
{
	return fg_GetLocalSys()->m_FileChangeNoticationContext->f_Open(_FileName, _OpenFlags, _pReportTo);
}

void NSys::NFile::fg_ChangeNotification_Close(void *_pNotification)
{
	fg_GetLocalSys()->m_FileChangeNoticationContext->f_Close(_pNotification);
}

bool NSys::NFile::fg_ChangeNotification_Changed(void *_pNotification)
{
	return fg_GetLocalSys()->m_FileChangeNoticationContext->f_Changed(_pNotification);
}

bool NSys::NFile::fg_ChangeNotification_GetNotification(void *_pNotification, NMib::NStr::CStr &_Path, NMib::NFile::EFileChangeNotification &_Notification, NMib::NStr::CStr &_PathFrom)
{
	return fg_GetLocalSys()->m_FileChangeNoticationContext->f_GetNotification(_pNotification, _Path, _Notification, _PathFrom);
}

bool NSys::NFile::fg_ChangeNotification_Supported()
{
	return true;
}

// *************************************************************************************************************************
// Net Implementation
// *************************************************************************************************************************

#include "Malterlib_Core_PlatformImp_MacOSX_Net.imp.h"

NSys::NNetwork::CAddress NSys::NNetwork::fg_CreateAddress(::NMib::NNetwork::ENetAddressType _Type, void const* _pData, mint _nDataBytes)
{
	return (NSys::NNetwork::CAddress)fg_GetLocalSys()->m_SocketContext->f_CreateAddress(_Type, _pData, _nDataBytes);
}

NSys::NNetwork::CAddress NSys::NNetwork::fg_DuplicateAddress(NSys::NNetwork::CAddress _Address)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return (NSys::NNetwork::CAddress)fg_GetLocalSys()->m_SocketContext->f_DuplicateAddress(*(CPOSIXAddress*)_Address);
}

::NMib::NNetwork::ENetAddressType NSys::NNetwork::fg_GetAddressType(NSys::NNetwork::CAddress _Address)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_GetAddressType(*(CPOSIXAddress*)_Address);
}

bool NSys::NNetwork::fg_GetAddressRaw(NSys::NNetwork::CAddress _Address, ::NMib::NNetwork::ENetAddressType _ExpectedType, void* _opRawData, mint _nDataBytes)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_GetAddressRaw(*(CPOSIXAddress*)_Address, _ExpectedType, _opRawData, _nDataBytes);
}

NSys::NNetwork::CAddress NSys::NNetwork::fg_SetAddressRaw(NSys::NNetwork::CAddress _Address, ::NMib::NNetwork::ENetAddressType _Type, void const* _pRawData, mint _nDataBytes)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return (NSys::NNetwork::CAddress)fg_GetLocalSys()->m_SocketContext->f_SetAddressRaw((CPOSIXAddress*)_Address, _Type, _pRawData, _nDataBytes);
}

NSys::NNetwork::CAddress NSys::NNetwork::fg_ResolveAddress(const NMib::NStr::CStr &_Address, ::NMib::NNetwork::ENetAddressType _PreferType)
{
	return fg_GetLocalSys()->m_SocketContext->f_ResolveAddress(_Address, _PreferType);
}

mint NSys::NNetwork::fg_GetMaxUnixSocketNameLength()
{
	return sizeof(sockaddr_un::sun_path) - 1;
}

void *NSys::NNetwork::fg_AsyncResolveAddress_Open(const NMib::NStr::CStr &_Address, ::NMib::NNetwork::ENetAddressType _PreferType, NMib::NFunction::TCFunction<void ()> &&_fOnFinish)
{
	return fg_GetLocalSys()->m_SocketContext->f_AsyncResolveAddress_Open(_Address, _PreferType, fg_Move(_fOnFinish));
}

bool NSys::NNetwork::fg_AsyncResolveAddress_GetResult(void *_pResolver, NSys::NNetwork::CAddress& _opAddress, NMib::NStr::CStr &_Error)
{
	return fg_GetLocalSys()->m_SocketContext->f_AsyncResolveAddress_GetResult(_pResolver, (CPOSIXAddress*&)_opAddress, _Error);
}

void NSys::NNetwork::fg_AsyncResolveAddress_Close(void *_pResolver)
{
	fg_GetLocalSys()->m_SocketContext->f_AsyncResolveAddress_Close(_pResolver);
}

int NSys::NNetwork::fg_CompareAddresses(NSys::NNetwork::CAddress _pFirst, NSys::NNetwork::CAddress _pSecond)
{
	DMibSafeCheck(_pFirst != nullptr, "Address is null!");
	DMibSafeCheck(_pSecond != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_CompareAddresses(*(CPOSIXAddress*)_pFirst, *(CPOSIXAddress*)_pSecond);
}

void NSys::NNetwork::fg_FreeAddress(NSys::NNetwork::CAddress _Address) // It is OK to free a nullptr address
{
	return fg_GetLocalSys()->m_SocketContext->f_FreeAddress((CPOSIXAddress*)_Address);
}

NMib::NStr::CStr NSys::NNetwork::fg_GetAddressString(NSys::NNetwork::CAddress _Address, bool _bIncludeType)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_GetAddressString(*(CPOSIXAddress*)_Address, _bIncludeType);
}

// Connection Operations
void *NSys::NNetwork::fg_AsyncConnect
	(
	 	NSys::NNetwork::CAddress _Address
	 	, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
	 	, NSys::NNetwork::CAddress _BindAddress
	)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_AsyncConnect(*(CPOSIXAddress*)_Address, fg_Move(_fOnStateChange), (CPOSIXAddress *)_BindAddress);
}

void NSys::NNetwork::fg_StartSocket(void *_pSocket)
{
	return fg_GetLocalSys()->m_SocketContext->f_StartSocket((CPOSIXSocket *)_pSocket);
}

void *NSys::NNetwork::fg_Listen
	(
	 	NSys::NNetwork::CAddress _Address
	 	, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
	 	, NMib::NNetwork::ENetFlag _Flags
	)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_Listen(*(CPOSIXAddress*)_Address, fg_Move(_fOnStateChange), _Flags);
}

void *NSys::NNetwork::fg_ListenDatagram
	(
	 	NSys::NNetwork::CAddress _Address
	 	, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
	 	, NMib::NNetwork::ENetFlag _Flags
	)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_ListenDatagram(*(CPOSIXAddress*)_Address, fg_Move(_fOnStateChange), _Flags);
}

void *NSys::NNetwork::fg_Accept(void *_pSocket, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange)
{
	return fg_GetLocalSys()->m_SocketContext->f_Accept((CPOSIXSocket*)_pSocket, fg_Move(_fOnStateChange));
}

void NSys::NNetwork::fg_Close(void *_pSocket) // Closes the socket and connectio
{
	fg_GetLocalSys()->m_SocketContext->f_Close((CPOSIXSocket*)_pSocket);
}

void NSys::NNetwork::fg_Shutdown(void *_pSocket)
{
	fg_GetLocalSys()->m_SocketContext->f_Shutdown((CPOSIXSocket*)_pSocket);
}

mint NSys::NNetwork::fg_Receive(void *_pSocket, void *_pData, mint _DataLen) // Returns bytes receive
{
	return fg_GetLocalSys()->m_SocketContext->f_Receive((CPOSIXSocket*)_pSocket, _pData, _DataLen);
}

mint NSys::NNetwork::fg_Send(void *_pSocket, const void *_pData, mint _DataLen) // Returns bytes sen
{
	return fg_GetLocalSys()->m_SocketContext->f_Send((CPOSIXSocket*)_pSocket, _pData, _DataLen);
}

mint NSys::NNetwork::fg_SendDatagram(void *_pSocket, NSys::NNetwork::CAddress _Address, const void *_pData, mint _DataLen) // Returns bytes sen
{
	return fg_GetLocalSys()->m_SocketContext->f_SendDatagram((CPOSIXSocket*)_pSocket, *((CPOSIXAddress*)_Address), _pData, _DataLen);
}

mint NSys::NNetwork::fg_ReceiveDatagram(void *_pSocket, NSys::NNetwork::CAddress _Address, void *_pData, mint _DataLen) // Returns bytes sen
{
	return fg_GetLocalSys()->m_SocketContext->f_ReceiveDatagram((CPOSIXSocket*)_pSocket, *((CPOSIXAddress*)_Address), _pData, _DataLen);
}

// Socket Properties & State

void NSys::NNetwork::fg_SetOnStateChange(void *_pSocket, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange)
{
	fg_GetLocalSys()->m_SocketContext->f_SetOnStateChange((CPOSIXSocket*)_pSocket, fg_Move(_fOnStateChange));
}

NMib::NNetwork::ENetTCPState NSys::NNetwork::fg_GetState(void *_pSocket) // Get the state of data availabl
{
	return fg_GetLocalSys()->m_SocketContext->f_GetState((CPOSIXSocket*)_pSocket);
}

NMib::NStr::CStr NSys::NNetwork::fg_GetCloseReason(void *_pSocket)
{
	return fg_GetLocalSys()->m_SocketContext->f_GetCloseReason((CPOSIXSocket*)_pSocket);
}

void *NSys::NNetwork::fg_InheritHandle2(void *_pSocket, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange)
{
	return fg_GetLocalSys()->m_SocketContext->f_InheritHandle2((CPOSIXSocket*)_pSocket, fg_Move(_fOnStateChange));
}

void *NSys::NNetwork::fg_GiveUpForInherit(void *_pSocket)
{
	return fg_GetLocalSys()->m_SocketContext->f_GiveUpForInherit((CPOSIXSocket*)_pSocket);
}

void *NSys::NNetwork::fg_GetOSSocket(void *_pSocket)
{
	return fg_GetLocalSys()->m_SocketContext->f_GetOSSocket((CPOSIXSocket*)_pSocket);
}

NSys::NNetwork::CAddress NSys::NNetwork::fg_GetPeerAddress(void *_pSocket)
{
	return (NSys::NNetwork::CAddress)fg_GetLocalSys()->m_SocketContext->f_GetPeerAddress((CPOSIXSocket*)_pSocket);
}

uint32 NSys::NNetwork::fg_GetListenPort(void *_pSocket)
{
	return fg_GetLocalSys()->m_SocketContext->f_GetListenPort((CPOSIXSocket*)_pSocket);
}

void NSys::NFile::fg_FileEnumOtherHandles(const NMib::NStr::CStr &_FileName, NContainer::TCVector<NMib::NFile::CFileHandle> &_HandleInfo)
{
	DMibPDebugBreak; // Not implemented
}

void NSys::NFile::fg_FileEnumOtherHandles(void *_pFile, NContainer::TCVector<NMib::NFile::CFileHandle> &_HandleInfo)
{
	DMibPDebugBreak; // Not implemented
}

bool NMib::NSys::fg_ConsoleOutputValid()
{
	return true;
}

bool NMib::NSys::fg_ConsoleInputValid()
{
	return true;
}

bool NMib::NSys::fg_ConsoleErrorOutputValid()
{
	return true;
}

void NSys::fg_Thread_Yield()
{
	sched_yield();
}

namespace NMib::NThread
{
	static_assert(sizeof(os_unfair_lock) == sizeof(CLowLevelLockAggregate::m_Lock));
	static_assert(os_unfair_lock(OS_UNFAIR_LOCK_INIT)._os_unfair_lock_opaque == 0);

	void CLowLevelLockAggregate::f_ForkedChildUnlocked()
	{
		m_Lock = 0;
	}

	void CLowLevelLockAggregate::f_ForkedChildLocked()
	{
#if DPlatformVersion < 10120
		if (&os_unfair_lock_lock)
		{
#endif
			m_Lock = 0;
			os_unfair_lock_lock((os_unfair_lock_t)&m_Lock);
#if DPlatformVersion < 10120
		}
		else
			m_Lock = 1;
#endif
	}

	void CLowLevelLockAggregate::f_Construct()
	{
		DMibSanitizerAnnotate_MutexCreate(this, __tsan_mutex_not_static);
		m_Lock = 0;
	}

	void CLowLevelLockAggregate::f_Destruct()
	{
		DMibSanitizerAnnotate_MutexDestroy(this, 0);
	}

	void CLowLevelLockAggregate::f_Lock()
	{
		DMibSanitizerAnnotate_MutexPreLock(this, 0);
#if DPlatformVersion < 10120
		if (&os_unfair_lock_lock)
		{
#endif
			os_unfair_lock_lock((os_unfair_lock_t)&m_Lock);
#if DPlatformVersion < 10120
		}
		else
		{
			uint32 Expected = 0;
			while (!m_Lock.f_CompareExchangeStrong(Expected, 1, NAtomic::EMemoryOrder_Acquire, NAtomic::EMemoryOrder_Acquire))
			{
				yield_cpu;
				yield_cpu;
				yield_cpu;
				yield_cpu;
				yield_cpu;
				yield_cpu;
				yield_cpu;
				yield_cpu;
				yield_cpu;
				yield_cpu;
				Expected = 0;
			}
		}
#endif

#if DMibEnableSafeCheck > 0
		m_ThreadID = NSys::fg_Thread_GetCurrentUID();
		m_AlternateThreadID = NSys::fg_Thread_GetCurrentUIDAlternate();
#endif
		DMibSanitizerAnnotate_MutexPostLock(this, 0, 1);
	}

	void CLowLevelLockAggregate::f_Unlock()
	{
		DMibSanitizerAnnotate_MutexPreUnlock(this, 0);
#if DMibEnableSafeCheck > 0
		m_ThreadID = 0;
		m_AlternateThreadID = 0;
#endif
#if DPlatformVersion < 10120
		if (&os_unfair_lock_lock)
#endif
			os_unfair_lock_unlock((os_unfair_lock_t)&m_Lock);
#if DPlatformVersion < 10120
		else
			m_Lock.f_Exchange(0, NAtomic::EMemoryOrder_Release);
#endif

		DMibSanitizerAnnotate_MutexPostUnlock(this, 0);
	}
}

uint16 NSys::fg_Langague_GetSystemLanguage(NMib::NStr::CStr &_Language)
{
	_Language = fg_MacOSX_GetSystemLanguage();
	return 0;
}

void* NSys::fg_GetExeData(char const* _pSegment, char const* _pSection, unsigned long long& _nDataBytes)
{

	unsigned long nBytes;
	uint8 * pData = (uint8 *)getsectdata(_pSegment, _pSection, &nBytes);

	pData += _dyld_get_image_vmaddr_slide(0);

	_nDataBytes = nBytes;
	return pData;

}

mint NSys::fg_Thread_GetVirtualCores()
{
 	int nCPUs = 0;
 	size_t DataSize = sizeof(nCPUs);
	int Ret = sysctlbyname("hw.logicalcpu", &nCPUs, &DataSize, nullptr, 0);

	if (Ret == 0)
		return (nCPUs > 0) ? nCPUs : 1;
	return 1;
}

mint NSys::fg_Thread_GetPhysicalCores()
{
 	int nCPUs = 0;
 	size_t DataSize = sizeof(nCPUs);
	int Ret = sysctlbyname("hw.physicalcpu", &nCPUs, &DataSize, nullptr, 0);

	if (Ret == 0)
		return (nCPUs > 0) ? nCPUs : 1;
	return 1;
}

NMib::NStr::CStr NSys::fg_System_GetCPUName()
{

	CStr Name;
 	size_t DataSize = 1024;
	int Ret = sysctlbyname("machdep.cpu.brand_string", Name.f_GetStr(1024), &DataSize, nullptr, 0);

	if (Ret == 0)
	{
		Name.f_SetAt(DataSize, 0);
		Name.f_TrimSize();
		return Name;
	}
	return "Unknown";
}


inline_never mint NMib::NSys::fg_System_GetStackTrace(CMibCodeAddress *_pStack, mint _nMaxDepth)
{
	return (mint)backtrace((void**)_pStack, (int)_nMaxDepth);
}

inline_never CMibCodeAddress NMib::NSys::fg_System_GetStackTrace(aint _iDepth)
{
	if (_iDepth > 255)
		return 0;
	CMibCodeAddress StackTraces[256];
	StackTraces[_iDepth] = 0;

	backtrace((void**)StackTraces, (int)_iDepth + 1);

	return StackTraces[_iDepth];
}

namespace
{
	class CCPUUsageMonitorImpl
	{
		host_cpu_load_info_data_t m_LastLoadStats;

		NMib::NSystem::CSystemCPUUsage m_LastRet;
	public:
		CCPUUsageMonitorImpl()
		{
			mach_msg_type_number_t Count = HOST_CPU_LOAD_INFO_COUNT;
			host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&m_LastLoadStats, &Count);

			m_LastRet.m_User = 0.0f;
			m_LastRet.m_Kernel = 0.0f;
			m_LastRet.m_Idle = 0.0f;
		}
		virtual ~CCPUUsageMonitorImpl()
		{
		}

	public:
		NMib::NSystem::CSystemCPUUsage f_GetUsage(bool &_bChanged)
		{
			host_cpu_load_info_data_t LoadStats;
			mach_msg_type_number_t Count = HOST_CPU_LOAD_INFO_COUNT;
			host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&LoadStats, &Count);

			uint64 IdleTime = LoadStats.cpu_ticks[CPU_STATE_IDLE];
			uint64 UserTime = LoadStats.cpu_ticks[CPU_STATE_USER] + LoadStats.cpu_ticks[CPU_STATE_NICE];
			uint64 KernelTime = LoadStats.cpu_ticks[CPU_STATE_SYSTEM];

			uint64 LastIdleTime = m_LastLoadStats.cpu_ticks[CPU_STATE_IDLE];
			uint64 LastUserTime = m_LastLoadStats.cpu_ticks[CPU_STATE_USER] + m_LastLoadStats.cpu_ticks[CPU_STATE_NICE];
			uint64 LastKernelTime = m_LastLoadStats.cpu_ticks[CPU_STATE_SYSTEM];

			uint64 DiffIdleTime = IdleTime - LastIdleTime;
			uint64 DiffUserTime = UserTime - LastUserTime;
			uint64 DiffKernelTime = KernelTime - LastKernelTime;

			uint64 TotalTime = DiffIdleTime + DiffUserTime + DiffKernelTime;

			if (TotalTime < 100)
			{
				_bChanged = false;
				return m_LastRet; // Guarantee 1 % resolution
			}

			_bChanged = true;

			NMib::NSystem::CSystemCPUUsage Ret;
			Ret.m_Idle = fp64(DiffIdleTime) / fp64(TotalTime);
			Ret.m_User = fp64(DiffUserTime) / fp64(TotalTime);
			Ret.m_Kernel = fp64(DiffKernelTime) / fp64(TotalTime);

			m_LastLoadStats = LoadStats;

			m_LastRet = Ret;
			return Ret;
		}
	};

}

namespace NMib
{
	namespace NSys
	{
		void *fg_System_CPUUsageMonitor_Open()
		{
			NStorage::TCUniquePointer<CCPUUsageMonitorImpl> pMonitor = fg_Construct();

			return pMonitor.f_Detach();
		}

		void fg_System_CPUUsageMonitor_Close(void *_pHandle)
		{
			NStorage::TCUniquePointer<CCPUUsageMonitorImpl> pMonitor = fg_Explicit((CCPUUsageMonitorImpl *)_pHandle);
		}

		NSystem::CSystemCPUUsage fg_System_CPUUsageMonitor_GetUsage(void *_pHandle, bool &_bChanged)
		{
			return ((CCPUUsageMonitorImpl *)_pHandle)->f_GetUsage(_bChanged);
		}

	}
}


void NSys::fg_Mem_EnableMemoryToucher(bool _bEnabled, fp64 _CPUUsage)
{
}

#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <mach/vm_map.h>
#include <mach/vm_statistics.h>
#include <sys/mman.h>
#include <mach/mach_vm.h>

void *NSys::fg_Mem_VirtualAllocInRange(mint &_Size, uint8 *_pLower, uint8 *_pUpper, EAllocationFlag _AllocFlags, ENumaNode _NumaNode, mint _Alignment)
{
	DMibFastCheck(_AllocFlags & EAllocationFlag_WillFreeWithSize);
	DMibFastCheck(_Alignment == 0);

	auto MachTask = mach_task_self();

	_Size = fg_AlignUp(_Size, NMib::NSys::NPrivate::g_PageSize);

	_pLower = fg_AlignUp(_pLower, NMib::NSys::NPrivate::g_PageSize);
	_pUpper = fg_AlignDown(_pUpper, NMib::NSys::NPrivate::g_PageSize);

	vm_address_t AllocAddress = (vm_address_t)(mint)_pLower;
	vm_address_t EndAddress = (vm_address_t)(mint)_pUpper - _Size;

	while (AllocAddress < EndAddress)
	{
		auto err = vm_allocate( MachTask, &AllocAddress, _Size, 0);
		if (err == KERN_SUCCESS)
			return (void *) AllocAddress;

		vm_size_t Size;
		vm_region_basic_info_64 BasicInfo;
		mach_msg_type_number_t InfoCount = VM_REGION_BASIC_INFO_COUNT_64;
		mach_port_t ObjectName = MACH_PORT_NULL;
		err = vm_region_64(MachTask, &AllocAddress, &Size, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&BasicInfo, &InfoCount, &ObjectName);
		if (err == KERN_SUCCESS)
		{
			AllocAddress = AllocAddress + Size;
		}
		else
			AllocAddress += NMib::NSys::NPrivate::g_PageSize;

		if (ObjectName != MACH_PORT_NULL)
			mach_port_deallocate(MachTask, ObjectName);
	}

	return nullptr;
}

void NSys::fg_Mem_VirtualFlushInstructionCache(void *_pMem, mint _Size)
{
	uint8 *pStart = fg_AlignDown((uint8 *)_pMem, NMib::NSys::NPrivate::g_PageSize);
	uint8 *pEnd = fg_AlignUp((uint8 *)_pMem + _Size, NMib::NSys::NPrivate::g_PageSize);
    if (msync(pStart, pEnd - pStart, MS_INVALIDATE | MS_SYNC))
		DMibError(NMib::NPlatform::fg_FormatErrno("msync", errno));
}



void NSys::fg_TerminateProcess(aint _ExitCode)
{
//	fflush(stdout);
//	fflush(stderr);
//	_exit(_ExitCode);
	raise(SIGKILL);
}

bool NSys::fg_HW_GetVirtualMachineInfo(CVirtualMachineInfo& _Info)
{
	_Info.m_bDetected = false;
	_Info.m_pName = nullptr;

	return false;
}

NMib::NFile::ECheckFileRights NSys::NFile::fg_CheckFileRights( const CStr & _File, NMib::NFile::EFileRight _Rights)
{
    if (NMib::NFile::CFile::fs_FileExists(_File))
        return NMib::NFile::ECheckFileRights_Access; // TODO
    else
        return NMib::NFile::ECheckFileRights_DoesNotExist; // TODO
}

ch8 const *NSys::NFile::fg_GetDllExtension()
{
	return ".dylib";
}

//
// Returns true iff some loaded mach-o image contains "addr".
//	info->mh							mach header of image containing addr
//  info->dwarf_section					pointer to start of __TEXT/__eh_frame section
//  info->dwarf_section_length			length of __TEXT/__eh_frame section
//  info->compact_unwind_section		pointer to start of __TEXT/__unwind_info section
//  info->compact_unwind_section_length	length of __TEXT/__unwind_info section
//
// Exists in Mac OS X 10.6 and later

#if DPlatformVersion <= 1050 || defined(DMibNoOSXCrossModuleExceptions)

struct dyld_unwind_sections
{
	const struct mach_header*		mh;
	const void*						dwarf_section;
	uintptr_t						dwarf_section_length;
	const void*						compact_unwind_section;
	uintptr_t						compact_unwind_section_length;
};

#if __LP64__
#define RELOC_SIZE 3
#define LC_SEGMENT_COMMAND		LC_SEGMENT_64
#define LC_ROUTINES_COMMAND		LC_ROUTINES_64
struct macho_header				: public mach_header_64  {};
struct macho_segment_command	: public segment_command_64  {};
struct macho_section			: public section_64  {};
//struct macho_nlist				: public nlist_64  {};
struct macho_routines_command	: public routines_command_64  {};
#else
#define RELOC_SIZE 2
#define LC_SEGMENT_COMMAND		LC_SEGMENT
#define LC_ROUTINES_COMMAND		LC_ROUTINES
struct macho_header				: public mach_header  {};
struct macho_segment_command	: public segment_command {};
struct macho_section			: public section  {};
//struct macho_nlist				: public nlist  {};
struct macho_routines_command	: public routines_command  {};
#endif


/*
extern uint32_t                    _dyld_image_count(void)                              AVAILABLE_MAC_OS_X_VERSION_10_1_AND_LATER;
extern const struct mach_header*   _dyld_get_image_header(uint32_t image_index)         AVAILABLE_MAC_OS_X_VERSION_10_1_AND_LATER;
extern intptr_t                    _dyld_get_image_vmaddr_slide(uint32_t image_index)   AVAILABLE_MAC_OS_X_VERSION_10_1_AND_LATER;
extern const char*                 _dyld_get_image_name(uint32_t image_index)           AVAILABLE_MAC_OS_X_VERSION_10_1_AND_LATER;
*/
#ifdef DMibNoOSXCrossModuleExceptions
namespace
{
	constinit NAtomic::TCAtomic<struct mach_header const *> g_ThisModuleImage = nullptr;
}
#endif

extern "C" bool _dyld_find_unwind_sections(void* addr, struct dyld_unwind_sections* info)
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#ifdef DMibNoOSXCrossModuleExceptions
	if (!g_ThisModuleImage.f_Load(NAtomic::EMemoryOrder_Acquire))
		g_ThisModuleImage.f_Store(_dyld_get_image_header_containing_address((void *)&_dyld_find_unwind_sections));
#endif
	auto pHeader = _dyld_get_image_header_containing_address(addr);
#pragma clang diagnostic pop
	if (!pHeader)
		return false;
#ifdef DMibNoOSXCrossModuleExceptions
	if (pHeader != g_ThisModuleImage)
		return false; // Just return unwind sections for this module, because we don't want exceptions to travel across module boundraries
#endif
	auto nImages = _dyld_image_count();
	intptr_t Slide = 0;
	bool bFound = false;
	for (auto i = 0; i < nImages; ++i)
	{
		if (_dyld_get_image_header(i) == pHeader)
		{
			Slide = _dyld_get_image_vmaddr_slide(i);
			bFound = true;
			break;
		}
	}

	if (!bFound)
		return false;

	uint32_t fEHFrameSectionOffset = 0;
	uint32_t fUnwindInfoSectionOffset = 0;

	const uint8_t *fMachOData = (uint8_t const *)pHeader;

	const uint32_t cmd_count = pHeader->ncmds;
	const struct load_command* const cmds = (struct load_command*) ((char *)pHeader + sizeof(struct macho_header));
	const struct load_command* cmd = cmds;
	for (uint32_t i = 0; i < cmd_count; ++i)
	{
		switch (cmd->cmd)
		{
			case LC_SEGMENT_COMMAND:
			{
				const struct macho_segment_command* seg = (struct macho_segment_command*)cmd;
				const bool isTextSeg = (strcmp(seg->segname, "__TEXT") == 0);
				if (isTextSeg)
				{
					const struct macho_section* const sectionsStart = (struct macho_section*)((char*)seg + sizeof(struct macho_segment_command));
					const struct macho_section* const sectionsEnd = &sectionsStart[seg->nsects];
					for (const struct macho_section* sect=sectionsStart; sect < sectionsEnd; ++sect) {
						if ((strcmp(sect->sectname, "__eh_frame") == 0) )
							fEHFrameSectionOffset = (uint8_t*)sect - fMachOData;
						else if ((strcmp(sect->sectname, "__unwind_info") == 0) )
							fUnwindInfoSectionOffset = (uint8_t*)sect - fMachOData;;
					}
				}
			}
			break;
		}
		cmd = (const struct load_command*)(((char*)cmd)+cmd->cmdsize);
	}


	info->mh = pHeader;
	info->dwarf_section = 0;
	info->dwarf_section_length = 0;
	info->compact_unwind_section = 0;
	info->compact_unwind_section_length = 0;
	if ( fEHFrameSectionOffset != 0 ) {
		const macho_section* sect = (macho_section*)&fMachOData[fEHFrameSectionOffset];
		info->dwarf_section = (void*)(sect->addr + Slide);
		info->dwarf_section_length = sect->size;
	}
	if ( fUnwindInfoSectionOffset != 0 ) {
		const macho_section* sect = (macho_section*)&fMachOData[fUnwindInfoSectionOffset];
		info->compact_unwind_section = (void*)(sect->addr + Slide);
		info->compact_unwind_section_length = sect->size;
	}
	return true;
}

#endif

