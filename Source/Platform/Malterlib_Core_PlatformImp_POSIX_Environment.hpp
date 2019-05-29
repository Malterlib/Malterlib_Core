// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

using namespace NMib;

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mman.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#include <uuid/uuid.h>
#include <poll.h>

#include <sys/param.h>

#include "Malterlib_Core_PlatformImp_POSIX.h"

// *************************************************************************************************************************
// POSIX Environment Vars Implementation
// *************************************************************************************************************************

NMib::NStr::CStr NMib::NSys::fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStr const &_VariableName)
{
	const char *pEnv = getenv(_VariableName);
	if (!pEnv) // Not an error, NULL is returned if the env var does not exist.
		return CStr();
	return CStr(pEnv);
}
NMib::NStr::CStrNonTracked NMib::NSys::fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStrNonTracked const &_VariableName)
{
	const char *pEnv = getenv(_VariableName);
	if (!pEnv) // Not an error, NULL is returned if the env var does not exist.
		return CStrNonTracked();
	return CStrNonTracked(pEnv);
}

NMib::NStr::CFStr256 NMib::NSys::fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CFStr256 const &_VariableName)
{
	const char *pEnv = getenv(_VariableName);
	if (!pEnv) // Not an error, NULL is returned if the env var does not exist.
		return NMib::NStr::CFStr256();
	return NMib::NStr::CFStr256(pEnv);
}

bool NMib::NSys::fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStr const &_VariableName, NMib::NStr::CStr& _Value)
{
	const char *pEnv = getenv(_VariableName);
	if (!pEnv)
		return false;
	_Value = CStr(pEnv);
	return true;
}
bool NMib::NSys::fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStrNonTracked const &_VariableName, NMib::NStr::CStrNonTracked &_Value)
{
	const char *pEnv = getenv(_VariableName);
	if (!pEnv)
		return false;
	_Value = CStrNonTracked(pEnv);
	return true;
}

void NMib::NSys::fg_Process_SetEnvironmentVariable_Unsafe(NMib::NStr::CStr const &_VariableName, NMib::NStr::CStr const &_Value)
{
	int ReturnValue = setenv(_VariableName, _Value, true);
	if (ReturnValue)
		DMibError(NPlatform::fg_FormatErrno(CStr::CFormat("setenv('{}')") << _VariableName, errno));
}

void NMib::NSys::fg_Process_SetEnvironmentVariable_Unsafe(NMib::NStr::CStrNonTracked const &_VariableName, NMib::NStr::CStrNonTracked const &_Value)
{
	int ReturnValue = setenv(_VariableName, _Value, true);
	if (ReturnValue)
		DMibError(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("setenv('{}')") << _VariableName, errno));
}

