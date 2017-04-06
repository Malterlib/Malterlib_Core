// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include <Mib/Core/DynamicLibrary>
#include <Mib/Desktop/DBus>
#include <Mib/Desktop/DesktopFile>
#include <Mib/Cryptography/UUID>
#include <Mib/String/AnsiConversion>

#include "Malterlib_Core_PlatformImp_Linux_FileNotificationContext.h"
#include "Malterlib_Core_PlatformImp_Linux_SecurePassword.h"

#define DMibAllowCodeStandardViolations 1

#include <Mib/Concurrency/ThreadSafeQueue>

#include <malloc.h>
#include <unwind.h>


using namespace NMib;
using namespace NMib::NStr;
using namespace NMib::NTime;
using namespace NMib::NMem;
using namespace NMib::NContainer;

namespace NLocal
{
	int (* g_f_pipe2)(int __pipedes[2], int __flags) __THROW __wur = nullptr;
	int (* g_f_inotify_init1)(int __flags) __THROW = nullptr;
	int (* g_f_inotify_init)(void) __THROW = nullptr;
	int (* g_f_inotify_add_watch)(int __fd, const char *__name, uint32_t __mask) __THROW = nullptr;
	int (* g_f_inotify_rm_watch)(int __fd, int __wd) __THROW = nullptr;
	int (* g_f_pthread_setname_np)(pthread_t __target_thread, __const char *__name) = nullptr;
	void *(* g_f_memcpy)(void *__restrict __dest, __const void *__restrict __src, __SIZE_TYPE__ __n) = &memmove;
	_Unwind_Reason_Code (*g_f_unwind_backtrace) (_Unwind_Trace_Fn, void *);
	_Unwind_Ptr (*g_f_unwind_getip) (struct _Unwind_Context *);
	
	void fg_GetSymbols()
	{
		(void * &)g_f_pipe2 = dlsym(RTLD_DEFAULT, "pipe2");
		(void * &)g_f_inotify_init1 = dlsym(RTLD_DEFAULT, "inotify_init1");
		(void * &)g_f_inotify_init = dlsym(RTLD_DEFAULT, "inotify_init");
		(void * &)g_f_inotify_add_watch = dlsym(RTLD_DEFAULT, "inotify_add_watch");
		(void * &)g_f_inotify_rm_watch = dlsym(RTLD_DEFAULT, "inotify_rm_watch");
		(void * &)g_f_pthread_setname_np = dlsym(RTLD_DEFAULT, "pthread_setname_np");
		(void * &)g_f_unwind_backtrace = dlsym(RTLD_DEFAULT, "_Unwind_Backtrace");
		(void * &)g_f_unwind_getip = dlsym(RTLD_DEFAULT, "_Unwind_GetIP");
		(void * &)g_f_memcpy = dlsym(RTLD_NEXT, "memcpy");
	}
}

bint g_bIsSharedLibrary = false;

mint g_MainModuleBase = 0;

void fg_ForkPrepare();
void fg_ForkParentOrChild();

#include "Malterlib_Core_Platform_POSIX_ErrNo.h"

// *************************************************************************************************************************
// POSIX Implementation specific headers
// *************************************************************************************************************************

#if !defined(DMibPOverrideOperatorNew)
	void operator delete(void *pUserData)
	{
		NMib::NMem::fg_Free(pUserData);
	}

	void * operator new(size_t _Size)
	{
		return NMib::NMem::fg_Alloc(_Size);
	}

#endif

// *************************************************************************************************************************
// POSIX Implementation
// *************************************************************************************************************************

#define DMibPLinuxKernel
#define DMibConfig_SemaphoreImplemented

#include "Malterlib_Core_PlatformImp_POSIX_PThread.hpp"
#include "Malterlib_Core_PlatformImp_Linux_PThread.hpp"
#include "Malterlib_Core_PlatformImp_POSIX.imp.h"
#include "Malterlib_Core_PlatformImp_POSIX_File.hpp"
#include "Malterlib_Core_PlatformImp_POSIX_Console.hpp"
#include "Malterlib_Core_PlatformImp_POSIX_Environment.hpp"
#include "Malterlib_Core_PlatformImp_POSIX_Module.hpp"
#include "Malterlib_Core_PlatformImp_POSIX_User.hpp"
#include "Malterlib_Core_PlatformImp_POSIX_VirtualMemory.hpp"
#include "Malterlib_Core_PlatformImp_POSIX_Net.imp.h"

// *************************************************************************************************************************
// Linux Implementation
// *************************************************************************************************************************

#include <sys/mman.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/mount.h>	
#include <sys/sysctl.h>
#include <sys/epoll.h>
#include <semaphore.h>

#include <execinfo.h>
#include <dlfcn.h>
#include <sys/time.h>

#include <string.h>
#include <dlfcn.h>
#include <cxxabi.h>

#include <uuid/uuid.h>	// For UUID gen

static inline_small class CSystemLinux *fg_GetLocalSys();

void calling_convention_c fg_Malterlib_MakeActive()
{

}

CStr fg_EscapeString(CStr _In)
{
	return _In.f_Replace("\"","\\\"");
}

class CImpSemaphore
{
public:
	
	NMib::NThread::CSpinLock m_Lock;
	
	sem_t m_Semaphore;
	zbool m_bSemaphoreInit;
	
	mint m_Value;
	mint m_Maximum;
	
	CImpSemaphore(mint _Value, mint _Maximum)
	{
		f_Init();
		m_Value = _Value;
		m_Maximum = _Maximum;
	}
	
	~CImpSemaphore()
	{
		{		
			DMibLock(m_Lock);
		}
		
		if (m_bSemaphoreInit)
		{
			if (sem_destroy(&m_Semaphore))
				DMibError(NMib::NPlatform::fg_FormatErrno("sem_destroy (semaphore destructor)", errno));
		}
	}
	
	void f_Init()
	{
		m_Lock.f_Construct();
		if (sem_init(&m_Semaphore, false, 0))
			DMibError(NMib::NPlatform::fg_FormatErrno("sem_init (semaphore init)", errno));
		m_bSemaphoreInit = true;
		
	}
	
	void f_Signal(mint _Count)
	{
		DMibLock(m_Lock);
		if (m_Value + _Count > m_Maximum)
			_Count = m_Maximum - m_Value;
		m_Value += _Count;

		while (_Count--)
			sem_post(&m_Semaphore);
	}
	
