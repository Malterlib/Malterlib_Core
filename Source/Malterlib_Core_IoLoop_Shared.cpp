// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

#include "Malterlib_Core_IoLoop_Internal.h"

using namespace NMib;

NStr::CStr CSharedIoLoop::CPollerThread::f_GetThreadName()
{
	return NStr::CStr("Io Poller");
}

aint CSharedIoLoop::CPollerThread::f_Main()
{
	mp_pLoop->f_SetOwnerThreadToCurrent();

	while (f_GetState() != NThread::EThreadState_EventWantQuit)
		mp_pLoop->f_WaitAndDispatch();

	// Deregistrations still in progress complete before the loop loses its thread, as the loop's
	// contract asks of its owner
	mp_pLoop->f_DrainForShutdown();

	return 0;
}

umint CSharedIoLoop::CPollerThread::f_Stop(bool _bBlock)
{
	// Asks for the quit without blocking first, so the wake below finds the state already set and
	// the thread leaves its park instead of dispatching once more
	NThread::CThread::f_Stop(false);
	mp_pLoop->f_Wake();

	return NThread::CThread::f_Stop(_bBlock);
}

CSharedIoLoop::CSharedIoLoop()
{
	// The loop exists before the thread that hosts it, so the thread body never checks. A platform
	// that cannot offer one leaves both null, and every consumer is expected to handle that: on
	// Windows it is what makes the socket context report its init failure
	mp_Thread.mp_pLoop = fg_CreatePlatformIoLoop();
	if (mp_Thread.mp_pLoop)
		mp_Thread.f_Start(EExecutionPriority_Highest);
}

CSharedIoLoop::~CSharedIoLoop()
{
	if (!mp_Thread.mp_pLoop)
		return;

	// The exit drain acknowledges the last removals; anything still registered past it is an error
	// in its owner's teardown order, which the loop's destruction checks
	mp_Thread.f_Stop(true);
	NSys::fg_DestroyIoLoop(mp_Thread.mp_pLoop);
	mp_Thread.mp_pLoop = nullptr;
}
