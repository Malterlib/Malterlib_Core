// Copyright © 2018 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#ifdef DMalterlibUseStaticLibCxx
#	include <__exception/exception.h>
#	include <__exception/exception_ptr.h>
#else
#	include <exception>
#endif

#include <Mib/Storage/UniquePointer>

namespace NMib::NException
{
	using CExceptionPointer = std::exception_ptr;
}

namespace NMib::NConcurrency::NPrivate
{
	struct CPromiseDataBase;
}

namespace NMib
{
	/// Type-erased interface for on-resume results that can carry either a value or an exception.
	/// A null TCUniquePointer<ICOnResumeResult> means "continue coroutine", non-null means result is set.
	struct ICOnResumeResult
	{
		virtual ~ICOnResumeResult() = default;

		/// Try to get exception. Returns empty pointer if result contains a value (not exception).
		virtual NException::CExceptionPointer f_TryGetException() && = 0;

		/// Replace/set the exception (for combining exceptions)
		virtual void f_SetException(NException::CExceptionPointer &&_pException) = 0;

		/// Apply the value to the typed promise data (only called if result contains value, not exception)
		virtual void f_ApplyValue(NConcurrency::NPrivate::CPromiseDataBase *_pPromiseData) = 0;
	};

	struct CCoroutineThreadLocalHandler
	{
		CCoroutineThreadLocalHandler(bool _bAddToCoroutine = true);
		CCoroutineThreadLocalHandler(CCoroutineThreadLocalHandler &&_Other) = default;
		~CCoroutineThreadLocalHandler();

		virtual void f_Suspend() noexcept = 0;
		[[nodiscard]] virtual NStorage::TCUniquePointer<ICOnResumeResult> f_Resume() noexcept;
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
		virtual NFunction::TCFunctionMovable<void () noexcept> f_StoreState(bool _bFromSuspend) = 0;

		DMibListLinkDS_Link(CCrossActorCallStateScope, m_Link);
	};
}
