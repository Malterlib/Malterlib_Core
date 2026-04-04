// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <sys/qos.h>

namespace NMib
{
	namespace NPlatform
	{
		qos_class_t fg_PriorityToQualityOfService(EExecutionPriority _Priority, int &o_RelativePriority);
	}
}
