// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define _CRT_DECLARE_GLOBAL_VARIABLES_DIRECTLY

#pragma warning(disable:4996)
#pragma warning(disable:4091)
#define PSAPI_VERSION 1


#include <Mib/Core/Core>

#include "Malterlib_Core_PlatformImp_MSVC_WindowsDefines.h"
#include <wincrypt.h>

#include <Mib/Cryptography/UUID>
#include <Mib/Cryptography/RandomID>
#include <Mib/Cryptography/Hashes/SHA>
#include <Mib/Core/PlatformSpecific/WindowsFilePath>
#include <Mib/Core/PlatformSpecific/WindowsError>
#include <Mib/Core/PlatformSpecific/WindowsRegistry>
#include <Mib/Core/PlatformSpecific/WindowsOptional>
#include <Mib/Core/PlatformSpecific/WindowsFile>
#include <Mib/Core/PlatformSpecific/WindowsInject>
#include <Mib/Core/PlatformSpecific/Windows>
#include <TlHelp32.h>

using namespace NMib;
using namespace NMib::NMemory;
using namespace NMib::NStr;
using namespace NMib::NContainer;
using namespace NMib::NIntrusive;
using namespace NMib::NStorage;
using namespace NMib::NAtomic;
using namespace NMib::NNetwork;
using namespace NMib::NThread;
using namespace NMib::NMisc;
using namespace NMib::NSystem;
using namespace NMib::NFile;
using namespace NMib::NException;
using namespace NMib::NTime;
using namespace NMib::NFunction;
using namespace NMib::NStorage;

HINSTANCE g_hDllInstance = 0;
bool g_bIsDll = false;

static NAtomic::TCAtomicAggregate<smint> gs_LibraryRefCount = {mint(smint(-1))};
static mint gs_ThreadLocalParentThread = 0xFFFFFFFF;

VOID (WINAPI *g_fOrgExitProcess)(__in  UINT _ExitCode) = nullptr;
BOOL (WINAPI *g_fOrgTerminateProcess)(__in  HANDLE _hProcess, __in  UINT _ExitCode) = nullptr;


extern NAtomic::TCAtomicAggregate<smint> g_bDoneMalterlibInitAll;

#pragma warning(disable:4344)

#undef int


HINSTANCE fg_Win32_GetInstance(const void *_pCode);

namespace NMib::NPlatform
{
	extern void fg_GenerateExcetionHandler(void *_pData, LONG (*_fCallback)(struct _EXCEPTION_POINTERS *_pExceptionInfo, void *_pData));

	void *fg_GetWindowsDllInstance()
	{
		return g_hDllInstance;
	}
}


#ifdef DArchitecture_x64
#define DDefaultCallingConv 
#else
#define DDefaultCallingConv __cdecl
#endif

#ifdef DArchitecture_x64
extern "C" mint fg_MalterlibGetFramePtr_X86_64();
extern "C" mint fg_MalterlibGetRDTSC_X86_64();
extern "C" uint32 fg_MalterlibGetCurrentThreadID_X86_64();
#endif

//#define DEnableTlsFastThreadLocal

#define DMibSystemVersion "1_0_0"

#if DMibConfig_MalterlibMemoryManager_Debug
#define DMibSystemManagerPrefix "Debug" DMibSystemVersion
#else
#define DMibSystemManagerPrefix "Release" DMibSystemVersion
#endif

static mint fsg_GetStackTrace(mint *_pStack, mint _nMaxDepth, mint _StackFrame);


//#include "sal.h"
extern "C" void DDefaultCallingConv fg_MalterlibInitStdLib();

#define DMibGuid "213391DE-87DB-4c39-9095-A31E6F14625B"

static SYSTEM_INFO gs_SysInfo;         // useful information about the system

namespace NLocal
{
	#define PROCESS_CALLBACK_FILTER_ENABLED     0x1

	constinit COptionalFunctions g_OptionalFunctions = {};
	constinit OSVERSIONINFOEX g_VersionInfo = {0};
}

static inline_small class CSystemWindowsMSVC *fg_GetLocalSys();
//static inline_small class CSharedSystemWindowsMSVC &GetLocalSharedSys();

void fg_DestroySystem();
void __cdecl fg_DestroyMalterlib();
void __cdecl fg_DestroyMalterlibAggregates();
void __cdecl fg_CreateMalterlib();


class CWin32File
{
public:
	virtual ~CWin32File()
	{
	}
	void *m_pFile;
	NMib::NFile::EFileOpen m_Flags;
	virtual CWStrPtr f_GetName() const = 0;
	virtual void f_Delete() = 0;
	virtual bool f_IsNonTracked() = 0;
};

template <typename t_CStr, typename t_CAllocator>
class TCWin32File : public CWin32File
{
	t_CStr m_FileName;
public:
	TCWin32File(t_CStr const &_FileName)
		: m_FileName(_FileName)
	{
	}
	virtual CWStrPtr f_GetName() const override
	{
		CWStrPtr Ret;
		Ret.f_SetConstPtr(m_FileName.f_GetStr(), m_FileName.f_GetLen());
		return Ret;
	}
	virtual void f_Delete() override
	{
		TCUniquePointer<TCWin32File, t_CAllocator> pPtr = fg_Explicit(this);
	}
	virtual bool f_IsNonTracked() override
	{
		return !t_CAllocator::mc_Reporting;
	}
};

namespace
{
	inline_small bool fg_IsGoodStackPtr(void *_pAddr, mint _Len, mint _StackStart, mint _StackEnd)
	{
		mint StackStart = _StackStart;
		mint StackEnd = _StackEnd;
		mint AddrStart = (mint)_pAddr;
		mint AddrEnd = AddrStart + _Len;

		if (AddrEnd < AddrStart)
			return false;

		return AddrEnd <= StackStart && AddrStart >= StackEnd;
	}
}


#include "Malterlib_Core_PlatformImp_MSVC_LocalSys.cpp"
#include "Malterlib_Core_PlatformImp_MSVC_NTSpecific.cpp"
#include "Malterlib_Core_PlatformImp_MSVC_CPUUsageMonitor.cpp"

// Note: These needs to be nade exactly like this to be compatible with old version of library (when Malterlib was named Ids)
#if defined(DArchitecture_x64) || defined(DArchitecture_arm64)
	#pragma comment(linker, "/export:IdsFreeLibraryExternal=fg_IdsFreeLibraryExternal")
	#pragma comment(linker, "/export:IdsLoadLibraryExternal=fg_IdsLoadLibraryExternal")
#else
	#pragma comment(linker, "/export:IdsFreeLibraryExternal=_fg_IdsFreeLibraryExternal")
	#pragma comment(linker, "/export:IdsLoadLibraryExternal=_fg_IdsLoadLibraryExternal")
#endif

extern "C" void __cdecl fg_IdsFreeLibraryExternal()
{
	if (--gs_LibraryRefCount == 0)
		fg_GetLocalSys()->f_DestroyThreadSpecific();
}

extern "C" void __cdecl fg_IdsLoadLibraryExternal()
{
	++gs_LibraryRefCount;
}

namespace NMib
{
	namespace NSys
	{
		static LPTOP_LEVEL_EXCEPTION_FILTER g_pExpectedFilter = nullptr;
		void fg_Windows_ExpectedFilter(LPTOP_LEVEL_EXCEPTION_FILTER _pFilter)
		{
			g_pExpectedFilter = _pFilter;
		}
	}
}

void* NSys::fg_LoadLibrary(CFStr256 const& _Library)
{
	if (!_Library.f_IsEmpty())
	{
		CFWStr256 LibPath = NMib::NFile::NPlatform::fg_ConvertToWindowsPath<CFWStr256, CFWStr256>(_Library, false);

		void *pRet = LoadLibraryW(LibPath.f_GetStr());
		if (pRet)
		{
			void (__cdecl *pMalterlibLoadLibraryExternal)();
			pMalterlibLoadLibraryExternal = (void (__cdecl *)())GetProcAddress((HMODULE)pRet, "IdsLoadLibraryExternal");
			if (pMalterlibLoadLibraryExternal)
				pMalterlibLoadLibraryExternal();
		}

		
		if (g_pExpectedFilter)
		{
			LPTOP_LEVEL_EXCEPTION_FILTER pFilterNew = SetUnhandledExceptionFilter(g_pExpectedFilter);
			if (pFilterNew != g_pExpectedFilter)
			{
				DMibDTrace("---------------------------------------- Restored unhandled exception filter after DLL load\n", 0);
			}
		}
		return pRet;
	}
	else
	{
		HMODULE Module;
		// NOTE: GetModuleHandleEx is used over GetModuleHandle as it increments the modules ref-count and we want that so
		// behaviour is the same across platforms (dlopen increments the ref of the exe as well).
		if (GetModuleHandleExW(0, NULL, &Module))
			return Module;
		else
			return nullptr;
	}
}


void *NSys::fg_LoadLibrary(const CStr& _Library)
{
	if (!_Library.f_IsEmpty())
	{
		CWStr LibPath = NMib::NFile::NPlatform::fg_ConvertToWindowsPath(_Library, false);

		void *pRet = LoadLibraryW(LibPath.f_GetStr());
		if (pRet)
		{
			void (__cdecl *pMalterlibLoadLibraryExternal)();
			pMalterlibLoadLibraryExternal = (void (__cdecl *)())GetProcAddress((HMODULE)pRet, "IdsLoadLibraryExternal");
			if (pMalterlibLoadLibraryExternal)
				pMalterlibLoadLibraryExternal();
		}

		if (g_pExpectedFilter)
		{
			LPTOP_LEVEL_EXCEPTION_FILTER pFilterNew = SetUnhandledExceptionFilter(g_pExpectedFilter);
			if (pFilterNew != g_pExpectedFilter)
			{
				DMibDTrace("---------------------------------------- Restored unhandled exception filter after DLL load\n", 0);
			}
		}
		return pRet;
	}
	else
	{
		HMODULE Module;
		// NOTE: GetModuleHandleEx is used over GetModuleHandle as it increments the modules ref-count and we want that so
		// behaviour is the same across platforms (dlopen increments the ref of the exe as well).
		if (GetModuleHandleExW(0, NULL, &Module))
			return Module;
		else
			return nullptr;
	}
}

void *NSys::fg_LoadLibrary(const CStrNonTracked& _Library)
{
	if (!_Library.f_IsEmpty())
	{
		CWStrNonTracked LibPath = NMib::NFile::NPlatform::fg_ConvertToWindowsPath<CWStrNonTracked, CWStrNonTracked>(_Library, false);

		void *pRet = LoadLibraryW(LibPath.f_GetStr());
		if (pRet)
		{
			void (__cdecl *pMalterlibLoadLibraryExternal)();
			pMalterlibLoadLibraryExternal = (void (__cdecl *)())GetProcAddress((HMODULE)pRet, "IdsLoadLibraryExternal");
			if (pMalterlibLoadLibraryExternal)
				pMalterlibLoadLibraryExternal();
		}

		if (g_pExpectedFilter)
		{
			LPTOP_LEVEL_EXCEPTION_FILTER pFilterNew = SetUnhandledExceptionFilter(g_pExpectedFilter);
			if (pFilterNew != g_pExpectedFilter)
			{
				DMibDTrace("---------------------------------------- Restored unhandled exception filter after DLL load\n", 0);
			}
		}
		return pRet;
	}
	else
	{
		HMODULE Module;
		// NOTE: GetModuleHandleEx is used over GetModuleHandle as it increments the modules ref-count and we want that so
		// behaviour is the same across platforms (dlopen increments the ref of the exe as well).
		if (GetModuleHandleExW(0, NULL, &Module))
			return Module;
		else
			return nullptr;
	}
}

void NSys::fg_FreeLibrary(void *_pModule)
{

	void (__cdecl *pMalterlibFreeLibraryExternal)();
	pMalterlibFreeLibraryExternal = (void (__cdecl *)())GetProcAddress((HMODULE)_pModule, "IdsFreeLibraryExternal");
	if (pMalterlibFreeLibraryExternal)
		pMalterlibFreeLibraryExternal();
	FreeLibrary((HMODULE)_pModule);

	if (g_pExpectedFilter)
	{
		LPTOP_LEVEL_EXCEPTION_FILTER pFilterNew = SetUnhandledExceptionFilter(g_pExpectedFilter);
		if (pFilterNew != g_pExpectedFilter)
		{
			DMibDTrace("--------------------------------------- Restored unhandled exception filter after DLL unload\n", 0);
		}
	}
}

void* NSys::fg_GetLibrarySymbol(void* _pModule, char const* _pSymbol)
{
	return GetProcAddress((HMODULE)_pModule, _pSymbol);
}

void* NSys::fg_GetExeData(char const* _pSegment, char const* _pSection, unsigned long long& _nDataBytes)
{
	return nullptr;
}

bool NSys::fg_System_BeingDebugged()
{
	UndocumentedPEB *pPeb = fg_GetPEB(fg_GetTEB());
	return pPeb->BeingDebugged != false;
}

inline_never bool NSys::fg_Compiler_AlwaysFalse()
{
	return false;
}

void (* volatile g_pFuncMakeActive)(const void *_pReference) = nullptr;

inline_never assure_used bool NSys::fg_Compiler_MakeActive(const void *_pReference)
{
	if (g_pFuncMakeActive)
		g_pFuncMakeActive(_pReference);
	return true;
}

class CMemoryToucher : public CVirtualDestructor
{
	class CInternal;
	TCUniquePointer<CInternal> m_pInternal;
public:
	CMemoryToucher(fp64 _CPUUsage);
	~CMemoryToucher();		
};


void NSys::fg_Mem_EnableMemoryToucher(bool _bEnabled, fp64 _CPUUsage)
{
	auto pLocalSys = fg_GetLocalSys();
	DMibLock(pLocalSys->m_MemoryToucherLock);
	if (_bEnabled)
		pLocalSys->m_pMemoryToucher = fg_Construct<CMemoryToucher>(_CPUUsage);
	else
		pLocalSys->m_pMemoryToucher.f_Clear();
}

namespace
{
	template <typename tf_CStrType>
	tf_CStrType fg_RemoveEscapeWinPath(tf_CStrType &_Str)
	{
		tf_CStrType Ret;
		const typename tf_CStrType::CChar *pParse = _Str;
		int Mode = 0;
		while (*pParse)
		{
			if (Mode == 0)
			{
				if (*pParse == '"')
				{
					Mode = 1;
					++pParse;
					continue;
				}
			}
			else if (Mode == 1)
			{
				if (*pParse == '"')
				{
					Mode = 0;
					++pParse;
					continue;
				}
			}
			Ret.f_AddChar(*pParse);
			++pParse;
		}
		return Ret;
	}

	template <typename tf_CStrType>
	tf_CStrType fg_GetWinPathSepEscaped(tf_CStrType &_Str, const ch8 *_pSep)
	{
		tf_CStrType Ret;
		const typename tf_CStrType::CChar *pParse = _Str;
		int32 Pos = 0;
		if (pParse[Pos] == '"')
		{
			Pos++;
			while (pParse[Pos])
			{		
				if (pParse[Pos] == '"')
					break;
				Pos++;
			}
		}

		aint iFind = fg_StrFind(pParse + Pos, _pSep);
		if (iFind >= 0)
		{
			Ret = _Str.f_Left(iFind + Pos);
			_Str = _Str.f_Extract(iFind + 1 + Pos);
		}
		else
		{
			Ret = _Str;
			_Str = "";
		}
		Ret = Ret.f_TrimLeft();
		Ret = Ret.f_TrimRight();
		if (Ret[0] == '"')
		{
			Ret = fg_RemoveEscapeWinPath(Ret);
		}
		return Ret;
	}
}

[[maybe_unused]] static mint fsg_GetStackTrace(mint *_pStack, mint _nMaxDepth, mint _StackFrame)
{
	CUndocumentedTEB *pTEB = fg_GetTEB();
	mint StackStart = (mint)pTEB->Tib.StackBase;
	mint StackEnd = (mint)pTEB->Tib.StackLimit;

	mint *pStack = _pStack;

	try
	{
		mint StackFrame = _StackFrame;
		while (_nMaxDepth)
		{
			if (!fg_IsGoodStackPtr((void *)StackFrame, sizeof(mint) * 2, StackStart, StackEnd))
				break;
			*pStack = *((mint *)(StackFrame + sizeof(mint)));
			++pStack;
			StackFrame = *((mint *)(StackFrame));
			--_nMaxDepth;			
		}
	}
	catch(...)
	{
	}
	if (pStack != _pStack && !pStack[-1])
		return (pStack-1) - _pStack;
	else
		return pStack - _pStack;
}

#include "winnt.h"

inline_never mint NSys::fg_System_GetStackTrace(CMibCodeAddress *_pStack, mint _nMaxDepth)
{
#if defined(DArchitecture_x64) || defined(DArchitecture_arm64)

	if (NLocal::g_VersionInfo.dwMajorVersion < 0x06)
		_nMaxDepth = fg_Min(_nMaxDepth, 63);

	return RtlCaptureStackBackTrace(0, _nMaxDepth, (void **)_pStack, nullptr);
#else
	
	try
	{
		
		mint RegEBP = (mint)_ReturnAddress();
#ifdef DArchitecture_x64
		RegEBP = fg_MalterlibGetFramePtr_X86_64();
#else
		__asm
		{
			mov RegEBP, ebp
		}
#endif

		//RegEBP = *((mint *)(RegEBP));
		return fsg_GetStackTrace((mint *)_pStack, _nMaxDepth, RegEBP);
	}
	catch(...)
	{
	}
	return 0;
#endif
}

inline_never CMibCodeAddress NSys::fg_System_GetStackTrace(aint _iDepth)
{
#if defined(DArchitecture_x64) || defined(DArchitecture_arm64)
	CMibCodeAddress StackTrace[1];
	if (RtlCaptureStackBackTrace(_iDepth+1, 1, (void **)StackTrace, nullptr) > 0)
		return StackTrace[0];
	else
		return 0;
#else

	CUndocumentedTEB *pTEB = fg_GetTEB();
	mint StackStart = (mint)pTEB->Tib.StackBase;
	mint StackEnd = (mint)pTEB->Tib.StackLimit;
	mint Ret = 0;
	try
	{
		mint RegEBP;
#ifdef DArchitecture_x64
		RegEBP = fg_MalterlibGetFramePtr_X86_64();
#else
		__asm
		{
			mov RegEBP, ebp
		}
#endif

		RegEBP = *((mint *)(RegEBP));

		mint Caller = 0;
		while (_iDepth)
		{
			if (!fg_IsGoodStackPtr((void *)RegEBP, 8, StackStart, StackEnd))
				return 0;
			Caller = *((mint *)(RegEBP + 4));
			RegEBP = *((mint *)(RegEBP));
			--_iDepth;			
		}
		Ret = Caller;
	}
	catch(...)
	{
	}
	return (CMibCodeAddress)Ret;
#endif
}


void NSys::fg_DebugOutput(const ch8 *_pToOutput)
{
	ch8 Temp[2048];
	mint Len = fg_StrLen(_pToOutput);
	while (Len)
	{
		ch8 *pTemp = NMib::NStr::fg_StrCopy(Temp, _pToOutput, 2048);
		mint nChars = pTemp - Temp;
		Len -= nChars;
		_pToOutput += nChars;
		OutputDebugStringA(Temp);
	}
}

void NSys::fg_DebugOutput(const ch16 *_pToOutput)
{
	ch16 Temp[2048];
	mint Len = fg_StrLen(_pToOutput);
	while (Len)
	{
		ch16 *pTemp = NMib::NStr::fg_StrCopy(Temp, _pToOutput, 2048);
		mint nChars = pTemp - Temp;
		Len -= nChars;
		_pToOutput += nChars;
		OutputDebugStringW(Temp);
	}
}

void NSys::fg_DebugOutput(const ch32 *_pToOutput)
{
	CUStr Temp = _pToOutput;
	while (!Temp.f_IsEmpty())
	{
		CUStr ThisTime = Temp.f_Left(1024+512);
		Temp = Temp.f_Extract(1024+512);
		fg_DebugOutput(CWStr(ThisTime).f_GetStr());
	}
}


void NSys::fg_DebugOutput(const CWStrNonTracked &_Output)
{
	CWStrNonTracked Temp = _Output;
	while (!Temp.f_IsEmpty())
	{
		CWStrNonTracked ThisTime = Temp.f_Left(1024+512);
		Temp = Temp.f_Extract(1024+512);
		fg_DebugOutput(ThisTime.f_GetStr());
	}
}
void NSys::fg_DebugOutput(const CStrNonTracked &_Output)
{
	return fg_DebugOutput(CWStrNonTracked(_Output));
}
void NSys::fg_DebugOutput(const CUStrNonTracked &_Output)
{
	return fg_DebugOutput(CWStrNonTracked(_Output));
}


void NSys::fg_System_ReportContractViolation(const NMib::NStr::CStrNonTracked &_Message)
{
	fg_GetLocalSys()->f_DebugReportContractViolation(_Message);
}

NMib::NStr::CStrNonTracked NSys::fg_System_GetContractViolationMessage()
{
	return fg_GetLocalSys()->f_DebugContractViolationMessage();
}



namespace
{
	template <typename tf_CWinStr, typename tf_UTF8Str, typename tf_CStr>
	void fg_ConsoleOutputHelper(const tf_CStr &_Str, DWORD _StdHandle, bool _bRaw)
	{
		static bool bEnableTerminalProcessing = true;
		if (bEnableTerminalProcessing)
		{
			bEnableTerminalProcessing = false;
			uint32 Mode = 0;
			auto fEnableVT = [&](HANDLE _File)
				{
					if (GetConsoleMode(_File, &Mode))
					{
						Mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
						SetConsoleMode(_File, Mode);
					}
				}
			;
			fEnableVT(GetStdHandle(STD_OUTPUT_HANDLE));
			fEnableVT(GetStdHandle(STD_ERROR_HANDLE));
		}

		tf_CWinStr WideChar = NStr::NPlatform::fg_StrToWindows<tf_CWinStr>(_Str);
	#if defined(DMibDebug) || DMibConfig_Tests_Enable || defined(DConfig_Profile)
		if (!_bRaw && NSys::fg_System_BeingDebugged())
			NSys::fg_DebugOutput(WideChar.f_GetStr()); // Output to trace in debug
	#endif
	//	printf("%s", _pToOutput);
	
		uint32 Written = 0;
		HANDLE hCon = GetStdHandle(_StdHandle);
		if (hCon)
		{
			if (GetFileType(hCon) == FILE_TYPE_CHAR)
			{
				const ch16 *pOut = WideChar;
				mint Len = WideChar.f_GetLen();;
				while (Len)
				{
					ch16 Temp[2048];
					ch16 *pTemp = NMib::NStr::fg_StrCopy(Temp, pOut, 2048);
					mint nChars = pTemp - Temp;
					Len -= nChars;
					pOut += nChars;
					if (!WriteConsoleW(hCon, Temp, nChars, &Written, nullptr))
					{
	//					CFStr256 Error = NMib::NPlatform::fg_Win32_GetLastErrorStr();
	//					DMibError((CFStr256::CFormat("Windows returned an error from WriteFile(hCon): {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
						break;
					}
				}
			}
			else
			{
				tf_UTF8Str Output = _Str;
				const ch8 *pOut = Output;
				mint Len = fg_StrLen(Output);
				while (Len)
				{
					ch8 Temp[2048];
					ch8 *pTemp = NMib::NStr::fg_StrCopy(Temp, pOut, 2048);
					mint nChars = pTemp - Temp;
					Len -= nChars;
					pOut += nChars;
					if (!WriteFile(hCon, Temp, nChars, &Written, nullptr))
					{
		//				CFStr256 Error = NMib::NPlatform::fg_Win32_GetLastErrorStr();
			//			DMibDTrace("Windows returned an error from WriteFile(hCon): {}", NMib::NPlatform::fg_Win32_GetLastErrorStr());
						break;
					}
				}
			}
		}
	}
}

bool NSys::fg_ConsoleOutputValid()
{
	HANDLE hCon = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hCon)
	{
		int FileType = GetFileType(hCon);
		if (FileType == FILE_TYPE_UNKNOWN)
		{
			if (GetLastError() != NO_ERROR)
				return false;
		}
		return true;
	}
	//MessageBox(nullptr, CWStr(CWStr::CFormat(str_utf16("{} {}")) << hCon << fileType), str_utf16("onetuho"), MB_OK);
	return false;
}

bool NSys::fg_ConsoleInputValid()
{
	HANDLE hCon = GetStdHandle(STD_INPUT_HANDLE);
	if (hCon)
	{
		int FileType = GetFileType(hCon);
		if (FileType == FILE_TYPE_UNKNOWN)
		{
			if (GetLastError() != NO_ERROR)
				return false;
		}
		return true;
	}
	//MessageBox(nullptr, CWStr(CWStr::CFormat(str_utf16("{} {}")) << hCon << fileType), str_utf16("onetuho"), MB_OK);
	return false;
}


bool NSys::fg_ConsoleErrorOutputValid()
{
	HANDLE hCon = GetStdHandle(STD_ERROR_HANDLE);
	if (hCon)
	{
		int FileType = GetFileType(hCon);
		if (FileType == FILE_TYPE_UNKNOWN)
		{
			if (GetLastError() != NO_ERROR)
				return false;
		}
		return true;
	}
	//MessageBox(nullptr, CWStr(CWStr::CFormat(str_utf16("{} {}")) << hCon << fileType), str_utf16("onetuho"), MB_OK);
	return false;
}

void NSys::fg_ConsoleErrorOutputFlush()
{
	/*
	HANDLE hCon = GetStdHandle(STD_ERROR_HANDLE);
	FlushFileBuffers(hCon);
	*/
}

void NSys::fg_ConsoleOutputFlush()
{
	/*
	HANDLE hCon = GetStdHandle(STD_OUTPUT_HANDLE);
	FlushFileBuffers(hCon);
	*/
}

NSys::CConsoleProperties NSys::fg_GetConsoleProperties()
{
	NSys::CConsoleProperties Return;

	CONSOLE_SCREEN_BUFFER_INFO ConsoleScreenBufferInfo;
	fg_MemClear(ConsoleScreenBufferInfo);

    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ConsoleScreenBufferInfo) && !GetConsoleScreenBufferInfo(GetStdHandle(STD_ERROR_HANDLE), &ConsoleScreenBufferInfo))
		return Return;
	
	Return.m_Width = ConsoleScreenBufferInfo.srWindow.Right - ConsoleScreenBufferInfo.srWindow.Left + 1;
	Return.m_Height = ConsoleScreenBufferInfo.srWindow.Bottom - ConsoleScreenBufferInfo.srWindow.Top + 1;

	return Return;
}

void NSys::fg_ConsoleOutputRaw(CStrNonTracked const &_Str)
{
	fg_ConsoleOutputHelper<CWStrNonTracked, CStrNonTracked>(_Str, STD_OUTPUT_HANDLE, true);
}

void NSys::fg_ConsoleOutput(CStrNonTracked const &_Str)
{
	fg_ConsoleOutputHelper<CWStrNonTracked, CStrNonTracked>(_Str, STD_OUTPUT_HANDLE, false);
}

void NSys::fg_ConsoleOutput(CStrSecure const &_Str)
{
	fg_ConsoleOutputHelper<CWStrSecure, CStrSecure>(_Str, STD_OUTPUT_HANDLE, false);
}

void NSys::fg_ConsoleOutput(ch8 const *_pStr, mint _Len)
{
	fg_ConsoleOutputHelper<CWStrNonTracked, CStrNonTracked>(CStrNonTracked(_pStr, _Len), STD_OUTPUT_HANDLE, true);
}

void NSys::fg_ConsoleOutput(NMib::NStr::CStrSpan const &_Str)
{
	fg_ConsoleOutputHelper<CFWStr1024, CFStr1024>(CFStr1024(_Str.f_GetStr(), _Str.f_GetLen()), STD_OUTPUT_HANDLE, true);
}

void NSys::fg_ConsoleErrorOutput(NMib::NStr::CStrSpan const &_Str)
{
	fg_ConsoleOutputHelper<CFWStr1024, CFStr1024>(CFStr1024(_Str.f_GetStr(), _Str.f_GetLen()), STD_ERROR_HANDLE, true);
}

void NSys::fg_ConsoleOutputBinary(NMib::NContainer::CSecureByteVector const &_Buffer)
{
	uint32 Written = 0;
	HANDLE hCon = GetStdHandle(STD_OUTPUT_HANDLE);

	if (!hCon)
		return;
	uint8 const *pOut = _Buffer.f_GetArray();
	mint Len = _Buffer.f_GetLen();
	uint8 Temp[2048];
	while (Len)
	{
		mint ToCopy = fg_Min(Len, 2048u);
		NMib::NMemory::fg_MemCopy(Temp, pOut, ToCopy);
		if (!WriteFile(hCon, Temp, ToCopy, &Written, nullptr))
		{
			break;
		}
		Len -= Written;
		pOut += Written;
	}
	fg_SecureMemClear(Temp);
}

void NSys::fg_ConsoleErrorOutput(const NMib::NStr::CStrNonTracked &_Str)
{
	fg_ConsoleOutputHelper<CWStrNonTracked, CStrNonTracked>(_Str, STD_ERROR_HANDLE, false);
}

void NSys::fg_ConsoleErrorOutput(NMib::NStr::CStrSecure const &_Str)
{
	fg_ConsoleOutputHelper<CWStrSecure, CStrSecure>(_Str, STD_ERROR_HANDLE, false);
}

