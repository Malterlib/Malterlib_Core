// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "../Malterlib_Core_IoLoop_Internal.h"

// Wake-pipe readiness feeds the backend's own wait; the read end is an internal registration.
struct CIoLoop_POSIXBase : public CIoLoop_Base
{
protected:
	CIoLoop_POSIXBase();
	~CIoLoop_POSIXBase() override;

	void fp_WakeKernel() override;

	int mp_ReadWritePipe[2];		// Used to wake the parked loop up
};
