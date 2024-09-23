// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define _LIBCPP_ENABLE_CXX17_REMOVED_UNEXPECTED_FUNCTIONS

#include <Mib/Core/Core>

#include <Mib/Core/DynamicLibrary>
#include <Mib/Desktop/DBus>
#include <Mib/Desktop/DesktopFile>
#include <Mib/Cryptography/UUID>
#include <Mib/String/AnsiConversion>

#include "Malterlib_Core_PlatformImp_Linux.h"
#include "Malterlib_Core_PlatformImp_Linux_FileNotification.h"
#include "Malterlib_Core_PlatformImp_Linux_SecurePassword.h"

#define DMibAllowCodeStandardViolations 1

#include <Mib/Concurrency/ThreadSafeQueue>
#include <Mib/Core/PlatformSpecific/PosixUser>

#include <malloc.h>
#include <unwind.h>
#include <linux/futex.h>
#include <unistd.h>
#include <sys/types.h>
#include <spawn.h>
#include <glob.h>
#include <sys/syscall.h>
#include <exception>
#include <utility>

using namespace NMib;
using namespace NMib::NStr;
using namespace NMib::NTime;
using namespace NMib::NMemory;
using namespace NMib::NContainer;

namespace NLocal
{
	int (*g_f_pipe2)(int __pipedes[2], int __flags) __THROW __wur = nullptr;
	int (*g_f_accept4)(int __fd, __SOCKADDR_ARG __addr, socklen_t *__restrict __addr_len, int __flags) = nullptr;
	int (*g_f_inotify_init1)(int __flags) __THROW = nullptr;
	int (*g_f_inotify_init)(void) __THROW = nullptr;
	int (*g_f_inotify_add_watch)(int __fd, const char *__name, uint32_t __mask) __THROW = nullptr;
	int (*g_f_inotify_rm_watch)(int __fd, int __wd) __THROW = nullptr;
	int (*g_f_pthread_setname_np)(pthread_t __target_thread, __const char *__name) = nullptr;
	void *(*g_f_memcpy)(void *__restrict __dest, __const void *__restrict __src, __SIZE_TYPE__ __n) = &memmove;
	int (*g_f_utimensat)(int dirfd, const char *pathname, const struct timespec times[2], int flags) = nullptr;
	int (*g_f_futimens)(int fd, const struct timespec times[2]) = nullptr;
	ssize_t (*g_f_getrandom)(void *buf, size_t buflen, unsigned int flags);

	int (*g_f_posix_spawn_file_actions_addchdir_np)(posix_spawn_file_actions_t *__restrict __actions, const char *__restrict __path) = nullptr;

	void *(*__real_versioned_memcpy_new)(void *__restrict __dest, __const void *__restrict __src, __SIZE_TYPE__ __n) = nullptr;

	pfp64 (*__real_versioned_exp_new)(pfp64 _Value) = nullptr;
	pfp64 (*__real_versioned_exp2_new)(pfp64 _Value) = nullptr;
	pfp64 (*__real_versioned_log_new)(pfp64 _Value) = nullptr;
	pfp64 (*__real_versioned_log2_new)(pfp64 _Value) = nullptr;
	pfp64 (*__real_versioned_pow_new)(pfp64 _Value, pfp64 _Power) = nullptr;

	pfp32 (*__real_versioned_expf_new)(pfp32 _Value) = nullptr;
	pfp32 (*__real_versioned_exp2f_new)(pfp32 _Value) = nullptr;
	pfp32 (*__real_versioned_logf_new)(pfp32 _Value) = nullptr;
	pfp32 (*__real_versioned_log2f_new)(pfp32 _Value) = nullptr;
	pfp32 (*__real_versioned_powf_new)(pfp32 _Value, pfp32 _Power) = nullptr;

	unsigned long (*__real_getauxval)(unsigned long type) = nullptr;

	int (*__real_versioned___sched_cpucount)(size_t __setsize, const cpu_set_t *__setp) = nullptr;

	int (*__real_versioned_clock_getres)(clockid_t __clock_id, struct timespec *__res) __THROW = nullptr;
	int (*__real_versioned_clock_gettime)(clockid_t __clock_id, struct timespec *__tp) __THROW = nullptr;

	int (*__real_versioned_posix_spawn)
		(
			pid_t *pid
			, const char *path
			, const posix_spawn_file_actions_t *file_actions
			, const posix_spawnattr_t *attrp
			, char *const argv[]
			, char *const envp[]
		)
	;

	int (*__real_versioned_posix_spawnp)
		(
			pid_t *pid
			, const char *path
			, const posix_spawn_file_actions_t *file_actions
			, const posix_spawnattr_t *attrp
			, char *const argv[]
			, char *const envp[]
		)
	;

	int (*__real_versioned_glob64)(__const char *__restrict __pattern, int __flags, int (*__errfunc) (__const char *, int), glob64_t *__restrict __pglob) __THROW = nullptr;

	int (*__real_pthread_mutexattr_setrobust)(pthread_mutexattr_t *__attr, int __robustness) __THROW = nullptr;
	int (*__real_pthread_mutex_consistent)(pthread_mutex_t *__mutex) __THROW = nullptr;
	int (*__real_fcntl64)(int __fd, int __cmd, ...) = nullptr;

	int	 (*__real___isoc99_vsscanf)(const char * __restrict __str, const char * __restrict __format, va_list) = nullptr;

	namespace
	{
		bool g_bSymbolsGotten = false;
	}

