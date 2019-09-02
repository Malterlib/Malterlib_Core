// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#if defined(DPlatformFamily_Linux) || defined(DPlatformFamily_Emscripten)
//#	include "string.h"
#elif defined(DPlatformFamily_OSX)
#	include "string.h"
#endif

namespace NMib
{
	namespace NPlatform
	{
		template <typename tf_CStr>
		tf_CStr fg_FormatOSStatus(typename tf_CStr::CFormat &&_Desc, int _Status)
		{
			auto const *pError = fg_GetOSStatusError(_Status);
			
			tf_CStr Ret;
			
			Ret = "The OS returned an error from ";
			Ret += _Desc;
			
			if (!pError)
				Ret += typename tf_CStr::CFormat(": Unknown error ({})") << _Status;
			else
				Ret += typename tf_CStr::CFormat(": {} ({} {})") << (pError->m_pLong ? pError->m_pLong : "Unknown") << _Status << (pError->m_pShort ? pError->m_pShort : "Unknown");
			return Ret;
		}
		
	}
}

