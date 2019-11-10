// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

namespace NMib
{
	using namespace NContainer;
	using namespace NStr;
	namespace
	{
		struct CSubSystem_Core_Environment : public CSubSystem
		{
			CSubSystem_Core_Environment()
				: m_Environment(NSys::fg_Process_GetEnvironmentVariables_NonProtected())
			{
				auto *pProtected = m_Environment.f_FindEqual("MalterlibProtectedEnvironment");
				if (!pProtected)
					return;
				
				CStr ToProtect = *pProtected;
				while (!ToProtect.f_IsEmpty())
				{
					CStr Variable = fg_GetStrSep(ToProtect, ";");
					auto *pValue = m_Environment.f_FindEqual(Variable);
					if (!pValue)
						continue;
					m_ProtectedEnvironment[Variable] = *pValue;
					m_Environment.f_Remove(Variable);
				}
				
				m_ProtectedEnvironment["MalterlibProtectedEnvironment"] = *pProtected;
				m_Environment.f_Remove(pProtected);
			}
			
			NThread::CMutualManyRead m_Lock;
			CSystemEnvironment m_Environment;
			CSystemEnvironment m_ProtectedEnvironment;
		};
		
		constinit TCSubSystem<CSubSystem_Core_Environment, ESubSystemDestruction_BeforeMemoryManager> g_SubSystem_Core_Environment = {DAggregateInit};
	}
	
	CSystemEnvironment CSystem::f_Environment() const
	{
		auto &SubSystem = *g_SubSystem_Core_Environment;
		DMibLockRead(SubSystem.m_Lock);
		return SubSystem.m_Environment;
	}
	
	void CSystem::f_SetEnvironmentVariable(NStr::CStr const &_Name, NStr::CStr const &_Value)
	{
		auto &SubSystem = *g_SubSystem_Core_Environment;
		DMibLock(SubSystem.m_Lock);
		SubSystem.m_Environment[_Name] = _Value;
	}	
	
	CStr CSystem::f_GetEnvironmentVariable(CStr const &_Name, CStr const &_Default, bool *o_pExists) const
	{
		auto &SubSystem = *g_SubSystem_Core_Environment;
		DMibLockRead(SubSystem.m_Lock);
		auto *pVariable = SubSystem.m_Environment.f_FindEqual(_Name);
		if (pVariable)
		{
			if (o_pExists)
				*o_pExists = true;
			return *pVariable;
		}
		if (o_pExists)
			*o_pExists = false;
		return _Default;
	}
	
	CSystemEnvironment CSystem::f_ProtectedEnvironment() const
	{
		auto &SubSystem = *g_SubSystem_Core_Environment;
		DMibLockRead(SubSystem.m_Lock);
		auto Environment = SubSystem.m_ProtectedEnvironment;
		Environment += SubSystem.m_Environment; 
		return Environment;
	}

	void CSystem::f_ProtectEnvironmentVariable(NStr::CStr const &_Variable)
	{
		auto &SubSystem = *g_SubSystem_Core_Environment;
		DMibLock(SubSystem.m_Lock);
		auto *pValue = SubSystem.m_Environment.f_FindEqual(_Variable);
		if (!pValue)
			return;
		SubSystem.m_ProtectedEnvironment[_Variable] = *pValue;
		SubSystem.m_Environment.f_Remove(_Variable);
	}	
	
	CStr CSystem::f_GetProtectedEnvironmentVariable(CStr const &_Name, CStr const &_Default, bool *o_pExists) const
	{
		auto &SubSystem = *g_SubSystem_Core_Environment;
		DMibLockRead(SubSystem.m_Lock);
		auto *pVariable = SubSystem.m_ProtectedEnvironment.f_FindEqual(_Name);
		if (pVariable)
		{
			if (o_pExists)
				*o_pExists = true;
			return *pVariable;
		}
		return f_GetEnvironmentVariable(_Name, _Default, o_pExists);
	}
}
