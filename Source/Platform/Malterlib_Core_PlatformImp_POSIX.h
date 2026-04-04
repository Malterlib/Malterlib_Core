// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <sys/types.h>
#include <dirent.h>

class CPOSIXFileFind
{
public:
	CPOSIXFileFind();
	~CPOSIXFileFind();
	uint64 f_ParseAttrib();

	DIR *m_pDir;

	NMib::NStr::CStr m_FullPath;
	NMib::NStr::CStr m_SearchPattern;
	NMib::NStr::CStr m_LastFullName;

};

namespace NMib
{
	namespace NSys
	{
		namespace NPrivate
		{
			void fg_SetupLimits();
		}
	}
}

struct CSystem_POSIX
{
	bool f_GetMalterlibDisableStdErrLog();
	void f_DestroyThreadSpecific();
	void f_Destruct();

	NMib::NAtomic::TCAtomic<uint32> m_MalterlibDisableStdErrLog{0};
	NMib::NThread::CMutual m_ForkLock;
};
