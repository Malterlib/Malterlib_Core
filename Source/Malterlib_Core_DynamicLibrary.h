// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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

			decltype(&::uuid_generate) m_fUUIDGenerate = nullptr;
			decltype(&::uuid_unparse) m_fUUIDUnparse = nullptr;

		protected:
			void fp_ClearSymbols() override
			{
				m_fUUIDGenerate = nullptr;
				m_fUUIDUnparse = nullptr;
			}

			void fp_FetchSymbols() override
			{
				fp_Fetch(m_fUUIDGenerate, "uuid_generate");
				fp_Fetch(m_fUUIDUnparse, "uuid_unparse");
			}
		};

	And you can use it like this:

		CUUIDLibrary UUIDLibrary;

		UUIDLibrary.f_Reload();

		if (UUIDLibrary.f_OK)
		{
			uuid_t UUID;
			UUIDLibrary.m_fUUIDGenerate(UUID);
		}
*/

#pragma once

namespace NMib
{
	DMibImpErrorClassDefine(CDynamicLibraryException, NMib::NException::CException);

	using CDLFlagUnderlying = int32;
	enum EDLFlag : int32 // EDynamicLibraryFlag, shortened for typing.
	{
		EDLFlag_None           = 0
		, EDLFlag_NoThrow      = DMibBit(0) // Do not throw an exception when an error is encountered, use return codes.
		, EDLFlag_AllowMissing = DMibBit(2) // Allow symbols to be missing without making the whole object bad.
	};

	struct CDynamicLibraryUtility
	{
		constexpr CDynamicLibraryUtility(EDLFlag _Flags);
		constexpr CDynamicLibraryUtility(NStr::CStr const &_Filename, EDLFlag _Flags);

		~CDynamicLibraryUtility();

		explicit operator bool () const;

		bool f_OK() const;
		bool f_Reload(NStr::CStr const &_Filename = {});
		void f_Unload();

	protected:
		virtual void fp_FetchSymbols() = 0;
		virtual void fp_ClearSymbols() = 0;

		void fp_Unload(bool _bClearSymbols);

		bool fp_LoadLibrary(NStr::CStr const &_Filename);

		template <typename tf_CType>
		bool fp_Fetch(tf_CType * &_pPtr, ch8 const *_pName);

		NStr::CStr mp_Filename;
		void *mp_pLibrary = nullptr;
		EDLFlag mp_Flags = EDLFlag_None;
		bool mp_bOK = false;
	};
}

#include "Malterlib_Core_DynamicLibrary.hpp"
