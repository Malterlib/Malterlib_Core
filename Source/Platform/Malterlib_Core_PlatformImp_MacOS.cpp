// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#define _LIBCPP_ENABLE_CXX17_REMOVED_UNEXPECTED_FUNCTIONS

#include <Mib/Core/Core>
#include <Mib/Cryptography/RandomID>
#include <Mib/Cryptography/UUID>

#define _DARWIN_USE_64_BIT_INODE

#define DMibAllowCodeStandardViolations 1

#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Authorization.h>
#include <Security/Security.h>
#include <CoreServices/CoreServices.h>

#include <Mib/Concurrency/ThreadSafeQueue>

#include <mach/mach.h>
#include <mach/mach_time.h>
#include <sys/utsname.h>
#include <crt_externs.h>
#include <sys/clonefile.h>
#include <sys/xattr.h>
#include <sys/attr.h>
#include <sys/acl.h>
#include <os/lock.h>
#include <sys/random.h>
#include <exception>

#ifdef DMibConfig_PThreadIntrospection
#include <Mib/Container/MapWithPool>
#include <Mib/Container/SetWithPool>
#include <pthread/introspection.h>
#include "Malterlib_Core_ThreadNotificationCrossModule.h"
#endif

#if __has_feature(ptrauth_calls)
#include <ptrauth.h>
#endif

#include <utility>

using namespace NMib;
using namespace NMib::NStr;
using namespace NMib::NTime;
using namespace NMib::NMemory;
using namespace NMib::NContainer;

#include <Mib/Core/PlatformSpecific/PosixErrNo>
#include <Mib/Core/PlatformSpecific/MacOSOSStatus>

#ifdef DMibDynamicLibrary
bool g_bIsSharedLibrary = true;
#else
bool g_bIsSharedLibrary = false;
#endif

bool g_bRegisteredAtFork = false;
bool g_bForking = false;

#ifdef DMibConfig_PThreadIntrospection
namespace
{
	constinit umint g_iThreadNotificationDestructor = 0;
	#ifndef DMibDynamicLibrary
		constinit NThread::CLowLevelRecursiveLockAggregate g_ThreadNotificationLock = {DAggregateInit};
	#else
		constinit NThread::CLowLevelRecursiveLockAggregate g_OwnThreadNotificationLock = {DAggregateInit};
	#endif

	void fg_PThreadNotificationDestructor(void *_pValue);

	void fg_ReservePThreadNotificationDestructor()
	{
		if (!g_iThreadNotificationDestructor)
			g_iThreadNotificationDestructor = NSys::fg_Thread_AllocLocalWithDestructor(&fg_PThreadNotificationDestructor);
	}

	void fg_FreePThreadNotificationDestructor()
	{
	#ifndef DMibDynamicLibrary
		DMibLock(g_ThreadNotificationLock);
	#else
		DMibLock(g_OwnThreadNotificationLock);
	#endif
		if (!g_iThreadNotificationDestructor)
			return;

		umint iThreadNotificationDestructor = g_iThreadNotificationDestructor;
		g_iThreadNotificationDestructor = 0;
		NSys::fg_Thread_FreeLocalWithDestructor(iThreadNotificationDestructor);
	}

	void fg_SetPThreadNotificationActive(umint _ThreadID)
	{
		DMibFastCheck(g_iThreadNotificationDestructor);
		NSys::fg_Thread_SetLocalDestructor(_ThreadID, g_iThreadNotificationDestructor, (void *)1);
	}

	void fg_SetPThreadNotificationDestroyed()
	{
		DMibFastCheck(g_iThreadNotificationDestructor);
		NSys::fg_Thread_SetLocal(g_iThreadNotificationDestructor, (void *)TCLimitsInt<umint>::mc_Max);
	}
}

inline_always_lto bool NSys::fg_Thread_GetLocalsDestroyed(umint)
{
	DMibFastCheck(g_iThreadNotificationDestructor);
	return (umint)NSys::fg_Thread_GetLocal(g_iThreadNotificationDestructor) == TCLimitsInt<umint>::mc_Max;
}
#endif

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

		void fg_MalterlibSystem_ForkPrepare();
		void fg_MalterlibSystem_ForkParent();
		void fg_MalterlibSystem_ForkChildFinished();
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
		umint Size = __nmemb * __size;
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
			return NMib::NMemory::CAllocator_NonTrackedHeap::f_Resize(__ptr, __size, 0, EAllocationFlag_SizeNotNeeded);
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
	void *nontracked_aligned_alloc(size_t _Alignment, size_t _Size)
	{
		return nontracked_memalign(_Alignment, _Size);
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
		if (!__ptr)
			return 0;

		DMibFastCheck(g_bCanUseSystemMalloc);
		return NMib::NMemory::CAllocator_NonTrackedHeap::f_Size(__ptr);
	}
}

// *************************************************************************************************************************
// POSIX Implementation
// *************************************************************************************************************************

#define DMibPMachKernel
#define DMibConfig_FutexImplemented

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
// macOS Implementation
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

#include "Malterlib_Core_PlatformImp_MacOS_ObjCPP.h"
#if defined(DArchitecture_x64) || defined(DArchitecture_x64)
#include <xmmintrin.h>
#endif

static inline_small class CSystemMacOS *fg_GetLocalSys();

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

[[noreturn]] void NMib::fg_NoReturn()
{
	std::unreachable();
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

void NSys::fg_Security_GenerateHighEntropyData(uint8 *_pData, umint _nBytes)
{
	if (&getentropy)
	{
		while (_nBytes)
		{
			umint ThisTime = fg_Min(_nBytes, 256);
			if (getentropy(_pData, ThisTime))
				DMibPDebugBreak;
			_nBytes -= ThisTime;
			_pData += ThisTime;
		}
		return;
	}

	if (NMib::NPlatform::fg_ReadProcFS("/dev/urandom", _pData, _nBytes) != _nBytes)
		DMibPDebugBreak;
}


NMib::NSys::EDesktopEnvironment NMib::NSys::fg_DesktopEnvironment_Get()
{
	return EDesktopEnvironment_MacOS;
}

class CSystemMacOS : public CSystem
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
			pthread_setspecific(Sys.m_ForkThreadLocal, (void *)(umint)getpid());
			Sys.m_Posix.f_GetMalterlibDisableStdErrLog(); // getenv fails on forked process, to workaround this here

			Sys.m_Posix.m_ForkLock.f_Lock();
			Sys.m_Posix.m_ForkLock.f_PrepareFork();
			Sys.f_PrepareFork();
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
			if (Current == (void *)(umint)getpid())
				__tsan_forked_parent();
			else
				__tsan_forked_child();
#endif

			pthread_setspecific(Sys.m_ForkThreadLocal, 0);
			if (Current != (void *)(umint)getpid())
			{
				Sys.m_bForkedChild = true;
			}
			if (Current == (void *)(umint)getpid())
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
			Sys.f_ForkedParent();
			Sys.m_Posix.m_ForkLock.f_ForkedParent();
			Sys.m_Posix.m_ForkLock.f_Unlock();
		}
	}

	static void fs_ForkChild()
	{
		fs_ForkChildStart();
		fs_ForkChildFinished();
	}

	static void fs_ForkChildStart()
	{
		auto &Sys = *fg_GetLocalSys();
		if (pthread_getspecific(Sys.m_ForkThreadLocal))
		{
			g_bForking = true;
#ifdef DMibSanitizerEnabled_Thread
			__tsan_forked_child();
#endif
			pthread_setspecific(Sys.m_ForkThreadLocal, 0);
			Sys.m_bForkedChild = true;
			g_bCanStartThreads = false;
			Sys.f_ForkedChild();
			Sys.m_Posix.m_ForkLock.f_ForkedChild();
			Sys.m_Posix.m_ForkLock.f_Unlock();

			if (!g_bRegisteredAtFork)
				g_bForking = false;
		}
	}

	static void fs_ForkChildFinished()
	{
		if (!g_bCanStartThreads)
		{
			g_bCanStartThreads = true;
			auto &Sys = *fg_GetLocalSys();
			Sys.f_MemoryManager_CanStartThreads();
			fg_MalterlibMallocOverride_CanStartThreads();
		}
	}

	CSystemMacOS()
		: CSystem(g_bIsSharedLibrary)
		, m_SocketContext{DAggregateInit}
		, m_FileChangeNoticationContext{DAggregateInit}
		, m_TimerFrequency{}
		, m_ForkThreadLocal(TCLimitsInt<pthread_key_t>::mc_Max)
	{
		fg_MemClear(m_SocketContext);

		fp_InitComplete();
	}

	~CSystemMacOS()
	{
	}

	void f_InitThreadLocal()
	{
		if (auto ErrNo = pthread_key_create(&m_ForkThreadLocal, nullptr))
			DMibError(NMib::NPlatform::fg_FormatErrno("pthread_key_create", ErrNo));
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

		if (auto ErrNo = pthread_key_delete(m_ForkThreadLocal))
			DMibError(NMib::NPlatform::fg_FormatErrno("pthread_key_delete", ErrNo));
	}

	static void fs_ThreadDestructionHook(void* _ThreadID)
	{

		fg_GetSys()->f_ThreadLocalFreeThread();
	}

	void f_RegisterDestructionHookForThread()
	{

	}

	NMib::NStorage::TCAggregate<NMib::NMacOSRuntime::CFileChangeNoticationContext, 64> m_FileChangeNoticationContext;


};

static inline_small CSystemMacOS *fg_GetLocalSys()
{
	return (CSystemMacOS *)fg_GetSys();
}

CSystem_POSIX *fg_GetSys_POSIX()
{
	return &fg_GetLocalSys()->m_Posix;
}

uint32 fg_CodepageToCFStringEncoding(uint32 _Codepage)
{
	switch (_Codepage)
	{
	case 1252: return kCFStringEncodingWindowsLatin1;
	case 10000: return kCFStringEncodingMacRoman;
	}

	return kCFStringEncodingInvalidId;
}

void NMib::NStr::NPlatform::fg_SystemEncodeAnsiStr(NMib::NStr::CStr const &_In, NMib::NStr::CAnsiStr &_Out, ch8 _ErrorChar)
{
	fg_SystemEncodeCodePageStr(_In, _Out, 1252, _ErrorChar); // 1252 == Windows Latin 1
}

void NMib::NStr::NPlatform::fg_SystemEncodeAnsiStr(NMib::NStr::CStrNonTracked const &_In, NMib::NStr::CAnsiStrNonTracked &_Out, ch8 _ErrorChar)
{
	fg_SystemEncodeCodePageStr(_In, _Out, 1252, _ErrorChar); // 1252 == Windows Latin 1
}


void NMib::NStr::NPlatform::fg_SystemDecodeAnsiStr(NMib::NStr::CAnsiStr const &_In, NMib::NStr::CStr &_Out)
{
	fg_SystemDecodeCodePageStr(_In, _Out, 1252);  // 1252 == Windows Latin 1
}

void NMib::NStr::NPlatform::fg_SystemDecodeAnsiStr(ch8 const *_pIn, NMib::NStr::CStr &_Out)
{
	fg_SystemDecodeCodePageStr(_pIn, _Out, 1252);  // 1252 == Windows Latin 1
}

void NMib::NStr::NPlatform::fg_SystemDecodeAnsiStr(NMib::NStr::CAnsiStrNonTracked const &_In, NMib::NStr::CStrNonTracked &_Out)
{
	fg_SystemDecodeCodePageStr(_In, _Out, 1252);  // 1252 == Windows Latin 1
}

void NMib::NStr::NPlatform::fg_SystemDecodeAnsiStr(ch8 const *_pIn, NMib::NStr::CStrNonTracked &_Out)
{
	fg_SystemDecodeCodePageStr(_pIn, _Out, 1252);  // 1252 == Windows Latin 1
}

