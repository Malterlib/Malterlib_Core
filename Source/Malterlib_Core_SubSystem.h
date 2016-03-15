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

	public:
#ifndef DMibNoAggregateConstexpr
		TCSubSystem() = delete;
		constexpr TCSubSystem(EAggregateInitialization _Init)
			: mp_ObjectSpace{}
			, mp_Lock{_Init}
			, mp_bWasCreated{false}
		{
		}
#endif
		
	public: // check if we can use private on MSVC and still get static initialization
		typedef uint8 CObjectType[sizeof(t_CSubSystem)];
		typedef typename NTraits::TCAlign<CObjectType, NTraits::TCAlignmentOf<t_CSubSystem>::mc_Value>::CType CTypeAligned;
		
		inline_never void fp_Create();
		
		CTypeAligned mp_ObjectSpace;
		NThread::CSpinLockAggregate mp_Lock;
		bool mp_bWasCreated;
	};
};

