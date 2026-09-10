// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Test/Performance>
#include <Mib/File/File>
#include <Mib/Atomic/Atomic>

#ifdef DPlatformFamily_macOS
#include <pthread.h>
#include <dlfcn.h>
#endif

using namespace NMib::NStr;

#if 0
void __cdecl fg_ValidExitProcess();
void __cdecl fg_ValidDestroyModule();
#endif

namespace
{
#if defined(DPlatformFamily_macOS) && !defined(DMibSanitizerEnabled_Thread)
	// Unloading waits for the threads already in pthread's exit path to leave it, so what the host
	// does while its threads exit decides whether an unload can finish at all
	NMib::NAtomic::TCAtomic<bool> g_bUnloadStarted{false};
	NMib::NAtomic::TCAtomic<bool> g_bUnloadFinished{false};
	NMib::NAtomic::TCAtomic<bool> g_bExitingThreadInDyld{false};
	NMib::NAtomic::TCAtomic<bool> g_bExitingThreadLeftDyld{false};
	NMib::NAtomic::TCAtomic<bool> g_bChurnStop{false};
	NMib::NAtomic::TCAtomic<umint> g_nChurned{0};
	pthread_key_t g_ChurnKey;

	void *fg_UnloadLibraryThread(void *_pDll)
	{
		NMib::NSys::fg_FreeLibrary(_pDll);
		g_bUnloadFinished = true;

		return nullptr;
	}

	// Unloading on a thread of its own lets the test report a stalled unload instead of hanging
	// with it. A stalled unloader is left running, since joining it would hang too
	bool fg_UnloadWithDeadline(void *_pDll, umint _DeadlineMilliseconds)
	{
		g_bUnloadFinished = false;
		g_bUnloadStarted = true;

		pthread_t Unloader;
		if (pthread_create(&Unloader, nullptr, &fg_UnloadLibraryThread, _pDll) != 0)
			return false;

		for (umint i = 0; i < _DeadlineMilliseconds / 10 && !g_bUnloadFinished.f_Load(); ++i)
			NMib::NSys::fg_Thread_Sleep(0.01);

		if (!g_bUnloadFinished.f_Load())
			return false;

		pthread_join(Unloader, nullptr);

		return true;
	}

	// dlopen and dlsym block for as long as an unloading image runs its destructors, so a host key
	// destructor that resolves a symbol stays marked as exiting for exactly that long
	void fg_DyldBoundKeyDestructor(void *)
	{
		g_bExitingThreadInDyld = true;
		while (!g_bUnloadStarted.f_Load())
			NMib::NSys::fg_Thread_Sleep(0.001);

		NMib::NSys::fg_Thread_Sleep(0.25); // let the unload reach its drain first
		(void)dlsym(RTLD_DEFAULT, "malloc");
		g_bExitingThreadLeftDyld = true;
	}

	void *fg_ExitThroughDyldThread(void *_pKey)
	{
		pthread_setspecific(*(pthread_key_t *)_pKey, (void *)1);

		return nullptr;
	}

	void fg_ChurnKeyDestructor(void *)
	{
		NMib::NSys::fg_Thread_Sleep(0.002);
	}

	void *fg_ChurnBody(void *)
	{
		pthread_setspecific(g_ChurnKey, (void *)1);

		return nullptr;
	}

	// Each worker is joined so none of them can reach the churn key after the suite deletes it
	void *fg_ChurnThread(void *)
	{
		while (!g_bChurnStop.f_Load())
		{
			pthread_t Short;
			if (pthread_create(&Short, nullptr, &fg_ChurnBody, nullptr) == 0)
				pthread_join(Short, nullptr);

			g_nChurned.f_FetchAdd(1);
		}

		return nullptr;
	}
#endif

	class CDll_Tests : public NMib::NTest::CTest
	{
	public:

