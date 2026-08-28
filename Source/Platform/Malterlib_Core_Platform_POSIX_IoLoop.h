// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>

namespace NMib::NSys
{
	// A descriptor's registration with a loop, owned by the loop and freed once its removal has
	// been acknowledged. The kernel-facing identity of the registration: epoll and kqueue carry
	// its address in their event payloads, and the io_uring backend packs it into completion user
	// data together with a tag in the low four bits, which the alignment keeps zero. Backends
	// allocate the concrete record, so the destructor is virtual for the shared acknowledgement
	// to free through. The named fields are immutable from when f_Register publishes the handle,
	// so any thread may read them; the request word is the one cross-thread mutable member
	struct alignas(16) CIoLoopRegistration
	{
		virtual ~CIoLoopRegistration() = default;

		void *m_pToken = nullptr;
		FIoLoopReadinessCallback m_fOnEvents = nullptr;
		NMib::NSys::EIoLoopEvent m_EventMask = NMib::NSys::EIoLoopEvent::mc_None;
		int m_Fd = -1;

		// Readiness request coalescing: EIoLoopEvent bits requested and not yet consumed by the
		// loop. The requester that turns the word nonzero owns pushing the queue notification;
		// later requests only accumulate bits, so a drain cycle's many would-block observations
		// cost one push
		NMib::NAtomic::TCAtomic<uint32> m_RequestedEvents{0};
	};
}

// The completion token of a synchronous deregistration, on the deregistering caller's stack. The
// acknowledgement stores the flag before signalling; the flag rather than the event is what the
// owner-thread self-drive spins on, because that path never parks
struct CIoLoopDeregWait
{
	NMib::NThread::CEvent m_Event;
	NMib::NAtomic::TCAtomic<bool> m_bDone{false};
};

// One queued registration change, pushed by any thread and consumed on the loop's thread. A
// removal carries its acknowledgement obligations — the wait block or continuation — so nothing
// about a removal lives on the io object itself. The descriptor a removal names stays open and
// owned by the caller until the acknowledgement runs, so it is targetable by number throughout
struct CIoLoopChange
{
	NMib::NSys::CIoLoopRegistration *m_pRegistration = nullptr;
	CIoLoopDeregWait *m_pDeregWait = nullptr;
	NMib::NFunction::TCFunctionMovable<void ()> m_fOnDeregistered;

	// The registration's descriptor number; for the internal entry the wake pipe's read end
	int m_Fd = -1;

	bool m_bRemove = false;
	// The loop's own wake pipe rather than an io object registration
	bool m_bInternal = false;
	// Invoke the registration's callback with 0 events once the add has been applied
	bool m_bNotifyRegistered = false;
	// The registration's request word turned nonzero; the loop consumes it and arms. Only pushed
	// by backends whose readiness is request-armed
	bool m_bReadinessRequest = false;
};

struct CIoLoopChangeQueue
{
	void f_Push(CIoLoopChange &&_Change);
	NMib::NContainer::TCVector<CIoLoopChange> f_Take();

private:
	NMib::NThread::CMutual mp_Lock;
	NMib::NContainer::TCVector<CIoLoopChange> mp_lQueue;
};

// A removal acknowledgement lifted out of its change entry or a backend's registration state,
// run only once nothing can name the registration anymore
struct CIoLoopDeferredAck
{
	NMib::NSys::CIoLoopRegistration *m_pRegistration;
	CIoLoopDeregWait *m_pDeregWait;
	NMib::NFunction::TCFunctionMovable<void ()> m_fOnDeregistered;
};

// Runs a removal's acknowledgement obligations. The io object must not be touched after the
// signal or the continuation, either of which can free it; the registration is the loop's own
// and is released last
void fg_RunDeregAcknowledgement(CIoLoopDeferredAck &_Ack);

// Shared base of the POSIX io loop backends: the registration change queue, the wake protocol
// and pipe, owner thread tracking, and the registration and deregistration entry points. Pass
// structure stays per backend — epoll, kqueue and io_uring order change processing, kernel
// waits, dispatch and acknowledgements differently, and those orders are load-bearing
struct CIoLoop_POSIXBase : public NMib::NSys::ICIoLoop
{
	void f_SetOwnerThreadToCurrent() override;

	auto f_Register(int _Fd, void *_pToken, NMib::NSys::EIoLoopEvent _EventMask, NMib::NSys::FIoLoopReadinessCallback _fOnEvents, bool _bNotifyRegistered) -> NMib::NSys::CIoLoopRegistration * override;
	void f_Deregister(NMib::NSys::CIoLoopRegistration *_pRegistration) override;
	void f_DeregisterAsync(NMib::NSys::CIoLoopRegistration *_pRegistration, NMib::NFunction::TCFunctionMovable<void ()> &&_fOnDeregistered) override;

	void f_WaitAndDispatch() override;
	bool f_PollAndDispatch() override;
	void f_Wake() override;
	void f_AbandonPendingTeardown() override;

protected:
	enum class EWakeState : uint32
	{
		mc_Pending = 1
		, mc_Parked = 2
	};

	bool fp_IsOwnerThread() const;

	// One pass: applies pending registration changes, waits for events up to the timeout and
	// reports them. Returns the number of events reported, not counting wakeups
	virtual umint fp_Iterate(bool _bBlock) = 0;

	// The registration record f_Register hands out; a backend with per-registration loop state
	// allocates its extended record here
	virtual auto fp_CreateRegistration() -> NMib::NSys::CIoLoopRegistration *;

	void fp_SignalWake();

	// Builds a removal change for the registration and pushes it
	void fp_PushRemoval(NMib::NSys::CIoLoopRegistration *_pRegistration, CIoLoopDeregWait *_pDeregWait, NMib::NFunction::TCFunctionMovable<void ()> &&_fOnDeregistered);

	// Creates the wake pipe and queues its registration; a backend constructor that fails to
	// create its kernel object simply throws — this base is then destroyed and closes the pipe
	CIoLoop_POSIXBase();
	~CIoLoop_POSIXBase() override;

	// Pending is set by a waker that owes the owner a wake, Parked while the owner is blocked in
	// the kernel wait. Both sides use read-modify-writes on this one word, so they order totally
	// against each other
	NMib::NAtomic::TCAtomic<uint32> mp_WakeState;
	int mp_ReadWritePipe[2];		// Used to wake the parked loop up
	CIoLoopChangeQueue mp_ChangeQueue;

	// The thread that drives this loop, so that a caller running on it can apply its own changes
	// instead of waiting for an acknowledgement it would have had to produce itself
	NMib::NAtomic::TCAtomic<umint> mp_OwnerThreadUID{0};
};

// The platform's backend pick, allocated on the non-tracked heap: a loop is reachable from every
// thread that wakes a worker parked in it and from every io object registered with it, and its
// owner destroys it only after those threads have been joined, which can be later than the
// tracking teardown order allows. NSys::fg_CreateIoLoop wraps this and marks the loop as created;
// the shared poller thread hosts one directly, unmarked
NMib::NSys::ICIoLoop *fg_CreatePlatformIoLoop();
