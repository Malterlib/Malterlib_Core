// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_Platform_POSIX_ErrNo.h"

namespace NMib
{
	namespace NPlatform
	{
		NStr::CFStr256 fg_FormatErrno(int _Err)
		{
			return fg_FormatErrno("", _Err);
		}

		NStr::CStr fg_FormatErrno(NStr::CStr::CFormat &_Desc, int _Err)
		{
			return fg_FormatErrno<NStr::CStr>(_Desc, _Err);
		}

		NStr::CStrNonTracked fg_FormatErrno(NStr::CStrNonTracked::CFormat &_Desc, int _Err)
		{
			return fg_FormatErrno<NStr::CStrNonTracked>(_Desc, _Err);
		}

		NStr::CFStr256 fg_FormatErrno(NStr::CFStr256::CFormat &_Desc, int _Err)
		{
			return fg_FormatErrno<NStr::CFStr256>(_Desc, _Err);
		}

#if defined(DPlatformFamily_Linux) || defined(DPlatformFamily_Emscripten)

		NStr::CFStr256 fg_FormatErrno(const ch8 *_pDesc, int _Err)
		{
			NStr::CFStr256 Ret;
			auto pError = strerror_r(_Err, Ret.f_GetStr(256), 256);
			if (!pError)
			{
				if (_pDesc[0])
					return NStr::CFStr256::CFormat("The OS returned an error from {}: Unknown error ({})") << _pDesc << _Err;
				else
					return NStr::CFStr256::CFormat("Unknown error ({})") << _Err;
			}
			if (_pDesc[0])
				return NStr::CFStr256::CFormat("The OS returned an error from {}: {} ({})") << _pDesc << pError << _Err;
			else
				return NStr::CFStr256::CFormat("{} ({})") << pError << _Err;
		}

#elif defined(DPlatformFamily_OSX)

		NStr::CFStr256 fg_FormatErrno(const ch8 *_pDesc, int _Err)
		{
			NStr::CFStr256 ErrorFormat;
			auto pError = ErrorFormat.f_GetStr();
			if (!strerror_r(_Err, ErrorFormat.f_GetStr(256), 256)) // IMPORTANT: This function has different semantics on OSX and Linux
				pError = ErrorFormat.f_GetStr();
			else
				pError = nullptr;		
			if (!pError)
			{
				if (_pDesc[0])
					return NStr::CFStr256::CFormat("The OS returned an error from {}: Unknown error ({})") << _pDesc << _Err;
				else
					return NStr::CFStr256::CFormat("Unknown error ({})") << _Err;
			}
			if (_pDesc[0])
				return NStr::CFStr256::CFormat("The OS returned an error from {}: {} ({})") << _pDesc << pError << _Err;
			else
				return NStr::CFStr256::CFormat("{} ({})") << pError << _Err;
		}

#else
	#error "Not implemented"
#endif
		
	}
}
