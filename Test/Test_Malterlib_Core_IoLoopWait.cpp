// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Test/Exception>

namespace
{
	using namespace NMib;
	using namespace NMib::NStr;

	struct CIoLoopWait_Tests : NTest::CTest
	{
		void f_DoTests()
		{
			DMibTestSuite("DeadlineAndWake")
			{
				auto *pLoop = NSys::fg_CreateIoLoop();
				DMibAssertTrue(pLoop != nullptr);
				auto *pPreviousLoop = NSys::fg_GetOwnedIoLoop();
				auto DestroyLoop = g_OnScopeExit / [&]
					{
						NSys::fg_DestroyIoLoop(pLoop);
						NSys::fg_SetOwnedIoLoop(pPreviousLoop);
					}
				;
				pLoop->f_SetOwnerThreadToCurrent();
				pLoop->f_PollAndDispatch();

				for (umint i = 0; i < 3; ++i)
				{
					DMibTestPath("Deadline {}"_f << i);
					NTime::CStopwatch Elapsed{true};
					umint nWaits = 0;
					while (Elapsed.f_GetTime() < 0.02 && nWaits < 16)
					{
						pLoop->f_WaitAndDispatchTimeout(fg_Max(fp64(0.0), fp64(0.02) - Elapsed.f_GetTime()).f_Get());
						++nWaits;
					}
					DMibExpect(Elapsed.f_GetTime(), >=, 0.02);
					DMibExpect(nWaits, <, umint(16));
				}

				NThread::CEventAutoReset Start;
				NAtomic::TCAtomic<bool> bSent = false;
				auto pSender = NThread::CThreadObject::fs_StartThread
					(
						[&](NThread::CThreadObject *) -> aint
						{
							Start.f_Wait();
							bSent.f_Store(true);
							pLoop->f_Wake();
							return 0;
						}
						, "Timed I/O wake"
					)
				;
				auto StopSender = g_OnScopeExit / [&]
					{
						Start.f_Signal();
						pSender->f_Stop(true);
					}
				;
				Start.f_Signal();
				NTime::CStopwatch WakeTime{true};
				pLoop->f_WaitAndDispatchTimeout(10.0);
				DMibExpectTrue(bSent.f_Load());
				DMibExpect(WakeTime.f_GetTime(), <, 5.0);

				pLoop->f_WaitAndDispatchTimeout(0.0);
			};
		}
	};
}

DMibTestRegister(CIoLoopWait_Tests, Malterlib::Core);
