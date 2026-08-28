// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Core_IoLoop.h"

#include <Mib/Storage/SharedPointer>
#include <Mib/Atomic/Atomic>
#include <Mib/Function/Function>
#include <Mib/Thread/Thread>

namespace NMib::NSys
{
	// One delivery of a receive stream: a span of arrived bytes, or the stream's end. The
	// segment carries a reference to whatever owns the bytes; the buffer is freed — and its
	// capacity charged against the stream's backpressure accounting released — when the last
	// reference anywhere is dropped, wherever that happens. Terminal segments (Done with zero
	// bytes, Error, Cancelled) carry no owner; segments are delivered in stream order, and
	// exactly one terminal ends the stream
	struct CIoStreamSegment
	{
		NStorage::TCSharedPointer<CVirtualDestroyBase const> m_pOwner;
		void const *m_pData = nullptr;
		umint m_nBytes = 0;
		int32 m_Error = 0;
		EIoCompletionStatus m_Status = EIoCompletionStatus::mc_Done;
	};

	// The accounting a receive stream's buffers are charged against, shared between the loop and
	// every buffer the stream has delivered. Lock free end to end: buffer death decrements the
	// count on whatever thread it happens; the loop reads it when deciding whether to hand the
	// kernel another buffer, and parks the stream over the limit; the decrement that crosses the
	// resume threshold while parked claims the flag and invokes the resume functor — installed
	// once before the stream starts, immutable after — which reschedules through whoever owns
	// the stream. The park protocol re-checks the count after setting the flag, so a decrement
	// racing the park cannot strand a stream below the limit
	struct CIoStreamBackpressure
	{
		// Buffer capacity delivered and not yet released, in bytes — capacity rather than
		// payload, because it is memory this bounds
		NAtomic::TCAtomic<umint> m_nOutstandingBytes = 0;

		// Zero = unlimited. Resume is lower than the limit so the stream does not ping-pong at
		// the boundary
		umint m_nLimitBytes = 0;
		umint m_nResumeBytes = 0;

		NAtomic::TCAtomic<uint32> m_bParked = 0;

		NFunction::TCFunctionMovable<void ()> m_fResume;
	};

	// The adaptive send window of one registration: the ceiling f_SetSendWindow configured, the
	// start a connection begins at, and the effective size the path has earned in between. The
	// fields belong to the single consumer that asks f_IsSendWindowFull and sets the window,
	// except the release lag epochs: a completion loop samples those on its own thread as the
	// kernel releases each send, and the asker can be on another thread when the socket lives on
	// the shared loop, so they are kept under their own lock
	struct CIoSendWindow
	{
		umint m_nMaxBytes = 0;
		umint m_nStartBytes = 0;
		umint m_nEffectiveBytes = 0;
		umint m_nShrinkTargetBytes = 0;

		// The largest single send this registration has submitted: the producer’s actual
		// granularity, which the growth target keeps two of above the rate-latency product
		// so the window can always carry a fresh send while one awaits release
		umint m_nLargestSendBytes = 0;

		uint64 m_QueryStamp = 0;
		uint64 m_ShrinkSince = 0;

		// The previous sample, for platforms that derive the rate from two readings
		uint64 m_LastBytesOut = 0;
		uint64 m_LastStamp = 0;

		// The release latency's sliding minimum: two epochs of the lowest submit-to-release
		// lag seen, so the target multiplies the delivery rate by the lag a release meets with
		// no self-queueing ahead of it, and a changed path re-teaches it within two epochs.
		// Under m_LagLock, held for the few loads and stores of a sample or a query: the
		// sampler and the asker each roll the epochs on their own clock reading
		NThread::CLowLevelLock m_LagLock;
		uint64 m_MinReleaseLagTicks[2] = {};
		uint64 m_LagEpochStamp = 0;
	};

	// One submit-to-release latency observation for a registration's window; the epoch length
	// bounds how long a stale minimum survives. Callable from any thread
	void fg_SampleIoSendReleaseLag(CIoSendWindow &_Window, uint64 _LagTicks, uint64 _Now, umint _nEpochTicks);

	// The least release lag the window has observed lately, 0 when none, the epochs aged to
	// _Now. Callable from any thread
	uint64 fg_GetIoSendMinReleaseLag(CIoSendWindow &_Window, uint64 _Now, umint _nEpochTicks);

	// The window is full and the asker has more to send: grow the effective window toward the
	// configured one when the path asks for it — by no more than a doubling per sample, so one
	// odd reading cannot open it wide. The target is the delivery rate times the least release
	// latency the window has observed — what must stay in flight for the pipeline to never run
	// dry of it — plus two whole sends of headroom, since without the granularity term a
	// window of one send can never admit a second. The least lag rather than the average keeps
	// the product from chasing its own queue: the average lag times the rate is, by Little's
	// law, whatever is in flight right now. A target at or under the window leaves it where it
	// is; one under three quarters of the window for a whole second brings it down, never
	// below the start; a rate the sender held back never shrinks anything
	void fg_ConsiderIoSendWindowGrowth(CIoSendWindow &_Window, umint _nDeliveryRateBytes, bool _bAppLimited, uint64 _Now, umint _nTicksPerSecond, umint _nShrinkAfterTicks);
}