void NMib::NStr::NPlatform::fg_SystemEncodeCodePageStr(NMib::NStr::CStr const &_In, NMib::NStr::CAnsiStr &_Out, uint32 _CodePage, ch8 _ErrorChar)
{
	uint32 CodePage = fg_CodepageToCFStringEncoding(_CodePage);
	if (CodePage == kCFStringEncodingInvalidId)
		DMibError(NMib::NStr::CStrNonTracked::CFormat("Codepage {} not supported") << _CodePage);

	CFStringRef pStringRef = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)_In.f_GetStr(), _In.f_GetLen(), kCFStringEncodingUTF8, false);

	if (!pStringRef)
		DMibError(NMib::NPlatform::fg_FormatErrno("CFStringCreateWithBytes (encode code page str)", errno));

	auto Cleanup0 = g_OnScopeExit / [&]
		{
			CFRelease(pStringRef);
		}
	;

	CFDataRef pData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, pStringRef, CodePage, _ErrorChar);
	if (!pData)
		DMibError(NMib::NPlatform::fg_FormatErrno("CFStringCreateExternalRepresentation (encode code page str)", errno));

	auto Cleanup1 = g_OnScopeExit / [&]
		{
			CFRelease(pData);
		}
	;

	_Out.f_SetStr((ch8 const *)CFDataGetBytePtr(pData), CFDataGetLength(pData));
}

void NMib::NStr::NPlatform::fg_SystemEncodeCodePageStr(NMib::NStr::CStrNonTracked const &_In, NMib::NStr::CAnsiStrNonTracked &_Out, uint32 _CodePage, ch8 _ErrorChar)
{
	uint32 CodePage = fg_CodepageToCFStringEncoding(_CodePage);
	if (CodePage == kCFStringEncodingInvalidId)
		DMibError(NMib::NStr::CStrNonTracked::CFormat("Codepage {} not supported") << _CodePage);

	CFStringRef pStringRef = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)_In.f_GetStr(), _In.f_GetLen(), kCFStringEncodingUTF8, false);

	if (!pStringRef)
		DMibError(NMib::NPlatform::fg_FormatErrno("CFStringCreateWithBytes (encode code page str)", errno));

	auto Cleanup0 = g_OnScopeExit / [&]
		{
			CFRelease(pStringRef);
		}
	;

	CFDataRef pData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, pStringRef, CodePage, _ErrorChar);
	if (!pData)
		DMibError(NMib::NPlatform::fg_FormatErrno("CFStringCreateExternalRepresentation (encode code page str)", errno));

	auto Cleanup1 = g_OnScopeExit / [&]
		{
			CFRelease(pData);
		}
	;

	_Out.f_SetStr((ch8 const *)CFDataGetBytePtr(pData), CFDataGetLength(pData));
}

void NMib::NStr::NPlatform::fg_SystemDecodeCodePageStr(NMib::NStr::CAnsiStr const &_In, NMib::NStr::CStr &_Out, uint32 _CodePage)
{
	uint32 CodePage = fg_CodepageToCFStringEncoding(_CodePage);
	if (CodePage == kCFStringEncodingInvalidId)
		DMibError(NMib::NStr::CStrNonTracked::CFormat("Codepage {} not supported") << _CodePage);

	CFStringRef pStringRef = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)_In.f_GetStr(), _In.f_GetLen(), CodePage, false);

	if (!pStringRef)
		DMibError(NMib::NPlatform::fg_FormatErrno("CFStringCreateWithBytes (decode code page str)", errno));

	auto Cleanup0 = g_OnScopeExit / [&]
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


void NMib::NStr::NPlatform::fg_SystemDecodeCodePageStr(ch8 const *_pIn, NMib::NStr::CStr &_Out, uint32 _CodePage)
{
	uint32 CodePage = fg_CodepageToCFStringEncoding(_CodePage);
	if (CodePage == kCFStringEncodingInvalidId)
		DMibError(NMib::NStr::CStrNonTracked::CFormat("Codepage {} not supported") << _CodePage);

	CFStringRef pStringRef = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)_pIn, fg_StrLen(_pIn), CodePage, false);

	if (!pStringRef)
		DMibError(NMib::NPlatform::fg_FormatErrno("CFStringCreateWithBytes (decode code page str)", errno));

	auto Cleanup0 = g_OnScopeExit / [&]
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


void NMib::NStr::NPlatform::fg_SystemDecodeCodePageStr(NMib::NStr::CAnsiStrNonTracked const &_In, NMib::NStr::CStrNonTracked &_Out, uint32 _CodePage)
{
	uint32 CodePage = fg_CodepageToCFStringEncoding(_CodePage);
	if (CodePage == kCFStringEncodingInvalidId)
		DMibError(NMib::NStr::CStrNonTracked::CFormat("Codepage {} not supported") << _CodePage);

	CFStringRef pStringRef = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)_In.f_GetStr(), _In.f_GetLen(), CodePage, false);

	if (!pStringRef)
		DMibError(NMib::NPlatform::fg_FormatErrno("CFStringCreateWithBytes (decode code page str)", errno));

	auto Cleanup0 = g_OnScopeExit / [&]
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

void NMib::NStr::NPlatform::fg_SystemDecodeCodePageStr(ch8 const *_pIn, NMib::NStr::CStrNonTracked &_Out, uint32 _CodePage)
{
	uint32 CodePage = fg_CodepageToCFStringEncoding(_CodePage);
	if (CodePage == kCFStringEncodingInvalidId)
		DMibError(NMib::NStr::CStrNonTracked::CFormat("Codepage {} not supported") << _CodePage);

	CFStringRef pStringRef = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)_pIn, fg_StrLen(_pIn), CodePage, false);

	if (!pStringRef)
		DMibError(NMib::NPlatform::fg_FormatErrno("CFStringCreateWithBytes (decode code page str)", errno));

	auto Cleanup0 = g_OnScopeExit / [&]
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

//NStorage::TCAggregate<NThread::TCThreadLocal<zint32, NMacOSDebug::CAllocator_NonTrackedHeap, NThread::EThreadLocalFlag_None>> g_DisableHeapOverride;

void NSys::fg_Thread_SetNumaAffinity(void *_pThread, ENumaNode _NumaNode)
{
	// TODO: Implement
}

umint NSys::fg_Mem_GetNumNumaNodes()
{
  // TODO: Implement
  return 0;
}

void NSys::fg_Mem_GetNumaNodes(ENumaNode *_pNodes, umint _nNodes)
{
  // TODO: Implement
}

void *NSys::fg_InterProcess_MemAlloc(ch8 const *_pName, umint _Size, void * &_pMemory)
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
		if (fg_StrCmp(_pArg, "--OutputStdErrToStdOut") == 0)
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
	umint align_cacheline g_SystemMemory[sizeof(CSystemMacOS) / sizeof(umint)];
	umint g_bCreatingSystemDone = false;
	umint g_bCanUseSystemMalloc = false;
	constinit NAtomic::TCAtomic<umint> g_bCanStartThreads{0};
	umint g_bCreatedSystem = false;
}

#include <mach-o/dyld.h>

void fg_ForkPrepare()
{
	CSystemMacOS::fs_ForkPrepare();
}

