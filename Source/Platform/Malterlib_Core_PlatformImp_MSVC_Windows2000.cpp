// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#ifndef NTDDI_VERSION
#define _WIN32_WINNT _WIN32_WINNT_WINXP
#define NTDDI_VERSION NTDDI_WINXPSP2
#endif

#include <Mib/Core/Core>
#include <Mib/Container/BitArrayHierarchical>

#include "Windows.h"

#define DMibAllowCodeStandardViolations 1

#ifndef FLS_OUT_OF_INDEXES
#define FLS_OUT_OF_INDEXES ((DWORD)0xFFFFFFFF)
#endif

// Disable this support for now
#if 0
// This sections make the executable runnable on Windows 2000 and Windows XP before SP2. The EncodePointer and DecodePointer functions are used by the CRT, and here we provide a default implementation if they are not available in kernel32.dll
#ifndef _M_X64
extern "C"
{
#pragma warning(disable: 4273)

	static PVOID WINAPI LocalEncodePointer(PVOID Ptr)
	{
		return Ptr;
	}

	static PVOID WINAPI LocalDecodePointer(PVOID Ptr)
	{
		return Ptr;
	}
	PVOID WINAPI _imp__EncodePointer(PVOID Ptr);
	PVOID WINAPI _imp__DecodePointer(PVOID Ptr);

#pragma section("writeblefunctions", read, execute)
//#pragma code_seg(push, r1, ".data")
#pragma alloc_text("writeblefunctions", _imp__EncodePointer)
#pragma alloc_text("writeblefunctions", _imp__DecodePointer)
#pragma optimize("", off)
	// Use differente xor in functions to stop comdat folding of functions.
	PVOID WINAPI _imp__EncodePointer(PVOID Ptr)
	{
		// Testing
		Ptr = (void *)~(size_t)DMibPLine;
		return Ptr;
	}

	PVOID WINAPI _imp__DecodePointer(PVOID Ptr)
	{
		Ptr = (void *)~(size_t)DMibPLine;
		return Ptr;
	}
#pragma optimize("", on)
//#pragma code_seg(pop, r1)

	

//	__out_opt PVOID (WINAPI *_imp__EncodePointer)(__in_opt PVOID Ptr) = &EncodePointer;
//	__out_opt PVOID (WINAPI *_imp__DecodePointer)(__in_opt PVOID Ptr) = &DecodePointer;

//	__out_opt PVOID (WINAPI *__imp__EncodePointer@4)(__in_opt PVOID Ptr) = &EncodePointer;
	//__out_opt PVOID (WINAPI *__imp__DecodePointer)(__in_opt PVOID Ptr) = &DecodePointer;


}

#endif

void __cdecl fg_FixFunctionPointers()
{
#ifndef _M_X64
	HMODULE hKernel32 = GetModuleHandle(str_utf16("kernel32"));
	DWORD OldProtect = 0;

	VirtualProtect(NMib::fg_AlignDown(NMib::NMisc::fg_FunctionPtrToVoidPtr(_imp__EncodePointer), 4096), 4096, PAGE_READWRITE, &OldProtect);
	void * &pEncodePointer = (void * &)(_imp__EncodePointer);
	pEncodePointer = GetProcAddress(hKernel32, "EncodePointer");
	if (!pEncodePointer)
		pEncodePointer = NMib::NMisc::fg_FunctionPtrToVoidPtr(&LocalEncodePointer);
	VirtualProtect(NMib::fg_AlignDown(NMib::NMisc::fg_FunctionPtrToVoidPtr(_imp__EncodePointer), 4096), 4096, PAGE_READONLY, &OldProtect);

	VirtualProtect(NMib::fg_AlignDown(NMib::NMisc::fg_FunctionPtrToVoidPtr(_imp__DecodePointer), 4096), 4096, PAGE_READWRITE, &OldProtect);
	void * &pDecodePointer = (void * &)(_imp__DecodePointer);
	pDecodePointer = GetProcAddress(hKernel32, "DecodePointer");
	if (!pDecodePointer)
		pDecodePointer = NMib::NMisc::fg_FunctionPtrToVoidPtr(&LocalDecodePointer);
	VirtualProtect(NMib::fg_AlignDown(NMib::NMisc::fg_FunctionPtrToVoidPtr(_imp__DecodePointer), 4096), 4096, PAGE_READONLY, &OldProtect);
#endif		
}
#endif
#if _MSC_VER >= 1700 && (!defined(DMibDebug) || !defined(_M_X64))
// Support Windows XP
// This sections make the executable runnable on Windows 2000 and Windows XP before SP2. The EncodePointer and DecodePointer functions are used by the CRT, and here we provide a default implementation if they are not available in kernel32.dll

