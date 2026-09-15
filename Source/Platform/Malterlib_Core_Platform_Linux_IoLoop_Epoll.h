// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Core_Platform_Linux_IoLoop.h"

struct CIoSubSystem_Linux;

struct CIoLoop_Epoll : public CIoLoop_POSIXBase
{
	CIoLoop_Epoll();
	~CIoLoop_Epoll() override;

private:
	umint fp_Iterate(bool _bBlock, fp64 _Timeout = -1.0) override;

#if DMibConfig_IoDebug_Enable
	CIoSubSystem_Linux *mp_pIo = nullptr;
#endif

	int mp_EpollFd = -1;
};
