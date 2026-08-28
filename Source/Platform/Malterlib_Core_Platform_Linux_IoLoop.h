// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Core_Platform_POSIX_IoLoop.h"

#include <sys/epoll.h>

// poll and epoll bits share values, allowing one translation for readiness events and ring completions.
constexpr NMib::NSys::EIoLoopEvent fg_IoLoopEventsFromPollBits(uint32 _PollBits)
{
	using NMib::NSys::EIoLoopEvent;

	EIoLoopEvent Events = EIoLoopEvent::mc_None;
	if (_PollBits & EPOLLIN)
		Events |= EIoLoopEvent::mc_Read;
	if (_PollBits & EPOLLOUT)
		Events |= EIoLoopEvent::mc_Write;
	if (_PollBits & EPOLLRDHUP)
		Events |= EIoLoopEvent::mc_ReadClosed;
	if (_PollBits & EPOLLHUP)
		Events |= EIoLoopEvent::mc_Hup;
	if (_PollBits & EPOLLERR)
		Events |= EIoLoopEvent::mc_Error;

	return Events;
}

constexpr uint32 fg_PollInterestFromIoLoopMask(NMib::NSys::EIoLoopEvent _EventMask)
{
	return
		(NMib::fg_IsSet(_EventMask, NMib::NSys::EIoLoopEvent::mc_Read) ? EPOLLIN : 0)
		| (NMib::fg_IsSet(_EventMask, NMib::NSys::EIoLoopEvent::mc_Write) ? EPOLLOUT : 0)
	;
}

void fg_UringLogMemlockFallback();

#if DMibConfig_IoDebug_Enable
void fg_UringTrace(char const *_pWhat, void const *_pToken, NMib::NSys::CIoLoopHandle _Handle, uint32 _Value);
#endif
