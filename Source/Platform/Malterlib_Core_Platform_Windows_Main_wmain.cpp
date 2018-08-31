// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include <windows.h>

using namespace NMib;

extern "C" int __cdecl main(int argc, char *argv[], char *envp[]);

int __cdecl wmain(int _nArguments, wchar_t *_pArguments[], wchar_t *_pEnvironment[])
{
	NContainer::TCVector<NStr::CStr> Arguments;
	NContainer::TCVector<char *> ArgumentPointers;

	for (auto i = 0; i < _nArguments; ++i)
		Arguments.f_Insert(NStr::CWStr(_pArguments[i]));

	for (auto i = 0; i < _nArguments; ++i)
		ArgumentPointers.f_Insert(Arguments[i].f_GetStrWritable());

	ArgumentPointers.f_Insert((char *)nullptr);

	NContainer::TCVector<NStr::CStr> Environment;
	NContainer::TCVector<char *> EnvironmentPointers;

	for (auto pEnvironment = _pEnvironment; *pEnvironment; ++pEnvironment)
		Environment.f_Insert(NStr::CWStr(*pEnvironment));

	for (auto &EnvVar : Environment)
		EnvironmentPointers.f_Insert(EnvVar.f_GetStrWritable());
	EnvironmentPointers.f_Insert((char *)nullptr);

	return main(_nArguments, ArgumentPointers.f_GetArray(), EnvironmentPointers.f_GetArray());
}
