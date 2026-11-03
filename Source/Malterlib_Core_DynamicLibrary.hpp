// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib
{
	constexpr CDynamicLibraryUtility::CDynamicLibraryUtility(EDLFlag _Flags)
		: mp_Flags(_Flags)
	{
	}

	constexpr CDynamicLibraryUtility::CDynamicLibraryUtility(NStr::CStr const &_Filename, EDLFlag _Flags)
		: mp_Filename(_Filename)
		, mp_Flags(_Flags)
	{
	}

	template <typename tf_CType>
	bool CDynamicLibraryUtility::fp_Fetch(tf_CType * &_pPtr, ch8 const *_pName)
	{
		if (!mp_bOK)
		{
			_pPtr = nullptr;
			return false;
		}

		*(void **)&_pPtr = NSys::fg_GetLibrarySymbol(mp_pLibrary, _pName);
		if (!_pPtr)
		{
			if (!(mp_Flags & EDLFlag_AllowMissing))
			{
				mp_bOK = false;
				if (!(mp_Flags & EDLFlag_NoThrow))
					DMibImpError(CDynamicLibraryException, (typename NStr::CStrNonTracked::CFormat("Failed to get symbol {}") << _pName).f_GetStr());
			}
		}

		return _pPtr ? true : false;
	}
}
