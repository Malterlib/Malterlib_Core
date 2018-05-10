// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

DMibAppNoClass;
DMibPMain;



NMib::NThread::TCThreadLocal<NMib::NStr::CStr, NMib::NMem::CAllocator_Heap, NMib::NThread::EThreadLocalFlag_AlwaysCreated> g_ThreadLocal;

extern "C"
{
	module_export void calling_convention_c fg_Test()
	{
		*g_ThreadLocal = NMib::NStr::CStr::fs_ToStr(NMib::NSys::fg_Thread_GetCurrentUID());
	}

	module_export void calling_convention_c fg_TestFileNotifications()
	{
		*g_ThreadLocal = NMib::NStr::CStr::fs_ToStr(NMib::NSys::fg_Thread_GetCurrentUID());
		
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