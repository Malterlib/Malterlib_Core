// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Core_Platform_POSIX_IoLoop.h"

#include <sys/epoll.h>

constexpr NMib::NSys::EIoLoopEvent fg_IoLoopEventsFromPollBits(uint32 _PollBits);
constexpr uint32 fg_PollInterestFromIoLoopMask(NMib::NSys::EIoLoopEvent _EventMask);

void fg_UringLogMemlockFallback();

#if DMibConfig_IoDebug_Enable
void fg_UringTrace(char const *_pWhat, void const *_pToken, NMib::NSys::CIoLoopHandle _Handle, uint32 _Value);
#endif

#include "Malterlib_Core_Platform_Linux_IoLoop.hpp"
