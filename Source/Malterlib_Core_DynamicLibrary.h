// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

/* 
	Author:		Michael Wynne

	Contents:	TDynamicLibraryUtility
				DMibDefineDynamicLibraryClass

	Docs:
			
		TDynamicLibraryUtility
		---------------------
		Subclass this, either manually or via the DMibDefineDynamicLibraryClass macro to handle loading a dynamic library
		and extracting symbols from it.

			template<typename t_CDerived, EDLFlag _Flags>
			class TDynamicLibraryUtility;

				t_CDerived		- The class deriving from TDynamicLibraryUtility
				_Flags			- A flags field of:
									EDLFlag_None
									EDLFlag_NoThrow			Do not throw an exception when an error is encountered, use return codes.
									EDLFlag_NoAutoLoad		Do not load the library on construction.
									EDLFlag_AllowMissing	Allow symbols to be missing without making the whole object bad.

		Once a dynamic library object is loaded (either upon construction or via f_Reload if EDLFlag_NoAutoLoad was specified),
		the library will attempt to find all registered symbols. 

		If the specified dynamic library could not be found or loaded f_OK() will return false.

		if EDLFlag_AllowMissing is not specified and some symbols could not be found f_OK() will return false.

		Otherwise f_OK() will return true.

		When specifying the name of the shared library to load (either via the macro or in the constructor of TDynamicLibraryUtility)
		you can specify more than one name (just comma speparate them). This will cause TDynamicLibraryUtility to try each name
		in turn when loading.

		************************************************************
		NOTE: Any subclasses of TDynamicLibraryUtility must be POD!!
		************************************************************

		Example usages:
		---------------

		With the macro:

			// Assuming the header file containing uuid_* has already been included:
			extern "C"
			{
				void uuid_generate(uuid_t _Out);
				void uuid_unparse(uuid_t _Uu, uuid_string_t _Out);
			};

			// Define the library class like this...
			DMibDefineDynamicLibraryClass(CUUIDLibrary, EDLFlag_NoThrow | EDLFlag_NoAutoLoad, "libuuid.so.1"
											,	uuid_generate				// These are actual functions available in the current scope...
											,	uuid_unparse
										);

		The above is equivalent to the following:

			struct CUUIDLibrary : public TDynamicLibraryUtility<CUUIDLibrary, EDLFlag_NoThrow | EDLFlag_NoAutoLoad>
			{
				CUUIDLibrary() : TDynamicLibraryUtility("libuuid.so.1")
				{
				}
				
				~CUUIDLibrary() {}

				void (*uuid_generate)(uuid_t _Out);
				void (*uuid_unparse)(uuid_t _Uu, uuid_string_t _Out);

			protected:

				typedef TDynamicLibraryUtility<CUUIDLibrary, EDLFlag_NoThrow> CDynamicLibraryUtility;
				friend CDynamicLibraryUtility;

				void fp_FetchSymbols()
				{	
					fp_Fetch(uuid_generate, "uuid_generate");
					fp_Fetch(uuid_unparse, "uuid_unparse");
				}

			};

		And you can use it like this:

			CUUIDLibrary UUIDLibrary;

			UUIDLibrary.f_Reload(); // Because we specified EDLFlag_NoAutoLoad we have to load it.

			if (UUIDLibrary.f_OK)
			{
				uuid_t UUID;
				UUIDLibrary.uuid_generate(UUID);
			}

		Sometimes the macro will not give you enough flexibility and you will have to create the subclass yourself.
*/

#pragma once

#ifdef BOOST_PP_VARIADICS
	#undef BOOST_PP_VARIADICS
#endif
#define BOOST_PP_VARIADICS 1
#include <boost/preprocessor/seq.hpp>
#include <boost/preprocessor/variadic.hpp>
#include <boost/preprocessor/stringize.hpp>

#include <Mib/Core/Core>

// Internal Macros
#define _DMibDefineDynamicLibraryClass_Member(_R, _Data, _Member)	\
	decltype(&::_Member) _Member;

#define _DMibDefineDynamicLibraryClass_MemberInit(_R, _Data, _Member)	\
	, _Member(nullptr)

