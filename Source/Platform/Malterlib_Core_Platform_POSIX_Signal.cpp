// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

#include <signal.h>

namespace NMib::NSys
{
	namespace
	{
		struct CSubSystem_Core_Signal : public CSubSystem
		{
			struct CSignalHandlers
			{
				void (*m_fOldSignal)(int) = nullptr;
				NAtomic::TCAtomic<umint> m_nPending;
				NAtomic::TCAtomic<umint> m_nThreadSignals;
				NContainer::TCLinkedList<NFunction::TCFunctionMutable<void ()>> m_Functions;
				bool m_bInstalled = false;
			};

			struct CThreadLocal
			{
				~CThreadLocal()
				{
					DMibFastCheck(!m_pThreadHandler); // Handler should have been unregistered already
				}

				NFunction::TCFunctionMutable<void ()> m_ThreadHandler;
				NAtomic::TCAtomic<NFunction::TCFunctionMutable<void ()> *> m_pThreadHandler = nullptr;
				int m_ThreadSignal = 0;
			};

			~CSubSystem_Core_Signal()
			{
				m_bWasDestroyed.f_Store(true);
			}

			void f_DestroyThreadSpecific() override
			{
				if (m_pThread)
				{
					m_pThread->f_Stop();
					m_pThread.f_Clear();
				}
			}

			static void fs_SignalHandler(int _Signal);

			NThread::CMutual m_Lock;
			CSignalHandlers m_SignalHandlers[NSIG];
			NAtomic::TCAtomic<bool> m_bWasDestroyed;
			NStorage::TCUniquePointer<NThread::CThreadObject> m_pThread;

			NThread::TCThreadLocal<CThreadLocal> m_ThreadLocal;
		};

		constinit TCSubSystem<CSubSystem_Core_Signal, ESubSystemDestruction_BeforeMemoryManager> g_SubSystem_Core_Signal = {DAggregateInit};

		void CSubSystem_Core_Signal::fs_SignalHandler(int _Signal)
		{
			if (_Signal > NSIG || _Signal < 0)
				return; // Invalid signal

			auto OldErrNo = errno;
			auto CleanupErrno = g_OnScopeExit / [&]
				{
					errno = OldErrNo;
				}
			;

			auto &SubSystem = *g_SubSystem_Core_Signal;

			auto &ThreadLocal = *SubSystem.m_ThreadLocal;

			auto pThreadHandler = ThreadLocal.m_pThreadHandler.f_Load();
			if (_Signal == ThreadLocal.m_ThreadSignal && pThreadHandler)
			{
				sigset_t BlockSet;
				sigset_t OldSet;
				sigemptyset(&BlockSet);
				sigaddset(&BlockSet, _Signal);
				pthread_sigmask(SIG_BLOCK, &BlockSet, &OldSet);

				auto Cleanup = g_OnScopeExit / [&]
					{
						pthread_sigmask(SIG_SETMASK, &OldSet, nullptr);
					}
				;

				(*pThreadHandler)();
			}

			auto CallOld = g_OnScopeExit / [&]
				{
					auto fOld = SubSystem.m_SignalHandlers[_Signal].m_fOldSignal;
					if (!fOld)
						return;
					fOld(_Signal);
				}
			;
			if (!SubSystem.m_pThread)
				return;
			if (SubSystem.m_SignalHandlers[_Signal].m_nPending.f_FetchAdd(1) == 0)
				SubSystem.m_pThread->m_EventWantQuit.f_Signal();
		}
	}

