// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_Platform_MacOS_QualityOfService.h"

namespace NMib
{
	namespace NPlatform
	{
		namespace
		{
			struct CQualityOfSerivceSettings
			{
				qos_class_t m_Class;
				int m_RelativePriority;
			};

			constexpr CQualityOfSerivceSettings fg_PriorityToQualityOfServiceConstexpr(EExecutionPriority _Priority)
			{
				auto fGetRelative = [&](int32 _Start, int32 _End)
					{
						auto RelativeRange = (-QOS_MIN_RELATIVE_PRIORITY) + 1;
						return fg_Clamp(((_Priority - _End) * RelativeRange) / (_End - _Start), QOS_MIN_RELATIVE_PRIORITY, 0);
					}
				;

				if (_Priority >= EExecutionPriority_High)
				{

					return
						{
							.m_Class = QOS_CLASS_USER_INTERACTIVE
							, .m_RelativePriority = fGetRelative(EExecutionPriority_High, EExecutionPriority_Highest)
						}
					;
				}
				else if (_Priority > EExecutionPriority_Normal)
				{
					return
						{
							.m_Class = QOS_CLASS_USER_INITIATED
							, .m_RelativePriority = fGetRelative(EExecutionPriority_Normal, EExecutionPriority_High)
						}
					;
				}
				else if (_Priority > EExecutionPriority_BelowNormal)
				{
					return
						{
							.m_Class = QOS_CLASS_DEFAULT
							, .m_RelativePriority = fGetRelative(EExecutionPriority_BelowNormal, EExecutionPriority_Normal)
						}
					;
				}
				else if (_Priority > EExecutionPriority_Low)
				{
					return
						{
							.m_Class = QOS_CLASS_UTILITY
							, .m_RelativePriority = fGetRelative(EExecutionPriority_Low, EExecutionPriority_BelowNormal)
						}
					;
				}
				else
				{
					return
						{
							.m_Class = QOS_CLASS_BACKGROUND
							, .m_RelativePriority = fGetRelative(0, EExecutionPriority_Low)
						}
					;
				}
			}

			// Verify priority boundaries map to expected QOS classes and relative priorities
			static_assert(fg_PriorityToQualityOfServiceConstexpr(EExecutionPriority_Highest).m_Class == QOS_CLASS_USER_INTERACTIVE);
			static_assert(fg_PriorityToQualityOfServiceConstexpr(EExecutionPriority_Highest).m_RelativePriority == 0);

			static_assert(fg_PriorityToQualityOfServiceConstexpr(EExecutionPriority_High).m_Class == QOS_CLASS_USER_INTERACTIVE);
			static_assert(fg_PriorityToQualityOfServiceConstexpr(EExecutionPriority_High).m_RelativePriority == QOS_MIN_RELATIVE_PRIORITY);

			static_assert(fg_PriorityToQualityOfServiceConstexpr(EExecutionPriority_AboveNormal).m_Class == QOS_CLASS_USER_INITIATED);

			static_assert(fg_PriorityToQualityOfServiceConstexpr(EExecutionPriority_Normal).m_Class == QOS_CLASS_DEFAULT);
			static_assert(fg_PriorityToQualityOfServiceConstexpr(EExecutionPriority_Normal).m_RelativePriority == 0);

			static_assert(fg_PriorityToQualityOfServiceConstexpr(EExecutionPriority_BelowNormal).m_Class == QOS_CLASS_UTILITY);
			static_assert(fg_PriorityToQualityOfServiceConstexpr(EExecutionPriority_BelowNormal).m_RelativePriority == 0);

			static_assert(fg_PriorityToQualityOfServiceConstexpr(EExecutionPriority_Low).m_Class == QOS_CLASS_BACKGROUND);
			static_assert(fg_PriorityToQualityOfServiceConstexpr(EExecutionPriority_Low).m_RelativePriority == 0);

			static_assert(fg_PriorityToQualityOfServiceConstexpr(EExecutionPriority_Lowest).m_Class == QOS_CLASS_BACKGROUND);
			static_assert(fg_PriorityToQualityOfServiceConstexpr(EExecutionPriority_Lowest).m_RelativePriority == QOS_MIN_RELATIVE_PRIORITY);
		}

		qos_class_t fg_PriorityToQualityOfService(EExecutionPriority _Priority, int &o_RelativePriority)
		{
			auto Settings = fg_PriorityToQualityOfServiceConstexpr(_Priority);

			o_RelativePriority = Settings.m_RelativePriority;

			return Settings.m_Class;
		}
	}
}

