// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Core_SubSystem.h"

#include <Mib/Container/Set>

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

	// Cumulative readiness-path socket statistics, reported at process exit when MalterlibIoStats=1:
	// the readiness counterpart of the IOCP completion counters, so the two transfer paths can be
	// compared on the same terms — how many transfer calls it took, how big they were, how often
	// they met an empty queue or a full buffer, and how many readiness arms and reports drove them.
	// Relaxed atomics: every loop and every calling thread writes them, exactness per counter is not
	// the point. Everything here and every recording site exists only in builds carrying the io
	// debugging overrides
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

	// Cumulative socket-level io statistics, reported at process exit when MalterlibIoStats=1;
	// what the loop-level numbers cannot see — where the actor's send pipeline stalled, how the
	// deliveries reached the consumer on both transfer paths, and how often the record layer
	// made no progress. Everything here and every recording site exists only in builds carrying
	// the io debugging overrides
	struct CNetIoStats
	{
		NAtomic::TCAtomic<uint64> m_nSendReadinessCalls = 0;
		NAtomic::TCAtomic<uint64> m_nSendReadinessBytes = 0;
		NAtomic::TCAtomic<uint64> m_nRecvReadinessCalls = 0;
		NAtomic::TCAtomic<uint64> m_nRecvReadinessBytes = 0;
		NAtomic::TCAtomic<uint64> m_nSendSubmits = 0;
		NAtomic::TCAtomic<uint64> m_nSendBlocked = 0;
		// The most completion sends any one socket had handed to its loop at once, including the
		// one being submitted: how much of the send depth a workload actually reaches
		NAtomic::TCAtomic<uint64> m_nSendMaxOutstanding = 0;
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
		// The most generations, and bytes, one SSL connection had pinned by sends awaiting release
		NAtomic::TCAtomic<uint64> m_nSslMaxPinned = 0;
		NAtomic::TCAtomic<uint64> m_nSslMaxPinnedBytes = 0;
		// The widest cap an SSL connection grew to, the last bandwidth-delay product read, and how often the path was asked
		NAtomic::TCAtomic<uint64> m_nSslWindowMax = 0;
		NAtomic::TCAtomic<uint64> m_nSslWindowBandwidthDelay = 0;
		NAtomic::TCAtomic<uint64> m_nSslWindowQueries = 0;
	};
#endif

	struct CIoSubSystem;

	// A statistics report the destructor runs, handed the subsystem the run registered it on
	using FIoStatsDump = void (*)(CIoSubSystem &);

	// Function pointers have no ordering of their own, so the report set compares their addresses
	struct CSort_StatsDump
	{
		auto operator()(FIoStatsDump _fLeft, FIoStatsDump _fRight) const
		{
			return (umint)_fLeft <=> (umint)_fRight;
		}
	};

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

		// MalterlibIoUringCompletion=1: force completion transfers onto local peers too, so the
		// local paths stay measurable; unset leaves them on readiness
		inline bool f_CompletionLocalForced() const;

		// MalterlibWebSocketFrameAhead: how many frame batches the websocket actor frames ahead
		// of its in-flight sends; 0 is the actor's default of one
		inline umint f_WebSocketFrameAhead() const;

		// Adds a statistics report to what the end of the run prints. An area registers its
		// report when it starts collecting, so a run only prints the areas it touched. Thread
		// safe; a report registered twice prints once
		void f_RegisterStatsDump(FIoStatsDump _fDump);

		// Prints the registered reports once. Runs from the destructor and from an atexit
		// handler the first registration arms: Windows release exits skip the subsystem
		// teardown, so only the handler reports there
		void f_DumpStats();

		// The send window asks pace their path queries by these; raw timer ticks, converted once
		umint m_nWindowQueryIntervalTicks = 0;
		umint m_nWindowShrinkAfterTicks = 0;
		umint m_nTicksPerSecond = 0;

#if DMibConfig_IoDebug_Enable
		CSocketIoStats m_SocketIoStats;
		CNetIoStats m_NetIoStats;
#endif

	protected:
		// The environment readers the constructors decide the knobs with, io-debug builds only
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

	// The platform's io subsystem seen as the shared base, created on first use. Each platform
	// defines this next to its own subsystem instance and derived-type getter
	CIoSubSystem &fg_IoSubSystem();
}

#if DMibConfig_IoDebug_Enable
// The socket statistics report, registered by the base constructor when MalterlibIoStats=1
void fg_DumpSocketIoStats(NMib::NSys::CIoSubSystem &_Io);

// The socket-actor level report, likewise registered by the base constructor; the counters are
// recorded by the Network module
void fg_DumpNetIoStats(NMib::NSys::CIoSubSystem &_Io);
#endif

#include "Malterlib_Core_IoSubSystem.hpp"
