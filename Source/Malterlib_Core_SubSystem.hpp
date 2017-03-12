// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	template <typename t_CSubSystem, ESubSystemDestruction t_DestructionOrder>
	bool TCSubSystem<t_CSubSystem, t_DestructionOrder>::f_WasCreated() const
	{
		return mp_bWasCreated;
	}

	template <typename t_CSubSystem, ESubSystemDestruction t_DestructionOrder>
	inline_never t_CSubSystem *TCSubSystem<t_CSubSystem, t_DestructionOrder>::fp_Create(NFunction::TCFunctionNoAlloc<t_CSubSystem *(void *_pMemory)> const &_fConstruct)
	{
		DMibLock(mp_Lock);
		if (mp_bWasCreated)
			return ((t_CSubSystem *)mp_ObjectSpace.m_Aligned);
		
		t_CSubSystem *pSubSystem;
		if (_fConstruct)
			pSubSystem = _fConstruct(mp_ObjectSpace.m_Aligned);
		else
			pSubSystem = new(mp_ObjectSpace.m_Aligned) t_CSubSystem();
		
		pSubSystem->m_DestructionOrder = t_DestructionOrder;
		
		fg_GetSys()->f_AddSubSystem(*pSubSystem);
		NMib::NAtomic::fg_MemoryFence();
		mp_bWasCreated = true;
		NMib::NAtomic::fg_MemoryFence();
		
		return ((t_CSubSystem *)mp_ObjectSpace.m_Aligned);
	}
	
	template <typename t_CSubSystem, ESubSystemDestruction t_DestructionOrder>
	void TCSubSystem<t_CSubSystem, t_DestructionOrder>::f_Construct(NFunction::TCFunctionNoAlloc<t_CSubSystem *(void *_pMemory)> const &_fConstruct)
	{
		DMibFastCheck(!mp_bWasCreated);
		fp_Create(_fConstruct);
	}
	
	template <typename t_CSubSystem, ESubSystemDestruction t_DestructionOrder>
	inline_always t_CSubSystem &TCSubSystem<t_CSubSystem, t_DestructionOrder>::operator *()
	{
		if (likely(mp_bWasCreated))
			return *((t_CSubSystem *)mp_ObjectSpace.m_Aligned);

		return *fp_Create(nullptr);
	}
	
	template <typename t_CSubSystem, ESubSystemDestruction t_DestructionOrder>
	inline_always t_CSubSystem *TCSubSystem<t_CSubSystem, t_DestructionOrder>::operator ->()
	{
		if (likely(mp_bWasCreated))
			return ((t_CSubSystem *)mp_ObjectSpace.m_Aligned);

		return fp_Create(nullptr);
	}
};

