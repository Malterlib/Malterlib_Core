// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_POSIX_IoLoop.h"
#include "Malterlib_Core_PlatformImp_POSIX.h"
#include "Malterlib_Core_Platform_POSIX_ErrNo.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#if defined(DPlatformFamily_Linux)
	#include "Malterlib_Core_Platform_Linux_Optional.h"
#endif

#if defined(DPlatformFamily_macOS)
CSystem_POSIX *fg_GetSys_POSIX();
#endif

using namespace NMib;
using namespace NMib::NMemory;

void fg_RunDeregAcknowledgement(CIoLoopDeferredAck &_Ack)
{
	auto fOnDeregistered = fg_Move(_Ack.m_fOnDeregistered);

	if (_Ack.m_pDeregWait)
	{
		_Ack.m_pDeregWait->m_bDone.f_Store(true, NAtomic::gc_MemoryOrder_Release);
		_Ack.m_pDeregWait->m_Event.f_SetSignaled();
	}

	if (fOnDeregistered)
		fOnDeregistered();

	fg_DeleteObject(CDefaultAllocator(), _Ack.m_pRegistration);
}

void CIoLoopChangeQueue::f_Push(CIoLoopChange &&_Change)
{
	DMibLock(mp_Lock);
	mp_lQueue.f_Insert(NMib::fg_Move(_Change));
}

NMib::NContainer::TCVector<CIoLoopChange> CIoLoopChangeQueue::f_Take()
{
	DMibLock(mp_Lock);
	return NMib::fg_Move(mp_lQueue);
}


static void fsg_MakePipeNonBlockingCloExec(int (&_Pipe)[2])
{
	for (int Fd : _Pipe)
	{
		fcntl(Fd, F_SETFL, fcntl(Fd, F_GETFL) | O_NONBLOCK);
		fcntl(Fd, F_SETFD, fcntl(Fd, F_GETFD) | FD_CLOEXEC);
	}
}

CIoLoop_POSIXBase::CIoLoop_POSIXBase()
{
	mp_WakeState.f_Store(0);

	int PipeRet;
#if defined(DPlatformFamily_Linux)
	if (NLocal::g_f_pipe2)
		PipeRet = NLocal::g_f_pipe2(mp_ReadWritePipe, O_NONBLOCK | O_CLOEXEC);
	else
	{
		PipeRet = pipe(mp_ReadWritePipe);
		if (!PipeRet)
			fsg_MakePipeNonBlockingCloExec(mp_ReadWritePipe);
	}
#else
	{
		// The pipes must not leak into other processes, and without an atomic pipe2 the close on
		// exec flags go on under the fork lock
		DMibLock(fg_GetSys_POSIX()->m_ForkLock);
		PipeRet = pipe(mp_ReadWritePipe);
		if (!PipeRet)
			fsg_MakePipeNonBlockingCloExec(mp_ReadWritePipe);
	}
#endif
	if (PipeRet)
		DMibErrorNet(NMib::NPlatform::fg_FormatErrno("pipe (io loop)", errno));

	CIoLoopChange CurChange;
	CurChange.m_bInternal = true;
	CurChange.m_Fd = mp_ReadWritePipe[0];
	mp_ChangeQueue.f_Push(fg_Move(CurChange));
}

CIoLoop_POSIXBase::~CIoLoop_POSIXBase()
{
	close(mp_ReadWritePipe[0]);
	close(mp_ReadWritePipe[1]);
}

void CIoLoop_POSIXBase::f_SetOwnerThreadToCurrent()
{
	mp_OwnerThreadUID.f_Store((umint)NSys::fg_Thread_GetCurrentUID(), NAtomic::gc_MemoryOrder_Release);
}

bool CIoLoop_POSIXBase::fp_IsOwnerThread() const
{
	// Relaxed is enough: a true answer means this very thread stored the UID, so it reads its
	// own store, and a stale false only sends the caller down the conservative cross-thread
	// path. No other thread's writes are accessed on the strength of this comparison
	umint OwnerUID = mp_OwnerThreadUID.f_Load(NAtomic::gc_MemoryOrder_Relaxed);
	return OwnerUID && OwnerUID == (umint)NSys::fg_Thread_GetCurrentUID();
}

auto CIoLoop_POSIXBase::fp_CreateRegistration() -> NSys::CIoLoopRegistration *
{
	return fg_ConstructObject<NSys::CIoLoopRegistration>(CDefaultAllocator());
}

