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
	m_pLoop->f_SetOwnerThreadToCurrent();

	while (f_GetState() != NThread::EThreadState_EventWantQuit)
		m_pLoop->f_WaitAndDispatch();

	// Acknowledge outstanding deregistrations before the loop loses its thread.
	m_pLoop->f_DrainForShutdown();

	return 0;
}

umint CSharedIoLoop::CPollerThread::f_Stop(bool _bBlock)
{
	// Publish quit before waking the parked thread.
	NThread::CThread::f_Stop(false);
	m_pLoop->f_Wake();

	return NThread::CThread::f_Stop(_bBlock);
}

CSharedIoLoop::CSharedIoLoop()
{
	// Create the loop before starting its thread; unsupported platforms leave it null.
	mp_Thread.m_pLoop = fg_CreatePlatformIoLoop();
	if (mp_Thread.m_pLoop)
		mp_Thread.f_Start(EExecutionPriority_Highest);
}

CSharedIoLoop::~CSharedIoLoop()
{
	if (!mp_Thread.m_pLoop)
		return;

	// The exit drain must acknowledge removals before the loop is destroyed.
	mp_Thread.f_Stop(true);
	NSys::fg_DestroyIoLoop(mp_Thread.m_pLoop);
	mp_Thread.m_pLoop = nullptr;
}