	void fg_GetSymbols()
	{
		if (g_bSymbolsGotten)
			return;

		g_bSymbolsGotten = true;

		(void * &)g_f_pipe2 = dlsym(RTLD_DEFAULT, "pipe2");
		(void * &)g_f_accept4 = dlsym(RTLD_DEFAULT, "accept4");
		(void * &)g_f_inotify_init1 = dlsym(RTLD_DEFAULT, "inotify_init1");
		(void * &)g_f_inotify_init = dlsym(RTLD_DEFAULT, "inotify_init");
		(void * &)g_f_inotify_add_watch = dlsym(RTLD_DEFAULT, "inotify_add_watch");
		(void * &)g_f_inotify_rm_watch = dlsym(RTLD_DEFAULT, "inotify_rm_watch");
		(void * &)g_f_pthread_setname_np = dlsym(RTLD_DEFAULT, "pthread_setname_np");
		(void * &)g_f_memcpy = dlsym(RTLD_NEXT, "memcpy");
		(void * &)g_f_utimensat = dlsym(RTLD_DEFAULT, "utimensat");
		(void * &)g_f_futimens = dlsym(RTLD_DEFAULT, "futimens");
		(void * &)g_f_getrandom = dlsym(RTLD_DEFAULT, "getrandom");

		(void * &)__real_versioned_exp_new = dlvsym(RTLD_DEFAULT, "exp", "GLIBC_2.29");
		if (!__real_versioned_exp_new)
			(void * &)__real_versioned_exp_new = dlsym(RTLD_DEFAULT, "exp");

		(void * &)__real_versioned_exp2_new = dlvsym(RTLD_DEFAULT, "exp2", "GLIBC_2.29");
		if (!__real_versioned_exp2_new)
			(void * &)__real_versioned_exp2_new = dlsym(RTLD_DEFAULT, "exp2");

		(void * &)__real_versioned_log_new = dlvsym(RTLD_DEFAULT, "log", "GLIBC_2.29");
		if (!__real_versioned_log_new)
			(void * &)__real_versioned_log_new = dlsym(RTLD_DEFAULT, "log");

		(void * &)__real_versioned_log2_new = dlvsym(RTLD_DEFAULT, "log2", "GLIBC_2.29");
		if (!__real_versioned_log2_new)
			(void * &)__real_versioned_log2_new = dlsym(RTLD_DEFAULT, "log2");

		(void * &)__real_versioned_pow_new = dlvsym(RTLD_DEFAULT, "pow", "GLIBC_2.29");
		if (!__real_versioned_pow_new)
			(void * &)__real_versioned_pow_new = dlsym(RTLD_DEFAULT, "pow");

		(void * &)__real_versioned_expf_new = dlvsym(RTLD_DEFAULT, "expf", "GLIBC_2.27");
		if (!__real_versioned_expf_new)
			(void * &)__real_versioned_expf_new = dlsym(RTLD_DEFAULT, "expf");

		(void * &)__real_versioned_exp2f_new = dlvsym(RTLD_DEFAULT, "exp2f", "GLIBC_2.27");
		if (!__real_versioned_exp2f_new)
			(void * &)__real_versioned_exp2f_new = dlsym(RTLD_DEFAULT, "exp2f");

		(void * &)__real_versioned_logf_new = dlvsym(RTLD_DEFAULT, "logf", "GLIBC_2.27");
		if (!__real_versioned_logf_new)
			(void * &)__real_versioned_logf_new = dlsym(RTLD_DEFAULT, "logf");

		(void * &)__real_versioned_log2f_new = dlvsym(RTLD_DEFAULT, "log2f", "GLIBC_2.27");
		if (!__real_versioned_log2f_new)
			(void * &)__real_versioned_log2f_new = dlsym(RTLD_DEFAULT, "log2f");

		(void * &)__real_versioned_powf_new = dlvsym(RTLD_DEFAULT, "powf", "GLIBC_2.27");
		if (!__real_versioned_powf_new)
			(void * &)__real_versioned_powf_new = dlsym(RTLD_DEFAULT, "powf");

		(void * &)__real_getauxval = dlvsym(RTLD_DEFAULT, "getauxval", "GLIBC_2.16");
		if (!__real_getauxval)
			(void * &)__real_getauxval = dlsym(RTLD_DEFAULT, "getauxval");

		(void * &)__real_versioned___sched_cpucount = dlvsym(RTLD_DEFAULT, "__sched_cpucount", "GLIBC_2.6");
		if (!__real_versioned___sched_cpucount)
			(void * &)__real_versioned___sched_cpucount = dlsym(RTLD_DEFAULT, "__sched_cpucount");

		(void * &)__real_versioned_memcpy_new = dlvsym(RTLD_DEFAULT, "memcpy", "GLIBC_2.14");
		if (!__real_versioned_memcpy_new)
			(void * &)__real_versioned_memcpy_new = dlsym(RTLD_DEFAULT, "memcpy");

		(void * &)__real_versioned_clock_getres = dlvsym(RTLD_DEFAULT, "clock_getres", "GLIBC_2.17");
		if (!__real_versioned_clock_getres)
			(void * &)__real_versioned_clock_getres = dlsym(RTLD_DEFAULT, "clock_getres");

		(void * &)__real_versioned_clock_gettime = dlvsym(RTLD_DEFAULT, "clock_gettime", "GLIBC_2.17");
		if (!__real_versioned_clock_gettime)
			(void * &)__real_versioned_clock_gettime = dlsym(RTLD_DEFAULT, "clock_gettime");

		(void * &)__real_versioned_posix_spawn = dlvsym(RTLD_DEFAULT, "posix_spawn", "GLIBC_2.15");
		if (!__real_versioned_posix_spawn)
			(void * &)__real_versioned_posix_spawn = dlsym(RTLD_DEFAULT, "posix_spawn");

		(void * &)__real_versioned_posix_spawnp = dlvsym(RTLD_DEFAULT, "posix_spawnp", "GLIBC_2.15");
		if (!__real_versioned_posix_spawnp)
			(void * &)__real_versioned_posix_spawnp = dlsym(RTLD_DEFAULT, "posix_spawnp");

		(void * &)g_f_posix_spawn_file_actions_addchdir_np = dlsym(RTLD_DEFAULT, "posix_spawn_file_actions_addchdir_np");

		(void * &)__real_versioned_glob64 = dlvsym(RTLD_DEFAULT, "glob64", "GLIBC_2.27");
		if (!__real_versioned_glob64)
			(void * &)__real_versioned_glob64 = dlsym(RTLD_DEFAULT, "glob64");

		(void * &)__real_pthread_mutexattr_setrobust = dlsym(RTLD_DEFAULT, "pthread_mutexattr_setrobust");
		if (!__real_pthread_mutexattr_setrobust)
			(void * &)__real_pthread_mutexattr_setrobust = dlsym(RTLD_DEFAULT, "pthread_mutexattr_setrobust_np");

		(void * &)__real_pthread_mutex_consistent = dlsym(RTLD_DEFAULT, "pthread_mutex_consistent");
		if (!__real_pthread_mutex_consistent)
			(void * &)__real_pthread_mutex_consistent = dlsym(RTLD_DEFAULT, "pthread_mutex_consistent_np");

		(void * &)__real_fcntl64 = dlsym(RTLD_DEFAULT, "fcntl64");
		if (!__real_fcntl64)
			(void * &)__real_fcntl64 = dlsym(RTLD_DEFAULT, "fcntl64");

		(void * &)__real___isoc99_vsscanf = dlsym(RTLD_DEFAULT, "__isoc99_vsscanf");
		if (!__real___isoc99_vsscanf)
			(void * &)__real___isoc99_vsscanf = dlsym(RTLD_DEFAULT, "__isoc99_vsscanf");
	}
}

bool g_bIsSharedLibrary = false;

mint g_MainModuleBase = 0;

void fg_ForkPrepare();
void fg_ForkParentOrChild();
int fg_GetUnixOpenFlags();
void fg_SetUnixHandleOptions(int _File);
void fg_MalterlibMallocOverride_CanStartThreads();

#include "Malterlib_Core_Platform_POSIX_ErrNo.h"

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
#include <linux/sysctl.h>
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

[[noreturn]] void NMib::fg_NoReturn()
{
	std::unreachable();
}

class CImpSemaphore
{
public:
	NMib::NThread::CLowLevelLock m_Lock;

	sem_t m_Semaphore = {0};
	bool m_bSemaphoreInit = false;

	mint m_Value;
	mint m_Maximum;

	CImpSemaphore(mint _Value, mint _Maximum)
		: m_Value(_Value)
		, m_Maximum(_Maximum)
	{
		f_Init();
	}

	~CImpSemaphore() noexcept(false)
	{
		DMibLock(m_Lock);

		if (m_bSemaphoreInit)
		{
			if (sem_destroy(&m_Semaphore))
				DMibError(NMib::NPlatform::fg_FormatErrno("sem_destroy (semaphore destructor)", errno));
		}
	}

	void f_ForkedChild()
	{
		m_Lock.f_ForkedChildUnlocked();
		if (sem_init(&m_Semaphore, false, 0))
			DMibError(NMib::NPlatform::fg_FormatErrno("sem_init (semaphore init)", errno));
		m_bSemaphoreInit = true;
	}

	void f_Init()
	{
		m_Lock.f_Construct();
		{
			DMibLock(m_Lock);
			if (sem_init(&m_Semaphore, false, 0))
				DMibError(NMib::NPlatform::fg_FormatErrno("sem_init (semaphore init)", errno));
			m_bSemaphoreInit = true;
		}
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
	return pSemaphore->f_WaitTimeout(_Timeout * NTime::CSystem_Time::fs_GetTimeSpeedReciprocal());
}

bool NSys::fg_Semaphore_TryWait(void * _pSemaphore)
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

struct CUUIDLibrary final : public NMib::CDynamicLibraryUtility
{
	constexpr CUUIDLibrary()
		: NMib::CDynamicLibraryUtility(gc_Str<"libuuid.so.1">, EDLFlag_NoThrow)
	{
	}

