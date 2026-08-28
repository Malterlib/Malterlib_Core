// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Core_SubSystem.h"

#include <Mib/Container/Set>

namespace NMib::NSys
{
	// mc_Default is resolved by the consuming module, which owns the compile-time default.
	enum class EIoKnob : uint8
	{
		mc_Default
		, mc_Off
		, mc_On
	};

	inline bool fg_ResolveIoKnob(EIoKnob _Knob, bool _bCompiledDefault);

#if DMibConfig_IoDebug_Enable
	struct CSocketIoStats
	{
		NAtomic::TCAtomic<uint64> m_nRecvCalls = 0;
		NAtomic::TCAtomic<uint64> m_nRecvBytes = 0;
		NAtomic::TCAtomic<uint64> m_nRecvWouldBlock = 0;
		NAtomic::TCAtomic<uint64> m_nRecvShort = 0;
		NAtomic::TCAtomic<uint64> m_nRecvEndOfStream = 0;
		NAtomic::TCAtomic<uint64> m_RecvSizeBuckets[33] = {};
		NAtomic::TCAtomic<uint64> m_nSendCalls = 0;
		NAtomic::TCAtomic<uint64> m_nSendBytesRequested = 0;
		NAtomic::TCAtomic<uint64> m_nSendBytesSent = 0;
		NAtomic::TCAtomic<uint64> m_nSendWouldBlock = 0;
		NAtomic::TCAtomic<uint64> m_nSendShort = 0;
		NAtomic::TCAtomic<uint64> m_SendSizeBuckets[33] = {};
		NAtomic::TCAtomic<uint64> m_nReadinessArmsRead = 0;
		NAtomic::TCAtomic<uint64> m_nReadinessArmsWrite = 0;
		NAtomic::TCAtomic<uint64> m_nReadinessReportsRead = 0;
		NAtomic::TCAtomic<uint64> m_nReadinessReportsWrite = 0;
	};

	struct CNetIoStats
	{
		NAtomic::TCAtomic<uint64> m_nSendReadinessCalls = 0;
		NAtomic::TCAtomic<uint64> m_nSendReadinessBytes = 0;
		NAtomic::TCAtomic<uint64> m_nRecvReadinessCalls = 0;
		NAtomic::TCAtomic<uint64> m_nRecvReadinessBytes = 0;
		NAtomic::TCAtomic<uint64> m_nSendSubmits = 0;
		NAtomic::TCAtomic<uint64> m_nSendBlocked = 0;
		NAtomic::TCAtomic<uint64> m_nSendMaxOutstanding = 0; // Includes the send currently being submitted.
		NAtomic::TCAtomic<uint64> m_nSendSyncParked = 0;
		NAtomic::TCAtomic<uint64> m_nSendContinuations = 0;
		NAtomic::TCAtomic<uint64> m_nRecvSharedDeliveries = 0;
		NAtomic::TCAtomic<uint64> m_nRecvSharedBytes = 0;
		NAtomic::TCAtomic<uint64> m_nRecvCopyDeliveries = 0;
		NAtomic::TCAtomic<uint64> m_nRecvCopyBytes = 0;
		NAtomic::TCAtomic<uint64> m_nSslSegments = 0;
		NAtomic::TCAtomic<uint64> m_nSslNoProgress = 0;
		NAtomic::TCAtomic<uint64> m_nSslCompacts = 0;
		NAtomic::TCAtomic<uint64> m_nPumpSubmits = 0;
		NAtomic::TCAtomic<uint64> m_nPumpInFlight = 0;
		NAtomic::TCAtomic<uint64> m_nPumpBeginRefused = 0;
		NAtomic::TCAtomic<uint64> m_nPumpKernelRefused = 0;
		NAtomic::TCAtomic<uint64> m_LastPumpPending = 0;
		NAtomic::TCAtomic<uint64> m_LastPumpPinned = 0;
		NAtomic::TCAtomic<uint64> m_LastPumpCanBegin = 0;
		NAtomic::TCAtomic<uint64> m_LastPumpOpsInUse = 0;
		NAtomic::TCAtomic<uint64> m_LastPumpOpsUnresolved = 0;
		NAtomic::TCAtomic<uint64> m_nSslMaxPinned = 0;
		NAtomic::TCAtomic<uint64> m_nSslMaxPinnedBytes = 0;
		NAtomic::TCAtomic<uint64> m_nSslWindowMax = 0;
		NAtomic::TCAtomic<uint64> m_nSslWindowBandwidthDelay = 0;
		NAtomic::TCAtomic<uint64> m_nSslWindowQueries = 0;
	};
#endif

	struct CIoSubSystem;

	// Invoked by the subsystem destructor on the subsystem where it was registered.
	using FIoStatsDump = void (*)(CIoSubSystem &);

	struct CSort_StatsDump
	{
		auto operator()(FIoStatsDump _fLeft, FIoStatsDump _fRight) const
		{
			return (umint)_fLeft <=> (umint)_fRight;
		}
	};

	// Reads environment overrides once at construction; non-debug accessors return compile-time defaults.
	// Statistics follow subsystem teardown ordering.
	struct CIoSubSystem : NMib::CSubSystem
	{
		CIoSubSystem();
		~CIoSubSystem() override;
		void f_ExitModule() override;

		inline bool f_StatsEnabled() const;

		inline umint f_SocketBufferBytesOverride() const;
		inline umint f_SocketSendBufferBytesOverride() const;
		inline bool f_SendWindowBuffersEnabled() const;
		inline umint f_ReceiveWindowBytesOverride() const;

		inline EIoKnob f_SslSendBatching() const;
		inline EIoKnob f_SslZeroCopy() const;
		inline EIoKnob f_SslCompletionIoSend() const;
		inline EIoKnob f_SslCompletionIoReceive() const;
		inline EIoKnob f_SslSealAhead() const;

		inline bool f_CompletionLocalForced() const;

		inline umint f_WebSocketFrameAhead() const;

		void f_RegisterStatsDump(FIoStatsDump _fDump);

		umint m_nWindowQueryIntervalTicks = 0;
		umint m_nWindowShrinkAfterTicks = 0;
		umint m_nTicksPerSecond = 0;

#if DMibConfig_IoDebug_Enable
		CSocketIoStats m_SocketIoStats;
		CNetIoStats m_NetIoStats;
#endif

		void f_DumpStats();

	protected:
#if DMibConfig_IoDebug_Enable
		static bool fsp_EnvFlag(char const *_pName, bool _bDefault);
		static umint fsp_EnvCount(char const *_pName, umint _Default, umint _Min, umint _Max);
		static EIoKnob fsp_EnvKnob(char const *_pName);

		bool mp_bStatsEnabled = false;
		bool mp_bSendWindowBuffers = true;
		bool mp_bCompletionLocalForced = false;
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

		NThread::CLowLevelLockAggregate mp_StatsDumpLock = {DAggregateInit};
		NContainer::TCSet<FIoStatsDump, CSort_StatsDump> mp_StatsDumps;
		bool mp_bStatsDumped = false;
	};

	CIoSubSystem &fg_IoSubSystem();
}

#if DMibConfig_IoDebug_Enable
void fg_DumpSocketIoStats(NMib::NSys::CIoSubSystem &_Io);
void fg_DumpNetIoStats(NMib::NSys::CIoSubSystem &_Io);
#endif

#include "Malterlib_Core_IoSubSystem.hpp"
