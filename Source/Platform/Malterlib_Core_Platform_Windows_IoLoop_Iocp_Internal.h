// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Core_Platform_Windows_IoSubSystem.h"

// Completion keys can outlive re-registration; resolve registrations through the packet's operation instead.
constexpr ULONG_PTR gc_IocpKey_Wake = 1;
constexpr ULONG_PTR gc_IocpKey_Afd = 2;

constexpr umint gc_IocpAfdGroupSize = 32; // Bound AFD's per-handle poll-list walk by distributing registrations across handles.

constexpr umint gc_IocpDequeueBatch = 64;

constexpr umint gc_IocpMaxInlinePerPass = 256; // Bound synchronous completion drains so the hosting queue cannot starve.
constexpr umint gc_IocpMaxRecvReportsPerCall = 64; // Bound one registration's drain even when receives keep completing inline.

constexpr umint gc_IocpReceiveSliceBytes = 256 * 1024; // Balance posted receive capacity against recycler cache footprint.

constexpr umint gc_IocpRecyclerMaxFreeBytes = 8 * 1024 * 1024; // Retain message-sized return bursts instead of sending them back to the allocator.

struct CIocpAfdGroup
{
	HANDLE m_hAfd = nullptr;
	umint m_nRegistrations = 0;
};

// Cross-thread control/submission messages applied by the loop before its next park.
struct CIocpPendingOp
{
	CIocpRegistration *m_pRegistration = nullptr;
	CIocpSendOp *m_pSendOp = nullptr;
	NMib::NSys::FIoStreamSink m_fSink;
	NMib::NStorage::TCSharedPointer<NMib::NSys::CIoStreamBackpressure> m_pBackpressure;
	umint m_nBytes = 0;
	bool m_bStreamStart = false;
	bool m_bStreamResume = false;
	bool m_bSendWindow = false;
};

#if DMibConfig_IoDebug_Enable
uint64 fg_IocpStatsNow();
#endif
