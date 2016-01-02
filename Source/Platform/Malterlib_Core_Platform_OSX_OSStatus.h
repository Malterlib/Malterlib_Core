// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

namespace NMib
{
	namespace NPlatform
	{
		struct COSXError
		{
			int m_Code;
			const ch8 *m_pShort;
			const ch8 *m_pLong;
		};
		
		COSXError const *fg_GetOSStatusError(int _Status);

		NStr::CFStr256 fg_FormatOSStatus(const ch8 *_pDesc, int _Status);
		template <typename tf_CStr>
		tf_CStr fg_FormatOSStatus(typename tf_CStr::CFormat &_Desc, int _Status);
	}
}

#include "Malterlib_Core_Platform_OSX_OSStatus.hpp"
