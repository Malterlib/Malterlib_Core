// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <sys/syscall.h>
#include <linux/sched.h>

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

	// Kernel sched_attr layout, declared locally to avoid libc/header definition conflicts.
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

	// Best-effort utilization bounds for the calling thread; unsupported kernels retain the existing policy.
	// Apply at thread start to replace inherited bounds.
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
		else if (_Priority <= EExecutionPriority_Low)
			UtilMax = gc_LinuxUtilMax / 4;
		else if (_Priority <= EExecutionPriority_BelowNormal)
			UtilMax = gc_LinuxUtilMax / 2;

		CLinuxSchedAttr Attr;
		fg_MemClear(Attr);
		Attr.m_Size = sizeof(Attr);
		Attr.m_Flags = SCHED_FLAG_KEEP_ALL | SCHED_FLAG_UTIL_CLAMP_MIN | SCHED_FLAG_UTIL_CLAMP_MAX;
		Attr.m_UtilMin = UtilMin;
		Attr.m_UtilMax = UtilMax;

		syscall(__NR_sched_setattr, 0, &Attr, 0);
	}
}
