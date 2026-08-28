// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Core_IoLoop.h"

#include <Mib/Storage/SharedPointer>
#include <Mib/Atomic/Atomic>
#include <Mib/Function/Function>

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
}
