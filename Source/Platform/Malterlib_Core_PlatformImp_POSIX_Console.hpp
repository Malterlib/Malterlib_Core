// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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
#include <sys/ioctl.h>

#include <sys/param.h>

#include "Malterlib_Core_PlatformImp_POSIX.h"

// *************************************************************************************************************************
// POSIX Console & DebugOut Implementation
// *************************************************************************************************************************

void NSys::fg_ConsoleErrorOutputFlush()
{
	fsync(2);
}
void NSys::fg_ConsoleOutputFlush()
{
	fsync(1);
}

void NSys::fg_ConsoleOutputRaw(NMib::NStr::CStrNonTracked const &_Str)
{
	fg_ConsoleOutput(_Str);
}

void fg_WriteStringToPipe(int _Handle, ch8 const *_pStr, umint _Len)
{
	if (!_Len)
		return;
	while (_Len)
	{
		auto Written = write(_Handle, _pStr, _Len);
		if (Written < 0)
		{
			int ErrNo = errno;
			if (ErrNo == EAGAIN || ErrNo == EWOULDBLOCK)
			{
				pollfd ToPoll[1] = {0};
				int nPoll = 0;
				ToPoll[nPoll].fd = _Handle;
				ToPoll[nPoll].events = POLLWRNORM | POLLOUT;
				ToPoll[nPoll].revents = 0;
				++nPoll;

				int PollReturn = poll(ToPoll, nPoll, -1);
				if (PollReturn == -1)
					break;
				continue;
			}
			break;
		}
		else
		{
			_Len -= Written;
			_pStr += Written;
		}
	}
}

void NSys::fg_ConsoleOutput(ch8 const *_pStr, umint _Len)
{
	fg_WriteStringToPipe(1, _pStr, _Len);
}

void NSys::fg_ConsoleOutput(NMib::NStr::CStrSpan const &_Str)
{
	fg_WriteStringToPipe(1, _Str.f_GetStr(), _Str.f_GetLen());
}

void NSys::fg_ConsoleOutput(NMib::NStr::CStrNonTracked const &_Str)
{
	fg_WriteStringToPipe(1, _Str.f_GetStr(), _Str.f_GetLen());
}

void NSys::fg_ConsoleOutput(NMib::NStr::CStrIO const &_Str)
{
	fg_WriteStringToPipe(1, _Str.f_GetStr(), _Str.f_GetLen());
}

void NSys::fg_ConsoleOutputBinary(NMib::NContainer::CIOByteVector const &_Buffer)
{
	fg_WriteStringToPipe(1, (const ch8 *)_Buffer.f_GetArray(), _Buffer.f_GetLen());
}

NSys::CConsoleProperties NSys::fg_GetConsoleProperties()
{
	NSys::CConsoleProperties Return;
	winsize WindowSize;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &WindowSize))
		return Return;

	Return.m_Width = WindowSize.ws_col;
	Return.m_Height = WindowSize.ws_row;

	return Return;
}

void NSys::fg_ConsoleErrorOutput(NMib::NStr::CStrSpan const &_Str)
{
	fg_WriteStringToPipe(2, _Str.f_GetStr(), _Str.f_GetLen());
}

void NSys::fg_ConsoleErrorOutput(NMib::NStr::CStrIO const &_Str)
{
	fg_WriteStringToPipe(2, _Str.f_GetStr(), _Str.f_GetLen());
}

void NSys::fg_ConsoleErrorOutput(NMib::NStr::CStrNonTracked const &_Str)
{
	fg_WriteStringToPipe(2, _Str.f_GetStr(), _Str.f_GetLen());
}

void NSys::fg_DebugOutput(const ch8 *_pToOutput)
{
	if (g_bCreatedSystem && g_bCreatingSystemDone && fg_GetSys_POSIX()->f_GetMalterlibDisableStdErrLog())
		return;

	fg_WriteStringToPipe(2, _pToOutput, fg_StrLen(_pToOutput));
}

void NSys::fg_DebugOutput(const ch16 *_pToOutput)
{
	if (g_bCreatedSystem && g_bCreatingSystemDone && fg_GetSys_POSIX()->f_GetMalterlibDisableStdErrLog())
		return;

	CStrNonTracked Output = CWStrNonTracked(_pToOutput);
	fg_WriteStringToPipe(2, Output.f_GetStr(), Output.f_GetLen());
}

void NSys::fg_DebugOutput(const ch32 *_pToOutput)
{
	if (g_bCreatedSystem && g_bCreatingSystemDone && fg_GetSys_POSIX()->f_GetMalterlibDisableStdErrLog())
		return;

	CStrNonTracked Output = CUStrNonTracked(_pToOutput);
	fg_WriteStringToPipe(2, Output.f_GetStr(), Output.f_GetLen());
}

void NSys::fg_DebugOutput(const NMib::NStr::CStrNonTracked &_Output)
{
	if (g_bCreatedSystem && g_bCreatingSystemDone && fg_GetSys_POSIX()->f_GetMalterlibDisableStdErrLog())
		return;

	fg_WriteStringToPipe(2, _Output.f_GetStr(), _Output.f_GetLen());
}

void NSys::fg_DebugOutput(const NMib::NStr::CWStrNonTracked &_Output)
{
	if (g_bCreatedSystem && g_bCreatingSystemDone && fg_GetSys_POSIX()->f_GetMalterlibDisableStdErrLog())
		return;

	CStrNonTracked Output = _Output;
	fg_WriteStringToPipe(2, Output.f_GetStr(), Output.f_GetLen());
}

void NSys::fg_DebugOutput(const NMib::NStr::CUStrNonTracked &_Output)
{
	if (g_bCreatedSystem && g_bCreatingSystemDone && fg_GetSys_POSIX()->f_GetMalterlibDisableStdErrLog())
		return;

	CStrNonTracked Output = _Output;
	fg_WriteStringToPipe(2, Output.f_GetStr(), Output.f_GetLen());
}