	bint f_TryWait()
	{
		bint bRet = false;
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
				if (sem_wait(&m_Semaphore))
				{
					int ErrNo = errno;
					if (ErrNo == EINTR)
						continue;
					DMibError(NMib::NPlatform::fg_FormatErrno("sem_wait (semaphore wait)", ErrNo));
				}
			}
		}
		--m_Value;
	}
	
	bint f_WaitTimeout(fp32 _Timeout)
	{
		bint bRet = true;
		CClockRaw TimeWait;
		TimeWait.f_Start();
		DMibLock(m_Lock);
		fp64 Time = TimeWait.f_GetTime();
		while (Time < _Timeout)
		{
			if (m_Value <= 0)
			{
				timespec ToWait;
				clock_gettime(CLOCK_REALTIME, &ToWait);
				
				fp64 ToWaitLeft = (_Timeout - Time) + fp32(ToWait.tv_nsec) * (fp32(1.0f) / fp32(1000000000.0f));
				fp64 nSec = ToWaitLeft.f_Floor();
				ToWait.tv_sec += nSec.f_ToInt();
				ToWait.tv_nsec = ((ToWaitLeft - nSec)*fp32(1000000000.0f)).f_ToInt();
				
				{
					DMibUnlock(m_Lock);
					if (sem_timedwait(&m_Semaphore, &ToWait))
					{
						int ErrNo = errno;
						if (ErrNo == ETIMEDOUT || ErrNo == EINTR)
							;
						else
							DMibError(NMib::NPlatform::fg_FormatErrno("sem_timedwait (semaphore wait timeout)", errno));
					}
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

NMem::TCPoolAggregate<CImpSemaphore, 128, NThread::CSpinLockAggregate, CPoolType_Freeable, CAllocator_VirtualNoTracking> g_ImpSemaphorePool = {DAggregateInit};

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

bint NSys::fg_Semaphore_WaitTimeout(void * _pSemaphore, fp64 _Timeout)
{
	CImpSemaphore *pSemaphore = (CImpSemaphore *)_pSemaphore;
	return pSemaphore->f_WaitTimeout(_Timeout * NTime::CSystem_Time::fs_GetTimeSpeedReciprocal());
}

bint NSys::fg_Semaphore_TryWait(void * _pSemaphore)
{
	CImpSemaphore *pSemaphore = (CImpSemaphore *)_pSemaphore;
	return pSemaphore->f_TryWait();
	
}

namespace NMib
{
	namespace NSys
	{
		extern const ch8* g_LinuxProgramIdentifier;
	}
}


namespace
{
	bool g_bCanStackTrace = false;
}

NMib::NSys::EDesktopEnvironment fg_DeduceDesktopEnvironment()
{
	using namespace NMib::NSys;
	
	NMib::NStr::CStr CurDesktop = fg_GetSys()->f_GetEnvironmentVariable("XDG_CURRENT_DESKTOP");
	if (!CurDesktop.f_IsEmpty())
	{
		if (CurDesktop == "Unity")
			return EDesktopEnvironment_Unity;
		else if (CurDesktop == "GNOME")
			return EDesktopEnvironment_GNOME;
		else if (CurDesktop == "LXDE")
			return EDesktopEnvironment_LXDE;
	}
	
	NMib::NStr::CStr DesktopSession = fg_GetSys()->f_GetEnvironmentVariable("DESKTOP_SESSION");
	if (!DesktopSession.f_IsEmpty())
	{
		if (DesktopSession == "gnome")
			return EDesktopEnvironment_GNOME;
		else if (DesktopSession == "kde4")
			return EDesktopEnvironment_KDE4;
		else if (DesktopSession == "kde")
		{
			if (!fg_GetSys()->f_GetEnvironmentVariable("KDE_SESSION_VERSION").f_IsEmpty())
				return EDesktopEnvironment_KDE4;
			
			return EDesktopEnvironment_KDE3;
		}
		else if (		DesktopSession == "xcfe"
				 ||	DesktopSession == "xubuntu")
			return EDesktopEnvironment_XCFE;
	}
	
	if (!fg_GetSys()->f_GetEnvironmentVariable("GNOME_DESKTOP_SESSION_ID").f_IsEmpty())
		return EDesktopEnvironment_GNOME;
	else if (!fg_GetSys()->f_GetEnvironmentVariable("KDE_FULL_SESSION").f_IsEmpty())
	{
		if (!fg_GetSys()->f_GetEnvironmentVariable("KDE_SESSION_VERSION").f_IsEmpty())
			return EDesktopEnvironment_KDE4;
		
		return EDesktopEnvironment_KDE3;
	}
	
	return EDesktopEnvironment_Linux;
}


typedef char uuid_string_t[256];

DMibDefineDynamicLibraryClassAggregate(CUUIDLibrary, EDLFlag_NoThrow | EDLFlag_NoAutoLoad, "libuuid.so.1"
								,	uuid_generate
								,	uuid_unparse
							);

CUUIDLibrary g_UUIDLibrary = { DAggregateInit };

static void fg_LoadLibraries()
{
	g_UUIDLibrary.f_Reload("libuuid.so.1");
}

class CSystemLinux : public CSystem
{
public:

	CSystem_POSIX m_Posix;
	
	pthread_key_t m_ThreadDestructionHook;

	NMib::NAggregate::TCAggregate<CPOSIXSocketContext> m_SocketContext = { DAggregateInit };

	NMib::NPtr::TCUniquePointer<NMib::NDBus::CSystem> m_pDBus; // May be nullptr

	NMib::NAtomic::TCAtomic<mint> m_PasswordManagerCreated;
	NMib::NPtr::TCUniquePointer<NMib::NSys::CLinuxPasswordManager> m_pPasswordManager;

	NMib::NSys::EDesktopEnvironment m_DesktopEnvironment;
	
	zbool m_bForkedChild;


	static void fs_ForkPrepare()
	{
		auto &Sys = *fg_GetLocalSys();
		if (!pthread_getspecific(Sys.m_ThreadDestructionHook))
		{
			pthread_setspecific(Sys.m_ThreadDestructionHook, (void *)(mint)getpid());
			Sys.m_Posix.f_GetMalterlibDisableStdErrLog(); // getenv fails on forked process, to workaround this here
			Sys.f_PrepareFork();
			g_EventEmulationPool.f_Lock();
			g_ImpSemaphorePool.f_Lock();
		}
	}

	static void fs_ForkParentOrChild()
	{
		auto &Sys = *fg_GetLocalSys();
		void *Current = pthread_getspecific(Sys.m_ThreadDestructionHook);
		if (Current)
		{
			pthread_setspecific(Sys.m_ThreadDestructionHook, nullptr);
			if (Current != (void *)(mint)getpid())
			{
				Sys.m_bForkedChild = true;
				g_bCanStackTrace = false;
			}
			g_ImpSemaphorePool.f_Unlock();
			g_EventEmulationPool.f_Unlock();
			if (Current == (void *)(mint)getpid())
				Sys.f_ForkedParent(); // Parent
			else
				Sys.f_ForkedChild(); // Child
		}
	}
	static void fs_ForkParent()
	{
		auto &Sys = *fg_GetLocalSys();
		if (pthread_getspecific(Sys.m_ThreadDestructionHook))
		{
			pthread_setspecific(Sys.m_ThreadDestructionHook, 0);
			g_ImpSemaphorePool.f_Unlock();
			g_EventEmulationPool.f_Unlock();
			Sys.f_ForkedParent();
		}
	}

	static void fs_ForkChild()
	{
		auto &Sys = *fg_GetLocalSys();
		if (pthread_getspecific(Sys.m_ThreadDestructionHook))
		{
			pthread_setspecific(Sys.m_ThreadDestructionHook, 0);
			Sys.m_bForkedChild = true;
			g_bCanStackTrace = false;
			g_ImpSemaphorePool.f_Unlock();
			g_EventEmulationPool.f_Unlock();
			Sys.f_ForkedChild();
		}
	}
	
	CSystemLinux()
		: CSystem(g_bIsSharedLibrary)
#ifdef DCompiler_clang
		, m_SocketContext{DAggregateInit}
#endif
		, m_PasswordManagerCreated(0)
	{
		fg_MemClear(m_SocketContext);

		pthread_key_create(&m_ThreadDestructionHook, fs_ThreadDestructionHook);

		m_DesktopEnvironment = fg_DeduceDesktopEnvironment();
		
		fp_InitComplete();
	}

	NMib::NSys::CLinuxPasswordManager* f_GetPasswordManager()
	{
		if (m_PasswordManagerCreated.f_Load() != 2)
		{
			mint Expected = 0;
			if (m_PasswordManagerCreated.f_CompareExchangeStrong(Expected, 1))
			{
				m_pPasswordManager = NMib::NSys::fg_CreateLinuxPasswordManager(m_pDBus.f_Get());
				m_PasswordManagerCreated.f_Exchange(2);
			}
			else
			{
				while (m_PasswordManagerCreated.f_Load() != 2)
				{
					NSys::fg_Thread_SmallestSleep();
				}
			}
		}
		
		return m_pPasswordManager.f_Get();
	}

	void f_InitModule()
	{
		CSystem::f_InitModule();

		m_pDBus = fg_Construct();

	}

	void f_DestroyThreadSpecific()
	{
		CSystem::f_PreDestructThreadSpecific();
		
		m_Posix.f_DestroyThreadSpecific();

		if (m_FileChangeNotificationContext.m_bConstructed)
			m_FileChangeNotificationContext.f_Destruct();
		
		CSystem::f_DestructThreadSpecific();
	}
	
	void f_Destruct()
	{
		m_Posix.f_Destruct();
		
		if (m_SocketContext.m_bConstructed)
			m_SocketContext.f_Destruct();
		

		g_UUIDLibrary.f_Unload();

		m_pPasswordManager = nullptr;
		m_pDBus = nullptr;

		CSystem::f_Destruct();

		pthread_key_delete(m_ThreadDestructionHook);

	}
	
	static void fs_ThreadDestructionHook(void* _ThreadID)
	{

		fg_GetSys()->f_ThreadLocalFreeThread();
	}

	void f_RegisterDestructionHookForThread()
	{
		pthread_setspecific(m_ThreadDestructionHook, (void*)NSys::fg_Thread_GetCurrentUID());
	}
	
	NMib::NAggregate::TCAggregate<CFileChangeNotificationContext> m_FileChangeNotificationContext = { DAggregateInit };

	
	void f_LoadLibraries()
	{
	}
	
};

static inline_small CSystemLinux *fg_GetLocalSys()
{
	return (CSystemLinux *)fg_GetSys();
}

CSystem_POSIX *fg_GetSys_POSIX()
{
	return &fg_GetLocalSys()->m_Posix;
}

void NSys::fg_System_GenerateUUID(NDataProcessing::CUniversallyUniqueIdentifier &_UUID)
{
	static_assert(sizeof(uuid_t) == sizeof(_UUID), "");
	if (g_UUIDLibrary.f_OK())
	{
		g_UUIDLibrary.uuid_generate((unsigned char *)&_UUID);
#		if DMibEnableSafeCheck > 0
			uuid_string_t RetStr;
			g_UUIDLibrary.uuid_unparse((unsigned char *)&_UUID, RetStr);
#		endif
		_UUID.m_TimeLow = fg_ByteSwapBE(_UUID.m_TimeLow);
		_UUID.m_TimeMid = fg_ByteSwapBE(_UUID.m_TimeMid);
		_UUID.m_TimeHiAndVersion = fg_ByteSwapBE(_UUID.m_TimeHiAndVersion);
#		if DMibEnableSafeCheck > 0
			DMibFastCheck(_UUID.f_GetAsStaticString(NDataProcessing::EUniversallyUniqueIdentifierFormat_Bare).f_CmpNoCase(RetStr) == 0);
#		endif
	}
	else
		_UUID = NDataProcessing::CUniversallyUniqueIdentifier(NDataProcessing::EUniversallyUniqueIdentifierGenerate_Random);
}


NStr::CStr NSys::fg_System_GenerateUUID()
{
	if (g_UUIDLibrary.f_OK())
	{
		uuid_t Ret;
		g_UUIDLibrary.uuid_generate(Ret);
		uuid_string_t RetStr;
		g_UUIDLibrary.uuid_unparse(Ret, RetStr);

		// {7EE072A7-458D-491f-ACCF-447AD4BE8DBF}
		return CStr(CStr::CFormat("{{{}}") << RetStr);
	}
	else
		return NDataProcessing::fg_GetRandomUuidString(NDataProcessing::EUniversallyUniqueIdentifierFormat_Registry);
}


struct CCodePageCache
{
	TCMap<ch32, ch8, CSort_Default, CAllocator_NonTrackedHeap> m_Cache;
	CCodePageCache()
	{
		for (int i = 0; i < 256; ++i)
		{
			ch32 Char = NMib::NTraits::TCUnsigned<ch32>::CType(NMib::NTraits::TCUnsigned<ch16>::CType(NMib::NStr::TCCharEncodingConverter<NMib::NStr::ECharacterEncoding_Windows_1252>::ms_Table[i].m_UnicodeChar));
			if (Char != 0xFFFF)
				m_Cache[Char] = i;
		}
			
	}
	
};

NMib::NAggregate::TCAggregate<CCodePageCache> g_CodePageCache = { DAggregateInit };

void NMib::NSys::NStr::fg_SystemEncodeAnsiStr(NMib::NStr::CStr const &_In, NMib::NStr::CAnsiStr &_Out, ch8 _ErrorChar)
{
	CStr Utf8 = _In;
	
	auto &Cache = *g_CodePageCache;
	
	_Out.f_Clear();
	auto Iter = Utf8.f_GetUnicodeIterator();
	while (Iter)
	{
		ch8 *pChar = Cache.m_Cache.f_FindEqual(*Iter);
		if (pChar)
		{
			_Out.f_AddChar(*pChar);
		}
		else
			_Out.f_AddChar(_ErrorChar);

		++Iter;
	}
}

void NMib::NSys::NStr::fg_SystemEncodeAnsiStr(NMib::NStr::CStrNonTracked const &_In, NMib::NStr::CAnsiStrNonTracked &_Out, ch8 _ErrorChar)
{
	CStrNonTracked Utf8 = _In;
	
	auto &Cache = *g_CodePageCache;
	
	_Out.f_Clear();
	auto Iter = Utf8.f_GetUnicodeIterator();
	while (Iter)
	{
		ch8 *pChar = Cache.m_Cache.f_FindEqual(*Iter);
		if (pChar)
			_Out.f_AddChar(*pChar);
		else
			_Out.f_AddChar(_ErrorChar);
		
		++Iter;
	}
}


void NMib::NSys::NStr::fg_SystemDecodeAnsiStr(NMib::NStr::CAnsiStr const &_In, NMib::NStr::CStr &_Out)
{
	_Out = NMib::NStr::fg_DecodeCharacterEncoding<NMib::NStr::ECharacterEncoding_Windows_1252>(_In);
}

void NMib::NSys::NStr::fg_SystemDecodeAnsiStr(ch8 const *_pIn, NMib::NStr::CStr &_Out)
{
	_Out = NMib::NStr::fg_DecodeCharacterEncoding<NMib::NStr::ECharacterEncoding_Windows_1252>(_pIn);
}

void NMib::NSys::NStr::fg_SystemDecodeAnsiStr(NMib::NStr::CAnsiStrNonTracked const &_In, NMib::NStr::CStrNonTracked &_Out)
{
	const NMib::NStr::CStrNonTracked::CChar *pIn = _In.f_GetStr();
	_Out = NMib::NStr::fg_DecodeCharacterEncodingNonTracked<NMib::NStr::ECharacterEncoding_Windows_1252>(pIn);
}

void NMib::NSys::NStr::fg_SystemDecodeAnsiStr(ch8 const *_pIn, NMib::NStr::CStrNonTracked &_Out)
{
	_Out = NMib::NStr::fg_DecodeCharacterEncodingNonTracked<NMib::NStr::ECharacterEncoding_Windows_1252>(_pIn);
}

void NMib::NSys::NStr::fg_SystemEncodeCodePageStr(NMib::NStr::CStrNonTracked const &_In, NMib::NStr::CAnsiStrNonTracked &_Out, uint32 _CodePage, ch8 _ErrorChar)
{
	if (_CodePage != 1252)
		DMibError(NMib::NStr::CStrNonTracked::CFormat("Codepage {} not supported") << _CodePage);
	fg_SystemEncodeAnsiStr(_In, _Out, _ErrorChar);
}

void NMib::NSys::NStr::fg_SystemDecodeCodePageStr(NMib::NStr::CAnsiStrNonTracked const &_In, NMib::NStr::CStrNonTracked &_Out, uint32 _CodePage)
{
	if (_CodePage != 1252)
		DMibError(NMib::NStr::CStrNonTracked::CFormat("Codepage {} not supported") << _CodePage);
	fg_SystemDecodeAnsiStr(_In, _Out);
}

void NMib::NSys::NStr::fg_SystemDecodeCodePageStr(ch8 const *_pIn, NMib::NStr::CStrNonTracked &_Out, uint32 _CodePage)
{
	if (_CodePage != 1252)
		DMibError(NMib::NStr::CStrNonTracked::CFormat("Codepage {} not supported") << _CodePage);
	fg_SystemDecodeAnsiStr(_pIn, _Out);
}

void NMib::NSys::NStr::fg_SystemEncodeCodePageStr(NMib::NStr::CStr const &_In, NMib::NStr::CAnsiStr &_Out, uint32 _CodePage, ch8 _ErrorChar)
{
	if (_CodePage != 1252)
		DMibError(NMib::NStr::CStrNonTracked::CFormat("Codepage {} not supported") << _CodePage);
	fg_SystemEncodeAnsiStr(_In, _Out, _ErrorChar);
}

void NMib::NSys::NStr::fg_SystemDecodeCodePageStr(NMib::NStr::CAnsiStr const &_In, NMib::NStr::CStr &_Out, uint32 _CodePage)
{
	if (_CodePage != 1252)
		DMibError(NMib::NStr::CStrNonTracked::CFormat("Codepage {} not supported") << _CodePage);
	fg_SystemDecodeAnsiStr(_In, _Out);
}

void NMib::NSys::NStr::fg_SystemDecodeCodePageStr(ch8 const *_pIn, NMib::NStr::CStr &_Out, uint32 _CodePage)
{
	if (_CodePage != 1252)
		DMibError(NMib::NStr::CStrNonTracked::CFormat("Codepage {} not supported") << _CodePage);
	fg_SystemDecodeAnsiStr(_pIn, _Out);
}

NContainer::TCMap<NMib::NStr::CStr, NMib::NStr::CStr> NMib::NSys::fg_Process_GetEnvironmentVariables_NonProtected()
{
	NContainer::TCMap<NMib::NStr::CStr, NMib::NStr::CStr> Vars;

	CStr Key;

	for 
		(
			char **pEnvironment = environ
			; *pEnvironment
			; ++pEnvironment
		)
	{
		CStr VarStr(*pEnvironment);
		Key = NMib::NStr::fg_GetStrSep(VarStr, "=");

		Vars[Key] = VarStr;
	}

	return fg_Move(Vars);
}

inline_never bint NSys::fg_Compiler_AlwaysFalse()
{
	return false;
}

assure_used inline_never bint NSys::fg_Compiler_MakeActive(const void *_Reference)
{
	(void)_Reference;
	return true;
}

void NSys::fg_Thread_Suspend(void *_pThread)
{
	DMibError("Thread supension not available on linux");
}

void NSys::fg_Thread_Resume(void *_pThread)
{
	DMibError("Thread supension not available on linux");
}

inline_never mint NSys::fg_System_GetStackTrace(CMibCodeAddress *_pStack, mint _nMaxDepth)
{
	if (_nMaxDepth == 0)
		return 0;

	if (!g_bCanStackTrace) // backtrace uses malloc in pthread_once, and _Unwind_Backtrace can deadlock on __GI___dl_iterate_phdr lock 
		return 0;
	
	if (!NLocal::g_f_unwind_backtrace || !NLocal::g_f_unwind_getip)
	{
#ifdef DArchitecture_x86
		// It's not safe to backtrace on x86 as we are using non-stackframe exception handling
		return 0;
#endif
		int nReturned = backtrace((void**)_pStack, (int)_nMaxDepth);
		
		if (nReturned < 0)
			return 0;
		
		return nReturned;
	}
				
	struct CContext
	{
		CMibCodeAddress *m_pStack;
		mint m_nMaxDepth;
		mint m_nAdded = 0;
	};
	
	CContext Context;
	Context.m_pStack = _pStack;
	Context.m_nMaxDepth = _nMaxDepth;
	
	NLocal::g_f_unwind_backtrace
		(
			[](_Unwind_Context *_pUnwindContext, void *_pContext) -> _Unwind_Reason_Code
			{
				CContext &Context = *((CContext *)_pContext);
				Context.m_pStack[Context.m_nAdded] = (CMibCodeAddressType *)NLocal::g_f_unwind_getip(_pUnwindContext);
				++Context.m_nAdded;
				if (Context.m_nAdded == Context.m_nMaxDepth)
					return _URC_END_OF_STACK;
				return _URC_NO_REASON;
			}
			, &Context
		)
	;
	
	if (Context.m_nAdded > 0 && _pStack[Context.m_nAdded - 1] == nullptr)
		--Context.m_nAdded;
	
	return Context.m_nAdded;
}

CMibCodeAddress NSys::fg_System_GetStackTrace(aint _iDepth)
{
	if (_iDepth > 255)
		return 0;
	CMibCodeAddress StackTraces[256];
	StackTraces[_iDepth] = 0;
	
	mint nReturned = fg_System_GetStackTrace(StackTraces, _iDepth - 1);
	if (nReturned <= _iDepth + 1)
		return StackTraces[_iDepth];
	return nullptr;
}

void NSys::fg_System_ReportContractViolation(const NMib::NStr::CStrNonTracked &_Message)
{
}

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

#include <stdio.h>

namespace
{
	template <typename tf_CStr>
	TCVector<tf_CStr> fg_GetCommandLineParams()
	{
		TCVector<tf_CStr> Return;

		tf_CStr CmdlinePath = typename tf_CStr::CFormat("/proc/{}/cmdline") << (mint)getpid();
		auto FileData = fg_ReadProcFS<tf_CStr>(CmdlinePath);
		
		auto pParse = FileData.f_GetArray();
		auto pParseEnd = pParse + FileData.f_GetLen() - 1;
		
		while (pParse < pParseEnd)
		{
			auto *pStart = pParse;
			while (*pParse && pParse < pParseEnd)
				++pParse;
			Return.f_Insert(tf_CStr(pStart, pParse-pStart));
			if (pParse < pParseEnd)
				++pParse;
		}
		
		return Return;
	}
}


void NSys::fg_Security_GenerateHighEntropyData(uint8 *_pData, mint _nBytes)
{
	if (NMib::NPlatform::fg_ReadProcFS("/dev/urandom", _pData, _nBytes) != _nBytes)
		DMibPDebugBreak;
}

NMib::NStr::CStr NSys::fg_Process_GetCommandLine()
{
	NMib::NStr::CStr Return;
	
	TCVector<CStr> Parameters = fg_GetCommandLineParams<CStr>();

	auto iParam = Parameters.f_GetIterator();
	for (; iParam; ++iParam)
	{
		if (!iParam->f_StartsWith("--OutputPID"))
			fg_AddStrSepEscaped(Return, *iParam, ' ');
	}
	
	return Return;
}

static char const *gc_SysVendorLookup[] = {
	 "VMware", "VMware",
	 "Parallels", "Parallels",
	 "innotek", "VirtualBox",
	 "Microsoft", "VirtualPC",
	 nullptr
};

static char const *gc_BiosVendorLookup[] = {
	 "Xen", "Xen",
	 "innotek", "VirtualBox",
	 nullptr
};

static char const *gc_ProductNameLookup[] = {
	 "KVM", "kvm",
	 "Bochs", "kvm",
	 "VMware", "VMware",
	 nullptr
};


bint NSys::fg_HW_GetVirtualMachineInfo(CVirtualMachineInfo& _Info)
{
	_Info.m_bDetected = false;
	_Info.m_pName = nullptr;

	CProcessorInfo ProcInfo;
	fg_HW_GetProcessorInfo(ProcInfo);

	if ((ProcInfo.m_Features & EProcessorFeature_HyperVisor))
		_Info.m_bDetected = true;

	try
	{
		if (NMib::NFile::CFile::fs_FileExists(CStrNonTracked("/sys/class/dmi/id/sys_vendor")))
		{
			auto SysVendorData = fg_ReadProcFS<CStrNonTracked>("/sys/class/dmi/id/sys_vendor");

			char const** pVendorLookup = gc_SysVendorLookup;

			while (*pVendorLookup)
			{
				mint NameLen = fg_StrLen(*pVendorLookup);
				if (	SysVendorData.f_GetLen() >= NameLen
					&&	fg_MemCmp((uint8 const *)SysVendorData.f_GetArray(), (uint8 const *)*pVendorLookup, NameLen) == 0)
				{
					_Info.m_bDetected = true;
					++pVendorLookup;
					_Info.m_pName = *pVendorLookup;
					++pVendorLookup;
					break;
				}
				pVendorLookup += 2;
			}
		}
	}
	catch(NMib::NFile::CExceptionFile const&)
	{
	}

	if (_Info.m_pName)
		return true;

	try
	{
		if (NMib::NFile::CFile::fs_FileExists(CStrNonTracked("/sys/devices/virtual/dmi/id/bios_vendor")))
		{
			auto BiosVendorData = fg_ReadProcFS<CStrNonTracked>("/sys/devices/virtual/dmi/id/bios_vendor");

			char const** pVendorLookup = gc_BiosVendorLookup;

			while (*pVendorLookup)
			{
				mint NameLen = fg_StrLen(*pVendorLookup);
				if (	BiosVendorData.f_GetLen() >= NameLen
					&&	fg_MemCmp((uint8 const *)BiosVendorData.f_GetArray(), (uint8 const *)*pVendorLookup, NameLen) == 0)
				{
					_Info.m_bDetected = true;
					++pVendorLookup;
					_Info.m_pName = *pVendorLookup;
					++pVendorLookup;
					break;
				}
				pVendorLookup += 2;
			}
		}
	}
	catch(NMib::NFile::CExceptionFile const&)
	{
	}


	if (_Info.m_pName)
		return true;

	try
	{
		if (NMib::NFile::CFile::fs_FileExists(CStrNonTracked("/sys/devices/virtual/dmi/id/product_name")))
		{
			auto ProductNameData = fg_ReadProcFS<CStrNonTracked>("/sys/devices/virtual/dmi/id/product_name");

			char const** pProductLookup = gc_ProductNameLookup;

			while (*pProductLookup)
			{
				mint NameLen = fg_StrLen(*pProductLookup);
				if (	ProductNameData.f_GetLen() >= NameLen
					&&	fg_MemCmp((uint8 const *)ProductNameData.f_GetArray(), (uint8 const *)*pProductLookup, NameLen) == 0)
				{
					_Info.m_bDetected = true;
					++pProductLookup;
					_Info.m_pName = *pProductLookup;
					++pProductLookup;
					break;
				}
				pProductLookup += 2;
			}
		}
	}
	catch(NMib::NFile::CExceptionFile const&)
	{
	}

	if (_Info.m_bDetected && !_Info.m_pName)
		_Info.m_pName = "unknown";

	return _Info.m_bDetected;
}

void NSys::fg_Process_GetCommandLineArgs(NContainer::TCVector<NMib::NStr::CStr> &_List)
{
	_List.f_Clear();

	TCVector<CStr> Parameters = fg_GetCommandLineParams<CStr>();

	auto iParam = Parameters.f_GetIterator();
	for (; iParam; ++iParam)
	{
		if (!iParam->f_StartsWith("--OutputPID"))
			_List.f_Insert(*iParam);
	}
	
}


NMib::NStr::CStr NSys::fg_CommandLineParameters()
{
	NMib::NStr::CStr Return;
	
	TCVector<CStr> Parameters = fg_GetCommandLineParams<CStr>();
	
	auto iParam = Parameters.f_GetIterator();
	if (iParam)
		++iParam; // The first param should not be retured for this variant
	for (; iParam; ++iParam)
	{
		if (!iParam->f_StartsWith("--OutputPID"))
			fg_AddStrSepEscaped(Return, *iParam, ' ');
	}
	
	return Return;
}

void NSys::fg_Message(const ch8 *_pMessageType, const ch8 *_pToOutput)
{
	DMibTraceRaw(_pToOutput);
}

void NSys::fg_Message(const ch16 *_pMessageType, const ch16 *_pToOutput)
{
}

void fg_DestroySystemAtExit()
{
	NSys::fg_DestroySystem();
}

namespace NMib
{
	mint align_cacheline g_SystemMemory[sizeof(CSystemLinux) / sizeof(mint)];
	mint g_bCreatingSystemDone = false;
	mint g_bCanUseSystemMalloc = false;
	mint g_bCanStartThreads = false;
}

void fg_ForkPrepare()
{
	CSystemLinux::fs_ForkPrepare();
}

void fg_ForkParentOrChild()
{
	CSystemLinux::fs_ForkParentOrChild();
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

#include <link.h>

using namespace NFunction;
namespace
{
	int	fg_DlIterate(struct dl_phdr_info *_pInfo, size_t _Size, void *_pData)
	{
		TCFunction<int (struct dl_phdr_info *_pInfo, size_t _Size)> *pCallback = fg_AutoStaticCast(_pData);
		return (*pCallback)(_pInfo, _Size);
	}
	
}

namespace NMib
{
	namespace NSys
	{
		int g_OperatingSystemMajor = -1;
		int g_OperatingSystemMinor = 0;
		int g_OperatingSystemFix = 0;
		EOperatingSystemArch g_OperatingSystemArch = EOperatingSystemArch_Unknown;
	}
}

#include <sys/utsname.h>


bint NSys::fg_System_GetOperatingSystemVersion(int& _oMajor, int& _oMinor, int& _oFix, EOperatingSystemArch& _Arch)
{
	if (g_OperatingSystemMajor >= 0)
	{
		_oMajor = g_OperatingSystemMajor;
		_oMinor = g_OperatingSystemMinor;
		_oFix = g_OperatingSystemFix;
		_Arch = g_OperatingSystemArch;
		return g_OperatingSystemMajor != 0;
	}

	g_OperatingSystemMajor = 0;
	
	utsname NameInfo;
	if (uname(&NameInfo))
	{
		return false;
	}
	
	(CStr::CParse("{}.{}.{}") >> g_OperatingSystemMajor >> g_OperatingSystemMinor >> g_OperatingSystemFix).f_Parse(NameInfo.release);

	CStrPtr Machine;
	Machine.f_SetConstPtr(NameInfo.machine, fg_StrLen(NameInfo.machine));
	
	if (Machine == "i386" || Machine == "i486" || Machine == "i586" || Machine == "i686")
		g_OperatingSystemArch = EOperatingSystemArch_x86;
	else if (Machine == "x86_64")
		g_OperatingSystemArch = EOperatingSystemArch_x64;
	else
	{
		DMibFastCheck(false);
		g_OperatingSystemArch = EOperatingSystemArch_Unknown;
	}
	
	return true;
}

namespace NMib
{
	namespace NSys
	{
		namespace NPrivate
		{
		#pragma clang diagnostic push
		#pragma clang diagnostic ignored "-Wdeprecated-declarations"
			void fg_InitBaseModuleAddress()
			{
				bool bFirst = true;
				
				struct dl_phdr_info FirstHeader;
				fg_MemClear(FirstHeader);
				
				TCFunction<int (struct dl_phdr_info *_pInfo, size_t _Size)> Iterate
					= [&](struct dl_phdr_info *_pInfo, size_t _Size) -> int
					{
						// We rely on the first image being the main executable
						if (bFirst)
						{
							bFirst = false;
							FirstHeader = *_pInfo;
						}
						return 0;
					}
				;
				
				dl_iterate_phdr(fg_DlIterate, &Iterate);
				
				if (bFirst)
					DMibPDebugBreak;
				
				Dl_info Info;
				dladdr((void *)&fg_CreateSystem, &Info);
				
				g_MainModuleBase = (mint)((uint8 const *)FirstHeader.dlpi_phdr - FirstHeader.dlpi_phdr->p_offset);
				
			}
		#pragma clang diagnostic pop
			
		}
	}
}


extern "C"
{
	void *nontracked_malloc(size_t __size) __THROW __wur
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
#		if DMibConfig_MalterlibMemoryManager_Debug
			return NMib::NMem::CAllocator_NonTrackedHeap::f_AllocDebug(__size, DMibPFile, DMibPLine, EHeapDebugFlag_Ignore);
#		else
			return NMib::NMem::CAllocator_NonTrackedHeap::f_Alloc(__size);
#		endif
	}

	void *nontracked_calloc (size_t __nmemb, size_t __size) __THROW __wur
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
		mint Size = __nmemb * __size;
#		if DMibConfig_MalterlibMemoryManager_Debug
			auto pMem = NMib::NMem::CAllocator_NonTrackedHeap::f_AllocDebug(Size, DMibPFile, DMibPLine, EHeapDebugFlag_Ignore);
#		else
			auto pMem = NMib::NMem::CAllocator_NonTrackedHeap::f_Alloc(Size);
#		endif
		fg_MemClear(pMem, Size);
		return pMem;
	}	

	void *nontracked_realloc (void *__ptr, size_t __size) __THROW
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
#		if DMibConfig_MalterlibMemoryManager_Debug
			return NMib::NMem::CAllocator_NonTrackedHeap::f_ReallocDebug(__ptr, __size, 0, DMibPFile, DMibPLine, EHeapDebugFlag_Ignore);
#		else
			return NMib::NMem::CAllocator_NonTrackedHeap::f_Realloc(__ptr, __size);
#		endif
	}

	void nontracked_free (void *__ptr) __THROW
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
		return NMib::NMem::CAllocator_NonTrackedHeap::f_Free(__ptr);
	}

	void nontracked_cfree (void *__ptr) __THROW
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
		return NMib::NMem::CAllocator_NonTrackedHeap::f_Free(__ptr);
	}
	void *nontracked_memalign (size_t __alignment, size_t __size) __THROW __wur
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
#		if DMibConfig_MalterlibMemoryManager_Debug
			return NMib::NMem::CAllocator_NonTrackedHeap::f_AllocAlignedDebug(__size, __alignment, DMibPFile, DMibPLine, EHeapDebugFlag_Ignore);
#		else
			return NMib::NMem::CAllocator_NonTrackedHeap::f_AllocAligned(__size, __alignment);
#		endif
	}
	void *nontracked_valloc (size_t __size) __THROW __wur
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
#		if DMibConfig_MalterlibMemoryManager_Debug
			return NMib::NMem::CAllocator_NonTrackedHeap::f_AllocAlignedDebug(__size, NMib::NSys::NPrivate::g_PageSize, DMibPFile, DMibPLine, EHeapDebugFlag_Ignore);
