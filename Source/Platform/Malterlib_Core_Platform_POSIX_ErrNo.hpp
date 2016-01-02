// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "string.h"

namespace NMib
{
	namespace NPlatform
	{

#if defined(DPlatformFamily_Linux) || defined(DPlatformFamily_Emscripten)

		template <typename tf_CStr>
		tf_CStr fg_FormatErrno(typename tf_CStr::CFormat &_Desc, int _Err)
		{ 
			tf_CStr ErrorFormat;
			auto pError = strerror_r(_Err, ErrorFormat.f_GetStr(256), 256); // IMPORTANT: This function has different semantics on OSX and Linux
			
			tf_CStr Ret;
			
			Ret = "The OS returned an error from ";
			Ret += _Desc;
			
			if (!pError)
				Ret += typename tf_CStr::CFormat(": Unknown error ({})") << _Err;
			else
				Ret += typename tf_CStr::CFormat(": {} ({})") << pError << _Err;
			return Ret;
		}

#elif defined(DPlatformFamily_OSX)

		template <typename tf_CStr>
		tf_CStr fg_FormatErrno(typename tf_CStr::CFormat &_Desc, int _Err)
		{
			tf_CStr ErrorFormat;
			auto pError = ErrorFormat.f_GetStr();
			if (!strerror_r(_Err, ErrorFormat.f_GetStr(256), 256)) // IMPORTANT: This function has different semantics on OSX and Linux
				pError = ErrorFormat.f_GetStr();
			else
				pError = nullptr;		
			
			tf_CStr Ret;
			
			Ret = "The OS returned an error from ";
			Ret += _Desc;
			
			if (!pError)
				Ret += typename tf_CStr::CFormat(": Unknown error ({})") << _Err;
			else
				Ret += typename tf_CStr::CFormat(": {} ({})") << pError << _Err;
			return Ret;
		}

#else
	#error "Not implemented"
#endif
		
	}
}
