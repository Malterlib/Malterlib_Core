// Copyright © 2018 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	struct CCoroutineThreadLocalHandler
	{
		CCoroutineThreadLocalHandler();
		CCoroutineThreadLocalHandler(CCoroutineThreadLocalHandler &&_Other) = default;
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
		CDebugThreadLocalScope(CDebugThreadLocalScope &&_Other);
		~CDebugThreadLocalScope();

		CCoroutineHandler *m_pCoroutineHandler;
		bool m_bValid = true;
	};

	#define DMibThreadLocalScopeDebugMember NMib::CDebugThreadLocalScope m_DebugThreadLocalScope
#else
	#define DMibThreadLocalScopeDebugMember
#endif

	struct CCrossActorCallStateScope : public CCoroutineThreadLocalHandler
	{
		CCrossActorCallStateScope();
		CCrossActorCallStateScope(CCrossActorCallStateScope &&_Other) = default;
		~CCrossActorCallStateScope();

		void f_Suspend() override;
		void f_Resume() override;
		virtual NFunction::TCFunctionMovable<void ()> f_StoreState() = 0;

		DMibListLinkDS_Link(CCrossActorCallStateScope, m_Link);
	};
}
