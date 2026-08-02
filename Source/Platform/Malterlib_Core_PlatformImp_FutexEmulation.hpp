// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Parking-lot emulation of NSys::fg_Futex_* for POSIX targets without a native
// futex API. Waiters park on a pthread condition in a bucket selected by address.
// Wakes broadcast the whole bucket; distinct addresses that share a bucket wake
// spuriously, which the fg_Futex_* contract allows (callers re-check their
// predicate in a loop).

namespace NMib::NSys::NPrivate
{
	struct align_cacheline CFutexEmulationBucket
	{
		pthread_mutex_t m_Lock = PTHREAD_MUTEX_INITIALIZER;
		pthread_cond_t m_Condition = PTHREAD_COND_INITIALIZER;
	};

	constexpr umint gc_nFutexEmulationBuckets = 64;

	CFutexEmulationBucket g_FutexEmulationBuckets[gc_nFutexEmulationBuckets];

	inline_small CFutexEmulationBucket &fg_GetFutexEmulationBucket(uint32 volatile *_pAddress)
	{
		return g_FutexEmulationBuckets[((umint)_pAddress >> 2) % gc_nFutexEmulationBuckets];
	}
}

void NSys::fg_Futex_Wait(uint32 volatile *_pAddress, uint32 _Expected)
{
	auto &Bucket = NPrivate::fg_GetFutexEmulationBucket(_pAddress);

	pthread_mutex_lock(&Bucket.m_Lock);
	if (*_pAddress == _Expected)
		pthread_cond_wait(&Bucket.m_Condition, &Bucket.m_Lock);
	pthread_mutex_unlock(&Bucket.m_Lock);
}

bool NSys::fg_Futex_WaitTimeout(uint32 volatile *_pAddress, uint32 _Expected, fp64 _Timeout)
{
	if (_Timeout <= 0.0)
		return true;

	timespec Deadline;
#ifdef CLOCK_REALTIME
	clock_gettime(CLOCK_REALTIME, &Deadline);
#else
	struct timeval TimeVal;
	gettimeofday(&TimeVal, nullptr);
	Deadline.tv_sec = TimeVal.tv_sec;
	Deadline.tv_nsec = TimeVal.tv_usec * 1000;
#endif

	fp64 ToWait = _Timeout + fp64((int64)Deadline.tv_nsec) * (fp64(1.0) / fp64(1000000000.0));
	fp64 nSec = ToWait.f_Floor();
	Deadline.tv_sec += nSec.f_ToInt();
	Deadline.tv_nsec = ((ToWait - nSec) * fp64(1000000000.0)).f_ToInt();

	auto &Bucket = NPrivate::fg_GetFutexEmulationBucket(_pAddress);

	bool bTimedOut = false;

	pthread_mutex_lock(&Bucket.m_Lock);
	if (*_pAddress == _Expected)
		bTimedOut = pthread_cond_timedwait(&Bucket.m_Condition, &Bucket.m_Lock, &Deadline) == ETIMEDOUT;
	pthread_mutex_unlock(&Bucket.m_Lock);

	return bTimedOut;
}

void NSys::fg_Futex_WakeOne(uint32 volatile *_pAddress)
{
	auto &Bucket = NPrivate::fg_GetFutexEmulationBucket(_pAddress);

	// Broadcast, not signal: a signal could be consumed by a waiter on a different
	// address sharing this bucket, which would lose the wake for the intended one
	pthread_mutex_lock(&Bucket.m_Lock);
	pthread_cond_broadcast(&Bucket.m_Condition);
	pthread_mutex_unlock(&Bucket.m_Lock);
}

void NSys::fg_Futex_WakeCount(uint32 volatile *_pAddress, uint32 _nToWake)
{
	if (_nToWake)
		fg_Futex_WakeAll(_pAddress);
}

void NSys::fg_Futex_WakeAll(uint32 volatile *_pAddress)
{
	auto &Bucket = NPrivate::fg_GetFutexEmulationBucket(_pAddress);

	pthread_mutex_lock(&Bucket.m_Lock);
	pthread_cond_broadcast(&Bucket.m_Condition);
	pthread_mutex_unlock(&Bucket.m_Lock);
}
