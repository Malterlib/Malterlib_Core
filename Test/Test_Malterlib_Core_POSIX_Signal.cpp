// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Test/Exception>

#ifndef DPlatformFamily_Windows
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

namespace
{
	using namespace NMib;
	using namespace NMib::NStr;

	struct CFailingSignalLoop : NSys::ICIoLoop
	{
		void f_WaitAndDispatch() override
		{
		}
		void f_WaitAndDispatchTimeout(pfp64) override
		{
		}
		bool f_PollAndDispatch() override
		{
			return false;
		}
		void f_Wake() override
		{
		}
		void f_SetOwnerThreadToCurrent() override
		{
		}
		void f_Deregister(NSys::CIoLoopRegistration *) override
		{
		}
		void f_DeregisterAsync(NSys::CIoLoopRegistration *, NFunction::TCFunctionMovable<void ()> &&_fOnClosed) override
		{
			_fOnClosed();
		}

		auto f_Register
			(
				NSys::CIoLoopHandle _Handle, void *, NSys::EIoLoopEvent, NSys::FIoLoopReadinessCallback, bool, NSys::CIoLoopRegisterOptions const &
			)
			-> NSys::CIoLoopRegistration * override
		{
			m_PipeDescriptor = int(_Handle);
			m_bSawPipe = fstat(m_PipeDescriptor, &m_PipeIdentity) == 0;

			DMibError("Injected signal registration failure");
		}

		bool f_PipeStillOpen() const
		{
			struct stat Current{};

			return fstat(m_PipeDescriptor, &Current) == 0 && Current.st_dev == m_PipeIdentity.st_dev && Current.st_ino == m_PipeIdentity.st_ino;
		}

		int m_PipeDescriptor = -1;
		struct stat m_PipeIdentity{};
		bool m_bSawPipe = false;
	};