void *fg_AllocVirtualMemory(mint &_Size, mint _Type, ENumaNode _NumaNode, mint _Alignment, EAllocationFlag _Flags)
{
//	DMibSafeCheck(_Size == fg_AlignUp(_Size, CAllocator_Virtual::f_GranularityAlloc()), "You are wasting space");
	if (_Size == 0)
		_Size = gs_SysInfo.dwAllocationGranularity;
	CSystemWindowsMSVC *pSys = fg_GetLocalSys();

	uint32 Flags = _Type;
	void *pMem = nullptr;
	if (!pSys || pSys->m_bDestroying)
		_Flags &= ~(EAllocationFlag_LocationUp | EAllocationFlag_LocationDown);

	mint Granularity = NSys::fg_Mem_VirtualGranularityAlloc((Flags & MEM_LARGE_PAGES) != 0);
	mint Size = fg_AlignUp(_Size, fg_Max(Granularity, _Alignment));
	mint IdealSize = Size;

	if (_Alignment)
	{
		Flags &= ~uint32(MEM_COMMIT);
	}

	void *pOldMem = nullptr;

	while (1)
	{
		if (_NumaNode != -1 && NLocal::g_OptionalFunctions.m_fVirtualAllocExNuma)
		{
			if (_Flags & EAllocationFlag_LocationUp)
			{
				pMem = NLocal::g_OptionalFunctions.m_fVirtualAllocExNuma(GetCurrentProcess(), pOldMem, Size, Flags, PAGE_READWRITE, _NumaNode);
				if (!pMem && (Flags & MEM_LARGE_PAGES))
					pMem = NLocal::g_OptionalFunctions.m_fVirtualAllocExNuma(GetCurrentProcess(), pOldMem, Size, (Flags) & (~uint32(MEM_LARGE_PAGES)), PAGE_READWRITE, _NumaNode);
			}
			else
			{
				pMem = NLocal::g_OptionalFunctions.m_fVirtualAllocExNuma(GetCurrentProcess(), pOldMem, Size, Flags | MEM_TOP_DOWN, PAGE_READWRITE, _NumaNode);
				if (!pMem && (Flags & MEM_LARGE_PAGES))
					pMem = NLocal::g_OptionalFunctions.m_fVirtualAllocExNuma(GetCurrentProcess(), pOldMem, Size, (Flags | MEM_TOP_DOWN) & (~uint32(MEM_LARGE_PAGES)), PAGE_READWRITE, _NumaNode);
			}

			if (!pMem)
			{
				if (_Alignment && pOldMem != nullptr)
				{
					pOldMem = nullptr;
					Size = fg_AlignUp(_Size, fg_Max(Granularity, _Alignment));
					Size += _Alignment;
					// Retry
					continue;
				}
				auto Error = NMib::NPlatform::fg_Win32_GetLastErrorStr();
				DMibErrorMemory((CFStr256::CFormat("Windows returned an error from VirtualAllocExNuma: {}") << Error).f_GetStr());
			}
		}
		else
		{
			if (_Flags & EAllocationFlag_LocationUp)
			{
				pMem = VirtualAlloc(pOldMem, Size, Flags, PAGE_READWRITE);
				if (!pMem && (Flags & MEM_LARGE_PAGES))
					pMem = VirtualAlloc(pOldMem, Size, (Flags) & (~uint32(MEM_LARGE_PAGES)), PAGE_READWRITE);
			}
			else
			{
				pMem = VirtualAlloc(pOldMem, Size, Flags | MEM_TOP_DOWN, PAGE_READWRITE);
				if (!pMem && (Flags & MEM_LARGE_PAGES))
					pMem = VirtualAlloc(pOldMem, Size, (Flags | MEM_TOP_DOWN) & (~uint32(MEM_LARGE_PAGES)), PAGE_READWRITE);
			}
			if (!pMem)
			{
				if (_Alignment && pOldMem != nullptr)
				{
					pOldMem = nullptr;
					Size = fg_AlignUp(_Size, fg_Max(Granularity, _Alignment));
					Size += _Alignment;
					// Retry
					continue;
				}
				DMibErrorMemory((CFStr256::CFormat("Windows returned an error from VirtualAlloc: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
			}
		}
		if (_Alignment)
		{
			if ((mint)pMem & (_Alignment - 1) || (Size != IdealSize))
			{
				VirtualFree(pMem, 0, MEM_RELEASE);
				if (Size == IdealSize)
				{
					Size = IdealSize + _Alignment;
					pOldMem = nullptr;
				}
				else
				{
					pOldMem = (void *)fg_AlignDown((mint)pMem + (_Alignment - 1), _Alignment);
					Size = IdealSize;
				}
				pMem = nullptr;
			}
			else
			{
				if (_Type & MEM_COMMIT)
					NSys::fg_Mem_VirtualCommit(pMem, Size);
				break;
			}
		}
		else
			break;
	}
	_Size = Size;
	return pMem;
}

void NSys::fg_Thread_SetNumaAffinity(void *_pThread, ENumaNode _NumaNode)
{
	if (!NLocal::g_OptionalFunctions.m_fVirtualAllocExNuma)
		return; // Numa nodes not supported

	if (NLocal::g_OptionalFunctions.m_fGetNumaNodeProcessorMaskEx && NLocal::g_OptionalFunctions.m_fSetThreadGroupAffinity)
	{
		GROUP_AFFINITY Affinity;
		fg_MemClear(Affinity);
		if (NLocal::g_OptionalFunctions.m_fGetNumaNodeProcessorMaskEx(_NumaNode, &Affinity))
		{
			GROUP_AFFINITY OldAffinity;
			fg_MemClear(OldAffinity);
			NLocal::g_OptionalFunctions.m_fSetThreadGroupAffinity(_pThread, &Affinity, &OldAffinity);
		}
	}
	else
	{
		ULONGLONG Mask = 0;
		if (GetNumaNodeProcessorMask(_NumaNode, &Mask))
		{
			SetThreadAffinityMask(_pThread, Mask);
		}
	}

}

mint NSys::fg_Mem_GetNumNumaNodes()
{
	if (!NLocal::g_OptionalFunctions.m_fVirtualAllocExNuma)
		return 0; // Numa nodes not supported

	ULONG HighestNumber;
	if (!GetNumaHighestNodeNumber(&HighestNumber))
		return 0;
	if (HighestNumber == 0)
		return 0;
	int32 Ret = 0;
	for (uint32 i = 0; i <= HighestNumber; ++i)
	{
		if (NLocal::g_OptionalFunctions.m_fGetNumaNodeProcessorMaskEx)
		{
			GROUP_AFFINITY Affinity;
			fg_MemClear(Affinity);
			if (NLocal::g_OptionalFunctions.m_fGetNumaNodeProcessorMaskEx(i, &Affinity))
				++Ret;			
		}
		else
		{
			ULONGLONG Mask = 0;
			if (GetNumaNodeProcessorMask(i, &Mask))
				++Ret;
		}
	}
	
	return Ret;
}

void NSys::fg_Mem_GetNumaNodes(ENumaNode *_pNodes, mint _nNodes)
{
	if (!NLocal::g_OptionalFunctions.m_fVirtualAllocExNuma)
		return; // Numa nodes not supported

	ULONG HighestNumber;
	if (!GetNumaHighestNodeNumber(&HighestNumber))
		return;
	if (HighestNumber == 0)
		return;
	mint Ret = 0;
	for (uint32 i = 0; i <= HighestNumber; ++i)
	{
		if (NLocal::g_OptionalFunctions.m_fGetNumaNodeProcessorMaskEx)
		{
			GROUP_AFFINITY Affinity;
			fg_MemClear(Affinity);
			if (NLocal::g_OptionalFunctions.m_fGetNumaNodeProcessorMaskEx(i, &Affinity))
			{
				_pNodes[Ret] = ENumaNode(i);
				++Ret;			
				if (Ret >= _nNodes)
					return;
			}
		}
		else
		{
			ULONGLONG Mask = 0;
			if (GetNumaNodeProcessorMask(i, &Mask))
			{
				_pNodes[Ret] = ENumaNode(i);
				++Ret;			
				if (Ret >= _nNodes)
					return;
			}
		}
	}
}

void *NSys::fg_Mem_VirtualAlloc(mint &_Size, EAllocationFlag _AllocFlags, ENumaNode _NumaNode, mint _Alignment)
{
	uint32 Flags;
	if (_AllocFlags & EAllocationFlag_NoCommit)
		Flags = MEM_RESERVE;
	else
		Flags = MEM_RESERVE | MEM_COMMIT;

	if (_AllocFlags & EAllocationFlag_LargePages)
	{
		if (fg_GetLocalSys()->f_EnableLargeMemorySupport())
			Flags |= MEM_LARGE_PAGES;
	}

	return fg_AllocVirtualMemory(_Size, Flags, _NumaNode, _Alignment, _AllocFlags);
}

void *NSys::fg_Mem_VirtualRealloc(void *_pMem, mint &_Size, mint _OldSize, EAllocationFlag _AllocFlags, ENumaNode _NumaNode)
{
	if (_OldSize == 0)
		_OldSize = fg_Mem_VirtualSize(_pMem);
	fg_Mem_VirtualFree(_pMem, _OldSize);
	return fg_Mem_VirtualAlloc(_Size, _AllocFlags, _NumaNode, _AllocFlags);
}

void *NSys::fg_Mem_VirtualResize(void *_pMem, mint &_Size, mint _OldSize, EAllocationFlag _AllocFlags, ENumaNode _NumaNode)
{
	if (_OldSize == 0)
		_OldSize = fg_Mem_VirtualSize(_pMem);
	void *pNewMem = fg_Mem_VirtualAlloc(_Size, _AllocFlags, _NumaNode, _AllocFlags);
	fg_MemCopy(pNewMem, _pMem, fg_Min(_Size, _OldSize));
	fg_Mem_VirtualFree(_pMem, _OldSize);
	return pNewMem;
}


void NSys::fg_Mem_VirtualProtect(void *_pMem, mint _Size, uaint _Protect)
{
	uaint Protect = PAGE_NOACCESS;
	switch ((_Protect) & EProtect_All)
	{
	case EProtect_All:
		Protect = PAGE_EXECUTE_READWRITE;
		break;
	case EProtect_ReadWrite:
		Protect = PAGE_READWRITE;
		break;
	case EProtect_Read:
		Protect = PAGE_READONLY;
		break;
	case EProtect_Exec:
		Protect = PAGE_EXECUTE;
		break;
	case EProtect_ReadExec:
		Protect = PAGE_EXECUTE_READ;
		break;
	case EProtect_WriteExec:
		DMibErrorMemory("Invalid Protection Mode (EProtect_WriteExec)");
		break;
	case EProtect_Write:
		DMibErrorMemory("Invalid Protection Mode (EProtect_Write)");
		break;
	}

	if (_Protect & EProtect_NoCache)
		Protect |= PAGE_NOCACHE;
	else if (_Protect & EProtect_WriteCombine)
		Protect |= PAGE_WRITECOMBINE;

	DWORD OldProtect = 0;
	if (!VirtualProtect(_pMem, _Size, Protect, &OldProtect))
	{
		mint Address = (mint)_pMem;
		mint Size = _Size;
		while (Size)
		{
			MEMORY_BASIC_INFORMATION MemInfo;
			VirtualQuery((void *)Address, &MemInfo, sizeof(MemInfo));
			mint CurrentAddress = (mint)MemInfo.BaseAddress + MemInfo.RegionSize;
			mint ThisTime = fg_Min(Size, CurrentAddress - Address);

			if (!VirtualProtect((void *)Address, ThisTime, Protect, &OldProtect))
			{
				CFStr256 Error = (CFStr256::CFormat("Windows returned an error from VirtualProtect: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr();
				DMibErrorMemory(Error);
			}
			
			Size -= ThisTime;
			Address += ThisTime;
		}

	}
}

void NSys::fg_Mem_VirtualCommit(void *_pMem, mint _Size)
{
#if 0 //def DMibDebug
	// Check if the range of memory already is commited

	mint Address = (mint)_pMem;
	mint Size = _Size;
	while (Size)
	{
		MEMORY_BASIC_INFORMATION MemInfo;
		VirtualQuery((void *)Address, &MemInfo, sizeof(MemInfo));
		mint CurrentAddress = (mint)MemInfo.BaseAddress + MemInfo.RegionSize;
		mint ThisTime = fg_Min(Size, CurrentAddress - Address);
		if (MemInfo.State != MEM_RESERVE || MemInfo.Type != MEM_PRIVATE)
		{
			CFStr256 Temp = CFStr256::CFormat("You are wasting clock cycles 0x{nfh,sf0,sj8} 0x{nfh,sf0,sj8} 0x{nfh,sf0,sj8} 0x{nfh,sf0,sj8}\n") << mint(_pMem) << mint(_Size) << mint(CurrentAddress) << mint(ThisTime);
			OutputDebugStringA(Temp);
		}
//		DMibSafeCheck(MemInfo.State == MEM_RESERVE && MemInfo.Type == MEM_PRIVATE, "You are wasting clock cycles");
		Size -= ThisTime;
		Address += ThisTime;
	}
	

#endif
	if (!VirtualAlloc((void *)_pMem, _Size, MEM_COMMIT, PAGE_READWRITE))
		DMibErrorMemory((CFStr256::CFormat("Windows returned an error from VirtualAlloc: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
#if 0
	{
	//	DMibDTrace("VirtualStupidizing\n", 0);
		mint Address = (mint)_pMem;
		while (_Size)
		{
			MEMORY_BASIC_INFORMATION MemInfo;
			VirtualQuery((void *)Address, &MemInfo, sizeof(MemInfo));
			mint CurrentAddress = (mint)MemInfo.BaseAddress + MemInfo.RegionSize;
			mint AllocBase = (mint)MemInfo.AllocationBase;
			mint NextBase = AllocBase;
			while (AllocBase == NextBase)
			{
				if (VirtualQuery((void *)CurrentAddress, &MemInfo, sizeof(MemInfo)))
				{
					NextBase = (mint)MemInfo.AllocationBase;
					CurrentAddress = (mint)MemInfo.BaseAddress + MemInfo.RegionSize;
				}
				else
				{
					NextBase = (mint)MemInfo.BaseAddress + MemInfo.RegionSize;
				}
			}

			mint ThisTime = fg_Min(_Size, NextBase - Address);
			_Size -= ThisTime;

			if (!VirtualAlloc((void *)Address, ThisTime, MEM_COMMIT, PAGE_READWRITE))
			{
				DMibErrorMemory((CFStr256::CFormat("Windows returned an error from VirtualAlloc: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
			}
			Address += ThisTime;

		}
	}
#endif
}

void NSys::fg_Mem_VirtualDecommit(void *_pMem, mint _Size)
{

#ifdef DMibDebug
	// Check if the range of memory already is commited

	mint Address = (mint)_pMem;
	mint Size = _Size;
	while (Size)
	{
		MEMORY_BASIC_INFORMATION MemInfo;
		VirtualQuery((void *)Address, &MemInfo, sizeof(MemInfo));
		DMibSafeCheck(MemInfo.State == MEM_COMMIT && MemInfo.Type == MEM_PRIVATE, "You are wasting clock cycles");
		mint CurrentAddress = (mint)MemInfo.BaseAddress + MemInfo.RegionSize;
		mint ThisTime = fg_Min(Size, CurrentAddress - Address);
		Size -= ThisTime;
		Address += ThisTime;
	}
	

#endif
	if (!VirtualFree((void *)_pMem, _Size, MEM_DECOMMIT))
		DMibErrorMemory((CFStr256::CFormat("Windows returned an error from VirtualFree: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
#if 0
	{
	//	DMibDTrace("VirtualStupidizing2\n", 0);
		mint Address = (mint)_pMem;
		while (_Size)
		{
			MEMORY_BASIC_INFORMATION MemInfo;
			VirtualQuery((void *)Address, &MemInfo, sizeof(MemInfo));
			mint CurrentAddress = (mint)MemInfo.BaseAddress + MemInfo.RegionSize;
			mint AllocBase = (mint)MemInfo.AllocationBase;
			mint NextBase = AllocBase;
			while (AllocBase == NextBase)
			{
				if (VirtualQuery((void *)CurrentAddress, &MemInfo, sizeof(MemInfo)))
				{
					NextBase = (mint)MemInfo.AllocationBase;
					CurrentAddress = (mint)MemInfo.BaseAddress + MemInfo.RegionSize;
				}
				else
				{
					NextBase = (mint)MemInfo.BaseAddress + MemInfo.RegionSize;
				}
			}

			mint ThisTime = fg_Min(_Size, NextBase - Address);
			_Size -= ThisTime;

			if (!VirtualFree((void *)Address, ThisTime, MEM_DECOMMIT))
			{
				DMibErrorMemory((CFStr256::CFormat("Windows returned an error from VirtualFree: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
			}
			Address += ThisTime;
		}
	}
#endif

}

void NSys::fg_Mem_VirtualFree(void *_pMem, mint _Size)
{
	if (!_pMem)
		return;

	if (!VirtualFree(_pMem, 0, MEM_RELEASE))
	{
		DMibErrorMemory((CFStr256::CFormat("Windows returned an error from VirtualFree: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}


void *NSys::fg_InterProcess_MemAlloc(ch8 const *_pName, mint _Size, void * &_pMemory)
{
	uint64 Size = _Size; //fg_AlignUp((uint64)_Size, (uint64)gs_SysInfo.dwAllocationGranularity);
	void *pHandle = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE | SEC_COMMIT, (Size >> 32) & 0xFFFFFFFFu, Size & 0xFFFFFFFFu, _pName);
	if (!pHandle)
	{
		auto Error = GetLastError();
		DMibErrorMemory((CFStr256::CFormat("Windows returned an error from CreateFileMappingA: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
	}
	void *pMemory = MapViewOfFile(pHandle, FILE_MAP_WRITE, 0, 0, Size);
	if (!pMemory)
	{
		auto Error = GetLastError();
		CloseHandle(pHandle);
		DMibErrorMemory((CFStr256::CFormat("Windows returned an error from MapViewOfFile: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
	}
	_pMemory = pMemory;
	return pHandle;
}

void NSys::fg_InterProcess_MemFree(void *_pHandle, void *_pMemory)
{
	if (!UnmapViewOfFile(_pMemory))
	{
		auto Error = GetLastError();
		DMibErrorMemory((CFStr256::CFormat("Windows returned an error from UnmapViewOfFile: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
	}
	if (!CloseHandle(_pHandle))
	{
		auto Error = GetLastError();
		DMibErrorMemory((CFStr256::CFormat("Windows returned an error from CloseHandle: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
	}
}
mint NSys::fg_Mem_VirtualSize(const void *_pMem)
{
	MEMORY_BASIC_INFORMATION Info;
	if (!VirtualQuery(_pMem, &Info, sizeof(Info)))
	{
		DMibErrorMemory((CFStr256::CFormat("Windows returned an error from VirtualProtect: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}

	return Info.RegionSize;
}

mint NSys::fg_Mem_VirtualTrySize(const void *_pMem)
{
	DMibPDebugBreak; // Not supported
	return 0;
}

mint NSys::fg_Mem_PageSize()
{
	return gs_SysInfo.dwPageSize;
}

void NMib::NSys::NStr::fg_SystemEncodeCodePageStr(NMib::NStr::CStr const &_In, NMib::NStr::CAnsiStr &_Out, uint32 _CodePage, ch8 _ErrorChar)
{
	CWStr Temp = _In;
	ch8 ErrorStr[] = {_ErrorChar, 0};
	int Len = WideCharToMultiByte(_CodePage, 0, Temp.f_GetStr(), -1, nullptr, 0, ErrorStr, nullptr);
	if (WideCharToMultiByte(_CodePage, 0, Temp.f_GetStr(), -1, _Out.f_GetStr(Len), Len, ErrorStr, nullptr))
		;
	else
		DMibErrorSystemImp((CStr::CFormat("Windows returned an error from WideCharToMultiByte: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NMib::NSys::NStr::fg_SystemDecodeCodePageStr(NMib::NStr::CAnsiStr const &_In, NMib::NStr::CStr &_Out, uint32 _CodePage)
{
	CWStr Out;
	int Len = MultiByteToWideChar(_CodePage, 0, _In.f_GetStr(), -1, nullptr, 0);
	if (MultiByteToWideChar(_CodePage, 0, _In.f_GetStr(), -1, Out.f_GetStr(Len), Len))
		_Out = Out;
	else
		DMibErrorSystemImp((CStr::CFormat("Windows returned an error from MultiByteToWideChar: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NMib::NSys::NStr::fg_SystemDecodeCodePageStr(ch8 const *_pIn, NMib::NStr::CStr &_Out, uint32 _CodePage)
{
	CWStr Out;
	int Len = MultiByteToWideChar(_CodePage, 0, _pIn, -1, nullptr, 0);
	if (MultiByteToWideChar(_CodePage, 0, _pIn, -1, Out.f_GetStr(Len), Len))
		_Out = Out;
	else
		DMibErrorSystemImp((CStr::CFormat("Windows returned an error from MultiByteToWideChar: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NMib::NSys::NStr::fg_SystemEncodeAnsiStr(NMib::NStr::CStr const &_In, NMib::NStr::CAnsiStr &_Out, ch8 _ErrorChar)
{
	CWStr Temp = _In;
	ch8 ErrorStr[] = {_ErrorChar, 0};
	int Len = WideCharToMultiByte(CP_ACP, 0, Temp.f_GetStr(), -1, nullptr, 0, ErrorStr, nullptr);
	if (WideCharToMultiByte(CP_ACP, 0, Temp.f_GetStr(), -1, _Out.f_GetStr(Len), Len, ErrorStr, nullptr))
		;
	else
		DMibErrorSystemImp((CStr::CFormat("Windows returned an error from WideCharToMultiByte: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NMib::NSys::NStr::fg_SystemDecodeAnsiStr(NMib::NStr::CAnsiStr const &_In, NMib::NStr::CStr &_Out)
{
	CWStr Out;
	int Len = MultiByteToWideChar(CP_ACP, 0, _In.f_GetStr(), -1, nullptr, 0);
	if (MultiByteToWideChar(CP_ACP, 0, _In.f_GetStr(), -1, Out.f_GetStr(Len), Len))
		_Out = Out;
	else
		DMibErrorSystemImp((CStr::CFormat("Windows returned an error from MultiByteToWideChar: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NMib::NSys::NStr::fg_SystemDecodeAnsiStr(ch8 const *_pIn, NMib::NStr::CStr &_Out)
{
	CWStr Out;
	int Len = MultiByteToWideChar(CP_ACP, 0, _pIn, -1, nullptr, 0);
	if (MultiByteToWideChar(CP_ACP, 0, _pIn, -1, Out.f_GetStr(Len), Len))
		_Out = Out;
	else
		DMibErrorSystemImp((CStr::CFormat("Windows returned an error from MultiByteToWideChar: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NMib::NSys::NStr::fg_SystemEncodeCodePageStr(NMib::NStr::CStrNonTracked const &_In, NMib::NStr::CAnsiStrNonTracked &_Out, uint32 _CodePage, ch8 _ErrorChar)
{
	CWStrNonTracked Temp = _In;
	CAnsiStrNonTracked Out;
	ch8 ErrorStr[] = {_ErrorChar, 0};
	int Len = WideCharToMultiByte(_CodePage, 0, Temp.f_GetStr(), -1, nullptr, 0, ErrorStr, nullptr);
	if (WideCharToMultiByte(_CodePage, 0, Temp.f_GetStr(), -1, Out.f_GetStr(Len), Len, ErrorStr, nullptr))
		_Out = Out;
	else
		DMibErrorSystemImp((CStr::CFormat("Windows returned an error from WideCharToMultiByte: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NMib::NSys::NStr::fg_SystemDecodeCodePageStr(NMib::NStr::CAnsiStrNonTracked const &_In, NMib::NStr::CStrNonTracked &_Out, uint32 _CodePage)
{
	CWStrNonTracked Out;
	int Len = MultiByteToWideChar(_CodePage, 0, _In.f_GetStr(), -1, nullptr, 0);
	if (MultiByteToWideChar(_CodePage, 0, _In.f_GetStr(), -1, Out.f_GetStr(Len), Len))
		_Out = Out;
	else
		DMibErrorSystemImp((CStr::CFormat("Windows returned an error from MultiByteToWideChar: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NMib::NSys::NStr::fg_SystemDecodeCodePageStr(ch8 const *_pIn, NMib::NStr::CStrNonTracked &_Out, uint32 _CodePage)
{
	CWStrNonTracked Out;
	int Len = MultiByteToWideChar(_CodePage, 0, _pIn, -1, nullptr, 0);
	if (MultiByteToWideChar(_CodePage, 0, _pIn, -1, Out.f_GetStr(Len), Len))
		_Out = Out;
	else
		DMibErrorSystemImp((CStr::CFormat("Windows returned an error from MultiByteToWideChar: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NMib::NSys::NStr::fg_SystemEncodeAnsiStr(NMib::NStr::CStrNonTracked const &_In, NMib::NStr::CAnsiStrNonTracked &_Out, ch8 _ErrorChar)
{
	CWStrNonTracked Temp = _In;
	ch8 ErrorStr[] = {_ErrorChar, 0};
	int Len = WideCharToMultiByte(CP_ACP, 0, Temp.f_GetStr(), -1, nullptr, 0, ErrorStr, nullptr);
	if (WideCharToMultiByte(CP_ACP, 0, Temp.f_GetStr(), -1, _Out.f_GetStr(Len), Len, ErrorStr, nullptr))
		;
	else
		DMibErrorSystemImp((CStr::CFormat("Windows returned an error from WideCharToMultiByte: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NMib::NSys::NStr::fg_SystemDecodeAnsiStr(NMib::NStr::CAnsiStrNonTracked const &_In, NMib::NStr::CStrNonTracked &_Out)
{
	CWStrNonTracked Out;
	int Len = MultiByteToWideChar(CP_ACP, 0, _In.f_GetStr(), -1, nullptr, 0);
	if (MultiByteToWideChar(CP_ACP, 0, _In.f_GetStr(), -1, Out.f_GetStr(Len), Len))
		_Out = Out;
	else
		DMibErrorSystemImp((CStr::CFormat("Windows returned an error from MultiByteToWideChar: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NMib::NSys::NStr::fg_SystemDecodeAnsiStr(ch8 const *_pIn, NMib::NStr::CStrNonTracked &_Out)
{
	CWStrNonTracked Out;
	int Len = MultiByteToWideChar(CP_ACP, 0, _pIn, -1, nullptr, 0);
	if (MultiByteToWideChar(CP_ACP, 0, _pIn, -1, Out.f_GetStr(Len), Len))
		_Out = Out;
	else
		DMibErrorSystemImp((CStr::CFormat("Windows returned an error from MultiByteToWideChar: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
}


void NSys::fg_System_ExitProcess(aint _ExitCode)
{
	HMODULE hmod;
	typedef void (WINAPI * PFN_EXIT_PROCESS)(UINT _ExitCode);
	PFN_EXIT_PROCESS pfn;


	hmod = GetModuleHandleA("mscoree.dll");
	if (hmod != nullptr) {
		pfn = (PFN_EXIT_PROCESS)GetProcAddress(hmod, "CorExitProcess");
		if (pfn != nullptr) {
			pfn(_ExitCode);
		}
	}

	/*
		* Either mscoree.dll isn't loaded,
		* or CorExitProcess isn't exported from mscoree.dll,
		* or CorExitProcess returned (should never happen).
		* Just call ExitProcess.
		*/

	::ExitProcess(_ExitCode);
}

void __cdecl fg_ValidExitProcess();
void __cdecl fg_ValidDestroyModule();

void NSys::fg_TerminateProcess(aint _ExitCode)
{
	fg_ValidExitProcess();
	fg_ValidDestroyModule();

	if (g_fOrgTerminateProcess)
		g_fOrgTerminateProcess(GetCurrentProcess(), _ExitCode);
	else
		TerminateProcess(GetCurrentProcess(), _ExitCode);
}

NStr::CStr NSys::fg_CommandLineParameters()
{
	LPWSTR pCommandLine = GetCommandLineW();


	int Mode = 0;

	while (*pCommandLine)
	{
		ch16 Temp = *pCommandLine;
		switch (Mode)
		{
		case 0:
			{
				if (Temp == '"')
				{
					Mode = 1;
				}
				else if (fg_CharIsWhiteSpace(Temp))
				{
					while (*pCommandLine && fg_CharIsWhiteSpace(*pCommandLine))
						++pCommandLine;
					return CWStr(pCommandLine);
				}
			}
			break;
		case 1:
			{
				if (Temp == '"')
				{
					Mode = 0;
				}
			}
			break;

		}

		++pCommandLine;
	}

	return "";
}


void NSys::fg_Thread_Sleep(fp32 _Seconds)
{
	mint MilliSec = (_Seconds * 1000.0).f_ToInt();

	Sleep(MilliSec);
}

bool g_bProcessDetached = false;

void fg_SetThreadLocalForOtherThread(mint _ThreadID, mint _iStorage, void *_pData)
{
	HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, false, _ThreadID);
	if (hThread)
	{
		[[maybe_unused]] bool bSuccess = false;
		CFStr256 ErrorStr;
		if (SuspendThread(hThread) != 0xFFFFFFFF)
		{
			NLocal::THREAD_BASIC_INFORMATION ThreadInfo;
			if (NT_SUCCESS(NLocal::g_OptionalFunctions.m_fNtQueryInformationThread(hThread, (::THREADINFOCLASS)NLocal::ThreadBasicInformation, &ThreadInfo, sizeof( NLocal::THREAD_BASIC_INFORMATION ), 0)))
			{
				mint *pTIB = (mint *)ThreadInfo.TebBaseAddress;
	#if defined(DArchitecture_x64) || defined(DArchitecture_arm64)
				if (_iStorage < 0x40)
					pTIB[_iStorage + 0x290] = (mint)_pData;
				else if (_iStorage < 0x440)
				{
					mint *pThreadLocalBlock = (mint *)pTIB[0x2F0];

					if (!pThreadLocalBlock)
					{
						// This is potentially unsafe because of race condition, should maybe suspend thread?
						pThreadLocalBlock = (mint *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(void*)*1024);
						if (fg_Volatile(pTIB[0x2F0]))
							DMibPDebugBreak;
						pTIB[0x2F0] = (mint)pThreadLocalBlock;
					}

					pThreadLocalBlock[_iStorage - 64] = (mint)_pData; 
				}
				else
					DMibFastCheck(0);
	#else
				if (_iStorage < 0x40)
				{
					pTIB[_iStorage + 0x384] = (mint)_pData;
				}
				else if (_iStorage < 0x440)
				{
					mint *pThreadLocalBlock = (mint *)pTIB[0x3E5];

					if (!pThreadLocalBlock)
					{
						pThreadLocalBlock = (mint *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(void*)*1024);
						if (fg_Volatile(pTIB[0x3E5]))
							DMibPDebugBreak;
						pTIB[0x3E5] = (mint)pThreadLocalBlock;
					}

					pThreadLocalBlock[_iStorage - 64] = (mint)_pData; 
				}
				else
					DMibFastCheck(0);
	#endif
				bSuccess = true;
			}
			else
				ErrorStr = "NtQueryInformationThread: " + NMib::NPlatform::fg_Win32_GetLastErrorStr(GetLastError());
	
			ResumeThread(hThread);
		}
		else
			ErrorStr = "SuspendThread: " + NMib::NPlatform::fg_Win32_GetLastErrorStr(GetLastError());
		CloseHandle(hThread);

//		if (!bSuccess && !g_bProcessDetached)
	//		DMibError((CFStr256::CFormat("Failed to set thread storage for another thread ({}): {}") << _ThreadID << ErrorStr).f_GetStr());
	}
}

void *fg_GetThreadLocalForOtherThread(mint _ThreadID, mint _iStorage)
{
	void *pRet = nullptr;
	HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, false, _ThreadID);
	if (hThread)
	{
		bool bSuccess = false;
		CFStr256 ErrorStr;
		if (SuspendThread(hThread) != 0xFFFFFFFF)
		{
			NLocal::THREAD_BASIC_INFORMATION ThreadInfo;
			if (NT_SUCCESS(NLocal::g_OptionalFunctions.m_fNtQueryInformationThread(hThread, (::THREADINFOCLASS)NLocal::ThreadBasicInformation, &ThreadInfo, sizeof( NLocal::THREAD_BASIC_INFORMATION ), 0)))
			{
				mint *pTIB = (mint *)ThreadInfo.TebBaseAddress;
	#if defined(DArchitecture_x64) || defined(DArchitecture_arm64)
				if (_iStorage < 0x40)
					pRet = (void *)pTIB[_iStorage + 0x290];
				else if (_iStorage < 0x440)
				{
					mint *pThreadLocalBlock = (mint *)pTIB[0x2F0];

					if (pThreadLocalBlock)
						pRet = (void *)pThreadLocalBlock[_iStorage - 64];

				}
				else
					DMibFastCheck(0);
	#else
				if (_iStorage < 0x40)
				{
					pRet = (void *)pTIB[_iStorage + 0x384];
				}
				else if (_iStorage < 0x440)
				{
					mint *pThreadLocalBlock = (mint *)pTIB[0x3E5];

					if (pThreadLocalBlock)
						pRet = (void *)pThreadLocalBlock[_iStorage - 64];
				}
				else
					DMibFastCheck(0);
	#endif
				bSuccess = true;
			}
			else
				ErrorStr = "NtQueryInformationThread: " + NMib::NPlatform::fg_Win32_GetLastErrorStr(GetLastError());
	
			ResumeThread(hThread);
		}
		else
			ErrorStr = "SuspendThread: " + NMib::NPlatform::fg_Win32_GetLastErrorStr(GetLastError());
		CloseHandle(hThread);

		if (!bSuccess && !g_bProcessDetached)
			DMibError((CFStr256::CFormat("Failed to get thread storage for another thread ({}): {}") << _ThreadID << ErrorStr).f_GetStr());
	}

	return pRet;
}

DWORD WINAPI fg_TlsAllocInternal( bool _bFast)
{
	CUndocumentedTEB *pTEB = fg_GetTEB();
	UndocumentedPEB *pPEB = fg_GetPEB(pTEB);
	DWORD index = -1;

	NLocal::g_OptionalFunctions.m_fRtlAcquirePebLock();
	if (_bFast)
	{
		index = NLocal::g_OptionalFunctions.m_fRtlFindClearBitsAndSet( pPEB->TlsBitmap, 1, 0 );
		if (index != ~0U) 
			pTEB->TlsSlots[index] = 0; /* clear the value */
	}
	else
	{
		index = NLocal::g_OptionalFunctions.m_fRtlFindClearBitsAndSet( pPEB->TlsExpansionBitmap, 1, 0 );
		if (index != ~0U)
		{
			if (!pTEB->TlsExpansionSlots &&
				!(pTEB->TlsExpansionSlots = (PPVOID)HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
				8 * sizeof(pPEB->TlsExpansionBitmapBits) * sizeof(void*) )))
			{
				NLocal::g_OptionalFunctions.m_fRtlClearBits( pPEB->TlsExpansionBitmap, index, 1 );
				index = ~0U;
				SetLastError( ERROR_NOT_ENOUGH_MEMORY );
			}
			else
			{
				pTEB->TlsExpansionSlots[index] = 0; /* clear the value */
				//index += TLS_MINIMUM_AVAILABLE;
			}
		}
		else 
			SetLastError( ERROR_NO_MORE_ITEMS );
	}
	NLocal::g_OptionalFunctions.m_fRtlReleasePebLock();
	return index;
}

mint NSys::fg_Thread_AllocLocal()
{
	mint Index = fg_TlsAllocInternal(false);
	if (Index == TLS_OUT_OF_INDEXES)
		DMibErrorSystemImp("Thread_AllocStorage: Out of indices");
	return Index;
}

void NSys::fg_Thread_FreeLocal(mint _iStorage)
{
	if (!TlsFree(_iStorage+0x40))
	{
		DMibErrorSystemImp("Thread_FreeStorage: Failed to free Thread Storage Index");
	}
}


void NSys::fg_Thread_SetLocal(mint _ThreadID, mint _iStorage, void *_pData)
{
	if (NSys::fg_Thread_GetCurrentUID() == _ThreadID)
		return fg_Thread_SetLocal(_iStorage, _pData);

	fg_SetThreadLocalForOtherThread(_ThreadID, _iStorage + 0x40, _pData);

}

void NSys::fg_Thread_SetLocal(mint _iStorage, void *_pData)
{
	if (!TlsSetValue(_iStorage+0x40, _pData))
	{
		DMibErrorSystemImp("Thread_FreeStorage: Failed to set Thread Storage Value");
	}
}

#if defined(DArchitecture_x64) || defined(DArchitecture_arm64)
mint g_OffsetThreadLocalOffset = 0x1780;
#else
mint g_OffsetThreadLocalOffset = 0xf94;
#endif

thread_local mint g_DebugTIB = (mint)fg_GetTEB();

void *NSys::fg_Thread_GetLocal(mint _ThreadID, mint _iStorage)
{
	if (NSys::fg_Thread_GetCurrentUID() == _ThreadID)
		return fg_Thread_GetLocal(_iStorage);

	return fg_GetThreadLocalForOtherThread(_ThreadID, _iStorage + 0x40);
}

void *NSys::fg_Thread_GetLocalAlwaysSet(mint _ThreadID, mint _iStorage)
{
	if (NSys::fg_Thread_GetCurrentUID() == _ThreadID)
		return fg_Thread_GetLocal(_iStorage);

	return fg_GetThreadLocalForOtherThread(_ThreadID, _iStorage + 0x40);
}

mint NSys::fg_Thread_AllocLocalFast()
{
	DWORD Return = fg_TlsAllocInternal(true);
	if (Return == TLS_OUT_OF_INDEXES)
		DMibErrorSystemImp("fg_Thread_AllocLocalFast: Out of indices");
	return Return;
}

void NSys::fg_Thread_FreeLocalFast(mint _iStorage)
{
	if (!TlsFree(_iStorage))
	{
		DMibErrorSystemImp("fg_Thread_FreeLocalFast: Failed to free Thread Storage Index");
	}
}

void NSys::fg_Thread_SetLocalFast(mint _iStorage, void *_pData)
{
	if (!TlsSetValue(_iStorage, _pData))
	{
		DMibErrorSystemImp("fg_Thread_SetLocalFast: Failed to set Thread Storage Value");
	}
}

void NSys::fg_Thread_SetLocalFast(mint _ThreadID, mint _iStorage, void *_pData)
{
	if (NSys::fg_Thread_GetCurrentUID() == _ThreadID)
		return fg_Thread_SetLocalFast(_iStorage, _pData);

	fg_SetThreadLocalForOtherThread(_ThreadID, _iStorage, _pData);
}

void *NSys::fg_Thread_GetLocalFast(mint _ThreadID, mint _iStorage)
{
	if (NSys::fg_Thread_GetCurrentUID() == _ThreadID)
		return fg_Thread_GetLocalFast(_iStorage);

	return fg_GetThreadLocalForOtherThread(_ThreadID, _iStorage);
}

void *NSys::fg_Thread_GetLocalAlwaysSetFast(mint _ThreadID, mint _iStorage)
{
	if (NSys::fg_Thread_GetCurrentUID() == _ThreadID)
		return fg_Thread_GetLocalFast(_iStorage);

	return fg_GetThreadLocalForOtherThread(_ThreadID, _iStorage);
}

bool fg_Win32_RunningWine()
{
	return NLocal::g_OptionalFunctions.m_fWineGetVersion != nullptr;
}

NStr::CFStr256 fg_Win32_WineVersion()
{
	if (NLocal::g_OptionalFunctions.m_fWineGetVersion)
		return NLocal::g_OptionalFunctions.m_fWineGetVersion();
	return "";
}

void __cdecl fg_FixFunctionPointers()
{
}

void __cdecl fg_FixFunctionPointers_Alloc()
{
}

void fg_LoadFunctionPointers()
{
	using namespace NLocal;
	if (!g_hKernel32)
	{
		g_hKernel32 = GetModuleHandle(str_utf16("kernel32.dll"));
		g_hNtDll = GetModuleHandle(str_utf16("ntdll.dll"));
		g_hAdvAPI32 = GetModuleHandle(str_utf16("advapi32.dll"));
		g_hAPIMSWinCoreSynchl120 = GetModuleHandle(str_utf16("API-MS-Win-Core-Synch-l1-2-0.dll"));
		auto &Functions = g_OptionalFunctions;
		
		(FARPROC &)Functions.m_fGetLogicalProcessorInformation = GetProcAddress(g_hKernel32, "GetLogicalProcessorInformation");
		(FARPROC &)Functions.m_fRtlAcquirePebLock = GetProcAddress(g_hNtDll, "RtlAcquirePebLock");
		(FARPROC &)Functions.m_fRtlReleasePebLock = GetProcAddress(g_hNtDll, "RtlReleasePebLock");
		(FARPROC &)Functions.m_fRtlFindClearBitsAndSet = GetProcAddress(g_hNtDll, "RtlFindClearBitsAndSet");
		(FARPROC &)Functions.m_fRtlClearBits = GetProcAddress(g_hNtDll, "RtlClearBits");
		(FARPROC &)Functions.m_fNtQueryInformationThread = GetProcAddress(g_hNtDll, "NtQueryInformationThread");

		(FARPROC &)Functions.m_fAddVectoredExceptionHandler = GetProcAddress(g_hKernel32, "AddVectoredExceptionHandler");
		(FARPROC &)Functions.m_fGetNativeSystemInfo = GetProcAddress(g_hKernel32, "GetNativeSystemInfo");
		(FARPROC &)Functions.m_fRemoveVectoredExceptionHandler = GetProcAddress(g_hKernel32, "RemoveVectoredExceptionHandler");

		(FARPROC &)Functions.m_fSetProcessUserModeExceptionPolicy = GetProcAddress(g_hKernel32, "SetProcessUserModeExceptionPolicy");
		(FARPROC &)Functions.m_fGetProcessUserModeExceptionPolicy = GetProcAddress(g_hKernel32, "GetProcessUserModeExceptionPolicy");

		Functions.m_pKiUserApcDispatcher = GetProcAddress(g_hNtDll, "KiUserApcDispatcher");
		Functions.m_pKiUserCallbackDispatcher = GetProcAddress(g_hNtDll, "KiUserCallbackDispatcher");

		(FARPROC &)Functions.m_fWineGetVersion = GetProcAddress(g_hNtDll, "wine_get_version");

		(FARPROC &)Functions.m_fLargePageMinimum = GetProcAddress(g_hKernel32, "GetLargePageMinimum");

		(FARPROC &)Functions.m_fVirtualAllocExNuma = GetProcAddress(g_hKernel32, "VirtualAllocExNuma");
		(FARPROC &)Functions.m_fGetNumaNodeProcessorMaskEx = GetProcAddress(g_hKernel32, "GetNumaNodeProcessorMaskEx");
		(FARPROC &)Functions.m_fSetThreadGroupAffinity = GetProcAddress(g_hKernel32, "SetThreadGroupAffinity");

		(FARPROC &)Functions.m_fWTSGetActiveConsoleSessionId = GetProcAddress(g_hKernel32, "WTSGetActiveConsoleSessionId");

		(FARPROC &)Functions.m_fCreateProcessWithTokenW = GetProcAddress(g_hAdvAPI32, "CreateProcessWithTokenW");
		(FARPROC &)Functions.m_fCreateSymbolicLinkW = GetProcAddress(g_hKernel32, "CreateSymbolicLinkW");
		(FARPROC &)Functions.m_fCreateHardLinkW = GetProcAddress(g_hKernel32, "CreateHardLinkW");

		(FARPROC &)Functions.m_fWow64DisableWow64FsRedirection = GetProcAddress(g_hKernel32, "Wow64DisableWow64FsRedirection");
		(FARPROC &)Functions.m_fWow64RevertWow64FsRedirection = GetProcAddress(g_hKernel32, "Wow64RevertWow64FsRedirection");

		(FARPROC &)Functions.m_fNtSetInformationProcess = GetProcAddress(g_hNtDll, "NtSetInformationProcess");

		(FARPROC &)Functions.m_fSetProcessInformation = GetProcAddress(g_hKernel32, "SetProcessInformation");

		(FARPROC &)Functions.m_fCancelSynchronousIo = GetProcAddress(g_hKernel32, "CancelSynchronousIo");
		(FARPROC &)Functions.m_fCancelIoEx = GetProcAddress(g_hKernel32, "CancelIoEx");

		(FARPROC &)Functions.m_fNtQuerySystemInformation = GetProcAddress(g_hNtDll, "NtQuerySystemInformation");

		(FARPROC &)Functions.m_fNtGetNextThread = GetProcAddress(g_hNtDll, "NtGetNextThread");

		(FARPROC &)Functions.m_fGetThreadId = GetProcAddress(g_hKernel32, "GetThreadId");

		(FARPROC &)Functions.m_fNtQueryInformationProcess = GetProcAddress(g_hNtDll, "NtQueryInformationProcess");
		(FARPROC &)Functions.m_fLdrDisableThreadCalloutsForDll = GetProcAddress(g_hNtDll, "LdrDisableThreadCalloutsForDll");
		(FARPROC &)Functions.m_fRtlGetVersion = GetProcAddress(g_hNtDll, "RtlGetVersion");

		(FARPROC &)Functions.m_fGetFileInformationByHandleEx = GetProcAddress(g_hKernel32, "GetFileInformationByHandleEx");

		(FARPROC &)Functions.m_fPrivIsDllSynchronizationHeld = GetProcAddress(g_hKernel32, "PrivIsDllSynchronizationHeld");

		(FARPROC &)Functions.m_fWaitOnAddress = GetProcAddress(g_hKernel32, "WaitOnAddress");
		if (!Functions.m_fWaitOnAddress)
			(FARPROC &)Functions.m_fWaitOnAddress = GetProcAddress(g_hAPIMSWinCoreSynchl120, "WaitOnAddress");

		(FARPROC &)Functions.m_fWakeByAddressSingle = GetProcAddress(g_hKernel32, "WakeByAddressSingle");
		if (!Functions.m_fWakeByAddressSingle)
			(FARPROC &)Functions.m_fWakeByAddressSingle = GetProcAddress(g_hAPIMSWinCoreSynchl120, "WakeByAddressSingle");

		if (!Functions.m_fWakeByAddressSingle)
			(FARPROC &)Functions.m_fWaitOnAddress = nullptr;

		g_VersionInfo.dwOSVersionInfoSize = sizeof(g_VersionInfo);
		if (Functions.m_fRtlGetVersion)
			Functions.m_fRtlGetVersion((PRTL_OSVERSIONINFOW)&g_VersionInfo);
		else
			GetVersionExW((OSVERSIONINFO *)&g_VersionInfo);

		CSystem::ms_PlatformVersion = g_VersionInfo.dwMajorVersion * 10'000'000 + g_VersionInfo.dwMinorVersion * 1'000'000 + g_VersionInfo.dwBuildNumber;
	}

}

NAtomic::TCAtomicAggregate<smint> g_bDoneMalterlibInitAll = {0};

extern "C" void __cdecl __security_init_cookie(void);

namespace NLocal
{
	typedef enum
	{
		StateInitialized,
		StateReady,
		StateRunning,
		StateStandby,
		StateTerminated,
		StateWait,
		StateTransition,
		StateUnknown,
	} THREAD_STATE;

	typedef enum
	{
		Executive,
		FreePage,
		PageIn,
		PoolAllocation,
		DelayExecution,
		Suspended,
		UserRequest,
		WrExecutive,
		WrFreePage,
		WrPageIn,
		WrPoolAllocation,
		WrDelayExecution,
		WrSuspended,
		WrUserRequest,
		WrEventPair,
		WrQueue,
		WrLpcReceive,
		WrLpcReply,
		WrVirtualMemory,
		WrPageOut,
		WrRendezvous,
		Spare2,
		Spare3,
		Spare4,
		Spare5,
		Spare6,
		WrKernel,
		MaximumWaitReason
	} KWAIT_REASON;

	typedef struct _VM_COUNTERS
	{
		SIZE_T PeakVirtualSize;
		SIZE_T VirtualSize;
		ULONG PageFaultCount;
		SIZE_T PeakWorkingSetSize;
		SIZE_T WorkingSetSize;
		SIZE_T QuotaPeakPagedPoolUsage;
		SIZE_T QuotaPagedPoolUsage;
		SIZE_T QuotaPeakNonPagedPoolUsage;
		SIZE_T QuotaNonPagedPoolUsage;
		SIZE_T PagefileUsage;
		SIZE_T PeakPagefileUsage;
	} VM_COUNTERS, *PVM_COUNTERS;

	typedef struct _SYSTEM_THREAD
	{
		LARGE_INTEGER KernelTime;
		LARGE_INTEGER UserTime;
		LARGE_INTEGER CreateTime;
		ULONG WaitTime;
		PVOID StartAddress;
		CLIENT_ID ClientId;
		KPRIORITY Priority;
		KPRIORITY BasePriority;
		ULONG ContextSwitchCount;
		THREAD_STATE State;
		KWAIT_REASON WaitReason;
		DWORD Reserved;
	} SYSTEM_THREAD, *PSYSTEM_THREAD;

	typedef struct _SYSTEM_PROCESS_INFORMATION
	{
		ULONG NextEntryOffset;
		ULONG NumberOfThreads;
		ULONG Reserved1[6];
		LARGE_INTEGER CreateTime;
		LARGE_INTEGER UserTime;
		LARGE_INTEGER KernelTime;
		UNICODE_STRING ImageName;
		KPRIORITY BasePriority;
		HANDLE UniqueProcessId;
		HANDLE InheritedFromUniqueProcessId;
		ULONG HandleCount;
		ULONG SessionId;
		ULONG PageDirectoryBase;
		VM_COUNTERS VirtualMemoryCounters;
		SIZE_T PrivatePageCount;
		IO_COUNTERS IoCounters;
		SYSTEM_THREAD Threads[1];
	} SYSTEM_PROCESS_INFORMATION, *PSYSTEM_PROCESS_INFORMATION;

	typedef enum _SYSTEM_INFORMATION_CLASS 
	{
		SystemBasicInformation,					// 0
		SystemProcessorInformation,				// 1
		SystemPerformanceInformation,			// 2
		SystemTimeOfDayInformation,				// 3
		SystemPathInformation,					// 4
		SystemProcessInformation,				// 5
		SystemCallCountInformation,				// 6
		SystemDeviceInformation,				// 7
		SystemProcessorPerformanceInformation,	// 8
		SystemFlagsInformation,					// 9
		SystemCallTimeInformation,				// 10
		SystemModuleInformation,				// 11
		SystemLocksInformation,					// 12
		SystemStackTraceInformation,			// 13
		SystemPagedPoolInformation,				// 14
		SystemNonPagedPoolInformation,			// 15
		Undocumented_SystemHandleInformation,				// 16
		SystemObjectInformation,				// 17
		SystemPageFileInformation,				// 18
		SystemVdmInstemulInformation,			// 19
		SystemVdmBopInformation,				// 20
		SystemFileCacheInformation,				// 21
		SystemPoolTagInformation,				// 22
		SystemInterruptInformation,				// 23
		SystemDpcBehaviorInformation,			// 24
		SystemFullMemoryInformation,			// 25
		SystemLoadGdiDriverInformation,			// 26
		SystemUnloadGdiDriverInformation,		// 27
		SystemTimeAdjustmentInformation,		// 28
		SystemSummaryMemoryInformation,			// 29
		SystemNextEventIdInformation,			// 30
		SystemEventIdsInformation,				// 31
		SystemCrashDumpInformation,				// 32
		SystemExceptionInformation,				// 33
		SystemCrashDumpStateInformation,		// 34
		SystemKernelDebuggerInformation,		// 35
		SystemContextSwitchInformation,			// 36
		SystemRegistryQuotaInformation,			// 37
		SystemExtendServiceTableInformation,	// 38
		SystemPrioritySeperation,				// 39
		SystemPlugPlayBusInformation,			// 40
		SystemDockInformation,					// 41
		SystemPowerInformation,					// 42
		SystemProcessorSpeedInformation,		// 43
		SystemCurrentTimeZoneInformation,		// 44
		SystemLookasideInformation				// 45
	} SYSTEM_INFORMATION_CLASS, *PSYSTEM_INFORMATION_CLASS;

}

void fg_EnumProcessThreads(TCFunctionNoAlloc<void (mint _ThreadID)> const &_fOnThread);

void fg_InitMalterlibAllEnumOtherThreads()
{
	mint ThisUID = NSys::fg_Thread_GetCurrentUID();
	fg_EnumProcessThreads
		(
			[&](mint _ThreadID)
			{
				fg_GetLocalSys()->f_ThreadLocalCreateThread(_ThreadID, ThisUID);
			}
		)
	;
}

void fg_EnumProcessThreadsInternal(TCFunctionNoAlloc<bool (mint _ThreadID, HANDLE _pThread)> const &_fOnThread)
{
	mint ThisUID = NSys::fg_Thread_GetCurrentUID();
	uint32 CurrentProcess = GetCurrentProcessId();

	//
	if (NLocal::g_OptionalFunctions.m_fNtGetNextThread && NLocal::g_OptionalFunctions.m_fGetThreadId)
	{
		auto CurrentProcess = GetCurrentProcess();
		HANDLE hThread = nullptr;
		NLocal::g_OptionalFunctions.m_fNtGetNextThread(CurrentProcess, nullptr, THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, 0, 0, &hThread);
		while (hThread)
		{
			auto ThreadID = NLocal::g_OptionalFunctions.m_fGetThreadId(hThread);

			bool bCloseHandle = true;
			if (ThreadID && ThreadID != ThisUID)
			{
				bCloseHandle = !_fOnThread(ThreadID, hThread);
			}
			HANDLE hPrevThread = hThread;
			hThread = nullptr;
			NLocal::g_OptionalFunctions.m_fNtGetNextThread(CurrentProcess, hPrevThread, THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, 0, 0, &hThread);
			if (bCloseHandle)
				CloseHandle(hPrevThread);
		}

		return;
	}

	while (NLocal::g_OptionalFunctions.m_fNtQuerySystemInformation)
	{
		DWORD NeededSize = 0;
		NLocal::g_OptionalFunctions.m_fNtQuerySystemInformation(SystemProcessInformation, nullptr, 0, &NeededSize);
		if (!NeededSize)
			break;

		NeededSize *= 2;
		TCVector<uint8, NMemory::CAllocator_VirtualNoTracking> Data;
		Data.f_SetLen(NeededSize);
		NLocal::SYSTEM_PROCESS_INFORMATION *pInfo = (NLocal::SYSTEM_PROCESS_INFORMATION *)Data.f_GetArray();

		if (NTSTATUS RetVal = NLocal::g_OptionalFunctions.m_fNtQuerySystemInformation(SystemProcessInformation, pInfo, NeededSize, &NeededSize))
			break;

		int32 SizeLeft = NeededSize;
		while (SizeLeft > 0)
		{
			if ((mint)pInfo->UniqueProcessId == CurrentProcess)
			{
				for (mint i = 0; i < pInfo->NumberOfThreads; ++i)
				{
					auto &Thread = pInfo->Threads[i];
					mint ThreadID = (mint)Thread.ClientId.UniqueThread;
					if (Thread.State != NLocal::StateTerminated && ThreadID != ThisUID)
						_fOnThread((mint)ThreadID, nullptr);
				}
				return;
			}

			if (!pInfo->NextEntryOffset)
				break;
			SizeLeft -= pInfo->NextEntryOffset;
			pInfo = (NLocal::SYSTEM_PROCESS_INFORMATION *)((mint)pInfo + pInfo->NextEntryOffset);
		}
		break;
	}

	{
		HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, GetCurrentProcessId());
		if (h != INVALID_HANDLE_VALUE) 
		{
			THREADENTRY32 te;
			te.dwSize = sizeof(te);
			if (Thread32First(h, &te)) 
			{
				do 
				{
					if 
						(
							(
								te.dwSize 
								>= 
								(
									FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) 
									+ sizeof(te.th32OwnerProcessID) 
								)
							)
							&& te.th32OwnerProcessID == CurrentProcess && te.th32ThreadID != ThisUID
						) 
					{
						//DMibTrace("Process {} Thread {}\n", te.th32OwnerProcessID << te.th32ThreadID);
						_fOnThread(te.th32ThreadID, nullptr);
					}
					te.dwSize = sizeof(te);
				} while (Thread32Next(h, &te));
			}
			CloseHandle(h);
		}
	}
}

namespace NPrivate
{
	struct CEnumThreadEntry
	{
		mint m_ThreadID;
		HANDLE m_pThread;
	};

	struct CEnumThreadEntrySort
	{
		COrdering_Partial operator ()(CEnumThreadEntry const &_Left, CEnumThreadEntry const &_Right) const
		{
			return _Left.m_ThreadID <=> _Right.m_ThreadID;
		}
		COrdering_Partial operator ()(CEnumThreadEntry const &_Left, mint _Right) const
		{
			return _Left.m_ThreadID <=> _Right;
		}
		COrdering_Partial operator ()(mint _Left, CEnumThreadEntry const &_Right) const
		{
			return _Left <=> _Right.m_ThreadID;
		}
	};

	bool fg_ThreadReady(HANDLE _pThread, bool &o_bValid)
	{
		NLocal::THREAD_BASIC_INFORMATION ThreadInfo;
		if (!NT_SUCCESS(NLocal::g_OptionalFunctions.m_fNtQueryInformationThread(_pThread, (::THREADINFOCLASS)NLocal::ThreadBasicInformation, &ThreadInfo, sizeof( NLocal::THREAD_BASIC_INFORMATION ), 0)))
			return false;
		CUndocumentedTEB *pOtherTEB = (CUndocumentedTEB *)ThreadInfo.TebBaseAddress;
		if (!pOtherTEB)
			return false;

		if (NLocal::g_VersionInfo.dwMajorVersion >= 10 && (pOtherTEB->LoaderWorker || pOtherTEB->SkipThreadAttach))
			o_bValid = false;

		return true;
	}

}


void fg_EnumProcessThreads(TCFunctionNoAlloc<void (mint _ThreadID)> const &_fOnThread)
{
	// Enum threads
	//	Suspend thread
	// Repeat until no change
	// Let user do thread manipulation
	// Resume threads

	using namespace ::NPrivate;

	struct CState
	{
		TCVector<CEnumThreadEntry, NMemory::CAllocator_VirtualNoTracking> m_Threads;
		mint m_nEnum = 0;
		mint m_nSuspend = 0;
		mint m_nReady = 0;
		mint m_nOpen = 0;
		bool m_bDoneSomething = true;
	};

	CState State;

	while (State.m_bDoneSomething)
	{
		State.m_bDoneSomething = false;
		mint nStartingThreads = State.m_Threads.f_GetLen();
		++State.m_nEnum;
		fg_EnumProcessThreadsInternal
			(
				[&](mint _ThreadID, HANDLE _pThread) -> bool
				{
					if (State.m_Threads.f_BinarySearch(CEnumThreadEntrySort(), _ThreadID, nStartingThreads) >= 0)
						return false;
					State.m_bDoneSomething = true;
					bool bOwnThread = !_pThread;
					if (!_pThread)
					{
						_pThread = OpenThread(THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, false, _ThreadID);
						if (!_pThread)
						{
							++State.m_nOpen;
							return false;
						}
					}
					if (SuspendThread(_pThread) == 0xFFFFFFFF)
					{
						FILETIME CreationTime = {0};
						FILETIME ExitTime = {0};
						FILETIME KernelTime = {0};
						FILETIME UserTime = {0};

						GetThreadTimes(_pThread, &CreationTime, &ExitTime, &KernelTime, &UserTime);

						if (bOwnThread)
							CloseHandle(_pThread);

						if (ExitTime.dwHighDateTime != 0 || ExitTime.dwLowDateTime != 0)
						{
							// Thread has exited, so we are not interested in it
							auto &NewThread = State.m_Threads.f_Insert();
							NewThread.m_ThreadID = _ThreadID;
							NewThread.m_pThread = nullptr;
						}
						++State.m_nSuspend;
						return false;
					}
					bool bValid = true;
					if (!fg_ThreadReady(_pThread, bValid))
					{
						ResumeThread(_pThread);
						if (bOwnThread)
							CloseHandle(_pThread);
						++State.m_nReady;
						return false;
					}
					if (!bValid)
					{
						ResumeThread(_pThread);
						if (bOwnThread)
							CloseHandle(_pThread);
						auto &NewThread = State.m_Threads.f_Insert();
						NewThread.m_ThreadID = _ThreadID;
						NewThread.m_pThread = nullptr;
						return false;
					}

					auto &NewThread = State.m_Threads.f_Insert();
					NewThread.m_ThreadID = _ThreadID;
					NewThread.m_pThread = _pThread;
					return true;
				}
			)
		;
		State.m_Threads.f_Sort(CEnumThreadEntrySort());
	}

	for (auto &Thread : State.m_Threads)
	{
		if (Thread.m_pThread)
			_fOnThread(Thread.m_ThreadID);
	}

	for (auto &Thread : State.m_Threads)
	{
		if (Thread.m_pThread)
		{
			ResumeThread(Thread.m_pThread);
			CloseHandle(Thread.m_pThread);
		}
	}
}

void fg_CheckProcessStop()
{
	using namespace NStr;
	ch16 OutputVariable[257];
	uint32 Size = GetEnvironmentVariableW(L"MalterlibLaunchStopProcess", OutputVariable, 256);

	if (!Size || Size > 256)
		return;

	NStr::CFWStr256 Value(OutputVariable, Size);

	uint32 ProcessID = Value.f_ToInt(uint32(0));
	if (!ProcessID)
	{
		fg_ConsoleOutputHelper<CFWStr256, CFStr256>("Invalid process ID\n", STD_ERROR_HANDLE, true);
		NMib::NSys::fg_TerminateProcess(1);
		return;
	}

    FreeConsole();
    if (!AttachConsole(ProcessID))
	{
		fg_ConsoleOutputHelper<CFWStr256, CFStr256>(fg_Format<CFStr256>("Failed to attach to console: {}\n", NMib::NPlatform::fg_Win32_GetLastErrorStr()), STD_ERROR_HANDLE, true);
		NMib::NSys::fg_TerminateProcess(1);
		return;
	}

    // Disable Ctrl-C handling for our program
    SetConsoleCtrlHandler(nullptr, true);

	if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, ProcessID))
	{
		fg_ConsoleOutputHelper<CFWStr256, CFStr256>(fg_Format<CFStr256>("Failed to generate CTRL_BREAK_EVENT: {}\n", NMib::NPlatform::fg_Win32_GetLastErrorStr()), STD_ERROR_HANDLE, true);
		NMib::NSys::fg_TerminateProcess(1);
		return;
	}
 
    FreeConsole();
 
	NMib::NSys::fg_TerminateProcess(0);
}

void fg_MakeTlsActive();
inline_never void fg_InitMalterlibAllInternalComplex(void *_pInstance)
{
	fg_LoadFunctionPointers();

	{
		auto pTeb = fg_GetTEB();
		UndocumentedPEB *pPeb = fg_GetPEB(pTeb);

		if (_pInstance == nullptr || _pInstance == pPeb->ImageBaseAddress)
		{
			g_bIsDll = false;
			g_hDllInstance = (HINSTANCE)(pPeb->ImageBaseAddress);
		}
		else
		{
			g_bIsDll = true;
			g_hDllInstance = (HINSTANCE)_pInstance;
		}
	}

	fg_CheckProcessStop();

	fg_CreateMalterlib();

	fg_FixFunctionPointers_Alloc();

	mint ThisUID = NSys::fg_Thread_GetCurrentUID();
	fg_GetLocalSys()->f_ThreadLocalCreateThread(ThisUID, 0);
	
	fg_InitMalterlibAllEnumOtherThreads();

	g_bDoneMalterlibInitAll.f_FetchAdd(1);

	fg_GetLocalSys()->f_InitModuleThreaded();

	NSys::fg_Compiler_MakeActive(&g_OffsetThreadLocalOffset);
	NSys::fg_Compiler_MakeActive(&g_DebugTIB);
	
	fg_MakeTlsActive();

	g_bDoneMalterlibInitAll.f_FetchAdd(1);
}

inline_never bool __cdecl fg_InitMalterlibAllInternal(void *_pInstance)
{
	smint Expected = 0;
	if (!g_bDoneMalterlibInitAll.f_CompareExchangeStrong(Expected, 1))
		return false;

	if (!NMib::NSys::fg_HW_MeetsMinimumRequirements())
	{
		MessageBoxA(	NULL
					,	"Your computer does not meet the minimum system requirements for this application.\n\nPlease upgrade or use a newer computer."
					,	"Malterlib based application"
					,	MB_ICONERROR | MB_OK);
		NMib::NSys::fg_TerminateProcess(0);
	}

	// Init security cookies the first thing we do

	fg_FixFunctionPointers();

	// We need to keep this function as simple as possible so no security cookie is inserted in this function
	__security_init_cookie();

	fg_InitMalterlibAllInternalComplex(_pInstance);
	return true;
}

void __cdecl fg_InitMalterlibAll(void *_pInstance)
{
	fg_InitMalterlibAllInternal(_pInstance);
}

void * __cdecl fg_MalterlibAllocNonTracked(size_t _Size)
{
	mint Size = _Size;
	void *pMem = CAllocator_NonTrackedHeap::f_Alloc(Size);
	memset(pMem, 0, _Size);
	return pMem;
}

bool NMib::NPlatform::fg_ThisThreadOwnsDllLock()
{
	if (NLocal::g_OptionalFunctions.m_fPrivIsDllSynchronizationHeld)
	{
		BOOL bHeld = false;
		NLocal::g_OptionalFunctions.m_fPrivIsDllSynchronizationHeld(&bHeld);
		return bHeld != 0;
	}
	UndocumentedPEB *pPeb = fg_GetPEB(fg_GetTEB());
	return (uint32)(mint)pPeb->LoaderLock->OwningThread == GetCurrentThreadId();
}


namespace
{
	bool g_bTerminatedThread = false;
	bool g_bCheckedTerminatedThread = false;

	bool fg_CheckTerminatedThread()
	{
		if (g_bCheckedTerminatedThread)
			return g_bTerminatedThread;
		g_bCheckedTerminatedThread = true;
		bool bTerminatedThread = false;
		mint nThreads = 0;
		fg_GetSys()->f_ThreadEnum
			(
				[&](mint _ThreadID)
				{
					++nThreads;
					HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT, false, _ThreadID);
					if (!hThread)
					{
						//DMibTraceSafe("BAD thread: {}\n", _ThreadID);
						bTerminatedThread = true;
					}
					else
					{
						NLocal::THREAD_BASIC_INFORMATION ThreadInfo;
						if (NT_SUCCESS(NLocal::g_OptionalFunctions.m_fNtQueryInformationThread(hThread, (::THREADINFOCLASS)NLocal::ThreadBasicInformation, &ThreadInfo, sizeof( NLocal::THREAD_BASIC_INFORMATION ), 0)))
						{
							//DMibTraceSafe("Good thread: {}\n", _ThreadID);
						}
						else
						{
							//DMibTraceSafe("BAD thread 2: {}\n", _ThreadID);
							bTerminatedThread = true;
						}

						CloseHandle(hThread);
					}
				}
			)
		;

		if (nThreads > 1)
		{
			//DMibTraceSafe("More than one thread at exit: {}\n", nThreads);
			bTerminatedThread = true;
		}

		g_bTerminatedThread = bTerminatedThread;
		return bTerminatedThread;
	}

	bool fg_TerminatedThread()
	{
		//if (g_bIsDll)
		{
			if (NLocal::g_OptionalFunctions.m_fLdrDisableThreadCalloutsForDll)
			{
				auto Ret = NLocal::g_OptionalFunctions.m_fLdrDisableThreadCalloutsForDll((void *)(mint)1);

				// Due to an implementation detail in ntdll.dll this means that the process is currently exiting
				if (Ret == 0)
				{
					return fg_CheckTerminatedThread();
				}
			}
		}

		return false;
	}
}


void __cdecl fg_MalterlibFreeNonTracked(void *_pMem)
{
	if (!fg_TerminatedThread()) // If thread was terminated incorretly it's not safe to delete memory
		CAllocator_NonTrackedHeap::f_FreeNoSize(_pMem);
}


extern bool g_bSysDeleted;

extern "C" BOOL WINAPI fg_MalterlibDllMain(HANDLE _pInstance, DWORD _Reason, void *_pReserved)
{
	if (_Reason == DLL_PROCESS_ATTACH)
	{
		fg_InitMalterlibAllInternal(_pInstance);
			//DMibDTraceSafe("fg_MalterlibDllMain({}): Process attach {} {}\r\n", NSys::fg_Thread_GetCurrentUID() << _pInstance << _pReserved);
		++gs_LibraryRefCount;
	}
	else if (_Reason == DLL_PROCESS_DETACH)
	{
		//DMibDTraceSafe("fg_MalterlibDllMain({}): Process detach {} {}\r\n", NSys::fg_Thread_GetCurrentUID() << _pInstance << _pReserved);
	}
	else if (_Reason == DLL_THREAD_ATTACH)
	{
		//DMibDTraceSafe("fg_MalterlibDllMain({}): Thread attach {} {}\r\n", NSys::fg_Thread_GetCurrentUID() << _pInstance << _pReserved);
	}
	else if (_Reason == DLL_THREAD_DETACH)
	{
		if (!g_bSysDeleted)
		{
			//DMibDTraceSafe("fg_MalterlibDllMain({}): Thread dettach {} {}\r\n", NSys::fg_Thread_GetCurrentUID() << _pInstance << _pReserved);
			fg_GetLocalSys()->f_OnThreadDestroyed();
		}
	}

	return 1;
}

extern "C" BOOL (WINAPI * const _pRawDllMain)(HANDLE, DWORD, LPVOID) = &fg_MalterlibDllMain;

void NTAPI fg_TLSCallback(void *_pInstance, DWORD _Reason, void *_pReserved)
{
	if (_Reason == DLL_PROCESS_ATTACH)
	{
		fg_InitMalterlibAllInternal(_pInstance);
		//DMibDTraceSafe("fg_TLSCallback({}): Process attach {} {} {}\r\n", NSys::fg_Thread_GetCurrentUID() << _pInstance << _pReserved << g_bIsDll);
	}
	else if (_Reason == DLL_PROCESS_DETACH)
	{
		g_bProcessDetached = true;
		if (!g_bSysDeleted && !g_bIsDll)
		{
			if (fg_CheckTerminatedThread())
				return;
			fg_DestroyMalterlib();
			//fg_GetLocalSys()->f_OnThreadDestroyed();
			//DMibDTraceSafe("fg_TLSCallback({}): Process detach {} {} {}\r\n", NSys::fg_Thread_GetCurrentUID() << _pInstance << _pReserved << g_bIsDll);
		}
	}
	else if (_Reason == DLL_THREAD_ATTACH)
	{
		//DMibDTraceSafe("fg_TLSCallback({}): Thread attach {} {} {}\r\n", NSys::fg_Thread_GetCurrentUID() << _pInstance << _pReserved << g_bIsDll);
		if (!g_bSysDeleted)
		{
			// Seems to be needed in Wine to wait until dll/process initialization is done (Windows does not start new threads in Ldr code).
			while (g_bDoneMalterlibInitAll.f_Load() < 2)
				Sleep(1);
			mint ParentThread = (mint)NSys::fg_Thread_GetLocal(gs_ThreadLocalParentThread);
			fg_GetLocalSys()->f_OnThreadCreated(NSys::fg_Thread_GetCurrentUID(), ParentThread);
		}
	}
	else if (_Reason == DLL_THREAD_DETACH)
	{
		if (!g_bSysDeleted && !g_bIsDll)
		{
			fg_GetLocalSys()->f_OnThreadDestroyed();
			//DMibDTraceSafe("fg_TLSCallback({}): Thread detach {} {} {}\r\n", NSys::fg_Thread_GetCurrentUID() << _pInstance << _pReserved << g_bIsDll);
		}
	}
}

extern "C" BOOL NTAPI fg_MalterlibDllMainCallback(void *_pInstance, DWORD _Reason, void *_pReserved)
{
	if (_Reason == DLL_PROCESS_DETACH && !g_bSysDeleted && g_bIsDll && _pReserved)
	{
		if (fg_CheckTerminatedThread())
		{
			//DMibTraceSafe("DETECTED thread TERMINATION, not running C cleanup\n", 0);
			return true;
		}
	}
	fg_TLSCallback(_pInstance, _Reason, _pReserved);
	return false;
}


// put a pointer in a special segment


#ifdef _M_IX86
#pragma comment (linker, "/INCLUDE:__tls_used")
#else
#pragma comment (linker, "/INCLUDE:_tls_used")
#endif

#pragma section(".CRT$XLB",long,read)

extern "C" __declspec(allocate(".CRT$XLB")) PIMAGE_TLS_CALLBACK g_MalterlibTlsCallback = fg_TLSCallback;

void fg_MakeTlsActive()
{
	NSys::fg_Compiler_MakeActive(0, &g_MalterlibTlsCallback);
}
class CThreadParameters
{
public:
	FThreadProc *m_pProc;
	void *m_pParam;
	NStr::CStrNonTracked m_Name;
};

void fg_SetThreadName( DWORD _ThreadID, CHAR const *_pThreadName)
{
	UndocumentedPEB *pPeb = fg_GetPEB(fg_GetTEB());
	if (!pPeb->BeingDebugged)
		return;

	THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = _pThreadName;
	info.dwThreadID = _ThreadID;
	info.dwFlags = 0;

	__try
	{
		RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info );
	}
	__except(EXCEPTION_CONTINUE_EXECUTION)
	{
	}
}

DWORD WINAPI fg_MalterlibMSVC_ThreadProc(void *_pParameter)
{
	TCUniquePointer<CThreadParameters, NMemory::CAllocator_NonTrackedHeap> pThreadParameters = fg_Explicit((CThreadParameters *)_pParameter);
	FThreadProc *pProc = pThreadParameters->m_pProc;
	void *pParam = pThreadParameters->m_pParam;
	fg_SetThreadName(GetCurrentThreadId(), pThreadParameters->m_Name);
	pThreadParameters.f_Clear();
	return pProc(pParam);
}

namespace
{
	int fg_TranslateThreadPrio(EExecutionPriority _Priority)
	{
		int Prio = THREAD_PRIORITY_NORMAL;
		if (_Priority < EExecutionPriority_Low)
			Prio = THREAD_PRIORITY_IDLE;
		else if (_Priority < EExecutionPriority_BelowNormal)
			Prio = THREAD_PRIORITY_LOWEST;
		else if (_Priority < EExecutionPriority_Normal)
			Prio = THREAD_PRIORITY_BELOW_NORMAL;
		else if (_Priority < EExecutionPriority_AboveNormal)
			Prio = THREAD_PRIORITY_NORMAL;
		else if (_Priority < EExecutionPriority_High)
			Prio = THREAD_PRIORITY_ABOVE_NORMAL;
		else if (_Priority < EExecutionPriority_Highest)
			Prio = THREAD_PRIORITY_HIGHEST;
		else
			Prio = THREAD_PRIORITY_TIME_CRITICAL;

		return Prio;
	}
}

void *NSys::fg_Thread_BeginDestroy(void *_pThread)
{
	HANDLE Ret = INVALID_HANDLE_VALUE;
	if (!DuplicateHandle(GetCurrentProcess(), _pThread, GetCurrentProcess(), &Ret, SYNCHRONIZE, false, 0))
	{
		DMibErrorSystemImp((CFStr256::CFormat("Windows returned an error from DuplicateHandle: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}
	return Ret;
	//return OpenThread(SYNCHRONIZE, false, (uint32)_pThread);
}

void NSys::fg_Thread_WillNotBlockUntilExit(void *_pThreadDestroyContext)
{
}

void NSys::fg_Thread_BlockUntilExit(void *_pThreadDestroyContext)
{
	if (NMib::NPlatform::fg_ThisThreadOwnsDllLock())
	{
		Sleep(10);
		return;
	}
	if (_pThreadDestroyContext != INVALID_HANDLE_VALUE)
		WaitForSingleObject(_pThreadDestroyContext, INFINITE);
}

void NSys::fg_Thread_EndDestroy(void *_pThreadDestroyContext)
{
	if (_pThreadDestroyContext != INVALID_HANDLE_VALUE)
		CloseHandle(_pThreadDestroyContext);
}

void *NSys::fg_Thread_Create
	(
		FThreadProc *_pThreadProc
		, void *_pParam
		, EExecutionPriority _Priority
		, mint _StackSize
		, bool _bSuspended
		, const ch8 *_pThreadName
		, mint _Affinity
		, mint &_ThreadID
	)
{
	TCUniquePointer<CThreadParameters, NMemory::CAllocator_NonTrackedHeap> pThreadParameters = fg_Construct();

	pThreadParameters->m_pProc = _pThreadProc;
	pThreadParameters->m_pParam = _pParam;
	pThreadParameters->m_Name = _pThreadName;

	DWORD ThreadID;
	HANDLE hThread = CreateThread(nullptr, _StackSize, fg_MalterlibMSVC_ThreadProc, pThreadParameters.f_Get(), CREATE_SUSPENDED, &ThreadID);
	if (!hThread)
	{
		DMibErrorSystemImp((CFStr256::CFormat("Windows returned an error from CreateThread: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}

	pThreadParameters.f_Detach();

	// This is needed to fix a wine bug where the TebAddress is not set for the thread until after a while
	while (1)
	{
		NLocal::THREAD_BASIC_INFORMATION ThreadInfo;
		if (NT_SUCCESS(NLocal::g_OptionalFunctions.m_fNtQueryInformationThread(hThread, (::THREADINFOCLASS)NLocal::ThreadBasicInformation, &ThreadInfo, sizeof( NLocal::THREAD_BASIC_INFORMATION ), 0)))
		{
			if (fg_Volatile(ThreadInfo.TebBaseAddress))
				break;
		}
	}

	NSys::fg_Thread_SetLocal(ThreadID, gs_ThreadLocalParentThread, (void *)NSys::fg_Thread_GetCurrentUID());

	_ThreadID = ThreadID;
	if (_Affinity)
		SetThreadAffinityMask(hThread, _Affinity);

	fg_Thread_SetPriority(hThread, _Priority);

	if (!_bSuspended)
		ResumeThread(hThread);

	return hThread;
}

void NSys::fg_Thread_EnumOtherThreadsInProcess(NFunction::TCFunctionNoAlloc<void (mint _ThreadID)> const &_fOnThread)
{
	fg_EnumProcessThreads(_fOnThread);
}

void NSys::fg_Thread_Suspend(void *_pThread)
{
	if (SuspendThread(_pThread) == 0xFFFFFFFF)
	{
		DMibErrorSystemImp((CFStr256::CFormat("Windows returned an error from SuspendThread: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}

void NSys::fg_Thread_Resume(void *_pThread)
{
	if (ResumeThread(_pThread) == 0xFFFFFFFF)
	{
		DMibErrorSystemImp((CFStr256::CFormat("Windows returned an error from ResumeThread: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}

void NSys::fg_Thread_SetPriority(void *_pThread, EExecutionPriority _Priority)
{
	if (!SetThreadPriority(_pThread, fg_TranslateThreadPrio(_Priority)))
	{
		DMibErrorSystemImp((CFStr256::CFormat("Windows returned an error from SetThreadPriority: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}

void NSys::fg_Thread_SetAffinity(void *_pThread, mint _Affinity)
{
	if (!SetThreadAffinityMask(_pThread, _Affinity))
	{
		DMibErrorSystemImp((CFStr256::CFormat("Windows returned an error from SetThreadAffinityMask: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}

void NSys::fg_Thread_Destroy(void *_pThread)
{
	if (_pThread != INVALID_HANDLE_VALUE)
		CloseHandle(_pThread);
}


void NSys::fg_Process_SetCrossModuleMemoryManagerInterface(void *_pInterface)
{
	DMibFastCheck(!fg_GetSys()->f_IsDll());

	if (!FindAtom(str_utf16("IdsCrossModuleMemManAtom")))
	{
		AddAtomW(str_utf16("IdsCrossModuleMemManAtom"));
		mint Pointer = (mint)_pInterface;
		for (mint i = 0; i < sizeof(mint) * 8; ++i)
		{
			if (Pointer & (mint(1) << i))
			{
				AddAtom(CFWStr128(CFWStr128::CFormat(str_utf16("IdsCrossModuleMemManAtom{}")) << i));
			}
		}
	}
	else
	{
		DMibFastCheck(false); // Should not be possible
	}
	
	DMibFastCheck(fg_Process_GetCrossModuleMemoryManagerInterface() == _pInterface);
}

void *NSys::fg_Process_GetCrossModuleMemoryManagerInterface()
{
	mint Pointer = 0;
	// This needs to be named exactly like this to be compatible with old versions of library (when Malterlib was named Ids)
	if (FindAtomW(str_utf16("IdsCrossModuleMemManAtom")))
	{
		for (mint i = 0; i < sizeof(mint) * 8; ++i)
		{
			if (FindAtomW(CFWStr128(CFWStr128::CFormat(str_utf16("IdsCrossModuleMemManAtom{}")) << i)))
				Pointer |= (mint(1) << i);
		}
	}
	return (void *)Pointer;
}


void NSys::fg_Security_GenerateHighEntropyData(uint8 *_pData, mint _nBytes)
{
	HCRYPTPROV hProvider = 0;

	if (!CryptAcquireContextW(&hProvider, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
		DMibError((CFStr256::CFormat("Windows returned an error from CryptAcquireContextW: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(GetLastError())).f_GetStr());

	auto Cleanup = fg_OnScopeExit
		(
			[&]()
			{
				CryptReleaseContext(hProvider, 0);
			}
		)
	;
	if (!CryptGenRandom(hProvider, _nBytes, _pData))
		DMibError((CFStr256::CFormat("Windows returned an error from CryptGenRandom: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(GetLastError())).f_GetStr());
}


#include "rpc.h"
#pragma comment(lib, "rpcrt4.lib")


void NSys::fg_System_GenerateUUID(NCryptography::CUniversallyUniqueIdentifier &_UUID)
{
	
	UUID Ret;
	HRESULT ErrorCode = UuidCreate(&Ret);
	if (ErrorCode != RPC_S_OK)
		DMibError((CFStr256::CFormat("Windows returned an error from UuidCreate: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(ErrorCode)).f_GetStr());
	
	_UUID.m_TimeLow = Ret.Data1;
	_UUID.m_TimeMid = Ret.Data2;
	_UUID.m_TimeHiAndVersion = Ret.Data3;
	_UUID.m_ClockSequenceHiAndReserved = Ret.Data4[0];
	_UUID.m_ClockSquenceLow = Ret.Data4[1];
	_UUID.m_Node[0] = Ret.Data4[2];
	_UUID.m_Node[1] = Ret.Data4[3];
	_UUID.m_Node[2] = Ret.Data4[4];
	_UUID.m_Node[3] = Ret.Data4[5];
	_UUID.m_Node[4] = Ret.Data4[6];
	_UUID.m_Node[5] = Ret.Data4[7];

	DMibFastCheck
		(
			CFStr256(CFStr256::CFormat("{{{nfh,sj8,sf0}-{nfh,sj4,sf0}-{nfh,sj4,sf0}-{nfh,sj2,sf0}{nfh,sj2,sf0}-{nfh,sj2,sf0}{nfh,sj2,sf0}{nfh,sj2,sf0}{nfh,sj2,sf0}{nfh,sj2,sf0}{nfh,sj2,sf0}}") << Ret.Data1 << Ret.Data2 << Ret.Data3 << Ret.Data4[0] << Ret.Data4[1] << Ret.Data4[2] << Ret.Data4[3] << Ret.Data4[4] << Ret.Data4[5] << Ret.Data4[6] << Ret.Data4[7])
			.f_CmpNoCase(_UUID.f_GetAsStaticString()) == 0
		)
	;
}

NStr::CStr NSys::fg_System_GenerateUUID()
{
	UUID Ret;
	HRESULT ErrorCode = UuidCreate(&Ret);
	if (ErrorCode != RPC_S_OK)
		DMibError((CStr::CFormat("Windows returned an error from UuidCreate: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(ErrorCode)).f_GetStr());

	// {7EE072A7-458D-491f-ACCF-447AD4BE8DBF}
	return CStr(CStr::CFormat("{{{nfh,sj8,sf0}-{nfh,sj4,sf0}-{nfh,sj4,sf0}-{nfh,sj2,sf0}{nfh,sj2,sf0}-{nfh,sj2,sf0}{nfh,sj2,sf0}{nfh,sj2,sf0}{nfh,sj2,sf0}{nfh,sj2,sf0}{nfh,sj2,sf0}}") << Ret.Data1 << Ret.Data2 << Ret.Data3 << Ret.Data4[0] << Ret.Data4[1] << Ret.Data4[2] << Ret.Data4[3] << Ret.Data4[4] << Ret.Data4[5] << Ret.Data4[6] << Ret.Data4[7]);
}


uint16 NSys::fg_Langague_GetSystemLanguage(NMib::NStr::CStr &_Language)
{
	return GetUserDefaultUILanguage();
}
void *NSys::fg_Module_Get(mint &_ModuleSize)
{
	MEMORY_BASIC_INFORMATION MemInfo;
	if (VirtualQuery(&NSys::fg_Module_Get, &MemInfo, sizeof(MemInfo)))
	{
		uint8 *pBase = (uint8 *)MemInfo.AllocationBase;
		uint8 *pTest = (uint8 *)MemInfo.BaseAddress + MemInfo.RegionSize;
		_ModuleSize = pTest - pBase;

		VirtualQuery(pTest, &MemInfo, sizeof(MemInfo));
		while (MemInfo.AllocationBase == pBase)
		{
			pTest = (uint8 *)MemInfo.BaseAddress + MemInfo.RegionSize;
			_ModuleSize = pTest - pBase;
			VirtualQuery(pTest, &MemInfo, sizeof(MemInfo));
		}
		return pBase;
	}
	return nullptr;
}

NStr::CStr NSys::fg_Process_GetCommandLine()
{
	CWStr Temp = GetCommandLineW();
	return Temp;
}

NContainer::TCMap<NMib::NStr::CStr, NMib::NStr::CStr> NSys::fg_Process_GetEnvironmentVariables_NonProtected()
{
	TCMap<NMib::NStr::CStr, NMib::NStr::CStr> Ret;
	LPWSTR pStrings = GetEnvironmentStringsW();
	if (!pStrings)
		DMibError((CStr::CFormat("Windows returned an error from GetEnvironmentStringsW: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	LPWSTR pStringsOrig = pStrings;
	auto Cleanup 
		= fg_OnScopeExit
		(
			[&]
			{
				if (pStringsOrig)
					FreeEnvironmentStringsW(pStringsOrig);
			}
		)
	;
	while (pStrings && *pStrings)
	{
		CStr String = CWStr(pStrings);
		CStr Key;
		CStr Value;
		if (String[0] == '=')
		{
			String = String.f_Extract(1);
			Key = "=" + fg_GetStrSep(String, "=");
		}
		else
			Key = fg_GetStrSep(String, "=");
		Value = String;
		Ret[Key] = Value;
		pStrings += fg_StrLen(pStrings) + 1;
	}

	return Ret;
}

NMib::NStr::CStr NSys::fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStr const &_VariableName)
{
	NMib::NStr::CWStr Temp;
	ch16 *pStr = Temp.f_GetStr(NMib::NStr::NPlatform::gc_MaxWindowsEnvVarLength);
	pStr[0] = 0;
	GetEnvironmentVariableW(NMib::NStr::NPlatform::fg_StrToWindows(_VariableName), pStr, NMib::NStr::NPlatform::gc_MaxWindowsEnvVarLength);
	Temp.f_TrimSize();
	return Temp;
}

bool NSys::fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStr const &_VariableName, NMib::NStr::CStr &_Value)
{
	NMib::NStr::CWStr Temp;
	ch16 *pStr = Temp.f_GetStr(NMib::NStr::NPlatform::gc_MaxWindowsEnvVarLength);
	pStr[0] = 0;
	if (!GetEnvironmentVariableW(NMib::NStr::NPlatform::fg_StrToWindows(_VariableName), pStr, NMib::NStr::NPlatform::gc_MaxWindowsEnvVarLength))
		return false;

	Temp.f_TrimSize();
	_Value = fg_Move(Temp);
	return true;
}

void NMib::NSys::fg_Process_SetEnvironmentVariable_Unsafe(NMib::NStr::CStr const &_VariableName, NMib::NStr::CStr const &_Value)
{
	if (!SetEnvironmentVariableW(NMib::NStr::NPlatform::fg_StrToWindows(_VariableName), NMib::NStr::NPlatform::fg_StrToWindows(_Value)))
		DMibError((CStr::CFormat("Windows returned an error from SetEnvironmentVariableW: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
}

NMib::NStr::CStrNonTracked NSys::fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStrNonTracked const &_VariableName)
{
	NMib::NStr::CWStrNonTracked Temp;
	ch16 *pStr = Temp.f_GetStr(NMib::NStr::NPlatform::gc_MaxWindowsEnvVarLength);
	pStr[0] = 0;
	GetEnvironmentVariableW(NMib::NStr::NPlatform::fg_StrToWindows<NMib::NStr::CWStrNonTracked>(_VariableName), pStr, NMib::NStr::NPlatform::gc_MaxWindowsEnvVarLength);
	Temp.f_TrimSize();
	return Temp;
}

bool NSys::fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStrNonTracked const &_VariableName, NMib::NStr::CStrNonTracked &_Value)
{
	NMib::NStr::CWStrNonTracked Temp;
	ch16 *pStr = Temp.f_GetStr(NMib::NStr::NPlatform::gc_MaxWindowsEnvVarLength);
	pStr[0] = 0;
	if (!GetEnvironmentVariableW(NMib::NStr::NPlatform::fg_StrToWindows<NMib::NStr::CWStrNonTracked>(_VariableName), pStr, NMib::NStr::NPlatform::gc_MaxWindowsEnvVarLength))
		return false;

	Temp.f_TrimSize();
	_Value = fg_Move(Temp);
	return true;
}

NMib::NStr::CFStr256 NSys::fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CFStr256 const &_VariableName)
{
	NMib::NStr::CFWStr256 Temp;
	ch16 *pStr = Temp.f_GetStr(256);
	pStr[0] = 0;
	GetEnvironmentVariableW(NMib::NStr::NPlatform::fg_StrToWindows<NMib::NStr::CFWStr256>(_VariableName), pStr, 256);
	return Temp;
}

void NMib::NSys::fg_Process_SetEnvironmentVariable_Unsafe(NMib::NStr::CStrNonTracked const &_VariableName, NMib::NStr::CStrNonTracked const &_Value)
{
	if (!SetEnvironmentVariableW(NMib::NStr::NPlatform::fg_StrToWindows<NMib::NStr::CWStrNonTracked>(_VariableName), NMib::NStr::NPlatform::fg_StrToWindows<NMib::NStr::CWStrNonTracked>(_Value)))
		DMibError((CStr::CFormat("Windows returned an error from SetEnvironmentVariableW: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
}


#if _MSC_VER >= 1900
typedef enum _crt_argv_mode
{
	_crt_argv_no_arguments,
	_crt_argv_unexpanded_arguments,
	_crt_argv_expanded_arguments,
} _crt_argv_mode;
extern "C" _crt_argv_mode __CRTDECL _get_startup_argv_mode();
extern "C" errno_t __cdecl _configure_wide_argv(_crt_argv_mode const mode);
extern int       __argc;
extern "C" char**    __argv;
extern wchar_t** __wargv;
#else
extern "C" _CRTIMP int __argc;
extern "C" _CRTIMP wchar_t **__wargv;
extern "C" wchar_t *_wcmdln;
extern "C" int __cdecl _wsetargv();
#endif

void NSys::fg_Process_GetCommandLineArgs(NContainer::TCVector<NMib::NStr::CStr> &_List)
{
#if _MSC_VER < 1900
	if (_wcmdln == nullptr)
		_wcmdln = GetCommandLineW();
	if (__wargv == nullptr)
		_wsetargv();

	int NumArgs = __argc;
	_List.f_SetLen(NumArgs);

	for (int i = 0; i < NumArgs; ++i)
		_List[i] = CWStr(__wargv[i]);
#else
	if (!__wargv)
		_configure_wide_argv(_get_startup_argv_mode());

	int NumArgs = __argc;
	_List.f_SetLen(NumArgs);

	for (int i = 0; i < NumArgs; ++i)
		_List[i] = CWStr(__wargv[i]);
#endif
}

NMib::NStr::CStr NSys::fg_System_GetCPUName()
{
	NMib::NPlatform::CWin32_Registry Registry(NMib::NPlatform::CWin32_Registry::ERegRoot_LocalMachine);
	if (Registry.f_ValueExists("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "ProcessorNameString"))
	{
		return Registry.f_Read_Str("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "ProcessorNameString");
	}

	return "Unknown";
}


mint NSys::fg_Thread_GetPhysicalCores()
{
	HMODULE pKernel32 = NLocal::g_hKernel32;
	if (pKernel32)
	{
		if (NLocal::g_OptionalFunctions.m_fGetLogicalProcessorInformation)
		{
			DWORD BufferLength = 0;
			CByteVector Buffer;
			bool bDone = false;
			bool bError = false;
			while (!bDone) 
			{
				BufferLength = Buffer.f_GetLen();
				bool bRet = NLocal::g_OptionalFunctions.m_fGetLogicalProcessorInformation((PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)Buffer.f_GetArray(), &BufferLength);

				if (!bRet) 
				{
					if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) 
						Buffer.f_SetLen(BufferLength);
					else
					{
						bError = true;
						bDone = true;
					}
				} 
				else 
					bDone = true;
			}

			if (!bError)
			{
				mint nPhysicalCores = 0;
				mint ByteOffset = 0;

				PSYSTEM_LOGICAL_PROCESSOR_INFORMATION pLPI = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)Buffer.f_GetArray();
				
				while (ByteOffset < BufferLength) 
				{
					switch (pLPI->Relationship) 
					{
						case RelationProcessorCore:
							nPhysicalCores += 1;
							break;

						default:
							break;
					}
					ByteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
					pLPI++;
				}
				return nPhysicalCores;
			}
		}
	}

	return gs_SysInfo.dwNumberOfProcessors;
}

mint NSys::fg_Thread_GetVirtualCores()
{
	return gs_SysInfo.dwNumberOfProcessors;
}

void NSys::fg_Thread_SmallestSleep()
{
//	timeBeginPeriod(1);
	Sleep(1);
//	timeEndPeriod(1);
//	return 0.015;
}

void NSys::fg_Thread_Yield()
{
//	yield_cpu;
//	timeBeginPeriod(1);
	if (!SwitchToThread())
		SleepEx(1, false);
//	timeEndPeriod(1);
//	return 0.015;
}

namespace NMib::NThread
{
	static_assert(sizeof(uint32) == sizeof(CLowLevelLockAggregate::m_Lock));

	void CLowLevelLockAggregate::f_ForkedChildUnlocked()
	{
		m_Lock = 0;
#if DMibEnableSafeCheck > 0
		++m_nForked;
		m_ThreadID = 0;
		m_AlternateThreadID = 0;
#endif
	}

	void CLowLevelLockAggregate::f_ForkedChildLocked()
	{
		m_Lock = NSys::fg_Thread_GetCurrentUID();

#if DMibEnableSafeCheck > 0
		++m_nForked;
		m_ThreadID = NSys::fg_Thread_GetCurrentUID();
		m_AlternateThreadID = NSys::fg_Thread_GetCurrentUIDAlternate();
#endif
	}

	void CLowLevelLockAggregate::f_Construct()
	{
		DMibSanitizerAnnotate_MutexCreate(this, __tsan_mutex_not_static);
		m_Lock = 0;
	}

	void CLowLevelLockAggregate::f_Destruct()
	{
		DMibSanitizerAnnotate_MutexDestroy(this, 0);
	}

	bool CLowLevelLockAggregate::f_TryLock()
	{
		DMibSanitizerAnnotate_MutexPreLock(this, __tsan_mutex_write_reentrant | __tsan_mutex_try_lock);

		if (NLocal::g_OptionalFunctions.m_fWaitOnAddress)
		{
			uint32 CurrentThreadID = NSys::fg_Thread_GetCurrentUID();
			uint32 PrevioustThreadID = 0;
			if (!m_Lock.f_CompareExchangeStrong(PrevioustThreadID, CurrentThreadID, EMemoryOrder_Acquire, EMemoryOrder_Acquire))
			{
				DMibSanitizerAnnotate_MutexPostLock(this, __tsan_mutex_write_reentrant | __tsan_mutex_try_lock | __tsan_mutex_try_lock_failed, 1);
				return false;
			}
		}
		else
		{
			uint32 Expected = 0;
			if (!m_Lock.f_CompareExchangeStrong(Expected, 1, NAtomic::EMemoryOrder_Acquire, NAtomic::EMemoryOrder_Acquire))
			{
				DMibSanitizerAnnotate_MutexPostLock(this, __tsan_mutex_write_reentrant | __tsan_mutex_try_lock | __tsan_mutex_try_lock_failed, 1);
				return false;
			}
		}

#if DMibEnableSafeCheck > 0
		m_ThreadID = NSys::fg_Thread_GetCurrentUID();
		m_AlternateThreadID = NSys::fg_Thread_GetCurrentUIDAlternate();
#endif
		DMibSanitizerAnnotate_MutexPostLock(this, __tsan_mutex_write_reentrant | __tsan_mutex_try_lock, 1);
		return true;
	}

	void CLowLevelLockAggregate::f_Lock()
	{
		DMibSanitizerAnnotate_MutexPreLock(this, 0);
		if (NLocal::g_OptionalFunctions.m_fWaitOnAddress)
		{
			uint32 CurrentThreadID = NSys::fg_Thread_GetCurrentUID();
			while (true) 
			{
				uint32 PrevioustThreadID = 0;
				if (m_Lock.f_CompareExchangeWeak(PrevioustThreadID, CurrentThreadID, EMemoryOrder_Acquire, EMemoryOrder_Acquire))
					break;

				NLocal::g_OptionalFunctions.m_fWaitOnAddress(&m_Lock, &PrevioustThreadID, sizeof(PrevioustThreadID), INFINITE);
			}
		}
		else
		{
			NMib::NThread::CThreadSpinWaiter SpinWaiter;
			uint32 Expected = 0;
			while (!m_Lock.f_CompareExchangeWeak(Expected, 1, NAtomic::EMemoryOrder_Acquire, NAtomic::EMemoryOrder_Acquire))
			{
				SpinWaiter.f_Wait();
				Expected = 0;
			}
		}

#if DMibEnableSafeCheck > 0
		m_ThreadID = NSys::fg_Thread_GetCurrentUID();
		m_AlternateThreadID = NSys::fg_Thread_GetCurrentUIDAlternate();
#endif
		DMibSanitizerAnnotate_MutexPostLock(this, 0, 1);
	}

	void CLowLevelLockAggregate::f_Unlock()
	{
		DMibSanitizerAnnotate_MutexPreUnlock(this, 0);
#if DMibEnableSafeCheck > 0
		m_ThreadID = 0;
		m_AlternateThreadID = 0;
#endif
		if (NLocal::g_OptionalFunctions.m_fWaitOnAddress)
		{
			m_Lock.f_Exchange(0, NAtomic::EMemoryOrder_Release);;
			NLocal::g_OptionalFunctions.m_fWakeByAddressSingle(&m_Lock);
		}
		else
			m_Lock.f_Exchange(0, NAtomic::EMemoryOrder_Release);

		DMibSanitizerAnnotate_MutexPostUnlock(this, 0);
	}

	bool CLowLevelLockAggregate::f_TryLockNoSanitize()
	{
		if (NLocal::g_OptionalFunctions.m_fWaitOnAddress)
		{
			uint32 CurrentThreadID = NSys::fg_Thread_GetCurrentUID();
			uint32 PrevioustThreadID = 0;
			if (!m_Lock.f_CompareExchangeStrong(PrevioustThreadID, CurrentThreadID, EMemoryOrder_Acquire, EMemoryOrder_Acquire))
				return false;
		}
		else
		{
			uint32 Expected = 0;
			if (!m_Lock.f_CompareExchangeStrong(Expected, 1, NAtomic::EMemoryOrder_Acquire, NAtomic::EMemoryOrder_Acquire))
				return false;
		}
#ifdef DMibSanitizerEnabled_Thread
		__tsan_acquire(&m_Lock);
#endif
#if DMibEnableSafeCheck > 0
		m_ThreadID = NSys::fg_Thread_GetCurrentUID();
		m_AlternateThreadID = NSys::fg_Thread_GetCurrentUIDAlternate();
#endif
		return true;
	}

	void CLowLevelLockAggregate::f_LockNoSanitize()
	{
		if (NLocal::g_OptionalFunctions.m_fWaitOnAddress)
		{
			uint32 CurrentThreadID = NSys::fg_Thread_GetCurrentUID();
			while (true)
			{
				uint32 PrevioustThreadID = 0;
				if (m_Lock.f_CompareExchangeWeak(PrevioustThreadID, CurrentThreadID, EMemoryOrder_Acquire, EMemoryOrder_Acquire))
					break;

				NLocal::g_OptionalFunctions.m_fWaitOnAddress(&m_Lock, &PrevioustThreadID, sizeof(PrevioustThreadID), INFINITE);
			}
		}
		else
		{
			NMib::NThread::CThreadSpinWaiter SpinWaiter;
			uint32 Expected = 0;
			while (!m_Lock.f_CompareExchangeWeak(Expected, 1, NAtomic::EMemoryOrder_Acquire, NAtomic::EMemoryOrder_Acquire))
			{
				SpinWaiter.f_Wait();
				Expected = 0;
			}
		}
#ifdef DMibSanitizerEnabled_Thread
		__tsan_acquire(&m_Lock);
#endif
#if DMibEnableSafeCheck > 0
		m_ThreadID = NSys::fg_Thread_GetCurrentUID();
		m_AlternateThreadID = NSys::fg_Thread_GetCurrentUIDAlternate();
#endif
	}

	void CLowLevelLockAggregate::f_UnlockNoSanitize()
	{
#if DMibEnableSafeCheck > 0
		m_ThreadID = 0;
		m_AlternateThreadID = 0;
#endif
#ifdef DMibSanitizerEnabled_Thread
		__tsan_release(&m_Lock);
#endif
		if (NLocal::g_OptionalFunctions.m_fWaitOnAddress)
		{
			m_Lock.f_Exchange(0, NAtomic::EMemoryOrder_Release);;
			NLocal::g_OptionalFunctions.m_fWakeByAddressSingle(&m_Lock);
		}
		else
			m_Lock.f_Exchange(0, NAtomic::EMemoryOrder_Release);
	}
}

void *NSys::fg_Thread_GetCurrent()
{
	return (void *)(mint)GetCurrentThread();	
}

#if 0
mint NSys::fg_Thread_GetCurrentUID()
{	
	return (mint)GetCurrentThreadId();	
}
#endif

void *NSys::fg_Semaphore_Alloc(mint _InitialCount, mint _MaximumCount)
{
	return CreateSemaphore(nullptr, _InitialCount, _MaximumCount, nullptr);
}

void NSys::fg_Semaphore_ForkedChild(void * _pSemaphore)
{
}

void *NSys::fg_Semaphore_Duplicate(void *_pSemaphore)
{
	HANDLE pTarget = nullptr;
	DuplicateHandle(GetCurrentProcess(), _pSemaphore, GetCurrentProcess(), &pTarget, 0, FALSE, DUPLICATE_SAME_ACCESS);
	return pTarget;
}


void NSys::fg_Semaphore_Free(void *_pSemaphore)
{
	CloseHandle(_pSemaphore);
}
		

void NSys::fg_Semaphore_Increase(void * _pSemaphore, mint _Count)
{
	ReleaseSemaphore(_pSemaphore, _Count, nullptr);
}

void NSys::fg_Semaphore_Wait(void * _pSemaphore)
{
	WaitForSingleObject(_pSemaphore, INFINITE);
}

bool NSys::fg_Semaphore_WaitTimeout(void * _pSemaphore, fp64 _Timeout)
{
	if (_Timeout < 0)
		return WaitForSingleObjectEx(_pSemaphore, ((-_Timeout) * 1000.0 / NTime::CSystem_Time::fs_GetTimeSpeed()).f_Ceil().f_ToInt(), true) != WAIT_OBJECT_0;
	else
		return WaitForSingleObjectEx(_pSemaphore, (_Timeout * 1000.0 / NTime::CSystem_Time::fs_GetTimeSpeed()).f_Ceil().f_ToInt(), false) != WAIT_OBJECT_0;
}

bool NSys::fg_Semaphore_TryWait(void * _pSemaphore)
{
	return WaitForSingleObject(_pSemaphore, 0) == WAIT_OBJECT_0;
}


void *NSys::fg_Event_Alloc(bool _InitialSignal)
{
	return CreateEventA(nullptr, true, _InitialSignal, nullptr);
}

void NSys::fg_Event_PrepareFork(void *_pEvent)
{
}

void NSys::fg_Event_ForkedChild(void *_pEvent)
{
}

void NSys::fg_Event_ForkedParent(void *_pEvent)
{
}

void NSys::fg_Event_Free(void *_pEvent)
{
	CloseHandle(_pEvent);
}

void NSys::fg_Event_SetSignaled(void * _pEvent)
{
	SetEvent(_pEvent);
}

void NSys::fg_Event_ResetSignaled(void * _pEvent)
{
	ResetEvent(_pEvent);
}

void NSys::fg_Event_Wait(void * _pEvent)
{
	WaitForSingleObject(_pEvent, INFINITE);
}

bool NSys::fg_Event_WaitTimeout(void * _pEvent, fp64 _Timeout)
{
	return WaitForSingleObject(_pEvent, (_Timeout * 1000.0 / NTime::CSystem_Time::fs_GetTimeSpeed()).f_Ceil().f_ToInt()) != WAIT_OBJECT_0;
}

bool NSys::fg_Event_TryWait(void * _pEvent)
{
	return WaitForSingleObject(_pEvent, 0) == WAIT_OBJECT_0;
}

void NSys::fg_Message(const ch8 *_pMessageType, const ch8 *_pToOutput)
{
	HWND hWndParent = nullptr;
	BOOL fNonInteractive = FALSE;

	/*
	* If the current process isn't attached to a visible WindowStation,
	* (e.g. a non-interactive service), then we need to set the
	* MB_SERVICE_NOTIFICATION flag, else the message box will be
	* invisible, hanging the program.
	*
	* This check only applies to Windows NT-based systems (for which we
	* retrieved the address of GetProcessWindowStation above).
	*/

	DWORD uType = MB_OK;
	HWINSTA hwinsta;
	USEROBJECTFLAGS uof;
	DWORD nDummy;

	bool bCopy = !NMib::NStr::fg_StrCmpNoCase(_pMessageType, "Copy");

	CStrVMem Message = _pToOutput;
	CStrVMem Message2;

	if (bCopy)
	{
		Message2 = Message.f_Replace("\n", "\r\n");
		Message = "Test\n\n" + Message + "\n\nIf you want to put the message in clipboard press OK.";
//		CStr Message2 = "Test";
//		Message = Message2;
//		CFStr256 Message3 = "Test";
//		Message3 = "hula" + Message3;

		uType = MB_OKCANCEL;
	}

	if (!NMib::NStr::fg_StrCmpNoCase(_pMessageType, "Fatal Error"))
		uType |= MB_ICONERROR;


	if ((nullptr == 
		(hwinsta = GetProcessWindowStation()))
		|| !GetUserObjectInformationA(hwinsta, UOI_FLAGS, &uof, sizeof(uof), &nDummy) || (uof.dwFlags & WSF_VISIBLE) == 0)
	{
		fNonInteractive = TRUE;
	}

	if (fNonInteractive)
	{
		if ( NLocal::g_VersionInfo.dwMajorVersion >= 4)
			uType |= MB_SERVICE_NOTIFICATION;
		else
			uType |= MB_SERVICE_NOTIFICATION_NT3X;
	}
	else
	{
		hWndParent = GetActiveWindow();

		if (hWndParent != nullptr)
			hWndParent = GetLastActivePopup(hWndParent);
	}

	DWORD Answer = MessageBoxA(hWndParent, Message, _pMessageType, uType);

	if (bCopy)
	{
		if (Answer == IDOK)
		{
			if (OpenClipboard(nullptr))
			{

				EmptyClipboard(); 
				HGLOBAL GlobalMem = GlobalAlloc(GMEM_MOVEABLE, Message2.f_GetSize()); 
				if (GlobalMem)
				{
					uint8 *pMem = (uint8 *)GlobalLock(GlobalMem);
					fg_MemCopy(pMem, Message2.f_GetStr(), Message2.f_GetSize());
					GlobalUnlock(GlobalMem); 
					// Place the handle on the clipboard. 
				
					SetClipboardData(CF_TEXT, GlobalMem); 
				}

				CloseClipboard();
			}
		}
	}
}

void NSys::fg_Message(const ch16 *_pMessageType, const ch16 *_pToOutput)
{
	HWND hWndParent = nullptr;
	BOOL fNonInteractive = FALSE;

	/*
	* If the current process isn't attached to a visible WindowStation,
	* (e.g. a non-interactive service), then we need to set the
	* MB_SERVICE_NOTIFICATION flag, else the message box will be
	* invisible, hanging the program.
	*
	* This check only applies to Windows NT-based systems (for which we
	* retrieved the address of GetProcessWindowStation above).
	*/

	HWINSTA hwinsta;
	USEROBJECTFLAGS uof;
	DWORD nDummy;
	DWORD uType = MB_OK;
	
	if (!NMib::NStr::fg_StrCmpNoCase(_pMessageType, "Fatal Error"))
		uType |= MB_ICONERROR;

	if ((nullptr == 
		(hwinsta = GetProcessWindowStation()))
		|| !GetUserObjectInformationA(hwinsta, UOI_FLAGS, &uof, sizeof(uof), &nDummy) || (uof.dwFlags & WSF_VISIBLE) == 0)
	{
		fNonInteractive = TRUE;
	}

	if (fNonInteractive)
	{
		if (NLocal::g_VersionInfo.dwMajorVersion >= 4)
			uType |= MB_SERVICE_NOTIFICATION;
		else
			uType |= MB_SERVICE_NOTIFICATION_NT3X;
	}
	else
	{
		hWndParent = GetActiveWindow();

		if (hWndParent != nullptr)
			hWndParent = GetLastActivePopup(hWndParent);
	}

	MessageBoxW(hWndParent, (LPCWSTR)_pToOutput, (LPCWSTR)_pMessageType, uType);
}

EFileSystemFeature NSys::NFile::fg_GetFileSystemFeatures()
{
	return EFileSystemFeature_HasDrives;
}

namespace
{
	template <typename tf_CWinStr, typename tf_CStr, bool tf_bThrow>
	EFileAttrib fg_GetAttributesInternalWithAttribs(ch16 const *_pFileName, uint32 _FileAttribs);
}

bool NSys::NFile::fg_FileExists(const CStr &_FileName, EFileAttrib _AttribMask)
{
	if (_FileName.f_IsEmpty())
		return false;
	CStr Drive = NMib::NFile::CFile::fs_GetDrive(_FileName);
	CWStr Temp = NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_FileName);
	CWStr DriveW = NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(Drive);
	if (Temp.f_CmpNoCase(DriveW) == 0)
	{
		UINT Ret = GetDriveType(DriveW + "\\");
		if ((_AttribMask & NMib::NFile::EFileAttrib_Directory) && Ret != DRIVE_NO_ROOT_DIR)
			return true;
		return false;
	}
	else
	{
		uint32 Attribs = GetFileAttributesW(Temp);

		if (Attribs == INVALID_FILE_ATTRIBUTES)
			return false;

		auto MalterlibAttribs = fg_GetAttributesInternalWithAttribs<CWStr, CStr, false>(Temp, Attribs);

		return (_AttribMask & MalterlibAttribs) != EFileAttrib_None;
	}
}

bool NSys::NFile::fg_FileExists(const CStrNonTracked &_FileName, EFileAttrib _AttribMask)
{
	if (_FileName.f_IsEmpty())
		return false;
	CStrNonTracked Drive = NMib::NFile::CFile::fs_GetDrive(_FileName);
	CWStrNonTracked Temp = NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_FileName);
	CWStrNonTracked DriveW = NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(Drive);
	if (Temp.f_CmpNoCase(DriveW) == 0)
	{
		UINT Ret = GetDriveType(DriveW + "\\");
		if ((_AttribMask & NMib::NFile::EFileAttrib_Directory) && Ret != DRIVE_NO_ROOT_DIR)
			return true;
		return false;
	}
	else
	{
		uint32 Attribs = GetFileAttributesW(Temp);

		if (Attribs == INVALID_FILE_ATTRIBUTES)
			return false;
		
		auto MalterlibAttribs = fg_GetAttributesInternalWithAttribs<CWStrNonTracked, CStrNonTracked, false>(Temp, Attribs);

		return (_AttribMask & MalterlibAttribs) != EFileAttrib_None;
	}
}


// From: http://blog.aaronballman.com/2011/08/how-to-check-access-rights/
ECheckFileRights NSys::NFile::fg_CheckFileRights( const CStr & _File, NMib::NFile::EFileRight _Rights)
{
	DWORD GenericRights
		= ((_Rights & EFileRight_Read) ? GENERIC_READ : 0)
		| ((_Rights & EFileRight_Write) ? GENERIC_WRITE : 0)
		| ((_Rights & EFileRight_Execute) ? GENERIC_EXECUTE : 0)
	;

	CWStr Path;
	Path = NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal<CWStr, CWStr>(_File);

	bool bRet = false;
	DWORD length = 0;
	if (	!::GetFileSecurity( Path, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, NULL, NULL, &length )
		&&	ERROR_INSUFFICIENT_BUFFER == ::GetLastError()
	) 
	{
		PSECURITY_DESCRIPTOR security = static_cast< PSECURITY_DESCRIPTOR >( ::malloc( length ) );
		if (	security
			&& ::GetFileSecurity( Path, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION
			| DACL_SECURITY_INFORMATION, security, length, &length ))
		{
			HANDLE hToken = NULL;
			if (::OpenProcessToken( ::GetCurrentProcess(), TOKEN_IMPERSONATE | TOKEN_QUERY | TOKEN_DUPLICATE | STANDARD_RIGHTS_READ, &hToken ))
			{
				HANDLE hImpersonatedToken = NULL;
				if (::DuplicateToken( hToken, SecurityImpersonation, &hImpersonatedToken ))
				{
					GENERIC_MAPPING mapping = { 0xFFFFFFFF };
					PRIVILEGE_SET privileges = { 0 };
					DWORD grantedAccess = 0, privilegesLength = sizeof( privileges );
					BOOL result = FALSE;

					mapping.GenericRead = FILE_GENERIC_READ;
					mapping.GenericWrite = FILE_GENERIC_WRITE;
					mapping.GenericExecute = FILE_GENERIC_EXECUTE;
					mapping.GenericAll = FILE_ALL_ACCESS;

					::MapGenericMask( &GenericRights, &mapping );
					if (::AccessCheck( security, hImpersonatedToken, GenericRights, &mapping, &privileges, &privilegesLength, &grantedAccess, &result ))
					{
						bRet = (result == TRUE);
					}

					::CloseHandle( hImpersonatedToken );
				}
				::CloseHandle( hToken );
			}
		::free( security );
		}
	}
	else 
	{
		DWORD LastError = ::GetLastError();
		if (	LastError == ERROR_FILE_NOT_FOUND
			||	LastError == ERROR_PATH_NOT_FOUND)
		{
			return ECheckFileRights_DoesNotExist;
		}
	}
	return bRet ? ECheckFileRights_Access : ECheckFileRights_NoAccess;
}

namespace
{
	template <typename tf_CWinStr, typename tf_CStr, bool tf_bOnLink = false>
	void fg_SetAttributesInternal(ch16 const *_pFileName, EFileAttrib _Attributes);
}

template <typename tf_CWinStr, typename tf_CErrorStr, typename tf_CStr>
void *fg_OpenHelper(const tf_CStr &_FileName, NMib::NFile::EFileOpen _OpenFlags, NMib::NFile::EFileAttrib _Attributes)
{
	if ((_OpenFlags & (NMib::NFile::EFileOpen_Read | NMib::NFile::EFileOpen_Write | NMib::NFile::EFileOpen_ReadAttribs | NMib::NFile::EFileOpen_WriteAttribs)) == 0)
		DMibErrorFile(tf_CErrorStr("Open flags contain neither read, read attribs, wriwe or write attribs flags, one of them must be specified"));

	if ((_OpenFlags & (NMib::NFile::EFileOpen_DontCreate | NMib::NFile::EFileOpen_DontOpenExisting)) == (NMib::NFile::EFileOpen_DontCreate | NMib::NFile::EFileOpen_DontOpenExisting))
		DMibErrorFile(tf_CErrorStr("Conflicting open flags (both don't open existing and don't create)"));

	if ((_OpenFlags & (NMib::NFile::EFileOpen_DontOpenExisting | NMib::NFile::EFileOpen_Read | NMib::NFile::EFileOpen_Write)) == (NMib::NFile::EFileOpen_DontOpenExisting | NMib::NFile::EFileOpen_Read))
		DMibErrorFile(tf_CErrorStr("You are trying to open a file that does not exist for read access only, this makes no sence)"));

	uint32 CreateDisposition = 0;

	if (_OpenFlags & NMib::NFile::EFileOpen_Directory)
		_OpenFlags |= NMib::NFile::EFileOpen_DontCreate | NMib::NFile::EFileOpen_DontTruncate;

	if (_OpenFlags & NMib::NFile::EFileOpen_DontOpenExisting)
	{
		CreateDisposition = CREATE_NEW;
	}		
	else if (_OpenFlags & NMib::NFile::EFileOpen_DontCreate)
	{
		if (_OpenFlags & NMib::NFile::EFileOpen_DontTruncate)
			CreateDisposition = OPEN_EXISTING;
		else
			CreateDisposition = TRUNCATE_EXISTING;
	}
	else
	{
		if (_OpenFlags & NMib::NFile::EFileOpen_Write)
		{
			if (_OpenFlags & NMib::NFile::EFileOpen_DontTruncate)
				CreateDisposition = OPEN_ALWAYS;
			else
				CreateDisposition = CREATE_ALWAYS;
		}
		else
			CreateDisposition = OPEN_EXISTING;
	}

	uint32 OpenFlags = 0;
	if (_OpenFlags & NMib::NFile::EFileOpen_Write)
		OpenFlags |= FILE_GENERIC_WRITE;
	if (_OpenFlags & NMib::NFile::EFileOpen_Read)
		OpenFlags |= FILE_GENERIC_READ;
	if (_OpenFlags & NMib::NFile::EFileOpen_ReadAttribs)
		OpenFlags |= FILE_READ_ATTRIBUTES | FILE_READ_EA;
	if (_OpenFlags & NMib::NFile::EFileOpen_WriteAttribs)
		OpenFlags |= FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA;


	uint32 ShareFlags = 0;

	if (_OpenFlags & NMib::NFile::EFileOpen_ShareRead)
		ShareFlags |= FILE_SHARE_READ;
	if (_OpenFlags & NMib::NFile::EFileOpen_ShareWrite)
		ShareFlags |= FILE_SHARE_WRITE;
	if (_OpenFlags & NMib::NFile::EFileOpen_ShareDelete)
		ShareFlags |= FILE_SHARE_DELETE;	

	uint32 FlagsAndAttribs = 0;
	if (_OpenFlags & NMib::NFile::EFileOpen_Temporary)
		FlagsAndAttribs |= FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE;
	if (_OpenFlags & NMib::NFile::EFileOpen_WriteThrough)
		FlagsAndAttribs |= FILE_FLAG_WRITE_THROUGH;
	if (_OpenFlags & NMib::NFile::EFileOpen_NoCache)
		FlagsAndAttribs |= FILE_FLAG_NO_BUFFERING;
	if (_OpenFlags & NMib::NFile::EFileOpen_Directory)
		FlagsAndAttribs |= FILE_FLAG_BACKUP_SEMANTICS;
	if (_OpenFlags & NMib::NFile::EFileOpen_Link)
		FlagsAndAttribs |= FILE_FLAG_OPEN_REPARSE_POINT;

	tf_CWinStr FileName;
	if (_OpenFlags & EFileOpen_RawFileName)
		FileName = NStr::NPlatform::fg_StrToWindows<tf_CWinStr>(_FileName);
	else
		FileName = NFile::NPlatform::fg_ConvertToWindowsPathLocal<tf_CWinStr, tf_CWinStr>(_FileName);	

	void *pFile = CreateFileW(FileName, OpenFlags, ShareFlags, nullptr, CreateDisposition, FlagsAndAttribs, nullptr);

	if (pFile == INVALID_HANDLE_VALUE)
	{
		DMibErrorFile
			(
				(
					typename tf_CErrorStr::CFormat("Windows returned an error from CreateFile({}, 0x{nfh,sf0,sj8}, 0x{nfh,sf0,sj8}, {}, 0x{nfh,sf0,sj8}): {}") 
					<< FileName 
					<< OpenFlags 
					<< ShareFlags
					<< CreateDisposition
					<< FlagsAndAttribs
					<< NMib::NPlatform::fg_Win32_GetLastErrorStr()
				).f_GetStr()
			)
		;
	}

	TCUniquePointer<TCWin32File<tf_CWinStr, typename tf_CWinStr::CAllocator>, typename tf_CWinStr::CAllocator> pNewFile = fg_Construct(FileName);
	pNewFile->m_pFile = pFile;
	pNewFile->m_Flags = _OpenFlags;
	
	if (_Attributes != EFileAttrib_None)
		fg_SetAttributesInternal<tf_CWinStr, tf_CStr>(pNewFile->f_GetName(), _Attributes);
	
	return pNewFile.f_Detach();
}

void *NSys::NFile::fg_Open(const CStr &_FileName, NMib::NFile::EFileOpen _OpenFlags, NMib::NFile::EFileAttrib _Attributes)
{
	return fg_OpenHelper<CWStr, CStr>(_FileName, _OpenFlags, _Attributes);
}

void *NSys::NFile::fg_Open(const CStrNonTracked &_FileName, NMib::NFile::EFileOpen _OpenFlags, NMib::NFile::EFileAttrib _Attributes)
{
	return fg_OpenHelper<CWStrNonTracked, CStrNonTracked>(_FileName, _OpenFlags, _Attributes);
}

void NSys::NFile::fg_Close(void *_pFile)
{
	if (!CloseHandle(((CWin32File *)_pFile)->m_pFile))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from CloseHandle: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}
	((CWin32File *)_pFile)->f_Delete();
}

CMibFilePos NMib::NSys::NFile::fg_GetSize(const NMib::NStr::CStr &_FileName)
{
	WIN32_FIND_DATAW FindData;
	void *pFindHandle;
	pFindHandle = FindFirstFileW(NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_FileName), &FindData);

	if (pFindHandle != INVALID_HANDLE_VALUE)
	{
		FindClose(pFindHandle);
		return (CMibFilePos(FindData.nFileSizeHigh) << 32) | CMibFilePos(FindData.nFileSizeLow);
	}
	DMibErrorFile((CStr::CFormat("Windows returned an error from FindFirstFile({}): {}") << _FileName << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
}

void *NSys::NFile::fg_GetOSFile(void *_pFile)
{
	return ((CWin32File *)_pFile)->m_pFile;
}

mint NSys::NFile::fg_Read(void *_pFile, void *_pData, const CMibFilePos &_Offset, mint _NumBytes)
{
	uint32 BytesRead;

	CMibFilePos NewOffset;
	
	if (!(((CWin32File *)_pFile)->m_Flags & NMib::NFile::EFileOpen_NoFileLength))
	{
		if (!SetFilePointerEx(((CWin32File *)_pFile)->m_pFile, *((LARGE_INTEGER *)&_Offset), (LARGE_INTEGER *)&NewOffset, FILE_BEGIN))
		{
			DMibErrorFile((CStr::CFormat("Windows returned an error from SetFilePointerEx({}): {}") << ((CWin32File *)_pFile)->f_GetName() << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
		}

		if (NewOffset != _Offset)
		{
			DMibErrorFile("Failed to move file pointer to read location");
		}
	}

	if (!ReadFile(((CWin32File *)_pFile)->m_pFile, _pData, _NumBytes, &BytesRead, nullptr))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from ReadFile({}): {}") << ((CWin32File *)_pFile)->f_GetName() << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}

	return BytesRead;
}

mint NSys::NFile::fg_Write(void *_pFile, const void *_pData, const CMibFilePos &_Offset, mint _NumBytes)
{
	uint32 BytesWritten;
	CMibFilePos NewOffset;
	
	if (!(((CWin32File *)_pFile)->m_Flags & NMib::NFile::EFileOpen_NoFileLength))
	{
		if (!SetFilePointerEx(((CWin32File *)_pFile)->m_pFile, *((LARGE_INTEGER *)&_Offset), (LARGE_INTEGER *)&NewOffset, FILE_BEGIN))
		{
			DMibErrorFile((CStr::CFormat("Windows returned an error from SetFilePointerEx({}): {}") << ((CWin32File *)_pFile)->f_GetName() << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
		}

		if (NewOffset != _Offset)
		{
			DMibErrorFile("Failed to move file pointer to write location");
		}
	}

	if (!WriteFile(((CWin32File *)_pFile)->m_pFile, _pData, _NumBytes, &BytesWritten, nullptr))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from WriteFile({}): {}") << ((CWin32File *)_pFile)->f_GetName() << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}

	return BytesWritten;
}

void NSys::NFile::fg_Flush(void *_pFile)
{
	if (!FlushFileBuffers(((CWin32File *)_pFile)->m_pFile))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from FlushFileBuffers({}): {}") << ((CWin32File *)_pFile)->f_GetName() << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}

void NSys::NFile::fg_LockRange(void *_pFile, const CMibFilePos &_Offset, const CMibFilePos &_NumBytes, NMib::NFile::EFileLock _Flags)
{
	OVERLAPPED Overlapped;
	NMib::NMemory::fg_MemClear(Overlapped);
	Overlapped.Offset = _Offset & 0xffffffffll;
	Overlapped.OffsetHigh = (_Offset >> 32) & 0xffffffffll;
	uint32 Flags = 0;
	if (!(Flags & NMib::NFile::EFileLock_Block))
		Flags |= LOCKFILE_FAIL_IMMEDIATELY;
	if (Flags & NMib::NFile::EFileLock_PreventRead)
		Flags |= LOCKFILE_EXCLUSIVE_LOCK;
	if (!LockFileEx(((CWin32File *)_pFile)->m_pFile, Flags, 0, _NumBytes & 0xffffffffll, (_NumBytes >> 32) & 0xffffffffll, &Overlapped))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from LockFileEx({}): {}") << ((CWin32File *)_pFile)->f_GetName() << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}

void NSys::NFile::fg_UnlockRange(void *_pFile, const CMibFilePos &_Offset, const CMibFilePos &_NumBytes)
{
	OVERLAPPED Overlapped;
	NMib::NMemory::fg_MemClear(Overlapped);
	Overlapped.Offset = _Offset & 0xffffffffll;
	Overlapped.OffsetHigh = (_Offset >> 32) & 0xffffffffll;
	if (!UnlockFileEx(((CWin32File *)_pFile)->m_pFile, 0, _NumBytes & 0xffffffffll, (_NumBytes >> 32) & 0xffffffffll, &Overlapped))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from UnlockFileEx({}): {}") << ((CWin32File *)_pFile)->f_GetName() << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}


struct CMalterlibExtendedAttributes
{
	EFileAttrib m_ExtendedAttributes;
	CMalterlibExtendedAttributes()
		: m_ExtendedAttributes(NFile::EFileAttrib_None)
	{

	}
	enum
	{
		EVersion = 0x101
	};
	
	template <typename tf_CStream>
	void f_Feed(tf_CStream &_Stream) const
	{
		_Stream << EVersion;
		_Stream << m_ExtendedAttributes;
	}

	template <typename tf_CStream>
	void f_Consume(tf_CStream &_Stream)
	{
		uint32 Version;
		_Stream >> Version;
		if (Version > EVersion)
			return;
		_Stream >> m_ExtendedAttributes;
	}

	bool operator == (CMalterlibExtendedAttributes const &_Right) const
	{
		return m_ExtendedAttributes == _Right.m_ExtendedAttributes;
	}
};

namespace
{
	template <typename tf_CWinStr, typename tf_CStr, bool tf_bOnLink>
	void fg_SetAttributesInternal(ch16 const *_pFileName, EFileAttrib _Attributes)
	{
		EFileAttrib ExtraAttributes = _Attributes;

		ExtraAttributes &= ~NMib::NFile::EFileAttrib_File;

		if (!(_Attributes & EFileAttrib_EmulatedLink))
		{
			ExtraAttributes &= ~NMib::NFile::EFileAttrib_Directory;
			ExtraAttributes &= ~NMib::NFile::EFileAttrib_Link;
		}

		uint32 FileAttribs = 0;
		if (_Attributes & NMib::NFile::EFileAttrib_Hidden)
		{
			ExtraAttributes &= ~NMib::NFile::EFileAttrib_Hidden;
			FileAttribs |= FILE_ATTRIBUTE_HIDDEN;
		}
		if (_Attributes & NMib::NFile::EFileAttrib_ReadOnly)
		{
			ExtraAttributes &= ~NMib::NFile::EFileAttrib_ReadOnly;
			FileAttribs |= FILE_ATTRIBUTE_READONLY;
		}
		if (_Attributes & NMib::NFile::EFileAttrib_System)
		{
			ExtraAttributes &= ~NMib::NFile::EFileAttrib_System;
			FileAttribs |= FILE_ATTRIBUTE_SYSTEM;
		}
		if (_Attributes & NMib::NFile::EFileAttrib_Archive)
		{
			ExtraAttributes &= ~NMib::NFile::EFileAttrib_Archive;
			FileAttribs |= FILE_ATTRIBUTE_ARCHIVE;
		}

		bool bReadOnly = (_Attributes & NMib::NFile::EFileAttrib_ReadOnly) != 0;

		if (ExtraAttributes && bReadOnly)
		{
			FileAttribs &= ~FILE_ATTRIBUTE_READONLY;
		}

		if (!FileAttribs)
			FileAttribs = FILE_ATTRIBUTE_NORMAL;


		if (!SetFileAttributesW(_pFileName, FileAttribs))
			DMibErrorFile((typename tf_CStr::CFormat("Windows returned an error from SetFileAttributesW({}): {}") << _pFileName << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());

		if constexpr (tf_bOnLink)
			return;

		if (CFile::CSetAttributeEmulationScope::fs_IsEmulationEnabled())
		{
			tf_CWinStr OriginalFileName(_pFileName);
			// This needs to be named exactly like this to be compatible with old version of library (when Malterlib was named Ids)
			auto ExtendedAttribName = OriginalFileName + ":IdsExtAttribs:$DATA";
			if (OriginalFileName.f_GetLen() < 260 && ExtendedAttribName.f_GetLen() >= 260)
				ExtendedAttribName = NFile::NPlatform::fg_ConvertToWindowsPathLocal(NFile::NPlatform::fg_ConvertFromWindowsPathInternal<tf_CWinStr>(ExtendedAttribName));

			if (ExtraAttributes)
			{
				{
					CMalterlibExtendedAttributes NewAttribs;
					NewAttribs.m_ExtendedAttributes = ExtraAttributes;
					TCBinaryStreamFile<> Stream;
					Stream.f_Open(tf_CStr(ExtendedAttribName), EFileOpen_Write | EFileOpen_ShareAll | EFileOpen_RawFileName);
					Stream << NewAttribs;
				}
				if (bReadOnly)
				{
					if (FileAttribs == FILE_ATTRIBUTE_NORMAL)
					{
						FileAttribs = 0;
					}
					FileAttribs |= FILE_ATTRIBUTE_READONLY;
					if (!SetFileAttributesW(_pFileName, FileAttribs))
						DMibErrorFile((typename tf_CStr::CFormat("Windows returned an error from SetFileAttributesW({}): {}") << _pFileName << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
				}
			}
			else
			{
				uint32 Attribs = GetFileAttributesW(ExtendedAttribName);
				if (Attribs != INVALID_FILE_ATTRIBUTES)
				{
					if (!DeleteFileW(NFile::NPlatform::fg_ConvertToWindowsPathLocal(ExtendedAttribName)))
						DMibErrorFile((typename tf_CStr::CFormat("Windows returned an error from DeleteFile({}): {}") << ExtendedAttribName << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
				}
			}
		}
	}

	template <typename tf_CWinStr, typename tf_CStr, bool tf_bThrow>
	EFileAttrib fg_GetAttributesInternalWithAttribs(ch16 const *_pFileName, uint32 _FileAttribs)
	{
		if (_FileAttribs == INVALID_FILE_ATTRIBUTES)
		{
			if constexpr (tf_bThrow)
				DMibErrorFile((typename tf_CStr::CFormat("Windows returned an error from GetFileAttributes({}): {}") << _pFileName << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
			else 
				return EFileAttrib_None;
		}

		uint32 MalterlibAttr = 0;
		if (_FileAttribs & FILE_ATTRIBUTE_DIRECTORY)
			MalterlibAttr |= NMib::NFile::EFileAttrib_Directory;
		else
			MalterlibAttr |= NMib::NFile::EFileAttrib_File;
		if (_FileAttribs & FILE_ATTRIBUTE_REPARSE_POINT)
			MalterlibAttr |= NMib::NFile::EFileAttrib_Link;
		if (_FileAttribs & FILE_ATTRIBUTE_HIDDEN)
			MalterlibAttr |= NMib::NFile::EFileAttrib_Hidden;
		if (_FileAttribs & FILE_ATTRIBUTE_READONLY)
			MalterlibAttr |= NMib::NFile::EFileAttrib_ReadOnly;
		if (_FileAttribs & FILE_ATTRIBUTE_SYSTEM)
			MalterlibAttr |= NMib::NFile::EFileAttrib_System;
		if (_FileAttribs & FILE_ATTRIBUTE_ARCHIVE)
			MalterlibAttr |= NMib::NFile::EFileAttrib_Archive;

		if (CFile::CSetAttributeEmulationScope::fs_IsEmulationEnabled())
		{
			tf_CWinStr OriginalFileName(_pFileName);
			auto ExtendedAttribName = OriginalFileName + ":IdsExtAttribs:$DATA";
			if (OriginalFileName.f_GetLen() < 260 && ExtendedAttribName.f_GetLen() >= 260)
				ExtendedAttribName = NFile::NPlatform::fg_ConvertToWindowsPathLocal(NFile::NPlatform::fg_ConvertFromWindowsPathInternal<tf_CWinStr>(ExtendedAttribName));

			uint32 Attribs = GetFileAttributesW(ExtendedAttribName);
			if (Attribs != INVALID_FILE_ATTRIBUTES)
			{
				try
				{
					TCBinaryStreamFile<> Stream;
					Stream.f_Open(tf_CStr(ExtendedAttribName), EFileOpen_Read | EFileOpen_ShareAll | EFileOpen_RawFileName);
					if (Stream.f_GetLength())
					{
						CMalterlibExtendedAttributes OldAttribs;
						Stream >> OldAttribs;

						MalterlibAttr |= OldAttribs.m_ExtendedAttributes;
					}
				}
				catch (CExceptionFile const &)
				{
				}
			}
		}

		return (EFileAttrib)MalterlibAttr;
	}

	template <typename tf_CWinStr, typename tf_CStr, bool tf_bThrow>
	EFileAttrib fg_GetAttributesInternal(ch16 const *_pFileName)
	{
		return fg_GetAttributesInternalWithAttribs<tf_CWinStr, tf_CStr, tf_bThrow>(_pFileName, GetFileAttributesW(_pFileName));
	}
}

NMib::NFile::EFileAttrib NSys::NFile::fg_GetSupportedAttributes()
{
	using namespace NMib::NFile;
	return EFileAttrib_Directory
		| EFileAttrib_Link
		| EFileAttrib_Hidden
		| EFileAttrib_ReadOnly
		| EFileAttrib_System
		| EFileAttrib_File
		| EFileAttrib_Archive
		| EFileAttrib_EmulatedLink
	;
}

NMib::NFile::EFileAttrib NSys::NFile::fg_GetValidAttributes()
{
	return NMib::NFile::EFileAttrib_None;
}

mint NSys::NFile::fg_MaximumPathLength()
{
	return NMib::NFile::NPlatform::gc_MaxWindowsPath;
}

void NSys::NFile::fg_SetAttributes(void *_pFile, EFileAttrib _Attributes)
{
	auto *pFile = ((CWin32File *)_pFile);
	if (pFile->f_IsNonTracked())
		fg_SetAttributesInternal<CWStrNonTracked, CStrNonTracked>(pFile->f_GetName(), _Attributes);
	else
		fg_SetAttributesInternal<CWStr, CStr>(pFile->f_GetName(), _Attributes);
}

void NSys::NFile::fg_SetAttributes(NMib::NStr::CStr const& _FileName, EFileAttrib _Attributes)
{
	fg_SetAttributesInternal<CWStr, CStr>(NMib::NFile::NPlatform::fg_ConvertToWindowsPath(_FileName, false), _Attributes);
}

void NSys::NFile::fg_SetAttributesOnLink(NMib::NStr::CStr const& _FileName, EFileAttrib _Attributes)
{
	fg_SetAttributesInternal<CWStr, CStr, true>(NMib::NFile::NPlatform::fg_ConvertToWindowsPath(_FileName, false), _Attributes);
}

EFileAttrib NSys::NFile::fg_GetAttributes(void *_pFile)
{
	auto *pFile = ((CWin32File *)_pFile);
	if (pFile->f_IsNonTracked())
		return fg_GetAttributesInternal<CWStrNonTracked, CStrNonTracked, true>(pFile->f_GetName());
	else
		return fg_GetAttributesInternal<CWStr, CStr, true>(pFile->f_GetName());
}


EFileAttrib NSys::NFile::fg_GetAttributes(NMib::NStr::CStr const& _FileName)
{
	return fg_GetAttributesInternal<CWStr, CStr, true>(NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_FileName));
}

EFileAttrib NSys::NFile::fg_GetAttributesOnLink(NMib::NStr::CStr const& _FileName)
{
	return fg_GetAttributes(_FileName);
}

NMib::NFile::CUniqueFileIdentifier NSys::NFile::fg_GetUniqueIdentifier(NMib::NStr::CStr const& _FileName)
{
	CFile File;
	auto FileOpenFlags = EFileOpen_ShareAll | EFileOpen_ReadAttribs;
	if (CFile::fs_FileExists(CStr(_FileName), EFileAttrib_Directory))
		FileOpenFlags |= EFileOpen_Directory;
	File.f_Open(_FileName, FileOpenFlags);

	if (NLocal::g_OptionalFunctions.m_fGetFileInformationByHandleEx)
	{
		Undocumented_FILE_ID_INFO FileIDInfo;
		if (NLocal::g_OptionalFunctions.m_fGetFileInformationByHandleEx(File.f_GetOSFile(), Undocumented_FileIdInfo, &FileIDInfo, sizeof(FileIDInfo)))
		{
			NMib::NFile::CUniqueFileIdentifier FileID;
			FileID.m_VolumeID = FileIDInfo.VolumeSerialNumber;
			FileID.m_FileID = 0;
			fg_MemCopy(&FileID.m_FileID, &FileIDInfo.FileId, fg_Min(sizeof(FileID.m_FileID), sizeof(FileIDInfo.FileId)));

			return FileID;
		}
	}

	BY_HANDLE_FILE_INFORMATION FileInfo;
	if (!GetFileInformationByHandle(File.f_GetOSFile(), &FileInfo))
		DMibErrorFile((CStr::CFormat("Windows returned an error from GetFileInformationByHandle({}): {}") << _FileName << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());

	NMib::NFile::CUniqueFileIdentifier FileID;
	FileID.m_VolumeID = FileInfo.dwVolumeSerialNumber;
	FileID.m_FileID = uint64(FileInfo.nFileIndexHigh) << 32; 
	FileID.m_FileID += FileInfo.nFileIndexLow;

	return FileID;
}

NMib::NFile::CUniqueFileIdentifier NSys::NFile::fg_GetUniqueIdentifierOnLink(NMib::NStr::CStr const &_FileName)
{
	return fg_GetUniqueIdentifier(_FileName);
}

CMibFilePos NSys::NFile::fg_GetSize(void *_pFile)
{
	CMibFilePos Ret;
	if (!GetFileSizeEx(((CWin32File *)_pFile)->m_pFile, (LARGE_INTEGER *)&Ret))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from GetFileSizeEx({}): {}") << ((CWin32File *)_pFile)->f_GetName() << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}
	return Ret;
}

void NSys::NFile::fg_SetSize(void *_pFile, const CMibFilePos &_Size)
{
	CMibFilePos NewOffset;
	
	if (!SetFilePointerEx(((CWin32File *)_pFile)->m_pFile, *((LARGE_INTEGER *)&_Size), (LARGE_INTEGER *)&NewOffset, FILE_BEGIN))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from SetFilePointerEx({}): {}") << ((CWin32File *)_pFile)->f_GetName() << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}

	if (NewOffset != _Size)
	{
		DMibErrorFile("Failed to move file pointer to file size location");
	}

	if (!SetEndOfFile(((CWin32File *)_pFile)->m_pFile))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from SetEndOfFile({}): {}") << ((CWin32File *)_pFile)->f_GetName() << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}

void NSys::NFile::fg_FileEnumOtherHandles(const NMib::NStr::CStr &_FileName, NContainer::TCVector<NMib::NFile::CFileHandle> &_HandleInfo)
{
	CStr CommonPath = NMib::NFile::CFile::fs_GetPath(_FileName);
	CStr TestFileName = CommonPath + "/" + CStr(NSys::fg_System_GenerateUUID());
	CStr FindFile = NMib::NFile::CFile::fs_GetFile(_FileName);
	void *pFile;
	try
	{
		pFile = NSys::NFile::fg_Open(TestFileName, NMib::NFile::EFileOpen_Write, NMib::NFile::EFileAttrib_None);
	}
	catch (NException::CException)
	{
		// Just fail
		return; 
	}

	
	void *pFileHandle = ((CWin32File *)pFile)->m_pFile;
	uint32 CurrentProcessID = GetCurrentProcessId();

	CHandleInformation Info;
	TCVector<CHandleInformation::CHandleInfo> Handles;
	Info.f_EnumHandles(Handles);

	CHandleInformation::CHandleInfo *pThisHandle = nullptr;
	mint nHandles = Handles.f_GetLen();
	CStr ToTrace;
	for (mint i = 0; i < nHandles; ++i)
	{
		CHandleInformation::CHandleInfo &Handle = Handles[i];
		DMibDTrace("{} - {} - {}\r\n", Handle.m_HandleName << Handle.m_ProcessID << Handle.m_HandleID);
		if (Handle.m_ProcessID == CurrentProcessID && Handle.m_HandleID == (uint32)(mint)pFileHandle)
		{
			pThisHandle = &Handle;
			break;
		}
	}
	if (pThisHandle)
	{
		CStr BasePath = NMib::NFile::CFile::fs_GetPath(pThisHandle->m_HandleName.f_ReplaceChar('\\', '/'));
		CStr FileToFind = BasePath + "/" + FindFile;
		mint nHandles = Handles.f_GetLen();
		for (mint i = 0; i < nHandles; ++i)
		{
			CHandleInformation::CHandleInfo &Handle = Handles[i];
			if (Handle.m_HandleName.f_ReplaceChar('\\', '/') == FileToFind)
			{
				NMib::NFile::CFileHandle &NewHandle = _HandleInfo.f_Insert();
				NewHandle.m_Process = Handle.m_ProcessName;
				NewHandle.m_ProcessID = Handle.m_ProcessID;
				NewHandle.m_HandleID = Handle.m_HandleID;
			}

		}
	}

	NSys::NFile::fg_Close(pFile);
	try
	{
		NSys::NFile::fg_Delete(TestFileName);
	}
	catch (NException::CException)
	{
	}
}

void NSys::NFile::fg_FileEnumOtherHandles(void *_pFile, NContainer::TCVector<NMib::NFile::CFileHandle> &_HandleInfo)
{
	void *pFileHandle = ((CWin32File *)_pFile)->m_pFile;
	uint32 CurrentProcessID = GetCurrentProcessId();

	CHandleInformation Info;
	TCVector<CHandleInformation::CHandleInfo> Handles;
	Info.f_EnumHandles(Handles);

	CHandleInformation::CHandleInfo *pThisHandle = nullptr;
	mint nHandles = Handles.f_GetLen();
	CStr ToTrace;
	for (mint i = 0; i < nHandles; ++i)
	{
		CHandleInformation::CHandleInfo &Handle = Handles[i];
		if (Handle.m_ProcessID == CurrentProcessID && Handle.m_HandleID == (uint32)(mint)pFileHandle)
		{
			pThisHandle = &Handle;
			break;
		}
	}
	if (pThisHandle)
	{
		mint nHandles = Handles.f_GetLen();
		for (mint i = 0; i < nHandles; ++i)
		{
			CHandleInformation::CHandleInfo &Handle = Handles[i];
			if (Handle.m_HandleName == pThisHandle->m_HandleName && &Handle != pThisHandle)
			{
				NMib::NFile::CFileHandle &NewHandle = _HandleInfo.f_Insert();
				NewHandle.m_Process = Handle.m_ProcessName;
				NewHandle.m_ProcessID = Handle.m_ProcessID;
				NewHandle.m_HandleID = Handle.m_HandleID;
			}

		}
	}
}

void NSys::NFile::fg_SetCreationTime(void *_pFile, const NTime::CTime &_Time)
{
	FILETIME Time;
	NMib::NFile::NPlatform::fg_Win32_MalterlibTimeToFileTime(_Time, Time);

	if (!SetFileTime(((CWin32File *)_pFile)->m_pFile, &Time, nullptr, nullptr))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from SetFileTime({}): {}") << ((CWin32File *)_pFile)->f_GetName() << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}

void NSys::NFile::fg_SetAccessTime(void *_pFile, const NTime::CTime &_Time)
{
	FILETIME Time;
	NMib::NFile::NPlatform::fg_Win32_MalterlibTimeToFileTime(_Time, Time);

	if (!SetFileTime(((CWin32File *)_pFile)->m_pFile, nullptr, &Time, nullptr))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from SetFileTime({}): {}") << ((CWin32File *)_pFile)->f_GetName() << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}

void NSys::NFile::fg_SetWriteTime(void *_pFile, const NTime::CTime &_Time)
{
	FILETIME Time;
	NMib::NFile::NPlatform::fg_Win32_MalterlibTimeToFileTime(_Time, Time);

	if (!SetFileTime(((CWin32File *)_pFile)->m_pFile, nullptr, nullptr, &Time))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from SetFileTime({}): {}") << ((CWin32File *)_pFile)->f_GetName() << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}

NTime::CTime NSys::NFile::fg_GetCreationTime(void *_pFile)
{
	FILETIME Time;
	if (!GetFileTime(((CWin32File *)_pFile)->m_pFile, &Time, nullptr, nullptr))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from GetFileTime({}): {}") << ((CWin32File *)_pFile)->f_GetName() << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}

	return NMib::NFile::NPlatform::fg_Win32_FileTimeToMalterlibTime(Time);
}

NTime::CTime NSys::NFile::fg_GetAccessTime(void *_pFile)
{
	FILETIME Time;
	if (!GetFileTime(((CWin32File *)_pFile)->m_pFile, nullptr, &Time, nullptr))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from GetFileTime({}): {}") << ((CWin32File *)_pFile)->f_GetName() << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}

	return NMib::NFile::NPlatform::fg_Win32_FileTimeToMalterlibTime(Time);
}

NTime::CTime NSys::NFile::fg_GetWriteTime(void *_pFile)
{
	FILETIME Time;
	if (!GetFileTime(((CWin32File *)_pFile)->m_pFile, nullptr, nullptr, &Time))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from GetFileTime({}): {}") << ((CWin32File *)_pFile)->f_GetName() << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}

	return NMib::NFile::NPlatform::fg_Win32_FileTimeToMalterlibTime(Time);
}

void NSys::NFile::fg_SetCreationTime(NMib::NStr::CStr const &_FileName, const NTime::CTime &_Time)
{
	CFile File;
	auto FileOpenFlags = EFileOpen_ShareAll | EFileOpen_ReadAttribs | EFileOpen_WriteAttribs;
	if (CFile::fs_FileExists(CStr(_FileName), EFileAttrib_Directory))
		FileOpenFlags |= EFileOpen_Directory;
	File.f_Open(_FileName, FileOpenFlags);
	File.f_SetCreationTime(_Time);
}

void NSys::NFile::fg_SetAccessTime(NMib::NStr::CStr const &_FileName, const NTime::CTime &_Time)
{
	CFile File;
	auto FileOpenFlags = EFileOpen_ShareAll | EFileOpen_ReadAttribs | EFileOpen_WriteAttribs;
	if (CFile::fs_FileExists(CStr(_FileName), EFileAttrib_Directory))
		FileOpenFlags |= EFileOpen_Directory;
	File.f_Open(_FileName, FileOpenFlags);
	File.f_SetAccessTime(_Time);
}

void NSys::NFile::fg_SetWriteTime(NMib::NStr::CStr const &_FileName, const NTime::CTime &_Time)
{
	CFile File;
	auto FileOpenFlags = EFileOpen_ShareAll | EFileOpen_ReadAttribs | EFileOpen_WriteAttribs;
	if (CFile::fs_FileExists(CStr(_FileName), EFileAttrib_Directory))
		FileOpenFlags |= EFileOpen_Directory;
	File.f_Open(_FileName, FileOpenFlags);
	File.f_SetWriteTime(_Time);
}

void NSys::NFile::fg_SetCreationTimeOnLink(NMib::NStr::CStr const &_FileName, const NTime::CTime &_Time)
{
	CFile File;
	auto FileOpenFlags = EFileOpen_ShareAll | EFileOpen_ReadAttribs | EFileOpen_WriteAttribs | EFileOpen_Link;
	if (CFile::fs_FileExists(CStr(_FileName), EFileAttrib_Directory))
		FileOpenFlags |= EFileOpen_Directory;
	File.f_Open(_FileName, FileOpenFlags);
	File.f_SetCreationTime(_Time);
}

void NSys::NFile::fg_SetAccessTimeOnLink(NMib::NStr::CStr const &_FileName, const NTime::CTime &_Time)
{
	CFile File;
	auto FileOpenFlags = EFileOpen_ShareAll | EFileOpen_ReadAttribs | EFileOpen_WriteAttribs | EFileOpen_Link;
	if (CFile::fs_FileExists(CStr(_FileName), EFileAttrib_Directory))
		FileOpenFlags |= EFileOpen_Directory;
	File.f_Open(_FileName, FileOpenFlags);
	File.f_SetAccessTime(_Time);
}

void NSys::NFile::fg_SetWriteTimeOnLink(NMib::NStr::CStr const &_FileName, const NTime::CTime &_Time)
{
	CFile File;
	auto FileOpenFlags = EFileOpen_ShareAll | EFileOpen_ReadAttribs | EFileOpen_WriteAttribs | EFileOpen_Link;
	if (CFile::fs_FileExists(CStr(_FileName), EFileAttrib_Directory))
		FileOpenFlags |= EFileOpen_Directory;
	File.f_Open(_FileName, FileOpenFlags);
	File.f_SetWriteTime(_Time);
}

NTime::CTime NSys::NFile::fg_GetCreationTime(NMib::NStr::CStr const& _FileName)
{
	auto FileOpenFlags = EFileOpen_ShareAll | EFileOpen_ReadAttribs;
	if (CFile::fs_FileExists(CStr(_FileName), EFileAttrib_Directory))
		FileOpenFlags |= EFileOpen_Directory;

	CFile File;
	File.f_Open(CStr(_FileName), FileOpenFlags);
	return File.f_GetCreationTime();
}

NTime::CTime NSys::NFile::fg_GetAccessTime(NMib::NStr::CStr const& _FileName)
{
	auto FileOpenFlags = EFileOpen_ShareAll | EFileOpen_ReadAttribs;
	if (CFile::fs_FileExists(CStr(_FileName), EFileAttrib_Directory))
		FileOpenFlags |= EFileOpen_Directory;

	CFile File;
	File.f_Open(CStr(_FileName), FileOpenFlags);
	return File.f_GetAccessTime();
}

NTime::CTime NSys::NFile::fg_GetWriteTime(NMib::NStr::CStr const& _FileName)
{
	auto FileOpenFlags = EFileOpen_ShareAll | EFileOpen_ReadAttribs;
	if (CFile::fs_FileExists(CStr(_FileName), EFileAttrib_Directory))
		FileOpenFlags |= EFileOpen_Directory;

	CFile File;
	File.f_Open(CStr(_FileName), FileOpenFlags);
	return File.f_GetWriteTime();
}

NTime::CTime NSys::NFile::fg_GetCreationTimeOnLink(NMib::NStr::CStr const& _FileName)
{
	auto FileOpenFlags = EFileOpen_ShareAll | EFileOpen_ReadAttribs | EFileOpen_Link;
	if (CFile::fs_FileExists(CStr(_FileName), EFileAttrib_Directory))
		FileOpenFlags |= EFileOpen_Directory;

	CFile File;
	File.f_Open(CStr(_FileName), FileOpenFlags);
	return File.f_GetCreationTime();
}

NTime::CTime NSys::NFile::fg_GetAccessTimeOnLink(NMib::NStr::CStr const& _FileName)
{
	auto FileOpenFlags = EFileOpen_ShareAll | EFileOpen_ReadAttribs | EFileOpen_Link;
	if (CFile::fs_FileExists(CStr(_FileName), EFileAttrib_Directory))
		FileOpenFlags |= EFileOpen_Directory;

	CFile File;
	File.f_Open(CStr(_FileName), FileOpenFlags);
	return File.f_GetAccessTime();
}

NTime::CTime NSys::NFile::fg_GetWriteTimeOnLink(NMib::NStr::CStr const& _FileName)
{
	auto FileOpenFlags = EFileOpen_ShareAll | EFileOpen_ReadAttribs | EFileOpen_Link;
	if (CFile::fs_FileExists(CStr(_FileName), EFileAttrib_Directory))
		FileOpenFlags |= EFileOpen_Directory;

	CFile File;
	File.f_Open(CStr(_FileName), FileOpenFlags);
	return File.f_GetWriteTime();
}

void *NSys::NFile::fg_ChangeNotification_Open(const CStr &_FileName, NMib::NFile::EFileChange _OpenFlags, NMib::NThread::CSemaphoreAggregate *_pReportTo)
{
	return fg_GetLocalSys()->m_FileChangeNoticationContext->f_Open(_FileName, _OpenFlags, _pReportTo);
}

void NSys::NFile::fg_ChangeNotification_Close(void *_pNotification)
{
	fg_GetLocalSys()->m_FileChangeNoticationContext->f_Close(_pNotification);
}

bool NSys::NFile::fg_ChangeNotification_Changed(void *_pNotification)
{
	return fg_GetLocalSys()->m_FileChangeNoticationContext->f_Changed(_pNotification);
}

bool NSys::NFile::fg_ChangeNotification_GetNotification(void *_pNotification, NMib::NStr::CStr &_Path, NMib::NFile::EFileChangeNotification &_Notification, NMib::NStr::CStr &_PathFrom)
{
	return fg_GetLocalSys()->m_FileChangeNoticationContext->f_GetNotification(_pNotification, _Path, _Notification, _PathFrom);
}

bool NSys::NFile::fg_ChangeNotification_Supported()
{
	return true;
}

NMib::NStr::CStr NSys::NFile::fg_GetOwnerOnLink(const NMib::NStr::CStr &_Path)
{
	return "";
}

NMib::NStr::CStr NSys::NFile::fg_GetGroupOnLink(const NMib::NStr::CStr &_Path)
{
	return "";
}

CStr NSys::NFile::fg_GetOwner(CStr const &_Path)
{
	return "";
}

CStr NSys::NFile::fg_GetGroup(CStr const &_Path)
{
	return "";
}

void NSys::NFile::fg_SetOwner(CStr const &_Path, CStr const &_Owner)
{
}

void NSys::NFile::fg_SetGroup(CStr const &_Path, CStr const &_Group)
{
}

void NSys::NFile::fg_SetOwner(void *_pFile, const NMib::NStr::CStr &_Owner)
{
}

void NSys::NFile::fg_SetGroup(void *_pFile, const NMib::NStr::CStr &_Group)
{
}

void NSys::NFile::fg_SetOwnerOnLink(CStr const &_Path, CStr const &_Owner)
{
}

void NSys::NFile::fg_SetGroupOnLink(CStr const &_Path, CStr const &_Group)
{
}

class CWin32FileFind
{
public:
	CWin32FileFind()
	{
		m_pFindHandle = nullptr;
		m_Mode = 0;
	}

	~CWin32FileFind()
	{
		if (m_pFindHandle)
		{
			FindClose(m_pFindHandle);
		}
	}

	EFileAttrib f_ParseAttrib()
	{
		uint32 FileAttribs = m_FindData.dwFileAttributes;
		CWStr OriginalFileName = m_FullPath + m_FindData.cFileName;

		EFileAttrib MalterlibAttr = fg_GetAttributesInternalWithAttribs<CWStr, CStr, false>(OriginalFileName, FileAttribs);

		return MalterlibAttr;
	}

	CWStr m_FullPath;
	CStr m_LastFullName;
	WIN32_FIND_DATAW m_FindData;
	void *m_pFindHandle;
	mint m_Mode;
};

	//fs_GetExpandedPath




HINSTANCE fg_Win32_GetInstance(const void *_pCode)
{
	MEMORY_BASIC_INFORMATION MemInfo;
	if (VirtualQuery(_pCode, &MemInfo, sizeof(MemInfo)))
	{
		return (HINSTANCE)MemInfo.AllocationBase;
	}
	return nullptr;
}



void *NSys::NFile::fg_FindOpen(const CStr &_FindPattern)
{
	CWStr FindPattern = NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_FindPattern);

	CWStr FullPath;
	if (FindPattern.f_Cmp("\\\\?\\UNC\\", 8) == 0)
	{
		FullPath = "/" + FindPattern.f_Extract(7);
		FullPath.f_SetAt(0, '/');
	}
	else if (FindPattern.f_Cmp("\\\\?\\", 4) == 0)
	{
		FullPath = FindPattern.f_Extract(4);
	}
	else
		FullPath = FindPattern;
	fg_StrReplaceChar(FullPath, '\\', '/');
	int iEnd = fg_StrFindCharReverse(FullPath, '/');

	if (iEnd >= 0)
		FullPath.f_GetStrUniqueWritable()[iEnd+1] = 0;
	else
		DMibErrorFile("Could not find the end of the path");

	CWin32FileFind *pFind = fg_ConstructObject<CWin32FileFind>(NMemory::CDefaultAllocator());

	FullPath.f_SetModified();

	pFind->m_FullPath = FullPath;
	pFind->m_pFindHandle = FindFirstFileW(FindPattern, &pFind->m_FindData);
	return pFind;
}

const CStr *NSys::NFile::fg_FindNext(void *_pFindContext, EFileAttrib &_FileAttribs)
{
	CWin32FileFind *pFind = (CWin32FileFind*)_pFindContext;

	if (pFind->m_pFindHandle == INVALID_HANDLE_VALUE)
		return nullptr;

	if (pFind->m_Mode == 2)
		return nullptr;
	else
	{
		bool bFound = false;
		while (!bFound)
		{
			if (pFind->m_Mode == 0)
			{
				++pFind->m_Mode;
			}
			else if (pFind->m_Mode == 1)
			{
				if (!FindNextFileW(pFind->m_pFindHandle, &pFind->m_FindData))
				{
					++pFind->m_Mode;
					return nullptr;
				}
			}

			bFound = fg_StrCmp(pFind->m_FindData.cFileName, ".") != 0 && fg_StrCmp(pFind->m_FindData.cFileName, "..") != 0;
		}
	}

	pFind->m_LastFullName = pFind->m_FullPath + pFind->m_FindData.cFileName;
	fg_StrReplaceChar(pFind->m_LastFullName, '\\', '/');
	_FileAttribs = pFind->f_ParseAttrib();
	return &pFind->m_LastFullName;

}

void NSys::NFile::fg_FindClose(void *_pFindContext)
{
	CWin32FileFind *pFind = (CWin32FileFind*)_pFindContext;
	fg_DeleteObject(NMemory::CDefaultAllocator(), pFind);
}

static DWORD CALLBACK fsg_CopyProgressRoutine
(
  LARGE_INTEGER _TotalFileSize,
  LARGE_INTEGER _TotalBytesTransferred,
  LARGE_INTEGER _StreamSize,
  LARGE_INTEGER _StreamBytesTransferred,
  DWORD _StreamNumber,
  DWORD _CallbackReason,
  HANDLE _hSourceFile,
  HANDLE _hDestinationFile,
  void *_pData
)
{
	NMib::NFile::CFileProgress *pProgress = (NMib::NFile::CFileProgress *)_pData;
	pProgress->f_Progress(_TotalBytesTransferred.QuadPart, _TotalFileSize.QuadPart);

	return PROGRESS_CONTINUE;
}

bool NMib::NSys::fg_System_GetOperatingSystemVersion(int &o_Major, int &o_Minor, int &o_Fix, EOperatingSystemArch &o_Arch, bool _bForceUpdate)
{
	OSVERSIONINFOEXW VersionInfo;
	fg_MemClear(VersionInfo);
	VersionInfo.dwOSVersionInfoSize = sizeof(VersionInfo);
	if (!GetVersionExW((OSVERSIONINFO *)&VersionInfo))
		return false;

	o_Major = VersionInfo.dwMajorVersion;
	o_Minor = VersionInfo.dwMinorVersion;
	o_Fix = VersionInfo.dwBuildNumber;

	switch (gs_SysInfo.dwProcessorType)
	{
	case PROCESSOR_ARCHITECTURE_INTEL:
		o_Arch = EOperatingSystemArch_x86;
		break;
	case PROCESSOR_ARCHITECTURE_AMD64:
		o_Arch = EOperatingSystemArch_x64;
		break;
	case PROCESSOR_ARCHITECTURE_ARM64:
		o_Arch = EOperatingSystemArch_arm64;
		break;		
	default:
		o_Arch = EOperatingSystemArch_Unknown;
		break;
	}

	return true;
}

void NSys::NFile::fg_Duplicate(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo)
{
	DMibErrorFile("Not supported");
}

bool NSys::NFile::fg_TryDuplicate(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo)
{
	return false;
}

void NSys::NFile::fg_Copy(const CStr &_FileFrom, const CStr &_FileTo, NMib::NFile::CFileProgress &_Progress)
{
	BOOL Cancel = false;
	uint32 Flags = 0;
	if (NLocal::g_VersionInfo.dwMajorVersion > 5 || (NLocal::g_VersionInfo.dwMajorVersion == 5 && NLocal::g_VersionInfo.dwMinorVersion >= 1))
		Flags |= COPY_FILE_ALLOW_DECRYPTED_DESTINATION;

	if (!CopyFileExW(NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_FileFrom), NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_FileTo), fsg_CopyProgressRoutine, &_Progress, &Cancel, Flags))
		DMibErrorFile((CStr::CFormat("Windows returned an error from CopyFile({}, {}): {}") << _FileFrom << _FileTo << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NSys::NFile::fg_Copy(const CStr &_FileFrom, const CStr &_FileTo)
{
	if (!CopyFileW(NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_FileFrom), NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_FileTo), false))
		DMibErrorFile((CStr::CFormat("Windows returned an error from CopyFile({}, {}): {}") << _FileFrom << _FileTo << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
}


bool NSys::NFile::fg_CanCreateSymbolicLink(EFileAttrib _Type, ESymbolicLinkFlag _Flags)
{
	if (_Type & EFileAttrib_Directory && !(_Flags & ESymbolicLinkFlag_Relative))
		return true;

	return fg_GetLocalSys()->f_EnableSymLinkSupport();
}

enum 
{
	EEmulateLinkVersion = 0x101
};

void NSys::NFile::fg_CreateSymbolicLink(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo, EFileAttrib _Type, ESymbolicLinkFlag _Flags)
{
	fg_GetLocalSys()->f_EnableSymLinkSupport();

	HRESULT CreateSymbolicLinkWError = 0;
	DWORD Flags = 0;
	if (NLocal::g_OptionalFunctions.m_fCreateSymbolicLinkW)
	{
		if (_Type & EFileAttrib_Directory)
			Flags |= 1;

		CWStr ToMount;
		if (_Flags & ESymbolicLinkFlag_Relative)
			ToMount = CWStr(_FileFrom).f_ReplaceChar('/', '\\');
		else if (_Flags & ESymbolicLinkFlag_ConvertToDevicePath)
		{
			ToMount = L"\\\\?\\GLOBALROOT" + NMib::NFile::NPlatform::fg_ConvertToDevicePath(_FileFrom);
			//ToMount.f_SetAt(1, '\\');
		}
		else
			ToMount = NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_FileFrom);

		if 
			(
				(NLocal::g_VersionInfo.dwMajorVersion > 10)
				|| (NLocal::g_VersionInfo.dwMajorVersion == 10 && NLocal::g_VersionInfo.dwMinorVersion > 0)
				|| (NLocal::g_VersionInfo.dwMajorVersion == 10 && NLocal::g_VersionInfo.dwBuildNumber >= 14972)
			)
		{
			Flags |= 2; // SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
		}

		if (NLocal::g_OptionalFunctions.m_fCreateSymbolicLinkW(NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_FileTo), ToMount, Flags))
			return;
		else
			CreateSymbolicLinkWError = GetLastError();

	}

	if (_Type & EFileAttrib_Directory && !(_Flags & ESymbolicLinkFlag_Relative))
	{ // Attempt using a reparse point first.
		CWStr ToMount;

		if (_Flags & ESymbolicLinkFlag_Relative)
			ToMount = CWStr(_FileFrom).f_ReplaceChar('/', '\\');
		else if (_Flags & ESymbolicLinkFlag_ConvertToDevicePath)
			ToMount = NMib::NFile::NPlatform::fg_ConvertToDevicePath(_FileFrom);
		else
		{
			ToMount = NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_FileFrom, true);
			ToMount.f_SetAt(1, '?');
		}

		NMib::NStr::CStr DestFile = _FileTo;

		if (NMib::NFile::CFile::fs_FileExists(DestFile)) // Relax to allow existing empty dir if _Type is dir?
			DMibErrorFile("Destination directory exists");

		EFileOpen TargetFileOpenFlags = EFileOpen_Write | EFileOpen_Link;

		if (_Type & EFileAttrib_Directory)
		{
			TargetFileOpenFlags |= EFileOpen_Directory;
			CFile::fs_CreateDirectory(DestFile);
		}

		CFile TargetFile;

		TargetFile.f_Open(DestFile, TargetFileOpenFlags);

		mint nPathBytes = ToMount.f_GetLen() * sizeof(ch16);

		mint Size = sizeof(REPARSE_DATA_BUFFER) + nPathBytes;
		REPARSE_DATA_BUFFER *pReparseData = (REPARSE_DATA_BUFFER *)NMemory::fg_Alloc(Size);
		fg_MemClear(pReparseData, Size);
		auto Cleanup = fg_OnScopeExit([&]{NMemory::fg_Free(pReparseData, Size);});

		{
			pReparseData->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
			pReparseData->ReparseDataLength    = nPathBytes + 12;
			pReparseData->Reserved             = 0;
			pReparseData->MountPointReparseBuffer.SubstituteNameOffset = 0;
			pReparseData->MountPointReparseBuffer.SubstituteNameLength = nPathBytes;
			pReparseData->MountPointReparseBuffer.PrintNameOffset      = nPathBytes + 2;
			pReparseData->MountPointReparseBuffer.PrintNameLength      = 0;
			fg_StrCopy(pReparseData->MountPointReparseBuffer.PathBuffer, ToMount);
		}

		mint nIOControlBytes = pReparseData->ReparseDataLength + 8;

		DWORD Return;

		if (DeviceIoControl(TargetFile.f_GetOSFile(), FSCTL_SET_REPARSE_POINT, pReparseData, nIOControlBytes, nullptr, 0, &Return, nullptr))
			return;
		else
		{
			DMibDTrace("DeviceIoControl(FSCTL_SET_REPARSE_POINT): {}\r\n", NMib::NPlatform::fg_Win32_GetLastErrorStr());
		}

	}

	if (_Flags & ESymbolicLinkFlag_AllowEmulation)
	{
		TCBinaryStreamFile<> File;
		File.f_Open(_FileTo, EFileOpen_Write | EFileOpen_ShareAll);
		File << EEmulateLinkVersion;
		File << _FileFrom;
		File.m_File.f_SetAttributes(File.m_File.f_GetAttributes() | EFileAttrib_Link | EFileAttrib_EmulatedLink | _Type); // Save link emulated

		return;
	}

	if (!NLocal::g_OptionalFunctions.m_fCreateSymbolicLinkW)
		DMibErrorFile("CreateSymbolicLink is not support on this version of Windows");

	DMibErrorFile((CStr::CFormat("Windows returned an error from CreateSymbolicLinkW({}, {}, {}): {}") << _FileTo << _FileFrom << Flags << NMib::NPlatform::fg_Win32_GetLastErrorStr(CreateSymbolicLinkWError)).f_GetStr());
}

NMib::NStr::CStr NSys::NFile::fg_ResolveSymbolicLink(const NMib::NStr::CStr &_FileFrom)
{
//	fg_GetLocalSys()->f_EnableBackupSupport();
	auto Attribs = fg_GetAttributesInternal<CWStr, CStr, false>(NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_FileFrom));
	if (Attribs & EFileAttrib_EmulatedLink)
	{
		TCBinaryStreamFile<> File;
		File.f_Open(_FileFrom, EFileOpen_Read | EFileOpen_ShareAll);

		uint32 Version;
		File >> Version;
		if (Version > EEmulateLinkVersion)
			DMibErrorFile(CStr(CStr::CFormat("Invalid emulated link version ({nfh} > {nfh})") << Version << EEmulateLinkVersion));
		CStr Link;
		File >> Link;

		return Link;
	}

	EFileOpen TargetFileOpenFlags = EFileOpen_ReadAttribs | EFileOpen_Link;
	if (Attribs & EFileAttrib_Directory)
		TargetFileOpenFlags |= EFileOpen_Directory;

	CFile TargetFile;
	TargetFile.f_Open(_FileFrom, TargetFileOpenFlags);

	mint Size = sizeof(REPARSE_DATA_BUFFER) + 65536 * sizeof(ch16);
	REPARSE_DATA_BUFFER *pReparseData = (REPARSE_DATA_BUFFER *)NMemory::fg_Alloc(Size);
	fg_MemClear(pReparseData, Size);
	auto Cleanup = fg_OnScopeExit([&]{NMemory::fg_Free(pReparseData, Size);});

	mint nIOControlBytes = Size;

	DWORD Return;

	if (!DeviceIoControl(TargetFile.f_GetOSFile(), FSCTL_GET_REPARSE_POINT, nullptr, 0, pReparseData, nIOControlBytes, &Return, nullptr))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from DeviceIoControl(FSCTL_GET_REPARSE_POINT, {}): {}") << _FileFrom << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}

	CWStr ReturnString;
	switch (pReparseData->ReparseTag)
	{
	case IO_REPARSE_TAG_MOUNT_POINT:
		ReturnString
			= CWStr
			(
				pReparseData->MountPointReparseBuffer.PathBuffer + pReparseData->MountPointReparseBuffer.SubstituteNameOffset / 2
				, pReparseData->MountPointReparseBuffer.SubstituteNameLength / 2
			)
		;
		break;
	case IO_REPARSE_TAG_SYMLINK:
		ReturnString
			= CWStr
			(
				pReparseData->SymbolicLinkReparseBuffer.PathBuffer + pReparseData->SymbolicLinkReparseBuffer.SubstituteNameOffset / 2
				, pReparseData->SymbolicLinkReparseBuffer.SubstituteNameLength / 2
			)
		;
		break;
	default:
		DMibErrorFile(CStr(CStr::CFormat("Unrecognized reparse tag: {nfh}") << pReparseData->ReparseTag));
	}
	if (ReturnString.f_StartsWith("\\??"))
	{
		ReturnString.f_SetAt(1, '\\');
	}
	if (ReturnString.f_StartsWith("\\\\?"))
		return NMib::NFile::NPlatform::fg_ConvertFromWindowsPath(ReturnString);
	return ReturnString.f_ReplaceChar('\\', '/');
}


void NSys::NFile::fg_CreateHardLink(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo)
{
	if (!NLocal::g_OptionalFunctions.m_fCreateHardLinkW)
		DMibErrorFile("CreateHardLink is not support on this version of Windows");

	if (!NLocal::g_OptionalFunctions.m_fCreateHardLinkW(NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_FileTo), NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_FileFrom), nullptr))
		DMibErrorFile((CStr::CFormat("Windows returned an error from CreateHardLinkW({}, {}): {}") << _FileTo << _FileFrom << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NSys::NFile::fg_Rename(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo, NMib::NFile::CFileProgress &_Progress)
{
	if
		(
			!MoveFileWithProgressW
			(
				NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_FileFrom)
				, NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_FileTo)
				, fsg_CopyProgressRoutine
				, &_Progress
				, MOVEFILE_WRITE_THROUGH | MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING
			)
		)
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from MoveFile({}, {}): {}") << _FileFrom << _FileTo << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}

void NSys::NFile::fg_Rename(const CStr &_FileFrom, const CStr &_FileTo)
{
	if
		(
			!MoveFileExW
			(
				NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_FileFrom)
				, NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_FileTo)
				, MOVEFILE_WRITE_THROUGH | MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING
			)
		)
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from MoveFile({}, {}): {}") << _FileFrom << _FileTo << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}

namespace
{
	struct CWindowsHandle
	{
		CWindowsHandle() = default;

		CWindowsHandle(HANDLE _pHandle)
			: m_pHandle(_pHandle)
		{
		}

		CWindowsHandle(CWindowsHandle &&_Other)
			: m_pHandle(fg_Exchange(_Other.m_pHandle, nullptr))
		{
		}

		~CWindowsHandle()
		{
			if (m_pHandle)
				CloseHandle(m_pHandle);
		}

		void f_Clear()
		{
			if (m_pHandle)
				CloseHandle(fg_Exchange(m_pHandle, nullptr));
		}

		operator bool () const
		{
			return !!m_pHandle;
		}

		HANDLE m_pHandle = nullptr;
	};

	template <typename tf_CStr, typename tf_CWStr>
	static CWindowsHandle fg_PreparePosixSemanticsRenameOrDelete(tf_CStr const &_File, tf_CWStr const &_WindowsFile, ch8 const *_pErrorDescription)
	{
		if (NLocal::g_VersionInfo.dwBuildNumber < 14393) // Windows 10 RS1
			return {};

		CWindowsHandle FileHandle = CreateFileW
			(
				_WindowsFile
				, DELETE
				, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE
				, nullptr
				, OPEN_EXISTING
				, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT
				, nullptr
			)
		;

		if (FileHandle.m_pHandle == INVALID_HANDLE_VALUE)
			DMibErrorFile("Windows returned an error from CreateFileW({})({}): {}"_f << _pErrorDescription << _File << NMib::NPlatform::fg_Win32_GetLastErrorStr());

		return FileHandle;
	}
}

#ifndef REPLACEFILE_IGNORE_ACL_ERRORS
#define REPLACEFILE_IGNORE_ACL_ERRORS 0x00000004
#endif

#ifndef FILE_RENAME_IGNORE_READONLY_ATTRIBUTE
#define FILE_RENAME_IGNORE_READONLY_ATTRIBUTE 0x00000040
#endif

template <typename tf_CWStr, bool t_bThrowError, typename tf_CStr>
static void fg_DeleteGeneric(tf_CStr &_File);

static void fg_AtomicReplaceImplementation(CStr const &_FileFrom, CStr const &_FileTo, bool _bTryNonAtomic)
{
	auto FileFrom = NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_FileFrom);
	if (auto FileHandle = fg_PreparePosixSemanticsRenameOrDelete(_FileFrom, FileFrom, "AtomicReplace"))
	{
		auto FileTo = NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_FileTo);

		mint SizeNeeded = sizeof(FILE_RENAME_INFO) + (FileTo.f_GetLen() + 1) * sizeof(ch16);

		TCVector<mint> Buffer;
		Buffer.f_SetLen((SizeNeeded + sizeof(mint) - 1) / sizeof(mint));

		FILE_RENAME_INFO *pRenameInfo = (FILE_RENAME_INFO *)Buffer.f_GetArray();

		pRenameInfo->RootDirectory = NULL;
		pRenameInfo->FileNameLength = FileTo.f_GetLen();
		pRenameInfo->Flags = FILE_RENAME_FLAG_POSIX_SEMANTICS | FILE_RENAME_FLAG_REPLACE_IF_EXISTS;
		if (NLocal::g_VersionInfo.dwBuildNumber >= 17763) // Windows 10 RS5
			pRenameInfo->Flags |= FILE_RENAME_IGNORE_READONLY_ATTRIBUTE;
		
		fg_StrCopy(pRenameInfo->FileName, FileTo.f_GetStr());

		if (!SetFileInformationByHandle(FileHandle.m_pHandle, FILE_INFO_BY_HANDLE_CLASS(22) /*FileRenameInfoEx*/, pRenameInfo, SizeNeeded))
		{
			auto Error = GetLastError();
			if (Error != ERROR_ACCESS_DENIED || !_bTryNonAtomic)
				DMibErrorFile("Windows returned an error from SetFileInformationByHandle(AtomicReplace)({}, {}): {} {}"_f << _FileFrom << _FileTo << Error << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));

			// Handle deletion of running executables

			FileHandle.f_Clear();
			CStr TempName = "{}~{}.TMP"_f << _FileTo << NCryptography::fg_RandomID();
			fg_AtomicReplaceImplementation(_FileTo, TempName, false);
			fg_AtomicReplaceImplementation(_FileFrom, _FileTo, false);

			fg_DeleteGeneric<CWStr, false>(TempName); // Try to delete if possible, but don't throw on failure
		}

		return;
	}

	if (!CFile::fs_FileExists(_FileTo))
		return CFile::fs_RenameFile(_FileFrom, _FileTo);

	DWORD Flags = REPLACEFILE_IGNORE_MERGE_ERRORS;
#ifdef REPLACEFILE_IGNORE_ACL_ERRORS
	if (NLocal::g_VersionInfo.dwMajorVersion >= 6)
		Flags |= REPLACEFILE_IGNORE_ACL_ERRORS;
#endif
	
	if (!ReplaceFileW(NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_FileTo), FileFrom, nullptr, Flags, nullptr, nullptr))
		DMibErrorFile((CStr::CFormat("Windows returned an error from ReplaceFile({}, {}): {}") << _FileFrom << _FileTo << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NSys::NFile::fg_AtomicReplace(CStr const &_FileFrom, CStr const &_FileTo)
{
	fg_AtomicReplaceImplementation(_FileFrom, _FileTo, true);
}

NMib::NStream::CFilePos NSys::NFile::fg_GetFreeSpace(const NMib::NStr::CStr &_Path)
{
	ULARGE_INTEGER FreeSpace;
	CWStr Path = NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_Path);
	if (Path.f_GetAt(Path.f_GetLen() - 1) == ':')
		Path += "\\";
	if (!GetDiskFreeSpaceExW(Path, &FreeSpace, nullptr, nullptr))
		DMibErrorFile((CStr::CFormat("Windows returned an error from GetDiskFreeSpaceExW({}): {}") << Path << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());

	return FreeSpace.QuadPart;
}

NMib::NStream::CFilePos NSys::NFile::fg_GetUsedSpace(const NMib::NStr::CStr &_Path)
{
	ULARGE_INTEGER FreeSpace;
	ULARGE_INTEGER TotalSpace;
	CWStr Path = NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_Path);
	if (Path.f_GetAt(Path.f_GetLen() - 1) == ':')
		Path += "\\";
	if (!GetDiskFreeSpaceExW(Path, &FreeSpace, &TotalSpace, nullptr))
		DMibErrorFile((CStr::CFormat("Windows returned an error from GetDiskFreeSpaceExW({}): {}") << Path << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());

	return TotalSpace.QuadPart - FreeSpace.QuadPart;
}

NMib::NStream::CFilePos NSys::NFile::fg_GetTotalSpace(const NMib::NStr::CStr &_Path)
{
	ULARGE_INTEGER FreeSpace;
	ULARGE_INTEGER TotalSpace;
	CWStr Path = NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_Path);
	if (Path.f_GetAt(Path.f_GetLen() - 1) == ':')
		Path += "\\";
	if (!GetDiskFreeSpaceExW(Path, &FreeSpace, &TotalSpace, nullptr))
		DMibErrorFile((CStr::CFormat("Windows returned an error from GetDiskFreeSpaceExW({}): {}") << Path << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());

	return TotalSpace.QuadPart;
}

NContainer::TCVector<NStr::CStr> NSys::NFile::fg_GetMounts(NMib::NFile::EFileMountType _Types)
{
	if (!(_Types & NMib::NFile::EFileMountType_Block))
		return {}; // We don't support special devices on Windows

	if ((_Types & (EFileMountType_Local | EFileMountType_Remote)) == EFileMountType_None)
		return {};

	using namespace NMib::NStr;


	NContainer::TCVector<CWStr> Volumes;
	CWStr VolumePath;
	HANDLE pFind = FindFirstVolumeW(VolumePath.f_GetStr(NMib::NFile::NPlatform::gc_MaxWindowsPath), NMib::NFile::NPlatform::gc_MaxWindowsPath);
	if (pFind == INVALID_HANDLE_VALUE)
	{
		auto LastError = GetLastError();
		if (LastError == ERROR_NO_MORE_FILES)
			return {};

		DMibErrorFile((CStr::CFormat("Windows returned an error from FindFirstVolumeW(): {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(LastError)).f_GetStr());
	}
	VolumePath.f_SetModified();

	auto Cleanup = g_OnScopeExit / [&]
		{
			FindVolumeClose(pFind);
		}
	;

	Volumes.f_Insert(VolumePath);

	while (true)
	{
		if (!FindNextVolumeW(pFind, VolumePath.f_GetStr(NMib::NFile::NPlatform::gc_MaxWindowsPath), NMib::NFile::NPlatform::gc_MaxWindowsPath))
		{
			auto LastError = GetLastError();
			if (LastError == ERROR_NO_MORE_FILES)
				break;
			DMibErrorFile((CStr::CFormat("Windows returned an error from FindNextVolumeW(): {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(LastError)).f_GetStr());
		}
		VolumePath.f_SetModified();

		Volumes.f_Insert(VolumePath);
	}

	NContainer::TCVector<CStr> Return;

	auto fAddPath = [&](CWStr const &_Path)
		{
			uint32 DriveType = GetDriveTypeW(_Path.f_GetStr());
			if (DriveType == DRIVE_REMOTE)
			{
				if (!(_Types & NMib::NFile::EFileMountType_Remote))
					return;
			}
			else
			{
				if (!(_Types & NMib::NFile::EFileMountType_Local))
					return;
			}

			Return.f_Insert(NMib::NFile::NPlatform::fg_ConvertFromWindowsPath(_Path));
		}
	;

	for (auto &Volume : Volumes)
	{
		fAddPath(Volume);

		HANDLE pFindVolume = FindFirstVolumeMountPointW(Volume.f_GetStr(), VolumePath.f_GetStr(NMib::NFile::NPlatform::gc_MaxWindowsPath), NMib::NFile::NPlatform::gc_MaxWindowsPath);
		if (pFindVolume == INVALID_HANDLE_VALUE)
		{
			auto LastError = GetLastError();
			if (LastError == ERROR_NO_MORE_FILES)
				continue;

			DMibErrorFile((CStr::CFormat("Windows returned an error from FindFirstVolumeMountPointW({}): {}") << Volume << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
		}
		VolumePath.f_SetModified();

		auto Cleanup = g_OnScopeExit / [&]
			{
				FindVolumeMountPointClose(pFindVolume);
			}
		;

		fAddPath(VolumePath);

		while (true)
		{
			if (!FindNextVolumeMountPointW(pFindVolume, VolumePath.f_GetStr(NMib::NFile::NPlatform::gc_MaxWindowsPath), NMib::NFile::NPlatform::gc_MaxWindowsPath))
			{
				auto LastError = GetLastError();
				if (LastError == ERROR_NO_MORE_FILES)
					break;
				DMibErrorFile((CStr::CFormat("Windows returned an error from FindNextVolumeW(): {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(LastError)).f_GetStr());
			}
			VolumePath.f_SetModified();

			fAddPath(VolumePath);
		}
	}

	return Return;
}

namespace
{
	bool fg_DirectoryExists(const ch16 *_pFileDirectory)
	{
		uint32 Attribs = GetFileAttributesW(_pFileDirectory);

		if (Attribs == INVALID_FILE_ATTRIBUTES)
			return false;

		if ((Attribs & FILE_ATTRIBUTE_DIRECTORY))
			return true;

		return false;
	}
	bool fg_ReparsePointDirectoryExists(const ch16 *_pFileDirectory)
	{
		uint32 Attribs = GetFileAttributesW(_pFileDirectory);

		if (Attribs == INVALID_FILE_ATTRIBUTES)
			return false;

		if ((Attribs & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
			return true;

		return false;
	}
}

template <typename tf_CWinStr, typename tf_CErrorStr, typename tf_CStr>
void fg_CreateDirectoryHelper(const tf_CStr &_FileDirectory)
{
	// CreateDirectoryW has a special 248 character limit...
	tf_CWinStr NewPath = NFile::NPlatform::fg_ConvertToWindowsPath<tf_CWinStr, tf_CWinStr>(_FileDirectory, true, -1, false);

	if (fg_DirectoryExists(NewPath))
		return;

	ch16 *pDir = NewPath.f_GetStrUniqueWritable();

	ch16 *pDirCheck;
	if (fg_StrCmpNoCase(pDir, "\\\\?\\UNC\\", 8) == 0)
		pDirCheck = pDir + 8;
	else if (fg_StrStartsWith(pDir, "\\\\?\\"))
		pDirCheck = pDir + 4;
	else
		pDirCheck = pDir;

	aint iCurrentPath = NMib::NStr::fg_StrFindChar(pDirCheck, '\\');
	if (iCurrentPath < 0)
		return;
	const ch16 *pCurrentPath = pDirCheck + iCurrentPath;

	pCurrentPath = fg_StrAdd(pCurrentPath+1,NMib::NStr::fg_StrFindChar(pCurrentPath+1, '\\'));

	while (1)
	{
		tf_CWinStr Current;
		if (!pCurrentPath)
			Current = pDir;
		else
			Current.f_AddStr(pDir, pCurrentPath - pDir);

		if (!fg_DirectoryExists(Current))
		{
			if (!CreateDirectoryW(Current, nullptr))
			{
				DWORD Error = GetLastError();
				if (Error == ERROR_ALREADY_EXISTS)
					Error = 0;
				if (Error)
					DMibErrorFile((typename tf_CErrorStr::CFormat("Windows returned an error from CreateDirectory({}, {}): {}") << Current << _FileDirectory << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
			}
		}
		if (!pCurrentPath)
			break;

		pCurrentPath = fg_StrAdd(pCurrentPath+1,NMib::NStr::fg_StrFindChar(pCurrentPath+1, '\\'));
	}
}

void NSys::NFile::fg_CreateDirectory(const CStr &_FileDirectory)
{
	fg_CreateDirectoryHelper<CWStr, CStr>(_FileDirectory);
}

void NSys::NFile::fg_CreateDirectory(const CStrNonTracked &_FileDirectory)
{
	fg_CreateDirectoryHelper<CWStrNonTracked, CStrNonTracked>(_FileDirectory);
}


void NSys::NFile::fg_DeleteDirectory(const CStr &_File);
void NSys::NFile::fg_DeleteDirectory(const CStrNonTracked &_File);

#ifndef FILE_DISPOSITION_FLAG_IGNORE_READONLY_ATTRIBUTE
#define FILE_DISPOSITION_FLAG_IGNORE_READONLY_ATTRIBUTE  0x00000010
#endif

template <typename tf_CWStr, bool t_bThrowError, typename tf_CStr>
static void fg_DeleteGeneric(tf_CStr &_File)
{
	auto FileName = NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal<tf_CWStr>(_File);

	if (auto FileHandle = fg_PreparePosixSemanticsRenameOrDelete(_File, FileName, "Delete"))
	{
		FILE_DISPOSITION_INFO_EX FileDispositoinInfo;

		FileDispositoinInfo.Flags = FILE_DISPOSITION_FLAG_DELETE | FILE_DISPOSITION_FLAG_POSIX_SEMANTICS;
		if (NLocal::g_VersionInfo.dwBuildNumber >= 17763) // Windows 10 RS5
			FileDispositoinInfo.Flags |= FILE_DISPOSITION_FLAG_IGNORE_READONLY_ATTRIBUTE;
		
		if (!SetFileInformationByHandle(FileHandle.m_pHandle, FILE_INFO_BY_HANDLE_CLASS(21) /*FileDispositionInfoEx*/, &FileDispositoinInfo, sizeof(FileDispositoinInfo)))
		{
			if constexpr (t_bThrowError)
				DMibErrorFile((CStr::CFormat("Windows returned an error from SetFileInformationByHandle(Delete)({}): {}") << _File << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
		}

		return;
	}

	if (!DeleteFileW(FileName))
	{
		if (fg_ReparsePointDirectoryExists(FileName))
			return NSys::NFile::fg_DeleteDirectory(_File);

		if constexpr (t_bThrowError)
			DMibErrorFile((CStr::CFormat("Windows returned an error from DeleteFile({}): {}") << _File << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}

void NSys::NFile::fg_Delete(const CStr &_File)
{
	fg_DeleteGeneric<CWStr, true>(_File);
}

void NSys::NFile::fg_Delete(const CStrNonTracked &_File)
{
	fg_DeleteGeneric<CWStrNonTracked, true>(_File);
}

void NSys::NFile::fg_DeleteDirectory(const CStr &_File)
{
	NTime::CClock Timeout{true};
	mint nTries = 0;

l_Retry:

	++nTries;

	if (!RemoveDirectoryW(NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_File)))
	{
		auto Error = GetLastError();
		if (Error == ERROR_DIR_NOT_EMPTY)
		{
			if (Timeout.f_GetTime() < 0.1 || nTries < 10)
			{
				Sleep(0);
				goto l_Retry;
			}
		}
		DMibErrorFile((CStr::CFormat("Windows returned an error from RemoveDirectory({}): {}") << _File << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}


void NSys::NFile::fg_DeleteDirectory(const CStrNonTracked &_File)
{
	NTime::CClock Timeout{true};
	mint nTries = 0;

l_Retry:

	++nTries;

	if (!RemoveDirectoryW(NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal<CWStrNonTracked, CWStrNonTracked>(_File)))
	{
		auto Error = GetLastError();
		if (Error == ERROR_DIR_NOT_EMPTY)
		{
			if (Timeout.f_GetTime() < 0.1 || nTries < 10)
			{
				Sleep(0);
				goto l_Retry;
			}
		}
		DMibErrorFile((CStrNonTracked::CFormat("Windows returned an error from RemoveDirectory({}): {}") << _File << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}


void NSys::NFile::fg_SetCurrentDirectory(const NMib::NStr::CStr &_Directory)
{
	if (!SetCurrentDirectoryW(NMib::NFile::NPlatform::fg_ConvertToWindowsPathLocal(_Directory)))
		DMibErrorFile((CStr::CFormat("Windows returned an error from SetCurrentDirectoryW({}): {}") << _Directory << NMib::NPlatform::fg_Win32_GetLastErrorStr(0)).f_GetStr());
}

ch8 const *NSys::NFile::fg_GetDllExtension()
{
	return ".dll";
}

CStr NSys::NFile::fg_GetProgramDirectory()
{
	return fg_GetLocalSys()->m_ProgramDir_CStr;
}


CStr NSys::NFile::fg_GetProgramPath()
{
	return fg_GetLocalSys()->m_ProgramPath_CStr;
} 

CStr NSys::NFile::fg_GetCurrentDirectory()
{
	CWStr Ret;
	if (!GetCurrentDirectoryW(NMib::NFile::NPlatform::gc_MaxWindowsPath, Ret.f_GetStr(NMib::NFile::NPlatform::gc_MaxWindowsPath)))
		DMibErrorFile((CStr::CFormat("Windows returned an error from GetCurrentDirectoryW: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(0)).f_GetStr());
	Ret.f_SetModified();
	return NMib::NFile::NPlatform::fg_ConvertFromWindowsPath(Ret);
}

#include <shlobj.h>
NStr::CStr NSys::NFile::fg_GetUserProgramDirectory()
{
	WCHAR szPath[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA|CSIDL_FLAG_CREATE, nullptr, 0, szPath))) 
	{
		CStr FileName = NMib::NFile::CFile::fs_GetFileNoExt(fg_GetProgramPath());
		aint iFind = FileName.f_FindReverse("_x");
		if (iFind >= 0)
			FileName = FileName.f_Left(iFind);

		CStr Ret = NMib::NFile::NPlatform::fg_ConvertFromWindowsPath(CWStr(szPath));
		Ret += "/";
		Ret += FileName;
		return Ret;
	}	

	return fg_GetProgramDirectory();
}

NStr::CStr NSys::NFile::fg_GetUserLocalProgramDirectory()
{
	WCHAR szPath[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA|CSIDL_FLAG_CREATE, nullptr, 0, szPath)))
	{
		CStr FileName = NMib::NFile::CFile::fs_GetFileNoExt(fg_GetProgramPath());
		aint iFind = FileName.f_FindReverse("_x");
		if (iFind >= 0)
			FileName = FileName.f_Left(iFind);
		CStr Ret = NMib::NFile::NPlatform::fg_ConvertFromWindowsPath(CWStr(szPath));
		Ret += "/";
		Ret += FileName;
		return Ret;
	}	

	return fg_GetProgramDirectory();
}

NStr::CStr NSys::NFile::fg_GetUserLocalProgramCacheDirectory()
{
	return fg_GetUserLocalProgramDirectory();
}

NStr::CStr NSys::NFile::fg_GetTemporaryDirectory()
{
	WCHAR szPath[MAX_PATH];
	if (SUCCEEDED(GetTempPath(MAX_PATH, szPath)))
	{
		CStr FileName = NMib::NFile::CFile::fs_GetFileNoExt(fg_GetProgramPath());
		aint iFind = FileName.f_FindReverse("_x");
		if (iFind >= 0)
			FileName = FileName.f_Left(iFind);
		CStr Ret = NMib::NFile::NPlatform::fg_ConvertFromWindowsPath(CWStr(szPath));
		Ret += "/";
		Ret += FileName;
		return Ret;
	}

	return fg_GetProgramDirectory();
}

NStr::CStr NSys::NFile::fg_GetRawTemporaryDirectory()
{
	WCHAR szPath[MAX_PATH];
	if (SUCCEEDED(GetTempPath(MAX_PATH, szPath)))
		return NMib::NFile::NPlatform::fg_ConvertFromWindowsPath(CWStr(szPath));

	return fg_GetProgramDirectory();
}

NMib::NStr::CStr NSys::NFile::fg_GetUserHomeDirectory()
{
	WCHAR szPath[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROFILE|CSIDL_FLAG_CREATE, nullptr, 0, szPath)))
		return NMib::NFile::NPlatform::fg_ConvertFromWindowsPath(CWStr(szPath));

	return fg_GetProgramDirectory();
}

CStr NSys::NFile::fg_GetModulePath(void *_pCode)
{
	MEMORY_BASIC_INFORMATION MemInfo;
	if (VirtualQuery(_pCode, &MemInfo, sizeof(MemInfo)))
	{
		CWStr BaseName;
		if (!GetModuleFileName((HMODULE)MemInfo.AllocationBase, BaseName.f_GetStr(1024), 1024))
		{
			DMibError((CStr::CFormat("Windows returned an error from GetModuleFileName(0x{nfh,sj*2,sf0}): {}") << (mint)MemInfo.AllocationBase << NMib::NPlatform::fg_Win32_GetLastErrorStr() << sizeof(mint)*2).f_GetStr());
		}
		return NMib::NFile::NPlatform::fg_ConvertFromWindowsPath(BaseName);
	}

	return "";
}

CStrNonTracked NSys::NFile::fg_GetProgramPathNonTracked()
{
	return fg_GetLocalSys()->m_ProgramPath_CStrNonTracked;
} 

CStrNonTracked NSys::NFile::fg_GetProgramDirectoryNonTracked()
{
	return fg_GetLocalSys()->m_ProgramDir_CStrNonTracked;
} 

CStrNonTracked NSys::NFile::fg_GetCurrentDirectoryNonTracked()
{
	CWStrNonTracked Ret;
	if (!GetCurrentDirectoryW(NMib::NFile::NPlatform::gc_MaxWindowsPath, Ret.f_GetStr(NMib::NFile::NPlatform::gc_MaxWindowsPath)))
		DMibErrorFile((CStr::CFormat("Windows returned an error from GetCurrentDirectoryW: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(0)).f_GetStr());
	Ret.f_SetModified();
	return NMib::NFile::NPlatform::fg_ConvertFromWindowsPath<CWStrNonTracked, CStrNonTracked>(Ret);
}
CStrNonTracked NSys::NFile::fg_GetUserProgramDirectoryNonTracked()
{
	WCHAR szPath[MAX_PATH];
	if(SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA|CSIDL_FLAG_CREATE, nullptr, 0, szPath))) 
	{
		CStrNonTracked FileName = NMib::NFile::CFile::fs_GetFileNoExt(fg_GetProgramPathNonTracked());
		aint iFind = FileName.f_FindReverse("_x");
		if (iFind >= 0)
			FileName = FileName.f_Left(iFind);

		CStrNonTracked Ret = NMib::NFile::NPlatform::fg_ConvertFromWindowsPath<CWStrNonTracked, CStrNonTracked>(CWStrNonTracked(szPath));
		Ret += "/";
		Ret += FileName;
		return Ret;
	}	
	return fg_GetProgramDirectoryNonTracked();
}
CStrNonTracked NSys::NFile::fg_GetUserLocalProgramDirectoryNonTracked()
{
	WCHAR szPath[MAX_PATH];
	if(SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA|CSIDL_FLAG_CREATE, nullptr, 0, szPath))) 
	{
		CStrNonTracked FileName = NMib::NFile::CFile::fs_GetFileNoExt(fg_GetProgramPathNonTracked());
		aint iFind = FileName.f_FindReverse("_x");
		if (iFind >= 0)
			FileName = FileName.f_Left(iFind);
		CStrNonTracked Ret = NMib::NFile::NPlatform::fg_ConvertFromWindowsPath<CWStrNonTracked, CStrNonTracked>(CWStrNonTracked(szPath));
		Ret += "/";
		Ret += FileName;
		return Ret;
	}	

	return fg_GetProgramDirectoryNonTracked();
}

NMib::NStr::CStr NSys::NFile::fg_GetLogDirectory()
{
	return fg_GetUserLocalProgramDirectory();
}

NMib::NStr::CStrNonTracked NSys::NFile::fg_GetLogDirectoryNonTracked()
{
	return fg_GetUserLocalProgramDirectoryNonTracked();
}

NMib::NStr::CStrNonTracked NSys::NFile::fg_GetUserHomeDirectoryNonTracked()
{
	WCHAR szPath[MAX_PATH];
	if(SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROFILE|CSIDL_FLAG_CREATE, nullptr, 0, szPath))) 
	{
		CStrNonTracked FileName = NMib::NFile::CFile::fs_GetFileNoExt(fg_GetProgramPathNonTracked());
		aint iFind = FileName.f_FindReverse("_x");
		if (iFind >= 0)
			FileName = FileName.f_Left(iFind);
		CStrNonTracked Ret = NMib::NFile::NPlatform::fg_ConvertFromWindowsPath<CWStrNonTracked, CStrNonTracked>(CStrNonTracked(szPath));
		return Ret;
	}	

	return fg_GetProgramDirectoryNonTracked();
}

CStrNonTracked NSys::NFile::fg_GetUserLocalProgramCacheDirectoryNonTracked()
{
	return fg_GetUserLocalProgramDirectoryNonTracked();
}
CStrNonTracked NSys::NFile::fg_GetTemporaryDirectoryNonTracked()
{
	WCHAR szPath[MAX_PATH];
	if(SUCCEEDED(GetTempPath(MAX_PATH, szPath))) 
	{
		CStrNonTracked FileName = NMib::NFile::CFile::fs_GetFileNoExt(fg_GetProgramPathNonTracked());
		aint iFind = FileName.f_FindReverse("_x");
		if (iFind >= 0)
			FileName = FileName.f_Left(iFind);
		CStrNonTracked Ret = NMib::NFile::NPlatform::fg_ConvertFromWindowsPath<CWStrNonTracked, CStrNonTracked>(CStrNonTracked(szPath));
		Ret += "/";
		Ret += FileName;
		return Ret;
	}	

	return fg_GetProgramDirectoryNonTracked();
}

CStrNonTracked NSys::NFile::fg_GetRawTemporaryDirectoryNonTracked()
{
	WCHAR szPath[MAX_PATH];
	if (SUCCEEDED(GetTempPath(MAX_PATH, szPath)))
		return NMib::NFile::NPlatform::fg_ConvertFromWindowsPath<CWStrNonTracked, CStrNonTracked>(CStrNonTracked(szPath));
	return fg_GetProgramDirectoryNonTracked();
}

CStrNonTracked NSys::NFile::fg_GetModulePathNonTracked(void *_pCode)
{
	MEMORY_BASIC_INFORMATION MemInfo;
	if (VirtualQuery(_pCode, &MemInfo, sizeof(MemInfo)))
	{
		CWStrNonTracked BaseName;
		if (!GetModuleFileName((HMODULE)MemInfo.AllocationBase, BaseName.f_GetStr(1024), 1024))
		{
			DMibError((CStr::CFormat("Windows returned an error from GetModuleFileName(0x{nfh,sj*2,sf0}): {}") << (mint)MemInfo.AllocationBase << NMib::NPlatform::fg_Win32_GetLastErrorStr() << sizeof(mint)*2).f_GetStr());
		}
		return NMib::NFile::NPlatform::fg_ConvertFromWindowsPath<CWStrNonTracked, CStrNonTracked>(BaseName);
	}

	return "";
}


// *************************************************************************************************************************
// Net Implementation
// *************************************************************************************************************************

NSys::NNetwork::CAddress NSys::NNetwork::fg_CreateAddress(::NMib::NNetwork::ENetAddressType _Type, void const* _pData, mint _nDataBytes)
{
	return (NSys::NNetwork::CAddress)fg_GetLocalSys()->m_SocketContext->f_CreateAddress(_Type, _pData, _nDataBytes);
}

NSys::NNetwork::CAddress NSys::NNetwork::fg_DuplicateAddress(NSys::NNetwork::CAddress _ToCopy)
{
	return (NSys::NNetwork::CAddress)fg_GetLocalSys()->m_SocketContext->f_DuplicateAddress((CWindowsAddress*)_ToCopy);
}

::NMib::NNetwork::ENetAddressType NSys::NNetwork::fg_GetAddressType(NSys::NNetwork::CAddress _Address)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_GetAddressType(*(CWindowsAddress*)_Address);
}

bool NSys::NNetwork::fg_GetAddressRaw(NSys::NNetwork::CAddress _Address, ::NMib::NNetwork::ENetAddressType _ExpectedType, void* _opRawData, mint _nDataBytes)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_GetAddressRaw(*(CWindowsAddress*)_Address, _ExpectedType, _opRawData, _nDataBytes);
}

NSys::NNetwork::CAddress NSys::NNetwork::fg_SetAddressRaw(NSys::NNetwork::CAddress _Address, ::NMib::NNetwork::ENetAddressType _Type, void const* _pRawData, mint _nDataBytes)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return (NSys::NNetwork::CAddress)fg_GetLocalSys()->m_SocketContext->f_SetAddressRaw((CWindowsAddress*)_Address, _Type, _pRawData, _nDataBytes);
}

NSys::NNetwork::CAddress NSys::NNetwork::fg_ResolveAddress(const NMib::NStr::CStr &_Address, ::NMib::NNetwork::ENetAddressType _PreferType)
{
	return fg_GetLocalSys()->m_SocketContext->f_ResolveAddress(_Address, _PreferType);
}

void *NSys::NNetwork::fg_AsyncResolveAddress_Open(const NMib::NStr::CStr &_Address, ::NMib::NNetwork::ENetAddressType _PreferType, NMib::NFunction::TCFunction<void ()> &&_fOnFinish)
{
	return fg_GetLocalSys()->m_SocketContext->f_AsyncResolveAddress_Open(_Address, _PreferType, fg_Move(_fOnFinish));
}

bool NSys::NNetwork::fg_AsyncResolveAddress_GetResult(void *_pResolver, NSys::NNetwork::CAddress& _opAddress, NMib::NStr::CStr &_Error)
{
	return fg_GetLocalSys()->m_SocketContext->f_AsyncResolveAddress_GetResult(_pResolver, (CWindowsAddress*&)_opAddress, _Error);
}

void NSys::NNetwork::fg_AsyncResolveAddress_Close(void *_pResolver)
{
	fg_GetLocalSys()->m_SocketContext->f_AsyncResolveAddress_Close(_pResolver);
}

int NSys::NNetwork::fg_CompareAddresses(NSys::NNetwork::CAddress _pFirst, NSys::NNetwork::CAddress _pSecond)
{
	DMibSafeCheck(_pFirst != nullptr, "Address is null!");
	DMibSafeCheck(_pSecond != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_CompareAddresses(*(CWindowsAddress*)_pFirst, *(CWindowsAddress*)_pSecond);
}

void NSys::NNetwork::fg_FreeAddress(NSys::NNetwork::CAddress _Address) // It is OK to free a nullptr address
{
	return fg_GetLocalSys()->m_SocketContext->f_FreeAddress((CWindowsAddress*)_Address);
}

NMib::NStr::CStr NSys::NNetwork::fg_GetAddressString(NSys::NNetwork::CAddress _Address, ENetAddressStringFlag _Flags)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_GetAddressString(*(CWindowsAddress*)_Address, _Flags);
}

// Connection Operations
void *NSys::NNetwork::fg_AsyncConnect
	(
		NSys::NNetwork::CAddress _Address
		, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
		, NSys::NNetwork::CAddress _BindAddress
	)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_AsyncConnect(*(CWindowsAddress *)_Address, fg_Move(_fOnStateChange), (CWindowsAddress *)_BindAddress);
}

void NSys::NNetwork::fg_StartSocket(void *_pSocket)
{
	return fg_GetLocalSys()->m_SocketContext->f_StartSocket((CWindowsSocket *)_pSocket);
}

void *NSys::NNetwork::fg_Listen
	(
		NSys::NNetwork::CAddress _Address
		, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
		, NMib::NNetwork::ENetFlag _Flags
	)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_Listen(*(CWindowsAddress*)_Address, fg_Move(_fOnStateChange), _Flags);
}

void *NSys::NNetwork::fg_ListenDatagram
	(
		NSys::NNetwork::CAddress _Address
		, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange
		, NMib::NNetwork::ENetFlag _Flags
	)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_ListenDatagram(*(CWindowsAddress*)_Address, fg_Move(_fOnStateChange), _Flags);
}

void *NSys::NNetwork::fg_Accept(void *_pSocket, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange)
{
	return fg_GetLocalSys()->m_SocketContext->f_Accept((CWindowsSocket*)_pSocket, fg_Move(_fOnStateChange));
}

void NSys::NNetwork::fg_Close(void *_pSocket) // Closes the socket and connectio
{
	fg_GetLocalSys()->m_SocketContext->f_Close((CWindowsSocket*)_pSocket);
}

void NSys::NNetwork::fg_Shutdown(void *_pSocket) // Closes the socket and connectio
{
	fg_GetLocalSys()->m_SocketContext->f_Shutdown((CWindowsSocket*)_pSocket);
}

mint NSys::NNetwork::fg_Receive(void *_pSocket, void *_pData, mint _DataLen) // Returns bytes receive
{
	return fg_GetLocalSys()->m_SocketContext->f_Receive((CWindowsSocket*)_pSocket, _pData, _DataLen);
}

mint NSys::NNetwork::fg_Send(void *_pSocket, const void *_pData, mint _DataLen) // Returns bytes sen
{
	return fg_GetLocalSys()->m_SocketContext->f_Send((CWindowsSocket*)_pSocket, _pData, _DataLen);
}

mint NSys::NNetwork::fg_SendDatagram(void *_pSocket, NSys::NNetwork::CAddress _Address, const void *_pData, mint _DataLen) // Returns bytes sen
{
	return fg_GetLocalSys()->m_SocketContext->f_SendDatagram((CWindowsSocket*)_pSocket, *((CWindowsAddress*)_Address), _pData, _DataLen);
}

mint NSys::NNetwork::fg_ReceiveDatagram(void *_pSocket, NSys::NNetwork::CAddress _Address, void *_pData, mint _DataLen) // Returns bytes sen
{
	return fg_GetLocalSys()->m_SocketContext->f_ReceiveDatagram((CWindowsSocket*)_pSocket, *((CWindowsAddress*)_Address), _pData, _DataLen);
}

// Socket Properties & State

void NSys::NNetwork::fg_SetOnStateChange(void *_pSocket, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange)
{
	fg_GetLocalSys()->m_SocketContext->f_SetOnStateChange((CWindowsSocket*)_pSocket, fg_Move(_fOnStateChange));
}

NMib::NNetwork::ENetTCPState NSys::NNetwork::fg_GetState(void *_pSocket) // Get the state of data availabl
{
	return fg_GetLocalSys()->m_SocketContext->f_GetState((CWindowsSocket*)_pSocket);
}

NMib::NStr::CStr NSys::NNetwork::fg_GetCloseReason(void *_pSocket)
{
	return fg_GetLocalSys()->m_SocketContext->f_GetCloseReason((CWindowsSocket*)_pSocket);
}

void *NSys::NNetwork::fg_InheritHandle2(void *_pSocket, NMib::NFunction::TCFunctionMovable<void (::NMib::NNetwork::ENetTCPState _StateAdded)> &&_fOnStateChange)
{
	return fg_GetLocalSys()->m_SocketContext->f_InheritHandle2((CWindowsSocket*)_pSocket, fg_Move(_fOnStateChange));
}

void *NSys::NNetwork::fg_GiveUpForInherit(void *_pSocket)
{
	return fg_GetLocalSys()->m_SocketContext->f_GiveUpForInherit((CWindowsSocket*)_pSocket);
}

void *NSys::NNetwork::fg_GetOSSocket(void *_pSocket)
{
	return fg_GetLocalSys()->m_SocketContext->f_GetOSSocket((CWindowsSocket*)_pSocket);
}

NSys::NNetwork::CAddress NSys::NNetwork::fg_GetPeerAddress(void *_pSocket)
{
	return (NSys::NNetwork::CAddress)fg_GetLocalSys()->m_SocketContext->f_GetPeerAddress((CWindowsSocket*)_pSocket);
}

uint32 NSys::NNetwork::fg_GetListenPort(void *_pSocket)
{
	return fg_GetLocalSys()->m_SocketContext->f_GetListenPort((CWindowsSocket*)_pSocket);
}



#ifndef _DEBUG
#	if (_MSC_VER >= 1300)
// On vc7 we can att link time code generation
//#pragma comment(linker, "/LTCG")
#	endif
//#pragma comment(linker, "/OPT:REF")
//#pragma comment(linker, "/OPT:ICF,16")
//#pragma comment(linker, "/OPT:nowin98")
#	else
#endif


#if _MSC_FULL_VER >= 140050214

#pragma comment(linker, "/merge:.CRT=.rdata")

#else  /* _MSC_FULL_VER >= 140050214 */

#if defined (_M_IA64) || defined (_M_AMD64)
#pragma comment(linker, "/merge:.CRT=.rdata")
#else  /* defined (_M_IA64) || defined (_M_AMD64) */
#pragma comment(linker, "/merge:.CRT=.data")
#endif  /* defined (_M_IA64) || defined (_M_AMD64) */

#endif  /* _MSC_FULL_VER >= 140050214 */

//#pragma comment(linker, "/merge:.CRT=.data")

//#pragma comment(linker, "/defaultlib:kernel32.lib")
//#pragma comment(linker, "/defaultlib:user32.lib")

//#pragma comment(linker, "/Stub:c:\\NOSTUB.EXE")

//#pragma comment(linker, "/Stub:VC7Fix\\NOSTUB.EXE")



#ifndef _DEBUG
//#pragma comment(linker,"/RELEASE")
#	pragma comment(linker,"/merge:.data=.var")
#	pragma comment(linker,"/merge:.rdata=.const")
#	pragma comment(linker,"/merge:.text=.code")
// Note that merging the .rdata section will result in LARGER exe's if you using
// MFC (esp. static link). If this is desirable, define _MERGE_RDATA_ in your project.
//#pragma comment(linker,"/merge:.rdata=.d")
//#pragma comment(linker,"/merge:.text=.data")
//#pragma comment(linker,"/merge:.reloc=.d")
//#pragma comment(linker,"/merge:.reloc=.r")
//#pragma comment(linker,"/ALIGN:0x200")
#endif // NDEBUG


//void wWinMainCRTStartup(
//void WinMainCRTStartup(
//void wmainCRTStartup(
//void mainCRTStartup(

namespace NLocal
{
	HMODULE g_hNtDll = nullptr;
	HMODULE g_hKernel32 = nullptr;
	HMODULE g_hAdvAPI32 = nullptr;
	HMODULE g_hAPIMSWinCoreSynchl120 = nullptr;
}

bool g_bValidExitProcess = false;
void __cdecl fg_ValidExitProcess()
{
	g_bValidExitProcess = true;
}

bool g_bValidDestroyModule = false;
void __cdecl fg_ValidDestroyModule()
{
	g_bValidDestroyModule = true;
}

bool g_bAllowInvalidExit = false;



VOID WINAPI fg_HookExitProcess(__in  UINT _ExitCode)
{
	//DMibTraceSafe("fg_HookExitProcess {} {}\r\n", _ExitCode << g_bValidExitProcess);
	fg_GetLocalSys()->f_DestroyThreadSpecific();
#ifdef DMibDebug
	fg_DestroyMalterlib();
#endif

	if (!g_bValidExitProcess && !g_bAllowInvalidExit)
	{
		NMib::NContainer::TCVector<CStr> GeneratedDumps;
		NSys::fg_Debug_GenerateCrashDump((CStr::CFormat("ExitProcess was called with exit code {}") << int32(_ExitCode)).f_GetStr(), "", GeneratedDumps, true);

		if (g_fOrgTerminateProcess)
			g_fOrgTerminateProcess(GetCurrentProcess(), _ExitCode);
		else
			TerminateProcess(GetCurrentProcess(), _ExitCode);
	}

	g_fOrgExitProcess(_ExitCode);
}


BOOL WINAPI fg_HookTerminateProcess(__in  HANDLE _hProcess,__in  UINT _ExitCode)
{
//	DMibTraceSafe("fg_HookTerminateProcess {} {}\r\n", _hProcess << _ExitCode);
	if (_hProcess == GetCurrentProcess() && !g_bAllowInvalidExit)
	{
		NMib::NContainer::TCVector<CStr> GeneratedDumps;
		NSys::fg_Debug_GenerateCrashDump((CStr::CFormat("TerminateProcess was called with exit code {}") << int32(_ExitCode)).f_GetStr(), "", GeneratedDumps, true);
	}

	return g_fOrgTerminateProcess(_hProcess, _ExitCode);
}

bool g_bPatchedExitProcess = false;
bool g_bPatchedTerminateProcess = false;
bool g_bSystemCreated = false;

void NSys::fg_DestroySystem()
{
}

void NSys::fg_PreDestroyHeap()
{
}

namespace NMib
{
	
	mint align_cacheline g_SystemMemory[sizeof(CSystemWindowsMSVC) / sizeof(mint)];
	static_assert(__alignof(g_SystemMemory) >= mint(DMibPMemoryCacheLineSize), "Alignment didn't work");
	mint g_bCreatingSystemDone = false;
	mint g_bCanUseSystemMalloc = true;
	constinit NAtomic::TCAtomicAggregate<mint> g_bCanStartThreads = {DAggregateInit};
	mint g_bCreatedSystem = false;
	namespace NSys
	{
		namespace NPrivate
		{
			mint g_VirtualAllocGranularity = 64*1024;
			mint g_VirtualAllocGranularityLarge = 64*1024;
			mint g_PageSizeLarge = 64*1024;
		}
		
	}
}

void NSys::fg_CreateSystem()
{
	if (g_bCreatedSystem)
		return;
	g_bSystemCreated = true;
	g_bCreatedSystem = true;

	GetSystemInfo(&gs_SysInfo);     // Get the system info structure. Used by various functions. Let it reside in every module for speed of access.

	NPrivate::g_PageSizeLarge = gs_SysInfo.dwPageSize;

	NPrivate::g_VirtualAllocGranularity = gs_SysInfo.dwAllocationGranularity;

	DMibFastCheck(gs_SysInfo.dwPageSize == 4096);

	if (NLocal::g_OptionalFunctions.m_fLargePageMinimum)
		NPrivate::g_PageSizeLarge = fg_Max(NLocal::g_OptionalFunctions.m_fLargePageMinimum(), gs_SysInfo.dwPageSize);

	NPrivate::g_VirtualAllocGranularityLarge = fg_Max(NPrivate::g_PageSizeLarge, NPrivate::g_VirtualAllocGranularity);

	gs_ThreadLocalParentThread = NSys::fg_Thread_AllocLocal();

//	if (!g_bIsDll)
	{
		if (S_OK == NMib::NPlatform::fg_PatchIAT(g_hDllInstance, "KERNEL32.dll", "ExitProcess", (PVOID) fg_HookExitProcess, (PVOID *) &g_fOrgExitProcess))
			g_bPatchedExitProcess = true;
		if (S_OK == NMib::NPlatform::fg_PatchIAT(g_hDllInstance, "KERNEL32.dll", "TerminateProcess", (PVOID) fg_HookTerminateProcess, (PVOID *) &g_fOrgTerminateProcess))
			g_bPatchedTerminateProcess = true;
	}

#ifdef DMibPIR
	fg_MalterlibInitStdLib();
#endif

	//	float Test = 0.0;

	CSystemWindowsMSVC *pLocalSys;
	{
		static_assert(alignof(CSystemWindowsMSVC) == DMibPMemoryCacheLineSize, "Aligment error");
		g_bCreatingSystemDone = true;
		pLocalSys = new(NMib::g_SystemMemory) CSystemWindowsMSVC();
		pLocalSys->f_Init();
	}

	if (pLocalSys)
		pLocalSys->f_InitModule();

}

bool g_bSysDeleted = false;
bool g_bAggregatesDestroyed = false;

void NSys::fg_Process_AllowInvalidExit(bool _bAllow)
{
	g_bAllowInvalidExit = _bAllow;
}

void __cdecl fg_CreateMalterlib()
{
	if (!g_bSystemCreated)
		NSys::fg_CreateSystem();
}

void __cdecl fg_DestroyMalterlib()
{
	if (!g_bValidDestroyModule && !g_bAllowInvalidExit)
	{
		NMib::NContainer::TCVector<CStr> GeneratedDumps;
		NSys::fg_Debug_GenerateCrashDump((CStr::CFormat("Invalid destruction sequence")).f_GetStr(), "", GeneratedDumps, true);
		if (g_fOrgTerminateProcess)
			g_fOrgTerminateProcess(GetCurrentProcess(), 541);
		else
			TerminateProcess(GetCurrentProcess(), 541);
	}
	if (g_bSystemCreated && !g_bSysDeleted)
	{
		fg_GetLocalSys()->f_DestroyThreadSpecific();

		g_bSysDeleted = true;
		fg_GetLocalSys()->f_ExitModule();

		fg_GetLocalSys()->f_Destruct();
		if (!g_bAllowInvalidExit)
		{
			fg_GetLocalSys()->~CSystemWindowsMSVC();
			fg_DestroySystem();
		}
	}
}

void __cdecl fg_DestroyMalterlibAggregates()
{
	if (!g_bValidDestroyModule && !g_bAllowInvalidExit)
	{
		NMib::NContainer::TCVector<CStr> GeneratedDumps;
		NSys::fg_Debug_GenerateCrashDump((CStr::CFormat("Invalid destruction sequence")).f_GetStr(), "", GeneratedDumps, true);
		if (g_fOrgTerminateProcess)
			g_fOrgTerminateProcess(GetCurrentProcess(), 541);
		else
			TerminateProcess(GetCurrentProcess(), 541);
	}
	if (g_bSystemCreated && !g_bSysDeleted)
	{
		g_bAggregatesDestroyed = true;
		fg_GetLocalSys()->f_DestroyThreadSpecific();
		fg_GetLocalSys()->f_DestroyAggregates();
	}
}

void fg_DestroySystem()
{
	NSys::fg_Thread_FreeLocal(gs_ThreadLocalParentThread);

}

void NMib::NSys::fg_HW_GetProcessorInfo(NMib::CProcessorInfo& _Info)
{ // Should probably be moved to a file Malterlib_x86_MSVC.cpp or similar.
	_Info.m_Architecture = NMib::EProcessorArchitecture_Unknown;
	_Info.m_Features = NMib::EProcessorFeature_None;	
#if defined(DArchitecture_arm64)
	_Info.m_Architecture = EProcessorArchitecture_arm64;
	_Info.m_Features |= EProcessorFeature_NEON;
#else
	int CPUInfo[4];
	__cpuid(CPUInfo, 0);

	int MaxInfoType = CPUInfo[0];

	if (MaxInfoType >= 1)
	{
		__cpuid(CPUInfo, 1);

		_Info.m_Features |=
			((CPUInfo[3] & DMibBit(23)) ? EProcessorFeature_MMX : EProcessorFeature_None)
			| ((CPUInfo[3] & DMibBit(25)) ? EProcessorFeature_SSE : EProcessorFeature_None)
			| ((CPUInfo[3] & DMibBit(26)) ? EProcessorFeature_SSE2 : EProcessorFeature_None)
			| ((CPUInfo[2] & DMibBit(0)) ? EProcessorFeature_SSE3 : EProcessorFeature_None)
			| ((CPUInfo[2] & DMibBit(9)) ? EProcessorFeature_SSSE3 : EProcessorFeature_None)
			| ((CPUInfo[2] & DMibBit(19)) ? EProcessorFeature_SSE4_1 : EProcessorFeature_None)
			| ((CPUInfo[2] & DMibBit(20)) ? EProcessorFeature_SSE4_2 : EProcessorFeature_None)
			| ((CPUInfo[2] & DMibBit(31)) ? EProcessorFeature_HyperVisor : EProcessorFeature_None)
		;
	}

	_Info.m_Architecture = (sizeof(void*) == 4) ? EProcessorArchitecture_x86 : EProcessorArchitecture_x86_64;
#endif
}

bool NMib::NSys::fg_HW_GetVirtualMachineInfo(CVirtualMachineInfo& _Info)
{
	_Info.m_bDetected = false;
	_Info.m_pName = nullptr;

	CProcessorInfo ProcInfo;
	fg_HW_GetProcessorInfo(ProcInfo);

	if ((ProcInfo.m_Features & EProcessorFeature_HyperVisor))
		_Info.m_bDetected = true;

	NMib::NPlatform::CWin32_Registry Registry(NMib::NPlatform::CWin32_Registry::ERegRoot_LocalMachine);
	if (Registry.f_ValueExists("HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemManufacturer"))
	{
		CStr SystemManufacturer = Registry.f_Read_Str("HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemManufacturer");

		if (SystemManufacturer.f_StartsWith("VMware"))
		{
			_Info.m_bDetected = true;
			_Info.m_pName = "VMware";
		}
		else if (SystemManufacturer.f_StartsWith("Parallels"))
		{
			_Info.m_bDetected = true;
			_Info.m_pName = "Parallels";
		}
		else if (SystemManufacturer.f_StartsWith("innotek"))
		{
			_Info.m_bDetected = true;
			_Info.m_pName = "VirtualBox";
		}
		else if (SystemManufacturer.f_StartsWith("Microsoft"))
		{
			_Info.m_bDetected = true;
			_Info.m_pName = "VirtualPC";
		}
	}

	if (_Info.m_pName)
		return true;

	if (Registry.f_ValueExists("HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemProductName"))
	{
		CStr SystemProductName = Registry.f_Read_Str("HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemProductName");

		if (SystemProductName.f_StartsWith("VMware"))
		{
			_Info.m_bDetected = true;
			_Info.m_pName = "VMware";
		}
		else if (SystemProductName.f_StartsWith("KVM"))
		{
			_Info.m_bDetected = true;
			_Info.m_pName = "kvm";
		}
		else if (SystemProductName.f_StartsWith("Bochs"))
		{
			_Info.m_bDetected = true;
			_Info.m_pName = "kvm";
		}
	}

	if (_Info.m_bDetected && !_Info.m_pName)
		_Info.m_pName = "unknown";

	return _Info.m_bDetected;
}

NMib::NSys::EDesktopEnvironment NMib::NSys::fg_DesktopEnvironment_Get()
{
	return NMib::NSys::EDesktopEnvironment_Windows;
}