#		else
			return NMib::NMem::CAllocator_NonTrackedHeap::f_AllocAligned(__size, NMib::NSys::NPrivate::g_PageSize);
#		endif
	}
	void * nontracked_pvalloc (size_t __size) __THROW __wur
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
#		if DMibConfig_MalterlibMemoryManager_Debug
			return NMib::NMem::CAllocator_NonTrackedHeap::f_AllocAlignedDebug(__size, NMib::NSys::NPrivate::g_PageSize, DMibPFile, DMibPLine, EHeapDebugFlag_Ignore);
#		else
			return NMib::NMem::CAllocator_NonTrackedHeap::f_AllocAligned(__size, NMib::NSys::NPrivate::g_PageSize);
#		endif
	}
	size_t nontracked_malloc_usable_size (void *__ptr) __THROW
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
		return NMib::NMem::CAllocator_NonTrackedHeap::f_Size(__ptr);
	}
}

void NSys::fg_Process_AllowInvalidExit(bool _bAllow)
{
}

extern "C" void fg_InitMalterlib();

namespace NMib
{
	mint g_bCreatedSystem = false;
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
		ExceptionInfo += CStrNonTracked::CFormat("Uncaught exception of type: {}\n") << _Exception.f_GetClass();
		ExceptionInfo += CStrNonTracked::CFormat("	File: {}\n") << _Exception.f_GetFile();
		ExceptionInfo += CStrNonTracked::CFormat("	Line: {}\n") << _Exception.f_GetLine();
		ExceptionInfo += CStrNonTracked::CFormat("	Error: {}\n") << _Exception.f_GetErrorStrNonTracked();
		NMib::NSys::fg_ConsoleErrorOutput(ExceptionInfo);
	}
	catch (std::exception const& _Exception)
	{
		CStrNonTracked ExceptionInfo;
		ExceptionInfo += "Uncaught exception of type inherited from: std::exception\n";
		ExceptionInfo += CStrNonTracked::CFormat("	Error: {}\n") << _Exception.what();
		NMib::NSys::fg_ConsoleErrorOutput(ExceptionInfo);
	}
	catch (...)
	{
		CStrNonTracked ExceptionInfo;
		ExceptionInfo += "Uncaught exception of type: Unknown\n";
		NMib::NSys::fg_ConsoleErrorOutput(ExceptionInfo);
	}
}

