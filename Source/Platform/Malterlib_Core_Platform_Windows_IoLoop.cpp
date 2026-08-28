// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Windows_IoLoop_Iocp_Internal.h"
#include "Malterlib_Core_Platform_Windows_Optional.h"

using namespace NMib;
using namespace NMib::NMemory;
using namespace NMib::NSys;

#if DMibConfig_IoDebug_Enable

// Monotonic nanoseconds for the send lag measurements
uint64 fg_IocpStatsNow()
{
	int64 Ticks = NTime::CSystem_Time::fs_GetTimerValue();
	int64 Frequency = NTime::CSystem_Time::fs_TimerFrequency();
	return (uint64)((Ticks / Frequency) * 1000000000 + ((Ticks % Frequency) * 1000000000) / Frequency);
}


#endif

// Newest first: the most recently returned block is the hottest
uint8 *CIocpBufferRecycler::f_TryTake()
{
	DMibLock(m_Lock);
	if (m_FreeBlocks.f_IsEmpty())
		return nullptr;

	return m_FreeBlocks.f_Pop();
}

CIocpBufferRecycler::~CIocpBufferRecycler()
{
	for (uint8 *pBlock : m_FreeBlocks)
		NMemory::CDefaultAllocator::f_Free(pBlock, m_nBufferBytes);
}

// Returns the block to the stack, or refuses — full or dead — and the caller frees
bool CIocpBufferRecycler::f_TryReturn(uint8 *_pBlock)
{
	DMibLock(m_Lock);
	if (m_bDead || m_FreeBlocks.f_GetLen() >= m_nMaxFree)
		return false;

	m_FreeBlocks.f_InsertLast(_pBlock);

	return true;
}

void CIocpBufferRecycler::f_Die()
{
	NContainer::TCVector<uint8 *> Blocks;
	{
		DMibLock(m_Lock);
		m_bDead = true;
		Blocks = fg_Move(m_FreeBlocks);
	}

	for (uint8 *pBlock : Blocks)
		NMemory::CDefaultAllocator::f_Free(pBlock, m_nBufferBytes);
}

CIocpStreamBuffer::~CIocpStreamBuffer()
{
	// The block goes back to its stream's recycle stack while the stream wants it — the next
	// post reuses it cache-hot — and to the allocator once the stack is full or the stream is
	// gone. An exact sized raw allocation, headerless on purpose
	if (m_pData)
	{
		if (!m_pRecycler || !m_pRecycler->f_TryReturn(m_pData))
			NMemory::CDefaultAllocator::f_Free(m_pData, m_nDataBytes);
	}

	if (!m_pBackpressure)
		return;

	auto &Backpressure = *m_pBackpressure;
	umint Previous = Backpressure.m_nOutstandingBytes.f_FetchSub(m_nCharged, NAtomic::gc_MemoryOrder_AcquireRelease);
	umint Now = Previous - m_nCharged;

	// The release that crosses the resume threshold while the stream is parked claims the
	// flag — the exchange dedups concurrent releases to a single resume — and reschedules
	// through the stream's owner. Lock free, and safe on any thread at any time: the functor
	// is immutable once the stream has started and reaches its owner through a weak
	// reference of its own
	if (Now <= Backpressure.m_nResumeBytes && Backpressure.m_bParked.f_Load(NAtomic::gc_MemoryOrder_Acquire))
	{
		if (Backpressure.m_bParked.f_Exchange(0, NAtomic::gc_MemoryOrder_AcquireRelease))
		{
			if (Backpressure.m_fResume)
				Backpressure.m_fResume();
		}
	}
}

NMib::NSys::ICIoLoop *fg_CreatePlatformIoLoop()
{
	// The native entry points the loop cannot do without, resolved with the other optional
	// functions at platform start
	auto const &Functions = NLocal::g_OptionalFunctions;
	if (!Functions.m_fNtCreateFile || !Functions.m_fNtDeviceIoControlFile || !Functions.m_fRtlNtStatusToDosError)
		return nullptr;

	auto *pLoop = fg_ConstructObject<CIoLoop_Iocp>(CAllocator_NonTrackedHeap());
	if (pLoop->f_IsCreated())
		return pLoop;

	fg_DeleteObject(CAllocator_NonTrackedHeap(), pLoop);

	return nullptr;
}
