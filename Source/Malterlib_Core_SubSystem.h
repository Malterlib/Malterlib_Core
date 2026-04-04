// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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

		mark_nodebug bool f_WasCreated() const;
		mark_nodebug t_CSubSystem &operator *();
		mark_nodebug t_CSubSystem *operator ->();
		mark_nodebug t_CSubSystem *f_GetUnsafe();

		mark_nodebug void f_Construct(NFunction::TCFunctionNoAlloc<t_CSubSystem *(void *_pMemory)> const &_fConstruct);

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

