// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "../Malterlib_Core_IoLoop_Internal.h"

// Shared base of the POSIX io loop backends: the wake pipe on top of the platform-neutral change
// queue and wake protocol. The pipe's read end is registered as the loop's internal entry, so a
// wake surfaces as readiness the backend's own wait reports
struct CIoLoop_POSIXBase : public CIoLoop_Base
{
protected:
	// Creates the wake pipe and queues its registration; a backend constructor that fails to
	// create its kernel object simply throws — this base is then destroyed and closes the pipe
	CIoLoop_POSIXBase();
	~CIoLoop_POSIXBase() override;

	void fp_WakeKernel() override;

	int mp_ReadWritePipe[2];		// Used to wake the parked loop up
};
