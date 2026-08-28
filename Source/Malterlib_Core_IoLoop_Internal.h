// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>

namespace NMib::NSys
{
	// Loop-owned until removal acknowledgement. Four aligned low bits are available for completion tags.
	// Published identity fields are immutable; the readiness request word is cross-thread mutable.
	struct alignas(16) CIoLoopRegistration
	{
		virtual ~CIoLoopRegistration() = default;

		void *m_pToken = nullptr;
		FIoLoopReadinessCallback m_fOnEvents = nullptr;
		NMib::NSys::EIoLoopEvent m_EventMask = NMib::NSys::EIoLoopEvent::mc_None;
		CIoLoopHandle m_Handle = gc_IoLoopHandleInvalid;
		CIoLoopRegisterOptions m_Options;

		NMib::NAtomic::TCAtomic<uint32> m_RequestedEvents{0}; // Unconsumed readiness bits; only the zero-to-nonzero requester queues a notification.
	};
}

// Lives on the deregistering caller's stack; publish completion before signalling. Owner-thread self-drive checks the flag without parking.
struct CIoLoopDeregWait
{
	NMib::NThread::CEvent m_Event;
	NMib::NAtomic::TCAtomic<bool> m_bDone{false};
};

// Queued by any thread, consumed by the loop. Removal keeps the descriptor open until its acknowledgement.
struct CIoLoopChange
{
	NMib::NSys::CIoLoopRegistration *m_pRegistration = nullptr;
	CIoLoopDeregWait *m_pDeregWait = nullptr;
	NMib::NFunction::TCFunctionMovable<void ()> m_fOnDeregistered;

	NMib::NSys::CIoLoopHandle m_Handle = NMib::NSys::gc_IoLoopHandleInvalid; // For an internal POSIX entry, the wake pipe's read end.

	bool m_bRemove = false;
	bool m_bInternal = false; // Identifies the loop's wake object rather than an external registration.
	bool m_bNotifyRegistered = false; // Acknowledge an applied add with a callback carrying no events.
	bool m_bReadinessRequest = false; // Consume and arm the registration's pending readiness request bits.
};

struct CIoLoopChangeQueue
{
	void f_Push(CIoLoopChange &&_Change);
	NMib::NContainer::TCVector<CIoLoopChange> f_Take();

private:
	NMib::NThread::CMutual mp_Lock;
	NMib::NContainer::TCVector<CIoLoopChange> mp_lQueue;
};

// A removal acknowledgement may run only when nothing can still name the registration.
struct CIoLoopDeferredAck
{
	NMib::NSys::CIoLoopRegistration *m_pRegistration;
	CIoLoopDeregWait *m_pDeregWait;
	NMib::NFunction::TCFunctionMovable<void ()> m_fOnDeregistered;
};

void fg_RunDeregAcknowledgement(CIoLoopDeferredAck &_Ack);

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
	~CIoLoop_Base() override;

	bool fp_IsOwnerThread() const;
	void fp_RunDeregAcknowledgement(CIoLoopDeferredAck &_Ack);

	virtual umint fp_Iterate(bool _bBlock) = 0;
	virtual auto fp_CreateRegistration() -> NMib::NSys::CIoLoopRegistration *;
	virtual void fp_WakeKernel() = 0;

	void fp_SignalWake();
	void fp_PushRemoval(NMib::NSys::CIoLoopRegistration *_pRegistration, CIoLoopDeregWait *_pDeregWait, NMib::NFunction::TCFunctionMovable<void ()> &&_fOnDeregistered);

	NMib::NAtomic::TCAtomic<uint32> mp_WakeState; // Pending and Parked share one atomic word so owner/waker read-modify-writes are totally ordered.
	CIoLoopChangeQueue mp_ChangeQueue;
	NMib::NAtomic::TCAtomic<umint> mp_nRegistrations{0}; // Includes removals not yet acknowledged.
	NMib::NAtomic::TCAtomic<umint> mp_OwnerThreadUID{0}; // Allows the owner to drive its own deregistration acknowledgement.

#if DMibEnableSafeCheck > 0
	umint mp_nDispatchDepth = 0; // Owner-only; nonzero forbids synchronous removal of registrations still named by the dispatch batch.
#endif
};

NMib::NSys::ICIoLoop *fg_CreatePlatformIoLoop();
