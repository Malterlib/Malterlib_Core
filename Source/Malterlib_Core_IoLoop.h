// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib
{
	struct CVirtualDestroyBase;
}

namespace NMib::NStorage
{
	template <typename t_CType, typename... tp_COptions>
	class TCSharedPointer;
}

namespace NMib::NThread
{
	class CEventAutoReset;
}

namespace NMib::NFunction
{
	template <typename t_CSignature>
	struct TCFunctionMovable;
}

namespace NMib::NSys
{
	// The kernel handle a registration names: a descriptor number on POSIX, a SOCKET or HANDLE
	// value on Windows. Wide enough for either; the all-ones value is invalid on both
#if defined(DPlatformFamily_Windows)
	using CIoLoopHandle = umint;
	constexpr CIoLoopHandle gc_IoLoopHandleInvalid = ~CIoLoopHandle(0);
#else
	using CIoLoopHandle = int;
	constexpr CIoLoopHandle gc_IoLoopHandleInvalid = -1;
#endif

	// An event loop that a worker thread parks in instead of its own wake event, so that what the
	// loop reports and the work that consumes it run on the same thread with no handoff between
	// them. Everything except f_Wake runs on the thread the loop was attached to.
	//
	// A loop stays reachable from any thread that wakes the worker for as long as it is attached,
	// so it may only be destroyed after it has been detached; detaching waits out the wakers that
	// are already holding it
	struct ICThreadIoLoop
	{
		virtual ~ICThreadIoLoop();

		// Blocks until the loop has something to report or a wake lands, then reports it
		virtual void f_WaitAndDispatch() = 0;

		// Reports whatever is already complete without blocking and returns whether anything was
		// reported. The owning thread calls this once per drain of its work queue, so a long run
		// of work cannot starve what the loop carries. Implementations must make the nothing to
		// report case cheap, since it is on that drain path
		virtual bool f_PollAndDispatch() = 0;

		// Returns from a concurrent f_WaitAndDispatch, or makes the next one return without
		// blocking. Callable from any thread
		virtual void f_Wake() = 0;

		// Called on the owning thread as it exits: runs the loop until every in-progress
		// deregistration has completed, so deferred io object destructions are not stranded when
		// nothing drives the loop anymore. A single poll is enough for backends whose removals
		// apply inline; multi-stage teardown (io_uring cancel handshakes) must iterate to
		// quiescence
		virtual void f_DrainForShutdown();

		// Called with every pool thread joined for a loop whose owner already exited: teardown
		// work queued after the owner's exit drain — deregistrations released by the stop
		// sequence's final queue passes — can never be iterated for, and a claimed ring cannot
		// be entered from another thread. Runs the stranded teardown continuations and cancels
		// never-submitted operations without touching the kernel objects; the loop's
		// destruction right after releases those wholesale
		virtual void f_AbandonPendingTeardown();

		// The wake event of the queue whose thread hosts this loop, handed over at attach. A loop
		// that can fold that event's park into its own kernel wait accepts it and returns true
		// from f_ParksOnQueueEvent; the queue then skips its explicit f_Wake on signals, because
		// the futex wake the event already performs is what reaches the parked loop
		virtual void f_SetParkEvent(NThread::CEventAutoReset *_pEvent);

		virtual bool f_ParksOnQueueEvent() const;
	};

// Io loops
//
// Io objects are serviced by one shared loop on its own thread unless a thread claims them.
// Claiming matters because the claimed loop can be parked in by a worker thread, which lets an
// event be reported on the very thread that will act on it instead of being handed across. The
// loop is io-kind agnostic: it watches file descriptors and completes submitted transfers against
// them, reporting through per-registration callbacks. Sockets are its current consumer, and
// anything else the platform backend can wait on or complete (file operations under io_uring) can
// ride the same loop

	// Readiness a loop reports for a registered descriptor. The bits are deliberately finer
	// grained than a poll mask so each platform's close semantics survive the translation and the
	// consumer can keep one decoder for all backends:
	// - Read/Write: the descriptor is readable/writable. What readable means (data, an incoming
	//   connection, a finished connect) is the consumer's to decide from its own mode
	// - ReadClosed: the peer closed its writing side; reads still drain what is buffered
	//   (EPOLLRDHUP, kqueue EV_EOF on the read filter)
	// - Hup: the connection is gone with no error implied by the event itself (EPOLLHUP)
	// - WriteClosed: the writing side is finished (kqueue EV_EOF on the write filter); whether
	//   that is an orderly close or a failure is answered by the socket error, which the consumer
	//   fetches itself
	// - Error: an error condition. _Error carries the platform error when the backend already has
	//   it (kqueue EV_ERROR, a failed registration); 0 means the consumer fetches it from the
	//   descriptor itself
	enum class EIoLoopEvent : uint32
	{
		mc_None			= 0,
		mc_Read			= 1 << 0,
		mc_Write		= 1 << 1,
		mc_ReadClosed	= 1 << 2,
		mc_Hup			= 1 << 3,
		mc_WriteClosed	= 1 << 4,
		mc_Error		= 1 << 5,
	};

