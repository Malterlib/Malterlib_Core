// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib
{
	template <typename t_CSubSystem, ESubSystemDestruction t_DestructionOrder>
	mark_nodebug bool TCSubSystem<t_CSubSystem, t_DestructionOrder>::f_WasCreated() const
	{
		return mp_bWasCreated.f_Load(NAtomic::gc_MemoryOrder_Acquire);
	}

	template <typename t_CSubSystem, ESubSystemDestruction t_DestructionOrder>
	inline_never t_CSubSystem *TCSubSystem<t_CSubSystem, t_DestructionOrder>::fp_Create(NFunction::TCFunctionNoAlloc<t_CSubSystem *(void *_pMemory)> const &_fConstruct)
	{
		DMibLock(mp_Lock);
		if (mp_bWasCreated.f_Load(NAtomic::gc_MemoryOrder_Acquire))
			return ((t_CSubSystem *)mp_ObjectSpace);

		t_CSubSystem *pSubSystem;
		if (_fConstruct)
			pSubSystem = _fConstruct(mp_ObjectSpace);
		else
			pSubSystem = new(mp_ObjectSpace) t_CSubSystem();

		pSubSystem->m_DestructionOrder = t_DestructionOrder;

		fg_GetSys()->f_AddSubSystem(*pSubSystem);
		mp_bWasCreated.f_Store(true);

		return ((t_CSubSystem *)mp_ObjectSpace);
	}

	template <typename t_CSubSystem, ESubSystemDestruction t_DestructionOrder>
	mark_nodebug void TCSubSystem<t_CSubSystem, t_DestructionOrder>::f_Construct(NFunction::TCFunctionNoAlloc<t_CSubSystem *(void *_pMemory)> const &_fConstruct)
	{
		DMibFastCheck(!mp_bWasCreated.f_Load(NAtomic::gc_MemoryOrder_Acquire));
		fp_Create(_fConstruct);
	}

	template <typename t_CSubSystem, ESubSystemDestruction t_DestructionOrder>
	mark_nodebug inline_always t_CSubSystem &TCSubSystem<t_CSubSystem, t_DestructionOrder>::operator *()
	{
		if (mp_bWasCreated.f_Load(NAtomic::gc_MemoryOrder_Acquire)) [[likely]]
			return *((t_CSubSystem *)mp_ObjectSpace);

		return *fp_Create(nullptr);
	}

	template <typename t_CSubSystem, ESubSystemDestruction t_DestructionOrder>
	mark_nodebug inline_always t_CSubSystem *TCSubSystem<t_CSubSystem, t_DestructionOrder>::operator ->()
	{
		if (mp_bWasCreated.f_Load(NAtomic::gc_MemoryOrder_Acquire)) [[likely]]
			return ((t_CSubSystem *)mp_ObjectSpace);

		return fp_Create(nullptr);
	}

	template <typename t_CSubSystem, ESubSystemDestruction t_DestructionOrder>
	mark_nodebug t_CSubSystem *TCSubSystem<t_CSubSystem, t_DestructionOrder>::f_GetUnsafe()
	{
		return ((t_CSubSystem *)mp_ObjectSpace);
	}
};

