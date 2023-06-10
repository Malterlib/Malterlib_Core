// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

/*
	Author:			Michael Wynne

	Contents:		POSIX implementation of platform specific functions
	
	Comments:		When using this to support a POSIX compliant platform ensure that you 
					implement the methods declared in the "Implementation specific headers"
					section below, PRIOR to including this cpp file.	
*/	
using namespace NMib;

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mman.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#include <uuid/uuid.h>
#include <poll.h>

#if (__x86_64__ || __i386__)
#include <cpuid.h>		// Used for __get_cpuid
#endif

#include <sys/param.h>

#include <Mib/Container/MapWithPool>

#include "Malterlib_Core_PlatformImp_POSIX.h"

// *************************************************************************************************************************
// POSIX Implementation Misc Types
// *************************************************************************************************************************

constinit NThread::CMutualAggregate g_VirtualMapLock = {DAggregateInit};
constinit NMib::NStorage::TCAggregateSimple<TCMapWithPool<mint, mint, NMib::CSort_Default, NMib::NMemory::CAllocator_VirtualNoTracking>> g_VirtualMap = {DAggregateInit};

constinit NAtomic::TCAtomicAggregate<mint> g_ForceMmapSequence = {DAggregateInit};

#ifdef DMibDebuggerHelpers
assure_used CMibCodeAddressType::CCodeAddressFunction *CMibCodeAddressType::fs_Debug_Function()
{
	return nullptr;
}

assure_used CMibCodeAddressType::CCodeAddressFile *CMibCodeAddressType::fs_Debug_File()
{
	return nullptr;
}

assure_used CMibCodeAddressType::CCodeAddressLine *CMibCodeAddressType::fs_Debug_Line()
{
	return nullptr;
}
#endif

bool CSystem_POSIX::f_GetMalterlibDisableStdErrLog()
{
	if (m_MalterlibDisableStdErrLog.f_Load(NMib::NAtomic::EMemoryOrder_Relaxed) == 0)
	{
		const char *pEnv = getenv("MalterlibDisableStdErrLog");

		if (pEnv && fg_StrCmp(pEnv, "true") == 0)
			m_MalterlibDisableStdErrLog = 2;
		else
			m_MalterlibDisableStdErrLog = 1;
	}

	return m_MalterlibDisableStdErrLog.f_Load(NMib::NAtomic::EMemoryOrder_Relaxed) == 2;
}

void CSystem_POSIX::f_DestroyThreadSpecific()
{
}

void CSystem_POSIX::f_Destruct()
{
#ifdef DMibDebuggerHelpers
	static_assert(TCInstantiateValue<&CMibCodeAddressType::fs_Debug_Function>::mc_Value);
	static_assert(TCInstantiateValue<&CMibCodeAddressType::fs_Debug_File>::mc_Value);
	static_assert(TCInstantiateValue<&CMibCodeAddressType::fs_Debug_Line>::mc_Value);
#endif
}

CSystem_POSIX *fg_GetSys_POSIX();

namespace NMib
{
	namespace NPlatform
	{
		NThread::CMutual &fg_ForkLock()
		{
			return fg_GetSys_POSIX()->m_ForkLock;
		}
		void fg_ForkPrepare()
		{
			::fg_ForkPrepare();			
		}
		void fg_ForkParentOrChild()
		{
			::fg_ForkParentOrChild();			
		}
	}
}


#ifdef DPlatformFamily_macOS
#include <sys/sysctl.h>
#endif

