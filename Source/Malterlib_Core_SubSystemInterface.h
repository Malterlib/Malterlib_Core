// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Intrusive/DoublyLinkedList>

namespace NMib
{
	enum ESubSystemDestruction
	{
		ESubSystemDestruction_BeforeMemoryManager
		, ESubSystemDestruction_BeforeNonTrackedMemoryManager
		, ESubSystemDestruction_Last
	};
	
	struct CSubSystem
	{
		DMibListLinkD_Link(CSubSystemImpl, m_Link);
		ESubSystemDestruction m_DestructionOrder;
		
		CSubSystem();
		virtual ~CSubSystem();
		
		virtual void f_PrepareFork();
		virtual void f_ForkedParent();
		virtual void f_ForkedChild();
		
		virtual void f_PreDestroyThreadSpecific();
		virtual void f_DestroyThreadSpecific();
		virtual void f_DestroyThreadLocal();

		virtual void f_ExitModule();
	};
};