void fg_UnexpectedExceptionHandler()
{
	fg_ReportCurrentException();
	abort();
}

void fg_TerminateHandler()
{
	fg_ReportCurrentException();
	abort();
}


void NSys::fg_CreateSystem()
{
	if (g_bCreatedSystem)
		return;
	
#ifdef DMibConfig_OverrideSystemMalloc
	g_bMemoryManagerNeededAfterDestroy = true;
#endif
	g_bCreatedSystem = true;

	NMib::NSys::NPrivate::g_PageSize = getpagesize();

	// Cannot use malloc here
	
#ifdef DMibDynamicLibrary
	g_bIsSharedLibrary = true;
#else
	g_bIsSharedLibrary = false;
#endif

	if (!g_bIsSharedLibrary)
		signal(SIGPIPE,SIG_IGN);
	
	g_VirtualMap.f_Construct();
	
	if (g_OperatingSystemMajor < 0)
	{
		int Dummy;
		EOperatingSystemArch Arch;
		
		if (!fg_System_GetOperatingSystemVersion(Dummy, Dummy, Dummy, Arch))
			DMibPDebugBreak; // Not supported
	}

	g_bCreatingSystemDone = true;
	
	auto pSystemMemory = (void *)NMib::g_SystemMemory;
	auto pSystem = new(pSystemMemory) CSystemLinux();
	static_assert(NTraits::TCAlignmentOf<CSystemLinux>::mc_Value <= mint(DMibPMemoryCacheLineSize), "Aligment error");
	
	NSys::fg_Compiler_MakeActive(&pSystemMemory);
	NSys::fg_Compiler_MakeActive(&pSystem);
	DMibFastCheck((void *)pSystem == pSystemMemory);
	
	std::set_unexpected(&fg_UnexpectedExceptionHandler);
	std::set_terminate(&fg_TerminateHandler);
	
#ifndef DMibPAutomaticSystemCreation
	NMib::g_pSys = pSystem;
#endif
	
	g_bCanUseSystemMalloc = true;

	// Do stack trace to init pthread_once for backtrace
	mint Trace[2];
	backtrace((void**)Trace, 2);
	g_bCanStackTrace = true;
	
	// Can use malloc from here on
	
	NPrivate::fg_InitBaseModuleAddress();
	NLocal::fg_GetSymbols(); // This uses malloc so needs to be run after memory manager is initialized
	NPrivate::fg_SetupLimits();

	if (!g_bIsSharedLibrary) // Only use pthread_atfork in non-dylibs as atfork handlers cannot be unregistered before dlclose
		pthread_atfork(&CSystemLinux::fs_ForkPrepare, &CSystemLinux::fs_ForkParent, &CSystemLinux::fs_ForkChild);
	
	//atexit(&fg_DestroySystemAtExit);

	fg_LoadLibraries();
	
	fg_InitBreakpad();
	
	setlinebuf(stdout); // Default to line buffered output
	setlinebuf(stderr); // Default to line buffered output
	
	{
		TCVector<CStr> Parameters = fg_GetCommandLineParams<CStr>();
		
		auto iParam = Parameters.f_GetIterator();
		for (; iParam; ++iParam)
		{			
			if (iParam->f_StartsWith("--OutputPID"))
			{			
				CStr Error;
				if (iParam->f_StartsWith("--OutputPID "))
				{				
					CStr PipeNames = iParam->f_Extract(fg_StrLen("--OutputPID "));					
					CStr StdErrPipeName = NMib::NFile::CFile::fs_GetPath(PipeNames) + "/NamePipeStdErr_" + NMib::NFile::CFile::fs_GetFile(PipeNames);
					CStr StdInPipeName = NMib::NFile::CFile::fs_GetPath(PipeNames) + "/NamePipeStdIn_" + NMib::NFile::CFile::fs_GetFile(PipeNames);

					// Std in pipe has to be opened first, otherwise it might hang
					if (!StdErrPipeName.f_IsEmpty())
					{					
						int Pipe = open(StdErrPipeName.f_GetStr(), O_CLOEXEC|O_WRONLY, S_IWUSR);
						
						if (Pipe == -1)
						{
							Error = NMib::NPlatform::fg_FormatErrno("open (named stderr pipe)", errno);
						}
						else
						{						
							if (dup2(Pipe, 2) == -1)
							{
								Error = NMib::NPlatform::fg_FormatErrno("dup2 (named stderr pipe)", errno);
							}
						}
					}
					
					DMibConOutRaw((CFStr256::CFormat("bdda0079-b6eb-41ac-88d0-01b50e8be939 {nfh} {}\n") << (mint)getpid() << Error.f_ReplaceChar('\n', '\r')).f_GetStr());

					if (!StdInPipeName.f_IsEmpty())
					{
						int Pipe = open(StdInPipeName.f_GetStr(), O_CLOEXEC|O_RDONLY, S_IRUSR);
							
						if (Pipe == -1)
						{
							Error = NMib::NPlatform::fg_FormatErrno("open (named stdin pipe)", errno);
						}
						else
						{
							if (dup2(Pipe, 0) == -1)
							{
								Error = NMib::NPlatform::fg_FormatErrno("dup2 (named stdin pipe)", errno);
							}
						}
					}


				}
				else
					DMibConOutRaw((CFStr256::CFormat("bdda0079-b6eb-41ac-88d0-01b50e8be939 {nfh} {}\n") << (mint)getpid() << Error.f_ReplaceChar('\n', '\r')).f_GetStr());

				
				break;
			}
		}
	}
	
	pSystem->f_LoadLibraries();
	
	pSystem->f_Init();
	
	pSystem->f_InitModule();
	pSystem->f_InitModuleThreaded();
	
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
			return;
		pSys->~CSystemLinux();

		g_VirtualMap.f_Destruct();
		g_VirtualMapLock.f_Destruct();
		
		if (!g_bMemoryManagerNeededAfterDestroy)
		{
			g_EventEmulationPool.f_Destruct();
			g_ImpSemaphorePool.f_Destruct();
		}
	}
}

