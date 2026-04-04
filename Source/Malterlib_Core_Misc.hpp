// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

namespace NMib
{
	namespace NMisc
	{

		template <typename tf_CIntType>
		tf_CIntType fg_GetHighEntropyRandomInteger()
		{
			static_assert(sizeof(tf_CIntType) <= 16);
			tf_CIntType Return;
			NSys::fg_Security_GenerateHighEntropyData((uint8 *)&Return, sizeof(Return));
			return Return;
		}

	} // Namespace NMisc

} // Namespace NMib