	// Invoked by the loop on the thread currently driving it, during a dispatch pass. _Events is
	// a combination of EIoLoopEvent bits, or mc_None for the registration-applied notification a
	// registration can ask for. A plain function pointer with a token rather than a stored
	// functor: every consumer of a given kind shares one decode function, so a registration
	// carries no allocation
	using FIoLoopReadinessCallback = void (*)(void *_pToken, EIoLoopEvent _Events, int _Error);

	enum class EIoCompletionStatus : uint8
	{
		mc_Done,
		// The operation never reached the kernel or was cancelled by deregistration or loop
		// teardown. The buffers are no longer referenced by the kernel, and nothing further
		// completes
		mc_Cancelled,
		mc_Error,
	};

	// Result of a completion transfer. A receive that completes with Done and zero bytes is end
	// of stream, matching what a zero read means on a readable descriptor
	struct CIoCompletion
	{
		static constexpr umint mc_iTransferNone = ~umint(0);

		umint m_nBytes = 0;

		// Which of the submitter's own transfers this answers. The loop neither sets nor reads it:
		// it names something only whoever wrapped the completion knows about, and travels with the
		// result so a submitter that has several outstanding can tell them apart without keeping a
		// side channel — one that a transfer completed without ever reaching the loop would not be
		// in, and would therefore answer under the wrong name
		umint m_iTransfer = mc_iTransferNone;

		int32 m_Error = 0; // Platform error code when m_Status is Error
		EIoCompletionStatus m_Status = EIoCompletionStatus::mc_Done;
	};

	// Invoked exactly once per submitted operation, on the thread that drives the loop. For a
	// receive the kernel is done with the operation's buffers by the time it runs; for a send the
	// buffers may still be referenced — a zero copy send keeps its pages until the peer
	// acknowledges them — and become reusable only when the operation's buffer-released functor
	// runs
	using FIoCompletion = NFunction::TCFunctionMovable<void (CIoCompletion _Result)>;

	// Invoked exactly once per accepted send, on the thread that drives the loop, at or after the
	// completion functor: the kernel no longer references the operation's buffers and they may be
	// reused. For an ordinary send it runs directly after the completion; for a zero copy send it
	// runs when the notification arrives, which waits on the peer and can be long after the
	// result — several sends' buffers can be awaiting release at once even though only one send
	// is ever in flight, and that generation count is what the send depth bounds
	using FIoBufferReleased = NFunction::TCFunctionMovable<void ()>;

	// One delivery of a receive stream: a span of arrived bytes, or the stream's end. Defined in
	// Mib/Core/IoStream — this header precedes the storage types the definition carries by value
	struct CIoStreamSegment;

	// The accounting a receive stream's buffers are charged against; defined in Mib/Core/IoStream
	struct CIoStreamBackpressure;

	// Invoked on the thread that drives the loop, once per segment
	using FIoStreamSink = NFunction::TCFunctionMovable<void (CIoStreamSegment &&_Segment)>;

	// One entry of a vectored operation; defined with the platform interface
	struct CIoSpan;

	// The most spans one submitted vectored operation carries; longer batches are the caller's to
	// split
	constexpr umint gc_IoLoopMaxSubmitSpans = 64;

	// A descriptor's registration with a loop, created by f_Register and owned by the loop. The
	// handle stays valid until the deregistration completes: the blocking f_Deregister returns,
	// or the asynchronous continuation runs. Opaque outside the platform backends
	struct CIoLoopRegistration;

	// The adaptive send window of one registration: the ceiling f_SetSendWindow configured, the
	// start a connection begins at, and the effective size the path has earned in between. Every
	// field belongs to the single consumer that asks f_IsSendWindowFull and sets the window —
	// the loop itself never reads it
	struct CIoSendWindow
	{
		umint m_nMaxBytes = 0;
		umint m_nStartBytes = 0;
		umint m_nEffectiveBytes = 0;
		umint m_nShrinkTargetBytes = 0;
		uint64 m_QueryStamp = 0;
		uint64 m_ShrinkSince = 0;

		// The previous sample, for platforms that derive the rate from two readings
		uint64 m_LastBytesOut = 0;
		uint64 m_LastStamp = 0;
	};

