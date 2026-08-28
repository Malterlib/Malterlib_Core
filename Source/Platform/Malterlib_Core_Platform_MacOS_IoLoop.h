// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Core_Platform_POSIX_IoLoop.h"

struct CKQueueRegistration : public NMib::NSys::CIoLoopRegistration
{
	bool m_bAddFailed = false; // Suppress the applied notification after a rejected add.
};

// Removals apply during change processing, so one poll completes the shutdown drain.
struct CIoLoop_KQueue : public CIoLoop_POSIXBase
{
	CIoLoop_KQueue();
	~CIoLoop_KQueue() override;

private:
	umint fp_Iterate(bool _bBlock) override;
	auto fp_CreateRegistration() -> NMib::NSys::CIoLoopRegistration * override;

	int mp_KQueue = -1;
};
