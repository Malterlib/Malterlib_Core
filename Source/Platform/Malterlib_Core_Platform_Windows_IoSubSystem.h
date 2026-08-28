// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "../Malterlib_Core_IoSubSystem.h"

#include "Malterlib_Core_Platform_Windows_IoLoop.h"

#if DMibConfig_IoDebug_Enable
struct CIocpTraceEntry
{
	int64 m_Ticks;
	char const *m_pWhat;
	void const *m_pToken;
	NMib::NSys::CIoLoopHandle m_Handle;
	uint32 m_Value;
	uint32 m_ThreadId;
};

constexpr umint gc_IocpTraceRingEntries = 1 << 16;
#endif

#if DMibConfig_IoDebug_Enable
struct CIocpStats
{
	NMib::NAtomic::TCAtomic<uint64> m_nRecvSegments = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nRecvBytes = 0;
	NMib::NAtomic::TCAtomic<uint64> m_RecvSizeBuckets[33] = {};
	NMib::NAtomic::TCAtomic<uint64> m_nRecvBufferAllocs = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nRecvBufferReuses = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nRecvPosts = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nRecvInline = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nStreamParks = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nStreamResumes = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nRecvErrors = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendOps = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendInline = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendDeferred = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendPendingAtIssue = 0; // Distinguishes kernel-pending sends from synchronous acceptance.
	NMib::NAtomic::TCAtomic<uint64> m_nSendMaxInFlight = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendMaxBytesInFlight = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendShort = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendBytesRequested = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendBytesSent = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendErrors = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendIdleGaps = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendIdleNs = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendSubmitLagNs = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendSubmitLagOps = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendPacketLagSyncNs = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendPacketLagSyncOps = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendPacketLagPendingNs = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSendPacketLagPendingOps = 0;
	NMib::NAtomic::TCAtomic<uint64> m_SendSizeBuckets[33] = {};
	NMib::NAtomic::TCAtomic<uint64> m_nPollArms = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nPollCancels = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nPollEvents = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nWakePosts = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nWaits = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nPackets = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nRegistrations = 0;
	NMib::NAtomic::TCAtomic<uint64> m_nSkipSuccessSockets = 0;
};
#endif

// Reads platform overrides once; non-debug accessors return compile-time defaults.
struct CIoSubSystem_Windows : NMib::NSys::CIoSubSystem
{
	CIoSubSystem_Windows();
	~CIoSubSystem_Windows() override;

	inline bool f_CompletionEnabled() const;
	inline bool f_SkipSuccessEnabled() const;
	inline bool f_LoopbackFastPathEnabled() const;
	inline bool f_DirectSendEnabled() const;
	inline umint f_SendDepth() const;
	inline umint f_RecvDepth() const;
	inline umint f_RecvBufferBytesOverride() const;

#if DMibConfig_IoDebug_Enable
	inline bool f_TraceEnabled() const;
	void f_Trace(char const *_pWhat, void const *_pToken, NMib::NSys::CIoLoopHandle _Handle, uint32 _Value);

	bool m_bCompletionEnabled = true;
	bool m_bSkipSuccessEnabled = true;
	bool m_bLoopbackFastPathEnabled = true;
	bool m_bDirectSendEnabled = true;
	int m_TraceMode = 0;
	umint m_nSendDepth = gc_IocpDefaultSendDepth;
	umint m_nRecvDepth = gc_IocpDefaultRecvDepth;
	umint m_nRecvBufferBytesOverride = 0;
	CIocpTraceEntry *m_pTraceRing = nullptr;
	NMib::NAtomic::TCAtomic<uint64> m_nTraceNext{0};

	CIocpStats m_IocpStats;
#endif
};

CIoSubSystem_Windows &fg_IoSubSystem_Windows();

#if DMibConfig_IoDebug_Enable
void fg_DumpIocpStats(NMib::NSys::CIoSubSystem &_Io);
#endif

#include "Malterlib_Core_Platform_Windows_IoSubSystem.hpp"