bint NSys::fg_System_BeingDebugged()
{
	CStrNonTracked StatusPath = CStrNonTracked::CFormat("/proc/{}/status") << (mint)getpid();
	auto FileData = fg_ReadProcFS<CStrNonTracked>(StatusPath);
	
	auto pParse = FileData.f_GetArray();
	
	while (*pParse)
	{
		auto *pStart = pParse;
		fg_ParseToEndOfLine(pParse);
		if (fg_StrStartsWith(pStart, "TracerPid:"))
		{
			pStart += fg_StrLen("TracerPid:");
			if (pStart < pParse)
			{
				fg_ParseWhiteSpace(pStart);
				if (fg_CharIsNumber(*pStart))
				{
					CFStr256 Str(pStart, pParse-pStart);
					
					uint32 PID = Str.f_ToInt(uint32(0));
					
					if (PID)
						return true;
				}
			}
		}
		fg_ParseEndOfLine(pParse);
	}
	
	return false;
}

namespace
{
	CStr fg_GetProgramUserName()
	{
		NMib::NStr::CStr FileName = NMib::NFile::CFile::fs_GetFileNoExt(NSys::NFile::fg_GetProgramPath());
		aint iFind = FileName.f_FindReverse("_x");
		if (iFind >= 0)
			FileName = FileName.f_Left(iFind);
		
		return "." + FileName;
	}

