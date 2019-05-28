// Copyright © 2018 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	struct CCoroutineThreadLocalHandler
	{
		CCoroutineThreadLocalHandler();
		~CCoroutineThreadLocalHandler();

		virtual void f_Suspend() = 0;
		virtual void f_Resume() = 0;

		DMibListLinkDS_Link(CCoroutineThreadLocalHandler, m_Link);
	};

	struct CCoroutineHandler
	{
		~CCoroutineHandler();

		DMibListLinkDS_List(CCoroutineThreadLocalHandler, m_Link) m_ThreadLocalHandlers;
#if DMibEnableSafeCheck > 0
		mint m_nThreadLocalScopes = 0;
#endif
	};

#if DMibEnableSafeCheck > 0
	struct CDebugThreadLocalScope
	{
		CDebugThreadLocalScope();
		~CDebugThreadLocalScope();

		CCoroutineHandler *m_pCoroutineHandler;
	};

	#define DMibThreadLocalScopeDebugMember NMib::CDebugThreadLocalScope m_DebugThreadLocalScope
#else
	#define DMibThreadLocalScopeDebugMember
#endif
}
