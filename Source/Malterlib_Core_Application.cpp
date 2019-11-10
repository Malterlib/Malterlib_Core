// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/System>
#include <Mib/Core/SubSystem>
#include <Mib/Core/RuntimeType>

#include "Malterlib_Core_Application.h"

namespace NMib
{
	namespace
	{
		struct CSubSystem_Core_Application : public CSubSystem
		{
			NStorage::TCUniquePointer<CApplication> m_pApplication;
			
			void f_ExitModule() override
			{
				m_pApplication.f_Clear();
			}
		};
		
		constinit TCSubSystem<CSubSystem_Core_Application, ESubSystemDestruction_BeforeMemoryManager> g_SubSystem_Core_Application = {DAggregateInit};
	}
	
	CApplication::CApplication()
	{

	}

	CApplication::~CApplication()
	{

	}

	void CApplication::f_Exit(aint _ExitCode)
	{

	}

	NStr::CStr CApplication::f_CommandLineParameters()
	{
		return fg_GetSys()->f_CommandLineParameters();
	}

	aint CApplication::f_NumCommandLineParameters()
	{
		return fg_GetSys()->f_NumCommandLineParameters();
	}

	NStr::CStr CApplication::f_CommandLineParameter(aint _iIndex)
	{
		return fg_GetSys()->f_CommandLineParameter(_iIndex);
	}

	
	
	aint CSystem::f_RunApplication()
	{		
		auto &ApplicationSubSystem = *g_SubSystem_Core_Application;
		
		ApplicationSubSystem.m_pApplication = fg_CreateRuntimeType<CApplication>(g_AppClasses.m_pAppClass);
		if (!ApplicationSubSystem.m_pApplication)
			return 255;
		
		return ApplicationSubSystem.m_pApplication->f_Main();
	}
}
