// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Test/Performance>
#include <Mib/File/File>

using namespace NMib::NStr;

#if 0
void __cdecl fg_ValidExitProcess();
void __cdecl fg_ValidDestroyModule();
#endif

namespace
{
	class CDll_Tests : public NMib::NTest::CTest
	{
	public:

		void f_DoTests()
		{
			CStr DllPath = CStr("Test_Malterlib_Helper_Core") + NMib::NFile::CFile::fs_GetDllExtension();
#ifdef DPlatformFamily_Linux
			DllPath = NMib::NFile::CFile::fs_AppendPath(NMib::NFile::CFile::fs_GetProgramDirectory(), DllPath);
#endif
#if 0
			DMibTestSuite("DllLoadNotUnload")
			{
				auto pDll = NMib::NSys::fg_LoadLibrary(DllPath);

				NMib::NThread::CThreadObject::fs_StartThread
					(
						[](NMib::NThread::CThreadObject *_pThreadObject) -> aint
						{
							while (true)
							{
								NMib::NMemory::CAllocator_NonTrackedHeap::f_Free(NMib::NMemory::CAllocator_NonTrackedHeap::f_Alloc(1));
							}

							return 0;
						}
						, "Leaked thread"
					).f_Detach()
				;

				NMib::NSys::fg_Thread_Sleep(0.1f);
				fg_ValidExitProcess();
				fg_ValidDestroyModule();
				NMib::NSys::fg_System_ExitProcess(2);
			};
#endif

			DMibTestSuite("MemoryTrackWithoutDll")
			{
				DMibMemLightweightTrackAddFlagsScope(NMib::NMemory::EMemoryReportLightweightScopeFlag_InCScope);
				DMibMemLightweightTrackDisableScope;
				delete (new int);
			};
			DMibTestSuite("DllLoad")
			{
				void *pDll = nullptr;
				NMib::NThread::CEvent ThreadStarted;

				NMib::NFile::CFile::fs_CreateDirectory(NMib::NFile::CFile::fs_GetProgramDirectory() / "DllStress");

				ThreadStarted.f_ResetSignaled();

				void (calling_convention_c *pTestFunc)() = nullptr;
				
				auto fl_ThreadProc
					= [&](NMib::NThread::CThreadObject *_pThread) -> aint
					{
						while (_pThread->f_GetState() != NMib::NThread::EThreadState_EventWantQuit)
						{
							if (pDll)
							{
								if (pTestFunc)
									pTestFunc();
							}
							ThreadStarted.f_SetSignaled();
							_pThread->m_EventWantQuit.f_Wait();
						}
						return 0;
					}
				;

				auto pThread0 = NMib::NThread::CThreadObject::fs_StartThread
					(
						fl_ThreadProc
						, "DllThreadTest"
					)
				;


				ThreadStarted.f_Wait();

				pDll = NMib::NSys::fg_LoadLibrary(DllPath);

				DMibTest(DMibExpr(pDll))(ETest_FailAndStop);
				
				if (NMib::NFile::CFileChangeNotification::fs_Supported())
					(void * &)pTestFunc = NMib::NSys::fg_GetLibrarySymbol(pDll, "fg_TestFileNotifications");
				else
					(void * &)pTestFunc = NMib::NSys::fg_GetLibrarySymbol(pDll, "fg_Test");
				DMibTest(DMibExpr(pTestFunc))(ETest_FailAndStop);

				ThreadStarted.f_ResetSignaled();
				pThread0->m_EventWantQuit.f_Signal();
				ThreadStarted.f_Wait();

				ThreadStarted.f_ResetSignaled();
				auto pThread1 = NMib::NThread::CThreadObject::fs_StartThread
					(
						fl_ThreadProc
						, "DllThreadTest"
					)
				;
				ThreadStarted.f_Wait();

				ThreadStarted.f_ResetSignaled();
				pThread1->m_EventWantQuit.f_Signal();
				ThreadStarted.f_Wait();
				pThread1.f_Clear();

				pThread0.f_Clear();

				ThreadStarted.f_ResetSignaled();
				auto pThread2 = NMib::NThread::CThreadObject::fs_StartThread
					(
						fl_ThreadProc
						, "DllThreadTest"
					)
				;
				ThreadStarted.f_Wait();

				ThreadStarted.f_ResetSignaled();
				pThread2->m_EventWantQuit.f_Signal();
				ThreadStarted.f_Wait();

				pThread2.f_Clear();
				
				NMib::NSys::fg_FreeLibrary(pDll);

			};
			DMibTestSuite("Thread stress")
			{
				void *pDll = nullptr;
				void (calling_convention_c *pTestFunc)() = nullptr;
				
				auto fl_ThreadProc
					= [&](NMib::NThread::CThreadObject *_pThread) -> aint
					{
						pTestFunc();
						while (_pThread->f_GetState() != NMib::NThread::EThreadState_EventWantQuit)
						{
							pTestFunc();
							_pThread->m_EventWantQuit.f_Wait();
						}
						return 0;
					}
				;

				pDll = NMib::NSys::fg_LoadLibrary(DllPath);

				NMib::NContainer::TCVector<NMib::NStorage::TCUniquePointer<NMib::NThread::CThreadObject>> Threads;
				Threads.f_SetLen(100);
				DMibTest(DMibExpr(pDll))(ETest_FailAndStop);
				
				(void * &)pTestFunc = NMib::NSys::fg_GetLibrarySymbol(pDll, "fg_Test");
				DMibTest(DMibExpr(pTestFunc))(ETest_FailAndStop);
				pTestFunc();
				

#ifdef DMibDebug
				mint AllThreads = 656/64;
#else
				mint AllThreads = 6554/64;
#endif
				for (mint i = 0; i < AllThreads; ++i)
				{
					for (auto i = 0; i < 64; ++i)
					{
						Threads[i] = NMib::NThread::CThreadObject::fs_StartThread
							(
								[&](NMib::NThread::CThreadObject *_pThread) -> aint
								{
									fl_ThreadProc(_pThread);
									NMib::NContainer::TCVector<NMib::NStorage::TCUniquePointer<NMib::NThread::CThreadObject>> Threads2;
									Threads2.f_SetLen(10);

									NMib::NThread::TCThreadLocal<NMib::TCAutoClearInt<int32>, NMib::NMemory::CAllocator_Heap, NMib::NThread::EThreadLocalFlag_AlwaysCreated> TestStorage;
									{
										NMib::NThread::TCThreadLocal<NMib::TCAutoClearInt<int32>, NMib::NMemory::CAllocator_Heap, NMib::NThread::EThreadLocalFlag_AlwaysCreated> TestStorage2;

										for (auto i = 0; i < 10; ++i)
										{
											Threads2[i] = NMib::NThread::CThreadObject::fs_StartThread
												(
													fl_ThreadProc
													, "DllThreadTest"
												)
											;
										}
									}
									return 0;

								}
								, "DllThreadTest"
							)
						;
					}
				}

				Threads.f_Clear();

				NMib::NSys::fg_FreeLibrary(pDll);
			};
			DMibTestSuite("Dll stress")
			{
				NMib::NFile::CFile::fs_CreateDirectory(NMib::NFile::CFile::fs_GetProgramDirectory() / "DllStress");
				for (int i = 0; i < 16; ++i)
				{
					void *pDll = nullptr;
					void (calling_convention_c *pTestFunc)() = nullptr;
					pDll = NMib::NSys::fg_LoadLibrary(DllPath);
					DMibTest(DMibExpr(pDll))(ETest_FailAndStop)(ETestFlag_Aggregated);
					if (NMib::NFile::CFileChangeNotification::fs_Supported())
						(void * &)pTestFunc = NMib::NSys::fg_GetLibrarySymbol(pDll, "fg_TestFileNotifications");
					else
						(void * &)pTestFunc = NMib::NSys::fg_GetLibrarySymbol(pDll, "fg_Test");
					DMibTest(DMibExpr(pTestFunc))(ETest_FailAndStop)(ETestFlag_Aggregated);
					for (int i = 0; i < 2; ++i)
						pTestFunc();
					NMib::NSys::fg_FreeLibrary(pDll);
				}
			};

			DMibTestSuite(CTestCategory("Performance") << CTestGroup("Performance"))
			{
				CTestPerformanceMeasure MalterlibTime("Malterlib");

				mint nTests = 256*16;
#ifdef DMibDebug
				nTests /= 16;
#endif
				nTests += 1;

				mint nLoops = 1;

				for(mint j = 0; j < nTests; ++j)
				{
					MalterlibTime.f_Start();
					for (mint i = 0; i < nLoops; ++i) 
					{
						auto pDll = NMib::NSys::fg_LoadLibrary(DllPath);
						DMibTest(DMibExpr(pDll))(ETestFlag_Aggregated);
						NMib::NSys::fg_FreeLibrary(pDll);
					}
					MalterlibTime.f_Stop(nLoops);
				}

				CTestPerformance PerfTest(1.0);
				PerfTest.f_Add(MalterlibTime);
				DMibTest(DMibExpr(PerfTest));

			};

		}

	};
}
DMibTestRegister(CDll_Tests, Malterlib::Core);
