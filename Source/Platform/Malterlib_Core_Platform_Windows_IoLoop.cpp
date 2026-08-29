// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "../Malterlib_Core_IoLoop_Internal.h"

// Placeholder until the IOCP backend lands: no loop to hand out, so io objects keep using the
// platform's shared poller
NMib::NSys::ICIoLoop *fg_CreatePlatformIoLoop()
{
	return nullptr;
}
