// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_Platform_Windows_Error.h"
#include <Windows.h>

namespace NMib
{
	namespace NPlatform
	{
		NStr::CFStr256 fg_Win32_GetLastErrorStr(uint32 _Error)
		{
			if (!_Error)
				_Error = GetLastError();
			LPVOID lpMsgBuf;
			if (FormatMessage( 
				FORMAT_MESSAGE_ALLOCATE_BUFFER | 
				FORMAT_MESSAGE_FROM_SYSTEM | 
				FORMAT_MESSAGE_IGNORE_INSERTS,
				nullptr,
				_Error,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
				(LPTSTR) &lpMsgBuf,
				0,
				nullptr 
			))
			{
				// Process any inserts in lpMsgBuf.
				// ...
				// Display the string.

				NStr::CFStr256 LastError;
				if (lpMsgBuf)
				{
					LastError = (LPTSTR)lpMsgBuf;
					LocalFree(lpMsgBuf);
				}

				return LastError.f_Trim();
			}
			else
			{
				return NStr::CFStr256::CFormat("0x{nfh,sj8,sf0}") << _Error;

			}
		}

		
	}
}