	decltype(&::uuid_generate) uuid_generate = nullptr;
	decltype(&::uuid_unparse) uuid_unparse = nullptr;

protected:
	void fp_ClearSymbols() override
	{
		uuid_generate = nullptr;
		uuid_unparse = nullptr;
	}

	void fp_FetchSymbols() override
	{
		fp_Fetch(uuid_generate, "uuid_generate");
		fp_Fetch(uuid_unparse, "uuid_unparse");
	}
};

constinit TCAggregateSimple<CUUIDLibrary> g_UUIDLibrary = { DAggregateInit };

static void fg_LoadLibraries()
{
	g_UUIDLibrary.f_Construct();
	(*g_UUIDLibrary).f_Reload();
}

void CSystemLinux::fs_ForkPrepare()
{
	auto &Sys = *fg_GetLocalSys();
	if (!pthread_getspecific(Sys.m_ThreadDestructionHook))
	{
		pthread_setspecific(Sys.m_ThreadDestructionHook, (void *)(mint)getpid());
		Sys.m_Posix.f_GetMalterlibDisableStdErrLog(); // getenv fails on forked process, to workaround this here

		Sys.m_Posix.m_ForkLock.f_Lock();
		Sys.m_Posix.m_ForkLock.f_PrepareFork();
		Sys.f_PrepareFork();
		g_EventEmulationPool.f_Lock();
		g_ImpSemaphorePool.f_Lock();
	}
}

void CSystemLinux::fs_ForkParentOrChild()
{
	auto &Sys = *fg_GetLocalSys();
	void *Current = pthread_getspecific(Sys.m_ThreadDestructionHook);
	if (Current)
	{
#ifdef DMibSanitizerEnabled_Thread
		if (Current == (void *)(mint)getpid())
			__tsan_forked_parent();
		else
			__tsan_forked_child();
#endif
		pthread_setspecific(Sys.m_ThreadDestructionHook, nullptr);
		if (Current != (void *)(mint)getpid())
		{
#if !defined(DMibDynamicLibrary) || (defined(DMibDynamicLibrary) && !defined(DMibAssumeMalterlibHost))
			g_MalterlibCurrentTID = syscall(SYS_gettid);
#endif
			Sys.m_bForkedChild = true;
			g_bCanStackTrace = false;
			g_ImpSemaphorePool.f_ForkedChildLocked();
			g_EventEmulationPool.f_ForkedChildLocked();
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

void CSystemLinux::fs_ForkParent()
{
	auto &Sys = *fg_GetLocalSys();
	if (pthread_getspecific(Sys.m_ThreadDestructionHook))
	{
#ifdef DMibSanitizerEnabled_Thread
		__tsan_forked_parent();
#endif
		pthread_setspecific(Sys.m_ThreadDestructionHook, 0);
		g_ImpSemaphorePool.f_Unlock();
		g_EventEmulationPool.f_Unlock();
		Sys.f_ForkedParent();
		Sys.m_Posix.m_ForkLock.f_ForkedParent();
		Sys.m_Posix.m_ForkLock.f_Unlock();
	}
}

void CSystemLinux::fs_ForkChild()
{
	auto &Sys = *fg_GetLocalSys();
	if (pthread_getspecific(Sys.m_ThreadDestructionHook))
	{
#if !defined(DMibDynamicLibrary) || (defined(DMibDynamicLibrary) && !defined(DMibAssumeMalterlibHost))
		g_MalterlibCurrentTID = syscall(SYS_gettid);
#endif

#ifdef DMibSanitizerEnabled_Thread
		__tsan_forked_child();
#endif
		pthread_setspecific(Sys.m_ThreadDestructionHook, 0);
		Sys.m_bForkedChild = true;
		g_bCanStartThreads = false;
		g_bCanStackTrace = false;
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

CSystemLinux::CSystemLinux()
	: CSystem(g_bIsSharedLibrary)
	, m_SocketContext{DAggregateInit}
	, m_PasswordManagerCreated(0)
{
	fg_MemClear(m_SocketContext);

	auto Error = pthread_key_create(&m_ThreadDestructionHook, fs_ThreadDestructionHook);
	if (Error)
		DMibError(NMib::NPlatform::fg_FormatErrno("pthread_key_create", Error));

	m_DesktopEnvironment = fg_DeduceDesktopEnvironment();

	fp_InitComplete();
}

NMib::NSys::CLinuxPasswordManager* CSystemLinux::f_GetPasswordManager()
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

void CSystemLinux::f_InitModule()
{
	CSystem::f_InitModule();

	m_pDBus = fg_Construct();

}

void CSystemLinux::f_DestroyThreadSpecific()
{
	CSystem::f_PreDestructThreadSpecific();

	m_Posix.f_DestroyThreadSpecific();

	if (m_FileChangeNotificationContext.f_IsConstructed())
		m_FileChangeNotificationContext.f_Destruct();

	CSystem::f_DestructThreadSpecific();
}

void CSystemLinux::f_Destruct()
{
	m_Posix.f_Destruct();

	if (m_SocketContext.f_IsConstructed())
		m_SocketContext.f_Destruct();

	(*g_UUIDLibrary).f_Unload();
	g_UUIDLibrary.f_Destruct();

	m_pPasswordManager = nullptr;
	m_pDBus = nullptr;

	CSystem::f_Destruct();

	pthread_key_delete(m_ThreadDestructionHook);

}

void CSystemLinux::fs_ThreadDestructionHook(void* _ThreadID)
{
	fg_GetSys()->f_ThreadLocalFreeThread();
}

void CSystemLinux::f_RegisterDestructionHookForThread()
{
	pthread_setspecific(m_ThreadDestructionHook, (void*)NSys::fg_Thread_GetCurrentUID());
}

void CSystemLinux::f_LoadLibraries()
{
}

static inline_small CSystemLinux *fg_GetLocalSys()
{
	return (CSystemLinux *)fg_GetSys();
}

CSystem_POSIX *fg_GetSys_POSIX()
{
	return &fg_GetLocalSys()->m_Posix;
}

void NSys::fg_System_GenerateUUID(NCryptography::CUniversallyUniqueIdentifier &_UUID)
{
	static_assert(sizeof(uuid_t) == sizeof(_UUID));
	auto &Library = *g_UUIDLibrary;
	if (Library.f_OK())
	{
		Library.uuid_generate((unsigned char *)&_UUID);
#		if DMibEnableSafeCheck > 0
			uuid_string_t RetStr;
			Library.uuid_unparse((unsigned char *)&_UUID, RetStr);
#		endif
		_UUID.m_TimeLow = fg_ByteSwapBE(_UUID.m_TimeLow);
		_UUID.m_TimeMid = fg_ByteSwapBE(_UUID.m_TimeMid);
		_UUID.m_TimeHiAndVersion = fg_ByteSwapBE(_UUID.m_TimeHiAndVersion);
#		if DMibEnableSafeCheck > 0
			DMibFastCheck(_UUID.f_GetAsStaticString(NCryptography::EUniversallyUniqueIdentifierFormat_Bare).f_CmpNoCase(RetStr) == 0);
#		endif
	}
	else
		_UUID = NCryptography::CUniversallyUniqueIdentifier(NCryptography::EUniversallyUniqueIdentifierGenerate_Random);
}


NStr::CStr NSys::fg_System_GenerateUUID()
{
	auto &Library = *g_UUIDLibrary;
	if (Library.f_OK())
	{
		uuid_t Ret;
		Library.uuid_generate(Ret);
		uuid_string_t RetStr;
		Library.uuid_unparse(Ret, RetStr);

		// {7EE072A7-458D-491f-ACCF-447AD4BE8DBF}
		return CStr(CStr::CFormat("{{{}}") << RetStr);
	}
	else
		return NCryptography::fg_GetRandomUuidString(NCryptography::EUniversallyUniqueIdentifierFormat_Registry);
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

constinit NMib::NStorage::TCAggregate<CCodePageCache> g_CodePageCache = { DAggregateInit };

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

inline_never bool NSys::fg_Compiler_AlwaysFalse()
{
	return false;
}

assure_used inline_never bool NSys::fg_Compiler_MakeActive(const void *_Reference)
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

extern "C" _Unwind_Reason_Code _Unwind_Backtrace(_Unwind_Trace_Fn, void *);
extern "C" uintptr_t _Unwind_GetIP(struct _Unwind_Context *context);

inline_never mint NSys::fg_System_GetStackTrace(CMibCodeAddress *_pStack, mint _nMaxDepth)
{
	if (_nMaxDepth == 0)
		return 0;

	if (!g_bCanStackTrace) // backtrace uses malloc in pthread_once, and _Unwind_Backtrace can deadlock on __GI___dl_iterate_phdr lock
		return 0;

#if DArchitecture_arm64
	// This seems to be faster
	return (mint)backtrace((void**)_pStack, (int)_nMaxDepth);
#else
	struct CContext
	{
		CMibCodeAddress *m_pStack;
		mint m_nMaxDepth;
		mint m_nAdded = 0;
	};

	CContext Context;
	Context.m_pStack = _pStack;
	Context.m_nMaxDepth = _nMaxDepth;

	_Unwind_Backtrace
		(
			[](_Unwind_Context *_pUnwindContext, void *_pContext) -> _Unwind_Reason_Code
			{
				CContext &Context = *((CContext *)_pContext);
				Context.m_pStack[Context.m_nAdded] = (CMibCodeAddressType *)_Unwind_GetIP(_pUnwindContext);
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
#endif
}

inline_never CMibCodeAddress NSys::fg_System_GetStackTrace(aint _iDepth)
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
	if (NLocal::g_f_getrandom)
	{
		while (_nBytes)
		{
			auto nBytes = NLocal::g_f_getrandom(_pData, _nBytes, 0);
			if (nBytes < 0)
			{
				int Error = errno;
				if (Error == EINTR)
					continue;
				else if (Error == ENOSYS)
					break;
				else
					DMibPDebugBreak;
			}
			_pData += nBytes;
			_nBytes -= nBytes;
			
			if (_nBytes == 0)
				return;
		}
	}

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


bool NSys::fg_HW_GetVirtualMachineInfo(CVirtualMachineInfo& _Info)
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
	constinit NAtomic::TCAtomicAggregate<mint> g_bCanStartThreads = {DAggregateInit};
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

		void fg_CreateSystemVersion();
	}
}

#include <sys/utsname.h>

bool NSys::fg_System_GetOperatingSystemVersion(int &o_Major, int &o_Minor, int &o_Fix, EOperatingSystemArch &o_Arch, bool _bForceUpdate)
{
	if (g_OperatingSystemMajor >= 0)
	{
		o_Major = g_OperatingSystemMajor;
		o_Minor = g_OperatingSystemMinor;
		o_Fix = g_OperatingSystemFix;
		o_Arch = g_OperatingSystemArch;
		return g_OperatingSystemMajor != 0;
	}

	g_OperatingSystemMajor = 0;

	utsname NameInfo;
	if (uname(&NameInfo))
		return false;

	(CStr::CParse("{}.{}.{}") >> g_OperatingSystemMajor >> g_OperatingSystemMinor >> g_OperatingSystemFix).f_Parse(NameInfo.release);

	CStrPtr Machine;
	Machine.f_SetConstPtr(NameInfo.machine, fg_StrLen(NameInfo.machine));

	if (Machine == "i386" || Machine == "i486" || Machine == "i586" || Machine == "i686")
		g_OperatingSystemArch = EOperatingSystemArch_x86;
	else if (Machine == "x86_64")
		g_OperatingSystemArch = EOperatingSystemArch_x64;
	else if (Machine == "aarch64")
		g_OperatingSystemArch = EOperatingSystemArch_arm64;
	else
	{
		DMibFastCheck(false);
		g_OperatingSystemArch = EOperatingSystemArch_Unknown;
	}

	o_Major = g_OperatingSystemMajor;
	o_Minor = g_OperatingSystemMinor;
	o_Fix = g_OperatingSystemFix;
	o_Arch = g_OperatingSystemArch;

	return true;
}

#if defined(DArchitecture_arm64)
namespace NMib::NSys
{
	mint g_ThreadSelfOffset = 0;
}
#endif

void NSys::fg_CreateSystemVersion()
{
	if (CSystem::ms_PlatformVersion != 0)
		return;

#if defined(DArchitecture_arm64)
	{
		mint ThreadLocal;
		asm volatile ("mrs %0, TPIDR_EL0" : "=r" (ThreadLocal));
		g_ThreadSelfOffset = ThreadLocal - pthread_self();
	}
#endif

	if (g_OperatingSystemMajor < 0)
	{
		int Dummy;
		EOperatingSystemArch Arch;

		if (!fg_System_GetOperatingSystemVersion(Dummy, Dummy, Dummy, Arch))
			DMibPDebugBreak; // Not supported
	}

	CSystem::ms_PlatformVersion = g_OperatingSystemMajor * 1'000'000 + g_OperatingSystemMinor * 1'000 + g_OperatingSystemFix;
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
			return NMib::NMemory::CAllocator_NonTrackedHeap::f_AllocDebug(__size, DMibPFile, DMibPLine, EHeapDebugFlag_Ignore);
#		else
			return NMib::NMemory::CAllocator_NonTrackedHeap::f_Alloc(__size);
#		endif
	}

	void *nontracked_calloc (size_t __nmemb, size_t __size) __THROW __wur
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

	void *nontracked_realloc (void *__ptr, size_t __size) __THROW
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
#		if DMibConfig_MalterlibMemoryManager_Debug
			return NMib::NMemory::CAllocator_NonTrackedHeap::f_ResizeDebug(__ptr, __size, 0, DMibPFile, DMibPLine, EHeapDebugFlag_Ignore);
#		else
			return NMib::NMemory::CAllocator_NonTrackedHeap::f_Resize(__ptr, __size, 0, EAllocationFlag_SizeNotNeeded);
#		endif
	}

	void nontracked_free (void *__ptr) __THROW
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
		return NMib::NMemory::CAllocator_NonTrackedHeap::f_FreeNoSize(__ptr);
	}

	void nontracked_cfree (void *__ptr) __THROW
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
		return NMib::NMemory::CAllocator_NonTrackedHeap::f_FreeNoSize(__ptr);
	}
	void *nontracked_memalign (size_t __alignment, size_t __size) __THROW __wur
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
	void *nontracked_valloc (size_t __size) __THROW __wur
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
#		if DMibConfig_MalterlibMemoryManager_Debug
			return NMib::NMemory::CAllocator_NonTrackedHeap::f_AllocAlignedDebug(__size, NMib::NSys::NPrivate::g_PageSize, DMibPFile, DMibPLine, EHeapDebugFlag_Ignore);
#		else
			return NMib::NMemory::CAllocator_NonTrackedHeap::f_AllocAligned(__size, NMib::NSys::NPrivate::g_PageSize);
#		endif
	}
	void * nontracked_pvalloc (size_t __size) __THROW __wur
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
#		if DMibConfig_MalterlibMemoryManager_Debug
			return NMib::NMemory::CAllocator_NonTrackedHeap::f_AllocAlignedDebug(__size, NMib::NSys::NPrivate::g_PageSize, DMibPFile, DMibPLine, EHeapDebugFlag_Ignore);
#		else
			return NMib::NMemory::CAllocator_NonTrackedHeap::f_AllocAligned(__size, NMib::NSys::NPrivate::g_PageSize);
#		endif
	}
	size_t nontracked_malloc_usable_size (void *__ptr) __THROW
	{
		if (!__ptr)
			return 0;

		DMibFastCheck(g_bCanUseSystemMalloc);
		return NMib::NMemory::CAllocator_NonTrackedHeap::f_Size(__ptr);
	}
}

void NSys::fg_Process_AllowInvalidExit(bool _bAllow)
{
}

extern "C" void fg_InitMalterlib();

#ifdef DMibSanitizerEnabled_Thread
	void fg_Malterlib_MakeActive_TSan();
#endif

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

namespace NMib::NSys
{
#ifdef DMibInitInPreInitArray
	void fg_CreateSystem_Early(char **envp)
	{
		__environ = envp;
		environ = envp;
		NSys::fg_CreateSystem();
	}
#endif
}

namespace NMib::NSys::NPrivate
{
	constinit mint g_PageSize = 0;
}

void NSys::fg_CreateSystem()
{
	if (g_bCreatedSystem)
		return;

	NPrivate::g_PageSize = sysconf(_SC_PAGE_SIZE);

	fg_CreateSystemVersion();

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
	static_assert(alignof(CSystemLinux) <= mint(DMibPMemoryCacheLineSize), "Aligment error");

	NSys::fg_Compiler_MakeActive(&pSystemMemory);
	NSys::fg_Compiler_MakeActive(&pSystem);
	DMibFastCheck((void *)pSystem == pSystemMemory);

	g_DefaultTerminateHandler = std::set_terminate(&fg_TerminateHandler);
	g_DefaultUnexpectedHandler = std::set_unexpected(&fg_UnexpectedExceptionHandler);

#ifndef DMibPAutomaticSystemCreation
	NMib::g_pSys = pSystem;
#endif

	g_bCanUseSystemMalloc = true;
	g_bCanStackTrace = true;

	// Can use malloc from here on

	NPrivate::fg_InitBaseModuleAddress();
	NLocal::fg_GetSymbols(); // This uses malloc so needs to be run after memory manager is initialized
	NPrivate::fg_SetupLimits();

	if (!g_bIsSharedLibrary) // Only use pthread_atfork in non-dylibs as atfork handlers cannot be unregistered before dlclose
		pthread_atfork(&CSystemLinux::fs_ForkPrepare, &CSystemLinux::fs_ForkParent, &CSystemLinux::fs_ForkChild);

	//atexit(&fg_DestroySystemAtExit);

	fg_LoadLibraries();

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
						int Pipe = open(StdErrPipeName.f_GetStr(), fg_GetUnixOpenFlags() | O_WRONLY, S_IWUSR);

						if (Pipe == -1)
						{
							Error = NMib::NPlatform::fg_FormatErrno("open (named stderr pipe)", errno);
						}
						else
						{
							fg_SetUnixHandleOptions(Pipe);

							if (dup2(Pipe, 2) == -1)
							{
								Error = NMib::NPlatform::fg_FormatErrno("dup2 (named stderr pipe)", errno);
							}
						}
					}

