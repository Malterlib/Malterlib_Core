// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

class CApp : public NMib::CApplication
{
	aint f_Main()
	{
		return 0;
	}
};

DMibAppImplement(CApp);

#ifdef DPlatformFamily_Windows

#include <windows.h>

int __cdecl wmain(int argc, wchar_t *argv[], wchar_t *envp[]){}
int __stdcall WinMain(struct HINSTANCE__ * hInstance, struct HINSTANCE__ * hPrevInstance, char *lpCmdLine,int nShowCmd){;}
int __stdcall wWinMain(struct HINSTANCE__ * hInstance, struct HINSTANCE__ * hPrevInstance, wchar_t  *lpCmdLine,int nShowCmd) {}

#endif