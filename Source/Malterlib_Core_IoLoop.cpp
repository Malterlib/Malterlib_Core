// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Core/IoStream>
#include <Mib/Core/System>

namespace NMib::NSys
{
	ICThreadIoLoop::~ICThreadIoLoop()
	{
	}

	ICIoLoop::~ICIoLoop()
	{
	}

	// Sets the loop used by I/O objects started on this thread; restore the binding immediately after creation.
	void fg_SetThreadIoLoop(ICIoLoop *_pLoop)
	{
		fg_SystemThreadLocal().m_pThreadIoLoop = _pLoop;
	}

	ICIoLoop *fg_GetThreadIoLoop()
	{
		return fg_SystemThreadLocal().m_pThreadIoLoop;
	}

	// Called on the owner before exit; drive pending deregistrations and deferred destruction to quiescence.
	void ICThreadIoLoop::f_DrainForShutdown()
	{
		f_PollAndDispatch();
	}

	// Accept the hosting queue's event when the backend can include it in its kernel wait.
	// f_ParksOnQueueEvent then tells signallers to omit the explicit loop wake.
	void ICThreadIoLoop::f_SetParkEvent(NThread::CEventAutoReset *_pEvent)
	{
	}

	bool ICThreadIoLoop::f_ParksOnQueueEvent() const
	{
		return false;
	}

	void ICIoLoop::f_RequestReadiness(CIoLoopRegistration *_pRegistration, EIoLoopEvent _EventMask)
	{
	}

	// Maximum send-buffer generations awaiting release per object, not simultaneous operation count.
	umint ICIoLoop::f_GetCompletionSendDepth() const
	{
		return 1;
	}

	bool ICIoLoop::f_SupportsCompletionIo() const
	{
		return false;
	}

	// True only when buffer release immediately follows completion; false also covers unsettled zero-copy eligibility.
	bool ICIoLoop::f_SendReleaseIsPrompt(CIoLoopRegistration const *_pRegistration) const
	{
		return true;
	}

	// Returns scheduled bytes, possibly a prefix; zero is terminal refusal. Done means all scheduled bytes sent; a short kernel transfer reports Error.
	umint ICIoLoop::f_SubmitSendVectored(CIoLoopRegistration *_pRegistration, CIoSpan const *_pSpans, umint _nSpans, FIoCompletion &&_fOnComplete, FIoBufferReleased &&_fOnBufferReleased)
	{
		return 0;
	}

	bool ICIoLoop::f_SupportsReceiveStream() const
	{
		return false;
	}

	bool ICIoLoop::f_StartReceiveStream(CIoLoopRegistration *_pRegistration, umint _nBufferBytes, NStorage::TCSharedPointer<CIoStreamBackpressure> _pBackpressure, FIoStreamSink &&_fSink)
	{
		return false;
	}

	void ICIoLoop::f_ResumeReceiveStream(CIoLoopRegistration *_pRegistration)
	{
	}

	// Age the minimum by time: a saturated enlarged window may never produce another low-occupancy sample.
	// Under the lag lock; an older clock reading from another thread counts as no elapsed time.
	static void fsg_RollIoSendLagEpochs(CIoSendWindow &_Window, uint64 _Now, umint _nEpochTicks)
	{
		if (!_Window.m_LagEpochStamp)
		{
			_Window.m_LagEpochStamp = _Now;
			return;
		}

		uint64 nElapsed = _Now > _Window.m_LagEpochStamp ? _Now - _Window.m_LagEpochStamp : 0;
		if (nElapsed < _nEpochTicks)
			return;

		_Window.m_MinReleaseLagTicks[1] = nElapsed >= 2 * _nEpochTicks ? 0 : _Window.m_MinReleaseLagTicks[0];
		_Window.m_MinReleaseLagTicks[0] = 0;
		_Window.m_LagEpochStamp = _Now;
	}

	// Records submit-to-release latency; callable from any thread. Epoch length bounds stale-minimum lifetime.
	void fg_SampleIoSendReleaseLag(CIoSendWindow &_Window, uint64 _LagTicks, uint64 _Now, umint _nEpochTicks)
	{
		DMibLock(_Window.m_LagLock);

		fsg_RollIoSendLagEpochs(_Window, _Now, _nEpochTicks);

		if (!_Window.m_MinReleaseLagTicks[0] || _LagTicks < _Window.m_MinReleaseLagTicks[0])
			_Window.m_MinReleaseLagTicks[0] = _LagTicks;
	}