		void f_DoTests()
		{
#if defined(DCompiler_MSVC_Workaround_DllsBroken)
			return;
#endif
			// tsan does not currently support unloading dlls
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

#if !(defined(DMibSanitizerEnabled_Thread) && defined(DPlatformFamily_macOS))
				NMib::NSys::fg_FreeLibrary(pDll);
#endif
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

				for (umint i = 0; i < 10; ++i)
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

#if !(defined(DMibSanitizerEnabled_Thread) && defined(DPlatformFamily_macOS))
				NMib::NSys::fg_FreeLibrary(pDll);
#endif
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

#if !(defined(DMibSanitizerEnabled_Thread) && defined(DPlatformFamily_macOS))
					NMib::NSys::fg_FreeLibrary(pDll);
#endif
				}
			};
#if defined(DPlatformFamily_macOS) && !defined(DMibSanitizerEnabled_Thread)
			// Unloading waits for threads to leave their exit path; threads terminating meanwhile, and one
			// held in a key destructor, must not stall or fault that wait
			DMibTestSuite("Unload during thread exit")
			{
				void *pDll = NMib::NSys::fg_LoadLibrary(DllPath);
				DMibTest(DMibExpr(pDll))(ETest_FailAndStop);

				pthread_key_t SlowKey;
				DMibTest(DMibExpr(pthread_key_create(&SlowKey, [](void *) { NMib::NSys::fg_Thread_Sleep(2.0); })) == DMibExpr(0))(ETest_FailAndStop);

				NMib::NAtomic::TCAtomic<bool> bStop{false};
				NMib::NAtomic::TCAtomic<umint> nChurned{0};
				pthread_t SlowThread;
				pthread_t ChurnThread;
				struct CChurn
				{
					NMib::NAtomic::TCAtomic<bool> *m_pbStop;
					NMib::NAtomic::TCAtomic<umint> *m_pnChurned;
					pthread_key_t m_SlowKey;
				} Churn{&bStop, &nChurned, SlowKey};

				auto fSlow = [](void *_pKey) -> void *
					{
						pthread_setspecific(*(pthread_key_t *)_pKey, (void *)1);
						return nullptr;
					}
				;
				auto fChurn = [](void *_pChurn) -> void *
					{
						auto &Churn = *(CChurn *)_pChurn;
						while (!Churn.m_pbStop->f_Load())
						{
							pthread_t Short;
							if (pthread_create(&Short, nullptr, [](void *) -> void * { return nullptr; }, nullptr) == 0)
								pthread_join(Short, nullptr);
							Churn.m_pnChurned->f_FetchAdd(1);
						}
						return nullptr;
					}
				;
				DMibTest(DMibExpr(pthread_create(&SlowThread, nullptr, fSlow, &SlowKey)) == DMibExpr(0))(ETest_FailAndStop);
				DMibTest(DMibExpr(pthread_create(&ChurnThread, nullptr, fChurn, &Churn)) == DMibExpr(0))(ETest_FailAndStop);
				NMib::NSys::fg_Thread_Sleep(0.2);

				NMib::NSys::fg_FreeLibrary(pDll);

				bStop = true;
				pthread_join(ChurnThread, nullptr);
				pthread_join(SlowThread, nullptr);
				pthread_key_delete(SlowKey);
				DMibTest(DMibExpr(nChurned.f_Load()) > DMibExpr(umint(0)));
			};
			// dyld holds its loader lock across the destructors dlclose runs, so an exiting host thread
			// that enters dyld meanwhile cannot leave the exit path the unload waits for. Nothing the
			// library does inside those destructors can change that, and a teardown before dlclose
			// would run ahead of the image's own static destructors, so a host must not enter dyld
			// from a key destructor while it unloads. Kept to show the stall, not to pass
			DMibTestSuite(CTestCategory("Unload with a thread exiting through dyld") << CTestGroup("Manual"))
			{
				void *pDll = NMib::NSys::fg_LoadLibrary(DllPath);
				DMibTest(DMibExpr(pDll))(ETest_FailAndStop);

				pthread_key_t DyldKey;
				DMibTest(DMibExpr(pthread_key_create(&DyldKey, &fg_DyldBoundKeyDestructor)) == DMibExpr(0))(ETest_FailAndStop);

				pthread_t Exiting;
				DMibTest(DMibExpr(pthread_create(&Exiting, nullptr, &fg_ExitThroughDyldThread, &DyldKey)) == DMibExpr(0))(ETest_FailAndStop);
				while (!g_bExitingThreadInDyld.f_Load())
					NMib::NSys::fg_Thread_Sleep(0.001);

				bool bUnloaded = fg_UnloadWithDeadline(pDll, 30000);
				DMibTest(DMibExpr(bUnloaded));

				if (bUnloaded)
				{
					pthread_join(Exiting, nullptr);
					pthread_key_delete(DyldKey);
				}
			};
			// Only threads already exiting when the key is deleted can still reach its destructor, so
			// threads that keep retiring must not hold the unload up
			DMibTestSuite("Unload during sustained thread exit")
			{
				void *pDll = NMib::NSys::fg_LoadLibrary(DllPath);
				DMibTest(DMibExpr(pDll))(ETest_FailAndStop);
				DMibTest(DMibExpr(pthread_key_create(&g_ChurnKey, &fg_ChurnKeyDestructor)) == DMibExpr(0))(ETest_FailAndStop);

				constexpr umint c_nChurners = 16;
				pthread_t Churners[c_nChurners];
				umint nStarted = 0;
				for (umint i = 0; i < c_nChurners; ++i)
				{
					if (pthread_create(&Churners[nStarted], nullptr, &fg_ChurnThread, nullptr) == 0)
						++nStarted;
				}
				DMibTest(DMibExpr(nStarted) == DMibExpr(c_nChurners))(ETest_FailAndStop);
				NMib::NSys::fg_Thread_Sleep(0.2);

				bool bUnloaded = fg_UnloadWithDeadline(pDll, 30000);

				g_bChurnStop = true;
				for (umint i = 0; i < nStarted; ++i)
					pthread_join(Churners[i], nullptr);

				pthread_key_delete(g_ChurnKey);
				DMibTest(DMibExpr(g_nChurned.f_Load()) > DMibExpr(umint(0)));
				DMibTest(DMibExpr(bUnloaded));
			};
#endif
			DMibTestSuite(CTestCategory("Performance") << CTestGroup("Performance"))
			{
				CTestPerformanceMeasure MalterlibTime("Malterlib");

				umint nTests = 256*16;
#ifdef DMibDebug
				nTests /= 16;
#endif
				nTests += 1;

				umint nLoops = 1;

				for(umint j = 0; j < nTests; ++j)
				{
					MalterlibTime.f_Start();
					for (umint i = 0; i < nLoops; ++i)
					{
						auto pDll = NMib::NSys::fg_LoadLibrary(DllPath);
						DMibTest(DMibExpr(pDll))(ETestFlag_Aggregated);
#if !(defined(DMibSanitizerEnabled_Thread) && defined(DPlatformFamily_macOS))
						NMib::NSys::fg_FreeLibrary(pDll);
#endif
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