void CIoLoop_POSIXBase::fp_SignalWake()
{
	// Either the owner observes the pending bit before it commits to blocking and polls instead,
	// or this observes the parked bit and pays the pipe write. A pending bit that is already set
	// means an earlier waker has secured one of the two, so the write is elided; that is what
	// keeps a signal storm at one syscall
	EWakeState Previous = EWakeState(mp_WakeState.f_FetchOr(uint32(EWakeState::mc_Pending), NAtomic::gc_MemoryOrder_SequentiallyConsistent));
	if (fg_IsSet(Previous, EWakeState::mc_Pending))
		return;

	if (fg_IsSet(Previous, EWakeState::mc_Parked))
	{
		// The pending bit is already latched, so no later waker will retry this write — an
		// interrupted attempt has to be retried right here or the wake is lost until unrelated
		// I/O arrives. A full pipe needs no retry: its unread bytes already owe the owner a wake
		char Byte = 1;
		while (write(mp_ReadWritePipe[1], &Byte, 1) == -1 && errno == EINTR)
			;
	}
}

auto CIoLoop_POSIXBase::f_Register(int _Fd, void *_pToken, NSys::EIoLoopEvent _EventMask, NSys::FIoLoopReadinessCallback _fOnEvents, bool _bNotifyRegistered) -> NSys::CIoLoopRegistration *
{
	auto *pRegistration = fp_CreateRegistration();

	pRegistration->m_Fd = _Fd;
	pRegistration->m_pToken = _pToken;
	pRegistration->m_EventMask = _EventMask;
	pRegistration->m_fOnEvents = _fOnEvents;

	// The implicit initial request: the full interest mask, consumed when the add is processed.
	// Backends with standing interest never read the word
	pRegistration->m_RequestedEvents.f_Store(uint32(_EventMask), NAtomic::gc_MemoryOrder_Relaxed);

	CIoLoopChange Change;
	Change.m_Fd = _Fd;
	Change.m_bNotifyRegistered = _bNotifyRegistered;
	Change.m_pRegistration = pRegistration;

	mp_ChangeQueue.f_Push(fg_Move(Change));
	fp_SignalWake();

	return pRegistration;
}

void CIoLoop_POSIXBase::fp_PushRemoval(NSys::CIoLoopRegistration *_pRegistration, CIoLoopDeregWait *_pDeregWait, NMib::NFunction::TCFunctionMovable<void ()> &&_fOnDeregistered)
{
	CIoLoopChange Change;
	Change.m_bRemove = true;
	Change.m_Fd = _pRegistration->m_Fd;
	Change.m_pRegistration = _pRegistration;
	Change.m_pDeregWait = _pDeregWait;
	Change.m_fOnDeregistered = fg_Move(_fOnDeregistered);

	mp_ChangeQueue.f_Push(fg_Move(Change));
	fp_SignalWake();
}

void CIoLoop_POSIXBase::f_Deregister(NSys::CIoLoopRegistration *_pRegistration)
{
	CIoLoopDeregWait DeregWait;
	DeregWait.m_Event.f_ResetSignaled();

	fp_PushRemoval(_pRegistration, &DeregWait, {});

	if (fp_IsOwnerThread())
	{
		// This thread is the one that drives the loop, so waiting here would be waiting for
		// itself. Apply the removal directly instead, which is what the loop would have done.
		// More than one pass can be needed when a cancel races an in-flight completion
		while (!DeregWait.m_bDone.f_Load(NAtomic::gc_MemoryOrder_Acquire))
			fp_Iterate(false);
	}
	else
		DeregWait.m_Event.f_Wait();
}

void CIoLoop_POSIXBase::f_DeregisterAsync(NSys::CIoLoopRegistration *_pRegistration, NMib::NFunction::TCFunctionMovable<void ()> &&_fOnDeregistered)
{
	fp_PushRemoval(_pRegistration, nullptr, fg_Move(_fOnDeregistered));
}

void CIoLoop_POSIXBase::f_WaitAndDispatch()
{
	fp_Iterate(true);
}

bool CIoLoop_POSIXBase::f_PollAndDispatch()
{
	return fp_Iterate(false) != 0;
}

void CIoLoop_POSIXBase::f_Wake()
{
	fp_SignalWake();
}

void CIoLoop_POSIXBase::f_AbandonPendingTeardown()
{
	// Every pool thread is joined, so nothing races this walk, and the kernel object is about to
	// be destroyed wholesale — only the userspace obligations matter: the continuations that
	// free the io objects. Backends with more state (io_uring's registration map and pending
	// operations) override and extend this
	auto Changes = fg_Move(mp_ChangeQueue.f_Take());
	for (auto &Change : Changes)
	{
		if (!Change.m_bRemove)
			continue;

		CIoLoopDeferredAck Ack{Change.m_pRegistration, Change.m_pDeregWait, fg_Move(Change.m_fOnDeregistered)};
		fg_RunDeregAcknowledgement(Ack);
	}
}

namespace NMib::NSys
{
	ICIoLoop *fg_CreateIoLoop()
	{
		ICIoLoop *pLoop = fg_CreatePlatformIoLoop();
		pLoop->m_bCreatedAsLoop = true;
		return pLoop;
	}

	void fg_DestroyIoLoop(ICIoLoop *_pLoop)
	{
		fg_DeleteObject(CAllocator_NonTrackedHeap(), _pLoop);
	}
}
