// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_OnScopeExit.h"
#include "Malterlib_Core_OnScopeExitCatch.h"

namespace NMib
{
	COnScopeExitHelper &g_OnScopeExit = *((COnScopeExitHelper *)nullptr);

	COnScopeExitCatchHelper &g_OnScopeExitCatch = *((COnScopeExitCatchHelper *)nullptr);

}

