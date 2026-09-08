// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

#include <Mib/Core/PlatformSpecific/PosixFork>
#ifdef DPlatformFamily_Linux
#	include <Mib/Core/PlatformSpecific/LinuxOptional>
#endif

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

namespace NMib::NSys
{
	namespace
	{
		struct CSubSystem_Core_Signal : public CSubSystem
		{
			struct CSignalHandlers
			{
				NAtomic::TCAtomic<void (*)(int)> m_fOldSignal = nullptr;
				NAtomic::TCAtomic<umint> m_nPending;
				NAtomic::TCAtomic<umint> m_nThreadSignals;
				NContainer::TCLinkedList<NFunction::TCFunctionMutable<void ()>> m_Functions;
				bool m_bInstalled = false;
			};

			struct CSignalPipe
			{
				~CSignalPipe();

				int m_Pipe[2] = {-1, -1};
			};

			struct CThreadDispatch : CSignalPipe
			{
				NSys::ICIoLoop *m_pLoop = nullptr;
				NSys::CIoLoopRegistration *m_pRegistration = nullptr;
				NFunction::TCFunctionMutable<void ()> m_fOnSignal;
				bool m_bClosing = false; // Only accessed on the registering thread, which also drives this loop.
			};

			struct CThreadLocal
			{
				~CThreadLocal()
				{
					DMibFastCheck(!m_pDispatch);
				}

				NStorage::TCSharedPointer<CThreadDispatch> m_pDispatch;

				NAtomic::TCAtomic<int> m_PipeWrite = -1; // Signal-handler descriptor: publish after registration and clear before removal; no locks or shared-pointer access.
				int m_ThreadSignal = 0;
			};

			~CSubSystem_Core_Signal()
			{
				m_bWasDestroyed.f_Store(true);
			}

			void f_DestroyThreadSpecific() override
			{
				// Release the dispatch registration before its owning loop is destroyed.
				DMibLock(m_Lock);
				fp_CloseDispatch();
			}

			static void fs_SignalHandler(int _Signal);

			struct CDispatch : CSignalPipe
			{
				NSys::ICIoLoop *m_pLoop = nullptr; // Chosen at first registration; callbacks run on this loop's owner thread.
				NSys::CIoLoopRegistration *m_pRegistration = nullptr;
			};

			void fp_OpenDispatch();
			void fp_CloseDispatch();
			void f_DispatchPending();
			static void fs_OnPipeReadable(void *_pToken, NSys::EIoLoopEvent _Events, int _Error);
			static void fs_OnThreadPipeReadable(void *_pToken, NSys::EIoLoopEvent _Events, int _Error);
			static void fs_OpenPipe(ch8 const *_pWhat, int (&o_Pipe)[2]);

			NThread::CMutual m_Lock;
			CSignalHandlers m_SignalHandlers[NSIG];
			NAtomic::TCAtomic<bool> m_bWasDestroyed;

			NStorage::TCSharedPointer<CDispatch> m_pDispatch;
			umint m_nDispatchedHandlers = 0;

			NThread::TCThreadLocal<CThreadLocal> m_ThreadLocal;
		};

		// Published before installing signal handlers and cleared after removal; handlers read it without locks.
		constinit NAtomic::TCAtomic<int> gs_SignalPipeWrite = -1;
		constinit NAtomic::TCAtomic<umint> g_nSignalWriters = 0;

		constinit TCSubSystem<CSubSystem_Core_Signal, ESubSystemDestruction_BeforeMemoryManager> g_SubSystem_Core_Signal = {DAggregateInit};

		void fg_WaitForSignalWriters()
		{
			NThread::CThreadSpinWaiter Waiter;
			while (g_nSignalWriters.f_Load())
				Waiter.f_Wait();
		}

		// Deregistration retains the pipe through the last loop callback. The writer count also
		// covers native handlers that loaded a descriptor before its publication was cleared.
		CSubSystem_Core_Signal::CSignalPipe::~CSignalPipe()
		{
			fg_WaitForSignalWriters();

			for (int Descriptor : m_Pipe)
			{
				if (Descriptor >= 0)
					close(Descriptor);
			}
		}

