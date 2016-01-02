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
	void TCSubSystem<t_CSubSystem, t_DestructionOrder>::fp_Create()
	{
		DMibLock(mp_Lock);
		if (mp_bWasCreated)
			return;
		
		t_CSubSystem *pSubSystem = new(mp_ObjectSpace.m_Aligned) t_CSubSystem();
		pSubSystem->m_DestructionOrder = t_DestructionOrder;
		
		fg_GetSys()->f_AddSubSystem(*pSubSystem);
		NMib::NAtomic::fg_MemoryFence();
		mp_bWasCreated = true;
		NMib::NAtomic::fg_MemoryFence();
	}
	
	template <typename t_CSubSystem, ESubSystemDestruction t_DestructionOrder>
	t_CSubSystem &TCSubSystem<t_CSubSystem, t_DestructionOrder>::operator *()
	{
		if (mp_bWasCreated)
			return *((t_CSubSystem *)mp_ObjectSpace.m_Aligned);

		fp_Create();
		return *((t_CSubSystem *)mp_ObjectSpace.m_Aligned);
	}
	
	template <typename t_CSubSystem, ESubSystemDestruction t_DestructionOrder>
	t_CSubSystem *TCSubSystem<t_CSubSystem, t_DestructionOrder>::operator ->()
	{
		if (mp_bWasCreated)
			return ((t_CSubSystem *)mp_ObjectSpace.m_Aligned);

		fp_Create();
		return ((t_CSubSystem *)mp_ObjectSpace.m_Aligned);
	}
};

