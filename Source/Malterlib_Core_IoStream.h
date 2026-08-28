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
	// Segments retain their buffer; its last reference releases capacity on whichever thread drops it.
	// Delivery is ordered and ends with exactly one ownerless terminal segment: Done with zero bytes, Error, or Cancelled.
	struct CIoStreamSegment
	{
		NStorage::TCSharedPointer<CVirtualDestroyBase const> m_pOwner;
		void const *m_pData = nullptr;
		umint m_nBytes = 0;
		int32 m_Error = 0;
		EIoCompletionStatus m_Status = EIoCompletionStatus::mc_Done;
	};

	// Buffer release may run on any thread. The release crossing the resume threshold claims the parked flag and invokes the immutable resume callback.
	// Parking must recheck the count after setting the flag so a racing release cannot strand the stream.
	struct CIoStreamBackpressure
	{
		NAtomic::TCAtomic<umint> m_nOutstandingBytes = 0; // Charges capacity, not payload, until the buffer's final reference is released.

		umint m_nLimitBytes = 0; // Zero is unlimited; resume below the limit to avoid oscillation.
		umint m_nResumeBytes = 0;
		umint m_nConsumerHoldBytes = 0; // Maximum payload retained before any release; set before starting so the backend can reserve partial-buffer headroom.

		NAtomic::TCAtomic<uint32> m_bParked = 0;

		NFunction::TCFunctionMovable<void ()> m_fResume;
	};

	// Window fields belong to the registration owner. Release-lag epochs are shared with the completion thread and require m_LagLock.
	struct CIoSendWindow
	{
		umint m_nMaxBytes = 0;
		umint m_nStartBytes = 0;
		umint m_nEffectiveBytes = 0;
		umint m_nShrinkTargetBytes = 0;
		umint m_nLargestSendBytes = 0; // Growth reserves two sends of this size beyond the rate-latency product.

		uint64 m_QueryStamp = 0;
		uint64 m_ShrinkSince = 0;

		uint64 m_LastBytesOut = 0; // Previous cumulative sample for deriving a delivery rate.
		uint64 m_LastStamp = 0;

		NThread::CLowLevelLock m_LagLock; // Protects a two-epoch minimum; sampler and asker both age it using their own clock readings.
		uint64 m_MinReleaseLagTicks[2] = {};
		uint64 m_LagEpochStamp = 0;
	};

	void fg_SampleIoSendReleaseLag(CIoSendWindow &_Window, uint64 _LagTicks, uint64 _Now, umint _nEpochTicks);
	uint64 fg_GetIoSendMinReleaseLag(CIoSendWindow &_Window, uint64 _Now, umint _nEpochTicks);

	void fg_ConsiderIoSendWindowGrowth(CIoSendWindow &_Window, umint _nDeliveryRateBytes, bool _bAppLimited, uint64 _Now, umint _nTicksPerSecond, umint _nShrinkAfterTicks);
}
