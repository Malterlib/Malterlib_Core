// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <sys/syscall.h>
#include <linux/sched.h>

// What a thread's execution priority means to the Linux scheduler beyond its policy and nice
// value, in the spirit of the quality of service classes on macOS: a utilization clamp. On a
// kernel that schedules by capacity — big.LITTLE, and anything whose frequency is chosen by
// schedutil — the clamp decides the core size and the clock a thread is given, ahead of what the
// scheduler has measured of it, which a thread that lives a few milliseconds never gets the time
// to establish. A kernel that reports every core at full capacity, Intel's hybrid parts among
// them, takes the clamp and ignores it for placement. The clamp is per thread and inherited by
// the threads a thread creates, so every thread applies its own on start, which also undoes what
// it inherited.
//
// The bands follow the macOS classes: High and above are interactive and get the whole machine,
// AboveNormal is user initiated and gets a big core, Normal is the default and is placed by its
// measured demand, BelowNormal is utility and Low and below are background, held to a small core
// and a low clock

namespace
{
	#ifndef SCHED_FLAG_KEEP_ALL
	#	define SCHED_FLAG_KEEP_ALL 0x18
	#endif
	#ifndef SCHED_FLAG_UTIL_CLAMP_MIN
	#	define SCHED_FLAG_UTIL_CLAMP_MIN 0x20
	#endif
	#ifndef SCHED_FLAG_UTIL_CLAMP_MAX
	#	define SCHED_FLAG_UTIL_CLAMP_MAX 0x40
	#endif

	// sched_setattr's argument, laid out as the kernel defines it. Declared here rather than
	// taken from linux/sched/types.h, whose struct newer glibc versions declare as well
	struct CLinuxSchedAttr
	{
		uint32 m_Size;
		uint32 m_Policy;
		uint64 m_Flags;
		int32 m_Nice;
		uint32 m_Priority;
		uint64 m_Runtime;
		uint64 m_Deadline;
		uint64 m_Period;
		uint32 m_UtilMin;
		uint32 m_UtilMax;
	};

	constexpr uint32 gc_LinuxUtilMax = 1024;

	// Applies the calling thread's band. Best effort: a kernel without utilization clamping
	// refuses the attribute and the thread keeps its policy, which was set separately
	void fg_Linux_ApplyThreadScheduling(EExecutionPriority _Priority)
	{
		if (_Priority == EExecutionPriority_Default)
			_Priority = EExecutionPriority_Normal;

		uint32 UtilMin = 0;
		uint32 UtilMax = gc_LinuxUtilMax;

		if (_Priority >= EExecutionPriority_High)
			UtilMin = gc_LinuxUtilMax;
		else if (_Priority > EExecutionPriority_Normal)
			UtilMin = (gc_LinuxUtilMax * 3) / 4;
		else if (_Priority > EExecutionPriority_BelowNormal)
			; // The default: placed and clocked by what the thread is measured to need
		else if (_Priority > EExecutionPriority_Low)
			UtilMax = gc_LinuxUtilMax / 2;
		else
			UtilMax = gc_LinuxUtilMax / 4;

		CLinuxSchedAttr Attr;
		fg_MemClear(Attr);
		Attr.m_Size = sizeof(Attr);
		Attr.m_Flags = SCHED_FLAG_KEEP_ALL | SCHED_FLAG_UTIL_CLAMP_MIN | SCHED_FLAG_UTIL_CLAMP_MAX;
		Attr.m_UtilMin = UtilMin;
		Attr.m_UtilMax = UtilMax;

		syscall(__NR_sched_setattr, 0, &Attr, 0);
	}
}
