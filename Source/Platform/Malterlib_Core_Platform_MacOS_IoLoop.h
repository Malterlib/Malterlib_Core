// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Core_Platform_POSIX_IoLoop.h"

// The kqueue loop's registration, carrying what the receipts of one pass said about it
struct CKQueueRegistration : public NMib::NSys::CIoLoopRegistration
{
	// The add in the changelist was refused, so the applied notification is withdrawn
	bool m_bAddFailed = false;
};

// The kqueue readiness backend, the platform's only one. Removals apply inline during change
// processing, so the base drain (one poll) is a complete drain
struct CIoLoop_KQueue : public CIoLoop_POSIXBase
{
	CIoLoop_KQueue();
	~CIoLoop_KQueue() override;

private:
	umint fp_Iterate(bool _bBlock) override;
	auto fp_CreateRegistration() -> NMib::NSys::CIoLoopRegistration * override;

	int mp_KQueue = -1;
};
