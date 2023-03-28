// Copyright © 2018 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NException
{
	using CExceptionPointer = std::exception_ptr;
}

namespace NMib
{
	struct CCoroutineThreadLocalHandler
	{
		CCoroutineThreadLocalHandler(bool _bAddToCoroutine = true);
		CCoroutineThreadLocalHandler(CCoroutineThreadLocalHandler &&_Other) = default;
		~CCoroutineThreadLocalHandler();

		virtual void f_Suspend() noexcept = 0;
		[[nodiscard]] virtual NException::CExceptionPointer f_Resume() noexcept;
		virtual void f_ResumeNoExcept() noexcept;

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
		CCrossActorCallStateScope(bool _bAddToCoroutine = true);
		CCrossActorCallStateScope(CCrossActorCallStateScope &&_Other) = default;
		~CCrossActorCallStateScope();

		void f_Suspend() noexcept override;
		void f_ResumeNoExcept() noexcept override;

		virtual void f_InitialSuspend() = 0;
		virtual NFunction::TCFunctionMovable<void (bool _bException) noexcept> f_StoreState(bool _bFromSuspend) = 0;

		DMibListLinkDS_Link(CCrossActorCallStateScope, m_Link);
	};
}
