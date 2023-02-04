// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

/* 
	Author:		Michael Wynne

	Contents:	CDynamicLibraryUtility

	Docs:
			
		TCDynamicLibraryUtility
		---------------------
		Subclass this to handle loading a dynamic library and extracting symbols from it.

			struct CDynamicLibraryUtility;

				_Flags			- A flags field of:
									EDLFlag_None
									EDLFlag_NoThrow			Do not throw an exception when an error is encountered, use return codes.
									EDLFlag_AllowMissing	Allow symbols to be missing without making the whole object bad.

		Once a dynamic library object is loaded via f_Reload the library will attempt to find all registered symbols.

		If the specified dynamic library could not be found or loaded f_OK() will return false.

		if EDLFlag_AllowMissing is not specified and some symbols could not be found f_OK() will return false.

		Otherwise f_OK() will return true.

		When specifying the name of the shared library to load (either via the macro or in the constructor of TCDynamicLibraryUtility)
		you can specify more than one name (just comma speparate them). This will cause TCDynamicLibraryUtility to try each name
		in turn when loading.

		Example usages:
		---------------

			struct CUUIDLibrary : public CDynamicLibraryUtility
			{
				CUUIDLibrary()
					: TCDynamicLibraryUtility(gc_Str<"libuuid.so.1">, EDLFlag_NoThrow)
				{
				}

				decltype(&::uuid_generate) uuid_generate = nullptr;
				decltype(&::uuid_unparse) uuid_unparse = nullptr;

			protected:
				void fp_ClearSymbols() override
				{
					uuid_generate = nullptr;
					uuid_unparse = nullptr;
				}

				void fp_FetchSymbols() override
				{
					fp_Fetch(uuid_generate, "uuid_generate");
					fp_Fetch(uuid_unparse, "uuid_unparse");
				}
			};

		And you can use it like this:

			CUUIDLibrary UUIDLibrary;

			UUIDLibrary.f_Reload();

			if (UUIDLibrary.f_OK)
			{
				uuid_t UUID;
				UUIDLibrary.uuid_generate(UUID);
			}
*/

#pragma once

namespace NMib
{
	DMibImpErrorClassDefine(CDynamicLibraryException, NMib::NException::CException);

	using CDLFlagUnderlying = int32;
	enum EDLFlag : int32 // EDynamicLibraryFlag, shortened for typing.
	{
		EDLFlag_None			= 0,
		EDLFlag_NoThrow			= DMibBit(0),		// Do not throw an exception when an error is encountered, use return codes.
		EDLFlag_AllowMissing	= DMibBit(2),	// Allow symbols to be missing without making the whole object bad.
	};

	struct CDynamicLibraryUtility
	{
		constexpr CDynamicLibraryUtility(EDLFlag _Flags)
			: mp_Flags(_Flags)
		{
		}

		constexpr CDynamicLibraryUtility(NStr::CStr const &_Filename, EDLFlag _Flags)
			: mp_Filename(_Filename)
			, mp_Flags(_Flags)
		{
		}

		~CDynamicLibraryUtility();

		explicit operator bool() const;

		bool f_OK() const;
		bool f_Reload(NStr::CStr const &_Filename = {});
		void f_Unload();

	protected:
		virtual void fp_FetchSymbols() = 0;
		virtual void fp_ClearSymbols() = 0;

		void fp_Unload(bool _bClearSymbols);

		bool fp_LoadLibrary(NStr::CStr const &_Filename);

		template<typename t_CType>
		bool fp_Fetch(t_CType* & _pPtr, ch8 const *_pName)
		{
			if (!mp_bOK)
			{
				_pPtr = nullptr;
				return false;
			}

			*(void**)&_pPtr = NSys::fg_GetLibrarySymbol(mp_pLibrary, _pName);
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

		NStr::CStr mp_Filename;
		void *mp_pLibrary = nullptr;
		EDLFlag mp_Flags = EDLFlag_None;
		bool mp_bOK = false;
	};
}
