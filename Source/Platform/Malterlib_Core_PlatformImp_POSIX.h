// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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

