// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

#include <windows.h>

using namespace NMib;

extern "C" int __cdecl main(int _nArguments, char *_Arguments[], char *_Environment[]);

int __cdecl wmain(int _nArguments, wchar_t *_Arguments[], wchar_t *_Environment[])
{
	NContainer::TCVector<NStr::CStr> Arguments;
	NContainer::TCVector<char *> ArgumentPointers;

	for (auto iArgument = 0; iArgument < _nArguments; ++iArgument)
		Arguments.f_Insert(NStr::CWStr(_Arguments[iArgument]));

	for (auto iArgument = 0; iArgument < _nArguments; ++iArgument)
		ArgumentPointers.f_Insert(Arguments[iArgument].f_GetStrWritable());

	ArgumentPointers.f_Insert((char *)nullptr);

	NContainer::TCVector<NStr::CStr> Environment;
	NContainer::TCVector<char *> EnvironmentPointers;

	for (auto pEnvironment = _Environment; *pEnvironment; ++pEnvironment)
		Environment.f_Insert(NStr::CWStr(*pEnvironment));

	for (auto &EnvVar : Environment)
		EnvironmentPointers.f_Insert(EnvVar.f_GetStrWritable());
	EnvironmentPointers.f_Insert((char *)nullptr);

	return main(_nArguments, ArgumentPointers.f_GetArray(), EnvironmentPointers.f_GetArray());
}