	struct CSignalDispatch_Tests : NTest::CTest
	{
		void f_DoTests()
		{
			DMibTestSuite("RegistrationFailure")
			{
				CFailingSignalLoop Loop;
				auto *pPreviousLoop = NSys::fg_GetThreadIoLoop();
				auto *pPreviousOwner = NSys::fg_GetOwnedIoLoop();
				auto Restore = g_OnScopeExit / [&]
					{
						NSys::fg_SetThreadIoLoop(pPreviousLoop);
						NSys::fg_SetOwnedIoLoop(pPreviousOwner);
					}
				;
				NSys::fg_SetThreadIoLoop(&Loop);
				NSys::fg_SetOwnedIoLoop(&Loop);

				for (bool bThreadSignal : {false, true})
				{
					DMibTestPath(bThreadSignal ? "Thread" : "Process");
					umint nReleased = 0;
					for (umint i = 0; i < 8; ++i)
					{
						DMibTestPath("Attempt {}"_f << i);
						auto Lifetime = g_OnScopeExitShared / [&]
							{
								++nReleased;
							}
						;
						auto fRegister = [&]
							{
								if (bThreadSignal)
									return NSys::fg_System_RegisterForThreadSignal(SIGWINCH, [Lifetime] {});

								return NSys::fg_System_RegisterForSignal(SIGWINCH, [Lifetime] {});
							}
						;
						DMibExpectExceptionType(fRegister(), NException::CException);
						DMibExpectTrue(Loop.m_bSawPipe);
						DMibExpectFalse(Loop.f_PipeStillOpen());
						Lifetime.f_Clear();
						DMibExpect(nReleased, ==, i + 1);
					}

				}
			};

			DMibTestSuite("CoalescingAndRemoval")
			{
				struct sigaction Ignore{};
				struct sigaction Previous{};
				// Darwin can deliver the replaced disposition during concurrent raises; keep the fallback callable.
				Ignore.sa_handler = [](int) {};
				sigemptyset(&Ignore.sa_mask);
				DMibAssert(sigaction(SIGWINCH, &Ignore, &Previous), ==, 0);
				auto RestoreSignal = g_OnScopeExit / [&]
					{
						sigaction(SIGWINCH, &Previous, nullptr);
					}
				;

				auto *pLoop = NSys::fg_CreateIoLoop();
				DMibAssertTrue(pLoop != nullptr);
				auto *pPreviousLoop = NSys::fg_GetThreadIoLoop();
				auto *pPreviousOwner = NSys::fg_GetOwnedIoLoop();
				auto Destroy = g_OnScopeExit / [&]
					{
						NSys::fg_DestroyIoLoop(pLoop);
						NSys::fg_SetThreadIoLoop(pPreviousLoop);
						NSys::fg_SetOwnedIoLoop(pPreviousOwner);
					}
				;
				pLoop->f_SetOwnerThreadToCurrent();
				NSys::fg_SetThreadIoLoop(pLoop);

				auto fPollUntil = [&](auto const &_fReady)
					{
						for (umint i = 0; i < 5000 && !_fReady(); ++i)
						{
							pLoop->f_PollAndDispatch();
							if (!_fReady())
								NSys::fg_Thread_Sleep(0.001);
						}

						return _fReady();
					}
				;

				umint nFirst = 0;
				umint nSecond = 0;
				umint nOtherSignal = 0;
				auto First = NSys::fg_System_RegisterForSignal(SIGWINCH, [&] { ++nFirst; });
				auto Second = NSys::fg_System_RegisterForSignal(SIGWINCH, [&] { ++nSecond; });
				auto Other = NSys::fg_System_RegisterForSignal(SIGUSR2, [&] { ++nOtherSignal; });

				for (umint i = 0; i < 16; ++i)
				{
					raise(SIGWINCH);
					raise(SIGUSR2);
				}
				DMibAssertTrue(fPollUntil([&] { return nFirst != 0 && nOtherSignal == 16; }));

				{
					DMibTestPath("InitialBurst");

					DMibExpect(nFirst, ==, 1);
					DMibExpect(nSecond, ==, 1);
					DMibExpect(nOtherSignal, ==, 16);
				}

				First.f_Clear();
				raise(SIGWINCH);
				DMibAssertTrue(fPollUntil([&] { return nSecond == 2; }));

				{
					DMibTestPath("OneSubscriberRemoved");

					DMibExpect(nFirst, ==, 1);
					DMibExpect(nSecond, ==, 2);
				}

				Other.f_Clear();
				Second.f_Clear();
				pLoop->f_DrainForShutdown();
				raise(SIGWINCH);
				pLoop->f_PollAndDispatch();

				{
					DMibTestPath("AllSubscribersRemoved");

					DMibExpect(nSecond, ==, 2);
				}

				{
					DMibTestPath("PendingSignalOnRemoval");
					auto Old = NSys::fg_System_RegisterForSignal(SIGWINCH, [] {});
					raise(SIGWINCH);
					Old.f_Clear();
					pLoop->f_DrainForShutdown();

					umint nReopened = 0;
					auto Reopened = NSys::fg_System_RegisterForSignal(SIGWINCH, [&] { ++nReopened; });
					raise(SIGWINCH);

					DMibAssertTrue(fPollUntil([&] { return nReopened != 0; }));
					DMibExpect(nReopened, ==, 1);
					Reopened.f_Clear();
					pLoop->f_DrainForShutdown();
				}

				auto pSender = NThread::CThreadObject::fs_StartThread
					(
						[](NThread::CThreadObject *_pThread) -> aint
						{
							while (_pThread->f_GetState() != NThread::EThreadState_EventWantQuit)
							{
								raise(SIGWINCH);
								NSys::fg_Thread_Yield();
							}

							return 0;
						}
						, "Signal teardown test"
					)
				;
				auto StopSender = g_OnScopeExit / [&]
					{
						pSender->f_Stop(true);
					}
				;

				for (umint i = 0; i < 64; ++i)
				{
					DMibTestPath("DescriptorReuse {}"_f << i);
					auto Subscription = NSys::fg_System_RegisterForSignal(SIGWINCH, [] {});
					pLoop->f_PollAndDispatch();
					Subscription.f_Clear();
					pLoop->f_DrainForShutdown();

					int Pipe[2];
					DMibAssert(pipe(Pipe), ==, 0);
					auto Close = g_OnScopeExit / [&]
						{
							close(Pipe[0]);
							close(Pipe[1]);
						}
					;
					DMibAssert(fcntl(Pipe[0], F_SETFL, O_NONBLOCK), ==, 0);
					NSys::fg_Thread_Yield();
					char Byte;
					auto nRead = read(Pipe[0], &Byte, 1);
					int Error = errno;

					DMibExpect(nRead, ==, -1);
					DMibExpect(Error, ==, EAGAIN);
				}
			};
		}
	};

	DMibTestRegister(CSignalDispatch_Tests, Malterlib::Core);
}
#endif
