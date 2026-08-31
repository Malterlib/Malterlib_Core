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

	void ICThreadIoLoop::f_AbandonPendingTeardown()
	{
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

	bool ICIoLoop::f_SubmitSendVectored(CIoLoopRegistration *_pRegistration, CIoSpan const *_pSpans, umint _nSpans, FIoCompletion &&_fOnComplete, FIoBufferReleased &&_fOnBufferReleased)
	{
		return false;
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

	void fg_ConsiderIoSendWindowGrowth(CIoSendWindow &_Window, umint _nBandwidthDelayBytes, bool _bAppLimited, uint64 _Now, umint _nShrinkAfterTicks)
	{
		umint nWindow = _Window.m_nEffectiveBytes;
		umint nHeadroom = fg_Max(2 * _Window.m_nLargestSendBytes, _Window.m_nStartBytes / 4);
		umint nTarget = fg_Min(_nBandwidthDelayBytes + _nBandwidthDelayBytes / 4 + nHeadroom, _Window.m_nMaxBytes);
		if (nTarget > nWindow)
		{
			_Window.m_nEffectiveBytes = fg_Min(nTarget, nWindow * 2);
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