	// The window is full and the asker has more to send: grow the effective window toward the
	// configured one when the path’s bandwidth-delay product asks for it — by no more than a
	// doubling per sample, so one odd reading cannot open it wide. A product at or under the
	// window leaves it where it is: the pipeline then keeps running dry, which is what lets the
	// buffer releases through. A product under three quarters of the window for a whole second
	// brings it down, never below the start; a rate the sender held back never shrinks anything
	void fg_ConsiderIoSendWindowGrowth(CIoSendWindow &_Window, umint _nBandwidthDelayBytes, bool _bAppLimited, uint64 _Now, umint _nShrinkAfterTicks);

	// What a registration asks of the loop beyond its interest mask. A readiness-only registration
	// is never bound to the loop's completion mechanism where that binding is permanent (an IOCP
	// association outlives every owner of the handle), so the handle stays free for another owner
	// — another module, even — to bind; it is driven by readiness polls alone
	struct CIoLoopRegisterOptions
	{
		bool m_bReadinessOnly = false;
	};

	// A loop seen by whatever registers io objects into it. Registration and submission are
	// callable from any thread; readiness callbacks, completion functors and deregistration
	// continuations run synchronously on the thread currently driving the loop — during
	// f_WaitAndDispatch, f_PollAndDispatch, f_DrainForShutdown, or a blocking f_Deregister
	// self-driven on the owner thread
	struct ICIoLoop : public ICThreadIoLoop
	{
		~ICIoLoop() override;

		// Called on the thread that will drive the loop, before it starts to. Deregistering
		// normally waits for the driving thread to acknowledge the removal, which would be
		// waiting on itself when the removal happens on that thread, so the loop has to know
		// whose thread it is
		virtual void f_SetOwnerThreadToCurrent() = 0;

		// Registers a descriptor. _EventMask is the EIoLoopEvent interest (Read/Write; the close
		// and error classes are always reported). _pToken is handed back verbatim to _fOnEvents
		// and never dereferenced by the loop. With _bNotifyRegistered the callback is invoked
		// once with 0 events when the registration has been applied, so a consumer can report
		// state that predates the registration
		virtual auto f_Register(CIoLoopHandle _Handle, void *_pToken, EIoLoopEvent _EventMask, FIoLoopReadinessCallback _fOnEvents, bool _bNotifyRegistered, CIoLoopRegisterOptions const &_Options = CIoLoopRegisterOptions()) -> CIoLoopRegistration * = 0;

		// Requests the next readiness report for the given EIoLoopEvent bits, callable from any
		// thread. Only meaningful directly after a would-block observation on that direction: a
		// backend with standing interest delivers nothing for a request on a currently-ready
		// descriptor until the next readiness transition — which the preceding would-block
		// observation guarantees is coming. Requests coalesce; spurious reports remain legal, and
		// backends with standing interest may ignore requests entirely. f_Register performs one
		// implicit request of the full event mask.
		//
		// The close class may be named as well. It is level state, not an edge: a backend whose
		// kernel has no event for the pair of half closes — a shutdown after the peer's reported
		// disconnect completes the connection with nothing further to poll for — answers such a
		// request at once with the disconnect it already holds, so the consumer completing the
		// pair learns of the full close. Backends whose kernel reports the full close itself may
		// ignore the bits
		virtual void f_RequestReadiness(CIoLoopRegistration *_pRegistration, EIoLoopEvent _EventMask);

		// Removes a registration and blocks until no callback can be in flight. On the owner
		// thread the loop drives itself to the acknowledgement instead of waiting on itself
		virtual void f_Deregister(CIoLoopRegistration *_pRegistration) = 0;

		// Removes a registration without blocking: the caller never waits, and _fOnDeregistered
		// runs on the loop's thread once the removal has been applied and no callback can be in
		// flight. The continuation may destroy the io object, so the loop touches neither token
		// nor registration after invoking it. The descriptor stays open and owned by the caller
		// until the continuation runs (or the blocking f_Deregister returns): it must not be
		// closed, reused, or re-registered before that, which is what lets the backend target it
		// by number throughout the removal
		virtual void f_DeregisterAsync(CIoLoopRegistration *_pRegistration, NFunction::TCFunctionMovable<void ()> &&_fOnDeregistered) = 0;

		// Completion sends: vectored sends submitted against a registration, reported through
		// their completion functors on the loop's thread, one at a time and in submission order —
		// which is what keeps the stream in order with no link protocol. How many the loop keeps
		// with the kernel at once is its own: the io_uring backend holds later submits back until
		// the one before completes, the IOCP backend pipelines several and reports them from the
		// head of its queue. False from the submit means the operation was not accepted and the
		// caller keeps using readiness io.
		//
		// Synchronization is the caller's: all submits against one registration — sends here,
		// stream starts and resumes below — must be sequenced by a single owner, and the last of
		// them must happen-before that owner requests deregistration. Within that ordering no
		// state check is needed at submit time: a submit queued ahead of a removal is resolved
		// on the loop's thread, where the operation completes as Cancelled. A submit concurrent
		// with (or after) the removal request is outside the contract — the registration may
		// already be freed
		virtual bool f_SupportsCompletionIo() const;

