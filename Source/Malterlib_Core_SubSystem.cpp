// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_SubSystem.h"

namespace NMib
{
	CSubSystem::CSubSystem()
	{
	}

	CSubSystem::~CSubSystem()
	{
	}
	
	void CSubSystem::f_PrepareFork()
	{
	}
	
	void CSubSystem::f_ForkedParent()
	{
	}
	
	void CSubSystem::f_ForkedChild()
	{
	}
	
	void CSubSystem::f_PreDestroyThreadSpecific()
	{
	}
	
	void CSubSystem::f_DestroyThreadSpecific()
	{
	}
	
	void CSubSystem::f_DestroyThreadLocal()
	{
	}
	
	void CSubSystem::f_ExitModule()
	{
	}
	
	void CSystem::f_AddSubSystem(CSubSystem &_SubSystem)
	{
		DMibLock(mp_SubSystemsLock);
		mp_SubSystems.f_InsertFirst(_SubSystem);
	}

	void CSystem::fp_SubSystem_PrepareFork_BeforeMemoryManager()
	{
		mp_SubSystemsLock.f_Lock();
		mp_SubSystemsLock.f_PrepareFork();
		for (auto &SubSystem : mp_SubSystems)
		{
			if (SubSystem.m_DestructionOrder != ESubSystemDestruction_Last)
				SubSystem.f_PrepareFork();
		}
	}

	void CSystem::fp_SubSystem_ForkedChild_BeforeMemoryManager()
	{
		for (auto &SubSystem : mp_SubSystems)
		{
			if (SubSystem.m_DestructionOrder != ESubSystemDestruction_Last)
				SubSystem.f_ForkedChild();
		}
		mp_SubSystemsLock.f_ForkedChild();
		mp_SubSystemsLock.f_Unlock();
	}

	void CSystem::fp_SubSystem_ForkedParent_BeforeMemoryManager()
	{
		for (auto &SubSystem : mp_SubSystems)
		{
			if (SubSystem.m_DestructionOrder != ESubSystemDestruction_Last)
				SubSystem.f_ForkedParent();
		}
		mp_SubSystemsLock.f_ForkedParent();
		mp_SubSystemsLock.f_Unlock();
	}

	void CSystem::fp_SubSystem_PrepareFork_AfterMemoryManager()
	{
		for (auto &SubSystem : mp_SubSystems)
		{
			if (SubSystem.m_DestructionOrder == ESubSystemDestruction_Last)
				SubSystem.f_PrepareFork();
		}
	}

	void CSystem::fp_SubSystem_ForkedChild_AfterMemoryManager()
	{
		for (auto &SubSystem : mp_SubSystems)
		{
			if (SubSystem.m_DestructionOrder == ESubSystemDestruction_Last)
				SubSystem.f_ForkedChild();
		}
	}

	void CSystem::fp_SubSystem_ForkedParent_AfterMemoryManager()
	{
		for (auto &SubSystem : mp_SubSystems)
		{
			if (SubSystem.m_DestructionOrder == ESubSystemDestruction_Last)
				SubSystem.f_ForkedParent();
		}
	}

	void CSystem::fp_SubSystem_PreDestroyThreadSpecific()
	{
		DMibLock(mp_SubSystemsLock);
		for (auto &SubSystem : mp_SubSystems)
			SubSystem.f_PreDestroyThreadSpecific();
	}

	void CSystem::fp_SubSystem_DestroyThreadSpecific()
	{
		DMibLock(mp_SubSystemsLock);
		for (auto &SubSystem : mp_SubSystems)
			SubSystem.f_DestroyThreadSpecific();
	}

	void CSystem::fp_SubSystem_DestroyThreadLocal()
	{
		DMibLock(mp_SubSystemsLock);
		for (auto &SubSystem : mp_SubSystems)
			SubSystem.f_DestroyThreadLocal();
	}

	void CSystem::fp_SubSystem_DestroyBeforeMemoryManager()
	{
		DMibLock(mp_SubSystemsLock);
		for (auto iSubSystem = mp_SubSystems.f_GetIterator(); iSubSystem;)
		{
			auto *pSubSystem = &*iSubSystem;
			++iSubSystem;
			if (pSubSystem->m_DestructionOrder == ESubSystemDestruction_BeforeMemoryManager)
				pSubSystem->~CSubSystem();
		}
	}
	
	void CSystem::fp_SubSystem_DestroyBeforeNonTrackedMemoryManager()
	{
		DMibLock(mp_SubSystemsLock);
		for (auto iSubSystem = mp_SubSystems.f_GetIterator(); iSubSystem;)
		{
			auto *pSubSystem = &*iSubSystem;
			++iSubSystem;
			if (pSubSystem->m_DestructionOrder == ESubSystemDestruction_BeforeNonTrackedMemoryManager)
				pSubSystem->~CSubSystem();
		}
	}
	
	void CSystem::fp_SubSystem_Destroy()
	{
		DMibLock(mp_SubSystemsLock);
		while (auto pSubSystem = mp_SubSystems.f_Pop())
			pSubSystem->~CSubSystem();
	}

	void CSystem::fp_SubSystem_ExitModule()
	{
		DMibLock(mp_SubSystemsLock);
		for (auto &SubSystem : mp_SubSystems)
			SubSystem.f_ExitModule();
	}
	
};

#include <Mib/Time/Timer>