void fg_ForkParentOrChild()
{
	CSystemMacOS::fs_ForkParentOrChild();
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

		bool fg_System_GetOperatingSystemVersion(int &o_Major, int &o_Minor, int &o_Fix, EOperatingSystemArch &o_Arch, bool _bForceUpdate)
		{
			if (g_OperatingSystemMajor >= 0)
			{
				o_Major = g_OperatingSystemMajor;
				o_Minor = g_OperatingSystemMinor;
				o_Fix = g_OperatingSystemFix;
				o_Arch = g_OperatingSystemArch;
				return g_OperatingSystemMajor != 0;
			}

			CFStr256 VersionString;
			bool bDarwinVersion = true;
			{
				size_t DataSize = 255;
				int Ret = sysctlbyname("kern.osproductversion", VersionString.f_GetStr(256), &DataSize, nullptr, 0);

				if (Ret == 0)
				{
					VersionString.f_SetAt(DataSize, 0);
					VersionString.f_GetLen();
					bDarwinVersion = false;
				}
				else
					VersionString.f_Clear();
			}

			// Arch
			{
				struct utsname un;
				int Res = uname(&un);
				if (Res >= 0)
				{
					if (VersionString.f_IsEmpty())
						VersionString = un.release;

					if (fg_StrCmpNoCase(un.machine, "arm64") == 0)
						g_OperatingSystemArch = EOperatingSystemArch_arm64;
					else
					{
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
				}
				else
				{
					g_OperatingSystemArch = EOperatingSystemArch_Unknown;
					return false;
				}

				o_Arch = g_OperatingSystemArch;
			}

			int Major = 0;
			int Minor = 0;
			int Fix = 0;

			Major = fg_GetStrSep(VersionString, ".").f_ToInt(0);
			Minor = fg_GetStrSep(VersionString, ".").f_ToInt(0);
			Fix = fg_GetStrSep(VersionString, ".").f_ToInt(0);

			if (bDarwinVersion)
			{
				// Try to convert from darwin version to macOS version
				if (Major >= 20)
				{
					g_OperatingSystemMajor = Major - 9;
					g_OperatingSystemMinor = Minor;
					g_OperatingSystemFix = Fix;
				}
				else
				{
					g_OperatingSystemMajor = 10;
					g_OperatingSystemMinor = Major - 4;
					g_OperatingSystemFix = Minor;
				}
			}
			else
			{
				g_OperatingSystemMajor = Major;
				g_OperatingSystemMinor = Minor;
				g_OperatingSystemFix = Fix;
			}

			o_Major = g_OperatingSystemMajor;
			o_Minor = g_OperatingSystemMinor;
			o_Fix = g_OperatingSystemFix;

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
		extern umint g_ThreadLocalOffsetPThread;
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

//#ifs defined(DConfig_Release) && !defined(DConfig)

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

NMib::NStr::CStrNonTracked NSys::fg_System_GetContractViolationMessage()
{
	DMibLock(g_CrashReporterLock);
	if (__crashreporter_info__)
	{
		if (__crashreporter_info__ == g_CrashReporterString.f_GetStr())
			return g_CrashReporterString;
		else
			return __crashreporter_info__;
	}
	else
		return {};
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

	if (g_OperatingSystemMajor > 10 || (g_OperatingSystemMajor == 10 && g_OperatingSystemMinor >= 9))
		g_ThreadLocalOffsetPThread += 16 * sizeof(void *);
	else if (g_OperatingSystemMajor == 10 && g_OperatingSystemMinor >= 7)
		;
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
	constinit umint g_PageSize = 0;

	void (* g_pDestroyAtExit)(void) = nullptr;
}

extern "C" void fg_MalterlibDestroySystem_MacOS()
{
	if (NMib::NSys::NPrivate::g_pDestroyAtExit)
		NMib::NSys::NPrivate::g_pDestroyAtExit();
}

void NSys::fg_CreateSystemMalloc(bool _bProvideDestroySystem)
{
	if (g_bCreatedSystemMalloc)
		return;

	NPrivate::g_PageSize = NSys::fg_Mem_PageSize();

	fg_CreateSystemVersion();

#ifdef DMibConfig_PThreadIntrospection
	fg_ReservePThreadNotificationDestructor();
#endif

	fg_MalterlibMallocOverrideInit();

	g_bCreatedSystemMalloc = true;

	g_VirtualMap.f_Construct();

	g_bCreatingSystemDone = true;

	auto pSystemMemory = (void *)NMib::g_SystemMemory;
	auto pSystem = new(pSystemMemory) CSystemMacOS();
	static_assert(alignof(CSystemMacOS) <= umint(DMibPMemoryCacheLineSize), "Aligment error");

	NSys::fg_Compiler_MakeActive(&pSystemMemory);
	NSys::fg_Compiler_MakeActive(&pSystem);
	DMibFastCheck((void *)pSystem == pSystemMemory);

	g_bCanUseSystemMalloc = true;

	if (!_bProvideDestroySystem)
		NPrivate::g_pDestroyAtExit = &fg_DestroySystemAtExit;
	else
		NPrivate::g_pDestroyAtExit = &fg_DestroySystemThreadsAtExit;

	pSystem->f_InitThreadLocal();

	g_DefaultTerminateHandler = std::set_terminate(&fg_TerminateHandler);
	g_DefaultUnexpectedHandler = std::set_unexpected(&fg_UnexpectedExceptionHandler);
}

extern "C" void fg_Malterlib_CreateSystem()
{
	NSys::fg_CreateSystem();
}

#if defined(DMibLinkTimeCodeGeneration)
extern "C" void __clear_cache(void *start, void *end);
#endif

#if defined(DMibSanitizerEnabled_Address)
module_export assure_used extern "C" int __asan_on_delete(void *ptr, size_t size)
{
	return NMib::NMemory::CCaptureDefaultDelete::fs_ReportDelete(ptr, size);
}
#endif

extern bool g_bSysDeleted;

namespace
{
	// Apple libpthread 454 through 539 keep cancel_state at this
	// pointer-width-dependent distance before the TSD base.
	constexpr umint gc_PThreadCancelStateOffsetFromTSD = 0x2a + 2 * sizeof(void *);
	constexpr uint16 gc_PThreadCancelStateExiting = 0x20;

	DMibSuppressThreadSanitizer inline_never bool fg_PThreadIsExiting(pthread_t _pThread)
	{
		auto pCancelState = reinterpret_cast<uint16 *>
			(
				reinterpret_cast<uint8 *>(_pThread)
					+ NSys::g_ThreadLocalOffsetPThread
					- gc_PThreadCancelStateOffsetFromTSD
			)
		;
	#ifdef DMibSanitizerEnabled_Thread
		// TSan does not model Mach thread suspension and reports this private
		// pthread-state read against its own earlier TLS initialization write.
		uint16 CancelState = *reinterpret_cast<uint16 volatile *>(pCancelState);
	#else
		uint16 CancelState = reinterpret_cast<NAtomic::TCAtomic<uint16> *>(pCancelState)->f_Load(NAtomic::gc_MemoryOrder_Relaxed);
	#endif
		return (CancelState & gc_PThreadCancelStateExiting) != 0;
	}

#ifdef DMibConfig_PThreadIntrospection
	void fg_Thread_EnumOtherThreadsInProcessKernel(NFunction::TCFunctionNoAlloc<void (umint _ThreadID)> const &_fOnThread)
	{
		thread_act_array_t pThreads = nullptr;
		mach_msg_type_number_t ThreadCount = 0;
		if (task_threads(mach_task_self(), &pThreads, &ThreadCount) != KERN_SUCCESS)
			return;

		umint CurrentThread = NSys::fg_Thread_GetCurrentUID();
		for (mach_msg_type_number_t i = 0; i < ThreadCount; ++i)
		{
			mach_port_t MachThread = pThreads[i];
			// Resolve the pthread while the target can still release libpthread's
			// global list lock. Matching MACH_THREAD_SELF also proves TSD setup.
			pthread_t pThread = pthread_from_mach_thread_np(MachThread);
			if (!pThread || (umint)pThread == CurrentThread)
			{
				mach_port_deallocate(mach_task_self(), MachThread);
				continue;
			}

			if (thread_suspend(MachThread) != KERN_SUCCESS)
			{
				mach_port_deallocate(mach_task_self(), MachThread);
				continue;
			}

			auto ResumeThread = g_OnScopeExit / [&]
				{
					thread_resume(MachThread);
					mach_port_deallocate(mach_task_self(), MachThread);
				}
			;

			if (!fg_PThreadIsExiting(pThread))
				_fOnThread((umint)pThread);
		}

		vm_deallocate(mach_task_self(), (vm_address_t)pThreads, ThreadCount * sizeof(*pThreads));
	}
#endif

	void fg_Thread_EnumOtherThreadsInProcessSuspended(NFunction::TCFunctionNoAlloc<void (umint _ThreadID)> const &_fOnThread)
	{
		struct CSuspendedThread
		{
			mach_port_t m_MachThread;
			umint m_ThreadID;
		};

		NContainer::TCSet<mach_port_t, CSort_Default, NMemory::CAllocator_VirtualNoTracking> SeenThreads;
		NContainer::TCVector<CSuspendedThread, NMemory::CAllocator_VirtualNoTracking> SuspendedThreads;
		mach_port_t CurrentMachThread = pthread_mach_thread_np(pthread_self());
		bool bFoundNewThread;
		do
		{
			bFoundNewThread = false;
			thread_act_array_t pThreads = nullptr;
			mach_msg_type_number_t ThreadCount = 0;
			if (task_threads(mach_task_self(), &pThreads, &ThreadCount) != KERN_SUCCESS)
				break;

			auto CleanupThreads = g_OnScopeExit / [&]
				{
					for (mach_msg_type_number_t i = 0; i < ThreadCount; ++i)
					{
						if (pThreads[i] != MACH_PORT_NULL)
							mach_port_deallocate(mach_task_self(), pThreads[i]);
					}

					vm_deallocate(mach_task_self(), (vm_address_t)pThreads, ThreadCount * sizeof(*pThreads));
				}
			;

			for (mach_msg_type_number_t i = 0; i < ThreadCount; ++i)
			{
				mach_port_t MachThread = pThreads[i];
				if (MachThread == CurrentMachThread || SeenThreads.f_Exists(MachThread))
					continue;

				SeenThreads.f_Insert(MachThread);
				bFoundNewThread = true;

				if (thread_suspend(MachThread) != KERN_SUCCESS)
					continue;

				auto ResumeThread = g_OnScopeExit / [&]
					{
						thread_resume(MachThread);
					}
				;

				thread_identifier_info_data_t ThreadInfo = {};
				mach_msg_type_number_t ThreadInfoCount = THREAD_IDENTIFIER_INFO_COUNT;
				if
				(
					thread_info(MachThread, THREAD_IDENTIFIER_INFO, reinterpret_cast<thread_info_t>(&ThreadInfo), &ThreadInfoCount) != KERN_SUCCESS
					|| ThreadInfo.thread_handle < NSys::g_ThreadLocalOffsetPThread
				)
				{
					continue;
				}

				// XNU exposes the TSD base as thread_handle. Deriving pthread_t from
				// it avoids entering libpthread while any target thread is suspended.
				umint ThreadID = (umint)ThreadInfo.thread_handle - NSys::g_ThreadLocalOffsetPThread;
				if (fg_PThreadIsExiting(reinterpret_cast<pthread_t>(ThreadID)))
					continue;

				SuspendedThreads.f_Insert(CSuspendedThread{MachThread, ThreadID});
				pThreads[i] = MACH_PORT_NULL;
				ResumeThread.f_Clear();
			}
		}
		while (bFoundNewThread);

		auto ResumeThreads = g_OnScopeExit / [&]
			{
				for (auto &Thread : SuspendedThreads)
				{
					thread_resume(Thread.m_MachThread);
					mach_port_deallocate(mach_task_self(), Thread.m_MachThread);
				}
			}
		;

		for (auto &Thread : SuspendedThreads)
			_fOnThread(Thread.m_ThreadID);
	}
}

#ifdef DMibConfig_PThreadIntrospection

// Threads not started by Malterlib (dispatch workers, XPC reply threads,
// threads started by frameworks) never pass the Malterlib thread trampoline,
// so the pthread introspection hook delivers the thread create notification
// that always-created thread locals require. The hook is a process global, so
// the executable installs it and distributes the notifications to every
// registered Malterlib module; a shared library registers with the executable
// through the exported functions, or chains the hook itself when the host is
// not a Malterlib executable

namespace
{
	// This module's notifications, invoked on the thread the event concerns
	// except when a module registers and receives the already existing threads
	void fg_MalterlibThreadCreatedNotificationLocal(umint _ThreadID, umint _ParentThreadID)
	{
		if (g_bSysDeleted)
			return;

		fg_SetPThreadNotificationActive(_ThreadID);
		fg_GetLocalSys()->f_OnThreadCreated(_ThreadID, _ParentThreadID);
	}

	void fg_MalterlibThreadTerminatedNotificationLocal(umint _ThreadID)
	{
		fg_GetLocalSys()->f_OnThreadDestroyed();
		fg_SetPThreadNotificationDestroyed();
	}
}

#ifndef DMibDynamicLibrary

namespace
{
	struct CThreadNotificationRegistration
	{
		auto f_GetModule() const -> NSys::NPrivate::CThreadNotificationModule *
		{
			using CNotifications = NContainer::TCMap<NSys::NPrivate::CThreadNotificationModule *, CThreadNotificationRegistration>;
			return CNotifications::fs_GetKey(*this);
		}

		DMibListLinkDS_Link(CThreadNotificationRegistration, m_Link);
	};

	struct CThreadNotificationThread
	{
		__darwin_pthread_handler_rec m_CleanupHandler{};
		bool m_bCleanupInstalled = false;
		bool m_bTerminated = false;
	};

	struct CThreadNotificationState
	{
		using CThreads = NContainer::TCMapWithPool<umint, CThreadNotificationThread, CSort_Default, NMemory::CAllocator_VirtualNoTracking>;
		using CStartingThreads = NContainer::TCSetWithPool<umint, CSort_Default, NMemory::CAllocator_VirtualNoTracking>;
		using CNotifications = NContainer::TCMap<NSys::NPrivate::CThreadNotificationModule *, CThreadNotificationRegistration>;

		CThreads m_LiveThreads;
		CStartingThreads m_StartingThreads;
		CNotifications m_Notifications;
		DMibListLinkDS_List(CThreadNotificationRegistration, m_Link) m_NotificationOrder;
		bool m_bSeeded = false;
	};

	constinit NStorage::TCAggregateSimple<CThreadNotificationState> g_ThreadNotificationState = {DAggregateInit};
	constinit NSys::NPrivate::CThreadNotificationModule g_LocalThreadNotificationModule =
		{
			.m_Version = NSys::NPrivate::EThreadNotificationCrossModule_Version
			, .m_Reserved = {}
			, .m_fCreated = &fg_MalterlibThreadCreatedNotificationLocal
			, .m_fTerminated = &fg_MalterlibThreadTerminatedNotificationLocal
			, .m_fForkPrepare = nullptr
			, .m_fForkParent = nullptr
			, .m_fForkChild = nullptr
		}
	;
	pthread_introspection_hook_t g_fPreviousIntrospectionHook = nullptr;
	constinit umint g_iThreadLocalParentThread = 0;
	constinit NAtomic::TCAtomic<bool> g_bThreadNotificationsForking{false};
	bool g_bThreadNotificationsInitialized = false;

	void fg_NotifyThreadTerminated(CThreadNotificationState &_State, umint _ThreadID, bool _bRestoreThreadLocals)
	{
		auto pThread = _State.m_LiveThreads.f_FindEqual(_ThreadID);
		if (pThread)
		{
			if (pThread->m_bTerminated && !_bRestoreThreadLocals)
				return;
			pThread->m_bTerminated = true;
		}
		else if (!_State.m_StartingThreads.f_Exists(_ThreadID))
			return;

		if (_bRestoreThreadLocals)
			fg_GetLocalSys()->f_ThreadLocalRestoreThread();

		auto iRegistration = _State.m_NotificationOrder.f_GetIterator();
		iRegistration.f_Reverse(_State.m_NotificationOrder);
		for (; iRegistration; --iRegistration)
			iRegistration->f_GetModule()->m_fTerminated(_ThreadID);
	}

	void fg_PThreadCleanup(void *_pThreadID)
	{
		if (!g_bThreadNotificationsInitialized)
			return;

		DMibLock(g_ThreadNotificationLock);
		fg_NotifyThreadTerminated(*g_ThreadNotificationState, (umint)_pThreadID, false);
	}

	void fg_InstallPThreadCleanup(CThreadNotificationThread &_Thread, umint _ThreadID)
	{
		DMibFastCheck(!_Thread.m_bCleanupInstalled);

		pthread_t pThread = pthread_self();
		_Thread.m_CleanupHandler.__routine = &fg_PThreadCleanup;
		_Thread.m_CleanupHandler.__arg = (void *)_ThreadID;
		_Thread.m_CleanupHandler.__next = pThread->__cleanup_stack;
		pThread->__cleanup_stack = &_Thread.m_CleanupHandler;
		_Thread.m_bCleanupInstalled = true;
	}

	void fg_PThreadNotificationDestructor(void *_pValue)
	{
		DMibLock(g_ThreadNotificationLock);
		if (!g_iThreadNotificationDestructor)
			return;

		if ((umint)_pValue != TCLimitsInt<umint>::mc_Max)
		{
			if (g_bThreadNotificationsInitialized)
				fg_NotifyThreadTerminated(*g_ThreadNotificationState, NSys::fg_Thread_GetCurrentUID(), false);
		}

		fg_SetPThreadNotificationDestroyed();
	}

	void fg_PThreadIntrospectionHook(unsigned int _Event, pthread_t _pThread, void *_pAddress, size_t _Size)
	{
		if (g_fPreviousIntrospectionHook)
			g_fPreviousIntrospectionHook(_Event, _pThread, _pAddress, _Size);

		DMibLock(g_ThreadNotificationLock);
		if
		(
			!g_bThreadNotificationsInitialized
			|| !g_iThreadLocalParentThread
			|| g_bThreadNotificationsForking.f_Load(NAtomic::gc_MemoryOrder_Acquire)
		)
		{
			return;
		}

		if (_Event == PTHREAD_INTROSPECTION_THREAD_CREATE)
		{
			// The creating thread stores itself in the new thread's storage
			// before the thread starts, mirroring the Windows implementation
			NSys::fg_Thread_SetLocal((umint)_pThread, g_iThreadLocalParentThread, (void *)NSys::fg_Thread_GetCurrentUID());

			g_ThreadNotificationState->m_StartingThreads.f_Insert((umint)_pThread);
		}
		else if (_Event == PTHREAD_INTROSPECTION_THREAD_START)
		{
			umint ParentThread = (umint)NSys::fg_Thread_GetLocal(g_iThreadLocalParentThread);
			umint CurrentThread = NSys::fg_Thread_GetCurrentUID();

			auto &State = *g_ThreadNotificationState;
			if (State.m_LiveThreads.f_FindEqual(CurrentThread))
				return;

			auto &Thread = State.m_LiveThreads[CurrentThread];
			fg_InstallPThreadCleanup(Thread, CurrentThread);
			for (auto &Registration : State.m_NotificationOrder)
				Registration.f_GetModule()->m_fCreated(CurrentThread, ParentThread);

			State.m_StartingThreads.f_Remove(CurrentThread);
		}
		else if (_Event == PTHREAD_INTROSPECTION_THREAD_TERMINATE)
		{
			// The terminate event arrives before the thread's stack is freed, so
			// holding the lock here keeps registration safe against exiting
			// threads
			umint CurrentThread = NSys::fg_Thread_GetCurrentUID();

			auto &State = *g_ThreadNotificationState;
			fg_NotifyThreadTerminated(State, CurrentThread, true);

			State.m_StartingThreads.f_Remove(CurrentThread);
			State.m_LiveThreads.f_Remove(CurrentThread);
		}
		else if (_Event == PTHREAD_INTROSPECTION_THREAD_DESTROY)
			g_ThreadNotificationState->m_StartingThreads.f_Remove((umint)_pThread);
	}

	void fg_ThreadNotificationsForkPrepare()
	{
		if (!g_bThreadNotificationsInitialized)
			return;

		g_bThreadNotificationsForking.f_Store(true, NAtomic::gc_MemoryOrder_Release);
		g_ThreadNotificationLock.f_Lock();

		auto &State = *g_ThreadNotificationState;
		for (auto &Registration : State.m_NotificationOrder)
		{
			auto pModule = Registration.f_GetModule();
			if (pModule->m_fForkPrepare)
				pModule->m_fForkPrepare();
		}
	}

	void fg_ThreadNotificationsForkParent()
	{
		if (!g_bThreadNotificationsInitialized)
			return;

		auto &State = *g_ThreadNotificationState;
		auto iRegistration = State.m_NotificationOrder.f_GetIterator();
		iRegistration.f_Reverse(State.m_NotificationOrder);
		for (; iRegistration; --iRegistration)
		{
			auto pModule = iRegistration->f_GetModule();
			if (pModule->m_fForkParent)
				pModule->m_fForkParent();
		}

		g_ThreadNotificationLock.f_Unlock();
		g_bThreadNotificationsForking.f_Store(false, NAtomic::gc_MemoryOrder_Release);
	}

	void fg_ThreadNotificationsForkChild()
	{
		if (!g_bThreadNotificationsInitialized)
			return;

		// Only the forking thread survives in the child
		auto &State = *g_ThreadNotificationState;
		auto iRegistration = State.m_NotificationOrder.f_GetIterator();
		iRegistration.f_Reverse(State.m_NotificationOrder);
		for (; iRegistration; --iRegistration)
		{
			auto pModule = iRegistration->f_GetModule();
			if (pModule->m_fForkChild)
				pModule->m_fForkChild();
		}

		umint CurrentThread = NSys::fg_Thread_GetCurrentUID();
		while (true)
		{
			umint ThreadToRemove = 0;
			for (auto &Thread : State.m_LiveThreads)
			{
				umint ThreadID = CThreadNotificationState::CThreads::fs_GetKey(Thread);
				if (ThreadID != CurrentThread)
				{
					ThreadToRemove = ThreadID;
					break;
				}
			}

			if (!ThreadToRemove)
				break;
			State.m_LiveThreads.f_Remove(ThreadToRemove);
		}
		if (!State.m_LiveThreads.f_FindEqual(CurrentThread))
			(void)State.m_LiveThreads[CurrentThread];
		State.m_StartingThreads.f_Clear();

		g_ThreadNotificationLock.f_ForkedChildLocked();
		g_ThreadNotificationLock.f_Unlock();
		g_bThreadNotificationsForking.f_Store(false, NAtomic::gc_MemoryOrder_Release);
	}

	void fg_InstallPThreadIntrospectionHook()
	{
		DMibFastCheck(!g_bThreadNotificationsInitialized);
		g_ThreadNotificationState.f_Construct();

		{
			DMibLock(g_ThreadNotificationLock);

			auto &State = *g_ThreadNotificationState;
			umint CurrentThread = NSys::fg_Thread_GetCurrentUID();
			(void)State.m_LiveThreads[CurrentThread];
			fg_SetPThreadNotificationActive(CurrentThread);
			auto &Registration = State.m_Notifications[&g_LocalThreadNotificationModule];
			State.m_NotificationOrder.f_Insert(Registration);
		}

		g_iThreadLocalParentThread = NSys::fg_Thread_AllocLocal();
		g_bThreadNotificationsInitialized = true;
		g_fPreviousIntrospectionHook = pthread_introspection_hook_install(&fg_PThreadIntrospectionHook);

		{
			DMibLock(g_ThreadNotificationLock);

			auto &State = *g_ThreadNotificationState;
			fg_Thread_EnumOtherThreadsInProcessKernel
				(
					[&](umint _ThreadID)
					{
						if (!State.m_StartingThreads.f_Exists(_ThreadID))
						{
							(void)State.m_LiveThreads[_ThreadID];
							fg_SetPThreadNotificationActive(_ThreadID);
						}
					}
				)
			;
			State.m_bSeeded = true;
		}
	}

	void fg_DestroyPThreadIntrospectionHook()
	{
		DMibLock(g_ThreadNotificationLock);
		if (!g_bThreadNotificationsInitialized)
			return;

		g_bThreadNotificationsInitialized = false;
		umint iThreadLocalParentThread = g_iThreadLocalParentThread;
		g_iThreadLocalParentThread = 0;
		NSys::fg_Thread_FreeLocal(iThreadLocalParentThread);
		g_ThreadNotificationState.f_Destruct();
	}

}

void DMibCrossmoduleAPI fg_ThreadNotificationRegister(NSys::NPrivate::CThreadNotificationModule *_pModule)
{
	if (g_bSysDeleted)
		return;

	DMibFastCheck(_pModule->m_Version >= NSys::NPrivate::EThreadNotificationCrossModule_Version_Min);

	DMibLock(g_ThreadNotificationLock);

	auto &State = *g_ThreadNotificationState;
	auto &Registration = State.m_Notifications[_pModule];
	DMibFastCheck(!Registration.m_Link.f_IsInList());
	State.m_NotificationOrder.f_Insert(Registration);

	// The registering module gets the threads that already exist; the lock
	// keeps them from completing termination while their thread locals are
	// created
	umint CurrentThread = NSys::fg_Thread_GetCurrentUID();
	for (auto &Thread : State.m_LiveThreads)
	{
		umint ThreadID = CThreadNotificationState::CThreads::fs_GetKey(Thread);
		if (!Thread.m_bTerminated && ThreadID != CurrentThread)
			_pModule->m_fCreated(ThreadID, 0);
	}
}

void DMibCrossmoduleAPI fg_ThreadNotificationUnregister(NSys::NPrivate::CThreadNotificationModule *_pModule)
{
	if (g_bSysDeleted)
		return;

	DMibLock(g_ThreadNotificationLock);

	auto &State = *g_ThreadNotificationState;
	auto pRegistration = State.m_Notifications.f_FindEqual(_pModule);
	if (!pRegistration)
		return;

	State.m_Notifications.f_Remove(_pModule);
}

void DMibCrossmoduleAPI fg_ThreadNotificationEnum(NSys::NPrivate::FThreadEnumCallback *_fThread, void *_pContext)
{
	if (g_bSysDeleted)
		return;

	DMibLock(g_ThreadNotificationLock);

	DMibFastCheck(g_ThreadNotificationState->m_bSeeded);
	umint CurrentThread = NSys::fg_Thread_GetCurrentUID();
	for (auto &Thread : g_ThreadNotificationState->m_LiveThreads)
	{
		umint ThreadID = CThreadNotificationState::CThreads::fs_GetKey(Thread);
		if (!Thread.m_bTerminated && ThreadID != CurrentThread)
			_fThread(ThreadID, _pContext);
	}
}

constinit NSys::NPrivate::CThreadNotificationCrossModule g_ThreadNotificationCrossModule =
	{
		.m_Version = NSys::NPrivate::EThreadNotificationCrossModule_Version
		, .m_Reserved = {}
		, .m_fRegister = &fg_ThreadNotificationRegister
		, .m_fUnregister = &fg_ThreadNotificationUnregister
		, .m_fEnum = &fg_ThreadNotificationEnum
	}
;

extern "C" assure_used module_export NSys::NPrivate::CThreadNotificationCrossModule *fg_MalterlibGetThreadNotificationCrossModule(uint32 _Version)
{
	if (_Version < NSys::NPrivate::EThreadNotificationCrossModule_Version_Min)
		return nullptr;

	// Malterlib executable initialization precedes every dependent Malterlib dylib
	// constructor. Returning this interface before the host has constructed its
	// dispatcher would violate the same ordering required by memory interposition.
	DMibFastCheck(g_bThreadNotificationsInitialized);
	return &g_ThreadNotificationCrossModule;
}

#else

namespace
{
	struct COwnThreadNotificationThread
	{
		bool m_bTerminated = false;
	};

	struct COwnThreadNotificationState
	{
		using CThreads = NContainer::TCMapWithPool<umint, COwnThreadNotificationThread, CSort_Default, NMemory::CAllocator_VirtualNoTracking>;
		using CStartingThreads = NContainer::TCSetWithPool<umint, CSort_Default, NMemory::CAllocator_VirtualNoTracking>;

		CThreads m_LiveThreads;
		CStartingThreads m_StartingThreads;
		bool m_bSeeded = false;
	};

	NSys::NPrivate::CThreadNotificationCrossModule *g_pHostThreadNotificationCrossModule = nullptr;
	pthread_introspection_hook_t g_fPreviousIntrospectionHook = nullptr;
	constinit NStorage::TCAggregateSimple<COwnThreadNotificationState> g_OwnThreadNotificationState = {DAggregateInit};
	constinit umint g_iThreadLocalParentThread = 0;
	bool g_bOwnIntrospectionHook = false;

	void fg_NotifyOwnThreadTerminated(COwnThreadNotificationState &_State, umint _ThreadID)
	{
		auto pThread = _State.m_LiveThreads.f_FindEqual(_ThreadID);
		if (pThread)
		{
			if (pThread->m_bTerminated)
				return;
			pThread->m_bTerminated = true;
		}
		else if (!_State.m_StartingThreads.f_Exists(_ThreadID))
			return;

		fg_MalterlibThreadTerminatedNotificationLocal(_ThreadID);
	}

	void fg_PThreadNotificationDestructor(void *_pValue)
	{
		DMibLock(g_OwnThreadNotificationLock);
		if (!g_iThreadNotificationDestructor)
			return;

		if ((umint)_pValue != TCLimitsInt<umint>::mc_Max)
		{
			if (g_pHostThreadNotificationCrossModule)
				fg_MalterlibThreadTerminatedNotificationLocal(NSys::fg_Thread_GetCurrentUID());
			else if (g_bOwnIntrospectionHook)
				fg_NotifyOwnThreadTerminated(*g_OwnThreadNotificationState, NSys::fg_Thread_GetCurrentUID());
		}

		fg_SetPThreadNotificationDestroyed();
	}

	void fg_PThreadIntrospectionHook(unsigned int _Event, pthread_t _pThread, void *_pAddress, size_t _Size)
	{
		if (g_fPreviousIntrospectionHook)
			g_fPreviousIntrospectionHook(_Event, _pThread, _pAddress, _Size);

		DMibLock(g_OwnThreadNotificationLock);
		if (!g_bOwnIntrospectionHook || !g_iThreadLocalParentThread)
			return;

		if (_Event == PTHREAD_INTROSPECTION_THREAD_CREATE)
		{
			NSys::fg_Thread_SetLocal((umint)_pThread, g_iThreadLocalParentThread, (void *)NSys::fg_Thread_GetCurrentUID());

			g_OwnThreadNotificationState->m_StartingThreads.f_Insert((umint)_pThread);
		}
		else if (_Event == PTHREAD_INTROSPECTION_THREAD_START)
		{
			umint ParentThread = (umint)NSys::fg_Thread_GetLocal(g_iThreadLocalParentThread);
			umint CurrentThread = NSys::fg_Thread_GetCurrentUID();

			auto &State = *g_OwnThreadNotificationState;
			if (State.m_LiveThreads.f_FindEqual(CurrentThread))
				return;

			// Initialize this module's always-created thread locals before using
			// the heap-backed notification registry on the new thread
			fg_MalterlibThreadCreatedNotificationLocal(CurrentThread, ParentThread);

			State.m_StartingThreads.f_Remove(CurrentThread);
			(void)State.m_LiveThreads[CurrentThread];
		}
		else if (_Event == PTHREAD_INTROSPECTION_THREAD_TERMINATE)
		{
			umint CurrentThread = NSys::fg_Thread_GetCurrentUID();

			auto &State = *g_OwnThreadNotificationState;
			fg_NotifyOwnThreadTerminated(State, CurrentThread);

			State.m_StartingThreads.f_Remove(CurrentThread);
			State.m_LiveThreads.f_Remove(CurrentThread);
		}
		else if (_Event == PTHREAD_INTROSPECTION_THREAD_DESTROY)
			g_OwnThreadNotificationState->m_StartingThreads.f_Remove((umint)_pThread);
	}

	void fg_MalterlibThreadForkPrepareLocal()
	{
		g_OwnThreadNotificationLock.f_Lock();
		fg_GetLocalSys()->f_ThreadLocal_PrepareFork();
	}

	void fg_MalterlibThreadForkParentLocal()
	{
		fg_GetLocalSys()->f_ThreadLocal_ForkedParent();
		g_OwnThreadNotificationLock.f_Unlock();
	}

	void fg_MalterlibThreadForkChildLocal()
	{
		fg_GetLocalSys()->f_ThreadLocal_ForkedChild();
		g_OwnThreadNotificationLock.f_ForkedChildLocked();
		g_OwnThreadNotificationLock.f_Unlock();
	}

	constinit NSys::NPrivate::CThreadNotificationModule g_ThreadNotificationModule =
		{
			.m_Version = NSys::NPrivate::EThreadNotificationCrossModule_Version
			, .m_Reserved = {}
			, .m_fCreated = &fg_MalterlibThreadCreatedNotificationLocal
			, .m_fTerminated = &fg_MalterlibThreadTerminatedNotificationLocal
			, .m_fForkPrepare = &fg_MalterlibThreadForkPrepareLocal
			, .m_fForkParent = &fg_MalterlibThreadForkParentLocal
			, .m_fForkChild = &fg_MalterlibThreadForkChildLocal
		}
	;

	void fg_RegisterThreadNotifications()
	{
		fg_SetPThreadNotificationActive(NSys::fg_Thread_GetCurrentUID());

		auto fGetInterface = (NSys::NPrivate::FGetThreadNotificationCrossModule *)dlsym(RTLD_DEFAULT, "fg_MalterlibGetThreadNotificationCrossModule");
		auto pInterface = fGetInterface ? fGetInterface(NSys::NPrivate::EThreadNotificationCrossModule_Version) : nullptr;

#ifdef DMibAssumeMalterlibHost
		DMibFastCheck(!pInterface || pInterface->m_Version >= NSys::NPrivate::EThreadNotificationCrossModule_Version_Min);
#endif

		if (pInterface && pInterface->m_Version >= NSys::NPrivate::EThreadNotificationCrossModule_Version_Min)
		{
			g_pHostThreadNotificationCrossModule = pInterface;
			pInterface->m_fRegister(&g_ThreadNotificationModule);
			return;
		}

		// The host is not a Malterlib executable, so this library chains the
		// process global hook itself and maintains its own thread registry
		g_OwnThreadNotificationState.f_Construct();
		(void)g_OwnThreadNotificationState->m_LiveThreads[NSys::fg_Thread_GetCurrentUID()];
		g_bOwnIntrospectionHook = true;
		g_iThreadLocalParentThread = NSys::fg_Thread_AllocLocal();
		g_fPreviousIntrospectionHook = pthread_introspection_hook_install(&fg_PThreadIntrospectionHook);

		{
			DMibLock(g_OwnThreadNotificationLock);

			auto &State = *g_OwnThreadNotificationState;
			fg_Thread_EnumOtherThreadsInProcessKernel
				(
					[&](umint _ThreadID)
					{
						if (!State.m_StartingThreads.f_Exists(_ThreadID))
						{
							(void)State.m_LiveThreads[_ThreadID];
							fg_SetPThreadNotificationActive(_ThreadID);
						}
					}
				)
			;
			State.m_bSeeded = true;
		}
	}

	void fg_UnregisterThreadNotifications()
	{
		if (g_pHostThreadNotificationCrossModule)
		{
			g_pHostThreadNotificationCrossModule->m_fUnregister(&g_ThreadNotificationModule);
			g_pHostThreadNotificationCrossModule = nullptr;
		}
	}

	void fg_DestroyOwnPThreadIntrospectionHook()
	{
		DMibLock(g_OwnThreadNotificationLock);
		if (!g_bOwnIntrospectionHook)
			return;

		g_bOwnIntrospectionHook = false;
		umint iThreadLocalParentThread = g_iThreadLocalParentThread;
		g_iThreadLocalParentThread = 0;

		// A Malterlib executable hosts the cross-module dispatcher, so a library
		// reaches this private-hook path only in a non-Malterlib process. That host
		// must have no other introspection-hook user, or be the sole owner itself.
		// pthread provides no safe conditional restore, so composing this path with
		// another hook owner or unloading it while another owner exists is unsupported.
		[[maybe_unused]] auto fCurrentHook = pthread_introspection_hook_install(g_fPreviousIntrospectionHook);
		DMibFastCheck(fCurrentHook == &fg_PThreadIntrospectionHook);
		NSys::fg_Thread_FreeLocal(iThreadLocalParentThread);
		g_OwnThreadNotificationState.f_Destruct();
	}
}

#endif

#endif

void NSys::fg_Thread_EnumOtherThreadsInProcess(NFunction::TCFunctionNoAlloc<void (umint _ThreadID)> const &_fOnThread)
{
#if defined(DMibConfig_PThreadIntrospection) && !defined(DMibDynamicLibrary)
	fg_ThreadNotificationEnum
		(
			[](umint _ThreadID, void *_pContext)
			{
				(*reinterpret_cast<NFunction::TCFunctionNoAlloc<void (umint _ThreadID)> const *>(_pContext))(_ThreadID);
			}
			, const_cast<void *>(reinterpret_cast<void const *>(&_fOnThread))
		)
	;
	return;
#elif defined(DMibConfig_PThreadIntrospection) && defined(DMibDynamicLibrary)
	if (g_pHostThreadNotificationCrossModule)
	{
		g_pHostThreadNotificationCrossModule->m_fEnum
			(
				[](umint _ThreadID, void *_pContext)
				{
					(*reinterpret_cast<NFunction::TCFunctionNoAlloc<void (umint _ThreadID)> const *>(_pContext))(_ThreadID);
				}
				, const_cast<void *>(reinterpret_cast<void const *>(&_fOnThread))
			)
		;
		return;
	}

	if (g_bOwnIntrospectionHook)
	{
		DMibLock(g_OwnThreadNotificationLock);

		DMibFastCheck(g_OwnThreadNotificationState->m_bSeeded);
		umint CurrentThread = NSys::fg_Thread_GetCurrentUID();
		for (auto &Thread : g_OwnThreadNotificationState->m_LiveThreads)
		{
			umint ThreadID = COwnThreadNotificationState::CThreads::fs_GetKey(Thread);
			if (!Thread.m_bTerminated && ThreadID != CurrentThread)
				_fOnThread(ThreadID);
		}
		return;
	}
#endif

	fg_Thread_EnumOtherThreadsInProcessSuspended(_fOnThread);
}

#ifdef DMibConfig_PThreadIntrospection
void fg_InitMalterlibAllEnumOtherThreads()
{
	umint ThisUID = NSys::fg_Thread_GetCurrentUID();
	NSys::fg_Thread_EnumOtherThreadsInProcess
		(
			[&](umint _ThreadID)
			{
				fg_GetLocalSys()->f_ThreadLocalCreateThread(_ThreadID, ThisUID);
			}
		)
	;
}
#endif

void NSys::fg_CreateSystem()
{
	if (g_bCreatedSystem)
		return;

	g_bCreatedSystem = true;

#if defined(DMibLinkTimeCodeGeneration)
	int Temp = 0;
	__clear_cache(&Temp, &Temp);
#endif

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
			pthread_atfork(&fg_MalterlibSystem_ForkPrepare, &fg_MalterlibSystem_ForkParent, &fg_MalterlibSystem_ForkChildFinished);
		}
		signal(SIGHUP,SIG_IGN);
	}

	//atexit(&fg_DestroySystemAtExit);

	// fg_MalterlibMallocOverrideInit_ReinstallHandler(); Breakpad does not use signal handlers on macOS, so we don't need to install handlers here

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
						NSys::fg_ConsoleOutput((CFStr256::CFormat("{nfh,sj16,sf0}") << (umint)getpid()).f_GetStr().f_Span());
					else if (fg_StrCmp(pArgs[i], "--OutputStdErrToStdOut") == 0)
						dup2(1, 2);
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

#ifdef DMibConfig_PThreadIntrospection
	umint ThisUID = NSys::fg_Thread_GetCurrentUID();
	pSystem->f_ThreadLocalCreateThread(ThisUID, 0);

	#ifndef DMibDynamicLibrary
		fg_InstallPThreadIntrospectionHook();
	#else
		fg_RegisterThreadNotifications();
	#endif
	fg_InitMalterlibAllEnumOtherThreads();
#endif

	fg_InitBreakpad();

	pSystem->f_InitModuleThreaded();

	setlinebuf(stdout); // Default to line buffered output
	setlinebuf(stderr); // Default to line buffered output
}

bool g_bSysDeleted = false;

void NSys::fg_PreDestroyHeap()
{
#ifdef DMibConfig_PThreadIntrospection
	#ifndef DMibDynamicLibrary
		fg_DestroyPThreadIntrospectionHook();
	#else
		fg_DestroyOwnPThreadIntrospectionHook();
	#endif
#endif
}

void NSys::fg_DestroySystem()
{
	if (g_bCreatedSystem && !g_bSysDeleted)
	{
		g_bSysDeleted = true;

		auto pSys = fg_GetLocalSys();
		pSys->f_DestroyThreadSpecific();
		// f_DestroyThreadSpecific() stops and joins every thread before lifecycle
		// notification state is torn down, so no introspection callback can race the code below.

		pSys->f_ExitModule();

#if defined(DMibConfig_PThreadIntrospection) && defined(DMibDynamicLibrary)
		// Keep termination coordinated while module aggregates release their
		// thread locals, then unregister before the context and module disappear
		fg_UnregisterThreadNotifications();
#endif

		// We need to flush these before the buffer memory is deleted
		fflush(stdout);
		fflush(stderr);

		fg_DestroyBreakpad();

		pSys->f_Destruct();
		if (pSys->m_bForkedChild)
			return; // Forked children have several problems with invalid semaphores etc, so lets just not destroy anything here
		pSys->~CSystemMacOS();

#ifdef DMibConfig_PThreadIntrospection
		fg_FreePThreadNotificationDestructor();
#endif

		g_VirtualMap.f_Destruct();
		g_VirtualMapLock.f_Destruct();
	}
}

namespace NMib
{
	namespace NSys
	{
		void fg_MalterlibSystem_ForkPrepare()
		{
		#if defined(DMibConfig_PThreadIntrospection) && !defined(DMibDynamicLibrary)
			fg_ThreadNotificationsForkPrepare();
		#endif
			CSystemMacOS::fs_ForkPrepare();
		}
		void fg_MalterlibSystem_ForkParent()
		{
			CSystemMacOS::fs_ForkParent();
		#if defined(DMibConfig_PThreadIntrospection) && !defined(DMibDynamicLibrary)
			fg_ThreadNotificationsForkParent();
		#endif
		}
		void fg_MalterlibSystem_ForkChildOverride()
		{
			CSystemMacOS::fs_ForkChildStart();
		}
		void fg_MalterlibSystem_ForkChildOverrideFinished()
		{
			#if defined(DMibConfig_PThreadIntrospection) && !defined(DMibDynamicLibrary)
			fg_ThreadNotificationsForkChild();
			#endif
			CSystemMacOS::fs_ForkChildFinished();
			g_bForking = false;
		}
		void fg_MalterlibSystem_ForkChild()
		{
			CSystemMacOS::fs_ForkChildStart();
		#if defined(DMibConfig_PThreadIntrospection) && !defined(DMibDynamicLibrary)
			fg_ThreadNotificationsForkChild();
		#endif
			CSystemMacOS::fs_ForkChildFinished();
		}

		void fg_MalterlibSystem_ForkChildFinished()
		{
			CSystemMacOS::fs_ForkChildStart();
		#if defined(DMibConfig_PThreadIntrospection) && !defined(DMibDynamicLibrary)
			fg_ThreadNotificationsForkChild();
		#endif
			CSystemMacOS::fs_ForkChildFinished();

			g_bForking = false;
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
	sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);

	// We're being debugged if the P_TRACED flag is set.

	return ( (info.kp_proc.p_flag & P_TRACED) != 0 );
}

////file handling

namespace NMib
{

	namespace NSys
	{

		NMib::NStr::CStr fg_MacOS_GetApplicationSupportDirectory();
		NMib::NStr::CStr fg_MacOS_GetCachesDirectory();
        NMib::NStr::CStr fg_MacOS_GetUserHomeDirectory();
		NMib::NStr::CStr fg_MacOS_GetLogDirectory();

		NMib::NStr::CStrNonTracked fg_MacOS_GetApplicationSupportDirectoryNonTracked();
		NMib::NStr::CStrNonTracked fg_MacOS_GetCachesDirectoryNonTracked();
        NMib::NStr::CStrNonTracked fg_MacOS_GetUserHomeDirectoryNonTracked();
		NMib::NStr::CStrNonTracked fg_MacOS_GetLogDirectoryNonTracked();

		NMib::NStr::CStr fg_MacOS_GetSystemLanguage();

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
	return NMib::NFile::CFile::fs_AppendPath(NMib::NSys::fg_MacOS_GetApplicationSupportDirectory(), fg_GetProgramUserName());
}

NMib::NStr::CStr NSys::NFile::fg_GetUserLocalProgramCacheDirectory()
{
	return NMib::NFile::CFile::fs_AppendPath(NMib::NSys::fg_MacOS_GetCachesDirectory(), fg_GetProgramUserName());
}

NMib::NStr::CStrNonTracked NSys::NFile::fg_GetUserLocalProgramDirectoryNonTracked()
{
	return NMib::NFile::CFile::fs_AppendPath(NMib::NSys::fg_MacOS_GetApplicationSupportDirectoryNonTracked(), fg_GetProgramUserNameNonTracked());
}

NMib::NStr::CStrNonTracked NSys::NFile::fg_GetUserLocalProgramCacheDirectoryNonTracked()
{
	return NMib::NFile::CFile::fs_AppendPath(NMib::NSys::fg_MacOS_GetCachesDirectoryNonTracked(), fg_GetProgramUserNameNonTracked());
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
		umint Len = TmpDir.f_GetLen();
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
		umint Len = TmpDir.f_GetLen();
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

CStr NSys::NFile::fg_GetProgramPathForExecutableContents()
{
	return fg_GetProgramPath();
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
    return fg_MacOS_GetUserHomeDirectory();
}

NMib::NStr::CStrNonTracked NSys::NFile::fg_GetUserHomeDirectoryNonTracked()
{
    return fg_MacOS_GetUserHomeDirectoryNonTracked();
}


NMib::NStr::CStr NSys::NFile::fg_GetLogDirectory()
{
	return NMib::NFile::CFile::fs_AppendPath(fg_MacOS_GetLogDirectory(), fg_GetProgramUserName());
}


NMib::NStr::CStrNonTracked NSys::NFile::fg_GetLogDirectoryNonTracked()
{
	return NMib::NFile::CFile::fs_AppendPath(fg_MacOS_GetLogDirectoryNonTracked(), fg_GetProgramUserNameNonTracked());
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

bool NSys::NFile::fg_TryCloneData(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo)
{
	if (!&fclonefileat)
		return false;

	// Shared advisory lock, same as a CFile read open with sharing — see the Linux clone path
	int LockFile = open(_FileFrom, O_RDONLY | O_CLOEXEC);
	if (LockFile < 0)
		return false;

	auto CloseLockFile = g_OnScopeExit / [&]
		{
			close(LockFile);
		}
	;

	if (flock(LockFile, LOCK_SH | LOCK_NB) != 0)
		return false;

	// The guards run on the very inode that stays open and locked, and the clone below runs
	// from the same descriptor: validating one inode and then cloning the path again would let
	// a concurrent atomic replace swap in an unvalidated, unlocked source — a compressed
	// replacement would then be stripped into an empty destination
	{
		struct stat SourceStat;
		if (fstat(LockFile, &SourceStat) != 0)
			return false;

		// clonefile would happily clone a whole directory hierarchy; this function promises a
		// data-only clone of one regular file, exactly like the Linux reflink path
		if (!S_ISREG(SourceStat.st_mode))
			return false;

		// A filesystem-compressed source keeps its logical bytes in the decmpfs attribute and
		// the resource fork behind UF_COMPRESSED — the metadata strip below would delete the
		// data itself (verified: the strip turns the clone into an empty file). Such sources
		// must materialize through the read/write fallback
		if (SourceStat.st_flags & UF_COMPRESSED)
			return false;
	}

	// clonefile clones the whole inode — mode, special bits, file flags and extended attributes
	// included — while data-only semantics promise the metadata of a freshly created file. The
	// probe learns what mode a fresh file at the destination gets: the umask applies inside the
	// kernel, so it is never read or temporarily cleared, which would race other threads
	// creating files.
	//
	// The probe is at the destination path itself, deliberately: default modes can depend on the
	// containing directory (default ACLs), so only the real path answers correctly. A concurrent
	// writer creating the same destination path mid-probe is outside the contract — the copy as
	// a whole overwrites that path, exactly as the fallback write would stomp the same file
	mode_t FreshMode;
	acl_t ProbeAcl = nullptr;
	auto FreeProbeAcl = g_OnScopeExit / [&]
		{
			if (ProbeAcl)
				acl_free(ProbeAcl);
		}
	;
	{
		int ProbeFile = open(_FileTo, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
		if (ProbeFile < 0)
			return false;

		struct stat ProbeStat;
		bool bProbeStated = fstat(ProbeFile, &ProbeStat) == 0;

		// The probe also answers what ACL a fresh file inherits from the destination directory;
		// null when it inherits none, which is the common case
		ProbeAcl = acl_get_fd_np(ProbeFile, ACL_TYPE_EXTENDED);

		close(ProbeFile);
		unlink(_FileTo);
		if (!bProbeStated)
			return false;

		FreshMode = ProbeStat.st_mode & 07777;
	}

	// The clone is materialized and sanitized inside a private 0700 staging directory next to the
	// destination and only renamed into place once it carries fresh-file metadata: clonefile
	// publishes the source's mode, flags, extended attributes and ACL, and sanitizing at the
	// final path would expose the source's metadata — and content, where the source mode is
	// wider than the fresh mode — to any observer of the destination directory for the whole
	// strip window. The staging directory denies every other account access to the clone until
	// it looks exactly like a freshly created file.
	//
	// The directory name carries a random ID — the same scheme as the diff copy's temporary
	// file, but the cryptographically secure generator, because the name is a security
	// boundary here: a predictable name could be pre-created, even as a symlink, by anyone
	// able to write the destination directory, and any recovery that removes such a
	// pre-existing path would delete attacker-chosen targets with this caller's privileges.
	// Nothing pre-existing is ever reused or cleaned for the same reason: a collision simply
	// fails over to the copy fallback, and a leftover from a crashed copy stays behind, bounded
	// by the crash and owned 0700 by the copying user.
	CStr StagingDir = CStr::CFormat("{}.{}.clonetmp") << _FileTo << NCryptography::fg_RandomID();
	if (mkdir(StagingDir, 0700) != 0)
		return false;

	// The path is trusted for exactly one open: in a destination directory writable by another
	// principal the staging directory can be renamed away and substituted the moment it
	// appears, so everything after the mkdir runs relative to this descriptor, with the inode
	// verified below as one this process created — a substitute cannot carry this user's
	// ownership. O_NOFOLLOW refuses a planted symlink outright
	int StagingDirFd = open(StagingDir, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	if (StagingDirFd < 0)
	{
		rmdir(StagingDir);
		return false;
	}

	auto CleanupStaging = g_OnScopeExit / [&]
		{
			// Descriptor-relative, so the entry removed is the staged file whatever happened
			// to the path; after a successful rename it is already gone and the unlinkat
			// misses harmlessly. The rmdir is by path and best effort: a directory renamed
			// away by an attacker leaks bounded to the crash case, and removing a substitute
			// only succeeds when it is empty
			unlinkat(StagingDirFd, "File", 0);
			close(StagingDirFd);
			rmdir(StagingDir);
		}
	;

	{
		struct stat StagingStat;
		if (fstat(StagingDirFd, &StagingStat) != 0 || StagingStat.st_uid != geteuid())
			return false;
	}

	// The 0700 mode alone is not the whole privacy boundary: an inheritable directory ACE on
	// the destination parent rides onto the fresh staging directory and can grant another
	// principal access regardless of the mode bits. The empty set expresses removal, exactly
	// as it does for the staged file's own ACL below
	{
		acl_t EmptyAcl = acl_init(1);
		if (!EmptyAcl)
			return false;

		int AclReturn = acl_set_fd_np(StagingDirFd, EmptyAcl, ACL_TYPE_EXTENDED);
		acl_free(EmptyAcl);
		if (AclReturn != 0)
			return false;
	}

	// The clone runs from the source descriptor the guards and the lock above validated into
	// the verified staging directory, never through full paths again. CLONE_NOOWNERCOPY: a
	// privileged caller would otherwise clone the source's uid and gid, where the fresh-file
	// contract promises ownership as if the caller had created the file
	if (fclonefileat(LockFile, StagingDirFd, "File", CLONE_NOOWNERCOPY))
		return false;

	// Nothing else can reach inside the verified 0700 directory, and O_NOFOLLOW refuses
	// anything that is not the plain file the clone just created; every strip below runs on
	// this descriptor
	int StagedFileFd = openat(StagingDirFd, "File", O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
	if (StagedFileFd < 0)
		return false;

	auto CloseStagedFile = g_OnScopeExit / [&]
		{
			close(StagedFileFd);
		}
	;

	// Flags first: an inherited immutable flag would block the strip below and the cleanup unlink
	if (fchflags(StagedFileFd, 0) != 0)
		return false;

	if (fchmod(StagedFileFd, FreshMode) != 0)
		return false;

	// Extended attributes carry quarantine state, resource forks and custom metadata the
	// fallback's fresh file would not have
	ssize_t NamesLen = flistxattr(StagedFileFd, nullptr, 0, 0);
	if (NamesLen < 0)
		return false;

	if (NamesLen > 0)
	{
		NContainer::TCVector<ch8> Names;
		Names.f_SetLen((umint)NamesLen + 1);
		NamesLen = flistxattr(StagedFileFd, Names.f_GetArray(), (size_t)NamesLen, 0);
		if (NamesLen < 0)
			return false;

		umint iName = 0;
		while (iName < (umint)NamesLen)
		{
			ch8 const *pName = Names.f_GetArray() + iName;
			if (fremovexattr(StagedFileFd, pName, 0) != 0 && errno != ENOATTR)
				return false;

			iName += strlen(pName) + 1;
		}
	}

	// ACLs ride the clone but live outside listxattr's view, so the strip above cannot see them.
	// A fresh file carries exactly the ACL the probe inherited from the destination directory —
	// none in the common case, which the empty set expresses as removal
	{
		acl_t FreshAcl = ProbeAcl ? ProbeAcl : acl_init(1);
		if (!FreshAcl)
			return false;

		int AclReturn = acl_set_fd_np(StagedFileFd, FreshAcl, ACL_TYPE_EXTENDED);
		if (!ProbeAcl)
			acl_free(FreshAcl);
		if (AclReturn != 0)
			return false;
	}

	// clonefile also clones the timestamps; a freshly created file is born now and has been
	// touched now, so both the creation time and the access/modification pair are reset. The
	// caller stamping its own write time afterwards lands on top of this, the same as it does
	// on a file the fallback wrote
	{
		struct timespec Now;
		if (clock_gettime(CLOCK_REALTIME, &Now) != 0)
			return false;

		struct attrlist AttrList;
		fg_MemClear(&AttrList, sizeof(AttrList));
		AttrList.bitmapcount = ATTR_BIT_MAP_COUNT;
		AttrList.commonattr = ATTR_CMN_CRTIME;

		struct timespec CreationTime = Now;
		if (fsetattrlist(StagedFileFd, &AttrList, &CreationTime, sizeof(CreationTime), 0) != 0)
			return false;

		struct timespec Times[2] = {Now, Now};
		if (futimens(StagedFileFd, Times) != 0)
			return false;
	}

	// Atomic publication out of the verified directory; a concurrent writer creating the same
	// destination path mid-copy is outside the contract, exactly as documented at the probe
	if (renameat(StagingDirFd, "File", AT_FDCWD, _FileTo) != 0)
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

void *NSys::NFile::fg_ChangeNotification_Open(const CStr &_FileName, NMib::NFile::EFileChange _OpenFlags, NMib::NThread::CSemaphoreAggregate *_pReportTo)
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

#include "Malterlib_Core_PlatformImp_MacOS_Net.imp.h"

NSys::NNetwork::CAddress NSys::NNetwork::fg_CreateAddress(::NMib::NNetwork::ENetAddressType _Type, void const* _pData, umint _nDataBytes)
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

bool NSys::NNetwork::fg_GetAddressRaw(NSys::NNetwork::CAddress _Address, ::NMib::NNetwork::ENetAddressType _ExpectedType, void* _opRawData, umint _nDataBytes)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_GetAddressRaw(*(CPOSIXAddress*)_Address, _ExpectedType, _opRawData, _nDataBytes);
}

NSys::NNetwork::CAddress NSys::NNetwork::fg_SetAddressRaw(NSys::NNetwork::CAddress _Address, ::NMib::NNetwork::ENetAddressType _Type, void const* _pRawData, umint _nDataBytes)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return (NSys::NNetwork::CAddress)fg_GetLocalSys()->m_SocketContext->f_SetAddressRaw((CPOSIXAddress*)_Address, _Type, _pRawData, _nDataBytes);
}

NSys::NNetwork::CAddress NSys::NNetwork::fg_ResolveAddress(const NMib::NStr::CStr &_Address, ::NMib::NNetwork::ENetAddressType _PreferType)
{
	return fg_GetLocalSys()->m_SocketContext->f_ResolveAddress(_Address, _PreferType);
}

umint NSys::NNetwork::fg_GetMaxUnixSocketNameLength()
{
	return CUnixAddress::mc_MaxAddressLength;
}

void *NSys::NNetwork::fg_AsyncResolveAddress_Open(const NMib::NStr::CStr &_Address, ::NMib::NNetwork::ENetAddressType _PreferType, NMib::NFunction::TCFunctionMutable<void ()> &&_fOnFinish)
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

NMib::NStr::CStr NSys::NNetwork::fg_GetAddressString(NSys::NNetwork::CAddress _Address, ENetAddressStringFlag _Flags)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_GetAddressString(*(CPOSIXAddress*)_Address, _Flags);
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

umint NSys::NNetwork::fg_Receive(void *_pSocket, void *_pData, umint _DataLen, bool &o_bEndOfStream) // Returns bytes receive
{
	return fg_GetLocalSys()->m_SocketContext->f_Receive((CPOSIXSocket*)_pSocket, _pData, _DataLen, o_bEndOfStream);
}

umint NSys::NNetwork::fg_Send(void *_pSocket, const void *_pData, umint _DataLen) // Returns bytes sen
{
	return fg_GetLocalSys()->m_SocketContext->f_Send((CPOSIXSocket*)_pSocket, _pData, _DataLen);
}

umint NSys::NNetwork::fg_SendVectored(void *_pSocket, NSys::CIoSpan const *_pSpans, umint _nSpans)
{
	return fg_GetLocalSys()->m_SocketContext->f_SendVectored((CPOSIXSocket*)_pSocket, _pSpans, _nSpans);
}

umint NSys::NNetwork::fg_SendDatagram(void *_pSocket, NSys::NNetwork::CAddress _Address, const void *_pData, umint _DataLen) // Returns bytes sen
{
	return fg_GetLocalSys()->m_SocketContext->f_SendDatagram((CPOSIXSocket*)_pSocket, *((CPOSIXAddress*)_Address), _pData, _DataLen);
}

umint NSys::NNetwork::fg_ReceiveDatagram(void *_pSocket, NSys::NNetwork::CAddress _Address, void *_pData, umint _DataLen) // Returns bytes sen
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

bool NSys::NNetwork::fg_GetProcessIdentity(void *_pSocket, CProcessIdentity &o_LocalIdentity, CProcessIdentity &o_PeerIdentity)
{
	return fg_GetLocalSys()->m_SocketContext->f_GetProcessIdentity((CPOSIXSocket*)_pSocket, o_LocalIdentity, o_PeerIdentity);
}

bool NSys::NNetwork::fg_HasUnixSocketPeerProcessIdentity()
{
	return true;
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
	// CLowLevelLockAggregate keeps per-platform implementations on purpose:
	// macOS os_unfair_lock donates priority to the owner, Linux uses a
	// priority-inheritance futex, Windows WaitOnAddress has neither. A generic
	// futex implementation would lose the priority handling.
	static_assert(sizeof(os_unfair_lock) == sizeof(CLowLevelLockAggregate::m_Lock));
	static_assert(os_unfair_lock(OS_UNFAIR_LOCK_INIT)._os_unfair_lock_opaque == 0);

	void CLowLevelLockAggregate::f_ForkedChildUnlocked()
	{
		m_Lock = 0;

#if DMibEnableSafeCheck > 0
		++m_nForked;
		m_ThreadID = 0;
		m_AlternateThreadID = 0;
#endif
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

#if DMibEnableSafeCheck > 0
		++m_nForked;
		m_ThreadID = NSys::fg_Thread_GetCurrentUID();
		m_AlternateThreadID = NSys::fg_Thread_GetCurrentUIDAlternate();
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

	bool CLowLevelLockAggregate::f_TryLock()
	{
		DMibSanitizerAnnotate_MutexPreLock(this, __tsan_mutex_write_reentrant | __tsan_mutex_try_lock);

#if DPlatformVersion < 10120
		if (&os_unfair_lock_lock)
		{
#endif
			if (!os_unfair_lock_trylock((os_unfair_lock_t)&m_Lock))
			{
				DMibSanitizerAnnotate_MutexPostLock(this, __tsan_mutex_write_reentrant | __tsan_mutex_try_lock | __tsan_mutex_try_lock_failed, 1);
				return false;
			}
#if DPlatformVersion < 10120
		}
		else
		{
			uint32 Expected = 0;
			if (!m_Lock.f_CompareExchangeStrong(Expected, 1, NAtomic::gc_MemoryOrder_Acquire, NAtomic::gc_MemoryOrder_Acquire))
			{
				DMibSanitizerAnnotate_MutexPostLock(this, __tsan_mutex_write_reentrant | __tsan_mutex_try_lock | __tsan_mutex_try_lock_failed, 1);
				return false;
			}
		}
#endif

#if DMibEnableSafeCheck > 0
		m_ThreadID = NSys::fg_Thread_GetCurrentUID();
		m_AlternateThreadID = NSys::fg_Thread_GetCurrentUIDAlternate();
#endif
		DMibSanitizerAnnotate_MutexPostLock(this, __tsan_mutex_write_reentrant | __tsan_mutex_try_lock, 1);
		return true;
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
			NMib::NThread::CThreadSpinWaiter SpinWaiter;
			uint32 Expected = 0;
			while (!m_Lock.f_CompareExchangeStrong(Expected, 1, NAtomic::gc_MemoryOrder_Acquire, NAtomic::gc_MemoryOrder_Acquire))
			{
				SpinWaiter.f_Wait();
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
			m_Lock.f_Exchange(0, NAtomic::gc_MemoryOrder_Release);
#endif

		DMibSanitizerAnnotate_MutexPostUnlock(this, 0);
	}

	bool CLowLevelLockAggregate::f_TryLockNoSanitize()
	{
#if DPlatformVersion < 10120
		if (&os_unfair_lock_lock)
		{
#endif
			if (!os_unfair_lock_trylock((os_unfair_lock_t)&m_Lock))
				return false;
#if DPlatformVersion < 10120
		}
		else
		{
			uint32 Expected = 0;
			if (!m_Lock.f_CompareExchangeStrong(Expected, 1, NAtomic::gc_MemoryOrder_Acquire, NAtomic::gc_MemoryOrder_Acquire))
				return false;
		}
#endif
#ifdef DMibSanitizerEnabled_Thread
		__tsan_acquire(&m_Lock);
#endif
#if DMibEnableSafeCheck > 0
		m_ThreadID = NSys::fg_Thread_GetCurrentUID();
		m_AlternateThreadID = NSys::fg_Thread_GetCurrentUIDAlternate();
#endif
		return true;
	}

	void CLowLevelLockAggregate::f_LockNoSanitize()
	{
#if DPlatformVersion < 10120
		if (&os_unfair_lock_lock)
		{
#endif
			os_unfair_lock_lock((os_unfair_lock_t)&m_Lock);
#if DPlatformVersion < 10120
		}
		else
		{
			NMib::NThread::CThreadSpinWaiter SpinWaiter;
			uint32 Expected = 0;
			while (!m_Lock.f_CompareExchangeStrong(Expected, 1, NAtomic::gc_MemoryOrder_Acquire, NAtomic::gc_MemoryOrder_Acquire))
			{
				SpinWaiter.f_Wait();
				Expected = 0;
			}
		}
#endif
#ifdef DMibSanitizerEnabled_Thread
		__tsan_acquire(&m_Lock);
#endif
#if DMibEnableSafeCheck > 0
		m_ThreadID = NSys::fg_Thread_GetCurrentUID();
		m_AlternateThreadID = NSys::fg_Thread_GetCurrentUIDAlternate();
#endif
	}

	void CLowLevelLockAggregate::f_UnlockNoSanitize()
	{
#if DMibEnableSafeCheck > 0
		m_ThreadID = 0;
		m_AlternateThreadID = 0;
#endif
#ifdef DMibSanitizerEnabled_Thread
		__tsan_release(&m_Lock);
#endif
#if DPlatformVersion < 10120
		if (&os_unfair_lock_lock)
#endif
			os_unfair_lock_unlock((os_unfair_lock_t)&m_Lock);
#if DPlatformVersion < 10120
		else
			m_Lock.f_Exchange(0, NAtomic::gc_MemoryOrder_Release);
#endif
	}
}

#if __has_include(<os/os_sync_wait_on_address.h>)
	// The native futex API ships with the macOS 14.4 SDK and later
	#include <os/os_sync_wait_on_address.h>
	#define DMibPMacOSHasOsSync 1
#endif

#if !defined(DMibPMacOSHasOsSync) || DPlatformVersion < 140400
	// Fallback for macOS < 14.4 (or pre-14.4 SDKs) where os_sync_wait_on_address is unavailable.
	// __ulock_wait/__ulock_wait2/__ulock_wake are private but ABI-stable syscall
	// wrappers (libc++ atomic waits and os_unfair_lock park through the same ulock
	// syscalls). __ulock_wait2 (uint64 nanosecond timeout) exists since macOS 11
	// and is weak-imported; where the symbol is absent the code falls back to
	// __ulock_wait (macOS 10.12+, uint32 microsecond timeout).
	// Delete this block when the deployment floor is raised to macOS 14.4.
	#define DMibPMacOSUseUlockFallback 1

	extern "C"
	{
		int __ulock_wait(uint32_t _Operation, void *_pAddress, uint64_t _Value, uint32_t _TimeoutUs);
		int __ulock_wait2(uint32_t _Operation, void *_pAddress, uint64_t _Value, uint64_t _TimeoutNs, uint64_t _Value2) __attribute__((weak_import));
		int __ulock_wake(uint32_t _Operation, void *_pAddress, uint64_t _WakeValue);
	}

	namespace
	{
		constexpr uint32 gc_UlockCompareAndWait = 0x00000001; // UL_COMPARE_AND_WAIT
		constexpr uint32 gc_UlockFlagWakeAll = 0x00000100; // ULF_WAKE_ALL
		constexpr uint32 gc_UlockFlagNoErrNo = 0x01000000; // ULF_NO_ERRNO
	}
#endif

#include <cstdlib>

namespace
{
	[[noreturn]] inline_never void fg_AbortFutexWord(int _Error)
	{
		[[maybe_unused]] volatile int Error = _Error;
		DMibPDebugBreak;
		std::abort();
	}
}

void NSys::fg_Futex_Wait(uint32 volatile *_pAddress, uint32 _Expected)
{
#if defined(DMibPMacOSHasOsSync)
#	if defined(DMibPMacOSUseUlockFallback)
	if (__builtin_available(macOS 14.4, iOS 17.4, *))
#	endif
	{
		int Result = os_sync_wait_on_address((void *)_pAddress, _Expected, sizeof(uint32), OS_SYNC_WAIT_ON_ADDRESS_NONE);
		if (Result >= 0)
			return;

		int ErrNo = errno;
		if (ErrNo != EINTR && ErrNo != EFAULT && ErrNo != ENOMEM)
			fg_AbortFutexWord(ErrNo);

		return;
	}
#endif

#if defined(DMibPMacOSUseUlockFallback)
	int Result;
	if (&__ulock_wait2)
		Result = __ulock_wait2(gc_UlockCompareAndWait | gc_UlockFlagNoErrNo, (void *)_pAddress, _Expected, 0, 0);
	else
		Result = __ulock_wait(gc_UlockCompareAndWait | gc_UlockFlagNoErrNo, (void *)_pAddress, _Expected, 0);

	if (Result >= 0)
		return;

	if (Result != -EINTR && Result != -EFAULT && Result != -ENOMEM)
		fg_AbortFutexWord(-Result);
#endif
}

bool NSys::fg_Futex_WaitTimeout(uint32 volatile *_pAddress, uint32 _Expected, fp64 _Timeout)
{
	if (_Timeout <= 0.0)
		return true;

	if (_Timeout >= fp64(1000000000.0))
	{
		// Longer than ~31 years; avoid nanosecond overflow and wait untimed
		fg_Futex_Wait(_pAddress, _Expected);
		return false;
	}

	uint64 TimeoutNs = (uint64)(_Timeout * fp64(1000000000.0)).f_Ceil().f_ToInt();

#if defined(DMibPMacOSHasOsSync)
#	if defined(DMibPMacOSUseUlockFallback)
	if (__builtin_available(macOS 14.4, iOS 17.4, *))
#	endif
	{
		int Result = os_sync_wait_on_address_with_timeout
			(
				(void *)_pAddress
				, _Expected
				, sizeof(uint32)
				, OS_SYNC_WAIT_ON_ADDRESS_NONE
				, OS_CLOCK_MACH_ABSOLUTE_TIME
				, TimeoutNs
			)
		;
		if (Result >= 0)
			return false;

		int ErrNo = errno;
		if (ErrNo == ETIMEDOUT)
			return true;

		// EINTR/EFAULT/ENOMEM count as spurious wakeups; the caller re-checks its predicate
		if (ErrNo != EINTR && ErrNo != EFAULT && ErrNo != ENOMEM)
			fg_AbortFutexWord(ErrNo);

		return false;
	}
#endif

#if defined(DMibPMacOSUseUlockFallback)
	if (&__ulock_wait2)
	{
		int Result = __ulock_wait2(gc_UlockCompareAndWait | gc_UlockFlagNoErrNo, (void *)_pAddress, _Expected, TimeoutNs, 0);
		if (Result >= 0)
			return false;

		if (Result == -ETIMEDOUT)
			return true;

		// EINTR/EFAULT/ENOMEM count as spurious wakeups; the caller re-checks its predicate
		if (Result != -EINTR && Result != -EFAULT && Result != -ENOMEM)
			fg_AbortFutexWord(-Result);

		return false;
	}

	// __ulock_wait takes a uint32 microsecond timeout (max ~71.6 minutes); longer
	// waits are capped and a capped expiry is reported as a spurious wake so the
	// caller re-arms against its deadline
	uint64 TimeoutUs = (TimeoutNs + 999) / 1000;
	bool bCapped = false;
	if (TimeoutUs > uint64(0xffffffff))
	{
		TimeoutUs = uint64(0xffffffff);
		bCapped = true;
	}

	int Result = __ulock_wait(gc_UlockCompareAndWait | gc_UlockFlagNoErrNo, (void *)_pAddress, _Expected, (uint32)TimeoutUs);
	if (Result >= 0)
		return false;

	if (Result == -ETIMEDOUT)
		return !bCapped;

	// EINTR/EFAULT/ENOMEM count as spurious wakeups; the caller re-checks its predicate
	if (Result != -EINTR && Result != -EFAULT && Result != -ENOMEM)
		fg_AbortFutexWord(-Result);

	return false;
#endif
}

// Wake errors tolerated in all wake paths: ENOENT (no waiters) and EDOM/EINVAL
// (the address was reused by a different ulock opcode or wait size, e.g. an
// os_unfair_lock, after a woken waiter destroyed the object; ulock reports this
// as EDOM, os_sync as EINVAL) — all are harmless stale wakes under the
// destruction-safe contract. Our own os_sync arguments are compile-time
// constants, so EINVAL cannot indicate an argument error here.
void NSys::fg_Futex_WakeOne(uint32 volatile *_pAddress)
{
#if defined(DMibPMacOSHasOsSync)
#	if defined(DMibPMacOSUseUlockFallback)
	if (__builtin_available(macOS 14.4, iOS 17.4, *))
#	endif
	{
		if (os_sync_wake_by_address_any((void *)_pAddress, sizeof(uint32), OS_SYNC_WAKE_BY_ADDRESS_NONE) != 0)
		{
			int ErrNo = errno;
			if (ErrNo != ENOENT && ErrNo != EDOM && ErrNo != EINVAL)
				fg_AbortFutexWord(ErrNo);
		}
		return;
	}
#endif

#if defined(DMibPMacOSUseUlockFallback)
	int Result = __ulock_wake(gc_UlockCompareAndWait | gc_UlockFlagNoErrNo, (void *)_pAddress, 0);
	if (Result < 0 && Result != -ENOENT && Result != -EDOM)
		fg_AbortFutexWord(-Result);
#endif
}

void NSys::fg_Futex_WakeCount(uint32 volatile *_pAddress, uint32 _nToWake)
{
	// No wake-count API on macOS; loop single wakes and stop as soon as the
	// kernel reports no remaining waiters
#if defined(DMibPMacOSHasOsSync)
#	if defined(DMibPMacOSUseUlockFallback)
	if (__builtin_available(macOS 14.4, iOS 17.4, *))
#	endif
	{
		while (_nToWake--)
		{
			if (os_sync_wake_by_address_any((void *)_pAddress, sizeof(uint32), OS_SYNC_WAKE_BY_ADDRESS_NONE) != 0)
			{
				int ErrNo = errno;
				if (ErrNo != ENOENT && ErrNo != EDOM && ErrNo != EINVAL)
					fg_AbortFutexWord(ErrNo);

				return;
			}
		}
		return;
	}
#endif

#if defined(DMibPMacOSUseUlockFallback)
	while (_nToWake--)
	{
		int Result = __ulock_wake(gc_UlockCompareAndWait | gc_UlockFlagNoErrNo, (void *)_pAddress, 0);
		if (Result < 0)
		{
			if (Result != -ENOENT && Result != -EDOM)
				fg_AbortFutexWord(-Result);

			return;
		}
	}
#endif
}

void NSys::fg_Futex_WakeAll(uint32 volatile *_pAddress)
{
#if defined(DMibPMacOSHasOsSync)
#	if defined(DMibPMacOSUseUlockFallback)
	if (__builtin_available(macOS 14.4, iOS 17.4, *))
#	endif
	{
		if (os_sync_wake_by_address_all((void *)_pAddress, sizeof(uint32), OS_SYNC_WAKE_BY_ADDRESS_NONE) != 0)
		{
			int ErrNo = errno;
			if (ErrNo != ENOENT && ErrNo != EDOM && ErrNo != EINVAL)
				fg_AbortFutexWord(ErrNo);
		}
		return;
	}
#endif

#if defined(DMibPMacOSUseUlockFallback)
	int Result = __ulock_wake(gc_UlockCompareAndWait | gc_UlockFlagWakeAll | gc_UlockFlagNoErrNo, (void *)_pAddress, 0);
	if (Result < 0 && Result != -ENOENT && Result != -EDOM)
		fg_AbortFutexWord(-Result);
#endif
}

uint16 NSys::fg_Langague_GetSystemLanguage(NMib::NStr::CStr &_Language)
{
	_Language = fg_MacOS_GetSystemLanguage();
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

umint NSys::fg_Thread_GetVirtualCores()
{
	int nCPUs = 0;
	size_t DataSize = sizeof(nCPUs);
	int Ret = sysctlbyname("hw.logicalcpu", &nCPUs, &DataSize, nullptr, 0);

	if (Ret == 0)
		return (nCPUs > 0) ? nCPUs : 1;
	return 1;
}

umint NSys::fg_Thread_GetPhysicalCores()
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


inline_never umint NMib::NSys::fg_System_GetStackTrace(CMibCodeAddress *_pStack, umint _nMaxDepth)
{
	return (umint)backtrace((void**)_pStack, (int)_nMaxDepth);
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

void *NSys::fg_Mem_VirtualAllocInRange(umint &_Size, uint8 *_pLower, uint8 *_pUpper, EAllocationFlag _AllocFlags, ENumaNode _NumaNode, umint _Alignment)
{
	DMibFastCheck(_AllocFlags & EAllocationFlag_WillFreeWithSize);
	DMibFastCheck(_Alignment == 0);

	auto MachTask = mach_task_self();

	_Size = fg_AlignUp(_Size, NMib::NSys::NPrivate::g_PageSize);

	_pLower = fg_AlignUp(_pLower, NMib::NSys::NPrivate::g_PageSize);
	_pUpper = fg_AlignDown(_pUpper, NMib::NSys::NPrivate::g_PageSize);

	vm_address_t AllocAddress = (vm_address_t)(umint)_pLower;
	vm_address_t EndAddress = (vm_address_t)(umint)_pUpper - _Size;

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

void NSys::fg_Mem_VirtualFlushInstructionCache(void *_pMem, umint _Size)
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

#if DPlatformVersion <= 1050 || defined(DMibNoMacOSCrossModuleExceptions)

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
#ifdef DMibNoMacOSCrossModuleExceptions
namespace
{
	constinit NAtomic::TCAtomic<struct mach_header const *> g_ThisModuleImage = nullptr;
}
#endif

extern "C" bool _dyld_find_unwind_sections(void* addr, struct dyld_unwind_sections* info)
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#ifdef DMibNoMacOSCrossModuleExceptions
	if (!g_ThisModuleImage.f_Load(NAtomic::gc_MemoryOrder_Acquire))
#if __has_feature(ptrauth_calls)
		g_ThisModuleImage.f_Store(_dyld_get_image_header_containing_address((void *)ptrauth_strip(&_dyld_find_unwind_sections, ptrauth_key_function_pointer)));
#else
		g_ThisModuleImage.f_Store(_dyld_get_image_header_containing_address((void *)&_dyld_find_unwind_sections));
#endif
#endif
	auto pHeader = _dyld_get_image_header_containing_address(addr);
#pragma clang diagnostic pop
	if (!pHeader)
		return false;
#ifdef DMibNoMacOSCrossModuleExceptions
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
