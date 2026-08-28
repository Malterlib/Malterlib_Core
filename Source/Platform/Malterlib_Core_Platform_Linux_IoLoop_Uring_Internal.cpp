// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Linux_IoLoop_Uring_Internal.h"
#include "Malterlib_Core_Platform_Linux_IoSubSystem.h"

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

using namespace NMib;
using namespace NMib::NMemory;
using namespace NMib::NSys;

namespace
{
	// Whether this descriptor's peer is somewhere a zero copy send is worth doing. A unix socket
	// or a loopback address is not: the pages end up referenced by the peer's receive queue on the
	// same machine, so nothing is saved and they stay pinned until it reads.
	//
	// MalterlibIoUringZeroCopyLocal=1 says yes regardless and =0 says no regardless, so both sides
	// of the decision can be measured
	bool fg_UringPeerIsRemote(CIoSubSystem_Linux *_pIo, int _Fd)
	{
		sockaddr_storage Peer;
		socklen_t nPeer = sizeof(Peer);
		fg_MemClear(&Peer, sizeof(Peer));

		if (getpeername(_Fd, (sockaddr *)&Peer, &nPeer) != 0)
			return false;

		// A unix socket can never do zero copy — SENDMSG_ZC is rejected outright for the
		// family, not degraded to a copy (measured: every forced send fails) — so the exclusion
		// holds whatever the override says. The override picks which side of the loopback line
		// an internet peer is treated as being on
		if (Peer.ss_family != AF_INET && Peer.ss_family != AF_INET6)
			return false;

		switch (_pIo->f_ZeroCopyOverride())
		{
		case EUringZeroCopyOverride::mc_Never:
			return false;
		case EUringZeroCopyOverride::mc_Always:
			return true;
		case EUringZeroCopyOverride::mc_None:
			break;
		}

		if (Peer.ss_family == AF_INET)
		{
			auto const &Address = *(sockaddr_in const *)&Peer;

			// Network byte order, so the first octet is the first byte in memory
			return ((uint8 const *)&Address.sin_addr.s_addr)[0] != 127;
		}

		if (Peer.ss_family == AF_INET6)
		{
			auto const &Address = *(sockaddr_in6 const *)&Peer;

			if (IN6_IS_ADDR_LOOPBACK(&Address.sin6_addr))
				return false;

			// A v4 mapped address carries the v4 rules with it
			if (IN6_IS_ADDR_V4MAPPED(&Address.sin6_addr))
				return Address.sin6_addr.s6_addr[12] != 127;

			return true;
		}

		return false;
	}
}

#if DMibConfig_IoDebug_Enable
uint64 fg_UringStatsNow()
{
	timespec Time;
	clock_gettime(CLOCK_MONOTONIC_RAW, &Time);
	return (uint64)Time.tv_sec * 1000000000 + (uint64)Time.tv_nsec;
}
#endif

void fg_UringProbePeerClass(CIoSubSystem_Linux *_pIo, CUringRegistration *_pRegistration)
{
	if (_pRegistration->m_bZeroCopyProbed)
		return;

	_pRegistration->m_bZeroCopyProbed = true;
	_pRegistration->m_bZeroCopyEligible = fg_UringPeerIsRemote(_pIo, _pRegistration->m_Handle);

#if DMibConfig_IoDebug_Enable
	if (_pIo->f_TraceEnabled())
		fg_UringTrace(_pRegistration->m_bZeroCopyEligible ? "zerocopy-eligible" : "zerocopy-local", _pRegistration->m_pToken, _pRegistration->m_Handle, 0);
#endif
}

umint fg_UringRingBytes(umint _nEntries)
{
	umint PageSize = NSys::fg_Mem_VirtualGranularityCommit(false);

	return (_nEntries * sizeof(CIoUringBuf) + PageSize - 1) & ~(PageSize - 1);
}

void *fg_UringAllocRing(umint _nRingBytes)
{
	return CDefaultAllocator::f_AllocAligned(_nRingBytes, NSys::fg_Mem_VirtualGranularityCommit(false));
}

// Newest first: the most recently returned block is the hottest
uint8 *CUringBufferRecycler::f_TryTake()
{
	DMibLock(m_Lock);
	if (m_FreeBlocks.f_IsEmpty())
		return nullptr;

	return m_FreeBlocks.f_Pop();
}

uint16 CUringRecvRing::f_OrderFront() const
{
	return m_PublishOrder[m_iOrderHead & (m_nRingEntries - 1)];
}

CUringBufferRecycler::~CUringBufferRecycler()
{
	for (uint8 *pBlock : m_FreeBlocks)
		NMemory::CDefaultAllocator::f_Free(pBlock, m_nBufferBytes);
}

// Returns the block to the stack, or refuses — full or dead — and the caller frees
bool CUringBufferRecycler::f_TryReturn(uint8 *_pBlock)
{
	DMibLock(m_Lock);
	if (m_bDead || m_FreeBlocks.f_GetLen() >= m_nMaxFree)
		return false;

	m_FreeBlocks.f_InsertLast(_pBlock);

	return true;
}

void CUringBufferRecycler::f_Die()
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

CUringStreamBuffer::~CUringStreamBuffer()
{
	// The block goes back to its ring's recycle stack while the ring wants it — the next
	// refill reuses it cache-hot — and to the allocator once the stack is full or the
	// ring is gone. An exact sized raw allocation, headerless on purpose: a power of two
	// buffer plus a vector header would round up into the next size class and waste half
	// the block
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

void CUringRecvRing::f_OrderPop()
{
	++m_iOrderHead;
	--m_nOrderCount;
}

// Loop thread only: lends _pBuffer's bytes to the kernel under _Bid
void CUringRecvRing::f_PublishBuffer(uint16 _Bid, CUringStreamBuffer &_Buffer)
{
	umint iEntry = m_Tail & (m_nRingEntries - 1);
	m_pRingEntries[iEntry].m_Addr = (uint64)(umint)_Buffer.m_pData;
	m_pRingEntries[iEntry].m_Len = (uint32)m_nBufferBytes;
	m_pRingEntries[iEntry].m_Bid = _Bid;
	++m_Tail;

	m_PublishOrder[(m_iOrderHead + m_nOrderCount) & (m_nRingEntries - 1)] = _Bid;
	++m_nOrderCount;

	__atomic_store_n(m_pRingTail, m_Tail, __ATOMIC_RELEASE);
}

void CUringSendRing::f_PublishSpan(void const *_pData, umint _nBytes)
{
	// The entry's length is the kernel's 32 bit one; fp_PublishSend splits a wider span before
	// it gets here
	DMibFastCheck(_nBytes <= TCLimitsInt<uint32>::mc_Max);

	umint iEntry = m_Tail & (m_nRingEntries - 1);
	m_pRingEntries[iEntry].m_Addr = (uint64)(umint)_pData;
	m_pRingEntries[iEntry].m_Len = (uint32)_nBytes;
	m_pRingEntries[iEntry].m_Bid = (uint16)iEntry;
	++m_Tail;
}

void CUringSendRing::f_CommitTail()
{
	__atomic_store_n(m_pRingTail, m_Tail, __ATOMIC_RELEASE);
}
