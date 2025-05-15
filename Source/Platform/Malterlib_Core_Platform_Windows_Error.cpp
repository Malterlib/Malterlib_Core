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

			LPWSTR pMessageBuffer = nullptr;

			auto fTryLanguage = [&](DWORD _Language)
				{
					return FormatMessageW
						(
							FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS
							, nullptr
							, _Error
							, _Language
							, (LPWSTR)&pMessageBuffer
							, 0
							, nullptr
						)
					;
				}
			;

			if (fTryLanguage(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)) || (GetLastError() == ERROR_RESOURCE_LANG_NOT_FOUND && fTryLanguage(0)))
			{
				if (pMessageBuffer)
				{
					NStr::CFStr256 Return = NStr::CFStr256::CFormat("{} {}") << _Error << pMessageBuffer;

					LocalFree(pMessageBuffer);

					return Return.f_Trim();
				}
			}

			return NStr::CFStr256::CFormat("{} Unknown error") << _Error;
		}

		template <typename tf_CStr>
		tf_CStr fg_ErrnoString(int _Err)
		{
			tf_CStr ErrorFormat;

			auto pError = strerror(_Err);
			if (!pError)
				return typename tf_CStr::CFormat("Unknown error ({})") << _Err;
			else
				return typename tf_CStr::CFormat("{} ({})") << pError << _Err;
		}

		template NStr::CStr fg_ErrnoString<NStr::CStr>(int _Err);
	}
}