	CStrNonTracked fg_GetProgramUserNameNonTracked()
	{
		NMib::NStr::CStrNonTracked FileName = NMib::NFile::CFile::fs_GetFileNoExt(NSys::NFile::fg_GetProgramPathNonTracked());
		aint iFind = FileName.f_FindReverse("_x");
		if (iFind >= 0)
			FileName = FileName.f_Left(iFind);
		
		return "." + FileName;
	}
}

#include <pwd.h>

NMib::NStr::CStr NSys::NFile::fg_GetUserHomeDirectory()
{
	NMib::NStr::CStr HomeDir = fg_GetSys()->f_GetEnvironmentVariable("HOME");
	
	if (!HomeDir.f_IsEmpty())
		return HomeDir;

	struct passwd *pPasswd = getpwuid(getuid());
	if (pPasswd && pPasswd->pw_dir && pPasswd->pw_dir[0])
		return CStr(pPasswd->pw_dir);
	
	return fg_GetProgramDirectory();
}

NMib::NStr::CStr NSys::NFile::fg_GetLogDirectory()
{
	return fg_GetUserLocalProgramDirectory();
}

NMib::NStr::CStrNonTracked NSys::NFile::fg_GetLogDirectoryNonTracked()
{
	return fg_GetUserLocalProgramDirectoryNonTracked();
}

NMib::NStr::CStrNonTracked NSys::NFile::fg_GetUserHomeDirectoryNonTracked()
{
	NMib::NStr::CStrNonTracked HomeDir = fg_Process_GetEnvironmentVariable_NonProtected(CStrNonTracked("HOME"));
	
	if (!HomeDir.f_IsEmpty())
		return HomeDir;
	
	struct passwd *pPasswd = getpwuid(getuid());
	if (pPasswd && pPasswd->pw_dir && pPasswd->pw_dir[0])
		return CStrNonTracked(pPasswd->pw_dir);

	return fg_GetProgramDirectoryNonTracked();
}

NMib::NStr::CStr NSys::NFile::fg_GetUserLocalProgramDirectory()
{
	NMib::NStr::CStr FileName = fg_GetProgramUserName();

	return NMib::NFile::CFile::fs_AppendPath(fg_GetUserHomeDirectory(), FileName);
}

NMib::NStr::CStr NSys::NFile::fg_GetUserLocalProgramCacheDirectory()
{
	NMib::NStr::CStr FileName = fg_GetProgramUserName();

	return NMib::NFile::CFile::fs_AppendPath(fg_GetUserHomeDirectory(), FileName);
}

NMib::NStr::CStrNonTracked NSys::NFile::fg_GetUserLocalProgramDirectoryNonTracked()
{
	NMib::NStr::CStrNonTracked FileName = fg_GetProgramUserNameNonTracked();

	return NMib::NFile::CFile::fs_AppendPath(fg_GetUserHomeDirectoryNonTracked(), FileName);
}

NMib::NStr::CStrNonTracked NSys::NFile::fg_GetUserLocalProgramCacheDirectoryNonTracked()
{
	NMib::NStr::CStrNonTracked FileName = fg_GetProgramUserNameNonTracked();

	return NMib::NFile::CFile::fs_AppendPath(fg_GetUserHomeDirectoryNonTracked(), FileName);
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


namespace NMib
{
	namespace NSys
	{
		namespace NFile
		{
			template <typename tf_CStr>
			tf_CStr fg_GetProgramDirectoryGeneral()
			{
				tf_CStr ExePath = typename tf_CStr::CFormat("/proc/{}/exe") << (mint)getpid();
				tf_CStr FullPath = fg_ResolveSymbolicLink<tf_CStr>(ExePath);
				return NMib::NFile::CFile::fs_GetPath(FullPath);
			}
			template <typename tf_CStr>
			tf_CStr fg_GetProgramPathGeneral()
			{
				tf_CStr ExePath = typename tf_CStr::CFormat("/proc/{}/exe") << (mint)getpid();
				tf_CStr FullPath = fg_ResolveSymbolicLink<tf_CStr>(ExePath);
				return FullPath;
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
	{
		if ((mint)ModuleInfo.dli_fbase == g_MainModuleBase)
			return fg_GetProgramPath();
		return ModuleInfo.dli_fname;
	}
	else
		return NMib::NStr::CStr();
}

NMib::NStr::CStrNonTracked NSys::NFile::fg_GetModulePathNonTracked(void *_pCode)
{
	Dl_info ModuleInfo;
	if (dladdr(_pCode, &ModuleInfo) != 0)
	{
		if ((mint)ModuleInfo.dli_fbase == g_MainModuleBase)
			return fg_GetProgramPathNonTracked();
		return ModuleInfo.dli_fname;
	}
	else
		return NMib::NStr::CStrNonTracked();
}

void NSys::NFile::fg_Copy(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo)
{
	NMib::NFile::CFile::fs_CopyFileRaw(_FileFrom, _FileTo); // No good way to do this on Linux unless you have kernel 2.6.33 or later (sendfile)
	
	// Copy the attributes
	{
		NMib::NFile::EFileAttrib Attribs = NMib::NFile::CFile::fs_GetAttributes(_FileFrom);
		NMib::NFile::CFile::fs_SetAttributes(_FileTo, Attribs);
	}
}


void NSys::NFile::fg_Rename(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo)
{
	if (rename(_FileFrom, _FileTo))
	{
		auto ErrNo = errno;
		if (ErrNo == EXDEV)
		{
			// Trying to rename between volumes, retry with copy/delete only if file
			auto Attribs = NMib::NFile::CFile::fs_GetAttributes(_FileFrom);
			if (!(Attribs & NMib::NFile::EFileAttrib_Directory) && !(Attribs & NMib::NFile::EFileAttrib_Link))
			{
				NMib::NFile::CFile::fs_CopyFile(_FileFrom, _FileTo);
				NMib::NFile::CFile::fs_DeleteFile(_FileFrom);
				return;
			}
		}
		DMibErrorFile(NMib::NPlatform::fg_FormatErrno(CStr::CFormat("rename('{}', '{}')") << _FileFrom << _FileTo, ErrNo));
	}
}

void NSys::NFile::fg_AtomicReplace(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo)
{
	return fg_Rename(_FileFrom, _FileTo);
}

void NSys::NFile::fg_Copy(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo, NMib::NFile::CFileProgress &_Progress)
{
	NMib::NFile::CFile::fs_CopyFileRaw(_FileFrom, _FileTo); // No good way to do this on Linux unless you have kernel 2.6.33 or later (sendfile)
	
	// Copy the attributes
	{
		NMib::NFile::EFileAttrib Attribs = NMib::NFile::CFile::fs_GetAttributes(_FileFrom);
		NMib::NFile::CFile::fs_SetAttributes(_FileTo, Attribs);
	}
}

void NSys::NFile::fg_Rename(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo, NMib::NFile::CFileProgress &_Progress)
{
	return fg_Rename(_FileFrom, _FileTo);
}

void *NSys::NFile::fg_ChangeNotification_Open(const CStr &_FileName, NMib::NFile::EFileChange _OpenFlags, NMib::NThread::CSemaphoreReportableAggregate *_pReportTo)
{
	return fg_GetLocalSys()->m_FileChangeNotificationContext->f_Open(_FileName, _OpenFlags, _pReportTo);
}

void NSys::NFile::fg_ChangeNotification_Close(void *_pNotification)
{
	fg_GetLocalSys()->m_FileChangeNotificationContext->f_Close(_pNotification);
}

bint NSys::NFile::fg_ChangeNotification_Changed(void *_pNotification)
{
	return fg_GetLocalSys()->m_FileChangeNotificationContext->f_Changed(_pNotification);
}

bint NSys::NFile::fg_ChangeNotification_GetNotification(void *_pNotification, NMib::NStr::CStr &_Path, NMib::NFile::EFileChangeNotification &_Notification, NMib::NStr::CStr &_PathFrom)
{
	return fg_GetLocalSys()->m_FileChangeNotificationContext->f_GetNotification(_pNotification, _Path, _Notification, _PathFrom);
}

bool NSys::NFile::fg_ChangeNotification_Supported()
{
	return NLocal::g_f_inotify_init && NLocal::g_f_inotify_rm_watch && NLocal::g_f_inotify_add_watch;
}

// *************************************************************************************************************************
// Net Implementation
// *************************************************************************************************************************

#include "Malterlib_Core_PlatformImp_Linux_Net.imp.h"

NSys::NNet::CAddress NSys::NNet::fg_CreateAddress(::NMib::NNet::ENetAddressType _Type, void const* _pData, mint _nDataBytes)
{
	return (NSys::NNet::CAddress)fg_GetLocalSys()->m_SocketContext->f_CreateAddress(_Type, _pData, _nDataBytes);
}

NSys::NNet::CAddress NSys::NNet::fg_DuplicateAddress(NSys::NNet::CAddress _Address)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return (NSys::NNet::CAddress)fg_GetLocalSys()->m_SocketContext->f_DuplicateAddress(*(CPOSIXAddress*)_Address);
}

::NMib::NNet::ENetAddressType NSys::NNet::fg_GetAddressType(NSys::NNet::CAddress _Address)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_GetAddressType(*(CPOSIXAddress*)_Address);
}

bint NSys::NNet::fg_GetAddressRaw(NSys::NNet::CAddress _Address, ::NMib::NNet::ENetAddressType _ExpectedType, void* _opRawData, mint _nDataBytes)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_GetAddressRaw(*(CPOSIXAddress*)_Address, _ExpectedType, _opRawData, _nDataBytes);
}

NSys::NNet::CAddress NSys::NNet::fg_SetAddressRaw(NSys::NNet::CAddress _Address, ::NMib::NNet::ENetAddressType _Type, void const* _pRawData, mint _nDataBytes)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return (NSys::NNet::CAddress)fg_GetLocalSys()->m_SocketContext->f_SetAddressRaw((CPOSIXAddress*)_Address, _Type, _pRawData, _nDataBytes);
}

