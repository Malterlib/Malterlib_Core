// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Core_SubSystem.h"

namespace NMib::NSys
{
	// An io knob the environment may override in io-debug builds. The module that owns the
	// compile time default resolves mc_Default against it, so this header does not need to see
	// that module's configuration macros
	enum class EIoKnob : uint8
	{
		mc_Default
		, mc_Off
		, mc_On
	};

	inline bool fg_ResolveIoKnob(EIoKnob _Knob, bool _bCompiledDefault);

	// The io configuration of the process, created on first use and destroyed with the other
	// subsystems. Every environment knob is read once, in the constructor; consumers keep a
	// pointer to the subsystem and read plain members through the accessors instead of asking
	// a global function per decision. Each platform derives its own subsystem carrying that
	// platform's knobs — fg_IoSubSystem answers with the platform one seen as this base, and
	// the platform getter answers with the derived type.
	//
	// Without the io debugging overrides the accessors answer their compile time defaults as
	// constants, so branches consulting them fold away and no member is even stored.
	//
	// The destructor prints the statistics reports the run registered (MalterlibIoStats=1),
	// which replaces registering them with atexit
	struct CIoSubSystem : NMib::CSubSystem
	{
		CIoSubSystem();
		~CIoSubSystem() override;

		// MalterlibIoStats=1: collect and, at destruction, print io statistics
		inline bool f_StatsEnabled() const;

		// MalterlibSocketBufferSize: SO_SNDBUF and SO_RCVBUF for every socket, a measurement aid;
		// 0 leaves the platform's policy
		inline umint f_SocketBufferBytesOverride() const;

		// MalterlibSocketSendBufferSize: SO_SNDBUF alone; ~umint(0) leaves the platform's policy,
		// 0 is meaningful (no send buffer at all)
		inline umint f_SocketSendBufferBytesOverride() const;

		// MalterlibSendWindowBuffers=0: leave the kernel socket buffers alone where a send window
		// would otherwise size them, for measuring what the sizing is worth
		inline bool f_SendWindowBuffersEnabled() const;

		// MalterlibReceiveWindow: the receive stream flow-control window in bytes; 0 leaves the
		// size derived from the connection's buffers
		inline umint f_ReceiveWindowBytesOverride() const;

		// MalterlibSSLSendBatching, MalterlibSSLZeroCopy, MalterlibSSLCompletionIoSend,
		// MalterlibSSLCompletionIoReceive, MalterlibSSLSealAhead: the SSL transport's measurement
		// knobs; the transport resolves them against its compiled defaults with fg_ResolveIoKnob
		inline EIoKnob f_SslSendBatching() const;
		inline EIoKnob f_SslZeroCopy() const;
		inline EIoKnob f_SslCompletionIoSend() const;
		inline EIoKnob f_SslCompletionIoReceive() const;
		inline EIoKnob f_SslSealAhead() const;

		// MalterlibWebSocketFrameAhead: how many frame batches the websocket actor frames ahead
		// of its in-flight sends; 0 is the actor's default of one
		inline umint f_WebSocketFrameAhead() const;

		// Adds a statistics report to what the destructor prints, in registration order. An area
		// registers its report when it starts collecting, so a run only prints the areas it
		// touched. Thread safe; a report registered twice prints once
		void f_RegisterStatsDump(void (*_fDump)());

	protected:
		// The environment readers the constructors decide the knobs with, io-debug builds only
#if DMibConfig_IoDebug_Enable
		static bool fsp_EnvFlag(char const *_pName, bool _bDefault);
		static umint fsp_EnvCount(char const *_pName, umint _Default, umint _Min, umint _Max);
		static EIoKnob fsp_EnvKnob(char const *_pName);

		bool mp_bStatsEnabled = false;
		bool mp_bSendWindowBuffers = true;
		EIoKnob mp_SslSendBatching = EIoKnob::mc_Default;
		EIoKnob mp_SslZeroCopy = EIoKnob::mc_Default;
		EIoKnob mp_SslCompletionIoSend = EIoKnob::mc_Default;
		EIoKnob mp_SslCompletionIoReceive = EIoKnob::mc_Default;
		EIoKnob mp_SslSealAhead = EIoKnob::mc_Default;
		umint mp_nSocketBufferBytesOverride = 0;
		umint mp_nSocketSendBufferBytesOverride = umint(-1);
		umint mp_nReceiveWindowBytesOverride = 0;
		umint mp_nWebSocketFrameAhead = 0;
#endif

		static constexpr umint mcp_nMaxStatsDumps = 8;
		NThread::CLowLevelLockAggregate mp_StatsDumpLock = {DAggregateInit};
		void (*mp_fStatsDumps[mcp_nMaxStatsDumps])() = {};
		umint mp_nStatsDumps = 0;
	};

	// The platform's io subsystem seen as the shared base, created on first use. Each platform
	// defines this next to its own subsystem instance and derived-type getter
	CIoSubSystem &fg_IoSubSystem();
}

#include "Malterlib_Core_IoSubSystem.hpp"
