// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <sys/qos.h>

namespace NMib
{
	namespace NPlatform
	{
		qos_class_t fg_PriorityToQualityOfService(EExecutionPriority _Priority, int &o_RelativePriority);
	}
}
