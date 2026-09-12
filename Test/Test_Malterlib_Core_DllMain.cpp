// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

#ifdef DPlatformFamily_Linux
#include <pthread.h>
#endif

DMibAppNoClass;
DMibPMain;

#ifdef DPlatformFamily_Linux
namespace
{
	// Destroyed when the thread that constructed it exits, which can be after the library was closed
	struct CThreadLocalWithDestructor
	{
		~CThreadLocalWithDestructor()
		{
			if (m_pDestroyedOnThread)
				*m_pDestroyedOnThread = (umint)pthread_self();
		}

		umint *m_pDestroyedOnThread = nullptr;
	};

	thread_local CThreadLocalWithDestructor g_ThreadLocalWithDestructor;
}
#endif



NMib::NThread::TCThreadLocal<NMib::NStr::CStr, NMib::NMemory::CAllocator_Heap, NMib::NThread::EThreadLocalFlag_AlwaysCreated> g_ThreadLocal;

extern "C"
{
	module_export void calling_convention_c fg_Test()
	{
		*g_ThreadLocal = NMib::NStr::CStr::fs_ToStr(NMib::NSys::fg_Thread_GetCurrentUID());
		{
			DMibMemLightweightTrackAddFlagsScope(NMib::NMemory::EMemoryReportLightweightScopeFlag_InCScope);
			DMibMemLightweightTrackDisableScope;
			delete (new int);
		}
	}

#ifdef DPlatformFamily_Linux
	module_export void calling_convention_c fg_TestConstructThreadLocalWithDestructor(umint *_pDestroyedOnThread)
	{
		g_ThreadLocalWithDestructor.m_pDestroyedOnThread = _pDestroyedOnThread;
	}
#endif

	module_export void calling_convention_c fg_TestFileNotifications()
	{
		*g_ThreadLocal = NMib::NStr::CStr::fs_ToStr(NMib::NSys::fg_Thread_GetCurrentUID());
		{
			DMibMemLightweightTrackAddFlagsScope(NMib::NMemory::EMemoryReportLightweightScopeFlag_InCScope);
			DMibMemLightweightTrackDisableScope;
			delete (new int);
		}

		using namespace NMib::NFile;
		CFileChangeNotification FileChangeNotification;
		//DMibTrace("ProgramDir: {}\n", CFile::fs_GetProgramDirectory());
		FileChangeNotification.f_Open(CFile::fs_GetProgramDirectory() / "DllStress", EFileChange_Recursive | EFileChange_Write | EFileChange_FileName, nullptr);
		FileChangeNotification.f_Close();
	}
}

#if 0
struct CTestClass
{
	CTestClass()
	{
		DMibNew int;
	};
	~CTestClass()
	{
		int x = 0; (void)x;
	}
};

CTestClass g_Test;
#endif
