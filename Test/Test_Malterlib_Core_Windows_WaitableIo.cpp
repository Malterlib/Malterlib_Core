// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Test/Exception>

#ifdef DPlatformFamily_Windows
#include <Windows.h>

namespace
{
	using namespace NMib;
	using namespace NMib::NStr;

	struct CWaitableIo_Tests : NTest::CTest
	{
		void f_DoTests()
		{
			DMibTestSuite("ArmRearmAndCancel")
			{
				auto *pLoop = NSys::fg_CreateIoLoop();
				DMibAssertTrue(pLoop != nullptr);

				auto *pPreviousLoop = NSys::fg_GetOwnedIoLoop();
				auto DestroyLoop = g_OnScopeExit / [pLoop, pPreviousLoop]
					{
						NSys::fg_DestroyIoLoop(pLoop);
						NSys::fg_SetOwnedIoLoop(pPreviousLoop);
					}
				;
				pLoop->f_SetOwnerThreadToCurrent();

				for (bool bManualReset : {false, true})
				{
					DMibTestPath(bManualReset ? "ManualReset" : "AutoReset");

					HANDLE hEvent = CreateEventW(nullptr, bManualReset, FALSE, nullptr);
					DMibAssertTrue(hEvent != nullptr);
					auto Close = g_OnScopeExit / [hEvent]
						{
							CloseHandle(hEvent);
						}
					;

					struct CReports
					{
						umint m_Reads = 0;
						umint m_Errors = 0;
					};

					CReports Reports;

					auto fPollUntil = [&](auto const &_fDone)
						{
							for (umint i = 0; i < 5000 && !_fDone(); ++i)
							{
								pLoop->f_PollAndDispatch();
								NSys::fg_Thread_Sleep(0.001);
							}

							return _fDone();
						}
					;

					DMibExpectTrue(SetEvent(hEvent) != 0);
					auto *pRegistration = pLoop->f_Register
						(
							(NSys::CIoLoopHandle)(umint)hEvent
							, &Reports
							, NSys::EIoLoopEvent::mc_Read
							, [](void *_pToken, NSys::EIoLoopEvent _Events, int)
							{
								auto &Reports = *static_cast<CReports *>(_pToken);
								if (fg_IsSet(_Events, NSys::EIoLoopEvent::mc_Read))
									++Reports.m_Reads;
								if (fg_IsSet(_Events, NSys::EIoLoopEvent::mc_Error))
									++Reports.m_Errors;
							}
							, false
							, {.m_bWaitableHandle = true}
						)
					;
					auto Deregister = g_OnScopeExit / [&]
						{
							if (pRegistration)
								pLoop->f_Deregister(pRegistration);
						}
					;

					DMibExpectTrue(fPollUntil([&] { return Reports.m_Reads || Reports.m_Errors; }));
					DMibExpect(Reports.m_Reads, ==, 1);
					pLoop->f_PollAndDispatch();
					{
						DMibTestPath("NoAutomaticRearm");

						DMibExpect(Reports.m_Reads, ==, 1);
					}

					// A manual-reset event remains signaled; an auto-reset event needs another signal.
					if (!bManualReset)
						SetEvent(hEvent);
					pLoop->f_RequestReadiness(pRegistration, NSys::EIoLoopEvent::mc_Read);
					DMibExpectTrue(fPollUntil([&] { return Reports.m_Reads == 2 || Reports.m_Errors; }));
					DMibExpect(Reports.m_Reads, ==, 2);

					ResetEvent(hEvent);
					pLoop->f_RequestReadiness(pRegistration, NSys::EIoLoopEvent::mc_Read);
					pLoop->f_PollAndDispatch();

					// Exercise cancellation with both a pending wait and a packet racing with removal.
					if (bManualReset)
						SetEvent(hEvent);
					umint nClosed = 0;
					pLoop->f_DeregisterAsync(pRegistration, [&] { ++nClosed; });
					pRegistration = nullptr;
					DMibExpectTrue(fPollUntil([&] { return nClosed != 0; }));

					umint nReadsAtClose = Reports.m_Reads;
					SetEvent(hEvent);
					for (umint i = 0; i < 16; ++i)
						pLoop->f_PollAndDispatch();

					DMibExpect(nClosed, ==, 1);
					DMibExpect(Reports.m_Reads, ==, nReadsAtClose);
					DMibExpect(Reports.m_Errors, ==, 0);
				}
			};
		}
	};

	DMibTestRegister(CWaitableIo_Tests, Malterlib::Core);
}
#endif
