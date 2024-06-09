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
		t_CSubSystem *f_GetUnsafe();

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
		inline_never t_CSubSystem *fp_Create(NFunction::TCFunctionNoAlloc<t_CSubSystem *(void *_pMemory)> const &_fConstruct);
		
		alignas(t_CSubSystem) uint8 mp_ObjectSpace[sizeof(t_CSubSystem)];
		NThread::CLowLevelLockAggregate mp_Lock;
		NAtomic::TCAtomic<bool> mp_bWasCreated;
	};
};