NSys::NNet::CAddress NSys::NNet::fg_ResolveAddress(const NMib::NStr::CStr &_Address, ::NMib::NNet::ENetAddressType _PreferType)
{
	return fg_GetLocalSys()->m_SocketContext->f_ResolveAddress(_Address, _PreferType);
}

mint NSys::NNet::fg_GetMaxUnixSocketNameLength()
{
	return sizeof(sockaddr_un::sun_path) - 1;
}

void *NSys::NNet::fg_AsyncResolveAddress_Open(const NMib::NStr::CStr &_Address, ::NMib::NNet::ENetAddressType _PreferType, NMib::NFunction::TCFunction<void ()>&& _fOnFinish)
{
	return fg_GetLocalSys()->m_SocketContext->f_AsyncResolveAddress_Open(_Address, _PreferType, fg_Move(_fOnFinish));
}

bint NSys::NNet::fg_AsyncResolveAddress_GetResult(void *_pResolver, NSys::NNet::CAddress& _opAddress, NMib::NStr::CStr &_Error)
{
	return fg_GetLocalSys()->m_SocketContext->f_AsyncResolveAddress_GetResult(_pResolver, (CPOSIXAddress*&)_opAddress, _Error);
}

void NSys::NNet::fg_AsyncResolveAddress_Close(void *_pResolver)
{
	fg_GetLocalSys()->m_SocketContext->f_AsyncResolveAddress_Close(_pResolver);
}

int NSys::NNet::fg_CompareAddresses(NSys::NNet::CAddress _pFirst, NSys::NNet::CAddress _pSecond)
{
	DMibSafeCheck(_pFirst != nullptr, "Address is null!");
	DMibSafeCheck(_pSecond != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_CompareAddresses(*(CPOSIXAddress*)_pFirst, *(CPOSIXAddress*)_pSecond);
}

void NSys::NNet::fg_FreeAddress(NSys::NNet::CAddress _Address) // It is OK to free a nullptr address
{
	return fg_GetLocalSys()->m_SocketContext->f_FreeAddress((CPOSIXAddress*)_Address);
}

NMib::NStr::CStr NSys::NNet::fg_GetAddressString(NSys::NNet::CAddress _Address, bint _bIncludeType)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_GetAddressString(*(CPOSIXAddress*)_Address, _bIncludeType);
}

// Connection Operations
void *NSys::NNet::fg_Connect(NSys::NNet::CAddress _Address, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange, NSys::NNet::CAddress _BindAddress) // Report to the supplied event when new data is received or when we are ready to send new dat
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_Connect(*(CPOSIXAddress*)_Address, fg_Move(_OnStateChange), (CPOSIXAddress*)_BindAddress);
}

void *NSys::NNet::fg_AsyncConnect(NSys::NNet::CAddress _Address, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange, NSys::NNet::CAddress _BindAddress) // Report to the supplied event when new data is received or when we are ready to send new data and when the connection is connecte
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_AsyncConnect(*(CPOSIXAddress*)_Address, fg_Move(_OnStateChange), (CPOSIXAddress*)_BindAddress);
}

void *NSys::NNet::fg_Listen(NSys::NNet::CAddress _Address, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange, NMib::NNet::ENetFlag _Flags) // Report to the supplied event when a new connection has arrive
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_Listen(*(CPOSIXAddress*)_Address, fg_Move(_OnStateChange), _Flags);
}

void *NSys::NNet::fg_ListenDatagram(NSys::NNet::CAddress _Address, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange, NMib::NNet::ENetFlag _Flags)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_ListenDatagram(*(CPOSIXAddress*)_Address, fg_Move(_OnStateChange), _Flags);
}

void *NSys::NNet::fg_Accept(void *_pSocket, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange) // Report to the supplied event when new data is received or when we are ready to send new dat
{
	return fg_GetLocalSys()->m_SocketContext->f_Accept((CPOSIXSocket*)_pSocket, fg_Move(_OnStateChange));
}

void NSys::NNet::fg_Close(void *_pSocket) // Closes the socket and connectio
{
	fg_GetLocalSys()->m_SocketContext->f_Close((CPOSIXSocket*)_pSocket);
}

void NSys::NNet::fg_Shutdown(void *_pSocket)
{
	fg_GetLocalSys()->m_SocketContext->f_Shutdown((CPOSIXSocket*)_pSocket);
}

mint NSys::NNet::fg_Receive(void *_pSocket, void *_pData, mint _DataLen) // Returns bytes receive
{
	return fg_GetLocalSys()->m_SocketContext->f_Receive((CPOSIXSocket*)_pSocket, _pData, _DataLen);
}

mint NSys::NNet::fg_Send(void *_pSocket, const void *_pData, mint _DataLen) // Returns bytes sen
{
	return fg_GetLocalSys()->m_SocketContext->f_Send((CPOSIXSocket*)_pSocket, _pData, _DataLen);
}

mint NSys::NNet::fg_SendDatagram(void *_pSocket, NSys::NNet::CAddress _Address, const void *_pData, mint _DataLen) // Returns bytes sen
{
	return fg_GetLocalSys()->m_SocketContext->f_SendDatagram((CPOSIXSocket*)_pSocket, *((CPOSIXAddress*)_Address), _pData, _DataLen);
}

mint NSys::NNet::fg_ReceiveDatagram(void *_pSocket, NSys::NNet::CAddress _Address, void *_pData, mint _DataLen) // Returns bytes sen
{
	return fg_GetLocalSys()->m_SocketContext->f_ReceiveDatagram((CPOSIXSocket*)_pSocket, *((CPOSIXAddress*)_Address), _pData, _DataLen);
}

// Socket Properties & State

void NSys::NNet::fg_SetOnStateChange(void *_pSocket, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange) // Report to the supplied event when new data is received or when we are ready to send new data			
{
	fg_GetLocalSys()->m_SocketContext->f_SetOnStateChange((CPOSIXSocket*)_pSocket, fg_Move(_OnStateChange));
}

NMib::NNet::ENetTCPState NSys::NNet::fg_GetState(void *_pSocket) // Get the state of data availabl
{
	return fg_GetLocalSys()->m_SocketContext->f_GetState((CPOSIXSocket*)_pSocket);
}

NMib::NStr::CStr NSys::NNet::fg_GetCloseReason(void *_pSocket)
{
	return fg_GetLocalSys()->m_SocketContext->f_GetCloseReason((CPOSIXSocket*)_pSocket);
}

void *NSys::NNet::fg_InheritHandle2(void *_pSocket, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange)
{
	return fg_GetLocalSys()->m_SocketContext->f_InheritHandle2((CPOSIXSocket*)_pSocket, fg_Move(_OnStateChange));
}

void *NSys::NNet::fg_GiveUpForInherit(void *_pSocket)
{
	return fg_GetLocalSys()->m_SocketContext->f_GiveUpForInherit((CPOSIXSocket*)_pSocket);
}

void *NSys::NNet::fg_GetOSSocket(void *_pSocket)
{
	return fg_GetLocalSys()->m_SocketContext->f_GetOSSocket((CPOSIXSocket*)_pSocket);
}

NSys::NNet::CAddress NSys::NNet::fg_GetPeerAddress(void *_pSocket)
{
	return (NSys::NNet::CAddress)fg_GetLocalSys()->m_SocketContext->f_GetPeerAddress((CPOSIXSocket*)_pSocket);
}

uint32 NSys::NNet::fg_GetListenPort(void *_pSocket)
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

void NSys::fg_Thread_Yield()
{
	sched_yield();
}

#include <locale.h>

uint16 NSys::fg_Langague_GetSystemLanguage(NMib::NStr::CStr &_Language)
{
	_Language = setlocale(LC_CTYPE, NULL);
	aint DotPos = _Language.f_Find(".");
	if (DotPos != -1)
		_Language = _Language.f_Left(DotPos);
	_Language = _Language.f_Replace("_", "-");
	return 0;
}

void* NSys::fg_GetExeData(char const* _pSegment, char const* _pSection, unsigned long long& _nDataBytes)
{
	DMibPDebugBreak;
	
	return nullptr;
}


mint NSys::fg_Thread_GetVirtualCores()
{
	return fg_Max(sysconf(_SC_NPROCESSORS_ONLN), 1);
}

mint NSys::fg_Thread_GetPhysicalCores()
{
	return fg_Max(sysconf(_SC_NPROCESSORS_ONLN), 1);
}