struct CFlsEmulator
{
	NMib::NThread::CMutual m_Lock;
	NMib::NContainer::TCBitArrayHierarchical<128> m_Allocated;
	struct CAllocator
	{
		PFLS_CALLBACK_FUNCTION m_fCallback;
		struct CThreadLocal
		{
			void *m_pValue;
			PFLS_CALLBACK_FUNCTION m_fCallback;
			CThreadLocal()
				: m_fCallback(nullptr)
				, m_pValue(nullptr)
			{
			}
			~CThreadLocal()
			{
				if (m_fCallback && m_pValue)
					m_fCallback(m_pValue);
			}
		};
		NMib::NThread::TCThreadLocal<CThreadLocal, NMib::NMem::CAllocator_Heap, NMib::NThread::EThreadLocalFlag_AlwaysCreated> m_ThreadLocal;
	};

	NMib::NAggregate::TCAggregateSimple<CAllocator> m_Values[128];
};

NMib::NAggregate::TCAggregate<CFlsEmulator, 0> g_FlsEmulator = {0};

extern "C"
{
#pragma warning(disable: 4273)

#pragma section("writeblefunctions", read, execute)

#ifdef DConfig_Release
#define DAlwaysEmulate false
#else
#define DAlwaysEmulate false
#endif

	// Boilerplate
#if defined(_M_X64) && !defined(DMibLinkTimeCodeGeneration)
#define DFunctionPointerName(_Name) __imp_##_Name
#else
#define DFunctionPointerName(_Name) _imp__##_Name
#endif

#define DDefineFunction(_Name, _Ret, _Params) \
	_Ret WINAPI DFunctionPointerName(_Name) _Params;\
	__pragma(alloc_text("writeblefunctions", DFunctionPointerName(_Name)))\
	__pragma(optimize("", off))\
	inline_never _Ret WINAPI DFunctionPointerName(_Name) _Params\
	{\
		DWORD x = ~(DWORD)DMibPLine;\
		return (_Ret)(mint)x;\
	}\
	__pragma(optimize("", on))\
	static _Ret WINAPI fg_##_Name _Params


//			OutputDebugString((NMib::NStr::CFWStr256::CFormat(str_utf16("Saving in pointer: {}\r\n")) << pEncodePointer).f_GetStr());

#define DImplementFunction(_Name) \
	{\
		void **pEncodePointer = (void **)NMib::NMisc::fg_FunctionPtrToVoidPtr(DFunctionPointerName(_Name));\
		uint8 *pAddress = NMib::fg_AlignDown((uint8 *)pEncodePointer, 4096);\
		uint8 *pEnd = NMib::fg_AlignUp((uint8 *)pEncodePointer + sizeof(void *), 4096);\
		VirtualProtect(pAddress, pEnd - pAddress, PAGE_READWRITE, &OldProtect);\
		*pEncodePointer = GetProcAddress(hKernel32, #_Name);\
		bEmulated = false;\
		if (!*pEncodePointer || DAlwaysEmulate)\
		{\
			bEmulated = true;\
			*pEncodePointer = NMib::NMisc::fg_FunctionPtrToVoidPtr(&fg_##_Name);\
		}\
		VirtualProtect(pAddress, pEnd - pAddress, OldProtect, &OldProtect);\
	}


#define DReplaceFunction(_Name, _Function) \
	{\
		void **pEncodePointer = (void **)NMib::NMisc::fg_FunctionPtrToVoidPtr(DFunctionPointerName(_Name));\
		uint8 *pAddress = NMib::fg_AlignDown((uint8 *)pEncodePointer, 4096);\
		uint8 *pEnd = NMib::fg_AlignUp((uint8 *)pEncodePointer + sizeof(void *), 4096);\
		VirtualProtect(pAddress, pEnd - pAddress, PAGE_READWRITE, &OldProtect);\
		*pEncodePointer = NMib::NMisc::fg_FunctionPtrToVoidPtr(&_Function);\
		VirtualProtect(pAddress, pEnd - pAddress, OldProtect, &OldProtect);\
	}

	DDefineFunction(FlsAlloc, DWORD, (PFLS_CALLBACK_FUNCTION lpCallback))
	{
		auto &Emulator = *g_FlsEmulator;
		DWORD Ret = 0;
		{
			DMibLock(Emulator.m_Lock);
			aint iBit = Emulator.m_Allocated.f_FindFreeBitAndSet();
			if (iBit < 0)
				return FLS_OUT_OF_INDEXES;
			Ret = iBit;
			auto &Value = Emulator.m_Values[iBit];
			Value.f_Construct();
			Value->m_fCallback = lpCallback;
		}

		return Ret;
	}

	DDefineFunction(FlsFree, BOOL, (DWORD dwFlsIndex))
	{
		auto &Emulator = *g_FlsEmulator;
		{
			DMibLock(Emulator.m_Lock);
			auto &Value = Emulator.m_Values[dwFlsIndex];
			Value.f_Destruct();
			Emulator.m_Allocated.f_SetBit(dwFlsIndex, 0);
		}

		return true;
	}

	DDefineFunction(FlsGetValue, PVOID, (DWORD dwFlsIndex))
	{
		auto &Emulator = *g_FlsEmulator;
		auto &Value = Emulator.m_Values[dwFlsIndex];
		auto &ThreadLocal = *(Value->m_ThreadLocal);
		return ThreadLocal.m_pValue;
	}

	DDefineFunction(FlsSetValue, BOOL, (DWORD dwFlsIndex, PVOID lpFlsData))
	{
		auto &Emulator = *g_FlsEmulator;
		auto &Value = Emulator.m_Values[dwFlsIndex];
		auto &ThreadLocal = *(Value->m_ThreadLocal);
		ThreadLocal.m_pValue = lpFlsData;
		ThreadLocal.m_fCallback = Value->m_fCallback;
		return true;
	}

	DDefineFunction(GetUserDefaultLocaleName, int, (LPWSTR lpLocaleName, int cchLocaleName))
	{
		LCID DefaultLCID = GetUserDefaultLCID();
		NMib::NStr::CFWStr16 Format = NMib::NStr::CFWStr16::CFormat(str_utf16("{}")) << DefaultLCID;
		if (!lpLocaleName || cchLocaleName < Format.f_GetLen() + 1)
		{
			SetLastError(ERROR_INSUFFICIENT_BUFFER);
			return false;
		}
		NMib::NStr::fg_StrCopy(lpLocaleName, Format, cchLocaleName);
		return true;
	}

	LCID fg_ParseLocaleID(LPCWSTR lpLocaleName)
	{
		if (lpLocaleName == nullptr)
			return LOCALE_USER_DEFAULT;
		else if (NMib::NStr::fg_StrCmp(lpLocaleName, str_utf16("")) == 0)
			return LOCALE_SYSTEM_DEFAULT;
		else if (NMib::NStr::fg_StrCmp(lpLocaleName, str_utf16("!x-sys-default-locale")) == 0)
			return LOCALE_INVARIANT;
		return NMib::NStr::fg_StrToInt(lpLocaleName, LCID(-1));
	}

	DDefineFunction(IsValidLocaleName, BOOL, (LPCWSTR lpLocaleName))
	{
		LCID NameID = fg_ParseLocaleID(lpLocaleName);
		return IsValidLocale(NameID, LCID_INSTALLED | LCID_SUPPORTED);
	}

	DDefineFunction(LCMapStringEx, int, (LPCWSTR lpLocaleName, DWORD dwMapFlags, LPCWSTR lpSrcStr, int cchSrc, LPWSTR lpDestStr, int cchDest, LPNLSVERSIONINFO lpVersionInformation, LPVOID lpReserved, LPARAM sortHandle))
	{
		LCID NameID = fg_ParseLocaleID(lpLocaleName);
		return LCMapString(NameID, dwMapFlags, lpSrcStr, cchSrc, lpDestStr, cchDest);
	}

	DDefineFunction(GetLocaleInfoEx, int, (LPCWSTR lpLocaleName, LCTYPE LCType, LPWSTR lpLCData, int cchData))
	{
		LCID NameID = fg_ParseLocaleID(lpLocaleName);
		return GetLocaleInfo(NameID, LCType, lpLCData, cchData);
	}

	DDefineFunction(GetTickCount64, ULONGLONG, ())
	{
		return GetTickCount();
	}

	typedef BOOL (CALLBACK* LOCALE_ENUMPROCEX)(LPWSTR, DWORD, LPARAM);

	struct CTempParams
	{
		LPARAM lParam;
		LPVOID lpReserved;
		LOCALE_ENUMPROCEX lpLocaleEnumProcEx;
		DWORD dwFlags;
	};
	NMib::NAggregate::TCAggregate<CTempParams> g_EnumSystemLocalesExParams = {0};
	BOOL CALLBACK fg_LocaleEnum(LPWSTR _pLocale)
	{
		auto &Params = (*g_EnumSystemLocalesExParams);

		return Params.lpLocaleEnumProcEx(_pLocale, Params.lParam, Params.dwFlags);
	}

	DDefineFunction(EnumSystemLocalesEx, BOOL, (LOCALE_ENUMPROCEX lpLocaleEnumProcEx, DWORD dwFlags, LPARAM lParam, LPVOID lpReserved))
	{
		auto &Params = (*g_EnumSystemLocalesExParams);
		Params.lParam = lParam;
		Params.lpReserved = lpReserved;
		Params.dwFlags = dwFlags;
		Params.lpLocaleEnumProcEx = lpLocaleEnumProcEx;
		return EnumSystemLocales(&fg_LocaleEnum, dwFlags);
	}

	DDefineFunction(CompareStringEx, int, (LPCWSTR lpLocaleName, DWORD dwCmpFlags, LPCWSTR lpString1, int cchCount1, LPCWSTR lpString2, int cchCount2, LPNLSVERSIONINFO lpVersionInformation, LPVOID lpReserved, LPARAM lParam))
	{
		LCID NameID = fg_ParseLocaleID(lpLocaleName);
		return CompareString(NameID, dwCmpFlags, lpString1, cchCount1, lpString2, cchCount2);
	}

	DDefineFunction(GetTimeFormatEx, int, (LPCWSTR lpLocaleName, DWORD dwFlags, CONST SYSTEMTIME * lpTime, LPCWSTR lpFormat, LPWSTR lpTimeStr, int cchTime))
	{
		LCID NameID = fg_ParseLocaleID(lpLocaleName);
		return GetTimeFormat(NameID, dwFlags, lpTime, lpFormat, lpTimeStr, cchTime);
	}

	DDefineFunction(GetDateFormatEx, int, (LPCWSTR lpLocaleName, DWORD dwFlags, CONST SYSTEMTIME * lpDate, LPCWSTR lpFormat, LPWSTR lpDateStr, int cchDate, LPCWSTR lpCalendar))
	{
		LCID NameID = fg_ParseLocaleID(lpLocaleName);
		return GetDateFormat(NameID, dwFlags, lpDate, lpFormat, lpDateStr, cchDate);
	}

	DDefineFunction(InitializeCriticalSectionEx, BOOL, (LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount, DWORD Flags))
	{
		return InitializeCriticalSectionAndSpinCount(lpCriticalSection, dwSpinCount);
	}

	DDefineFunction(InitOnceExecuteOnce, BOOL, (PINIT_ONCE InitOnce, PINIT_ONCE_FN InitFn, PVOID Parameter, LPVOID *Context))
	{
		NMib::NAtomic::TCAtomicAggregate<mint> *pInit = (NMib::NAtomic::TCAtomicAggregate<mint> *)(&(InitOnce->Ptr));
		
		if ((pInit->f_FetchOr(1) & 1) == 0)
		{
			// We are the one initializining

			InitFn(InitOnce, Parameter, Context);

			// Initialized
			if (Context)
				pInit->f_FetchOr(mint(2) | (mint)*Context);
			else
				pInit->f_FetchOr(mint(2));
		}
		else
		{
			// We are not initializing
			while (1)
			{
				if (pInit->f_Load() & 2)
					break; // Initializied

				NMib::NSys::fg_Thread_SmallestSleep();
			}
			if (Context)
				*Context = (void *)(pInit->f_Load() & ~mint(3));
		}
		
		return true;
	}

}

DWORD WINAPI fg_FlsAllocFast(PFLS_CALLBACK_FUNCTION lpCallback)
{
	auto &Emulator = g_FlsEmulator.f_GetUnsafe();
	DWORD Ret = 0;
	{
		DMibLock(Emulator.m_Lock);
		aint iBit = Emulator.m_Allocated.f_FindFreeBitAndSet();
		if (iBit < 0)
			return FLS_OUT_OF_INDEXES;
		Ret = iBit;
		auto &Value = Emulator.m_Values[iBit];
		Value.f_Construct();
		auto &AllocValue = *Value;
		AllocValue.m_fCallback = lpCallback;
	}

	return Ret;
}

BOOL WINAPI fg_FlsFreeFast(DWORD dwFlsIndex)
{
	auto &Emulator = g_FlsEmulator.f_GetUnsafe();
	{
		DMibLock(Emulator.m_Lock);
		auto &Value = Emulator.m_Values[dwFlsIndex];
		Value.f_Destruct();
		Emulator.m_Allocated.f_SetBit(dwFlsIndex, 0);
	}

	return true;
}

PVOID WINAPI fg_FlsGetValueFast(DWORD dwFlsIndex)
{
	auto &Emulator = g_FlsEmulator.f_GetUnsafe();
	auto &Value = Emulator.m_Values[dwFlsIndex];
	auto &ThreadLocal = *(Value->m_ThreadLocal);
	return ThreadLocal.m_pValue;
}

BOOL WINAPI fg_FlsSetValueFast(DWORD dwFlsIndex, PVOID lpFlsData)
{
	auto &Emulator = g_FlsEmulator.f_GetUnsafe();
	auto &Value = Emulator.m_Values[dwFlsIndex];
	bool bIsValid = Value->m_ThreadLocal.f_IsValid();
	auto &ThreadLocal = *(Value->m_ThreadLocal);
	ThreadLocal.m_pValue = lpFlsData;
	ThreadLocal.m_fCallback = Value->m_fCallback;
	return true;
}


bint g_IsFlsEmulated = false;
void __cdecl fg_FixFunctionPointers()
{
	HMODULE hKernel32 = GetModuleHandle(str_utf16("kernel32"));
	DWORD OldProtect = 0;
	bool bEmulated = false;

	DImplementFunction(FlsAlloc);
	if (bEmulated)
		g_IsFlsEmulated = true;
	DImplementFunction(FlsFree);
	DImplementFunction(FlsGetValue);
	DImplementFunction(FlsSetValue);
	DImplementFunction(GetUserDefaultLocaleName);
	DImplementFunction(IsValidLocaleName);
	DImplementFunction(LCMapStringEx);
	DImplementFunction(GetLocaleInfoEx);
	DImplementFunction(GetTickCount64);
	DImplementFunction(EnumSystemLocalesEx);
	DImplementFunction(CompareStringEx);	
	DImplementFunction(GetTimeFormatEx);
	DImplementFunction(GetDateFormatEx);
	DImplementFunction(InitializeCriticalSectionEx);
	DImplementFunction(InitOnceExecuteOnce);
	 
}

void __cdecl fg_FixFunctionPointers_Alloc()
{
	if (g_IsFlsEmulated)
	{
		*g_FlsEmulator;
		HMODULE hKernel32 = GetModuleHandle(str_utf16("kernel32"));
		DWORD OldProtect = 0;
		DReplaceFunction(FlsAlloc, fg_FlsAllocFast);
		DReplaceFunction(FlsFree, fg_FlsFreeFast);
		DReplaceFunction(FlsGetValue, fg_FlsGetValueFast);
		DReplaceFunction(FlsSetValue, fg_FlsSetValueFast);
	}
}
#else
void __cdecl fg_FixFunctionPointers()
{
}
void __cdecl fg_FixFunctionPointers_Alloc()
{
}

#endif

extern "C"
{
WINBASEAPI
DWORD
WINAPI
FlsAlloc(
    _In_opt_ PFLS_CALLBACK_FUNCTION lpCallback
    );


WINBASEAPI
PVOID
WINAPI
FlsGetValue(
    _In_ DWORD dwFlsIndex
    );


WINBASEAPI
BOOL
WINAPI
FlsSetValue(
    _In_ DWORD dwFlsIndex,
    _In_opt_ PVOID lpFlsData
    );


WINBASEAPI
BOOL
WINAPI
FlsFree(
    _In_ DWORD dwFlsIndex
    );
}

DWORD winFlsAlloc(PFLS_CALLBACK_FUNCTION lpCallback)
{
	return FlsAlloc(lpCallback);
}

BOOL winFlsFree(DWORD dwFlsIndex)
{
	return FlsFree(dwFlsIndex);
}

PVOID winFlsGetValue(DWORD dwFlsIndex)
{
	return FlsGetValue(dwFlsIndex);
}

BOOL winFlsSetValue(DWORD dwFlsIndex, PVOID lpFlsData)
{
	return FlsSetValue(dwFlsIndex, lpFlsData);
}


