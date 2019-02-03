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
}
