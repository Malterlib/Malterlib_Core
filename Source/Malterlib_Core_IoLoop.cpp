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

	void fg_SetThreadIoLoop(ICIoLoop *_pLoop)
	{
		fg_SystemThreadLocal().m_pThreadIoLoop = _pLoop;
	}

	ICIoLoop *fg_GetThreadIoLoop()
	{
		return fg_SystemThreadLocal().m_pThreadIoLoop;
	}

	void ICThreadIoLoop::f_DrainForShutdown()
	{
		f_PollAndDispatch();
	}

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

	umint ICIoLoop::f_GetCompletionSendDepth() const
	{
		return 1;
	}

	bool ICIoLoop::f_SupportsCompletionIo() const
	{
		return false;
	}

	bool ICIoLoop::f_SendReleaseIsPrompt(CIoLoopRegistration const *_pRegistration) const
	{
		return true;
	}

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

	// Ages the sliding minimum on time rather than on samples: a saturated pipeline whose
	// window ballooned takes no more low-occupancy samples, and a minimum that only ages on
	// samples would keep the value that ballooned it forever. Empty epochs zero the product,
	// the target falls to its headroom, the shrink rule brings the window down, and the
	// smaller occupancy is what lets honest samples flow again. Under the window's lag lock;
	// a clock reading behind the stamp, from a thread that read its clock before the sampler
	// stamped, counts as no time passed
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

	void fg_SampleIoSendReleaseLag(CIoSendWindow &_Window, uint64 _LagTicks, uint64 _Now, umint _nEpochTicks)
	{
		DMibLock(_Window.m_LagLock);

		fsg_RollIoSendLagEpochs(_Window, _Now, _nEpochTicks);

		if (!_Window.m_MinReleaseLagTicks[0] || _LagTicks < _Window.m_MinReleaseLagTicks[0])
			_Window.m_MinReleaseLagTicks[0] = _LagTicks;
	}

	uint64 fg_GetIoSendMinReleaseLag(CIoSendWindow &_Window, uint64 _Now, umint _nEpochTicks)
	{
		DMibLock(_Window.m_LagLock);

		fsg_RollIoSendLagEpochs(_Window, _Now, _nEpochTicks);

		uint64 nLagTicks = _Window.m_MinReleaseLagTicks[0];
		if (_Window.m_MinReleaseLagTicks[1] && (!nLagTicks || _Window.m_MinReleaseLagTicks[1] < nLagTicks))
			nLagTicks = _Window.m_MinReleaseLagTicks[1];

		return nLagTicks;
	}

	void fg_ConsiderIoSendWindowGrowth(CIoSendWindow &_Window, umint _nDeliveryRateBytes, bool _bAppLimited, uint64 _Now, umint _nTicksPerSecond, umint _nShrinkAfterTicks)
	{
		uint64 nLagTicks = fg_GetIoSendMinReleaseLag(_Window, _Now, _nShrinkAfterTicks);

		umint nWindow = _Window.m_nEffectiveBytes;
		umint nHeadroom = fg_Max(2 * _Window.m_nLargestSendBytes, _Window.m_nStartBytes / 4);

		// Without a release that met a low occupancy lately the honest latency is unknown:
		// the target then decays a quarter per shrink period, and the smaller window is
		// itself what lets samples flow again and stop the decay where they say
		umint nTarget;
		if (nLagTicks)
		{
			// Formed in 64 bits, and saturated: a rate times a lag that a 64 bit product cannot
			// hold means the window's maximum, as does a product at or past it, so neither the
			// multiplication nor the headroom added below can wrap
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
			// Doubled in 64 bits: the window may already be past half of umint. The configured
			// ceiling holds whichever branch formed the target; the headroom of the unknown-lag
			// branch follows the producer's gather size, not the window
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

	void ICIoLoop::f_SetSendWindow(CIoLoopRegistration *_pRegistration, umint _nBytes)
	{
	}

	bool ICIoLoop::f_IsSendWindowFull(CIoLoopRegistration *, umint, umint)
	{
		return false;
	}
	bool ICIoLoop::f_AdoptHandle(CIoLoopHandle, int &)
	{
		return true;
	}
}