	// Returns the recent minimum aged to _Now, or zero without a sample. Callable from any thread.
	uint64 fg_GetIoSendMinReleaseLag(CIoSendWindow &_Window, uint64 _Now, umint _nEpochTicks)
	{
		DMibLock(_Window.m_LagLock);

		fsg_RollIoSendLagEpochs(_Window, _Now, _nEpochTicks);

		uint64 nLagTicks = _Window.m_MinReleaseLagTicks[0];
		if (_Window.m_MinReleaseLagTicks[1] && (!nLagTicks || _Window.m_MinReleaseLagTicks[1] < nLagTicks))
			nLagTicks = _Window.m_MinReleaseLagTicks[1];

		return nLagTicks;
	}

	// Use minimum release lag rather than average so growth does not chase self-queueing.
	// Target delivery-rate times lag plus two sends; grow by at most double per sample.
	// Shrink sustained targets below three quarters, never below the start; app-limited samples must not shrink the window.
	void fg_ConsiderIoSendWindowGrowth(CIoSendWindow &_Window, umint _nDeliveryRateBytes, bool _bAppLimited, uint64 _Now, umint _nTicksPerSecond, umint _nShrinkAfterTicks)
	{
		uint64 nLagTicks = fg_GetIoSendMinReleaseLag(_Window, _Now, _nShrinkAfterTicks);

		umint nWindow = _Window.m_nEffectiveBytes;
		umint nHeadroom = fg_Max(2 * _Window.m_nLargestSendBytes, _Window.m_nStartBytes / 4);

		// Without recent low-occupancy samples, decay the target to let unbiased latency samples resume.
		umint nTarget;
		if (nLagTicks)
		{
			// Saturate the rate-lag product before adding headroom so neither operation wraps.
			uint64 nProduct = uint64(_nDeliveryRateBytes) > TCLimitsInt<uint64>::mc_Max / nLagTicks
				? TCLimitsInt<uint64>::mc_Max
				: uint64(_nDeliveryRateBytes) * nLagTicks / _nTicksPerSecond
			;
			if (nProduct >= uint64(_Window.m_nMaxBytes))
				nTarget = _Window.m_nMaxBytes;
			else
				nTarget = umint(fg_Min(nProduct + nProduct / 4 + nHeadroom, uint64(_Window.m_nMaxBytes)));
		}
		else
			nTarget = fg_Max(nHeadroom, nWindow - nWindow / 4 - 1);
		if (nTarget > nWindow)
		{
			// Double in 64 bits because the window can exceed half of umint; retain the configured ceiling.
			uint64 nGrown = fg_Min(uint64(nTarget), uint64(nWindow) * 2);
			if (_Window.m_nMaxBytes)
				nGrown = fg_Min(nGrown, uint64(_Window.m_nMaxBytes));

			_Window.m_nEffectiveBytes = umint(nGrown);
			_Window.m_ShrinkSince = 0;
		}
		else if (_bAppLimited || nTarget >= nWindow - nWindow / 4)
			_Window.m_ShrinkSince = 0;
		else
		{
			if (!_Window.m_ShrinkSince)
				_Window.m_ShrinkSince = _Now;

			_Window.m_nShrinkTargetBytes = nTarget;
			if (_Now - _Window.m_ShrinkSince >= _nShrinkAfterTicks)
			{
				_Window.m_nEffectiveBytes = fg_Max(_Window.m_nShrinkTargetBytes, _Window.m_nStartBytes);
				_Window.m_ShrinkSince = 0;
			}
		}
	}

	// Sets the unreleased-byte limit for sends that complete on peer acknowledgement. The registration owner must sequence this with sends.
	void ICIoLoop::f_SetSendWindow(CIoLoopRegistration *_pRegistration, umint _nBytes)
	{
	}

	// Ask before gathering another batch and again after release. _nStartBytes is the initial window; growth stays within the configured ceiling.
	// The registration owner sequences this with sends. Kernel-buffered backends return false.
	bool ICIoLoop::f_IsSendWindowFull(CIoLoopRegistration *, umint, umint)
	{
		return false;
	}
	// Adopt before registration. Returns false with the platform error if a permanent completion binding belongs to another loop.
	bool ICIoLoop::f_AdoptHandle(CIoLoopHandle, int &)
	{
		return true;
	}
}
