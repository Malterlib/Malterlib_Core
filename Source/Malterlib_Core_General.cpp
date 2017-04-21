// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

namespace NMib
{
	constexpr CExplicitHelper g_ExplicitInit{};
	CExplicitHelper const &g_Explicit = g_ExplicitInit;
}
