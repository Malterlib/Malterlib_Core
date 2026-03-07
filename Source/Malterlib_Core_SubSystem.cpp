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

	void CSubSystem::f_DestroyAggregates(bool _bDestroySystem)
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

	void CSubSystem::f_ForkedChildAfterThreadLocal()
	{
	}

	void CSubSystem::f_ForkedParentAfterThreadLocal()
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
		for (auto &SubSystem : mp_SubSystems)
		{
			if (!SubSystem.f_IsAfterMemoryManager())
				SubSystem.f_PrepareFork();
		}
	}

	void CSystem::fp_SubSystem_ForkedChild_BeforeMemoryManager()
	{
		for (auto &SubSystem : mp_SubSystems)
		{
			if (!SubSystem.f_IsAfterMemoryManager())
				SubSystem.f_ForkedChild();
		}
	}

	void CSystem::fp_SubSystem_ForkedChild_AfterThreadLocal()
	{
		for (auto &SubSystem : mp_SubSystems)
			SubSystem.f_ForkedChildAfterThreadLocal();
	}

	void CSystem::fp_SubSystem_ForkedParent_AfterThreadLocal()
	{
		for (auto &SubSystem : mp_SubSystems)
			SubSystem.f_ForkedParentAfterThreadLocal();
	}

	void CSystem::fp_SubSystem_ForkedParent_BeforeMemoryManager()
	{
		for (auto &SubSystem : mp_SubSystems)
		{
			if (!SubSystem.f_IsAfterMemoryManager())
				SubSystem.f_ForkedParent();
		}
	}

	void CSystem::fp_SubSystem_PrepareFork_AfterMemoryManager()
	{
		for (auto &SubSystem : mp_SubSystems)
		{
			if (SubSystem.f_IsAfterMemoryManager())
				SubSystem.f_PrepareFork();
		}
	}

	void CSystem::fp_SubSystem_ForkedChild_AfterMemoryManager()
	{
		for (auto &SubSystem : mp_SubSystems)
		{
			if (SubSystem.f_IsAfterMemoryManager())
				SubSystem.f_ForkedChild();
		}
	}

	void CSystem::fp_SubSystem_ForkedParent_AfterMemoryManager()
	{
		for (auto &SubSystem : mp_SubSystems)
		{
			if (SubSystem.f_IsAfterMemoryManager())
				SubSystem.f_ForkedParent();
		}
	}

	void CSystem::fp_SubSystem_PreDestroyThreadSpecific()
	{
		DMibLock(mp_SubSystemsLock);
		for (auto iSubSystem = mp_SubSystems.f_GetIterator();iSubSystem;)
		{
			auto &SubSystem = *iSubSystem;
			++iSubSystem;
			DMibUnlock(mp_SubSystemsLock);
			SubSystem.f_PreDestroyThreadSpecific();
		}
	}

	void CSystem::fp_SubSystem_DestroyThreadSpecific()
	{
		DMibLock(mp_SubSystemsLock);
		for (auto iSubSystem = mp_SubSystems.f_GetIterator();iSubSystem;)
		{
			auto &SubSystem = *iSubSystem;
			++iSubSystem;
			DMibUnlock(mp_SubSystemsLock);
			SubSystem.f_DestroyThreadSpecific();
		}
	}

	void CSystem::fp_SubSystem_DestroyThreadLocal()
	{
		DMibLock(mp_SubSystemsLock);
		for (auto iSubSystem = mp_SubSystems.f_GetIterator();iSubSystem;)
		{
			auto &SubSystem = *iSubSystem;
			++iSubSystem;
			DMibUnlock(mp_SubSystemsLock);
			SubSystem.f_DestroyThreadLocal();
		}
	}

	void CSystem::fp_SubSystem_DestroySubsystems(ESubSystemDestruction _ToDestroy)
	{
		DMibLock(mp_SubSystemsLock);
		for (auto iSubSystem = mp_SubSystems.f_GetIterator(); iSubSystem;)
		{
			auto &SubSystem = *iSubSystem;
			++iSubSystem;
			if (SubSystem.m_DestructionOrder == _ToDestroy)
				SubSystem.~CSubSystem();
		}
	}

	void CSystem::fp_SubSystem_DestroyBeforeMemoryManager()
	{
		fp_SubSystem_DestroySubsystems(ESubSystemDestruction_BeforeMemoryManager);
	}

	void CSystem::fp_SubSystem_DestroyBeforeNonTrackedMemoryManager()
	{
		fp_SubSystem_DestroySubsystems(ESubSystemDestruction_BeforeNonTrackedMemoryManager);
	}

	void CSystem::fp_SubSystem_DestroyBeforeThreadLocals()
	{
		fp_SubSystem_DestroySubsystems(ESubSystemDestruction_BeforeThreadLocals);
	}

	void CSystem::fp_SubSystem_Destroy()
	{
		DMibLock(mp_SubSystemsLock);
		while (auto pSubSystem = mp_SubSystems.f_Pop())
			pSubSystem->~CSubSystem();
	}

	void CSystem::fp_SubSystem_DestroyAggregates(bool _bDestroySystem)
	{
		DMibLock(mp_SubSystemsLock);
		for (auto &SubSystem : mp_SubSystems)
			SubSystem.f_DestroyAggregates(_bDestroySystem);
	}

	void CSystem::fp_SubSystem_ExitModule()
	{
		DMibLock(mp_SubSystemsLock);
		for (auto &SubSystem : mp_SubSystems)
			SubSystem.f_ExitModule();
	}
};
