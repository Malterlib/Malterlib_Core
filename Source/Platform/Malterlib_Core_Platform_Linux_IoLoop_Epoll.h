// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Core_Platform_Linux_IoLoop.h"

// The epoll readiness backend: the fallback where the io_uring probe does not pass. Removals
// apply inline during change processing, so the base drain (one poll) is a complete drain
struct CIoLoop_Epoll : public CIoLoop_POSIXBase
{
	CIoLoop_Epoll();
	~CIoLoop_Epoll() override;

private:
	umint fp_Iterate(bool _bBlock) override;

	int mp_EpollFd = -1;
};