#define _DMibDefineDynamicLibraryClass_Fetch(_R, _Data, _Member)	\
	fp_Fetch(_Member, BOOST_PP_STRINGIZE(_Member));

/*
	Utility Macro

	DMibDefineDynamicLibraryClass(_ClassName, _Flags, _LibraryFile, _FunctionNames...)

	Function names should be names of already declared global functions.

	e.g. You include the header file for a library and then use DMibDefineDynamicLibraryClass
	to create a class to load the library and access each function.
*/
#ifndef DMibNoAggregateConstexpr
#define DMibDefineDynamicLibraryClassAggregate(_ClassName, _Flags, _LibraryFile, ...)	\
	struct _ClassName : public NMib::TDynamicLibraryUtilityAggregate<_ClassName, (NMib::EDLFlag)(_Flags), NMib::NStr::CFStrAggregate256, NMib::NStr::CFStr256> 	\
	{																		\
		typedef NMib::TDynamicLibraryUtilityAggregate<_ClassName, (NMib::EDLFlag)(_Flags), NMib::NStr::CFStrAggregate256, NMib::NStr::CFStr256> CSuper; \
		constexpr _ClassName(EAggregateInitialization _Init)				\
			: CSuper{_Init}	\
			BOOST_PP_SEQ_FOR_EACH( _DMibDefineDynamicLibraryClass_MemberInit, 0, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__) )	\
		{																	\
		}																	\
		~_ClassName() {}													\
		BOOST_PP_SEQ_FOR_EACH( _DMibDefineDynamicLibraryClass_Member, 0, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__) )	\
	protected:																\
		friend class NMib::TDynamicLibraryUtilityAggregate<_ClassName, (NMib::EDLFlag)(_Flags), NMib::NStr::CFStrAggregate256, NMib::NStr::CFStr256>;	\
		void fp_FetchSymbols()												\
		{																	\
			BOOST_PP_SEQ_FOR_EACH( _DMibDefineDynamicLibraryClass_Fetch, 0, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__) ) \
		} \
	};
#endif

#define DMibDefineDynamicLibraryClass(_ClassName, _Flags, _LibraryFile, ...)	\
	struct _ClassName : public NMib::TDynamicLibraryUtility<_ClassName, (NMib::EDLFlag)(_Flags)> 	\
	{																		\
		_ClassName() : TDynamicLibraryUtility(_LibraryFile)					\
		{																	\
		}																	\
		~_ClassName() {}													\
		BOOST_PP_SEQ_FOR_EACH( _DMibDefineDynamicLibraryClass_Member, 0, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__) )	\
	protected:																\
		friend class NMib::TDynamicLibraryUtility<_ClassName, (NMib::EDLFlag)(_Flags)>;								\
		friend class NMib::TDynamicLibraryUtilityAggregate<_ClassName, (NMib::EDLFlag)(_Flags)>;								\
		void fp_FetchSymbols()												\
		{																	\
			BOOST_PP_SEQ_FOR_EACH( _DMibDefineDynamicLibraryClass_Fetch, 0, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__) ) \
		} \
	};

namespace NMib
{

	DMibImpErrorClass(CDynamicLibraryException, NMib::NException::CException);

	using CDLFlagUnderlaying = int32;
	enum EDLFlag : int32 // EDynamicLibraryFlag, shortened for typing.
	{
		EDLFlag_None			= 0,
		EDLFlag_NoThrow			= DMibBit(0),		// Do not throw an exception when an error is encountered, use return codes.
		EDLFlag_NoAutoLoad		= DMibBit(1),		// Do not load the library on construction.
		EDLFlag_AllowMissing	= DMibBit(2),	// Allow symbols to be missing without making the whole object bad.
	};

	template<typename t_CDerived, CDLFlagUnderlaying _Flags, typename t_CStr = NStr::CStrNonTracked, typename t_CStrTemp = NStr::CStrNonTracked>
	class TDynamicLibraryUtilityAggregate
	{
	public:

#ifndef DMibNoAggregateConstexpr
		constexpr TDynamicLibraryUtilityAggregate(EAggregateInitialization _Init)
			: mp_Filename{_Init}
			, mp_pLibrary(nullptr)
			, mp_bOK(false)
		{
		}
		TDynamicLibraryUtilityAggregate()
			: mp_pLibrary(nullptr)
			, mp_bOK(false)
		{
		}
#endif
	
