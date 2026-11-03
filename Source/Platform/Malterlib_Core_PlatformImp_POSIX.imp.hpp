// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

bool CSystem_POSIX::f_GetMalterlibDisableStdErrLog()
{
	if (m_MalterlibDisableStdErrLog.f_Load(NMib::NAtomic::gc_MemoryOrder_Relaxed) == 0)
	{
		char const *pEnv = getenv("MalterlibDisableStdErrLog");

		if (pEnv && fg_StrCmp(pEnv, "true") == 0)
			m_MalterlibDisableStdErrLog = 2;
		else
			m_MalterlibDisableStdErrLog = 1;
	}

	return m_MalterlibDisableStdErrLog.f_Load(NMib::NAtomic::gc_MemoryOrder_Relaxed) == 2;
}

void CSystem_POSIX::f_DestroyThreadSpecific()
{
}

void CSystem_POSIX::f_Destruct()
{
}

namespace NMib::NPlatform
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

namespace NMib::NSys::NPrivate
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
					Limits.rlim_cur = fg_Min(fg_Min(Limits.rlim_cur, MaxFiles), Limits.rlim_max);
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

		// The io_uring rings of the io loops are charged against the locked memory limit on Linux, and
		// the soft limit the process inherits is usually a few megabytes
		if (!getrlimit(RLIMIT_MEMLOCK, &Limits))
		{
			Limits.rlim_cur = Limits.rlim_max;
			if (setrlimit(RLIMIT_MEMLOCK, &Limits))
				DMibDTraceSafe("setrlimit RLIMIT_MEMLOCK failed: {}\n", strerror(errno));
		}
		else
			DMibDTraceSafe("getrlimit RLIMIT_MEMLOCK failed: {}\n", strerror(errno));

#ifdef DPlatformFamily_Linux
		// How far a thread may raise its own priority above the one it inherits from its creator
		if (!getrlimit(RLIMIT_NICE, &Limits))
		{
			Limits.rlim_cur = Limits.rlim_max;
			if (setrlimit(RLIMIT_NICE, &Limits))
				DMibDTraceSafe("setrlimit RLIMIT_NICE failed: {}\n", strerror(errno));
		}
		else
			DMibDTraceSafe("getrlimit RLIMIT_NICE failed: {}\n", strerror(errno));

		if (!getrlimit(RLIMIT_RTPRIO, &Limits))
		{
			Limits.rlim_cur = Limits.rlim_max;
			if (setrlimit(RLIMIT_RTPRIO, &Limits))
				DMibDTraceSafe("setrlimit RLIMIT_RTPRIO failed: {}\n", strerror(errno));
		}
		else
			DMibDTraceSafe("getrlimit RLIMIT_RTPRIO failed: {}\n", strerror(errno));
#endif
	}
}

// *************************************************************************************************************************
// Low Level Query Implementation
// (Not actually POSIX, more clang/gcc)
// *************************************************************************************************************************

void NMib::NSys::fg_HW_GetProcessorInfo(NMib::CProcessorInfo &_Info)
{ // Should probably be moved to a file Malterlib_x86_MacOS.cpp or similar.

	_Info.m_Features = NMib::EProcessorFeature_None;

#if defined(DArchitecture_x86) || defined(DArchitecture_x64)
	unsigned int CPUInfo[4];

	_Info.m_Architecture = NMib::EProcessorArchitecture_Unknown;

	__get_cpuid(0, &CPUInfo[0], &CPUInfo[1], &CPUInfo[2], &CPUInfo[3]);

	int MaxInfoType = CPUInfo[0];

	if (MaxInfoType >= 1)
	{
		__get_cpuid(1, &CPUInfo[0], &CPUInfo[1], &CPUInfo[2], &CPUInfo[3]);

		_Info.m_Features
			|= ((CPUInfo[3] & DMibBit(23)) ? EProcessorFeature_MMX : EProcessorFeature_None)
			| ((CPUInfo[3] & DMibBit(25)) ? EProcessorFeature_SSE : EProcessorFeature_None)
			| ((CPUInfo[3] & DMibBit(26)) ? EProcessorFeature_SSE2 : EProcessorFeature_None)
			| ((CPUInfo[2] & DMibBit(0)) ? EProcessorFeature_SSE3 : EProcessorFeature_None)
			| ((CPUInfo[2] & DMibBit(9)) ? EProcessorFeature_SSSE3 : EProcessorFeature_None)
			| ((CPUInfo[2] & DMibBit(19)) ? EProcessorFeature_SSE4_1 : EProcessorFeature_None)
			| ((CPUInfo[2] & DMibBit(20)) ? EProcessorFeature_SSE4_2 : EProcessorFeature_None)
			| ((CPUInfo[2] & DMibBit(31)) ? EProcessorFeature_HyperVisor : EProcessorFeature_None)
		;
	}

	_Info.m_Architecture = (sizeof(void *) == 4) ? EProcessorArchitecture_X86 : EProcessorArchitecture_X86_64;

#elif defined(DArchitecture_ppc32)
	_Info.m_Architecture = EProcessorArchitecture_Ppc32;
#elif defined(DArchitecture_ppc64)
	_Info.m_Architecture = EProcessorArchitecture_Ppc64;
#elif defined(DArchitecture_le32)
	_Info.m_Architecture = EProcessorArchitecture_Le32;
#elif defined(DArchitecture_arm64)
	_Info.m_Architecture = EProcessorArchitecture_Arm64;
#elif defined(DArchitecture_arm64e)
	_Info.m_Architecture = EProcessorArchitecture_Arm64e;
#else
#	error "Not implemented"
#endif
}

void NMib::NSys::fg_System_ExitProcess(aint _ExitCode)
{
	exit(_ExitCode);
}
