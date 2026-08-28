// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifdef DPlatformFamily_Linux

#include "../Source/Platform/Malterlib_Core_Platform_Linux_IoLoop_Uring_Internal.h"

#include <unistd.h>

namespace
{
	using namespace NMib;
	using namespace NMib::NMemory;

	struct CCloseEventCounts
	{
		umint m_nReadClosed = 0; // Half-close reports without a hangup
		umint m_nHup = 0;
		umint m_nOther = 0;
	};

	void fg_CountCloseEvents(void *_pToken, NSys::EIoLoopEvent _Events, int)
	{
		auto &Counts = *(CCloseEventCounts *)_pToken;
		if (fg_IsSet(_Events, NSys::EIoLoopEvent::mc_Hup))
			++Counts.m_nHup;
		else if (fg_IsSet(_Events, NSys::EIoLoopEvent::mc_ReadClosed))
			++Counts.m_nReadClosed;
		else
			++Counts.m_nOther;
	}

	struct CIoUring_Tests : NMib::NTest::CTest
	{
		void f_DoTests()
		{
			// The kernel adds EPOLLRDHUP to every poll mask, so a close poll rearmed after the peer
			// half-closed completes at once, and every dispatch pass would report the half-close again.
			// The close watch must still be standing when the local side shuts down afterwards
			DMibTestSuite("HalfCloseReportedOnce")
			{
				auto *pLoop = fg_ConstructObject<CIoLoop_IoUring>(CAllocator_NonTrackedHeap());
				auto DeleteLoop = g_OnScopeExit / [&]
					{
						fg_DeleteObject(CAllocator_NonTrackedHeap(), pLoop);
					}
				;
				if (!pLoop->f_IsRingCreated())
					return;

				pLoop->f_SetOwnerThreadToCurrent();

				int Sockets[2];
				DMibTest(DMibExpr(socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, Sockets)) == DMibExpr(0))(NTest::ETest_FailAndStop);
				auto CloseSockets = g_OnScopeExit / [&]
					{
						close(Sockets[0]);
						close(Sockets[1]);
					}
				;

				CCloseEventCounts Counts;
				auto *pRegistration = pLoop->f_Register(Sockets[0], &Counts, NSys::EIoLoopEvent::mc_None, &fg_CountCloseEvents, false, NSys::CIoLoopRegisterOptions());
				pLoop->f_PollAndDispatch();

				auto fDispatchFor = [&](umint _nPasses)
					{
						for (umint i = 0; i < _nPasses; ++i)
						{
							pLoop->f_PollAndDispatch();
							NSys::fg_Thread_Sleep(0.002);
						}
					}
				;

				shutdown(Sockets[1], SHUT_WR);
				fDispatchFor(50);
				umint nReadClosedAfterPeerHalfClose = Counts.m_nReadClosed;
				DMibExpect(nReadClosedAfterPeerHalfClose, ==, umint(1));
				umint nHupAfterPeerHalfClose = Counts.m_nHup;
				DMibExpect(nHupAfterPeerHalfClose, ==, umint(0));

				shutdown(Sockets[0], SHUT_WR);
				fDispatchFor(50);
				umint nHupAfterLocalShutdown = Counts.m_nHup;
				DMibExpect(nHupAfterLocalShutdown, ==, umint(1));
				umint nReadClosedAfterLocalShutdown = Counts.m_nReadClosed;
				DMibExpect(nReadClosedAfterLocalShutdown, ==, umint(1));
				umint nOther = Counts.m_nOther;
				DMibExpect(nOther, ==, umint(0));

				pLoop->f_Deregister(pRegistration);
			};
			DMibTestSuite("SendBundleConsumption")
			{
				auto fCheck = [](char const *_pCase, uint16 _Head, uint16 _KernelHead, umint _nCovered, bool _bLost)
					{
						DMibTestPath(_pCase);

						CIoUringBuf Entries[8] = {};
						Entries[_Head & 7].m_Len = 100;
						Entries[(_Head + 1) & 7].m_Len = 200;
						Entries[(_Head + 2) & 7].m_Len = 400;

						CUringSendRing Ring;
						Ring.m_pRingEntries = Entries;
						Ring.m_nRingEntries = 8;
						Ring.m_nEntriesOutstanding = 3;
						Ring.m_Tail = uint16(_Head + 3);

						auto &Record = Ring.m_Records.f_Insert();
						Record.m_nBytes = 300;
						Record.m_nEntries = 2;
						Record.m_nCovered = _nCovered;
						auto &Later = Ring.m_Records.f_Insert();
						Later.m_nBytes = 400;
						Later.m_nEntries = 1;

						DMibExpect(Ring.f_HasLostEntries(_KernelHead), ==, _bLost);
					}
				;

				fCheck("UnselectedRecordsCanRearm", 0, 0, 0, false);
				fCheck("SuccessfulStopWithinRecord", 0, 1, 100, false);
				fCheck("FailureWithinEntry", 0, 1, 99, true);
				fCheck("FailureBetweenEntries", 0, 2, 100, true);
				fCheck("FailureBeforeAnyBytes", 0, 1, 0, true);
				fCheck("EarlierRecordsRemoved", 2, 2, 0, false);
				fCheck("LaterRecordConsumedWithoutBytes", 2, 3, 0, true);
				fCheck("WrappedHeadAndTailSuccess", 65535, 0, 100, false);
				fCheck("WrappedHeadAndTailFailure", 65535, 1, 100, true);

				DMibTestCategory("CompletedRing")
				{
					CUringSendRing Ring;

					DMibExpectFalse(Ring.f_HasLostEntries(1));
				};
			};
		}
	};
}

DMibTestRegister(CIoUring_Tests, Malterlib::Core);

#endif