		bint f_OK() const { return mp_bOK; }
		operator bool() const { return mp_bOK; }

		bint f_Reload(ch8 const* _pFileName = nullptr)
		{
			if (_pFileName)
			{
				mp_Filename.f_Clear();
				mp_Filename.f_AddStr(_pFileName);
			}
			if (mp_pLibrary)
				f_Unload();

			return fp_LoadLibrary(mp_Filename);
		}

		void f_Unload()
		{
			if (mp_pLibrary)
			{
				NSys::fg_FreeLibrary(mp_pLibrary);
				mp_pLibrary = nullptr;
			}

			fp_ClearSymbols();
		}

	protected:
		t_CStr mp_Filename;
		void* mp_pLibrary;
		bint mp_bOK;

	protected:

		void fp_ClearSymbols()
		{
			// Don't try this at home kids....
			// We are clearing all members of the derived class.
			uint8* pAfterThis = (uint8*)(this + 1);
			
			NMem::fg_MemClear(pAfterThis, sizeof(t_CDerived) - sizeof(TDynamicLibraryUtilityAggregate));
		}
	
		void fp_Init() 
		{
			// Should be in TDynamicLibraryUtility() that is delegated to from the other constructors,
			// but MSVC does not support it atm...
			mp_pLibrary = nullptr;
			mp_bOK = false;
			fp_ClearSymbols();
		}

		template<typename tf_CStr>
		bint fp_LoadLibrary(tf_CStr const& _Filename)
		{
			t_CStrTemp Filename = _Filename;
			t_CStrTemp CurFilename;
			while (!Filename.f_IsEmpty() && !mp_pLibrary)
			{
				CurFilename = fg_GetStrSep(Filename, ",");

				mp_pLibrary = NSys::fg_LoadLibrary(CurFilename);
			}

			if (!mp_pLibrary)
			{
				mp_bOK = false;
				if ( !(_Flags & EDLFlag_NoThrow))
					DMibImpError(CDynamicLibraryException, (typename tf_CStr::CFormat("Failed to load library from {}") << _Filename).f_GetStr());
			}
			else
			{
				mp_bOK = true;
				mp_Filename.f_Assign(CurFilename);
			}

			if (mp_bOK)
			{
				((t_CDerived*)this)->fp_FetchSymbols();
			}

			return mp_bOK;
		}

		template<typename t_CType>
		bint fp_Fetch(t_CType* & _pPtr, char const* _pName)
		{
			if (!mp_bOK)
			{
				_pPtr = nullptr;
				return false;
			}

			*(void**)&_pPtr = NSys::fg_GetLibrarySymbol(mp_pLibrary, _pName);
			if (!_pPtr)
			{
				if (! (_Flags & EDLFlag_AllowMissing) )
				{
					mp_bOK = false;
					if ( !(_Flags & EDLFlag_NoThrow))
						DMibImpError(CDynamicLibraryException, (typename t_CStr::CFormat("Failed to get symbol {}") << _pName).f_GetStr());
				}
			}

			return _pPtr ? true : false;
		}
	};
	
	template<typename t_CDerived, CDLFlagUnderlaying _Flags, typename t_CStr = NStr::CStrNonTracked, typename t_CStrTemp = NStr::CStrNonTracked>
	class TDynamicLibraryUtility : public TDynamicLibraryUtilityAggregate<t_CDerived, _Flags, t_CStr, t_CStrTemp>
	{
	public:

		~TDynamicLibraryUtility()
		{
			this->f_Unload();
			this->mp_Filename.f_Clear();
		}

		TDynamicLibraryUtility()
		{
			this->fp_Init();
		}

		TDynamicLibraryUtility(t_CStr const& _Filename)
		{
			this->fp_Init();
			this->mp_Filename = _Filename;
			if (!(_Flags & EDLFlag_NoAutoLoad))
				this->f_Reload();
		}

		TDynamicLibraryUtility(ch8 const* _pFilename)
		{
			this->fp_Init();
			this->mp_Filename = _pFilename;
			if (!(_Flags & EDLFlag_NoAutoLoad))
				this->f_Reload();
		}
	};

} // Namespace NMib
