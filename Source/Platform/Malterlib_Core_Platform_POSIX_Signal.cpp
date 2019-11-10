// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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
				NAtomic::TCAtomic<mint> m_nPending;
				NContainer::TCLinkedList<NFunction::TCFunctionMutable<void ()>> m_Functions;
				bool m_bInstalled = false;
			};
			
			NThread::CMutual m_Lock;
			CSignalHandlers m_SignalHandlers[NSIG];
			NAtomic::TCAtomic<bool> m_bWasDestroyed;
			NStorage::TCUniquePointer<NThread::CThreadObject> m_pThread;
			
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
		};
		
		constinit TCSubSystem<CSubSystem_Core_Signal, ESubSystemDestruction_BeforeMemoryManager> g_SubSystem_Core_Signal = {DAggregateInit};

		void CSubSystem_Core_Signal::fs_SignalHandler(int _Signal)
		{
			if (_Signal > NSIG || _Signal < 0)
				return; // Invalid signal
			auto &SubSystem = *g_SubSystem_Core_Signal;
			auto CallOld = g_OnScopeExit > [&]
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
										for (mint i = 0; i < nPending; ++i)
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
		auto pOnExit = g_OnScopeExitShared > [pFunction, _Signal]() mutable
			{
				auto &SubSystem = *g_SubSystem_Core_Signal;
				if (SubSystem.m_bWasDestroyed.f_Load())
					return;
				
				DMibLock(SubSystem.m_Lock);
				auto &SignalHandler = SubSystem.m_SignalHandlers[_Signal];
				SignalHandler.m_Functions.f_Remove(*pFunction);
				if (SignalHandler.m_Functions.f_IsEmpty())
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
