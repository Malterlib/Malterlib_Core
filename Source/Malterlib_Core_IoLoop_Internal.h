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
		CIoLoopHandle m_Handle = gc_IoLoopHandleInvalid;
		CIoLoopRegisterOptions m_Options;

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

	// The registration's handle; for a POSIX loop's internal entry the wake pipe's read end
	NMib::NSys::CIoLoopHandle m_Handle = NMib::NSys::gc_IoLoopHandleInvalid;

	bool m_bRemove = false;
	// The loop's own wake object rather than an io object registration
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

// Shared base of every io loop backend: the registration change queue, the wake protocol, owner
// thread tracking, and the registration and deregistration entry points. The kernel object the
// wake reaches is the backend's — a pipe write on POSIX, a completion packet on Windows — so the
// base only decides whether a wake is owed. Pass structure stays per backend — epoll, kqueue,
// io_uring and IOCP order change processing, kernel waits, dispatch and acknowledgements
// differently, and those orders are load-bearing
struct CIoLoop_Base : public NMib::NSys::ICIoLoop
{
	void f_SetOwnerThreadToCurrent() override;

	auto f_Register
		(
			NMib::NSys::CIoLoopHandle _Handle
			, void *_pToken
			, NMib::NSys::EIoLoopEvent _EventMask
			, NMib::NSys::FIoLoopReadinessCallback _fOnEvents
			, bool _bNotifyRegistered
			, NMib::NSys::CIoLoopRegisterOptions const &_Options
		)
		-> NMib::NSys::CIoLoopRegistration * override
	;
	void f_Deregister(NMib::NSys::CIoLoopRegistration *_pRegistration) override;
	void f_DeregisterAsync(NMib::NSys::CIoLoopRegistration *_pRegistration, NMib::NFunction::TCFunctionMovable<void ()> &&_fOnDeregistered) override;

	void f_WaitAndDispatch() override;
	bool f_PollAndDispatch() override;
	void f_Wake() override;

protected:
	enum class EWakeState : uint32
	{
		mc_Pending = 1
		, mc_Parked = 2
	};

	CIoLoop_Base();

	// Every io object is closed and its removal acknowledged before the loop goes: the thread
	// that drives the loop drains it to quiescence on exit, and an io object still registered
	// or a change still queued at destruction is an error in the owner's teardown order
	~CIoLoop_Base() override;

	bool fp_IsOwnerThread() const;

	// Runs a removal's acknowledgement and takes the registration out of the live count
	void fp_RunDeregAcknowledgement(CIoLoopDeferredAck &_Ack);

	// One pass: applies pending registration changes, waits for events up to the timeout and
	// reports them. Returns the number of events reported, not counting wakeups
	virtual umint fp_Iterate(bool _bBlock) = 0;

	// The registration record f_Register hands out; a backend with per-registration loop state
	// allocates its extended record here
	virtual auto fp_CreateRegistration() -> NMib::NSys::CIoLoopRegistration *;

	// Reaches the owner parked in its kernel wait. Called by fp_SignalWake only when the wake
	// state says the owner is parked and no earlier waker already owes it a wake, so a signal
	// storm costs one kernel call. Must be callable from any thread
	virtual void fp_WakeKernel() = 0;

	void fp_SignalWake();

	// Builds a removal change for the registration and pushes it
	void fp_PushRemoval(NMib::NSys::CIoLoopRegistration *_pRegistration, CIoLoopDeregWait *_pDeregWait, NMib::NFunction::TCFunctionMovable<void ()> &&_fOnDeregistered);

	// Pending is set by a waker that owes the owner a wake, Parked while the owner is blocked in
	// the kernel wait. Both sides use read-modify-writes on this one word, so they order totally
	// against each other
	NMib::NAtomic::TCAtomic<uint32> mp_WakeState;
	CIoLoopChangeQueue mp_ChangeQueue;

	// Registrations made and not yet acknowledged as removed, for the destruction check
	NMib::NAtomic::TCAtomic<umint> mp_nRegistrations{0};

	// The thread that drives this loop, so that a caller running on it can apply its own changes
	// instead of waiting for an acknowledgement it would have had to produce itself
	NMib::NAtomic::TCAtomic<umint> mp_OwnerThreadUID{0};

#if DMibEnableSafeCheck > 0
	// Nonzero while the owner is inside a dispatch, read by the owner alone: a synchronous
	// deregistration from there would retire a registration the batch being delivered still names
	umint mp_nDispatchDepth = 0;
#endif
};

// The platform's backend pick, allocated on the non-tracked heap: a loop is reachable from every
// thread that wakes a worker parked in it and from every io object registered with it, and its
// owner destroys it only after those threads have been joined, which can be later than the
// tracking teardown order allows. NSys::fg_CreateIoLoop wraps this and marks the loop as created;
// the shared poller thread hosts one directly, unmarked. Returns nullptr where the platform has
// no loop to offer
NMib::NSys::ICIoLoop *fg_CreatePlatformIoLoop();