		// How many sends' buffers may be awaiting release at once for one io object — the zero
		// copy generation cap, not an operation concurrency: completions are still reported one
		// at a time. The default says one, which is what every caller assumed before any could
		// pipeline
		virtual umint f_GetCompletionSendDepth() const;

		// Whether an accepted send's buffer-released functor runs directly after its completion
		// for this registration, so no buffer outlives the reported result. False while zero copy
		// sends are possible, since their pages stay pinned until the peer acknowledges them —
		// and a backend that has not yet settled whether zero copy applies answers false. A
		// caller that would otherwise copy bytes out of storage it recycles at the completion
		// can skip that copy on a true answer
		virtual bool f_SendReleaseIsPrompt(CIoLoopRegistration const *_pRegistration) const;

		virtual bool f_SubmitSendVectored(CIoLoopRegistration *_pRegistration, CIoSpan const *_pSpans, umint _nSpans, FIoCompletion &&_fOnComplete, FIoBufferReleased &&_fOnBufferReleased);

		// Receive stream: one standing kernel receive per registration, delivered in order
		// through the sink, each segment carrying its buffer by reference. Started at most once
		// per registration; ends with exactly one terminal segment — at end of stream, on error,
		// or when the registration is deregistered. False means the backend cannot provide it
		// and the caller keeps using readiness receives.
		//
		// _pBackpressure bounds how much buffer capacity may be outstanding at once; when the
		// limit parks the stream, its resume functor fires on the release that crosses the
		// resume threshold, and the owner answers by calling f_ResumeReceiveStream. The resume
		// functor may fire on any thread — the owner reschedules onto its own sequence before
		// calling back in, because start and resume fall under the same owner-sequencing
		// contract as completion sends above
		virtual bool f_SupportsReceiveStream() const;
		virtual bool f_StartReceiveStream(CIoLoopRegistration *_pRegistration, umint _nBufferBytes, NStorage::TCSharedPointer<CIoStreamBackpressure> _pBackpressure, FIoStreamSink &&_fSink);
		virtual void f_ResumeReceiveStream(CIoLoopRegistration *_pRegistration);

		// The bytes the registration's sends may hold unreleased at once, for a loop whose sends on
		// this socket finish only at the peer's acknowledgement: such a loop issues queued sends
		// while the unacknowledged bytes are within the window, instead of within its count of sends
		// in flight. Falls under the owner-sequencing contract of the sends. Loops whose sends
		// release promptly, or that leave the window to the kernel's buffers, ignore it
		virtual void f_SetSendWindow(CIoLoopRegistration *_pRegistration, umint _nBytes);

		// Whether the registration’s sends should pause: the bytes whose release functors have not
		// run have reached the window the path has earned. The consumer asks before gathering
		// another batch; a full answer leaves the batch in the consumer’s own queue, and the next
		// release re-asks. _nStartBytes is the window the connection begins with — a few frames —
		// which the ask grows toward the configured window only while full answers would otherwise
		// throttle a path whose bandwidth-delay product needs more. Falls under the owner-sequencing
		// contract of the sends. Loops that leave the window to the kernel’s buffers answer false
		virtual bool f_IsSendWindowFull(CIoLoopRegistration *_pRegistration, umint _nUnreleasedBytes, umint _nStartBytes);

		// Takes over a handle another owner gave up, ahead of registering it: where the loop's
		// completion binding is permanent this binds the handle to this loop's port, which is
		// exactly what fails for a handle a previous owner had bound elsewhere. False with the
		// platform error then, and the caller refuses the handle; true on platforms with nothing
		// to bind
		virtual bool f_AdoptHandle(CIoLoopHandle _Handle, int &o_Error);

		// True for loops handed out for worker threads to park in (fg_CreateIoLoop), false for a
		// platform's shared loop on its own thread. What lets an io object be asked which loop it
		// belongs to — restoring a binding needs the claimed loop, and the shared loop is not a
		// binding
		bool m_bCreatedAsLoop = false;
	};

	// Returns nullptr where the platform has no loop of its own, in which case io objects keep
	// using the shared one. The caller owns the result and must keep it alive until nothing
	// refers to it
	ICIoLoop *fg_CreateIoLoop();
	void fg_DestroyIoLoop(ICIoLoop *_pLoop);

	// Io objects started on this thread register with this loop rather than the shared one, so
	// they land on the intended loop instead of being moved there afterwards. Set around the
	// call that creates the object and cleared again right after
	void fg_SetThreadIoLoop(ICIoLoop *_pLoop);
	ICIoLoop *fg_GetThreadIoLoop();
}
