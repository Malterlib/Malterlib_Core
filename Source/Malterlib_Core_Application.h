// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/System>
#include <Mib/Core/RuntimeType>

namespace NMib
{
	class CApplication
	{
	public:
		CApplication();
		virtual ~CApplication();
		virtual aint f_Main() = 0;
		void f_Exit(aint _ExitCode);

		NStr::CStr f_CommandLineParameters();
		aint f_NumCommandLineParameters();
		NStr::CStr f_CommandLineParameter(aint _iIndex);
	};

#	ifdef DMibPIR
#		define DMibAppImplement(_AppName) NMib::CAppClasses NMib::g_AppClasses = {"NMib::NAppName::" #_AppName}; namespace NAppName { DMibRuntimeClassNamed(NMib::CApplication, _AppName, NMib::NAppName::_AppName);}
#	else
#		define DMibAppImplement(_AppName) NMib::CAppClasses NMib::g_AppClasses = {"NMib::NAppName::" #_AppName}; namespace NAppName { DMibRuntimeClassNamed(NMib::CApplication, _AppName, NMib::NAppName::_AppName);} DMibPMain
#	endif

#	ifndef DMibPNoShortCuts
#		define DAppImplement(_AppName) DMibAppImplement(_AppName)
#	endif

}