struct CCpuNameCache
{
	NAtomic::TCAtomic<int32> m_Finished;
	CStr m_Name;
};

NAggregate::TCAggregate<CCpuNameCache> g_CPUNameCache = {DAggregateInit};

NMib::NStr::CStr NSys::fg_System_GetCPUName()
{
	auto &Cache = *g_CPUNameCache;
	if (Cache.m_Finished.f_Load() == 2)
		return Cache.m_Name;
	
	int32 Expected = 0;
	if (Cache.m_Finished.f_CompareExchangeStrong(Expected, 1))
	{
		Cache.m_Name = "Unknown";
		
		auto FileData = fg_ReadProcFS<CStrNonTracked>("/proc/cpuinfo");
		auto pParse = FileData.f_GetArray();
		
		while (*pParse)
		{
			auto *pStart = pParse;
			fg_ParseToEndOfLine(pParse);
			if (fg_StrStartsWith(pStart, "model name"))
			{
				pStart += fg_StrLen("model name");
				if (pStart < pParse)
				{
					fg_ParseWhiteSpace(pStart);
					if (*pStart == ':')
						++pStart;
					fg_ParseWhiteSpace(pStart);
					CStr Str(pStart, pParse-pStart);
					Cache.m_Name = Str;
				}
				break;
			}
			fg_ParseEndOfLine(pParse);
		}
		Cache.m_Finished.f_Exchange(2);
	}
	else
	{
		while (Cache.m_Finished.f_Load() != 2)
			fg_Thread_SmallestSleep();
	}
	return Cache.m_Name;
}


namespace
{
	class CCPUUsageMonitorImpl
	{
		struct CLoadInfo
		{
			uint64 m_User;
			uint64 m_Nice;
			uint64 m_System;
			uint64 m_Idle;
		};
		CLoadInfo m_LastLoadStats;
		
		NMib::NSystem::CSystemCPUUsage m_LastRet;
		
		CLoadInfo fp_GetLoadStats() const
		{
			CLoadInfo Ret;
			fg_MemClear(Ret);
			
			auto FileData = fg_ReadProcFS<CStrNonTracked>("/proc/stat");
			auto pParse = FileData.f_GetArray();
			
			while (*pParse)
			{
				auto *pStart = pParse;
				fg_ParseToEndOfLine(pParse);
				if (fg_StrStartsWith(pStart, "cpu "))
				{
					pStart += fg_StrLen("cpu ");
					if (pStart < pParse)
					{
						fg_ParseWhiteSpace(pStart);
						(CStr::CParse("{} {} {} {}") >> Ret.m_User >> Ret.m_Nice >> Ret.m_System >> Ret.m_Idle).f_Parse(pStart);
					}
					break;
				}
				fg_ParseEndOfLine(pParse);
			}
			return Ret;
		}
		
	public:
		CCPUUsageMonitorImpl()
		{
			m_LastLoadStats = fp_GetLoadStats();
			
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
			CLoadInfo LoadStats = fp_GetLoadStats();
			
			uint64 IdleTime = LoadStats.m_Idle;
			uint64 UserTime = LoadStats.m_User + LoadStats.m_Nice;
			uint64 KernelTime = LoadStats.m_System;
			
			uint64 LastIdleTime = m_LastLoadStats.m_Idle;
			uint64 LastUserTime = m_LastLoadStats.m_User + m_LastLoadStats.m_Nice;
			uint64 LastKernelTime = m_LastLoadStats.m_System;
			
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
			NPtr::TCUniquePointer<CCPUUsageMonitorImpl> pMonitor = fg_Construct();
			
			return pMonitor.f_Detach();
		}
		
		void fg_System_CPUUsageMonitor_Close(void *_pHandle)
		{
			NPtr::TCUniquePointer<CCPUUsageMonitorImpl> pMonitor = fg_Explicit((CCPUUsageMonitorImpl *)_pHandle);
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

void NSys::fg_TerminateProcess(aint _ExitCode)
{
//	fflush(stdout);
//	fflush(stderr);
//	_exit(_ExitCode);
	raise(SIGKILL);
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
	return ".so";
}

bint NMib::NSys::fg_ConsoleOutputValid()
{
	return true;
}

bint NMib::NSys::fg_ConsoleInputValid()
{
	return true;
}

bint NMib::NSys::fg_ConsoleErrorOutputValid()
{
	return true;
}

namespace NMib
{
	namespace NSys
	{
		
		
		/*
		 Basic interface for storing secure passwords on a per-user, per-application basis.
		 */
		
		ESecurePassword fg_SecurePassword_SetLocation(NMib::NStr::CStr const& _Location)
		{
			return fg_GetLocalSys()->f_GetPasswordManager()->f_SecurePassword_SetLocation(_Location);
		}
		
		ESecurePassword fg_SecurePassword_Store(CStr const& _Key, CStrSecure const& _Password)
		{
			return fg_GetLocalSys()->f_GetPasswordManager()->f_SecurePassword_Store(_Key, _Password);
		}
		
		ESecurePassword fg_SecurePassword_Remove(CStr const& _Key)
		{
			return fg_GetLocalSys()->f_GetPasswordManager()->f_SecurePassword_Remove(_Key);
		}
		
		ESecurePassword fg_SecurePassword_Get(CStr const& _Key, CStrSecure& _oPassword)
		{
			return fg_GetLocalSys()->f_GetPasswordManager()->f_SecurePassword_Get(_Key, _oPassword);
		}
		
		ESecurePassword fg_SecurePassword_Exists(CStr const& _Key)
		{
			return fg_GetLocalSys()->f_GetPasswordManager()->f_SecurePassword_Exists(_Key);
		}		
		
		bool fg_SecurePassword_Supported()
		{
			return fg_GetLocalSys()->f_GetPasswordManager()->f_SecurePassword_Supported();
		}


		// Desktop environment
		EDesktopEnvironment fg_DesktopEnvironment_Get()
		{
			return fg_GetLocalSys()->m_DesktopEnvironment;
		}

	}
}

#ifdef DMibUseGoldLinker
#if 0
	#ifdef __amd64__
		#define DDefineGLibCSymbolCompatible(d_Symbols) __asm__(".symver __real_" #d_Symbols ",__real_" #d_Symbols "@GLIBC_2.2.5")
	#elif defined(__i386__)
		#define DDefineGLibCSymbolCompatible(d_Symbols) __asm__(".symver __real_" #d_Symbols ",__real_" #d_Symbols "@GLIBC_2.0")
	#else
		#error "Implement this"
	#endif

	extern "C" void *__real_memcpy (void *__restrict __dest, __const void *__restrict __src, __SIZE_TYPE__ __n);
	DDefineGLibCSymbolCompatible(memcpy);
#endif

	extern "C" void *__wrap_memcpy (void *__restrict __dest, __const void *__restrict __src, __SIZE_TYPE__ __n)
	{
		return NLocal::g_f_memcpy(__dest, __src, __n); // Due to bug in gold linker we can't get correct version of memcpy
		//return __real_memcpy(__dest, __src, __n);
	}

#else

	#ifdef __amd64__
		#define DDefineGLibCSymbolCompatible(d_Symbols) __asm__(".symver " #d_Symbols "," #d_Symbols "@GLIBC_2.2.5")
	#elif defined(__i386__)
		#define DDefineGLibCSymbolCompatible(d_Symbols) __asm__(".symver " #d_Symbols "," #d_Symbols "@GLIBC_2.0")
	#else
		#error "Implement this"
	#endif

	DDefineGLibCSymbolCompatible(memcpy);

	extern "C" void *__wrap_memcpy (void *__restrict __dest, __const void *__restrict __src, __SIZE_TYPE__ __n)
	{
		return memcpy(__dest, __src, __n);
	}
#endif

namespace NMib
{
	namespace NSys
	{
		void * g_pCrossModuleMemoryManagerInterface = nullptr;
	}
}

#ifndef DMibDynamicLibrary

// This needs to be named exactly like this to be compatible with old versions of library (when Malterlib was named Ids)
extern "C" module_export void *fg_IdsGetCrossModuleMemoryManagerInterface()
{
//	DMibTraceSafe("fg_IdsGetCrossModuleMemoryManagerInterface({}, {}) = {}\n", &fg_IdsGetCrossModuleMemoryManagerInterface << fg_GetSys()->f_IsDll() << NMib::NSys::g_pCrossModuleMemoryManagerInterface);
	return NMib::NSys::g_pCrossModuleMemoryManagerInterface;
}

#endif

void *NSys::fg_Process_GetCrossModuleMemoryManagerInterface()
{
#ifdef DMibDynamicLibrary
	void * ( *fMalterlibGetCrossModuleMemoryManagerInterface)();
	(void * &)fMalterlibGetCrossModuleMemoryManagerInterface = dlsym(RTLD_DEFAULT, "fg_IdsGetCrossModuleMemoryManagerInterface");
	
	auto pRet = g_pCrossModuleMemoryManagerInterface;
	
	if (fMalterlibGetCrossModuleMemoryManagerInterface)
		pRet = fMalterlibGetCrossModuleMemoryManagerInterface();
	
//	DMibTraceSafe("fg_Process_GetCrossModuleMemoryManagerInterface({}) = {}\n", fg_GetSys()->f_IsDll() << pRet);
	return pRet;
#else
	return fg_IdsGetCrossModuleMemoryManagerInterface();
#endif
}

void NSys::fg_Process_SetCrossModuleMemoryManagerInterface(void *_pInterface)
{
	DMibFastCheck(!fg_GetSys()->f_IsDll());
	
	g_pCrossModuleMemoryManagerInterface = _pInterface;
	
	DMibFastCheck(fg_Process_GetCrossModuleMemoryManagerInterface() == _pInterface);
}

#include <stdarg.h>

extern "C" int __isoc99_sscanf(const char *s, const char *format, ...)
{
	va_list arg;
	int done;
	va_start(arg, format);
	done = vsscanf(s, format, arg);
	va_end(arg);
	return done;
}

