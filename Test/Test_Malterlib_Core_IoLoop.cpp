// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#if defined(DPlatformFamily_macOS) || defined(DPlatformFamily_Linux)

#include "../Source/Malterlib_Core_IoLoop_Internal.h"

#include <sys/socket.h>
#include <unistd.h>

namespace
{
	using namespace NMib;
	using namespace NMib::NMemory;

	struct CCloseOnlyEventCounts
	{
		umint m_nReadClosed = 0; // Half-close reports without a hangup
		umint m_nClosed = 0; // Hangup or write side closed
		umint m_nOther = 0;
	};

	void fg_CountCloseOnlyEvents(void *_pToken, NSys::EIoLoopEvent _Events, int)
	{
		auto &Counts = *(CCloseOnlyEventCounts *)_pToken;
		if (fg_IsSet(_Events, NSys::EIoLoopEvent::mc_Hup) || fg_IsSet(_Events, NSys::EIoLoopEvent::mc_WriteClosed))
			++Counts.m_nClosed;
		else if (fg_IsSet(_Events, NSys::EIoLoopEvent::mc_ReadClosed))
			++Counts.m_nReadClosed;
		else
			++Counts.m_nOther;
	}

	struct CIoLoop_Tests : NMib::NTest::CTest
	{
		void f_DoTests()
		{
			// Every registration hears the peer half-close and the later full close whatever readiness it
			// asked for, and one without readiness interest hears nothing else
			DMibTestSuite("CloseNotifications")
			{
				NSys::ICIoLoop *pLoop = fg_CreatePlatformIoLoop();
				auto DeleteLoop = g_OnScopeExit / [&]
					{
						fg_DeleteObject(CAllocator_NonTrackedHeap(), pLoop);
					}
				;
				pLoop->f_SetOwnerThreadToCurrent();

				auto fDispatchFor = [&](umint _nPasses)
					{
						for (umint i = 0; i < _nPasses; ++i)
						{
							pLoop->f_PollAndDispatch();
							NSys::fg_Thread_Sleep(0.002);
						}
					}
				;

				struct CMaskCase
				{
					char const *m_pName;
					NSys::EIoLoopEvent m_Mask;
				};
				constexpr CMaskCase c_Cases[] =
					{
						{"None", NSys::EIoLoopEvent::mc_None}
						, {"Read", NSys::EIoLoopEvent::mc_Read}
						, {"Write", NSys::EIoLoopEvent::mc_Write}
					}
				;

				for (auto const &Case : c_Cases)
				{
					DMibTestPath(Case.m_pName);

					int Sockets[2];
					DMibTest(DMibExpr(socketpair(AF_UNIX, SOCK_STREAM, 0, Sockets)) == DMibExpr(0))(NTest::ETest_FailAndStop);
					auto CloseSockets = g_OnScopeExit / [&]
						{
							close(Sockets[0]);
							close(Sockets[1]);
						}
					;

					CCloseOnlyEventCounts Counts;
					auto *pRegistration = pLoop->f_Register(Sockets[0], &Counts, Case.m_Mask, &fg_CountCloseOnlyEvents, false, NSys::CIoLoopRegisterOptions());
					fDispatchFor(5);

					// A backend may report the half-close through both its readiness and its close watch,
					// but a rearmed watch would report it on every pass
					shutdown(Sockets[1], SHUT_WR);
					fDispatchFor(50);
					umint nReadClosedAfterPeerHalfClose = Counts.m_nReadClosed;
					DMibExpect(nReadClosedAfterPeerHalfClose, >=, umint(1));
					DMibExpect(nReadClosedAfterPeerHalfClose, <=, umint(2));
					umint nClosedAfterPeerHalfClose = Counts.m_nClosed;
					DMibExpect(nClosedAfterPeerHalfClose, ==, umint(0));

					shutdown(Sockets[0], SHUT_WR);
					fDispatchFor(50);
					umint nClosedAfterLocalShutdown = Counts.m_nClosed;
					DMibExpect(nClosedAfterLocalShutdown, ==, umint(1));
					umint nReadClosedAfterLocalShutdown = Counts.m_nReadClosed;
					DMibExpect(nReadClosedAfterLocalShutdown, ==, nReadClosedAfterPeerHalfClose);

					if (Case.m_Mask == NSys::EIoLoopEvent::mc_None)
					{
						umint nOther = Counts.m_nOther;
						DMibExpect(nOther, ==, umint(0));
					}

					pLoop->f_Deregister(pRegistration);
				}
			};
		}
	};
}

DMibTestRegister(CIoLoop_Tests, Malterlib::Core);

#endif
