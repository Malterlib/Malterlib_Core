// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Core_Platform_POSIX_IoLoop.h"

#include <sys/epoll.h>

// Poll readiness bits to the loop's normalized event vocabulary; poll and epoll masks are the
// same values, so one translation serves epoll events, ring poll completions and probe revents
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

// Diagnostic trace for the uring bring-up, enabled with MalterlibUringTrace=1: close-class events
// and registration lifecycle only, so the volume stays too low to shift timing. In builds without
// the io debugging overrides every trace site is compiled out with the declarations
#if DMibConfig_IoDebug_Enable
bool fg_UringTraceEnabled();
void fg_UringTrace(char const *_pWhat, void const *_pToken, int _Fd, uint32 _Value);
#endif