	NMib::COnScopeExitShared fg_System_RegisterForThreadSignal(int _Signal, NFunction::TCFunctionMutable<void ()> &&_fOnSignal)
	{
		if (_Signal > NSIG || _Signal < 0)
			DMibError("Invalid signal");

		auto &SubSystem = *g_SubSystem_Core_Signal;
		auto &ThreadLocal = *SubSystem.m_ThreadLocal;

		if (ThreadLocal.m_pThreadHandler)
			DMibError("Only a single thread signal handler can be installed");

		ThreadLocal.m_ThreadSignal = _Signal;
		ThreadLocal.m_ThreadHandler = fg_Move(_fOnSignal);
		ThreadLocal.m_pThreadHandler.f_Store(&ThreadLocal.m_ThreadHandler);

		{
			DMibLock(SubSystem.m_Lock);

			auto &SignalHandler = SubSystem.m_SignalHandlers[_Signal];
			++SignalHandler.m_nThreadSignals;

			if (!SignalHandler.m_bInstalled)
			{
				SignalHandler.m_bInstalled = true;
				SignalHandler.m_fOldSignal = signal(_Signal, &CSubSystem_Core_Signal::fs_SignalHandler);
			}
		}

		auto pOnExit = g_OnScopeExitShared / [_Signal]() mutable
			{
				auto &SubSystem = *g_SubSystem_Core_Signal;
				if (SubSystem.m_bWasDestroyed.f_Load())
					return;

				auto &ThreadLocal = *SubSystem.m_ThreadLocal;

				ThreadLocal.m_pThreadHandler.f_Store(nullptr);
				ThreadLocal.m_ThreadHandler.f_Clear();

				DMibLock(SubSystem.m_Lock);
				auto &SignalHandler = SubSystem.m_SignalHandlers[_Signal];
				umint ThreadSignals = SignalHandler.m_nThreadSignals.f_FetchSub(1) - 1;
				if (SignalHandler.m_Functions.f_IsEmpty() && ThreadSignals == 0 )
				{
					SignalHandler.m_bInstalled = false;
					signal(_Signal, SignalHandler.m_fOldSignal);
					SignalHandler.m_fOldSignal = nullptr;
				}
			}
		;
		return pOnExit;
	}

	NMib::COnScopeExitShared fg_System_RegisterForSignal(int _Signal, NFunction::TCFunctionMutable<void ()> &&_fOnSignal)
	{
		if (_Signal > NSIG || _Signal < 0)
			DMibError("Invalid signal");

		auto &SubSystem = *g_SubSystem_Core_Signal;
		NFunction::TCFunctionMutable<void ()> *pFunction;
		{
			DMibLock(SubSystem.m_Lock);
			auto &SignalHandler = SubSystem.m_SignalHandlers[_Signal];
			pFunction = &SignalHandler.m_Functions.f_Insert(fg_Move(_fOnSignal));
			if (!SubSystem.m_pThread)
			{
				SubSystem.m_pThread = NThread::CThreadObject::fs_StartThread
					(
						[](NThread::CThreadObject* _pThread) -> aint
						{
							while (_pThread->f_GetState() != NThread::EThreadState_EventWantQuit)
							{
								auto &SubSystem = *g_SubSystem_Core_Signal;
								{
									DMibLock(SubSystem.m_Lock);
									for (auto &Handler : SubSystem.m_SignalHandlers)
									{
										auto nPending = Handler.m_nPending.f_Exchange(0);
										for (umint i = 0; i < nPending; ++i)
										{
											for (auto &fOnSignal : Handler.m_Functions)
												fOnSignal();
										}
									}
								}
								_pThread->m_EventWantQuit.f_Wait();
							}
							return 0;
						}
						, "Signal handlers"
					)
				;
			}
			if (!SignalHandler.m_bInstalled)
			{
				SignalHandler.m_bInstalled = true;
				SignalHandler.m_fOldSignal = signal(_Signal, &CSubSystem_Core_Signal::fs_SignalHandler);
			}
		}

		auto pOnExit = g_OnScopeExitShared / [pFunction, _Signal]() mutable
			{
				auto &SubSystem = *g_SubSystem_Core_Signal;
				if (SubSystem.m_bWasDestroyed.f_Load())
					return;

				DMibLock(SubSystem.m_Lock);
				auto &SignalHandler = SubSystem.m_SignalHandlers[_Signal];
				SignalHandler.m_Functions.f_Remove(*pFunction);
				if (SignalHandler.m_Functions.f_IsEmpty() && SignalHandler.m_nThreadSignals.f_Load() == 0)
				{
					SignalHandler.m_bInstalled = false;
					signal(_Signal, SignalHandler.m_fOldSignal);
					SignalHandler.m_fOldSignal = nullptr;
				}
			}
		;
		return pOnExit;
	}
}