		void CSubSystem_Core_Signal::fs_SignalHandler(int _Signal)
		{
			if (_Signal >= NSIG || _Signal <= 0)
				return; // Invalid signal

			auto OldErrNo = errno;
			auto CleanupErrno = g_OnScopeExit / [&]
				{
					errno = OldErrNo;
				}
			;

			auto &SubSystem = *g_SubSystem_Core_Signal;

			auto CallOld = g_OnScopeExit / [&]
				{
					auto fOld = SubSystem.m_SignalHandlers[_Signal].m_fOldSignal.f_Load();
					if (!fOld || fOld == SIG_IGN || fOld == SIG_ERR)
						return;
					fOld(_Signal);
				}
			;

			// Sequential consistency pairs publication removal with the writer count before close.
			++g_nSignalWriters;
			auto FinishWrite = g_OnScopeExit / []
				{
					--g_nSignalWriters;
				}
			;

			// f_TryGet must not allocate: thread-local construction is unsafe in a signal handler.
			auto *pThreadLocalEntry = SubSystem.m_ThreadLocal.f_TryGet();

			if (pThreadLocalEntry && _Signal == pThreadLocalEntry->m_ThreadSignal)
			{
				// Wake this thread's loop; ordinary framework callbacks cannot run inside the signal handler.
				int ThreadPipeWrite = pThreadLocalEntry->m_PipeWrite.f_Load();
				if (ThreadPipeWrite >= 0)
				{
					ch8 Byte = 't';
					(void)!write(ThreadPipeWrite, &Byte, 1);
				}
			}

			int PipeWrite = gs_SignalPipeWrite.f_Load();
			if (PipeWrite < 0)
				return;

			if (SubSystem.m_SignalHandlers[_Signal].m_nPending.f_FetchAdd(1) == 0)
			{
				// Only the wake matters; delivery reads the pending counts rather than the byte value.
				ch8 Byte = 's';
				(void)!write(PipeWrite, &Byte, 1);
			}
		}

		// Runs pending handlers on the dispatch loop's thread.
		void CSubSystem_Core_Signal::f_DispatchPending()
		{
			DMibLock(m_Lock);

			if (m_pDispatch)
			{
				ch8 Bytes[64];
				while (read(m_pDispatch->m_Pipe[0], Bytes, sizeof(Bytes)) > 0)
					; // Drain fully to coalesce signal bursts.

				m_pDispatch->m_pLoop->f_RequestReadiness(m_pDispatch->m_pRegistration, NSys::EIoLoopEvent::mc_Read);
			}

			for (umint iSignal = 0; iSignal < NSIG; ++iSignal)
			{
				auto &Handler = m_SignalHandlers[iSignal];
				auto nPending = Handler.m_nPending.f_Exchange(0);
				if (iSignal == SIGWINCH && nPending)
					nPending = 1;

				for (umint i = 0; i < nPending; ++i)
				{
					for (auto &fOnSignal : Handler.m_Functions)
						fOnSignal();
				}
			}
		}

		void CSubSystem_Core_Signal::fs_OnPipeReadable(void *_pToken, NSys::EIoLoopEvent _Events, int _Error)
		{
			if (!fg_IsSet(_Events, NSys::EIoLoopEvent::mc_Read))
				return;

			static_cast<CSubSystem_Core_Signal *>(_pToken)->f_DispatchPending();
		}

		// Creates a nonblocking, close-on-exec pipe; throws on failure.
		void CSubSystem_Core_Signal::fs_OpenPipe(ch8 const *_pWhat, int (&o_Pipe)[2])
		{
			using namespace NMib::NStr;

#ifdef DPlatformFamily_Linux
			if (NLocal::g_f_pipe2)
			{
				if (NLocal::g_f_pipe2(o_Pipe, O_CLOEXEC | O_NONBLOCK))
					DMibError("pipe2 ({}) failed with errno {}"_f << _pWhat << errno);
			}
			else
#endif
			{
				// The pipes must not leak into a forked child
				DMibLock(::NMib::NPlatform::fg_ForkLock());
				if (pipe(o_Pipe))
					DMibError("pipe ({}) failed with errno {}"_f << _pWhat << errno);

				fcntl(o_Pipe[0], F_SETFD, fcntl(o_Pipe[0], F_GETFD) | FD_CLOEXEC);
				fcntl(o_Pipe[1], F_SETFD, fcntl(o_Pipe[1], F_GETFD) | FD_CLOEXEC);
				fcntl(o_Pipe[0], F_SETFL, fcntl(o_Pipe[0], F_GETFL) | O_NONBLOCK);
				fcntl(o_Pipe[1], F_SETFL, fcntl(o_Pipe[1], F_GETFL) | O_NONBLOCK);
			}

#ifdef F_SETNOSIGPIPE
			fcntl(o_Pipe[0], F_SETNOSIGPIPE, 1);
			fcntl(o_Pipe[1], F_SETNOSIGPIPE, 1);
#endif
		}

