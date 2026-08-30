// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NSys
{
	inline bool fg_ResolveIoKnob(EIoKnob _Knob, bool _bCompiledDefault)
	{
		if (_Knob == EIoKnob::mc_Default)
			return _bCompiledDefault;

		return _Knob == EIoKnob::mc_On;
	}

	inline bool CIoSubSystem::f_StatsEnabled() const
	{
#if DMibConfig_IoDebug_Enable
		return mp_bStatsEnabled;
#else
		return false;
#endif
	}

	inline umint CIoSubSystem::f_SocketBufferBytesOverride() const
	{
#if DMibConfig_IoDebug_Enable
		return mp_nSocketBufferBytesOverride;
#else
		return 0;
#endif
	}

	inline umint CIoSubSystem::f_SocketSendBufferBytesOverride() const
	{
#if DMibConfig_IoDebug_Enable
		return mp_nSocketSendBufferBytesOverride;
#else
		return umint(-1);
#endif
	}

	inline bool CIoSubSystem::f_CompletionLocalForced() const
	{
	#if DMibConfig_IoDebug_Enable
		return mp_bCompletionLocalForced;
	#else
		return false;
	#endif
	}

	inline bool CIoSubSystem::f_SendWindowBuffersEnabled() const
	{
#if DMibConfig_IoDebug_Enable
		return mp_bSendWindowBuffers;
#else
		return true;
#endif
	}

	inline umint CIoSubSystem::f_ReceiveWindowBytesOverride() const
	{
#if DMibConfig_IoDebug_Enable
		return mp_nReceiveWindowBytesOverride;
#else
		return 0;
#endif
	}

	inline EIoKnob CIoSubSystem::f_SslSendBatching() const
	{
#if DMibConfig_IoDebug_Enable
		return mp_SslSendBatching;
#else
		return EIoKnob::mc_Default;
#endif
	}

	inline EIoKnob CIoSubSystem::f_SslZeroCopy() const
	{
#if DMibConfig_IoDebug_Enable
		return mp_SslZeroCopy;
#else
		return EIoKnob::mc_Default;
#endif
	}

	inline EIoKnob CIoSubSystem::f_SslCompletionIoSend() const
	{
#if DMibConfig_IoDebug_Enable
		return mp_SslCompletionIoSend;
#else
		return EIoKnob::mc_Default;
#endif
	}

	inline EIoKnob CIoSubSystem::f_SslCompletionIoReceive() const
	{
#if DMibConfig_IoDebug_Enable
		return mp_SslCompletionIoReceive;
#else
		return EIoKnob::mc_Default;
#endif
	}

	inline EIoKnob CIoSubSystem::f_SslSealAhead() const
	{
#if DMibConfig_IoDebug_Enable
		return mp_SslSealAhead;
#else
		return EIoKnob::mc_Default;
#endif
	}

	inline umint CIoSubSystem::f_WebSocketFrameAhead() const
	{
#if DMibConfig_IoDebug_Enable
		return mp_nWebSocketFrameAhead;
#else
		return 0;
#endif
	}
}
