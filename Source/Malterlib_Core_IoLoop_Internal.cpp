// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_IoLoop_Internal.h"

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

CIoLoop_Base::CIoLoop_Base()
{
	mp_WakeState.f_Store(0);
}

CIoLoop_Base::~CIoLoop_Base()
{
	// A loop that was never driven still holds the internal entry its constructor queued for
	// its own wake object; anything else queued is an io object's change nobody will apply
	[[maybe_unused]] umint nExternalChanges = 0;
	for (auto &Change : mp_ChangeQueue.f_Take())
		nExternalChanges += !Change.m_bInternal;

	DMibFastCheck(nExternalChanges == 0);
	DMibFastCheck(mp_nRegistrations.f_Load(NAtomic::gc_MemoryOrder_Relaxed) == 0);
}

void CIoLoop_Base::fp_RunDeregAcknowledgement(CIoLoopDeferredAck &_Ack)
{
	mp_nRegistrations.f_FetchSub(1, NAtomic::gc_MemoryOrder_Relaxed);
	fg_RunDeregAcknowledgement(_Ack);
}

void CIoLoop_Base::f_SetOwnerThreadToCurrent()
{
	mp_OwnerThreadUID.f_Store((umint)NSys::fg_Thread_GetCurrentUID(), NAtomic::gc_MemoryOrder_Release);
}

bool CIoLoop_Base::fp_IsOwnerThread() const
{
	// Relaxed is enough: a true answer means this very thread stored the UID, so it reads its
	// own store, and a stale false only sends the caller down the conservative cross-thread
	// path. No other thread's writes are accessed on the strength of this comparison
	umint OwnerUID = mp_OwnerThreadUID.f_Load(NAtomic::gc_MemoryOrder_Relaxed);
	return OwnerUID && OwnerUID == (umint)NSys::fg_Thread_GetCurrentUID();
}

auto CIoLoop_Base::fp_CreateRegistration() -> NSys::CIoLoopRegistration *
{
	return fg_ConstructObject<NSys::CIoLoopRegistration>(CDefaultAllocator());
}

void CIoLoop_Base::fp_SignalWake()
{
	// Either the owner observes the pending bit before it commits to blocking and polls instead,
	// or this observes the parked bit and pays the kernel wake. A pending bit that is already set
	// means an earlier waker has secured one of the two, so the wake is elided; that is what
	// keeps a signal storm at one kernel call
	EWakeState Previous = EWakeState(mp_WakeState.f_FetchOr(uint32(EWakeState::mc_Pending), NAtomic::gc_MemoryOrder_SequentiallyConsistent));
	if (fg_IsSet(Previous, EWakeState::mc_Pending))
		return;

	if (fg_IsSet(Previous, EWakeState::mc_Parked))
		fp_WakeKernel();
}

auto CIoLoop_Base::f_Register
	(
		NSys::CIoLoopHandle _Handle
		, void *_pToken
		, NSys::EIoLoopEvent _EventMask
		, NSys::FIoLoopReadinessCallback _fOnEvents
		, bool _bNotifyRegistered
		, NSys::CIoLoopRegisterOptions const &_Options
	)
	-> NSys::CIoLoopRegistration *
{
	auto *pRegistration = fp_CreateRegistration();
	mp_nRegistrations.f_FetchAdd(1, NAtomic::gc_MemoryOrder_Relaxed);

	pRegistration->m_Handle = _Handle;
	pRegistration->m_Options = _Options;
	pRegistration->m_pToken = _pToken;
	pRegistration->m_EventMask = _EventMask;
	pRegistration->m_fOnEvents = _fOnEvents;

	// The implicit initial request: the full interest mask, consumed when the add is processed.
	// Backends with standing interest never read the word
	pRegistration->m_RequestedEvents.f_Store(uint32(_EventMask), NAtomic::gc_MemoryOrder_Relaxed);

	CIoLoopChange Change;
	Change.m_Handle = _Handle;
	Change.m_bNotifyRegistered = _bNotifyRegistered;
	Change.m_pRegistration = pRegistration;

	mp_ChangeQueue.f_Push(fg_Move(Change));
	fp_SignalWake();

	return pRegistration;
}

void CIoLoop_Base::fp_PushRemoval(NSys::CIoLoopRegistration *_pRegistration, CIoLoopDeregWait *_pDeregWait, NMib::NFunction::TCFunctionMovable<void ()> &&_fOnDeregistered)
{
	CIoLoopChange Change;
	Change.m_bRemove = true;
	Change.m_Handle = _pRegistration->m_Handle;
	Change.m_pRegistration = _pRegistration;
	Change.m_pDeregWait = _pDeregWait;
	Change.m_fOnDeregistered = fg_Move(_fOnDeregistered);

	mp_ChangeQueue.f_Push(fg_Move(Change));
	fp_SignalWake();
}

void CIoLoop_Base::f_Deregister(NSys::CIoLoopRegistration *_pRegistration)
{
	CIoLoopDeregWait DeregWait;
	DeregWait.m_Event.f_ResetSignaled();

	fp_PushRemoval(_pRegistration, &DeregWait, {});

	if (fp_IsOwnerThread())
	{
		// Not from inside a dispatch: the pass this runs applies removals, and the batch being
		// delivered out there may still name a registration that pass would free. A callback
		// deregisters asynchronously instead
#if DMibEnableSafeCheck > 0
		DMibFastCheck(mp_nDispatchDepth == 0);
#endif

		// This thread is the one that drives the loop, so waiting here would be waiting for
		// itself. Apply the removal directly instead, which is what the loop would have done.
		// More than one pass can be needed when a cancel races an in-flight completion
		while (!DeregWait.m_bDone.f_Load(NAtomic::gc_MemoryOrder_Acquire))
		{
#if DMibEnableSafeCheck > 0
			++mp_nDispatchDepth;
#endif
			fp_Iterate(false);
#if DMibEnableSafeCheck > 0
			--mp_nDispatchDepth;
#endif
		}
	}
	else
		DeregWait.m_Event.f_Wait();
}

void CIoLoop_Base::f_DeregisterAsync(NSys::CIoLoopRegistration *_pRegistration, NMib::NFunction::TCFunctionMovable<void ()> &&_fOnDeregistered)
{
	fp_PushRemoval(_pRegistration, nullptr, fg_Move(_fOnDeregistered));
}

void CIoLoop_Base::f_WaitAndDispatch()
{
#if DMibEnableSafeCheck > 0
	++mp_nDispatchDepth;
#endif
	fp_Iterate(true);
#if DMibEnableSafeCheck > 0
	--mp_nDispatchDepth;
#endif
}

bool CIoLoop_Base::f_PollAndDispatch()
{
#if DMibEnableSafeCheck > 0
	++mp_nDispatchDepth;
#endif
	umint nReported = fp_Iterate(false);
#if DMibEnableSafeCheck > 0
	--mp_nDispatchDepth;
#endif

	return nReported != 0;
}

void CIoLoop_Base::f_Wake()
{
	fp_SignalWake();
}

namespace NMib::NSys
{
	ICIoLoop *fg_CreateIoLoop()
	{
		ICIoLoop *pLoop = fg_CreatePlatformIoLoop();
		if (pLoop)
			pLoop->m_bCreatedAsLoop = true;

		return pLoop;
	}

	void fg_DestroyIoLoop(ICIoLoop *_pLoop)
	{
		fg_DeleteObject(CAllocator_NonTrackedHeap(), _pLoop);
	}
}
