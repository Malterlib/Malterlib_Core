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
				// Normally already closed by the last unregistration; this covers a caller that
				// leaked its subscription
				DMibLock(m_Lock);
				fp_CloseDispatch();
			}

			static void fs_SignalHandler(int _Signal);

			// What a signal is delivered through while any handler is registered. Held by a shared
			// pointer so an asynchronous deregistration's continuation can outlive the detach: a
			// readiness callback may still be in flight until the loop acknowledges the removal
			struct CDispatch
			{
				// A signal handler may only use async signal safe facilities. A nonblocking write
				// to a pipe is one; the framework's events and semaphores are not, since their
				// signal paths take locks. A byte already queued guarantees the wake, so a write
				// that fails on a full pipe loses nothing
				int m_Pipe[2] = {-1, -1};

				// The loop watching the read end. A caller registering under a CIoLoopCreateScope
				// gets its own loop's thread; everyone else shares the process wide poller. This
				// subsystem never owns a thread either way
				NSys::ICIoLoop *m_pLoop = nullptr;
				NSys::CIoLoopRegistration *m_pRegistration = nullptr;
			};

			// Opens the pipe and starts watching it. Called under m_Lock for the first registration
			void fp_OpenDispatch();
			// Stops watching and releases the pipe. Called under m_Lock when the last one goes away,
			// so nothing is left registered with a loop the concurrency manager may destroy later
			void fp_CloseDispatch();
			// Drains the pipe and runs the pending handlers, on the loop's thread
			void f_DispatchPending();
			static void fs_OnPipeReadable(void *_pToken, NSys::EIoLoopEvent _Events, int _Error);
			// Opens a nonblocking, close on exec self pipe. Throws with the errno on failure
			static void fs_OpenPipe(ch8 const *_pWhat, int (&o_Pipe)[2]);

			NThread::CMutual m_Lock;
			CSignalHandlers m_SignalHandlers[NSIG];
			NAtomic::TCAtomic<bool> m_bWasDestroyed;

			NStorage::TCSharedPointer<CDispatch> m_pDispatch;
			umint m_nDispatchedHandlers = 0;

			NThread::TCThreadLocal<CThreadLocal> m_ThreadLocal;
		};

		// Read by the signal handler, which has no context of its own and may not take a lock to
		// find one. Published before the handler is installed and cleared after it is removed
		constinit NAtomic::TCAtomic<int> gs_SignalPipeWrite = -1;

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

			// Only the thread signal feature needs this, and constructing a thread local from a
			// handler allocates, which a handler may not do. f_TryGet answers without constructing,
			// so a thread that never registered one is simply skipped
			auto *pThreadLocalEntry = SubSystem.m_ThreadLocal.f_TryGet();

			auto pThreadHandler = pThreadLocalEntry ? pThreadLocalEntry->m_pThreadHandler.f_Load() : nullptr;
			if (pThreadHandler && _Signal == pThreadLocalEntry->m_ThreadSignal)
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
			int PipeWrite = gs_SignalPipeWrite.f_Load(NAtomic::gc_MemoryOrder_Relaxed);
			if (PipeWrite < 0)
				return;

			if (SubSystem.m_SignalHandlers[_Signal].m_nPending.f_FetchAdd(1) == 0)
			{
				// The only wake this handler is allowed to perform. Whoever drains the pipe reads
				// the pending counts, so which byte arrives does not matter
				ch8 Byte = 's';
				(void)!write(PipeWrite, &Byte, 1);
			}
		}

		void CSubSystem_Core_Signal::f_DispatchPending()
		{
			DMibLock(m_Lock);

			if (m_pDispatch)
			{
				ch8 Bytes[64];
				while (read(m_pDispatch->m_Pipe[0], Bytes, sizeof(Bytes)) > 0)
					; // Drained in whole so a burst of signals coalesces into one pass
			}

			for (auto &Handler : m_SignalHandlers)
			{
				auto nPending = Handler.m_nPending.f_Exchange(0);
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

		void CSubSystem_Core_Signal::fp_OpenDispatch()
		{
			if (m_pDispatch)
				return;

			NStorage::TCSharedPointer<CDispatch> pDispatch = fg_Construct();

			int Pipes[2];
			fs_OpenPipe("signal dispatch", Pipes);

			pDispatch->m_Pipe[0] = Pipes[0];
			pDispatch->m_Pipe[1] = Pipes[1];

			// A caller that registers inside a CIoLoopCreateScope gets the dispatch on that loop's
			// thread; everyone else lands on the process wide poller, which is shared with the
			// sockets. Either way this subsystem owns no thread
			pDispatch->m_pLoop = NSys::fg_GetThreadIoLoop();
			if (!pDispatch->m_pLoop)
				pDispatch->m_pLoop = NSys::fg_GetSharedIoLoop();

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
			gs_SignalPipeWrite.f_Store(Pipes[1], NAtomic::gc_MemoryOrder_Relaxed);
		}

		void CSubSystem_Core_Signal::fp_CloseDispatch()
		{
			if (!m_pDispatch)
				return;

			// No further handler can write once this is cleared, and the signal dispositions are
			// restored by the caller that dropped the last registration
			gs_SignalPipeWrite.f_Store(-1, NAtomic::gc_MemoryOrder_Relaxed);

			auto pDispatch = fg_Move(m_pDispatch);
			m_pDispatch.f_Clear();

			// Asynchronously, because a loop binding can be handed out before its queue has claimed
			// the loop and nothing may block on that. The pipe stays open, and pDispatch alive,
			// until the loop reports that no callback can be in flight
			auto *pLoop = pDispatch->m_pLoop;
			auto *pRegistration = pDispatch->m_pRegistration;

			pLoop->f_DeregisterAsync
				(
					pRegistration
					, [pDispatch = fg_Move(pDispatch)]() mutable
					{
						for (auto &Descriptor : pDispatch->m_Pipe)
						{
							if (Descriptor >= 0)
								close(Descriptor);

							Descriptor = -1;
						}

						pDispatch.f_Clear();
					}
				)
			;
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
			++SubSystem.m_nDispatchedHandlers;
			SubSystem.fp_OpenDispatch();
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

				// Nothing is delivered any more, so the pipe and whatever watches it go away. A
				// registration left on a concurrency loop would outlive the loop, which is torn
				// down long before this subsystem
				if (--SubSystem.m_nDispatchedHandlers == 0)
					SubSystem.fp_CloseDispatch();
			}
		;
		return pOnExit;
	}
}
