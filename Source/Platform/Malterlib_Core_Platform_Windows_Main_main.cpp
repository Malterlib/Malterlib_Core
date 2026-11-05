// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

#include <windows.h>

using namespace NMib;

extern "C" int __cdecl wmain(int _nArguments, wchar_t *_Arguments[], wchar_t *_Environment[]);

int __cdecl main(int _nArguments, char *_Arguments[], char *_Environment[])
{
	NContainer::TCVector<NStr::CWStr> Arguments;
	NContainer::TCVector<wchar_t *> ArgumentPointers;

	for (auto iArgument = 0; iArgument < _nArguments; ++iArgument)
		Arguments.f_Insert(NStr::CStr(_Arguments[iArgument]));

	for (auto iArgument = 0; iArgument < _nArguments; ++iArgument)
		ArgumentPointers.f_Insert(Arguments[iArgument].f_GetStrWritable());

	ArgumentPointers.f_Insert((wchar_t *)nullptr);

	NContainer::TCVector<NStr::CWStr> Environment;
	NContainer::TCVector<wchar_t *> EnvironmentPointers;

	for (auto pEnvironment = _Environment; *pEnvironment; ++pEnvironment)
		Environment.f_Insert(NStr::CStr(*pEnvironment));

	for (auto &EnvVar : Environment)
		EnvironmentPointers.f_Insert(EnvVar.f_GetStrWritable());
	EnvironmentPointers.f_Insert((wchar_t *)nullptr);

	return wmain(_nArguments, ArgumentPointers.f_GetArray(), EnvironmentPointers.f_GetArray());
}