		// Runs on the registering thread's loop, outside the native signal handler.
		void CSubSystem_Core_Signal::fs_OnThreadPipeReadable(void *_pToken, NSys::EIoLoopEvent _Events, int _Error)
		{
			if (!fg_IsSet(_Events, NSys::EIoLoopEvent::mc_Read))
				return;

			auto &Dispatch = *static_cast<CThreadDispatch *>(_pToken);
			if (Dispatch.m_bClosing)
				return;

			bool bReceivedSignal = false;
			ch8 Bytes[64];
			while (true)
			{
				auto nRead = read(Dispatch.m_Pipe[0], Bytes, sizeof(Bytes));
				if (nRead > 0)
					bReceivedSignal = true;
				else if (nRead < 0 && errno == EINTR)
					continue;
				else
					break;
			}

			Dispatch.m_pLoop->f_RequestReadiness(Dispatch.m_pRegistration, NSys::EIoLoopEvent::mc_Read);
			if (bReceivedSignal)
				Dispatch.m_fOnSignal();
		}

		// Requires m_Lock; called for the first registration.
		void CSubSystem_Core_Signal::fp_OpenDispatch()
		{
			if (m_pDispatch)
				return;

			NStorage::TCSharedPointer<CDispatch> pDispatch = fg_Construct();

			int Pipes[2];
			fs_OpenPipe("signal dispatch", Pipes);

			pDispatch->m_Pipe[0] = Pipes[0];
			pDispatch->m_Pipe[1] = Pipes[1];

			pDispatch->m_pLoop = NSys::fg_GetThreadIoLoop();
			if (!pDispatch->m_pLoop)
				pDispatch->m_pLoop = NSys::fg_GetSharedIoLoop();
			if (!pDispatch->m_pLoop)
				DMibError("No I/O loop is available for signal dispatch");

			pDispatch->m_pRegistration = pDispatch->m_pLoop->f_Register
				(
					Pipes[0]
					, this
					, NSys::EIoLoopEvent::mc_Read
					, &fs_OnPipeReadable
					, false
				)
			;

			m_pDispatch = fg_Move(pDispatch);

			// Last, so no handler can observe a descriptor before it is being watched
			gs_SignalPipeWrite.f_Store(Pipes[1]);
		}

		// Requires m_Lock and no registered handlers. Removal must finish before the owning loop is destroyed.
		void CSubSystem_Core_Signal::fp_CloseDispatch()
		{
			if (!m_pDispatch)
				return;

			// Stop publishing the descriptor before queuing its asynchronous close.
			gs_SignalPipeWrite.f_Store(-1);
			fg_WaitForSignalWriters();

			// A replacement pipe must not inherit pending counts whose wake byte belonged to this pipe.
			for (auto &Handler : m_SignalHandlers)
				Handler.m_nPending.f_Store(0);

			auto pDispatch = fg_Move(m_pDispatch);

			// A pool loop may not have an owner yet, so deregistration must not block.
			// Keep the pipe and dispatch alive until acknowledgement.
			auto *pLoop = pDispatch->m_pLoop;
			auto *pRegistration = pDispatch->m_pRegistration;

			pLoop->f_DeregisterAsync
				(
					pRegistration
					, [pDispatch = fg_Move(pDispatch)]() mutable
					{
						pDispatch.f_Clear();
					}
				)
			;
		}
	}

