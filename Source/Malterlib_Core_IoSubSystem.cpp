// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

#include "Malterlib_Core_IoSubSystem.h"

namespace NMib::NSys
{
	CIoSubSystem::CIoSubSystem()
	{
#if DMibConfig_IoDebug_Enable
		mp_bStatsEnabled = fsp_EnvFlag("MalterlibIoStats", false);
		mp_bSendWindowBuffers = fsp_EnvFlag("MalterlibSendWindowBuffers", true);
		mp_bCompletionLocalForced = fsp_EnvFlag("MalterlibIoUringCompletion", false);
		mp_SslSendBatching = fsp_EnvKnob("MalterlibSSLSendBatching");
		mp_SslZeroCopy = fsp_EnvKnob("MalterlibSSLZeroCopy");
		// The direction specific name wins over the shared one, which is there so a recipe that
		// predates the split still sets both
		mp_SslCompletionIoSend = fsp_EnvKnob("MalterlibSSLCompletionIoSend");
		if (mp_SslCompletionIoSend == EIoKnob::mc_Default)
			mp_SslCompletionIoSend = fsp_EnvKnob("MalterlibSSLCompletionIo");
		mp_SslCompletionIoReceive = fsp_EnvKnob("MalterlibSSLCompletionIoReceive");
		if (mp_SslCompletionIoReceive == EIoKnob::mc_Default)
			mp_SslCompletionIoReceive = fsp_EnvKnob("MalterlibSSLCompletionIo");
		mp_SslSealAhead = fsp_EnvKnob("MalterlibSSLSealAhead");
		mp_nSocketBufferBytesOverride = fsp_EnvCount("MalterlibSocketBufferSize", 0, 1, umint(1) << 30);
		mp_nSocketSendBufferBytesOverride = fsp_EnvCount("MalterlibSocketSendBufferSize", umint(-1), 1, umint(1) << 30);
		// Zero is a meaningful value for the send buffer (no buffering at all), which the count
		// reader would take for unset
		if (fg_Process_GetEnvironmentVariable_NonProtected(NStr::CStrNonTracked("MalterlibSocketSendBufferSize")) == "0")
			mp_nSocketSendBufferBytesOverride = 0;
		mp_nReceiveWindowBytesOverride = fsp_EnvCount("MalterlibReceiveWindow", 0, 1, umint(1) << 40);
		mp_nWebSocketFrameAhead = fsp_EnvCount("MalterlibWebSocketFrameAhead", 0, 1, 64);

		if (mp_bStatsEnabled)
		{
			f_RegisterStatsDump(&fg_DumpSocketIoStats);
			f_RegisterStatsDump(&fg_DumpNetIoStats);
		}
#endif
	}

	CIoSubSystem::~CIoSubSystem()
	{
		for (auto fDump : mp_StatsDumps)
			fDump(*this);
	}

	void CIoSubSystem::f_RegisterStatsDump(FIoStatsDump _fDump)
	{
		DMibLock(mp_StatsDumpLock);
		if (!mp_StatsDumps.f_FindEqual(_fDump))
			mp_StatsDumps.f_Insert(_fDump);
	}

#if DMibConfig_IoDebug_Enable
	bool CIoSubSystem::fsp_EnvFlag(char const *_pName, bool _bDefault)
	{
		auto Setting = fg_Process_GetEnvironmentVariable_NonProtected(NStr::CStrNonTracked(_pName));
		if (Setting == "0")
			return false;
		if (Setting == "1")
			return true;

		return _bDefault;
	}

	umint CIoSubSystem::fsp_EnvCount(char const *_pName, umint _Default, umint _Min, umint _Max)
	{
		auto Setting = fg_Process_GetEnvironmentVariable_NonProtected(NStr::CStrNonTracked(_pName));
		umint nValue = Setting.f_ToIntExact(umint(0));
		if (!nValue)
			return _Default;

		return fg_Clamp(nValue, _Min, _Max);
	}

	EIoKnob CIoSubSystem::fsp_EnvKnob(char const *_pName)
	{
		auto Setting = fg_Process_GetEnvironmentVariable_NonProtected(NStr::CStrNonTracked(_pName));
		if (Setting == "0")
			return EIoKnob::mc_Off;
		if (Setting == "1")
			return EIoKnob::mc_On;

		return EIoKnob::mc_Default;
	}
#endif
}
