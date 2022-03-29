// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Core_SubSystemInterface.h"

#include <Mib/Intrusive/DoublyLinkedList>
#include <Mib/Type/Alignment>
#include <Mib/Thread/SpinLock>

namespace NMib
{
	template <typename t_CSubSystem, ESubSystemDestruction t_DestructionOrder>
	class TCSubSystem
	{
	public:
		
		bool f_WasCreated() const;
		t_CSubSystem &operator *();
		t_CSubSystem *operator ->();
		
		void f_Construct(NFunction::TCFunctionNoAlloc<t_CSubSystem *(void *_pMemory)> const &_fConstruct);

	public:
		TCSubSystem() = delete;
		constexpr TCSubSystem(EAggregateInitialization _Init)
			: mp_ObjectSpace{}
			, mp_Lock{_Init}
			, mp_bWasCreated{false}
		{
		}
		
	public: // check if we can use private on MSVC and still get static initialization
		typedef uint8 CObjectType[sizeof(t_CSubSystem)];
		typedef typename NTraits::TCAlign<CObjectType, alignof(t_CSubSystem)>::CType CTypeAligned;
		
		inline_never t_CSubSystem *fp_Create(NFunction::TCFunctionNoAlloc<t_CSubSystem *(void *_pMemory)> const &_fConstruct);
		
		CTypeAligned mp_ObjectSpace;
		NThread::CLowLevelLockAggregate mp_Lock;
		NAtomic::TCAtomic<bool> mp_bWasCreated;
	};
};

