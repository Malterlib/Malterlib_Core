# I/O loop contracts

## ICThreadIoLoop::f_PollAndDispatch

Poll without blocking and return whether anything was reported. The idle path must be cheap because workers
poll between drains.

## ICThreadIoLoop::f_Wake

Callable from any thread. Interrupt an active wait or make the next wait return without blocking.

## ICIoLoop::f_Register

Register Read/Write interest; close and error events are always reported. The loop returns the opaque token to
the callback. With notification requested, a callback with no events acknowledges applied registration.

## ICIoLoop::f_RequestReadiness

Request Read/Write only after observing would-block; standing-interest backends may wait for the next
transition or ignore the request. Requests coalesce, spurious reports are legal, and registration implicitly
requests the full mask. Close requests may immediately replay held disconnect state where the kernel does not
report the completed pair of half-closes.

## ICIoLoop::f_Deregister

Block until no callback remains in flight. On the owner thread, drive the loop to acknowledgement; never call
synchronously inside dispatch.

## ICIoLoop::f_DeregisterAsync

The continuation runs on the loop thread after callbacks finish and may destroy the I/O object. Keep the
descriptor open, owned, and unreused until then. The loop must not touch its token or registration after
invoking the continuation.

## Submission ordering

All submissions on a registration must be sequenced by one owner and happen before deregistration is
requested. Sends complete in submission order; accepted operations queued before removal complete as
cancelled. Submitting concurrently with removal can access a freed registration.

## Receive streams

Start at most once per registration. Accepted streams deliver ordered retaining segments and exactly one
terminal segment on EOF, error, or deregistration. Unsupported backends return false.

Backpressure charges whole buffer capacity. Its resume callback can run on any thread; the owner must sequence
start and resume with sends. Set the maximum retained message size before starting so the backend can reserve
headroom for partially used buffers and avoid parking permanently on the consumer's hold.

## CIoLoop_Base::fp_Iterate

Apply pending changes, optionally block, then dispatch. Return the number of reported events excluding wakes.

## CIoLoop_Base::fp_WakeKernel

Callable from any thread. The wake protocol calls this only for a parked owner with no previously owed kernel
wake.

## fg_CreatePlatformIoLoop

Return a non-tracked allocation, or nullptr if unavailable. Loops can outlive tracking teardown because they
remain reachable until waking threads have joined.