					DMibConOutRaw((CFStr256::CFormat("bdda0079-b6eb-41ac-88d0-01b50e8be939 {nfh} {}\n") << (mint)getpid() << Error.f_ReplaceChar('\n', '\r')).f_GetStr());

					if (!StdInPipeName.f_IsEmpty())
					{
						int Pipe = open(StdInPipeName.f_GetStr(), fg_GetUnixOpenFlags() | O_RDONLY, S_IRUSR);

						if (Pipe == -1)
						{
							Error = NMib::NPlatform::fg_FormatErrno("open (named stdin pipe)", errno);
						}
						else
						{
							fg_SetUnixHandleOptions(Pipe);

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
	fg_InitBreakpad();
	pSystem->f_InitModuleThreaded();

	#ifdef DMibSanitizerEnabled_Thread
		fg_Malterlib_MakeActive_TSan();
	#endif
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

bool NSys::fg_System_BeingDebugged()
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

	NMib::NPlatform::CGetPwUidState State;
	auto *pPasswd = fg_Helper_GetPwUid(getuid(), State);
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

	NMib::NPlatform::CGetPwUidState State;
	auto *pPasswd = fg_Helper_GetPwUid(getuid(), State);
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
				tf_CStr ExePath = typename tf_CStr::CFormat("/proc/{}/exe") << (mint)getpid();
				tf_CStr FullPath = fg_ResolveSymbolicLink<tf_CStr>(ExePath);
				return NMib::NFile::CFile::fs_GetPath(FullPath);
			}
			template <typename tf_CStr>
			tf_CStr fg_GetProgramPathGeneral()
			{
				tf_CStr ExePath = typename tf_CStr::CFormat("/proc/{}/exe") << (mint)getpid();
				tf_CStr FullPath = fg_ResolveSymbolicLink<tf_CStr>(ExePath);
				return FullPath.f_RemoveSuffix(" (deleted)");
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
	return CStr::CFormat("/proc/{}/exe") << (mint)getpid();
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

void NSys::NFile::fg_Duplicate(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo)
{
	DMibErrorFile("Not supported");
}

bool NSys::NFile::fg_TryDuplicate(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo)
{
	return false;
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

void *NSys::NFile::fg_ChangeNotification_Open(const CStr &_FileName, NMib::NFile::EFileChange _OpenFlags, NMib::NThread::CSemaphoreAggregate *_pReportTo)
{
	return fg_GetLocalSys()->m_FileChangeNotificationContext->f_Open(_FileName, _OpenFlags, _pReportTo);
}

void NSys::NFile::fg_ChangeNotification_Close(void *_pNotification)
{
	fg_GetLocalSys()->m_FileChangeNotificationContext->f_Close(_pNotification);
}

bool NSys::NFile::fg_ChangeNotification_Changed(void *_pNotification)
{
	return fg_GetLocalSys()->m_FileChangeNotificationContext->f_Changed(_pNotification);
}

bool NSys::NFile::fg_ChangeNotification_GetNotification(void *_pNotification, NMib::NStr::CStr &_Path, NMib::NFile::EFileChangeNotification &_Notification, NMib::NStr::CStr &_PathFrom)
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
	return CUnixAddress::mc_MaxAddressLength;
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
	return fg_GetLocalSys()->m_SocketContext->f_AsyncConnect(*(CPOSIXAddress*)_Address, fg_Move(_fOnStateChange), (CPOSIXAddress*)_BindAddress);
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
	[[maybe_unused]] volatile int Line = DMibPLine;
	DMibPDebugBreak; // Not implemented
}

void NSys::NFile::fg_FileEnumOtherHandles(void *_pFile, NContainer::TCVector<NMib::NFile::CFileHandle> &_HandleInfo)
{
	[[maybe_unused]] volatile int Line = DMibPLine;
	DMibPDebugBreak; // Not implemented
}

void NSys::fg_Thread_Yield()
{
	sched_yield();
}

pid_t fg_Malterlib_Thread_GetTID_Local();

namespace NMib::NThread
{
	static_assert(sizeof(int) == sizeof(CLowLevelLockAggregate::m_Lock));

	namespace
	{
		constexpr uint32 gc_FutexThreadMask = uint32(FUTEX_TID_MASK);
	}

	uint32 call_futex(NAtomic::TCAtomicAggregate<CLowLevelLockAggregateLockType> &_Value, int _Operation)
	{
		return syscall(SYS_futex, (int *)&_Value, _Operation, 0, nullptr, nullptr, 0);
	}

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
		if (NMib::CSystem::ms_PlatformVersion >= 2'006'018)
			m_Lock = fg_Malterlib_Thread_GetTID_Local();
		else
			m_Lock = 1;

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

	namespace
	{
		[[noreturn]] inline_never void fg_AbortFutex(int _Error)
		{
			[[maybe_unused]] volatile int Line = DMibPLine;
			[[maybe_unused]] volatile int Error = _Error;
			DMibPDebugBreak;
			std::abort();
		}
	}

	bool CLowLevelLockAggregate::f_TryLock()
	{
		DMibSanitizerAnnotate_MutexPreLock(this, __tsan_mutex_write_reentrant | __tsan_mutex_try_lock);

		if (NMib::CSystem::ms_PlatformVersion >= 2'006'018)
		{
			uint32 ThreadID = fg_Malterlib_Thread_GetTID_Local();

			uint32 OldValue = 0;
			if (!m_Lock.f_CompareExchangeStrong(OldValue, ThreadID, NAtomic::EMemoryOrder_Acquire, NAtomic::EMemoryOrder_Relaxed))
			{
				if ((OldValue & gc_FutexThreadMask) == ThreadID)
					fg_AbortFutex(-1); // Recursive lock not supported

				if (OldValue & FUTEX_OWNER_DIED)
					fg_AbortFutex(-2); // Killing threads in not supported

				DMibSanitizerAnnotate_MutexPostLock(this, __tsan_mutex_write_reentrant | __tsan_mutex_try_lock | __tsan_mutex_try_lock_failed, 1);
				return false;
			}
		}
		else
		{
			uint32 Expected = 0;
			if (!m_Lock.f_CompareExchangeStrong(Expected, 1, NAtomic::EMemoryOrder_Acquire, NAtomic::EMemoryOrder_Acquire))
			{
				DMibSanitizerAnnotate_MutexPostLock(this, __tsan_mutex_write_reentrant | __tsan_mutex_try_lock | __tsan_mutex_try_lock_failed, 1);
				return false;
			}
		}

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
		if (NMib::CSystem::ms_PlatformVersion >= 2'006'018)
		{
			uint32 ThreadID = fg_Malterlib_Thread_GetTID_Local();

			while (true)
			{
				uint32 OldValue = 0;
				if (m_Lock.f_CompareExchangeWeak(OldValue, ThreadID, NAtomic::EMemoryOrder_Acquire, NAtomic::EMemoryOrder_Relaxed))
					break;

				if ((OldValue & gc_FutexThreadMask) == ThreadID)
					fg_AbortFutex(-1); // Recursive lock not supported

				if (OldValue & FUTEX_OWNER_DIED)
					fg_AbortFutex(-2); // Killing threads in not supported

				if (OldValue & gc_FutexThreadMask)
				{
					if (!call_futex(m_Lock, FUTEX_LOCK_PI_PRIVATE))
						break;

					auto Error = errno;
					switch (Error)
					{
					case EAGAIN: break;
					default: fg_AbortFutex(Error); // Broken
					}
				}
			}
		}
		else
		{
			NMib::NThread::CThreadSpinWaiter SpinWaiter;
			uint32 Expected = 0;
			while (!m_Lock.f_CompareExchangeWeak(Expected, 1, NAtomic::EMemoryOrder_Acquire, NAtomic::EMemoryOrder_Acquire))
			{
				SpinWaiter.f_Wait();
				Expected = 0;
			}
		}

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
		if (NMib::CSystem::ms_PlatformVersion >= 2'006'018)
		{
			uint32 ThreadID = fg_Malterlib_Thread_GetTID_Local();

			uint32 OldValue = ThreadID;

			if (!m_Lock.f_CompareExchangeStrong(OldValue, 0, NAtomic::EMemoryOrder_Release, NAtomic::EMemoryOrder_Relaxed))
			{
				// Contended
				if (call_futex(m_Lock, FUTEX_UNLOCK_PI_PRIVATE))
					fg_AbortFutex(errno);
			}
		}
		else
			m_Lock.f_Exchange(0, NAtomic::EMemoryOrder_Release);

		DMibSanitizerAnnotate_MutexPostUnlock(this, 0);
	}

	bool CLowLevelLockAggregate::f_TryLockNoSanitize()
	{
		if (NMib::CSystem::ms_PlatformVersion >= 2'006'018)
		{
			uint32 ThreadID = fg_Malterlib_Thread_GetTID_Local();

			uint32 OldValue = 0;
			if (!m_Lock.f_CompareExchangeStrong(OldValue, ThreadID, NAtomic::EMemoryOrder_Acquire, NAtomic::EMemoryOrder_Relaxed))
			{
				if ((OldValue & gc_FutexThreadMask) == ThreadID)
					fg_AbortFutex(-1); // Recursive lock not supported

				if (OldValue & FUTEX_OWNER_DIED)
					fg_AbortFutex(-2); // Killing threads in not supported

				return false;
			}
		}
		else
		{
			uint32 Expected = 0;
			if (!m_Lock.f_CompareExchangeStrong(Expected, 1, NAtomic::EMemoryOrder_Acquire, NAtomic::EMemoryOrder_Acquire))
				return false;
		}
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
		if (NMib::CSystem::ms_PlatformVersion >= 2'006'018)
		{
			uint32 ThreadID = fg_Malterlib_Thread_GetTID_Local();

			while (true)
			{
				uint32 OldValue = 0;
				if (m_Lock.f_CompareExchangeWeak(OldValue, ThreadID, NAtomic::EMemoryOrder_Acquire, NAtomic::EMemoryOrder_Relaxed))
					break;

				if ((OldValue & gc_FutexThreadMask) == ThreadID)
					fg_AbortFutex(-1); // Recursive lock not supported

				if (OldValue & FUTEX_OWNER_DIED)
					fg_AbortFutex(-2); // Killing threads in not supported

				if (OldValue & gc_FutexThreadMask)
				{
					if (!call_futex(m_Lock, FUTEX_LOCK_PI_PRIVATE))
						break;

					auto Error = errno;
					switch (Error)
					{
					case EAGAIN: break;
					default: fg_AbortFutex(Error); // Broken
					}
				}
			}
		}
		else
		{
			NMib::NThread::CThreadSpinWaiter SpinWaiter;
			uint32 Expected = 0;
			while (!m_Lock.f_CompareExchangeWeak(Expected, 1, NAtomic::EMemoryOrder_Acquire, NAtomic::EMemoryOrder_Acquire))
			{
				SpinWaiter.f_Wait();
				Expected = 0;
			}
		}
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
		if (NMib::CSystem::ms_PlatformVersion >= 2'006'018)
		{
			uint32 ThreadID = fg_Malterlib_Thread_GetTID_Local();

			uint32 OldValue = ThreadID;

			if (!m_Lock.f_CompareExchangeStrong(OldValue, 0, NAtomic::EMemoryOrder_Release, NAtomic::EMemoryOrder_Relaxed))
			{
				// Contended
				if (call_futex(m_Lock, FUTEX_UNLOCK_PI_PRIVATE))
					fg_AbortFutex(errno);
			}
		}
		else
			m_Lock.f_Exchange(0, NAtomic::EMemoryOrder_Release);
	}
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

constinit NStorage::TCAggregate<CCpuNameCache> g_CPUNameCache = {DAggregateInit};

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

namespace NMib
{
	namespace NSys
	{


		/*
		 Basic interface for storing secure passwords on a per-user, per-application basis.
		 */
		bool fg_SecurePassword_IsLocked()
		{
			return fg_GetLocalSys()->f_GetPasswordManager()->f_SecurePassword_IsLocked();
		}

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

// Make sure we can target glibc 2.3
#if !defined(DMibSanitizerEnabled) && defined(DArchitecture_x64)
__asm__(".symver __real_versioned_memcpy,memcpy@GLIBC_2.2.5");

extern "C" void *__real_versioned_memcpy(void *__restrict __dest, __const void *__restrict __src, __SIZE_TYPE__ __n);
extern "C" void *memcpy(void *__restrict __dest, __const void *__restrict __src, __SIZE_TYPE__ __n) __attribute__((no_builtin))
{
	if (NLocal::__real_versioned_memcpy_new)
		return NLocal::__real_versioned_memcpy_new(__dest, __src, __n);
	return __real_versioned_memcpy(__dest, __src, __n);
}

#endif

#include "math.h"

extern "C" pfp64 exp(pfp64 _Value) __attribute__((no_builtin))
{
	return NLocal::__real_versioned_exp_new(_Value);
}

extern "C" pfp64 exp2(pfp64 _Value) __attribute__((no_builtin))
{
	return NLocal::__real_versioned_exp2_new(_Value);
}

extern "C" pfp64 log(pfp64 _Value) __attribute__((no_builtin))
{
	return NLocal::__real_versioned_log_new(_Value);
}

extern "C" pfp64 log2(pfp64 _Value) __attribute__((no_builtin))
{
	return NLocal::__real_versioned_log2_new(_Value);
}

extern "C" pfp64 pow(pfp64 _Value, pfp64 _Power) __attribute__((no_builtin))
{
	return NLocal::__real_versioned_pow_new(_Value, _Power);
}

extern "C" pfp32 expf(pfp32 _Value) __attribute__((no_builtin))
{
	return NLocal::__real_versioned_expf_new(_Value);
}

extern "C" pfp32 exp2f(pfp32 _Value) __attribute__((no_builtin))
{
	return NLocal::__real_versioned_exp2f_new(_Value);
}

extern "C" pfp32 logf(pfp32 _Value) __attribute__((no_builtin))
{
	return NLocal::__real_versioned_logf_new(_Value);
}

extern "C" pfp32 log2f(pfp32 _Value) __attribute__((no_builtin))
{
	return NLocal::__real_versioned_log2f_new(_Value);
}

extern "C" pfp32 powf(pfp32 _Value, pfp32 _Power) __attribute__((no_builtin))
{
	return NLocal::__real_versioned_powf_new(_Value, _Power);
}

extern "C" int clock_getres(clockid_t __clock_id, struct timespec *__res) __THROW __attribute__((no_builtin))
{
	return NLocal::__real_versioned_clock_getres(__clock_id, __res);
}

extern "C" int clock_gettime(clockid_t __clock_id, struct timespec *__tp) __THROW __attribute__((no_builtin))
{
	return NLocal::__real_versioned_clock_gettime(__clock_id, __tp);
}

extern "C" int glob64(__const char *__restrict __pattern, int __flags, int (*__errfunc) (__const char *, int), glob64_t *__restrict __pglob) __THROW
{
	return NLocal::__real_versioned_glob64(__pattern, __flags, __errfunc, __pglob);
}

extern "C" int pthread_mutexattr_setrobust(pthread_mutexattr_t *__attr, int __robustness) __THROW
{
	return NLocal::__real_pthread_mutexattr_setrobust(__attr, __robustness);
}

extern "C" int pthread_mutex_consistent(pthread_mutex_t *__mutex) __THROW
{
	return NLocal::__real_pthread_mutex_consistent(__mutex);
}

extern "C" int __isoc99_vsscanf(const char * __restrict _pString, const char * __restrict _pFormat, va_list _VaList)
{
	va_list VaList;
	va_copy(VaList, _VaList);

	int Return;
	if (NLocal::__real___isoc99_vsscanf)
		Return = NLocal::__real___isoc99_vsscanf(_pString, _pFormat, VaList);
	else
		Return = sscanf(_pString, _pFormat, VaList);

	va_end(_VaList);

	return Return;
}

extern "C" int fcntl64(int _FileDescriptor, int _Command, ...)
{
    int Result;
    va_list VarArgs;
    va_start(VarArgs, _Command);

#ifdef SYS_fcntl64
	static constexpr int c_SysCallNumber = SYS_fcntl64;
#else
	static constexpr int c_SysCallNumber = SYS_fcntl;
#endif

	auto fHandleVoid = [&]
		{
			va_end(VarArgs);
			if (NLocal::__real_fcntl64)
				return NLocal::__real_fcntl64(_FileDescriptor, _Command);
			else
				return (int)syscall(c_SysCallNumber, _FileDescriptor, _Command);
		}
	;

	auto fHandleInt = [&]
		{
			if (NLocal::__real_fcntl64)
				Result = NLocal::__real_fcntl64(_FileDescriptor, _Command, va_arg(VarArgs, int));
			else
				Result = syscall(c_SysCallNumber, _FileDescriptor, _Command, va_arg(VarArgs, int));
			va_end(VarArgs);
			return Result;
		}
	;

	auto fHandleFlockPtr = [&]
		{
			if (NLocal::__real_fcntl64)
				Result = NLocal::__real_fcntl64(_FileDescriptor, _Command, va_arg(VarArgs, struct flock64 *));
			else
				Result = syscall(c_SysCallNumber, _FileDescriptor, _Command, va_arg(VarArgs, struct flock64 *));
			va_end(VarArgs);
			return Result;
		}
	;

	auto fHandleOwnerExPtr = [&]
		{
			if (NLocal::__real_fcntl64)
				Result = NLocal::__real_fcntl64(_FileDescriptor, _Command, va_arg(VarArgs, struct f_owner_ex*));
			else
				Result = syscall(c_SysCallNumber, _FileDescriptor, _Command, va_arg(VarArgs, struct f_owner_ex*));
			va_end(VarArgs);
			return Result;
		}
	;

	auto fHandleUint64Ptr = [&]
		{
			if (NLocal::__real_fcntl64)
				Result = NLocal::__real_fcntl64(_FileDescriptor, _Command, va_arg(VarArgs, uint64_t*));
			else
				Result = syscall(c_SysCallNumber, _FileDescriptor, _Command, va_arg(VarArgs, uint64_t*));
			va_end(VarArgs);
			return Result;
		}
	;

    switch (_Command)
	{
	//
	// File descriptor flags
	//
	case F_GETFD: return fHandleVoid();
	case F_SETFD: return fHandleInt();

	// File status flags
	//
	case F_GETFL: return fHandleVoid();
	case F_SETFL: return fHandleInt();

	// File byte range locking, not held across fork() or clone()
	//
	case F_SETLK: return fHandleFlockPtr();
	case F_SETLKW: return fHandleFlockPtr();
	case F_GETLK: return fHandleFlockPtr();

	// File byte range locking, held across fork()/clone() -- Not POSIX
	//
	case F_OFD_SETLK: return fHandleFlockPtr();
	case F_OFD_SETLKW: return fHandleFlockPtr();
	case F_OFD_GETLK: return fHandleFlockPtr();

	// Managing I/O availability signals
	//
	case F_GETOWN: return fHandleVoid();
	case F_SETOWN: return fHandleInt();
	case F_GETOWN_EX: return fHandleOwnerExPtr();
	case F_SETOWN_EX: return fHandleOwnerExPtr();
	case F_GETSIG: return fHandleVoid();
	case F_SETSIG: return fHandleInt();

	// Notified when process tries to open or truncate file (Linux 2.4+)
	//
	case F_SETLEASE: return fHandleInt();
	case F_GETLEASE: return fHandleVoid();

	// File and directory change notification
	//
	case F_NOTIFY: return fHandleInt();

	// Changing pipe capacity (Linux 2.6.35+)
	//
	case F_SETPIPE_SZ: return fHandleInt();
	case F_GETPIPE_SZ: return fHandleVoid();

	// File sealing (Linux 3.17+)
	//
	case F_ADD_SEALS: return fHandleInt();
	case F_GET_SEALS: return fHandleVoid();

	// File read/write hints (Linux 4.13+)
	//
	case F_GET_RW_HINT: return fHandleUint64Ptr();
	case F_SET_RW_HINT: return fHandleUint64Ptr();
	case F_GET_FILE_RW_HINT: return fHandleUint64Ptr();
	case F_SET_FILE_RW_HINT: return fHandleUint64Ptr();

	case F_DUPFD: return fHandleInt();
	case F_DUPFD_CLOEXEC: return fHandleInt();

	default:
		DMibPDebugBreak;
    }

	DMibPDebugBreak;
	return 1;
}

#ifdef DArchitecture_x64
__asm__(".symver __real_versioned___strtok_r_1c,__strtok_r_1c@GLIBC_2.2.5");

extern "C" char *__real_versioned___strtok_r_1c(char *__s, char __sep, char **__nextp);
extern "C" char *__strtok_r_1c(char *__s, char __sep, char **__nextp) __attribute__((no_builtin))
{
	return __real_versioned___strtok_r_1c(__s, __sep, __nextp);
}
#endif

#if !defined(DMibSanitizerEnabled_Address) && !defined(DMibSanitizerEnabled_Thread)
extern "C" unsigned long getauxval(unsigned long _Type)
{
#ifdef DMibSanitizerEnabled
	if (!NLocal::g_bSymbolsGotten)
		NLocal::fg_GetSymbols();
#endif

	if (NLocal::__real_getauxval)
		return NLocal::__real_getauxval(_Type);

	try
	{
		auto Data = NMib::NPlatform::fg_ReadProcFS("/proc/self/auxv");

		NStream::CBinaryStreamMemoryPtr<> Stream;
		Stream.f_OpenRead((uint8 const *)Data.f_GetArray(), Data.f_GetLen());

		struct CAuxvEntry
		{
			uint32 m_Tag;
			uint32 m_Value;
		};

		while (!Stream.f_IsAtEndOfStream())
		{
			CAuxvEntry Entry;
			Stream.f_ConsumeBytes(&Entry, sizeof(Entry));

			if (Entry.m_Tag == _Type)
				return Entry.m_Value;
		}
	}
	catch (...)
	{
	}

	return 0;
}
#endif

extern "C" int __sched_cpucount(size_t _SetSize, cpu_set_t const *_pSet)
{
	if (NLocal::__real_versioned___sched_cpucount)
		return NLocal::__real_versioned___sched_cpucount(_SetSize, _pSet);

	int CpuCount = 0;
	auto const *pSetIterator = _pSet->__bits;
	auto const *pSetIteratorEnd = pSetIterator + _SetSize / sizeof (__cpu_mask);
	for (; pSetIterator < pSetIteratorEnd; ++pSetIterator)
	{
		auto Mask = *pSetIterator;

		if constexpr (sizeof(Mask) == sizeof(uint64))
		{
#ifdef DMibPNumBitsSet64
			CpuCount += DMibPNumBitsSet64(Mask);
			continue;
#endif
		}
		else if constexpr (sizeof(Mask) == sizeof(uint32))
		{
#ifdef DMibPNumBitsSet32
			CpuCount += DMibPNumBitsSet32(Mask);
			continue;
#endif
		}

		for (mint iBit = 0; iBit < sizeof(Mask) * 8; ++iBit)
		{
			if (Mask & (__cpu_mask(1) << iBit))
				++CpuCount;
		}
	}

	return CpuCount;
}

extern "C" int posix_spawn
	(
		pid_t *pid
		, const char *path
		, const posix_spawn_file_actions_t *file_actions
		, const posix_spawnattr_t *attrp
		, char *const argv[]
		, char *const envp[]
	)
{
	return NLocal::__real_versioned_posix_spawn(pid, path, file_actions, attrp, argv, envp);
}

extern "C" int posix_spawnp
	(
		pid_t *pid
		, const char *path
		, const posix_spawn_file_actions_t *file_actions
		, const posix_spawnattr_t *attrp
		, char *const argv[]
		, char *const envp[]
	)
{
	return NLocal::__real_versioned_posix_spawnp(pid, path, file_actions, attrp, argv, envp);
}

extern "C" int utimensat(int dirfd, const char *pathname, const struct timespec times[2], int flags)
{
	if (NLocal::g_f_utimensat)
		return NLocal::g_f_utimensat(dirfd, pathname, times, flags);

	timeval Vals[2];

	if (times[0].tv_nsec == UTIME_OMIT || times[1].tv_nsec == UTIME_OMIT)
	{
		struct stat Stats;
		if (auto Return = fstatat(dirfd, pathname, &Stats, flags))
			return Return;

		if (times[0].tv_nsec == UTIME_OMIT)
			TIMESPEC_TO_TIMEVAL(Vals + 0, &Stats.st_atim);
		if (times[1].tv_nsec == UTIME_OMIT)
			TIMESPEC_TO_TIMEVAL(Vals + 1, &Stats.st_mtim);
	}

	if (times[0].tv_nsec == UTIME_NOW || times[1].tv_nsec == UTIME_NOW)
	{
		struct timeval Now;
		if (auto Return = gettimeofday(&Now, NULL))
			return Return;

		if (times[0].tv_nsec == UTIME_NOW)
			Vals[0] = Now;
		if (times[1].tv_nsec == UTIME_NOW)
			Vals[1] = Now;
	}

	if (times[0].tv_nsec != UTIME_NOW && times[0].tv_nsec != UTIME_OMIT)
		TIMESPEC_TO_TIMEVAL(Vals + 0, times + 0);

	if (times[1].tv_nsec != UTIME_NOW && times[1].tv_nsec != UTIME_OMIT)
		TIMESPEC_TO_TIMEVAL(Vals + 1, times + 1);

	if (auto Result = futimesat(dirfd, pathname, Vals))
		return Result;

	return 0;
}

extern "C" int futimens(int fd, const struct timespec times[2])
{
	if (NLocal::g_f_futimens)
		return NLocal::g_f_futimens(fd, times);

	timeval Vals[2];

	if (times[0].tv_nsec == UTIME_OMIT || times[1].tv_nsec == UTIME_OMIT)
	{
		struct stat Stats;
		if (auto Return = fstat(fd, &Stats))
			return Return;

		if (times[0].tv_nsec == UTIME_OMIT)
			TIMESPEC_TO_TIMEVAL(Vals + 0, &Stats.st_atim);
		if (times[1].tv_nsec == UTIME_OMIT)
			TIMESPEC_TO_TIMEVAL(Vals + 1, &Stats.st_mtim);
	}

	if (times[0].tv_nsec == UTIME_NOW || times[1].tv_nsec == UTIME_NOW)
	{
		struct timeval Now;
		if (auto Return = gettimeofday(&Now, NULL))
			return Return;

		if (times[0].tv_nsec == UTIME_NOW)
			Vals[0] = Now;
		if (times[1].tv_nsec == UTIME_NOW)
			Vals[1] = Now;
	}

	if (times[0].tv_nsec != UTIME_NOW && times[0].tv_nsec != UTIME_OMIT)
		TIMESPEC_TO_TIMEVAL(Vals + 0, times + 0);

	if (times[1].tv_nsec != UTIME_NOW && times[1].tv_nsec != UTIME_OMIT)
		TIMESPEC_TO_TIMEVAL(Vals + 1, times + 1);

	if (auto Result = futimes(fd, Vals))
		return Result;

	return 0;
}

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

#ifdef DMibSanitizerEnabled_Address

#include <sanitizer/lsan_interface.h>

extern "C" int __attribute__((used)) __lsan_is_turned_off(void)
{
    return 1;
}

#endif
