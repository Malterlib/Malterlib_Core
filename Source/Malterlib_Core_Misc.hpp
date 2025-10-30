// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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
