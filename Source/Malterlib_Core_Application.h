// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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

#ifdef DMibPIR
#	define DMibAppImplement(d_AppName) \
		NMib::CAppClasses NMib::g_AppClasses = {"NMib::NAppName::" #d_AppName}; \
		namespace NAppName { DMibRuntimeClassNamed(NMib::CApplication, d_AppName, NMib::NAppName::d_AppName);}
#else
#	define DMibAppImplement(d_AppName) \
		NMib::CAppClasses NMib::g_AppClasses = {"NMib::NAppName::" #d_AppName}; \
		namespace NAppName { DMibRuntimeClassNamed(NMib::CApplication, d_AppName, NMib::NAppName::d_AppName);} \
		DMibPMain
#endif

#ifndef DMibPNoShortCuts
#	define DAppImplement(d_AppName) DMibAppImplement(d_AppName)
#endif
}
