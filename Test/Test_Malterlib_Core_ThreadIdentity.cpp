// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Test/Test>

#ifdef DPlatformFamily_Windows

namespace
{
	using namespace NMib;

	struct CThreadIdentity_Tests : NTest::CTest
	{
		void f_DoTests()
		{
			// A thread's start address is what identifies a foreign thread once the thread is gone; it must resolve
			// to a module and a function through the debug subsystem
			DMibTestSuite("StartAddress")
			{
				void const *pStart = NSys::fg_Thread_GetStartAddress(NSys::fg_Thread_GetCurrentUID());
				DMibExpectTrue(pStart != nullptr);

				CStackTraceInfo *pInfo = NSys::fg_Debug_AquireStackTraceInfo((CMibCodeAddress)pStart);
				DMibExpectTrue(pInfo != nullptr);
				if (pInfo)
				{
					DMibExpectTrue(pInfo->m_pModuleName && pInfo->m_pModuleName[0]);
					DMibExpectTrue(pInfo->m_pFunctionName && pInfo->m_pFunctionName[0]);
					NSys::fg_Debug_ReleaseStackTraceInfo(pInfo);
				}
			};
		}
	};
}

DMibTestRegister(CThreadIdentity_Tests, Malterlib::Core);

#endif
