// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Slow paths for the futex-based NThread::CSemaphoreAggregate and
// NThread::CEventAggregate. The fast paths are inline in Malterlib_Thread.h;
// these live in the platform layer because timed waits need CStopwatchRaw and
// time-speed scaling, which are not available at the Thread header's position
// in the include chain. Included by each platform implementation file.
//
// Destruction contract: a woken waiter may destroy the object as soon as its
// wait returns, so the wake functions must not read object memory — they only
// receive values captured atomically by the signaling operation and issue
// futex wakes, which are safe on stale addresses.

namespace NMib::NThread
{
	void CSemaphoreAggregate::fp_WaitSlow()
	{
		uint32 volatile *pValue = fp_GetFutexWord();

		while (true)
		{
			uint64 Current = m_Data.f_FetchAdd(mcp_WaiterOne, NAtomic::gc_MemoryOrder_Relaxed) + mcp_WaiterOne;
			while ((uint32)Current == 0)
			{
				NSys::fg_Futex_Wait(pValue, 0);
				Current = m_Data.f_Load(NAtomic::gc_MemoryOrder_Relaxed);
			}
			m_Data.f_FetchSub(mcp_WaiterOne, NAtomic::gc_MemoryOrder_Relaxed);

			if (f_TryWait())
				return;
		}
	}

	// Returns true if the wait timed out
	bool CSemaphoreAggregate::fp_WaitTimeoutSlow(fp64 _Timeout)
	{
		_Timeout = _Timeout * NTime::CSystem_Time::fs_GetTimeSpeedReciprocal();
		if (_Timeout <= 0.0)
			return true;

		CStopwatchRaw TimeWait;
		TimeWait.f_Start();

		uint32 volatile *pValue = fp_GetFutexWord();

		while (true)
		{
			if (f_TryWait())
				return false;

			fp64 Remaining = _Timeout - TimeWait.f_GetTime();
			if (Remaining <= 0.0)
				return !f_TryWait();

			m_Data.f_FetchAdd(mcp_WaiterOne, NAtomic::gc_MemoryOrder_Relaxed);

			bool bTimedOut = false;
			if ((uint32)m_Data.f_Load(NAtomic::gc_MemoryOrder_Relaxed) == 0)
				bTimedOut = NSys::fg_Futex_WaitTimeout(pValue, 0, Remaining);

			m_Data.f_FetchSub(mcp_WaiterOne, NAtomic::gc_MemoryOrder_Relaxed);

			if (f_TryWait())
				return false;

			if (bTimedOut)
				return true;
		}
	}

	void CSemaphoreAggregate::fp_WakeSlow(uint32 _Delta, uint32 _nWaiters)
	{
		// Only the address is derived from this; no object memory is read (the
		// semaphore may already be destroyed by a woken waiter)
		uint32 volatile *pValue = fp_GetFutexWord();
		if (_Delta == 1)
			NSys::fg_Futex_WakeOne(pValue);
		else if (_Delta >= _nWaiters)
			NSys::fg_Futex_WakeAll(pValue);
		else
			NSys::fg_Futex_WakeCount(pValue, _Delta);
	}

	void CEventAggregate::fp_WaitSlow()
	{
		// A waiter completes when the event is signaled or when the signal
		// generation moved while it waited (a signal followed by an immediate
		// reset must still release the waiters present at the signal)
		uint32 First = m_State.f_Load(NAtomic::gc_MemoryOrder_Acquire);
		uint32 Observed = First;

		while (true)
		{
			if (Observed & mcp_FlagSignaled)
				return;

			if (((Observed ^ First) & mcp_GenerationMask) != 0)
				return;

			if (!(Observed & mcp_FlagWaiters))
			{
				if (!m_State.f_CompareExchangeWeak(Observed, Observed | mcp_FlagWaiters, NAtomic::gc_MemoryOrder_Acquire, NAtomic::gc_MemoryOrder_Acquire))
					continue;

				Observed |= mcp_FlagWaiters;
			}

			NSys::fg_Futex_Wait((uint32 volatile *)&m_State, Observed);
			Observed = m_State.f_Load(NAtomic::gc_MemoryOrder_Acquire);
		}
	}

	// Returns true if the wait timed out
	bool CEventAggregate::fp_WaitTimeoutSlow(fp64 _Timeout)
	{
		_Timeout = _Timeout * NTime::CSystem_Time::fs_GetTimeSpeedReciprocal();
		if (_Timeout <= 0.0)
			return true;

		CStopwatchRaw TimeWait;
		TimeWait.f_Start();

		uint32 First = m_State.f_Load(NAtomic::gc_MemoryOrder_Acquire);
		uint32 Observed = First;

		auto fCompleted
			= [&](uint32 _Current)
			{
				return (_Current & mcp_FlagSignaled) != 0 || ((_Current ^ First) & mcp_GenerationMask) != 0;
			}
		;

		while (true)
		{
			if (fCompleted(Observed))
				return false;

			fp64 Remaining = _Timeout - TimeWait.f_GetTime();
			if (Remaining <= 0.0)
				return !fCompleted(m_State.f_Load(NAtomic::gc_MemoryOrder_Acquire));

			if (!(Observed & mcp_FlagWaiters))
			{
				if (!m_State.f_CompareExchangeWeak(Observed, Observed | mcp_FlagWaiters, NAtomic::gc_MemoryOrder_Acquire, NAtomic::gc_MemoryOrder_Acquire))
					continue;

				Observed |= mcp_FlagWaiters;
			}

			bool bTimedOut = NSys::fg_Futex_WaitTimeout((uint32 volatile *)&m_State, Observed, Remaining);
			Observed = m_State.f_Load(NAtomic::gc_MemoryOrder_Acquire);

			if (bTimedOut && !fCompleted(Observed))
				return true;
		}
	}

	void CEventAggregate::fp_WakeAllSlow()
	{
		// Only the address is derived from this; no object memory is read (the
		// event may already be destroyed by a woken waiter)
		NSys::fg_Futex_WakeAll((uint32 volatile *)&m_State);
	}
}
