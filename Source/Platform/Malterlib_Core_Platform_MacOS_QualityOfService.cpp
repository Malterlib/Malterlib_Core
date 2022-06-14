// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_Platform_MacOS_QualityOfService.h"

namespace NMib
{
	namespace NPlatform
	{
		qos_class_t fg_PriorityToQualityOfService(EExecutionPriority _Priority, int &o_RelativePriority)
		{
			auto fGetRelative = [&](int32 _Start, int32 _End)
				{
					auto RelativeRange = (-QOS_MIN_RELATIVE_PRIORITY) + 1;
					return fg_Clamp(((_Priority - _End) * RelativeRange) / (_End - _Start), QOS_MIN_RELATIVE_PRIORITY, 0);
				}
			;

			if (_Priority >= EExecutionPriority_High)
			{
				o_RelativePriority = fGetRelative(EExecutionPriority_High, EExecutionPriority_Highest);
				return QOS_CLASS_USER_INTERACTIVE;
			}
			else if (_Priority >= EExecutionPriority_AboveNormal)
			{
				o_RelativePriority = fGetRelative(EExecutionPriority_AboveNormal, EExecutionPriority_High);
				return QOS_CLASS_USER_INITIATED;
			}
			else if (_Priority > EExecutionPriority_BelowNormal)
			{
				o_RelativePriority = fGetRelative(EExecutionPriority_BelowNormal, EExecutionPriority_AboveNormal);
				return QOS_CLASS_DEFAULT;
			}
			else if (_Priority > EExecutionPriority_Low)
			{
				o_RelativePriority = fGetRelative(EExecutionPriority_Low, EExecutionPriority_BelowNormal);
				return QOS_CLASS_UTILITY;
			}
			else
			{
				o_RelativePriority = fGetRelative(0, EExecutionPriority_Low);
				return QOS_CLASS_BACKGROUND;
			}
		}
	}
}

