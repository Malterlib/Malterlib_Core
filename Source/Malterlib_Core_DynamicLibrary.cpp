// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_DynamicLibrary.h"

namespace NMib
{
	DMibImpErrorClassImplement(CDynamicLibraryException);

	CDynamicLibraryUtility::~CDynamicLibraryUtility()
	{
		fp_Unload(false);
		mp_Filename.f_Clear();
	}

	bool CDynamicLibraryUtility::f_OK() const
	{
		return mp_bOK;
	}

	CDynamicLibraryUtility::operator bool() const
	{
		return mp_bOK;
	}

	bool CDynamicLibraryUtility::f_Reload(NStr::CStr const &_Filename)
	{
		if (_Filename)
			mp_Filename = _Filename;

		if (mp_pLibrary)
			f_Unload();

		return fp_LoadLibrary(mp_Filename);
	}

	void CDynamicLibraryUtility::fp_Unload(bool _bClearSymbols)
	{
		if (mp_pLibrary)
		{
			NSys::fg_FreeLibrary(mp_pLibrary);
			mp_pLibrary = nullptr;
		}

		if (_bClearSymbols)
			fp_ClearSymbols();
	}

	void CDynamicLibraryUtility::f_Unload()
	{
		fp_Unload(true);
	}

	bool CDynamicLibraryUtility::fp_LoadLibrary(NStr::CStr const &_Filename)
	{
		NStr::CStr Filename = _Filename;
		NStr::CStr CurFilename;
		while (!Filename.f_IsEmpty() && !mp_pLibrary)
		{
			CurFilename = fg_GetStrSep(Filename, ",");

			mp_pLibrary = NSys::fg_LoadLibrary(CurFilename);
		}

		if (!mp_pLibrary)
		{
			mp_bOK = false;
			if (!(mp_Flags & EDLFlag_NoThrow))
				DMibImpError(CDynamicLibraryException, (typename NStr::CStrNonTracked::CFormat("Failed to load library from {}") << _Filename).f_GetStr());
		}
		else
		{
			mp_bOK = true;
			mp_Filename = CurFilename;
		}

		if (mp_bOK)
			fp_FetchSymbols();

		return mp_bOK;
	}
}