	NMib::COnScopeExitShared fg_System_RegisterForThreadSignal(int _Signal, NFunction::TCFunctionMutable<void ()> &&_fOnSignal)
	{
		if (_Signal >= NSIG || _Signal <= 0)
			DMibError("Invalid signal");

		auto &SubSystem = *g_SubSystem_Core_Signal;
		auto &ThreadLocal = *SubSystem.m_ThreadLocal;

		if (ThreadLocal.m_pDispatch)
			DMibError("Only a single thread signal handler can be installed");

		// The registration binding may name another pool thread; use this thread's driven loop to preserve callback affinity.
		NSys::ICIoLoop *pLoop = NSys::fg_GetOwnedIoLoop();
		if (!pLoop)
			DMibError("A thread signal handler can only be installed on a thread that drives an io loop");

		auto pDispatch = NStorage::TCSharedPointer<CSubSystem_Core_Signal::CThreadDispatch>(fg_Construct());
		pDispatch->m_fOnSignal = fg_Move(_fOnSignal);
		pDispatch->m_pLoop = pLoop;

		CSubSystem_Core_Signal::fs_OpenPipe("thread signal", pDispatch->m_Pipe);

		pDispatch->m_pRegistration = pLoop->f_Register
			(
				pDispatch->m_Pipe[0]
				, pDispatch.f_Get()
				, NSys::EIoLoopEvent::mc_Read
				, &CSubSystem_Core_Signal::fs_OnThreadPipeReadable
				, false
			)
		;

		ThreadLocal.m_ThreadSignal = _Signal;
		ThreadLocal.m_pDispatch = pDispatch;

		// Last, so the handler cannot see a descriptor before it is being watched
		ThreadLocal.m_PipeWrite.f_Store(pDispatch->m_Pipe[1]);

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

		auto pOnExit = g_OnScopeExitShared / [_Signal, ThreadUID = fg_Thread_GetCurrentUID()]() mutable
			{
				DMibFastCheck(fg_Thread_GetCurrentUID() == ThreadUID);

				auto &SubSystem = *g_SubSystem_Core_Signal;
				if (SubSystem.m_bWasDestroyed.f_Load())
					return;

				auto &ThreadLocal = *SubSystem.m_ThreadLocal;

				// Clear the signal-handler descriptor before asynchronous deregistration; retain the dispatch until acknowledgement.
				ThreadLocal.m_PipeWrite.f_Store(-1);
				ThreadLocal.m_ThreadSignal = 0;

				if (auto pDispatch = fg_Move(ThreadLocal.m_pDispatch))
				{
					pDispatch->m_bClosing = true;

					auto *pLoop = pDispatch->m_pLoop;
					auto *pRegistration = pDispatch->m_pRegistration;

					pLoop->f_DeregisterAsync
						(
							pRegistration
							, [pDispatch = fg_Move(pDispatch)]() mutable
							{
								pDispatch.f_Clear();
							}
						)
					;
				}

				DMibLock(SubSystem.m_Lock);
				auto &SignalHandler = SubSystem.m_SignalHandlers[_Signal];
				umint ThreadSignals = SignalHandler.m_nThreadSignals.f_FetchSub(1) - 1;
				if (SignalHandler.m_Functions.f_IsEmpty() && ThreadSignals == 0 )
				{
					SignalHandler.m_bInstalled = false;
					signal(_Signal, SignalHandler.m_fOldSignal.f_Load());
					SignalHandler.m_fOldSignal = nullptr;
				}
			}
		;
		return pOnExit;
	}

	// Callbacks run on the selected I/O-loop thread under the subsystem lock. They must not block
	// or change signal subscriptions; dispatch that work to the application's actor or queue.
	NMib::COnScopeExitShared fg_System_RegisterForSignal(int _Signal, NFunction::TCFunctionMutable<void ()> &&_fOnSignal)
	{
		if (_Signal >= NSIG || _Signal <= 0)
			DMibError("Invalid signal");

		auto &SubSystem = *g_SubSystem_Core_Signal;
		NFunction::TCFunctionMutable<void ()> *pFunction;
		{
			DMibLock(SubSystem.m_Lock);
			auto &SignalHandler = SubSystem.m_SignalHandlers[_Signal];
			pFunction = &SignalHandler.m_Functions.f_Insert(fg_Move(_fOnSignal));
			++SubSystem.m_nDispatchedHandlers;
			auto Rollback = g_OnScopeExit / [&]
				{
					SignalHandler.m_Functions.f_Remove(*pFunction);
					if (--SubSystem.m_nDispatchedHandlers == 0)
						SubSystem.fp_CloseDispatch();
				}
			;

			SubSystem.fp_OpenDispatch();
			if (!SignalHandler.m_bInstalled)
			{
				SignalHandler.m_bInstalled = true;
				SignalHandler.m_fOldSignal = signal(_Signal, &CSubSystem_Core_Signal::fs_SignalHandler);
			}

			Rollback.f_Clear();
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
					signal(_Signal, SignalHandler.m_fOldSignal.f_Load());
					SignalHandler.m_fOldSignal = nullptr;
				}

				// Release the last registration before the concurrency loop can be destroyed.
				if (--SubSystem.m_nDispatchedHandlers == 0)
					SubSystem.fp_CloseDispatch();
			}
		;
		return pOnExit;
	}
}