namespace NMib
{
	namespace NSys
	{
		namespace NPrivate
		{
			void fg_SetupLimits()
			{
				rlimit Limits;
				if (!getrlimit(RLIMIT_NOFILE, &Limits))
				{
#ifdef DPlatformFamily_macOS
					Limits.rlim_cur = fg_Min(fg_Max(Limits.rlim_cur, OPEN_MAX), Limits.rlim_max);
					{
						int SysCtl[2];
						SysCtl[0] = CTL_KERN;
						SysCtl[1] = KERN_MAXFILESPERPROC;
						size_t Size = sizeof(int);
						int MaxFiles;
						if (!sysctl(SysCtl, sizeof(SysCtl) / sizeof(*SysCtl), (void *)&MaxFiles, &Size, NULL, 0))
							Limits.rlim_cur = fg_Min(fg_Min(Limits.rlim_cur, MaxFiles), Limits.rlim_max);;
					}
#else
					Limits.rlim_cur = Limits.rlim_max;
#endif
					if (setrlimit(RLIMIT_NOFILE, &Limits))
						DMibDTraceSafe("setrlimit RLIMIT_NOFILE failed: {}\n", strerror(errno));
				}
				else
					DMibDTraceSafe("getrlimit RLIMIT_NOFILE failed: {}\n", strerror(errno));
				
				if (!getrlimit(RLIMIT_NPROC, &Limits))
				{
					Limits.rlim_cur = Limits.rlim_max;
					if (setrlimit(RLIMIT_NPROC, &Limits))
						DMibDTraceSafe("setrlimit RLIMIT_NPROC failed: {}\n", strerror(errno));
				}
				else
					DMibDTraceSafe("getrlimit RLIMIT_NPROC failed: {}\n", strerror(errno));
				
				
			}
		}
	}
}


// *************************************************************************************************************************
// Low Level Query Implementation
// (Not actually POSIX, more clang/gcc)
// *************************************************************************************************************************

void NMib::NSys::fg_HW_GetProcessorInfo(NMib::CProcessorInfo& _Info)
{ // Should probably be moved to a file Malterlib_x86_MacOS.cpp or similar.

	_Info.m_Features = NMib::EProcessorFeature_None;

#if defined(DArchitecture_x86) || defined(DArchitecture_x64)
	unsigned int CPUInfo[4];

	_Info.m_Architecture = NMib::EProcessorArchitecture_Unknown;

	__get_cpuid
		(
			0
			, &CPUInfo[0]
			, &CPUInfo[1]
			, &CPUInfo[2]
			, &CPUInfo[3]
		)
	;

	int MaxInfoType = CPUInfo[0];

	if (MaxInfoType >= 1)
	{
		__get_cpuid
			(
				1
				, &CPUInfo[0]
				, &CPUInfo[1]
				, &CPUInfo[2]
				, &CPUInfo[3]
			)
		;

		_Info.m_Features |=
			((CPUInfo[3] & DMibBit(23)) ? EProcessorFeature_MMX : EProcessorFeature_None)
			| ((CPUInfo[3] & DMibBit(25)) ? EProcessorFeature_SSE : EProcessorFeature_None)
			| ((CPUInfo[3] & DMibBit(26)) ? EProcessorFeature_SSE2 : EProcessorFeature_None)
			| ((CPUInfo[2] & DMibBit(0)) ? EProcessorFeature_SSE3 : EProcessorFeature_None)
			| ((CPUInfo[2] & DMibBit(9)) ? EProcessorFeature_SSSE3 : EProcessorFeature_None)
			| ((CPUInfo[2] & DMibBit(19)) ? EProcessorFeature_SSE4_1 : EProcessorFeature_None)
			| ((CPUInfo[2] & DMibBit(20)) ? EProcessorFeature_SSE4_2 : EProcessorFeature_None)
			| ((CPUInfo[2] & DMibBit(31)) ? EProcessorFeature_HyperVisor : EProcessorFeature_None)
		;
	}

	_Info.m_Architecture = (sizeof(void*) == 4) ? EProcessorArchitecture_x86 : EProcessorArchitecture_x86_64;

#elif defined(DArchitecture_ppc32)
	_Info.m_Architecture = EProcessorArchitecture_ppc32;
#elif defined(DArchitecture_ppc64)
	_Info.m_Architecture = EProcessorArchitecture_ppc64;
#elif defined(DArchitecture_le32)
	_Info.m_Architecture = EProcessorArchitecture_le32;
#elif defined(DArchitecture_arm64)
	_Info.m_Architecture = EProcessorArchitecture_arm64;
#elif defined(DArchitecture_arm64e)
	_Info.m_Architecture = EProcessorArchitecture_arm64e;
#else
#error "Not implemented"
#endif
}


void NMib::NSys::fg_System_ExitProcess(aint _ExitCode)
{
	exit(_ExitCode);
}
