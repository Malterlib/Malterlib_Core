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
#include <Mib/Cryptography/Hashes/SHA>

using namespace NMib;
using namespace NMib::NMem;
using namespace NMib::NStr;
using namespace NMib::NContainer;
using namespace NMib::NIntrusive;
using namespace NMib::NPtr;
using namespace NMib::NAtomic;
using namespace NMib::NNet;
using namespace NMib::NThread;
using namespace NMib::NMisc;
using namespace NMib::NSystem;
using namespace NMib::NFile;
using namespace NMib::NException;
using namespace NMib::NProcess;
using namespace NMib::NTime;
using namespace NMib::NFunction;
using namespace NMib::NAggregate;

HINSTANCE g_hDllInstance = 0;
bint g_bIsDll = false;

static NAtomic::TCAtomicAggregate<smint> gs_LibraryRefCount = {mint(smint(-1))};
static mint gs_ThreadLocalParentThread = 0xFFFFFFFF;

VOID (WINAPI *g_fOrgExitProcess)
(
  __in  UINT _ExitCode
) = nullptr;


BOOL (WINAPI *g_fOrgTerminateProcess)
(
  __in  HANDLE _hProcess,
  __in  UINT _ExitCode
) = nullptr;


extern NAtomic::TCAtomicAggregate<smint> g_bDoneMalterlibInitAll;

#pragma warning(disable:4344)

#undef int

#include "Malterlib_Core_PlatformImp_MSVC_Registry.h"

CWStr fg_StrToWindows(const CStr &_Str);

template <typename tf_CRet, typename tf_CSrc>
tf_CRet fg_StrToWindows(const tf_CSrc &_Str);

HRESULT fg_PatchIAT(HMODULE _hMod, CHAR *_pImportedModuleName, CHAR *_pImportedProcName, void *_pHookingProc, void **_pOriginalProc);
HRESULT fg_DumpIATs(HMODULE _hMod);
HRESULT fg_PatchDIAT(HMODULE _hMod, CHAR *_pImportedModuleName, CHAR *_pImportedProcName, void *_pHookingProc, void **_pOriginalProc);
enum EInjectDllResult
{
	EInjectDllResult_Failed = 0
	, EInjectDllResult_Done
	, EInjectDllResult_Delayed
};

EInjectDllResult fg_InjectDLL(DWORD _ProcessID, DWORD _ThreadID, const WCHAR *_pDLLName, CStr &_Error);
EInjectDllResult fg_InjectDLL(HANDLE _hProcess, HANDLE _hThread, const WCHAR *_pDLLName, CStr &_Error);
BOOL fg_DejectDLL(DWORD _ProcessID, const WCHAR *_pDLLName, NStr::CStr &_Error);
HMODULE fg_GetProcessImageModule(DWORD _ProcessID);

uint32 fg_Win32_TranslateProcessPriority(EExecutionPriority _Priority);
NTime::CTime fg_Win32_FileTimeToMalterlibTime(FILETIME &_FileTime);
NTime::CTimeSpan fg_Win32_FileTimeToMalterlibTimeSpan(FILETIME &_FileTime);


CStr fg_StrFromWindows(const CWStr &_Str);
CWStr fg_ConvertToWindowsPath(const CStr &_Path, bint _bAddCurrentDir, aint _MaxLen = _MAX_PATH, bool _bTryShorten = true);
template <typename tf_CWindowsStr, typename tf_CRet, typename tf_CSrc>
tf_CRet fg_ConvertToWindowsPath(const tf_CSrc &_Path, bint _bAddCurrentDir, aint _MaxLen = _MAX_PATH, bool _bTryShorten = true);
CStr fg_ConvertFromWindowsPath(const CWStr &_Path);
CStr fg_ConvertFromWindowsPath(const CStr &_Path);
template <typename tf_CWindowsStr, typename tf_CRet, typename tf_CSrc>
tf_CRet fg_ConvertFromWindowsPath(const tf_CSrc &_Path);
HINSTANCE fg_Win32_GetInstance(const void *_pCode);
CWStr fg_ConvertToWindowsPathLocal(const CStr &_Path, bool _bForceLong = false)
{
#ifdef DMibAlwaysUseLongWindowsPaths
	_bForceLong = true;
#endif
	if (_bForceLong)
		return fg_ConvertToWindowsPath(_Path, true, -1);
	else
		return fg_ConvertToWindowsPath(_Path, true, _MAX_PATH, false);
}

template <typename tf_CRet, typename tf_CStr>
tf_CRet fg_ConvertToWindowsPathLocal(const tf_CStr &_Path, bool _bForceLong = false)
{
#ifdef DMibAlwaysUseLongWindowsPaths
	_bForceLong = true;
#endif
	if (_bForceLong)
		return fg_ConvertToWindowsPath<tf_CRet, tf_CRet>(_Path, true, -1);
	else
		return fg_ConvertToWindowsPath<tf_CRet, tf_CRet>(_Path, true, _MAX_PATH, false);
}

extern void fg_GenerateExcetionHandler(void *_pData, LONG (*_fCallback)(struct _EXCEPTION_POINTERS *_pExceptionInfo, void *_pData));

#ifdef DArchitecture_x64
#define DArchX86_64
#endif

#ifdef DArchX86_64
#define DDefaultCallingConv 
#else
#define DDefaultCallingConv __cdecl
#endif

#ifdef DArchX86_64
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

CFStr256 fg_Win32_GetLastErrorStr(uint32 _Error = 0);

#define DMibGuid "213391DE-87DB-4c39-9095-A31E6F14625B"

static SYSTEM_INFO gs_SysInfo;         // useful information about the system
extern HMODULE g_hNtDll;
extern HMODULE g_hKernel32;
extern HMODULE g_hAdvAPI32;



namespace NLocal
{

	NTSTATUS (NTAPI *g_fNtQueryInformationThread)(
		IN HANDLE _ThreadHandle,
		IN THREADINFOCLASS _ThreadInformationClass,
		OUT PVOID _ThreadInformation,
		IN ULONG _ThreadInformationLength,
		OUT PULONG _ReturnLength OPTIONAL
		) = nullptr;
	BOOL (WINAPI *g_fGetLogicalProcessorInformation)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION _pBuffer, PDWORD _pReturnLength)  = nullptr;
	VOID (WINAPI *g_fRtlAcquirePebLock)(void) = nullptr;
	VOID (WINAPI *g_fRtlReleasePebLock)(void) = nullptr;
	ULONG  (WINAPI *g_fRtlFindClearBitsAndSet)(IN PRTL_BITMAP  _pBitMapHeader, IN ULONG  _NumberToFind, IN ULONG  _HintIndex) = nullptr; 
	VOID (WINAPI *g_fRtlClearBits)(IN PRTL_BITMAP  _pBitMapHeader, IN ULONG  _StartingIndex, IN ULONG  _NumberToClear) = nullptr;

	void (WINAPI *g_fGetNativeSystemInfo)(__out LPSYSTEM_INFO _pSystemInfo) = nullptr;
	PVOID (WINAPI *g_fAddVectoredExceptionHandler)(ULONG _First, PVECTORED_EXCEPTION_HANDLER _pHandler) = nullptr;
	ULONG (WINAPI *g_fRemoveVectoredExceptionHandler)(PVOID _pHandler) = nullptr;
	
	
	SIZE_T (WINAPI *g_fLargePageMinimum)();

	char *(WINAPI *g_fWineGetVersion)()  = nullptr;

	void *g_pKiUserApcDispatcher = nullptr;
	void *g_pKiUserCallbackDispatcher = nullptr;

	// Numa functions
	LPVOID (WINAPI *g_fVirtualAllocExNuma)(HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD  flAllocationType, DWORD  flProtect, DWORD  nndPreferred);
	BOOL (WINAPI *g_fGetNumaNodeProcessorMaskEx)(USHORT Node, PGROUP_AFFINITY ProcessorMask);
	BOOL (WINAPI *g_fSetThreadGroupAffinity)(HANDLE hThread, const GROUP_AFFINITY *GroupAffinity, PGROUP_AFFINITY PreviousGroupAffinity);

	#define PROCESS_CALLBACK_FILTER_ENABLED     0x1
	BOOL (WINAPI *g_fSetProcessUserModeExceptionPolicy)(DWORD dwFlags);
	BOOL (WINAPI *g_fGetProcessUserModeExceptionPolicy)(LPDWORD lpFlags);

	DWORD (WINAPI *g_fWTSGetActiveConsoleSessionId)();

	BOOL (WINAPI *g_fCreateProcessWithTokenW)(HANDLE hToken, DWORD dwLogonFlags, LPCWSTR lpApplicationName, LPWSTR lpCommandLine, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation );

	BOOLEAN (APIENTRY *g_fCreateSymbolicLinkW)(LPCWSTR lpSymlinkFileName, LPCWSTR lpTargetFileName, DWORD dwFlags);

	BOOL (WINAPI *g_fCreateHardLinkW)(LPCWSTR lpFileName, LPCWSTR lpExistingFileName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
	
	OSVERSIONINFOEX g_VersionInfo = {0};

	BOOL (WINAPI *g_fWow64DisableWow64FsRedirection)(PVOID * OldValue);
	BOOL (WINAPI *g_fWow64RevertWow64FsRedirection)(PVOID OlValue);

	NTSTATUS (NTAPI *g_fNtSetInformationProcess)(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass, PVOID pProcessInformation, ULONG ProcessInformationLength);

	BOOL (WINAPI *g_fSetProcessInformation)(HANDLE hProcess, PROCESS_INFORMATION_CLASS ProcessInformationClass, LPVOID ProcessInformation, DWORD ProcessInformationSize);

	BOOL (WINAPI *g_fCancelSynchronousIo)(HANDLE hThread);
	BOOL (WINAPI *g_fCancelIoEx)(HANDLE hFile, LPOVERLAPPED lpOverlapped);

	NTSTATUS (WINAPI *g_fNtQuerySystemInformation)(DWORD SystemInformationClass, PVOID SystemInformation, DWORD SystemInformationLength, PDWORD ReturnLength);

	NTSTATUS (WINAPI *g_fNtGetNextThread)(HANDLE ProcessHandle, HANDLE ThreadHandle, ACCESS_MASK DesiredAccess, ULONG HandleAttributes, ULONG Flags, PHANDLE NewThreadHandle);

	DWORD (WINAPI *g_fGetThreadId)(HANDLE Thread);

	NTSTATUS (WINAPI *g_fNtQueryInformationProcess)(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength);

	NTSTATUS (WINAPI *g_fLdrDisableThreadCalloutsForDll)(IN PVOID BaseAddress);
}



static inline_small class CSystemWindowsMSVC *fg_GetLocalSys();
//static inline_small class CSharedSystemWindowsMSVC &GetLocalSharedSys();

void fg_DestroySystem();
void __cdecl fg_DestroyMalterlib();
void __cdecl fg_DestroyMalterlibAggregates();
void __cdecl fg_CreateMalterlib();



inline_small CUndocumentedTEB *fg_GetTEB()
{
#if defined(_M_X64)
	return (CUndocumentedTEB *)__readgsqword(6*sizeof(void*));
#else
	return (CUndocumentedTEB *)__readfsdword(6*sizeof(void*));
#endif

}


template <typename tf_CType>
inline_small tf_CType fg_GetTebData(mint _iIndex)
{
#if defined(_M_X64)
	return (tf_CType)__readgsqword(_iIndex);
#else
	return (tf_CType)__readfsdword(_iIndex);
#endif
}


UndocumentedPEB *fg_GetPEB(CUndocumentedTEB *_pTeb)
{
	return _pTeb->Peb;
}


class CWin32File
{
public:
	virtual ~CWin32File()
	{
	}
	void *m_pFile;
	NMib::NFile::EFileOpen m_Flags;
	virtual CWStrPtr f_GetName() const pure;
	virtual void f_Delete() pure;
	virtual bool f_IsNonTracked() pure;
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
	virtual bool f_IsNonTracked()
	{
		return !t_CAllocator::mc_Reporting;
	}
};

namespace
{
	inline_small bint fg_IsGoodStackPtr(void *_pAddr, mint _Len, mint _StackStart, mint _StackEnd)
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


#include "Malterlib_Core_PlatformImp_MSVC_StackTrace.cpp"
#include "Malterlib_Core_PlatformImp_MSVC_LocalSys.cpp"
#include "Malterlib_Core_PlatformImp_MSVC_NTSpecific.cpp"
#include "Malterlib_Core_PlatformImp_MSVC_LaunchProcess.cpp"
#include "Malterlib_Core_PlatformImp_MSVC_CPUUsageMonitor.cpp"
#include "Malterlib_Core_PlatformImp_MSVC_StdInReader.cpp"

// Note: These needs to be nade exactly like this to be compatible with old version of library (when Malterlib was named Ids)
#ifdef DArchitecture_x64
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

void* NSys::fg_LoadLibrary(CFStr256 const& _Library)
{
	if (!_Library.f_IsEmpty())
	{
		CFWStr256 LibPath = fg_ConvertToWindowsPath<CFWStr256, CFWStr256>(_Library, false);

		LPTOP_LEVEL_EXCEPTION_FILTER pFilter = SetUnhandledExceptionFilter(nullptr);
		SetUnhandledExceptionFilter(pFilter);

		void *pRet = LoadLibraryW(LibPath.f_GetStr());
		if (pRet)
		{
			void (__cdecl *pMalterlibLoadLibraryExternal)();
			pMalterlibLoadLibraryExternal = (void (__cdecl *)())GetProcAddress((HMODULE)pRet, "IdsLoadLibraryExternal");
			if (pMalterlibLoadLibraryExternal)
				pMalterlibLoadLibraryExternal();
		}

		LPTOP_LEVEL_EXCEPTION_FILTER pFilterNew = SetUnhandledExceptionFilter(nullptr);
		if (pFilterNew != pFilter)
		{
			DMibDTrace("---------------------------------------- Restored unhandled exception filter after DLL load\n", 0);
		}
		SetUnhandledExceptionFilter(pFilter);
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
		CWStr LibPath = fg_ConvertToWindowsPath(_Library, false);

		LPTOP_LEVEL_EXCEPTION_FILTER pFilter = SetUnhandledExceptionFilter(nullptr);
		SetUnhandledExceptionFilter(pFilter);

		void *pRet = LoadLibraryW(LibPath.f_GetStr());
		if (pRet)
		{
			void (__cdecl *pMalterlibLoadLibraryExternal)();
			pMalterlibLoadLibraryExternal = (void (__cdecl *)())GetProcAddress((HMODULE)pRet, "IdsLoadLibraryExternal");
			if (pMalterlibLoadLibraryExternal)
				pMalterlibLoadLibraryExternal();
		}

		LPTOP_LEVEL_EXCEPTION_FILTER pFilterNew = SetUnhandledExceptionFilter(nullptr);
		if (pFilterNew != pFilter)
		{
			DMibDTrace("---------------------------------------- Restored unhandled exception filter after DLL load\n", 0);
		}
		SetUnhandledExceptionFilter(pFilter);
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
		CWStrNonTracked LibPath = fg_ConvertToWindowsPath<CWStrNonTracked, CWStrNonTracked>(_Library, false);

		LPTOP_LEVEL_EXCEPTION_FILTER pFilter = SetUnhandledExceptionFilter(nullptr);
		SetUnhandledExceptionFilter(pFilter);

		void *pRet = LoadLibraryW(LibPath.f_GetStr());
		if (pRet)
		{
			void (__cdecl *pMalterlibLoadLibraryExternal)();
			pMalterlibLoadLibraryExternal = (void (__cdecl *)())GetProcAddress((HMODULE)pRet, "IdsLoadLibraryExternal");
			if (pMalterlibLoadLibraryExternal)
				pMalterlibLoadLibraryExternal();
		}

		LPTOP_LEVEL_EXCEPTION_FILTER pFilterNew = SetUnhandledExceptionFilter(nullptr);
		if (pFilterNew != pFilter)
		{
			DMibDTrace("---------------------------------------- Restored unhandled exception filter after DLL load\n", 0);
		}
		SetUnhandledExceptionFilter(pFilter);
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
	LPTOP_LEVEL_EXCEPTION_FILTER pFilter = SetUnhandledExceptionFilter(nullptr);
	SetUnhandledExceptionFilter(pFilter);

	void (__cdecl *pMalterlibFreeLibraryExternal)();
	pMalterlibFreeLibraryExternal = (void (__cdecl *)())GetProcAddress((HMODULE)_pModule, "IdsFreeLibraryExternal");
	if (pMalterlibFreeLibraryExternal)
		pMalterlibFreeLibraryExternal();
	FreeLibrary((HMODULE)_pModule);

	LPTOP_LEVEL_EXCEPTION_FILTER pFilterNew = SetUnhandledExceptionFilter(nullptr);
	if (pFilterNew != pFilter)
	{
		DMibDTrace("--------------------------------------- Restored unhandled exception filter after DLL unload\n", 0);
	}
	SetUnhandledExceptionFilter(pFilter);

}

void* NSys::fg_GetLibrarySymbol(void* _pModule, char const* _pSymbol)
{
	return GetProcAddress((HMODULE)_pModule, _pSymbol);
}

void* NSys::fg_GetExeData(char const* _pSegment, char const* _pSection, unsigned long long& _nDataBytes)
{
	return nullptr;
}

CFStr256 fg_Win32_GetLastErrorStr(uint32 _Error)
{
	if (!_Error)
		_Error = GetLastError();
	LPVOID lpMsgBuf;
	if (FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		_Error,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		nullptr 
	))
	{
		// Process any inserts in lpMsgBuf.
		// ...
		// Display the string.

		CFStr256 LastError;
		if (lpMsgBuf)
		{
			LastError = (LPTSTR)lpMsgBuf;
			LocalFree(lpMsgBuf);
		}

		return LastError.f_Trim();
	}
	else
	{
		return CFStr256::CFormat("0x{nfh,sj8,sf0}") << _Error;

	}
}

bint NSys::fg_System_BeingDebugged()
{
	UndocumentedPEB *pPeb = fg_GetPEB(fg_GetTEB());
	return pPeb->BeingDebugged != false;
}

void NSys::fg_Debug_PauseDeadlockDetector()
{
	fg_GetLocalSys()->f_Debug_PauseDeadlockDetector();
}

void NSys::fg_Debug_ResumeDeadlockDetector()
{
	fg_GetLocalSys()->f_Debug_ResumeDeadlockDetector();
}

bint NSys::fg_Debug_IsDeadlocked()
{
	return fg_GetLocalSys()->f_Debug_IsDeadlocked();
}

inline_never bint NSys::fg_Compiler_AlwaysFalse()
{
	return false;
}

bint NSys::fg_Compiler_MakeActive(const void *_pReference)
{
	DMibTraceSafe("", mint(_pReference));
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

void NSys::fg_Debug_StartDeadlockDetector(fp64 _Timeout)
{
	fg_GetLocalSys()->f_Debug_StartDeadlockDetector(_Timeout);
}

void NSys::fg_Debug_NotDeadlocked()
{
	fg_GetLocalSys()->f_Debug_NotDeadlocked();
}

void NSys::fg_Debug_StopDeadlockDetector()
{
	fg_GetLocalSys()->f_Debug_StopDeadlockDetector();
}

NMib::EDebugCheckFailureAction NSys::fg_Debug_ReportContractFailure(const ch8 *_pFileName, int32 _Line, void *_pCodePointer, const NMib::NStr::CStrNonTracked &_ErrorMessage)
{
	DMibDeadlockDetectorPause;

	NMib::EDebugCheckFailureAction Ret;
	auto fl_DisplayMessage 
		= [&] (CThreadObjectNonTracked *_pThread) -> aint
		{
		#if defined(DDebug) && 0
			// _CrtDbgReportW uses memory manager so it's not optimal to use this
			UndocumentedPEB *pPeb = fg_GetPEB(fg_GetTEB());
			if (pPeb->BeingDebugged)
			{
				Ret = EDebugContractFailureAction_NotHandled;
				return 0;
			}

			auto Module = NMib::NFile::CFile::fs_GetFile(NSys::NFile::fg_GetModulePathNonTracked(_pCodePointer));
			int LocalRet = _CrtDbgReportW(_CRT_ASSERT, fg_StrToWindows<CWStrNonTracked>(CStrNonTracked(_pFileName)), _Line, fg_StrToWindows<CWStrNonTracked>(Module), str_utf16("%s"), fg_StrToWindows<CWStrNonTracked>(_ErrorMessage).f_GetStr());
			if (LocalRet == 1)
			{
				Ret = EDebugContractFailureAction_Break;
				return 0;
			}

			Ret = EDebugContractFailureAction_Continue;
		#else
			UndocumentedPEB *pPeb = fg_GetPEB(fg_GetTEB());
			if (pPeb->BeingDebugged)
			{
				Ret = EDebugContractFailureAction_NotHandled;
				return 0;
			}

			if (fg_GetSys()->f_GetRunningAsService())
			{
				Ret = EDebugContractFailureAction_Break;
				return 0;
			}

			auto Module = NMib::NFile::CFile::fs_GetFile(NSys::NFile::fg_GetModulePathNonTracked(_pCodePointer));

			CStrNonTracked MessageText;
			MessageText += "Assertion failure:\r\n\r\n";
			MessageText += _ErrorMessage;
			MessageText += "\r\n";
			MessageText += CStrNonTracked::CFormat("In module: {}. At:\r\n") << Module;
			MessageText += CStrNonTracked::CFormat("{}({})\r\n\r\n") << _pFileName << _Line;
			MessageText += "Decide how to proceed.";

			switch (MessageBoxW(nullptr, fg_StrToWindows<CWStrNonTracked>(MessageText), str_utf16("Assertion failure"), MB_ABORTRETRYIGNORE))
			{
			case IDABORT:
				Ret = EDebugContractFailureAction_Abort;
				return 0;
			case IDRETRY:
				Ret = EDebugContractFailureAction_Break;
				return 0;
			}
			Ret = EDebugContractFailureAction_Continue;
		#endif

			return 0;
		}
	;

	BOOL bDllHeld;
	AuxUlibIsDLLSynchronizationHeld(&bDllHeld);

	if (bDllHeld || g_bDoneMalterlibInitAll.f_Load() < 3)
	{
		fl_DisplayMessage(nullptr);
	}
	else
	{
		TCUniquePointer<CThreadObjectNonTracked, NMib::NMem::CAllocator_NonTrackedHeap> pThread = CThreadObjectNonTracked::fs_StartThread(fl_DisplayMessage, "Report assert display message thread");
	}

	return Ret;
}

namespace
{
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

	CStr fg_ExpandEnvironmentVars(CStr const &_Path)
	{
		CStr Path = _Path.f_ReplaceChar('\\', '/');
		CStr RetPath;
		while (!Path.f_IsEmpty())
		{
			CStr SubPath = fg_GetStrSep(Path, "/");
			if (SubPath.f_IsEmpty())
			{
				RetPath += "/";
			}
			else
			{
				if (SubPath[0] == '%')
				{
					SubPath = NSys::fg_Process_GetEnvironmentVariable(CStr(SubPath.f_Replace("%", ""))
						);
				}
				if (!RetPath.f_IsEmpty() && RetPath[RetPath.f_GetLen() - 1] == '/')
					RetPath += SubPath;
				else
					fg_AddStrSep(RetPath, SubPath, "/");
			}
		}
		return RetPath;
	}

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


	class CLauncherThread : public NThread::CThread
	{
	public:
		CStr m_Operation;
		CStr m_File;
		CStr m_Params;
		CStr m_Directory;
		int m_ShowCmd;
		DWORD *m_pProgramExitCode;
		HANDLE *m_pProcessHandle;

		HRESULT m_Result;

		CLauncherThread()
		{
			m_Result = S_FALSE;
		}

		virtual aint f_Main()
		{
			CStr Program;
			CStr Params;
			CWStr ParamsW = fg_StrToWindows(m_Params);
			bint bTryCreateProcess = false;
			aint MaxLen = _MAX_PATH;
			CWStr FileW = fg_ConvertToWindowsPath(m_File, false, MaxLen);
			if ((m_Operation == "open" || m_Operation == "") && NFile::CFile::fs_GetExtension(m_File).f_CmpNoCase("exe") == 0)
			{
				Program = m_File;
				Params = m_Params;
				bTryCreateProcess = true;
			}
			bint bIsURL = false;

				
			try
			{
				aint iFind = m_File.f_FindChar(':');
				if (iFind > 1)
				{
					bIsURL = true;
					FileW = fg_StrToWindows(m_File);
					CStr URLHandler = m_File.f_Left(iFind);
					NRuntimeMSVC::CWin32_Registry Registry(NRuntimeMSVC::CWin32_Registry::ERegRoot_Classes);

					if ((m_Operation == "open" || m_Operation == "") && Registry.f_KeyExists(URLHandler))
					{
						if (Registry.f_ValueExists(URLHandler, "URL Protocol"))
						{
							CStr Command = URLHandler + "\\shell\\open\\command";
							if (Registry.f_KeyExists(Command))
							{
								CStr LocalPath = Registry.f_Read_Str(Command, "");

								Program = fg_GetWinPathSepEscaped(LocalPath, " ");
								Program = fg_ExpandEnvironmentVars(Program);

								if (NFile::CFile::fs_FileExists(Program) && LocalPath.f_Find("%1") >= 0)
								{
									Params = LocalPath.f_Replace("%1", fg_StrToWindows(m_File));
									if (Params.f_FindChar('%') < 0)
										bTryCreateProcess = true;
								}
							}

						}
					}
				}
					
			}
			catch (const NException::CException &)
			{
			}
				
			if (!bTryCreateProcess && !bIsURL)
			{
				try
				{
					NRuntimeMSVC::CWin32_Registry Registry(NRuntimeMSVC::CWin32_Registry::ERegRoot_Classes);

					CStr ExtClass = "." + NFile::CFile::fs_GetExtension(m_File);

					if (Registry.f_KeyExists(ExtClass))
					{
						CStr Class = Registry.f_Read_Str(ExtClass, "");

						CStr Command = Class + "\\shell\\" + m_Operation + "\\command";
						CStr DDECommand = Class + "\\shell\\" + m_Operation + "\\ddeexec";
						if (Registry.f_KeyExists(DDECommand))
						{
							MaxLen = 218; // Hack for making XLS documents open!!!
							FileW = fg_ConvertToWindowsPath(m_File, false, MaxLen);
						}
						else if (Registry.f_KeyExists(Command))
						{
							CStr LocalPath = Registry.f_Read_Str(Command, "");

							Program = fg_GetWinPathSepEscaped(LocalPath, " ");
							Program = fg_ExpandEnvironmentVars(Program);

							if (NFile::CFile::fs_FileExists(Program) && LocalPath.f_Find("%1") >= 0)
							{
								Params = LocalPath.f_Replace("%1", fg_ConvertToWindowsPath(m_File, false));
								if (Params.f_FindChar('%') < 0)
									bTryCreateProcess = true;
							}
						}
					}
				}
				catch (const NException::CException &)
				{
				}
			}

			if (bTryCreateProcess && (FileW.f_GetLen() > MaxLen || ParamsW.f_GetLen() >= 2048))
			{
				PROCESS_INFORMATION ProcessInfo;
				STARTUPINFOW StartupInfo;
				fg_MemClear(ProcessInfo);
				fg_MemClear(StartupInfo);

				StartupInfo.cb = sizeof(STARTUPINFOW);
				StartupInfo.wShowWindow = m_ShowCmd;
				StartupInfo.dwFlags = STARTF_USESHOWWINDOW;

				// Launch the child process.
				CWStr ParamsW = fg_StrToWindows("\"" + fg_ConvertToWindowsPath(Program, true) + "\" " + Params);
//					CWStr ParamsW = fg_StrToWindows(Params);
				CWStr DirectoryW = fg_ConvertToWindowsPath(m_Directory, true);
				{
					if (::CreateProcessW(
						fg_ConvertToWindowsPath(Program, true),
						ParamsW.f_GetStrUniqueWritable(),
						nullptr, nullptr,
						false,
						CREATE_UNICODE_ENVIRONMENT,
						nullptr, 
						(!DirectoryW.f_IsEmpty() ? DirectoryW.f_GetStr() : nullptr) ,
						&StartupInfo,
						&ProcessInfo))
					{

						CloseHandle(ProcessInfo.hThread);
						if (m_pProcessHandle)
							*m_pProcessHandle = ProcessInfo.hProcess;

						if (m_pProgramExitCode)
						{
							WaitForSingleObject(ProcessInfo.hProcess, INFINITE);
							GetExitCodeProcess(ProcessInfo.hProcess, m_pProgramExitCode);
						}
						if (!m_pProcessHandle)
							CloseHandle(ProcessInfo.hProcess);

						m_Result = S_OK;

						return 0;
					}
					else
					{
						DMibDTrace("CreateProcessW: {}\r\n", fg_Win32_GetLastErrorStr(GetLastError()));
					}
				}
			}
			{
				SHELLEXECUTEINFOW ExecInfo;

				CWStr Operation = fg_StrToWindows(m_Operation);
				CWStr File = FileW;
				CWStr Params = fg_StrToWindows(m_Params);
				CWStr Directory = fg_ConvertToWindowsPath(m_Directory, false);

				//DMibDTrace("FileLen: {}\n", File.f_GetLen());

				fg_MemClear(ExecInfo);
				ExecInfo.cbSize = sizeof(ExecInfo);
				ExecInfo.lpVerb = !Operation.f_IsEmpty() ? Operation.f_GetStr() : nullptr;
				ExecInfo.lpFile = !File.f_IsEmpty() ? File.f_GetStr() : nullptr;
				ExecInfo.hwnd = nullptr;
				ExecInfo.nShow = m_ShowCmd;
				ExecInfo.lpParameters = !Params.f_IsEmpty() ? Params.f_GetStr() : nullptr;
				ExecInfo.lpDirectory = !Directory.f_IsEmpty() ? Directory.f_GetStr() : nullptr;
				ExecInfo.fMask = SEE_MASK_UNICODE | SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC; // SEE_MASK_ASYNCOK
				if (m_pProgramExitCode || m_pProcessHandle)
					ExecInfo.fMask |= SEE_MASK_NOCLOSEPROCESS;

				if (!ShellExecuteEx(&ExecInfo))
					m_Result = GetLastError();
				else
				{
					m_Result = S_OK;
					if (m_pProcessHandle)
						*m_pProcessHandle = ExecInfo.hProcess;

					if (m_pProgramExitCode)
					{
						WaitForSingleObject(ExecInfo.hProcess, INFINITE);
						GetExitCodeProcess(ExecInfo.hProcess, m_pProgramExitCode);
						if (!m_pProcessHandle)
							CloseHandle(ExecInfo.hProcess);
					}
				}

				return 0;
			}
		}

		virtual NStr::CStr f_GetThreadName()
		{
			return "AOShellExecuteThread";
		}
	};
}

bint fg_Win32_ThreadedShellExecute(CStr _Operation, CStr _File, CStr _Params, CStr _Directory, int _ShowCmd, DWORD *_pProgramExitCode, HANDLE *_pProcessHandle, TCFunction<void (NThread::CThread const &_Thread)> const &_WaitLoop)
{
	DMibDeadlockDetectorPause;

	if (_pProgramExitCode)
		*_pProgramExitCode = 255;
	CLauncherThread LauncherThread;
	LauncherThread.m_Operation = _Operation;
	LauncherThread.m_File = _File;
	LauncherThread.m_Params = _Params;
	LauncherThread.m_Directory = _Directory;
	LauncherThread.m_ShowCmd = _ShowCmd;
	LauncherThread.m_pProgramExitCode = _pProgramExitCode;
	LauncherThread.m_pProcessHandle = _pProcessHandle;
	LauncherThread.f_Start();

	if (_WaitLoop)
		_WaitLoop(LauncherThread);

	LauncherThread.f_Stop();

	if (LauncherThread.m_Result)
	{
		::SetLastError(LauncherThread.m_Result);

		return false;
	}
	return true;
}

void NSys::fg_Debug_DiffStrings(const NMib::NStr::CStr &_FirstStr, const NMib::NStr::CStr &_SecondStr, const NMib::NStr::CStr &_FirstName, const NMib::NStr::CStr &_SecondName)
{
#if 1

	try
	{
		NMib::NStr::CStr ExecutablePath = "c:/Program Files/Araxis/Araxis Merge/Merge.exe";
		if (!NMib::NFile::CFile::fs_FileExists(ExecutablePath))
		{
			try
			{
				NRuntimeMSVC::CWin32_Registry Reg(NRuntimeMSVC::CWin32_Registry::ERegRoot_LocalMachine, "Software\\Thingamahoochie\\WinMerge");
				ExecutablePath = Reg.f_Read_Str("", "Executable", "");
			}
			catch (NException::CException)
			{
			}
		}

		if (!NMib::NFile::CFile::fs_FileExists(ExecutablePath))
		{
			try
			{
				NRuntimeMSVC::CWin32_Registry Reg(NRuntimeMSVC::CWin32_Registry::ERegRoot_LocalMachine, "Software\\Wow6432Node\\Thingamahoochie\\WinMerge");
				ExecutablePath = Reg.f_Read_Str("", "Executable", "");
			}
			catch (NException::CException)
			{
			}
		}
		if (!NMib::NFile::CFile::fs_FileExists(ExecutablePath))
		{
			try
			{
				NRuntimeMSVC::CWin32_Registry Reg(NRuntimeMSVC::CWin32_Registry::ERegRoot_CurrentUser, "Software\\Wow6432Node\\Thingamahoochie\\WinMerge");
				ExecutablePath = Reg.f_Read_Str("", "Executable", "");
			}
			catch (NException::CException)
			{
			}
		}
		if (!NMib::NFile::CFile::fs_FileExists(ExecutablePath))
		{
			try
			{
				NRuntimeMSVC::CWin32_Registry Reg(NRuntimeMSVC::CWin32_Registry::ERegRoot_CurrentUser, "Software\\Thingamahoochie\\WinMerge");
				ExecutablePath = Reg.f_Read_Str("", "Executable", "");
			}
			catch (NException::CException)
			{
			}
		}
		if (!NMib::NFile::CFile::fs_FileExists(ExecutablePath))
		{
			ExecutablePath = "C:/Program Files/Perforce/p4merge.exe";
		}
		{
			uint32 FileHash0 = CStr(_FirstStr).f_Hash();
			uint32 FileHash1 = CStr(_SecondStr).f_Hash();
			NMib::NStr::CStr TempPath = NSys::NFile::fg_GetTemporaryDirectory();

			NMib::NFile::CFile::fs_CreateDirectory(TempPath);

			CStr FileName0 = TempPath + CStr(CStr::CFormat("/MalterlibTempDiff_{nfh,sf0,sj8}.txt") << FileHash0);
			CStr FileName1 = TempPath + CStr(CStr::CFormat("/MalterlibTempDiff_{nfh,sf0,sj8}.txt") << FileHash1);

			NMib::NFile::CFile::fs_WriteStringToFile(FileName0, _FirstStr);
			NMib::NFile::CFile::fs_WriteStringToFile(FileName1, _SecondStr);

			if (NMib::NFile::CFile::fs_FileExists(ExecutablePath))
			{
				fg_Win32_ThreadedShellExecute
					(
						"open"
						, ExecutablePath
						, CStr::CFormat("\"{}\" \"{}\"") << FileName0 << FileName1
						, NMib::NFile::CFile::fs_GetPath(ExecutablePath)
						, SW_SHOW
						, nullptr
						, nullptr
						, TCFunction<void (NThread::CThread const &_Thread)>()
					)
				;
			}
		}
	}
	catch (NException::CException)
	{
	}
#endif
}

static mint fsg_GetStackTrace(mint *_pStack, mint _nMaxDepth, mint _StackFrame)
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

mint NSys::fg_System_GetStackTrace(CMibCodeAddress *_pStack, mint _nMaxDepth)
{
#ifdef DArchX86_64

	if (NLocal::g_VersionInfo.dwMajorVersion < 0x06)
		_nMaxDepth = fg_Min(_nMaxDepth, 63);

	return RtlCaptureStackBackTrace(0, _nMaxDepth, (void **)_pStack, nullptr);
#else
	
	try
	{
		
		mint RegEBP = (mint)_ReturnAddress();
#ifdef DArchX86_64
		RegEBP = fg_MalterlibGetFramePtr_X86_64();
#else
		__asm
		{
			mov RegEBP, ebp
		}
#endif

		RegEBP = *((mint *)(RegEBP));
		return fsg_GetStackTrace((mint *)_pStack, _nMaxDepth, RegEBP);
	}
	catch(...)
	{
	}
	return 0;
#endif
}

CMibCodeAddress NSys::fg_System_GetStackTrace(aint _iDepth)
{
#ifdef DArchX86_64
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
#ifdef DArchX86_64
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

bool NSys::fg_Debug_AquireStackTraceInfo(CStackTraceInfo &_oInfo, CMibCodeAddress _Address, bool _bCanAllocNonTracked)
{
	if (_bCanAllocNonTracked)
	{
		auto pInfo = NSys::fg_Debug_AquireStackTraceInfo(_Address);
		if (!pInfo)
			return false;
		_oInfo = *pInfo;
		_oInfo.m_pContext = pInfo;
		return true;
	}

	return false;
}


CStackTraceInfo *NSys::fg_Debug_AquireStackTraceInfo(CMibCodeAddress _Address)
{
	return fg_GetLocalSys()->f_AquireStackTraceInfo(_Address);
}

void NSys::fg_Debug_ReleaseStackTraceInfo(CStackTraceInfo *_pInfo)
{
	if (_pInfo->m_pContext)
		return fg_GetLocalSys()->f_ReleaseStackTraceInfo((CStackTraceInfo *)_pInfo->m_pContext);
	else
		return fg_GetLocalSys()->f_ReleaseStackTraceInfo(_pInfo);
}

void NSys::fg_Debug_EnableCrashDumpCaches()
{

	return fg_GetLocalSys()->f_EnableCrashDumpCaches();
}

void NSys::fg_Debug_BlockingMessage(NMib::NStr::CStr const &_Heading, NMib::NStr::CStr const &_Message)
{
	class CMessageBoxThread : public NMib::NThread::CThread
	{
	public:
		virtual ch8 const *f_GetThreadNameRaw()
		{
			return "Malterlib_MessageBoxThread";
		}
		virtual CStr f_GetThreadName()
		{
			return "Malterlib_MessageBoxThread";
		}
		CStrNonTracked m_MessageBoxText;
		CStrNonTracked m_MessageBoxHeading;
		uint32 m_MessageBoxFlags;
		uint32 m_bRet;

		aint f_Main()
		{
			m_bRet = ::MessageBoxW(nullptr, fg_StrToWindows<CWStrNonTracked>(m_MessageBoxText), fg_StrToWindows<CWStrNonTracked>(m_MessageBoxHeading), m_MessageBoxFlags);
			return 0;
		}
		void f_Run()
		{
			BOOL bDllHeld;
			AuxUlibIsDLLSynchronizationHeld(&bDllHeld);

			if (bDllHeld  || g_bDoneMalterlibInitAll.f_Load() < 3)
				f_Main();
			else
			{
				f_Start();
				f_Stop();
			}
		}
	};

	CMessageBoxThread MessageBox;

	MessageBox.m_MessageBoxHeading = _Heading;
	MessageBox.m_MessageBoxText = _Message;
	MessageBox.m_MessageBoxFlags = MB_OK;
	MessageBox.m_bRet = 0;
	MessageBox.f_Run();

}

void NSys::fg_Debug_GenerateCrashDump(const NMib::NStr::CStr &_Message, const NMib::NStr::CStr &_ExtraLog, TCVector<NMib::NStr::CStr> &_GeneratedLogs, bint _bDisplayGUI)
{
	return fg_GetLocalSys()->f_GenerateCrashDump(_Message, _ExtraLog, _GeneratedLogs, _bDisplayGUI);
}

void NSys::fg_Debug_GenerateMemoryDump(NMib::NContainer::TCVector<void*, NMib::NMem::CAllocator_NonTrackedHeap> const& _Locations, NMib::NContainer::TCVector<mint, NMib::NMem::CAllocator_NonTrackedHeap> const& _Sizes)
{
	fg_GetLocalSys()->f_GenerateMemoryDump(_Locations, _Sizes);
}

void NSys::fg_Debug_SetCrashDumpUserNotifyFunction(NSys::FCrashDumpUserNotify *_pCrashDumpUserNotify)
{
	return fg_GetLocalSys()->f_SetCrashDumpUserNotifyFunction(_pCrashDumpUserNotify);
}

void NSys::fg_Debug_SetCrashDumpUserNotifyFormats(NMib::NStr::CStrNonTracked const &_CustomMessage, NMib::NStr::CStrNonTracked const &_CanContinueMessage, NMib::NStr::CStrNonTracked const &_NoContinueMessage)
{
	return fg_GetLocalSys()->f_SetCrashDumpUserNotifyFormats( _CustomMessage, _CanContinueMessage, _NoContinueMessage );
}

void NSys::fg_Debug_SetDeadlockNotifyFunction(FDeadlockUserNotify *_pCrashDumpUserNotify)
{
	return fg_GetLocalSys()->f_Debug_SetDeadlockUserNotifyFunction(_pCrashDumpUserNotify);
}

void NSys::fg_Debug_UndecorateName(const ch8 *_pName, NMib::NStr::CStr &_Destination)
{
	return fg_GetLocalSys()->f_UndecorateName(_pName, _Destination);
}

void NSys::fg_Debug_UndecorateName(const ch8 *_pName, NMib::NStr::CStrNonTracked &_Destination)
{
	return fg_GetLocalSys()->f_UndecorateName(_pName, _Destination);
}

void NSys::fg_Debug_UndecorateName(const ch8 *_pName, ch8 *_pDestination, mint _MaxLen)
{
	return fg_GetLocalSys()->f_UndecorateName(_pName, _pDestination, _MaxLen);
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


namespace
{
	class CForegroundColour
	{
		HANDLE mp_hConsole;
		WORD mp_OldCharAttr;
		WORD fp_GetCharAttr() const
		{
			CONSOLE_SCREEN_BUFFER_INFO info;
			GetConsoleScreenBufferInfo(mp_hConsole,&info);
			return info.wAttributes;
		}
	public:
		CForegroundColour(HANDLE _Console,NSys::EColor _Color)
			: mp_hConsole(_Console)
			, mp_OldCharAttr(fp_GetCharAttr())
		{
			WORD NewCharAttr = mp_OldCharAttr;
			switch(_Color)
			{
			case NSys::EColor_Default: break;
			case NSys::EColor_Green: 
				NewCharAttr &= ~(FOREGROUND_BLUE 
					| FOREGROUND_RED 
					| FOREGROUND_INTENSITY);
				NewCharAttr |= FOREGROUND_GREEN | FOREGROUND_INTENSITY;
				break;
			case NSys::EColor_Red: 
				NewCharAttr &= ~(FOREGROUND_GREEN
					| FOREGROUND_BLUE 
					| FOREGROUND_INTENSITY);
				NewCharAttr |= FOREGROUND_RED | FOREGROUND_INTENSITY;
				break;
			case NSys::EColor_Yellow: 
				NewCharAttr &= ~(FOREGROUND_BLUE 
					| FOREGROUND_INTENSITY);
				NewCharAttr |= FOREGROUND_GREEN
					| FOREGROUND_RED | FOREGROUND_INTENSITY;
				break;
			}
			SetConsoleTextAttribute(mp_hConsole, NewCharAttr);
		}
		~CForegroundColour()
		{
			SetConsoleTextAttribute(mp_hConsole, mp_OldCharAttr);
		}
	};

	template <typename tf_CWinStr, typename tf_UTF8Str, typename tf_CStr>
	void fg_ConsoleOutputHelper(NSys::EColor _Foreground, const tf_CStr &_Str, DWORD _StdHandle, bool _bRaw)
	{
		tf_CWinStr WideChar = fg_StrToWindows<tf_CWinStr>(_Str);
	#if defined(DMibDebug) || DMibConfig_Tests_Enable || defined(DConfig_Profile)
		if (!_bRaw)
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
					CForegroundColour fc(hCon,_Foreground);
					if (!WriteConsoleW(hCon, Temp, nChars, &Written, nullptr))
					{
	//					CFStr256 Error = fg_Win32_GetLastErrorStr();
	//					DMibError((CFStr256::CFormat("Windows returned an error from WriteFile(hCon): {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
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
		//				CFStr256 Error = fg_Win32_GetLastErrorStr();
			//			DMibDTrace("Windows returned an error from WriteFile(hCon): {}", fg_Win32_GetLastErrorStr());
						break;
					}
				}
			}
		}
	}
}

NMib::NStr::CStr NSys::fg_UserManagement_GetProcessRealUser()
{
	return NMib::NStr::CStr();
}

NMib::NStr::CStr NSys::fg_UserManagement_GetProcessEffectiveUser()
{
	return NMib::NStr::CStr();
}

NMib::NStr::CStr NSys::fg_UserManagement_GetProcessRealGroup()
{
	return NMib::NStr::CStr();
}

NMib::NStr::CStr NSys::fg_UserManagement_GetProcessEffectiveGroup()
{
	return NMib::NStr::CStr();
}

NMib::NStr::CStr NSys::fg_UserManagement_GetProcessRealUserName()
{
	return NMib::NStr::CStr();
}

NMib::NStr::CStr NSys::fg_UserManagement_GetProcessEffectiveUserName()
{
	return NMib::NStr::CStr();
}

NMib::NStr::CStr NSys::fg_UserManagement_GetProcessRealGroupName()
{
	return NMib::NStr::CStr();
}

NMib::NStr::CStr NSys::fg_UserManagement_GetProcessEffectiveGroupName()
{
	return NMib::NStr::CStr();
}

bint NSys::fg_UserManagement_IsValidName(NMib::NStr::CStr const &_Name)
{
	return true;
}


bint NSys::fg_ConsoleOutputValid()
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

bint NSys::fg_ConsoleInputValid()
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


bint NSys::fg_ConsoleErrorOutputValid()
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

void NSys::fg_ConsoleOutput(EColor _Foreground, const NMib::NStr::CStrNonTracked &_Str)
{
	fg_ConsoleOutputHelper<CWStrNonTracked, CStrNonTracked>(_Foreground, _Str, STD_OUTPUT_HANDLE, false);
}

void NSys::fg_ConsoleOutputRaw(const NMib::NStr::CStrNonTracked &_Str)
{
	fg_ConsoleOutputHelper<CWStrNonTracked, CStrNonTracked>(EColor_Default, _Str, STD_OUTPUT_HANDLE, true);
}

void NSys::fg_ConsoleOutput(const NMib::NStr::CStrNonTracked &_Str)
{
	fg_ConsoleOutputHelper<CWStrNonTracked, CStrNonTracked>(EColor_Default, _Str, STD_OUTPUT_HANDLE, false);
}

void NSys::fg_ConsoleErrorOutput(EColor _Foreground, const NMib::NStr::CStrNonTracked &_Str)
{
	fg_ConsoleOutputHelper<CWStrNonTracked, CStrNonTracked>(_Foreground, _Str, STD_ERROR_HANDLE, false);
}

void NSys::fg_ConsoleErrorOutput(const NMib::NStr::CStrNonTracked &_Str)
{
	fg_ConsoleOutputHelper<CWStrNonTracked, CStrNonTracked>(EColor_Default, _Str, STD_ERROR_HANDLE, false);
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
		if (_NumaNode != -1 && NLocal::g_fVirtualAllocExNuma)
		{
			if (_Flags & EAllocationFlag_LocationUp)
			{
				pMem = NLocal::g_fVirtualAllocExNuma(GetCurrentProcess(), pOldMem, Size, Flags, PAGE_READWRITE, _NumaNode);
				if (!pMem && (Flags & MEM_LARGE_PAGES))
					pMem = NLocal::g_fVirtualAllocExNuma(GetCurrentProcess(), pOldMem, Size, (Flags) & (~uint32(MEM_LARGE_PAGES)), PAGE_READWRITE, _NumaNode);
			}
			else
			{
				pMem = NLocal::g_fVirtualAllocExNuma(GetCurrentProcess(), pOldMem, Size, Flags | MEM_TOP_DOWN, PAGE_READWRITE, _NumaNode);
				if (!pMem && (Flags & MEM_LARGE_PAGES))
					pMem = NLocal::g_fVirtualAllocExNuma(GetCurrentProcess(), pOldMem, Size, (Flags | MEM_TOP_DOWN) & (~uint32(MEM_LARGE_PAGES)), PAGE_READWRITE, _NumaNode);
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
				auto Error = fg_Win32_GetLastErrorStr();
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
				DMibErrorMemory((CFStr256::CFormat("Windows returned an error from VirtualAlloc: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
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
	if (!NLocal::g_fVirtualAllocExNuma)
		return; // Numa nodes not supported

	if (NLocal::g_fGetNumaNodeProcessorMaskEx && NLocal::g_fSetThreadGroupAffinity)
	{
		GROUP_AFFINITY Affinity;
		fg_MemClear(Affinity);
		if (NLocal::g_fGetNumaNodeProcessorMaskEx(_NumaNode, &Affinity))
		{
			GROUP_AFFINITY OldAffinity;
			fg_MemClear(OldAffinity);
			NLocal::g_fSetThreadGroupAffinity(_pThread, &Affinity, &OldAffinity);
			int x = 0;
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
	if (!NLocal::g_fVirtualAllocExNuma)
		return 0; // Numa nodes not supported

	ULONG HighestNumber;
	if (!GetNumaHighestNodeNumber(&HighestNumber))
		return 0;
	if (HighestNumber == 0)
		return 0;
	int32 Ret = 0;
	for (uint32 i = 0; i <= HighestNumber; ++i)
	{
		if (NLocal::g_fGetNumaNodeProcessorMaskEx)
		{
			GROUP_AFFINITY Affinity;
			fg_MemClear(Affinity);
			if (NLocal::g_fGetNumaNodeProcessorMaskEx(i, &Affinity))
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
	if (!NLocal::g_fVirtualAllocExNuma)
		return; // Numa nodes not supported

	ULONG HighestNumber;
	if (!GetNumaHighestNodeNumber(&HighestNumber))
		return;
	if (HighestNumber == 0)
		return;
	mint Ret = 0;
	for (uint32 i = 0; i <= HighestNumber; ++i)
	{
		if (NLocal::g_fGetNumaNodeProcessorMaskEx)
		{
			GROUP_AFFINITY Affinity;
			fg_MemClear(Affinity);
			if (NLocal::g_fGetNumaNodeProcessorMaskEx(i, &Affinity))
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
				CFStr256 Error = (CFStr256::CFormat("Windows returned an error from VirtualProtect: {}") << fg_Win32_GetLastErrorStr()).f_GetStr();
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
		DMibErrorMemory((CFStr256::CFormat("Windows returned an error from VirtualAlloc: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
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
				DMibErrorMemory((CFStr256::CFormat("Windows returned an error from VirtualAlloc: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
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
		DMibErrorMemory((CFStr256::CFormat("Windows returned an error from VirtualFree: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
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
				DMibErrorMemory((CFStr256::CFormat("Windows returned an error from VirtualFree: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
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
		DMibErrorMemory((CFStr256::CFormat("Windows returned an error from VirtualFree: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}


void *NSys::fg_InterProcess_MemAlloc(ch8 const *_pName, mint _Size, void * &_pMemory)
{
	uint64 Size = _Size; //fg_AlignUp((uint64)_Size, (uint64)gs_SysInfo.dwAllocationGranularity);
	void *pHandle = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE | SEC_COMMIT, (Size >> 32) & 0xFFFFFFFFu, Size & 0xFFFFFFFFu, _pName);
	if (!pHandle)
	{
		auto Error = GetLastError();
		DMibErrorMemory((CFStr256::CFormat("Windows returned an error from CreateFileMappingA: {}") << fg_Win32_GetLastErrorStr(Error)).f_GetStr());
	}
	void *pMemory = MapViewOfFile(pHandle, FILE_MAP_WRITE, 0, 0, Size);
	if (!pMemory)
	{
		auto Error = GetLastError();
		CloseHandle(pHandle);
		DMibErrorMemory((CFStr256::CFormat("Windows returned an error from MapViewOfFile: {}") << fg_Win32_GetLastErrorStr(Error)).f_GetStr());
	}
	_pMemory = pMemory;
	return pHandle;
}

void NSys::fg_InterProcess_MemFree(void *_pHandle, void *_pMemory)
{
	if (!UnmapViewOfFile(_pMemory))
	{
		auto Error = GetLastError();
		DMibErrorMemory((CFStr256::CFormat("Windows returned an error from UnmapViewOfFile: {}") << fg_Win32_GetLastErrorStr(Error)).f_GetStr());
	}
	if (!CloseHandle(_pHandle))
	{
		auto Error = GetLastError();
		DMibErrorMemory((CFStr256::CFormat("Windows returned an error from CloseHandle: {}") << fg_Win32_GetLastErrorStr(Error)).f_GetStr());
	}
}
mint NSys::fg_Mem_VirtualSize(const void *_pMem)
{
	MEMORY_BASIC_INFORMATION Info;
	if (!VirtualQuery(_pMem, &Info, sizeof(Info)))
	{
		DMibErrorMemory((CFStr256::CFormat("Windows returned an error from VirtualProtect: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
	}

	return Info.RegionSize;
}

mint NSys::fg_Mem_VirtualTrySize(const void *_pMem)
{
	DMibPDebugBreak; // Not supported
	return 0;
}


void NMib::NSys::NStr::fg_SystemEncodeCodePageStr(NMib::NStr::CStr const &_In, NMib::NStr::CAnsiStr &_Out, uint32 _CodePage, ch8 _ErrorChar)
{
	CWStr Temp = _In;
	ch8 ErrorStr[] = {_ErrorChar, 0};
	int Len = WideCharToMultiByte(_CodePage, 0, Temp.f_GetStr(), -1, nullptr, 0, ErrorStr, nullptr);
	if (WideCharToMultiByte(_CodePage, 0, Temp.f_GetStr(), -1, _Out.f_GetStr(Len), Len, ErrorStr, nullptr))
		;
	else
		DMibErrorSystemImp((CStr::CFormat("Windows returned an error from WideCharToMultiByte: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NMib::NSys::NStr::fg_SystemDecodeCodePageStr(NMib::NStr::CAnsiStr const &_In, NMib::NStr::CStr &_Out, uint32 _CodePage)
{
	CWStr Out;
	int Len = MultiByteToWideChar(_CodePage, 0, _In.f_GetStr(), -1, nullptr, 0);
	if (MultiByteToWideChar(_CodePage, 0, _In.f_GetStr(), -1, Out.f_GetStr(Len), Len))
		_Out = Out;
	else
		DMibErrorSystemImp((CStr::CFormat("Windows returned an error from MultiByteToWideChar: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NMib::NSys::NStr::fg_SystemDecodeCodePageStr(ch8 const *_pIn, NMib::NStr::CStr &_Out, uint32 _CodePage)
{
	CWStr Out;
	int Len = MultiByteToWideChar(_CodePage, 0, _pIn, -1, nullptr, 0);
	if (MultiByteToWideChar(_CodePage, 0, _pIn, -1, Out.f_GetStr(Len), Len))
		_Out = Out;
	else
		DMibErrorSystemImp((CStr::CFormat("Windows returned an error from MultiByteToWideChar: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NMib::NSys::NStr::fg_SystemEncodeAnsiStr(NMib::NStr::CStr const &_In, NMib::NStr::CAnsiStr &_Out, ch8 _ErrorChar)
{
	CWStr Temp = _In;
	ch8 ErrorStr[] = {_ErrorChar, 0};
	int Len = WideCharToMultiByte(CP_ACP, 0, Temp.f_GetStr(), -1, nullptr, 0, ErrorStr, nullptr);
	if (WideCharToMultiByte(CP_ACP, 0, Temp.f_GetStr(), -1, _Out.f_GetStr(Len), Len, ErrorStr, nullptr))
		;
	else
		DMibErrorSystemImp((CStr::CFormat("Windows returned an error from WideCharToMultiByte: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NMib::NSys::NStr::fg_SystemDecodeAnsiStr(NMib::NStr::CAnsiStr const &_In, NMib::NStr::CStr &_Out)
{
	CWStr Out;
	int Len = MultiByteToWideChar(CP_ACP, 0, _In.f_GetStr(), -1, nullptr, 0);
	if (MultiByteToWideChar(CP_ACP, 0, _In.f_GetStr(), -1, Out.f_GetStr(Len), Len))
		_Out = Out;
	else
		DMibErrorSystemImp((CStr::CFormat("Windows returned an error from MultiByteToWideChar: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NMib::NSys::NStr::fg_SystemDecodeAnsiStr(ch8 const *_pIn, NMib::NStr::CStr &_Out)
{
	CWStr Out;
	int Len = MultiByteToWideChar(CP_ACP, 0, _pIn, -1, nullptr, 0);
	if (MultiByteToWideChar(CP_ACP, 0, _pIn, -1, Out.f_GetStr(Len), Len))
		_Out = Out;
	else
		DMibErrorSystemImp((CStr::CFormat("Windows returned an error from MultiByteToWideChar: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
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
		DMibErrorSystemImp((CStr::CFormat("Windows returned an error from WideCharToMultiByte: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NMib::NSys::NStr::fg_SystemDecodeCodePageStr(NMib::NStr::CAnsiStrNonTracked const &_In, NMib::NStr::CStrNonTracked &_Out, uint32 _CodePage)
{
	CWStrNonTracked Out;
	int Len = MultiByteToWideChar(_CodePage, 0, _In.f_GetStr(), -1, nullptr, 0);
	if (MultiByteToWideChar(_CodePage, 0, _In.f_GetStr(), -1, Out.f_GetStr(Len), Len))
		_Out = Out;
	else
		DMibErrorSystemImp((CStr::CFormat("Windows returned an error from MultiByteToWideChar: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NMib::NSys::NStr::fg_SystemDecodeCodePageStr(ch8 const *_pIn, NMib::NStr::CStrNonTracked &_Out, uint32 _CodePage)
{
	CWStrNonTracked Out;
	int Len = MultiByteToWideChar(_CodePage, 0, _pIn, -1, nullptr, 0);
	if (MultiByteToWideChar(_CodePage, 0, _pIn, -1, Out.f_GetStr(Len), Len))
		_Out = Out;
	else
		DMibErrorSystemImp((CStr::CFormat("Windows returned an error from MultiByteToWideChar: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NMib::NSys::NStr::fg_SystemEncodeAnsiStr(NMib::NStr::CStrNonTracked const &_In, NMib::NStr::CAnsiStrNonTracked &_Out, ch8 _ErrorChar)
{
	CWStrNonTracked Temp = _In;
	ch8 ErrorStr[] = {_ErrorChar, 0};
	int Len = WideCharToMultiByte(CP_ACP, 0, Temp.f_GetStr(), -1, nullptr, 0, ErrorStr, nullptr);
	if (WideCharToMultiByte(CP_ACP, 0, Temp.f_GetStr(), -1, _Out.f_GetStr(Len), Len, ErrorStr, nullptr))
		;
	else
		DMibErrorSystemImp((CStr::CFormat("Windows returned an error from WideCharToMultiByte: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NMib::NSys::NStr::fg_SystemDecodeAnsiStr(NMib::NStr::CAnsiStrNonTracked const &_In, NMib::NStr::CStrNonTracked &_Out)
{
	CWStrNonTracked Out;
	int Len = MultiByteToWideChar(CP_ACP, 0, _In.f_GetStr(), -1, nullptr, 0);
	if (MultiByteToWideChar(CP_ACP, 0, _In.f_GetStr(), -1, Out.f_GetStr(Len), Len))
		_Out = Out;
	else
		DMibErrorSystemImp((CStr::CFormat("Windows returned an error from MultiByteToWideChar: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NMib::NSys::NStr::fg_SystemDecodeAnsiStr(ch8 const *_pIn, NMib::NStr::CStrNonTracked &_Out)
{
	CWStrNonTracked Out;
	int Len = MultiByteToWideChar(CP_ACP, 0, _pIn, -1, nullptr, 0);
	if (MultiByteToWideChar(CP_ACP, 0, _pIn, -1, Out.f_GetStr(Len), Len))
		_Out = Out;
	else
		DMibErrorSystemImp((CStr::CFormat("Windows returned an error from MultiByteToWideChar: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
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
		bint bSuccess = false;
		CFStr256 ErrorStr;
		if (SuspendThread(hThread) != 0xFFFFFFFF)
		{
			NLocal::THREAD_BASIC_INFORMATION ThreadInfo;
			if (NT_SUCCESS(NLocal::g_fNtQueryInformationThread(hThread, (::THREADINFOCLASS)NLocal::ThreadBasicInformation, &ThreadInfo, sizeof( NLocal::THREAD_BASIC_INFORMATION ), 0)))
			{
				mint *pTIB = (mint *)ThreadInfo.TebBaseAddress;
	#if defined(_M_X64)
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
				ErrorStr = "NtQueryInformationThread: " + fg_Win32_GetLastErrorStr(GetLastError());
	
			ResumeThread(hThread);
		}
		else
			ErrorStr = "SuspendThread: " + fg_Win32_GetLastErrorStr(GetLastError());
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
		bint bSuccess = false;
		CFStr256 ErrorStr;
		if (SuspendThread(hThread) != 0xFFFFFFFF)
		{
			NLocal::THREAD_BASIC_INFORMATION ThreadInfo;
			if (NT_SUCCESS(NLocal::g_fNtQueryInformationThread(hThread, (::THREADINFOCLASS)NLocal::ThreadBasicInformation, &ThreadInfo, sizeof( NLocal::THREAD_BASIC_INFORMATION ), 0)))
			{
				mint *pTIB = (mint *)ThreadInfo.TebBaseAddress;
	#if defined(_M_X64)
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
				ErrorStr = "NtQueryInformationThread: " + fg_Win32_GetLastErrorStr(GetLastError());
	
			ResumeThread(hThread);
		}
		else
			ErrorStr = "SuspendThread: " + fg_Win32_GetLastErrorStr(GetLastError());
		CloseHandle(hThread);

		if (!bSuccess && !g_bProcessDetached)
			DMibError((CFStr256::CFormat("Failed to get thread storage for another thread ({}): {}") << _ThreadID << ErrorStr).f_GetStr());
	}

	return pRet;
}

DWORD WINAPI fg_TlsAllocInternal( bint _bFast)
{
	CUndocumentedTEB *pTEB = fg_GetTEB();
	UndocumentedPEB *pPEB = fg_GetPEB(pTEB);
	DWORD index = -1;

	NLocal::g_fRtlAcquirePebLock();
	if (_bFast)
	{
		index = NLocal::g_fRtlFindClearBitsAndSet( pPEB->TlsBitmap, 1, 0 );
		if (index != ~0U) 
			pTEB->TlsSlots[index] = 0; /* clear the value */
	}
	else
	{
		index = NLocal::g_fRtlFindClearBitsAndSet( pPEB->TlsExpansionBitmap, 1, 0 );
		if (index != ~0U)
		{
			if (!pTEB->TlsExpansionSlots &&
				!(pTEB->TlsExpansionSlots = (PPVOID)HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
				8 * sizeof(pPEB->TlsExpansionBitmapBits) * sizeof(void*) )))
			{
				NLocal::g_fRtlClearBits( pPEB->TlsExpansionBitmap, index, 1 );
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
	NLocal::g_fRtlReleasePebLock();
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

#if defined(_M_X64)
mint g_OffsetThreadLocalOffset = 0x1780;
#else
mint g_OffsetThreadLocalOffset = 0xf94;
#endif


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

bint fg_Win32_RunningWine()
{
	return NLocal::g_fWineGetVersion != nullptr;
}

NStr::CFStr256 fg_Win32_WineVersion()
{
	if (NLocal::g_fWineGetVersion)
		return NLocal::g_fWineGetVersion();
	return "";
}

void __cdecl fg_FixFunctionPointers();
void __cdecl fg_FixFunctionPointers_Alloc();

void fg_LoadFunctionPointers()
{
	if (!g_hKernel32)
	{
		g_hKernel32 = GetModuleHandle(str_utf16("kernel32.dll"));
		g_hNtDll = GetModuleHandle(str_utf16("ntdll.dll"));
		g_hAdvAPI32 = GetModuleHandle(str_utf16("advapi32.dll"));

		
		(FARPROC &)NLocal::g_fGetLogicalProcessorInformation = GetProcAddress(g_hKernel32, "GetLogicalProcessorInformation");
		(FARPROC &)NLocal::g_fRtlAcquirePebLock = GetProcAddress(g_hNtDll, "RtlAcquirePebLock");
		(FARPROC &)NLocal::g_fRtlReleasePebLock = GetProcAddress(g_hNtDll, "RtlReleasePebLock");
		(FARPROC &)NLocal::g_fRtlFindClearBitsAndSet = GetProcAddress(g_hNtDll, "RtlFindClearBitsAndSet");
		(FARPROC &)NLocal::g_fRtlClearBits = GetProcAddress(g_hNtDll, "RtlClearBits");
		(FARPROC &)NLocal::g_fNtQueryInformationThread = GetProcAddress(g_hNtDll, "NtQueryInformationThread");

		(FARPROC &)NLocal::g_fAddVectoredExceptionHandler = GetProcAddress(g_hKernel32, "AddVectoredExceptionHandler");
		(FARPROC &)NLocal::g_fGetNativeSystemInfo = GetProcAddress(g_hKernel32, "GetNativeSystemInfo");
		(FARPROC &)NLocal::g_fRemoveVectoredExceptionHandler = GetProcAddress(g_hKernel32, "RemoveVectoredExceptionHandler");

		(FARPROC &)NLocal::g_fSetProcessUserModeExceptionPolicy = GetProcAddress(g_hKernel32, "SetProcessUserModeExceptionPolicy");
		(FARPROC &)NLocal::g_fGetProcessUserModeExceptionPolicy = GetProcAddress(g_hKernel32, "GetProcessUserModeExceptionPolicy");
		NLocal::g_pKiUserApcDispatcher = GetProcAddress(g_hNtDll, "KiUserApcDispatcher");
		NLocal::g_pKiUserCallbackDispatcher = GetProcAddress(g_hNtDll, "KiUserCallbackDispatcher");


		(FARPROC &)NLocal::g_fWineGetVersion = GetProcAddress(g_hNtDll, "wine_get_version");

		(FARPROC &)NLocal::g_fLargePageMinimum = GetProcAddress(g_hKernel32, "GetLargePageMinimum");

		(FARPROC &)NLocal::g_fVirtualAllocExNuma = GetProcAddress(g_hKernel32, "VirtualAllocExNuma");
		(FARPROC &)NLocal::g_fGetNumaNodeProcessorMaskEx = GetProcAddress(g_hKernel32, "GetNumaNodeProcessorMaskEx");
		(FARPROC &)NLocal::g_fSetThreadGroupAffinity = GetProcAddress(g_hKernel32, "SetThreadGroupAffinity");

		(FARPROC &)NLocal::g_fWTSGetActiveConsoleSessionId = GetProcAddress(g_hKernel32, "WTSGetActiveConsoleSessionId");


		(FARPROC &)NLocal::g_fCreateProcessWithTokenW = GetProcAddress(g_hAdvAPI32, "CreateProcessWithTokenW");
		(FARPROC &)NLocal::g_fCreateSymbolicLinkW = GetProcAddress(g_hKernel32, "CreateSymbolicLinkW");
		(FARPROC &)NLocal::g_fCreateHardLinkW = GetProcAddress(g_hKernel32, "CreateHardLinkW");

		(FARPROC &)NLocal::g_fWow64DisableWow64FsRedirection = GetProcAddress(g_hKernel32, "Wow64DisableWow64FsRedirection");
		(FARPROC &)NLocal::g_fWow64RevertWow64FsRedirection = GetProcAddress(g_hKernel32, "Wow64RevertWow64FsRedirection");

		(FARPROC &)NLocal::g_fNtSetInformationProcess = GetProcAddress(g_hNtDll, "NtSetInformationProcess");

		(FARPROC &)NLocal::g_fSetProcessInformation = GetProcAddress(g_hKernel32, "SetProcessInformation");

		(FARPROC &)NLocal::g_fCancelSynchronousIo = GetProcAddress(g_hKernel32, "CancelSynchronousIo");
		(FARPROC &)NLocal::g_fCancelIoEx = GetProcAddress(g_hKernel32, "CancelIoEx");

		(FARPROC &)NLocal::g_fNtQuerySystemInformation = GetProcAddress(g_hNtDll, "NtQuerySystemInformation");

		(FARPROC &)NLocal::g_fNtGetNextThread = GetProcAddress(g_hNtDll, "NtGetNextThread");

		(FARPROC &)NLocal::g_fGetThreadId = GetProcAddress(g_hKernel32, "GetThreadId");

		(FARPROC &)NLocal::g_fNtQueryInformationProcess = GetProcAddress(g_hNtDll, "NtQueryInformationProcess");
		(FARPROC &)NLocal::g_fLdrDisableThreadCalloutsForDll = GetProcAddress(g_hNtDll, "LdrDisableThreadCalloutsForDll");

		NLocal::g_VersionInfo.dwOSVersionInfoSize = sizeof(NLocal::g_VersionInfo);
		GetVersionExW((OSVERSIONINFO *)&NLocal::g_VersionInfo);
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

void fg_InitMalterlibAllEnumOtherThreads()
{
//	if (!g_bIsDll) // If we are the main executable we don't need to enum other threads
//		return;

	mint ThisUID = NSys::fg_Thread_GetCurrentUID();
	uint32 CurrentProcess = GetCurrentProcessId();

	//
	bool bDone = false;

	if (NLocal::g_fNtGetNextThread && NLocal::g_fGetThreadId)
	{
		auto CurrentProcess = GetCurrentProcess();
		HANDLE hThread = nullptr;
		bDone = true;
		NLocal::g_fNtGetNextThread(CurrentProcess, nullptr, THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, 0, 0, &hThread);
		while (hThread)
		{
			auto ThreadID = NLocal::g_fGetThreadId(hThread);

			if (ThreadID && ThreadID != ThisUID)
			{
				if (SuspendThread(hThread) != 0xFFFFFFFF)
				{
					fg_GetLocalSys()->f_ThreadLocalCreateThread(ThreadID, ThisUID);
					ResumeThread(hThread);
				}
			}
			HANDLE hPrevThread = hThread;
			hThread = nullptr;
			NLocal::g_fNtGetNextThread(CurrentProcess, hPrevThread, THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, 0, 0, &hThread);
			CloseHandle(hPrevThread);
		}
	}

	while (NLocal::g_fNtQuerySystemInformation && !bDone)
	{
		DWORD NeededSize = 0;
		NLocal::g_fNtQuerySystemInformation(SystemProcessInformation, nullptr, 0, &NeededSize);
		if (!NeededSize)
			break;

		NeededSize *= 2;
		TCVector<uint8> Data;
		Data.f_SetLen(NeededSize);
		NLocal::SYSTEM_PROCESS_INFORMATION *pInfo = (NLocal::SYSTEM_PROCESS_INFORMATION *)Data.f_GetArray();

		if (NTSTATUS RetVal = NLocal::g_fNtQuerySystemInformation(SystemProcessInformation, pInfo, NeededSize, &NeededSize))
			break;

		int32 SizeLeft = NeededSize;
		while (SizeLeft > 0)
		{
			if ((mint)pInfo->UniqueProcessId == CurrentProcess)
			{
				bDone = true;
				for (mint i = 0; i < pInfo->NumberOfThreads; ++i)
				{
					auto &Thread = pInfo->Threads[i];
					mint ThreadID = (mint)Thread.ClientId.UniqueThread;
					if (Thread.State != NLocal::StateTerminated && ThreadID != ThisUID)
						fg_GetLocalSys()->f_ThreadLocalCreateThread((mint)ThreadID, ThisUID);
				}
				break;
			}

			if (!pInfo->NextEntryOffset)
				break;
			SizeLeft -= pInfo->NextEntryOffset;
			pInfo = (NLocal::SYSTEM_PROCESS_INFORMATION *)((mint)pInfo + pInfo->NextEntryOffset);
		}
		break;
	}
	
	if (!bDone)
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
						fg_GetLocalSys()->f_ThreadLocalCreateThread(te.th32ThreadID, ThisUID);
					}
					te.dwSize = sizeof(te);
				} while (Thread32Next(h, &te));
			}
			CloseHandle(h);
		}
	}
}

void fg_MakeTlsActive();
void fg_InitMalterlibAllInternalComplex(void *_pInstance)
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

	fg_CreateMalterlib();

	fg_FixFunctionPointers_Alloc();

	mint ThisUID = NSys::fg_Thread_GetCurrentUID();
	fg_GetLocalSys()->f_ThreadLocalCreateThread(ThisUID, 0);
	
	fg_InitMalterlibAllEnumOtherThreads();

	g_bDoneMalterlibInitAll.f_FetchAdd(1);

	g_bCanStartThreads = true;
	fg_GetLocalSys()->f_InitModuleThreaded();
	

	NSys::fg_Compiler_MakeActive(&g_OffsetThreadLocalOffset);
	fg_MakeTlsActive();

	g_bDoneMalterlibInitAll.f_FetchAdd(1);
}

bool __cdecl fg_InitMalterlibAllInternal(void *_pInstance)
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
	AuxUlibInitialize();

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
						if (NT_SUCCESS(NLocal::g_fNtQueryInformationThread(hThread, (::THREADINFOCLASS)NLocal::ThreadBasicInformation, &ThreadInfo, sizeof( NLocal::THREAD_BASIC_INFORMATION ), 0)))
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
			if (NLocal::g_fLdrDisableThreadCalloutsForDll)
			{
				auto Ret = NLocal::g_fLdrDisableThreadCalloutsForDll((void *)(mint)1);

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
		CAllocator_NonTrackedHeap::f_Free(_pMem);
}


extern bint g_bSysDeleted;

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
	TCUniquePointer<CThreadParameters, NMem::CAllocator_NonTrackedHeap> pThreadParameters = fg_Explicit((CThreadParameters *)_pParameter);
	FThreadProc *pProc = pThreadParameters->m_pProc;
	void *pParam = pThreadParameters->m_pParam;
	fg_SetThreadName(GetCurrentThreadId(), pThreadParameters->m_Name);
	pThreadParameters.f_Clear();
	return pProc(pParam);
}

namespace
{
	int fg_TranslateThreadPrio(mint _Priority)
	{
		int Prio = THREAD_PRIORITY_NORMAL;
		if (_Priority < EThreadPriority_Low)
			Prio = THREAD_PRIORITY_IDLE;
		else if (_Priority < EThreadPriority_BelowNormal)
			Prio = THREAD_PRIORITY_LOWEST;
		else if (_Priority < EThreadPriority_Normal)
			Prio = THREAD_PRIORITY_BELOW_NORMAL;
		else if (_Priority < EThreadPriority_AboveNormal)
			Prio = THREAD_PRIORITY_NORMAL;
		else if (_Priority < EThreadPriority_High)
			Prio = THREAD_PRIORITY_ABOVE_NORMAL;
		else if (_Priority < EThreadPriority_Highest)
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
		DMibErrorSystemImp((CFStr256::CFormat("Windows returned an error from DuplicateHandle: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
	}
	return Ret;
	//return OpenThread(SYNCHRONIZE, false, (uint32)_pThread);
}

void NSys::fg_Thread_WillNotBlockUntilExit(void *_pThreadDestroyContext)
{
}

void NSys::fg_Thread_BlockUntilExit(void *_pThreadDestroyContext)
{
	BOOL bDllHeld = false;
	AuxUlibIsDLLSynchronizationHeld(&bDllHeld);
	if (bDllHeld)
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

void *NSys::fg_Thread_Create(FThreadProc *_pThreadProc, void *_pParam, mint _Priority, mint _StackSize, bint _bSuspended, const ch8 *_pThreadName, mint _Affinity, mint &_ThreadID)
{
	TCUniquePointer<CThreadParameters, NMem::CAllocator_NonTrackedHeap> pThreadParameters = fg_Construct();

	pThreadParameters->m_pProc = _pThreadProc;
	pThreadParameters->m_pParam = _pParam;
	pThreadParameters->m_Name = _pThreadName;

	DWORD ThreadID;
	HANDLE hThread = CreateThread(nullptr, _StackSize, fg_MalterlibMSVC_ThreadProc, pThreadParameters.f_Get(), CREATE_SUSPENDED, &ThreadID);
	if (!hThread)
	{
		DMibErrorSystemImp((CFStr256::CFormat("Windows returned an error from CreateThread: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
	}

	pThreadParameters.f_Detach();

	// This is needed to fix a wine bug where the TebAddress is not set for the thread until after a while
	while (1)
	{
		NLocal::THREAD_BASIC_INFORMATION ThreadInfo;
		if (NT_SUCCESS(NLocal::g_fNtQueryInformationThread(hThread, (::THREADINFOCLASS)NLocal::ThreadBasicInformation, &ThreadInfo, sizeof( NLocal::THREAD_BASIC_INFORMATION ), 0)))
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

void NSys::fg_Thread_Suspend(void *_pThread)
{
	if (SuspendThread(_pThread) == 0xFFFFFFFF)
	{
		DMibErrorSystemImp((CFStr256::CFormat("Windows returned an error from SuspendThread: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}

void NSys::fg_Thread_Resume(void *_pThread)
{
	if (ResumeThread(_pThread) == 0xFFFFFFFF)
	{
		DMibErrorSystemImp((CFStr256::CFormat("Windows returned an error from ResumeThread: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}

void NSys::fg_Thread_SetPriority(void *_pThread, mint _Priority)
{
	if (!SetThreadPriority(_pThread, fg_TranslateThreadPrio(_Priority)))
	{
		DMibErrorSystemImp((CFStr256::CFormat("Windows returned an error from SetThreadPriority: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}

void NSys::fg_Thread_SetAffinity(void *_pThread, mint _Affinity)
{
	if (!SetThreadAffinityMask(_pThread, _Affinity))
	{
		DMibErrorSystemImp((CFStr256::CFormat("Windows returned an error from SetThreadAffinityMask: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}

void NSys::fg_Thread_Destroy(void *_pThread)
{
	if (_pThread != INVALID_HANDLE_VALUE)
		CloseHandle(_pThread);
}

uint32 fg_Win32_TranslateProcessPriority(EExecutionPriority _Priority)
{

	if (_Priority == EExecutionPriority_Default)
	{
		return 0;
	}
	else if (_Priority == EExecutionPriority_Highest)
	{
		return REALTIME_PRIORITY_CLASS;
	}
	else if (_Priority >= EExecutionPriority_High)
	{
		return HIGH_PRIORITY_CLASS;
	}
	else if (_Priority >= EExecutionPriority_AboveNormal)
	{
		return ABOVE_NORMAL_PRIORITY_CLASS;
	}
	else if (_Priority >= EExecutionPriority_Normal)
	{
		return NORMAL_PRIORITY_CLASS;
	}
	else if (_Priority >= EExecutionPriority_BelowNormal)
	{
		return BELOW_NORMAL_PRIORITY_CLASS;
	}
	else
	{
		return IDLE_PRIORITY_CLASS;
	}
}

void NSys::fg_Process_SetPriority(uint16 _Priority)
{
	SetPriorityClass(GetCurrentProcess(), fg_Win32_TranslateProcessPriority((EExecutionPriority)_Priority));
}

NContainer::TCVector<NProcess::CProcessInfo> NSys::fg_Process_Enum(NProcess::EProcessInfoFlag _ToGet, NContainer::TCVector<NProcess::CProcessInfo> * _pOldEnum)
{
	NContainer::TCVector<NProcess::CProcessInfo> Ret;
	
	CProcessEntry RootProcess;
	HANDLE hProcessSnap;
	PROCESSENTRY32 pe32;

	// Take a snapshot of all processes in the system.
	hProcessSnap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
	if (hProcessSnap != INVALID_HANDLE_VALUE)
	{
		pe32.dwSize = sizeof( PROCESSENTRY32 );

		if (Process32First( hProcessSnap, &pe32 ) )
		{
			do
			{
				auto & NewProcess = Ret.f_Insert();
				
				NewProcess.m_ProcessID = pe32.th32ProcessID;
				NewProcess.m_ParentProcessID = pe32.th32ParentProcessID;
				
				HANDLE pThisProcess = OpenProcess(PROCESS_QUERY_INFORMATION, false, pe32.th32ProcessID);

				FILETIME CreateTime;
				FILETIME ExitTime;
				FILETIME KernelTime;
				FILETIME UserTime;

				if (GetProcessTimes(pThisProcess, &CreateTime, &ExitTime, &KernelTime, &UserTime))
					NewProcess.m_StartTime = uint64(CreateTime.dwHighDateTime) << 32 | uint64(CreateTime.dwLowDateTime);

				if (pThisProcess)
					CloseHandle(pThisProcess);
			} 
			while( Process32Next( hProcessSnap, &pe32 ) );
		}

		CloseHandle( hProcessSnap );
	}
	return Ret;
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


void NSys::fg_Security_GenerateHighEntropyData(uint8 *_pData, mint _nBytes)
{
	HCRYPTPROV hProvider = 0;

	if (!CryptAcquireContextW(&hProvider, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
		DMibError((CStr::CFormat("Windows returned an error from CryptAcquireContextW: {}") << fg_Win32_GetLastErrorStr(GetLastError())).f_GetStr());

	auto Cleanup = fg_OnScopeExit
		(
			[&]()
			{
				CryptReleaseContext(hProvider, 0);
			}
		)
	;
	if (!CryptGenRandom(hProvider, _nBytes, _pData))
		DMibError((CStr::CFormat("Windows returned an error from CryptGenRandom: {}") << fg_Win32_GetLastErrorStr(GetLastError())).f_GetStr());
}


#include "rpc.h"
#pragma comment(lib, "rpcrt4.lib")


void NSys::fg_System_GenerateUUID(NDataProcessing::CUniversallyUniqueIdentifier &_UUID)
{
	
	UUID Ret;
	HRESULT ErrorCode = UuidCreate(&Ret);
	if (ErrorCode != RPC_S_OK)
		DMibError((CStr::CFormat("Windows returned an error from UuidCreate: {}") << fg_Win32_GetLastErrorStr(ErrorCode)).f_GetStr());
	
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
		DMibError((CStr::CFormat("Windows returned an error from UuidCreate: {}") << fg_Win32_GetLastErrorStr(ErrorCode)).f_GetStr());

	// {7EE072A7-458D-491f-ACCF-447AD4BE8DBF}
	return CStr(CStr::CFormat("{{{nfh,sj8,sf0}-{nfh,sj4,sf0}-{nfh,sj4,sf0}-{nfh,sj2,sf0}{nfh,sj2,sf0}-{nfh,sj2,sf0}{nfh,sj2,sf0}{nfh,sj2,sf0}{nfh,sj2,sf0}{nfh,sj2,sf0}{nfh,sj2,sf0}}") << Ret.Data1 << Ret.Data2 << Ret.Data3 << Ret.Data4[0] << Ret.Data4[1] << Ret.Data4[2] << Ret.Data4[3] << Ret.Data4[4] << Ret.Data4[5] << Ret.Data4[6] << Ret.Data4[7]);
}

NStr::CStr NSys::fg_Process_GetUserName()
{
	uint32 Size = 0;
	::GetUserNameW(nullptr, &Size);
	
	++Size;
	NMib::NStr::CWStr Return;
	::GetUserNameW(Return.f_GetStr(Size), &Size);
	Return.f_TrimSize();
	return Return;
}

NStr::CStr NSys::fg_Process_GetComputerAddress()
{
	return NSys::fg_Process_GetComputerName();
}

NStr::CStr NSys::fg_Process_GetComputerName()
{
	uint32 Size = 0;
	::GetComputerNameW(nullptr, &Size);
	
	++Size;
	NMib::NStr::CWStr Return;
	::GetComputerNameW(Return.f_GetStr(Size), &Size);
	Return.f_TrimSize();
	return Return;
}

NStr::CStr NSys::fg_Process_GetHostName()
{
	CWStr Temp;
	DWORD Size = 0;
	GetComputerNameExW(ComputerNameDnsFullyQualified, Temp.f_GetStr(1), &Size);
	GetComputerNameExW(ComputerNameDnsFullyQualified, Temp.f_GetStr(Size), &Size);
	Temp.f_GetLen();
	return fg_StrFromWindows(Temp);
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

NStr::CStr NSys::fg_Process_GetComputerDomain()
{
	CWStr Temp;
	DWORD Size = 0;
	GetComputerNameExW(ComputerNameDnsDomain, Temp.f_GetStr(1), &Size);
	GetComputerNameExW(ComputerNameDnsDomain, Temp.f_GetStr(Size), &Size);
	Temp.f_GetLen();

	return fg_StrFromWindows(Temp);
}

mint NSys::fg_Process_GetCurrentUID()
{
	return GetCurrentProcessId();
}

bint NSys::fg_Process_GetProcessIsParentProcess(mint _ProcessID)
{
	// Not implemented yet
	return false;
}

NContainer::TCMap<NMib::NStr::CStr, NMib::NStr::CStr> NSys::fg_Process_GetEnvironmentVariables()
{
	TCMap<NMib::NStr::CStr, NMib::NStr::CStr> Ret;
	LPWSTR pStrings = GetEnvironmentStringsW();
	if (!pStrings)
		DMibError((CStr::CFormat("Windows returned an error from GetEnvironmentStringsW: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
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

NContainer::TCMap<NMib::NStr::CStrNonTracked, NMib::NStr::CStrNonTracked> NSys::fg_Process_GetEnvironmentVariablesNonTracked()
{
	TCMap<NMib::NStr::CStrNonTracked, NMib::NStr::CStrNonTracked> Ret;
	LPWSTR pStrings = GetEnvironmentStringsW();
	if (!pStrings)
		DMibError((CStr::CFormat("Windows returned an error from GetEnvironmentStringsW: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
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
		CStrNonTracked String = CWStrNonTracked(pStrings);
		CStrNonTracked Key;
		CStrNonTracked Value;
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

NMib::NStr::CStr NSys::fg_Process_GetEnvironmentVariable(NMib::NStr::CStr const &_VariableName)
{
	NMib::NStr::CWStr Temp;
	ch16 *pStr = Temp.f_GetStr(32768);
	pStr[0] = 0;
	GetEnvironmentVariableW(fg_StrToWindows(_VariableName), pStr, 32768);
	Temp.f_TrimSize();
	return Temp;
}

bint NSys::fg_Process_GetEnvironmentVariable(NMib::NStr::CStr const &_VariableName, NMib::NStr::CStr &_Value)
{
	NMib::NStr::CWStr Temp;
	ch16 *pStr = Temp.f_GetStr(32768);
	pStr[0] = 0;
	if (!GetEnvironmentVariableW(fg_StrToWindows(_VariableName), pStr, 32768))
		return false;

	Temp.f_TrimSize();
 	_Value = fg_Move(Temp);
	return true;
}

void NMib::NSys::fg_Process_SetEnvironmentVariable(NMib::NStr::CStr const &_VariableName, NMib::NStr::CStr const &_Value)
{
	if (!SetEnvironmentVariableW(fg_StrToWindows(_VariableName), fg_StrToWindows(_Value)))
		DMibError((CStr::CFormat("Windows returned an error from SetEnvironmentVariableW: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
}

NMib::NStr::CStrNonTracked NSys::fg_Process_GetEnvironmentVariable(NMib::NStr::CStrNonTracked const &_VariableName)
{
	NMib::NStr::CWStrNonTracked Temp;
	ch16 *pStr = Temp.f_GetStr(32768);
	pStr[0] = 0;
	GetEnvironmentVariableW(fg_StrToWindows<NMib::NStr::CWStrNonTracked>(_VariableName), pStr, 32768);
	Temp.f_TrimSize();
	return Temp;
}

bint NSys::fg_Process_GetEnvironmentVariable(NMib::NStr::CStrNonTracked const &_VariableName, NMib::NStr::CStrNonTracked &_Value)
{
	NMib::NStr::CWStrNonTracked Temp;
	ch16 *pStr = Temp.f_GetStr(32768);
	pStr[0] = 0;
	if (!GetEnvironmentVariableW(fg_StrToWindows<NMib::NStr::CWStrNonTracked>(_VariableName), pStr, 32768))
		return false;

	Temp.f_TrimSize();
	_Value = fg_Move(Temp);
	return true;
}

void NMib::NSys::fg_Process_SetEnvironmentVariable(NMib::NStr::CStrNonTracked const &_VariableName, NMib::NStr::CStrNonTracked const &_Value)
{
	if (!SetEnvironmentVariableW(fg_StrToWindows<NMib::NStr::CWStrNonTracked>(_VariableName), fg_StrToWindows<NMib::NStr::CWStrNonTracked>(_Value)))
		DMibError((CStr::CFormat("Windows returned an error from SetEnvironmentVariableW: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
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

static inline_always void fg_MakeFunctionInline()
{
}

void NSys::fg_Process_GetMemoryCurrentStatistics(void *_pProcess, CProcessStatistics &_Stats)
{
	PROCESS_MEMORY_COUNTERS_EX MemoryInfo;
	fg_MemClear(MemoryInfo);

	if (GetProcessMemoryInfo(_pProcess, (PROCESS_MEMORY_COUNTERS *)&MemoryInfo, sizeof(MemoryInfo)))
	{
		_Stats.m_Statistics("Working set size", CProcessStat(EProcessStatUnit_Bytes, MemoryInfo.WorkingSetSize, 1024 * 1024));
		_Stats.m_Statistics("Paged pool usage", CProcessStat(EProcessStatUnit_Bytes, MemoryInfo.QuotaPagedPoolUsage, 1024));
		_Stats.m_Statistics("Non paged pool usage", CProcessStat(EProcessStatUnit_Bytes, MemoryInfo.QuotaNonPagedPoolUsage, 1024));
		_Stats.m_Statistics("Page file usage", CProcessStat(EProcessStatUnit_Bytes, MemoryInfo.PagefileUsage, 1024 * 1024));
		_Stats.m_Statistics("Private usage", CProcessStat(EProcessStatUnit_Bytes, MemoryInfo.PrivateUsage, 1024 * 1024));
	}
}

void NSys::fg_Process_GetMemoryOverallStatistics(void *_pProcess, CProcessStatistics &_Stats)
{
	PROCESS_MEMORY_COUNTERS_EX MemoryInfo;
	fg_MemClear(MemoryInfo);

	if (GetProcessMemoryInfo(_pProcess, (PROCESS_MEMORY_COUNTERS *)&MemoryInfo, sizeof(MemoryInfo)))
	{
		_Stats.m_Statistics("Total page faults", CProcessStat(EProcessStatUnit_GeneralNumber, MemoryInfo.PageFaultCount));
		_Stats.m_Statistics("Peak working set size", CProcessStat(EProcessStatUnit_Bytes, MemoryInfo.PeakWorkingSetSize, 1024 * 1024));
		_Stats.m_Statistics("Peak paged pool usage", CProcessStat(EProcessStatUnit_Bytes, MemoryInfo.QuotaPeakPagedPoolUsage, 1024));
		_Stats.m_Statistics("Peak non paged pool usage", CProcessStat(EProcessStatUnit_Bytes, MemoryInfo.QuotaPeakNonPagedPoolUsage, 1024));
		_Stats.m_Statistics("Peak page file usage", CProcessStat(EProcessStatUnit_Bytes, MemoryInfo.PeakPagefileUsage, 1024 * 1024));
	}
}

void NSys::fg_Process_GetExecutionCurrentStatistics(void *_pProcess, CProcessStatistics &_Stats)
{
	FILETIME CreateTime1;
	FILETIME ExitTime1;
	FILETIME KernelTime1;
	FILETIME UserTime1;
	
	if (GetProcessTimes(_pProcess, &CreateTime1, &ExitTime1, &KernelTime1, &UserTime1))
	{
		NTime::CTime CreateTime = fg_Win32_FileTimeToMalterlibTime(CreateTime1);
		NTime::CTimeSpan KernelTime = fg_Win32_FileTimeToMalterlibTimeSpan(KernelTime1);
		NTime::CTimeSpan UserTime = fg_Win32_FileTimeToMalterlibTimeSpan(UserTime1);
		NTime::CTimeSpan RunTime = NTime::CTime::fs_NowUTC() - CreateTime;
		
		fp64 KernelSeconds = KernelTime.f_GetSecondsFraction();
		fp64 UserSeconds = UserTime.f_GetSecondsFraction();
		fp64 RunSeconds = RunTime.f_GetSecondsFraction();
		
		_Stats.m_Statistics("CPU utilization Total", CProcessStat(EProcessStatUnit_Fraction, (KernelSeconds + UserSeconds) / RunSeconds));
		_Stats.m_Statistics("CPU utilization User", CProcessStat(EProcessStatUnit_Fraction, UserSeconds / RunSeconds));
		_Stats.m_Statistics("CPU utilization Kernel", CProcessStat(EProcessStatUnit_Fraction, KernelSeconds / RunSeconds));
	}
}

void NSys::fg_Process_GetExecutionOverallStatistics(void *_pProcess, CProcessStatistics &_Stats)
{
	FILETIME CreateTime1;
	FILETIME ExitTime1;
	FILETIME KernelTime1;
	FILETIME UserTime1;

	if (GetProcessTimes(_pProcess, &CreateTime1, &ExitTime1, &KernelTime1, &UserTime1))
	{
		NTime::CTime CreateTime = fg_Win32_FileTimeToMalterlibTime(CreateTime1);
		NTime::CTime ExitTime = fg_Win32_FileTimeToMalterlibTime(ExitTime1);
		NTime::CTimeSpan KernelTime = fg_Win32_FileTimeToMalterlibTimeSpan(KernelTime1);
		NTime::CTimeSpan UserTime = fg_Win32_FileTimeToMalterlibTimeSpan(UserTime1);
		NTime::CTimeSpan RunTime = ExitTime - CreateTime;

		fp64 KernelSeconds = KernelTime.f_GetSecondsFraction();
		fp64 UserSeconds = UserTime.f_GetSecondsFraction();
		fp64 RunSeconds = RunTime.f_GetSecondsFraction();

		_Stats.m_Statistics("CPU utilization Total", CProcessStat(EProcessStatUnit_Fraction, (KernelSeconds + UserSeconds) / RunSeconds));
		_Stats.m_Statistics("CPU utilization User", CProcessStat(EProcessStatUnit_Fraction, UserSeconds / RunSeconds));
		_Stats.m_Statistics("CPU utilization Kernel", CProcessStat(EProcessStatUnit_Fraction, KernelSeconds / RunSeconds));
	}
}


uint64 NSys::fg_Process_GetPhysicalMemory()
{
	MEMORYSTATUSEX MemoryStatus;
	MemoryStatus.dwLength = sizeof(MemoryStatus);
	GlobalMemoryStatusEx(&MemoryStatus);
	uint64 Max = MemoryStatus.ullTotalPhys;
	return Max;
}

NMib::NStr::CStr NSys::fg_System_GetCPUName()
{
	NRuntimeMSVC::CWin32_Registry Registry(NRuntimeMSVC::CWin32_Registry::ERegRoot_LocalMachine);
	if (Registry.f_ValueExists("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "ProcessorNameString"))
	{
		return Registry.f_Read_Str("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "ProcessorNameString");
	}

	return "Unknown";
}


mint NSys::fg_Thread_GetPhysicalCores()
{
	HMODULE pKernel32 = g_hKernel32;
	if (pKernel32)
	{
		if (NLocal::g_fGetLogicalProcessorInformation)
		{
			DWORD BufferLength = 0;
			TCVector<uint8> Buffer;
			bint bDone = false;
			bint bError = false;
			while (!bDone) 
			{
				BufferLength = Buffer.f_GetLen();
				bint bRet = NLocal::g_fGetLogicalProcessorInformation((PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)Buffer.f_GetArray(), &BufferLength);

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
	SYSTEM_INFO Info;
	GetSystemInfo(&Info);
	return Info.dwNumberOfProcessors;
}

mint NSys::fg_Thread_GetVirtualCores()
{
	SYSTEM_INFO Info;
	GetSystemInfo(&Info);
	return Info.dwNumberOfProcessors;
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

bint NSys::fg_Semaphore_WaitTimeout(void * _pSemaphore, fp64 _Timeout)
{
	if (_Timeout < 0)
		return WaitForSingleObjectEx(_pSemaphore, ((-_Timeout) * 1000.0 / fg_GetSys()->f_GetTimeSpeed()).f_Ceil().f_ToInt(), true) != WAIT_OBJECT_0;
	else
		return WaitForSingleObjectEx(_pSemaphore, (_Timeout * 1000.0 / fg_GetSys()->f_GetTimeSpeed()).f_Ceil().f_ToInt(), false) != WAIT_OBJECT_0;
}

bint NSys::fg_Semaphore_TryWait(void * _pSemaphore)
{
	return WaitForSingleObject(_pSemaphore, 0) == WAIT_OBJECT_0;
}


void *NSys::fg_Event_Alloc(bint _InitialSignal)
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

bint NSys::fg_Event_WaitTimeout(void * _pEvent, fp64 _Timeout)
{
	return WaitForSingleObject(_pEvent, (_Timeout * 1000.0 / fg_GetSys()->f_GetTimeSpeed()).f_Ceil().f_ToInt()) != WAIT_OBJECT_0;
}

bint NSys::fg_Event_TryWait(void * _pEvent)
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

	bint bCopy = !NMib::NStr::fg_StrCmpNoCase(_pMessageType, "Copy");

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

bint NSys::NFile::fg_FileExists(const CStr &_FileName, EFileAttrib _AttribMask)
{
	if (_FileName.f_IsEmpty())
		return false;
	CStr Drive = NMib::NFile::CFile::fs_GetDrive(_FileName);
	CWStr Temp = fg_ConvertToWindowsPathLocal(_FileName);
	CWStr DriveW = fg_ConvertToWindowsPathLocal(Drive);
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
		{
			return false;
		}

		if ((Attribs & FILE_ATTRIBUTE_DIRECTORY) && (_AttribMask & NMib::NFile::EFileAttrib_Directory))
			return true;

		if (!(Attribs & FILE_ATTRIBUTE_DIRECTORY) && (_AttribMask & NMib::NFile::EFileAttrib_File))
			return true;
		return false;
	}
}

bint NSys::NFile::fg_FileExists(const CStrNonTracked &_FileName, EFileAttrib _AttribMask)
{
	if (_FileName.f_IsEmpty())
		return false;
	CStrNonTracked Drive = NMib::NFile::CFile::fs_GetDrive(_FileName);
	CWStrNonTracked Temp = fg_ConvertToWindowsPathLocal(_FileName);
	CWStrNonTracked DriveW = fg_ConvertToWindowsPathLocal(Drive);
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
		{
			return false;
		}

		if ((Attribs & FILE_ATTRIBUTE_DIRECTORY) && (_AttribMask & NMib::NFile::EFileAttrib_Directory))
			return true;

		if (!(Attribs & FILE_ATTRIBUTE_DIRECTORY) && (_AttribMask & NMib::NFile::EFileAttrib_File))
			return true;
		return false;
	}
}


// From: http://blog.aaronballman.com/2011/08/how-to-check-access-rights/
ECheckFileRights NSys::NFile::fg_CheckFileRights( const CStr & _File, NMib::NFile::EFileRight _Rights)
{
	DWORD GenericRights = 		((_Rights & EFileRight_Read) ? GENERIC_READ : 0)
							|	((_Rights & EFileRight_Write) ? GENERIC_WRITE : 0)
							|	((_Rights & EFileRight_Execute) ? GENERIC_EXECUTE : 0);

	CWStr Path;
	Path = fg_ConvertToWindowsPathLocal<CWStr, CWStr>(_File);

	bint bRet = false;
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

template <typename tf_CWinStr, typename tf_CErrorStr, typename tf_CStr>
void *fg_OpenHelper(const tf_CStr &_FileName, NMib::NFile::EFileOpen _OpenFlags)
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
		FileName = fg_StrToWindows<tf_CWinStr>(_FileName);
	else
		FileName = fg_ConvertToWindowsPathLocal<tf_CWinStr, tf_CWinStr>(_FileName);	

	void *pFile = CreateFileW(FileName, OpenFlags, ShareFlags, nullptr, CreateDisposition, FlagsAndAttribs, nullptr);

	if (pFile == INVALID_HANDLE_VALUE)
	{
		DMibErrorFile
			(
				(
					tf_CErrorStr::CFormat("Windows returned an error from CreateFile({}, 0x{nfh,sf0,sj8}, 0x{nfh,sf0,sj8}, {}, 0x{nfh,sf0,sj8}): {}") 
					<< FileName 
					<< OpenFlags 
					<< ShareFlags
					<< CreateDisposition
					<< FlagsAndAttribs
					<< fg_Win32_GetLastErrorStr()
				).f_GetStr()
			)
		;
	}

	TCUniquePointer<TCWin32File<tf_CWinStr, typename tf_CWinStr::CAllocator>, typename tf_CWinStr::CAllocator> pNewFile = fg_Construct(FileName);
	pNewFile->m_pFile = pFile;
	pNewFile->m_Flags = _OpenFlags;
	return pNewFile.f_Detach();
}

void *NSys::NFile::fg_Open(const CStr &_FileName, NMib::NFile::EFileOpen _OpenFlags)
{
	return fg_OpenHelper<CWStr, CStr>(_FileName, _OpenFlags);
}

void *NSys::NFile::fg_Open(const CStrNonTracked &_FileName, NMib::NFile::EFileOpen _OpenFlags)
{
	return fg_OpenHelper<CWStrNonTracked, CStrNonTracked>(_FileName, _OpenFlags);
}

void NSys::NFile::fg_Close(void *_pFile)
{
	if (!CloseHandle(((CWin32File *)_pFile)->m_pFile))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from CloseHandle: {}") << fg_Win32_GetLastErrorStr()).f_GetStr());
	}
	((CWin32File *)_pFile)->f_Delete();
}

CMibFilePos NMib::NSys::NFile::fg_GetSize(const NMib::NStr::CStr &_FileName)
{
	WIN32_FIND_DATAW FindData;
	void *pFindHandle;
	pFindHandle = FindFirstFileW(fg_ConvertToWindowsPathLocal(_FileName), &FindData);

	if (pFindHandle != INVALID_HANDLE_VALUE)
	{
		FindClose(pFindHandle);
		return (CMibFilePos(FindData.nFileSizeHigh) << 32) | CMibFilePos(FindData.nFileSizeLow);
	}
	DMibErrorFile((CStr::CFormat("Windows returned an error from FindFirstFile({}): {}") << _FileName << fg_Win32_GetLastErrorStr()).f_GetStr());
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
			DMibErrorFile((CStr::CFormat("Windows returned an error from SetFilePointerEx({}): {}") << ((CWin32File *)_pFile)->f_GetName() << fg_Win32_GetLastErrorStr()).f_GetStr());
		}

		if (NewOffset != _Offset)
		{
			DMibErrorFile("Failed to move file pointer to read location");
		}
	}

	if (!ReadFile(((CWin32File *)_pFile)->m_pFile, _pData, _NumBytes, &BytesRead, nullptr))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from ReadFile({}): {}") << ((CWin32File *)_pFile)->f_GetName() << fg_Win32_GetLastErrorStr()).f_GetStr());
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
			DMibErrorFile((CStr::CFormat("Windows returned an error from SetFilePointerEx({}): {}") << ((CWin32File *)_pFile)->f_GetName() << fg_Win32_GetLastErrorStr()).f_GetStr());
		}

		if (NewOffset != _Offset)
		{
			DMibErrorFile("Failed to move file pointer to write location");
		}
	}

	if (!WriteFile(((CWin32File *)_pFile)->m_pFile, _pData, _NumBytes, &BytesWritten, nullptr))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from WriteFile({}): {}") << ((CWin32File *)_pFile)->f_GetName() << fg_Win32_GetLastErrorStr()).f_GetStr());
	}

	return BytesWritten;
}

void NSys::NFile::fg_Flush(void *_pFile)
{
	if (!FlushFileBuffers(((CWin32File *)_pFile)->m_pFile))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from FlushFileBuffers({}): {}") << ((CWin32File *)_pFile)->f_GetName() << fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}

void NSys::NFile::fg_LockRange(void *_pFile, const CMibFilePos &_Offset, const CMibFilePos &_NumBytes, NMib::NFile::EFileLock _Flags)
{
	OVERLAPPED Overlapped;
	NMib::NMem::fg_MemClear(Overlapped);
	Overlapped.Offset = _Offset & 0xffffffffll;
	Overlapped.OffsetHigh = (_Offset >> 32) & 0xffffffffll;
	uint32 Flags = 0;
	if (!(Flags & NMib::NFile::EFileLock_Block))
		Flags |= LOCKFILE_FAIL_IMMEDIATELY;
	if (Flags & NMib::NFile::EFileLock_PreventRead)
		Flags |= LOCKFILE_EXCLUSIVE_LOCK;
	if (!LockFileEx(((CWin32File *)_pFile)->m_pFile, Flags, 0, _NumBytes & 0xffffffffll, (_NumBytes >> 32) & 0xffffffffll, &Overlapped))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from LockFileEx({}): {}") << ((CWin32File *)_pFile)->f_GetName() << fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}

void NSys::NFile::fg_UnlockRange(void *_pFile, const CMibFilePos &_Offset, const CMibFilePos &_NumBytes)
{
	OVERLAPPED Overlapped;
	NMib::NMem::fg_MemClear(Overlapped);
	Overlapped.Offset = _Offset & 0xffffffffll;
	Overlapped.OffsetHigh = (_Offset >> 32) & 0xffffffffll;
	if (!UnlockFileEx(((CWin32File *)_pFile)->m_pFile, 0, _NumBytes & 0xffffffffll, (_NumBytes >> 32) & 0xffffffffll, &Overlapped))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from UnlockFileEx({}): {}") << ((CWin32File *)_pFile)->f_GetName() << fg_Win32_GetLastErrorStr()).f_GetStr());
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
	template <typename tf_CRet, typename tf_CSrc>
	tf_CRet fg_ConvertFromWindowsPathInternal(const tf_CSrc &_Path)
	{
		auto ToRet = _Path;
		fg_StrReplaceChar(ToRet, '\\', '/');
	
		if (ToRet.f_CmpNoCase("//?/UNC/", 8) == 0)
		{
			return "//" + ToRet.f_Extract(8);
		}
		else if (ToRet.f_CmpNoCase("//?/", 4) == 0)
		{
			return ToRet.f_Extract(4);
		}

		return ToRet.f_TrimRight();
	}

	template <typename tf_CWinStr, typename tf_CStr>
	void fg_SetAttributesInternal(ch16 const *_pFileName, EFileAttrib _Attributes)
	{
		EFileAttrib ExtraAttributes = _Attributes;
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
			DMibErrorFile((tf_CStr::CFormat("Windows returned an error from SetFileAttributesW({}): {}") << _pFileName << fg_Win32_GetLastErrorStr()).f_GetStr());

		tf_CWinStr OriginalFileName(_pFileName);
		// This needs to be named exactly like this to be compatible with old version of library (when Malterlib was named Ids)
		auto ExtendedAttribName = OriginalFileName + ":IdsExtAttribs:$DATA";
		if (OriginalFileName.f_GetLen() < 260 && ExtendedAttribName.f_GetLen() >= 260)
			ExtendedAttribName = fg_ConvertToWindowsPathLocal(fg_ConvertFromWindowsPathInternal<tf_CWinStr>(ExtendedAttribName));

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
					DMibErrorFile((tf_CStr::CFormat("Windows returned an error from SetFileAttributesW({}): {}") << _pFileName << fg_Win32_GetLastErrorStr()).f_GetStr());
			}
		}
		else
		{
			uint32 Attribs = GetFileAttributesW(ExtendedAttribName);
			if (Attribs != INVALID_FILE_ATTRIBUTES)
			{
				if (!DeleteFileW(fg_ConvertToWindowsPathLocal(ExtendedAttribName)))
					DMibErrorFile((CStr::CFormat("Windows returned an error from DeleteFile({}): {}") << ExtendedAttribName << fg_Win32_GetLastErrorStr()).f_GetStr());
			}
		}


	}

	template <typename tf_CWinStr, typename tf_CStr>
	EFileAttrib fg_GetAttributesInternal(ch16 const *_pFileName, bool _bThrow = true)
	{
		uint32 FileAttribs = GetFileAttributesW(_pFileName);
		if (FileAttribs == INVALID_FILE_ATTRIBUTES)
		{
			if (_bThrow)
				DMibErrorFile((tf_CStr::CFormat("Windows returned an error from GetFileAttributes({}): {}") << _pFileName << fg_Win32_GetLastErrorStr()).f_GetStr());
			else 
				return EFileAttrib_None;
		}

		uint32 MalterlibAttr = 0;
		if (FileAttribs & FILE_ATTRIBUTE_DIRECTORY)
			MalterlibAttr |= NMib::NFile::EFileAttrib_Directory;
		if (FileAttribs & FILE_ATTRIBUTE_REPARSE_POINT)
			MalterlibAttr |= NMib::NFile::EFileAttrib_Link;
		if (FileAttribs & FILE_ATTRIBUTE_HIDDEN)
			MalterlibAttr |= NMib::NFile::EFileAttrib_Hidden;
		if (FileAttribs & FILE_ATTRIBUTE_READONLY)
			MalterlibAttr |= NMib::NFile::EFileAttrib_ReadOnly;
		if (FileAttribs & FILE_ATTRIBUTE_SYSTEM)
			MalterlibAttr |= NMib::NFile::EFileAttrib_System;
		if (FileAttribs & FILE_ATTRIBUTE_ARCHIVE)
			MalterlibAttr |= NMib::NFile::EFileAttrib_Archive;

		tf_CWinStr OriginalFileName(_pFileName);
		auto ExtendedAttribName = OriginalFileName + ":IdsExtAttribs:$DATA";
		if (OriginalFileName.f_GetLen() < 260 && ExtendedAttribName.f_GetLen() >= 260)
			ExtendedAttribName = fg_ConvertToWindowsPathLocal(fg_ConvertFromWindowsPathInternal<tf_CWinStr>(ExtendedAttribName));

		uint32 Attribs = GetFileAttributesW(ExtendedAttribName);
		if (Attribs != INVALID_FILE_ATTRIBUTES)
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

		return (EFileAttrib)MalterlibAttr;
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
	fg_SetAttributesInternal<CWStr, CStr>(fg_ConvertToWindowsPath(_FileName, false), _Attributes);
}


EFileAttrib NSys::NFile::fg_GetAttributes(void *_pFile)
{
	auto *pFile = ((CWin32File *)_pFile);
	if (pFile->f_IsNonTracked())
		return fg_GetAttributesInternal<CWStrNonTracked, CStrNonTracked>(pFile->f_GetName());
	else
		return fg_GetAttributesInternal<CWStr, CStr>(pFile->f_GetName());
}


EFileAttrib NSys::NFile::fg_GetAttributes(NMib::NStr::CStr const& _FileName)
{
	return fg_GetAttributesInternal<CWStr, CStr>(fg_ConvertToWindowsPathLocal(_FileName));
}


CMibFilePos NSys::NFile::fg_GetSize(void *_pFile)
{
	CMibFilePos Ret;
	if (!GetFileSizeEx(((CWin32File *)_pFile)->m_pFile, (LARGE_INTEGER *)&Ret))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from GetFileSizeEx({}): {}") << ((CWin32File *)_pFile)->f_GetName() << fg_Win32_GetLastErrorStr()).f_GetStr());
	}
	return Ret;
}

void NSys::NFile::fg_SetSize(void *_pFile, const CMibFilePos &_Size)
{
	CMibFilePos NewOffset;
	
	if (!SetFilePointerEx(((CWin32File *)_pFile)->m_pFile, *((LARGE_INTEGER *)&_Size), (LARGE_INTEGER *)&NewOffset, FILE_BEGIN))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from SetFilePointerEx({}): {}") << ((CWin32File *)_pFile)->f_GetName() << fg_Win32_GetLastErrorStr()).f_GetStr());
	}

	if (NewOffset != _Size)
	{
		DMibErrorFile("Failed to move file pointer to file size location");
	}

	if (!SetEndOfFile(((CWin32File *)_pFile)->m_pFile))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from SetEndOfFile({}): {}") << ((CWin32File *)_pFile)->f_GetName() << fg_Win32_GetLastErrorStr()).f_GetStr());
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
		pFile = NSys::NFile::fg_Open(TestFileName, NMib::NFile::EFileOpen_Write);
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

void fg_Win32_MalterlibTimeToFileTime(const NTime::CTime &_Time, FILETIME &_FileTime)
{
	NTime::CTime BaseTime = fg_GetLocalSys()->m_FileTimeBase;
	NTime::CTimeSpan FileTimeSpan = _Time - BaseTime;

	LARGE_INTEGER Temp;
	Temp.QuadPart = FileTimeSpan.f_GetSeconds() * 10000000;
	Temp.QuadPart += (FileTimeSpan.f_GetFraction() * 10000000.0).f_ToInt();

	_FileTime.dwHighDateTime = Temp.HighPart;
	_FileTime.dwLowDateTime = Temp.LowPart;
}

NTime::CTimeSpan fg_Win32_FileTimeToMalterlibTimeSpan(FILETIME &_FileTime)
{
	LARGE_INTEGER Temp;
	Temp.HighPart = _FileTime.dwHighDateTime;
	Temp.LowPart = _FileTime.dwLowDateTime;

	uint64 Nano100 = Temp.QuadPart;
	uint64 nSeconds = Nano100 / 10000000;
	NTime::CTimeSpan FileTimeSpan;
	FileTimeSpan.f_SetSeconds(nSeconds);
	FileTimeSpan.f_SetFraction(fp64(Nano100 % 10000000) / fp64(10000000.0));

	return FileTimeSpan;
}

NTime::CTime fg_Win32_FileTimeToMalterlibTime(FILETIME &_FileTime)
{
	NTime::CTime BaseTime = fg_GetLocalSys()->m_FileTimeBase;

	return BaseTime + fg_Win32_FileTimeToMalterlibTimeSpan(_FileTime);
}

static void fsg_MalterlibTimeToSystemTime_(const NTime::CTime &_Time, SYSTEMTIME &_SysTime)
{
	NTime::CTimeConvert::CDateTime DateTime;
	NTime::CTimeConvert(_Time).f_ExtractDateTime(DateTime);
	
	_SysTime.wYear = DateTime.m_Year;
	_SysTime.wMonth = DateTime.m_Month;
	_SysTime.wDayOfWeek = DateTime.m_DayOfWeek;
	_SysTime.wDay = DateTime.m_DayOfMonth;
	_SysTime.wHour = DateTime.m_Hour;
	_SysTime.wMinute = DateTime.m_Minute;
	_SysTime.wSecond = DateTime.m_Second;
	_SysTime.wMilliseconds = (DateTime.m_Fraction * 1000.0).f_ToInt();
}

void NSys::NFile::fg_SetCreationTime(void *_pFile, const NTime::CTime &_Time)
{
	FILETIME Time;
	fg_Win32_MalterlibTimeToFileTime(_Time, Time);

	if (!SetFileTime(((CWin32File *)_pFile)->m_pFile, &Time, nullptr, nullptr))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from SetFileTime({}): {}") << ((CWin32File *)_pFile)->f_GetName() << fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}

void NSys::NFile::fg_SetAccessTime(void *_pFile, const NTime::CTime &_Time)
{
	FILETIME Time;
	fg_Win32_MalterlibTimeToFileTime(_Time, Time);

	if (!SetFileTime(((CWin32File *)_pFile)->m_pFile, nullptr, &Time, nullptr))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from SetFileTime({}): {}") << ((CWin32File *)_pFile)->f_GetName() << fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}

void NSys::NFile::fg_SetWriteTime(void *_pFile, const NTime::CTime &_Time)
{
	FILETIME Time;
	fg_Win32_MalterlibTimeToFileTime(_Time, Time);

	if (!SetFileTime(((CWin32File *)_pFile)->m_pFile, nullptr, nullptr, &Time))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from SetFileTime({}): {}") << ((CWin32File *)_pFile)->f_GetName() << fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}

NTime::CTime NSys::NFile::fg_GetCreationTime(void *_pFile)
{
	FILETIME Time;
	if (!GetFileTime(((CWin32File *)_pFile)->m_pFile, &Time, nullptr, nullptr))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from GetFileTime({}): {}") << ((CWin32File *)_pFile)->f_GetName() << fg_Win32_GetLastErrorStr()).f_GetStr());
	}

	return fg_Win32_FileTimeToMalterlibTime(Time);
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

NTime::CTime NSys::NFile::fg_GetAccessTime(void *_pFile)
{
	FILETIME Time;
	if (!GetFileTime(((CWin32File *)_pFile)->m_pFile, nullptr, &Time, nullptr))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from GetFileTime({}): {}") << ((CWin32File *)_pFile)->f_GetName() << fg_Win32_GetLastErrorStr()).f_GetStr());
	}

	return fg_Win32_FileTimeToMalterlibTime(Time);
}

NTime::CTime NSys::NFile::fg_GetWriteTime(void *_pFile)
{
	FILETIME Time;
	if (!GetFileTime(((CWin32File *)_pFile)->m_pFile, nullptr, nullptr, &Time))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from GetFileTime({}): {}") << ((CWin32File *)_pFile)->f_GetName() << fg_Win32_GetLastErrorStr()).f_GetStr());
	}

	return fg_Win32_FileTimeToMalterlibTime(Time);
}


void *NSys::NFile::fg_ChangeNotification_Open(const CStr &_FileName, NMib::NFile::EFileChange _OpenFlags, NMib::NThread::CSemaphoreReportableAggregate *_pReportTo)
{
	return fg_GetLocalSys()->m_FileChangeNoticationContext->f_Open(_FileName, _OpenFlags, _pReportTo);
}

void NSys::NFile::fg_ChangeNotification_Close(void *_pNotification)
{
	fg_GetLocalSys()->m_FileChangeNoticationContext->f_Close(_pNotification);
}

bint NSys::NFile::fg_ChangeNotification_Changed(void *_pNotification)
{
	return fg_GetLocalSys()->m_FileChangeNoticationContext->f_Changed(_pNotification);
}

bint NSys::NFile::fg_ChangeNotification_GetNotification(void *_pNotification, NMib::NStr::CStr &_Path, NMib::NFile::EFileChangeNotification &_Notification)
{
	return fg_GetLocalSys()->m_FileChangeNoticationContext->f_GetNotification(_pNotification, _Path, _Notification);
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

		EFileAttrib MalterlibAttr = EFileAttrib_None;
		if (FileAttribs & FILE_ATTRIBUTE_DIRECTORY)
			MalterlibAttr |= NMib::NFile::EFileAttrib_Directory;
		else 
			MalterlibAttr |= NMib::NFile::EFileAttrib_File;

		if (FileAttribs & FILE_ATTRIBUTE_REPARSE_POINT)
			MalterlibAttr |= NMib::NFile::EFileAttrib_Link;

		if (FileAttribs & FILE_ATTRIBUTE_HIDDEN)
			MalterlibAttr |= NMib::NFile::EFileAttrib_Hidden;

		if (FileAttribs & FILE_ATTRIBUTE_READONLY)
			MalterlibAttr |= NMib::NFile::EFileAttrib_ReadOnly;

		if (FileAttribs & FILE_ATTRIBUTE_SYSTEM)
			MalterlibAttr |= NMib::NFile::EFileAttrib_System;

		CWStr OriginalFileName = m_FullPath + m_FindData.cFileName;
		auto ExtendedAttribName = OriginalFileName + ":IdsExtAttribs:$DATA";
		if (OriginalFileName.f_GetLen() < 260 && ExtendedAttribName.f_GetLen() >= 260)
			ExtendedAttribName = fg_ConvertToWindowsPathLocal(fg_ConvertFromWindowsPathInternal<CWStr>(ExtendedAttribName));

		uint32 Attribs = GetFileAttributesW(ExtendedAttribName);
		if (Attribs != INVALID_FILE_ATTRIBUTES)
		{
			TCBinaryStreamFile<> Stream;
			Stream.f_Open(CStr(ExtendedAttribName), EFileOpen_Read | EFileOpen_ShareAll | EFileOpen_RawFileName);
			if (Stream.f_GetLength())
			{
				CMalterlibExtendedAttributes OldAttribs;
				Stream >> OldAttribs;

				MalterlibAttr |= OldAttribs.m_ExtendedAttributes;
			}
		}

		return MalterlibAttr;
	}

	CWStr m_FullPath;
	CStr m_LastFullName;
	WIN32_FIND_DATAW m_FindData;
	void *m_pFindHandle;
	mint m_Mode;
};

	//fs_GetExpandedPath


template <typename tf_CWindows, typename tf_CRet, typename tf_CSource>
tf_CRet fg_ConvertToWindowsPath(const tf_CSource &_Path, bint _bAddCurrentDir, aint _MaxLen, bool _bTryShorten)
{
	if (_Path.f_IsEmpty())
		return tf_CRet();

	if (_Path.f_StartsWith("//."))
	{
		return _Path.f_ReplaceChar('/', '\\');
	}
	auto ToRet = NFile::CFile::fs_GetExpandedPath(_Path, _bAddCurrentDir);
	fg_StrReplaceChar(ToRet, '\\', '/');

	if (ToRet.f_Cmp("//?/", 4) == 0)
	{
		ToRet = fg_ConvertFromWindowsPath<tf_CWindows, tf_CRet>(ToRet);
	}

	auto ToRetW = fg_StrToWindows<tf_CWindows>(ToRet);
	fg_StrReplaceChar(ToRetW, '/', '\\');

	aint MaxLenToUse = _MaxLen;
	if (MaxLenToUse > _MAX_PATH)
		MaxLenToUse = _MAX_PATH;

	if ((_MaxLen < 0 || ToRetW.f_GetLen() >= MaxLenToUse) && ((ToRet.f_GetLen() > 1 && ToRet[1] == ':') || ToRet.f_Cmp("\\\\", 2) != 0))
	{
		auto Path = fg_StrToWindows<tf_CWindows>(NFile::CFile::fs_GetPath(ToRet));
		auto File = fg_StrToWindows<tf_CWindows>(NFile::CFile::fs_GetFile(ToRet));

		if (_MaxLen > 0 && _bTryShorten)
		{
			auto TempW = fg_StrToWindows<tf_CWindows>(Path);
			fg_StrReplaceChar(TempW, '/', '\\');
			if (TempW.f_Cmp("\\\\", 2) == 0)
				TempW = "\\\\?\\UNC\\" + TempW.f_Extract(2);
			else
				TempW = "\\\\?\\" + TempW;
			mint NeededLen = GetShortPathNameW(TempW, nullptr, 0);
			if (NeededLen)
			{
				tf_CWindows ShortPathW;
				mint NeededLen2 = GetShortPathNameW(TempW, ShortPathW.f_GetStr(NeededLen), NeededLen);
				NeededLen2;
				DMibSafeCheck(NeededLen2 <= NeededLen, "");
				TempW = ShortPathW + "\\" + File;
				if (TempW.f_CmpNoCase("\\\\?\\UNC\\", 8) == 0)
				{
					TempW = "\\\\" + TempW.f_Extract(8);
				}
				else if (TempW.f_CmpNoCase("\\\\?\\", 4) == 0)
				{
					TempW = TempW.f_Extract(4);
				}

				if (TempW.f_GetLen() < _MaxLen)
					return TempW;
			}
		}

		if (ToRetW.f_Cmp("\\\\", 2) == 0)
			ToRetW = "\\\\?\\UNC\\" + ToRetW.f_Extract(2);
		else
			ToRetW = "\\\\?\\" + ToRetW;

		if (_MaxLen > 0 && _bTryShorten)
		{
			mint NeededLen = GetShortPathNameW(ToRetW, nullptr, 0);
			if (NeededLen)
			{
				tf_CWindows ShortPathW;
				mint NeededLen2 = GetShortPathNameW(ToRetW, ShortPathW.f_GetStr(NeededLen), NeededLen);
				NeededLen2;
				DMibSafeCheck(NeededLen2 <= NeededLen, "");
				auto TempW = ShortPathW;
				if (TempW.f_CmpNoCase("\\\\?\\UNC\\", 8) == 0)
				{
					TempW = "\\\\" + TempW.f_Extract(8);
				}
				else if (TempW.f_CmpNoCase("\\\\?\\", 4) == 0)
				{
					TempW = TempW.f_Extract(4);
				}
				if (TempW.f_GetLen() < _MaxLen)
					return TempW;
			}
		}

		return ToRetW;
	}

	return ToRetW;
}

template <typename tf_CWindows, typename tf_CRet, typename tf_CSource>
tf_CRet fg_ConvertToShortWindowsPath(const tf_CSource &_Path, bint _bAddCurrentDir)
{
	if (_Path.f_IsEmpty())
		return tf_CRet();
	if (_Path.f_StartsWith("//."))
	{
		return _Path.f_ReplaceChar('/', '\\');
	}

	auto ToRet = NFile::CFile::fs_GetExpandedPath(_Path, _bAddCurrentDir);
	fg_StrReplaceChar(ToRet, '\\', '/');

	if (ToRet.f_Cmp("//?/", 4) == 0)
	{
		ToRet = fg_ConvertFromWindowsPath<tf_CWindows, tf_CRet>(ToRet);
	}

	auto ToRetW = fg_StrToWindows<tf_CWindows>(ToRet);
	fg_StrReplaceChar(ToRetW, '/', '\\');
	mint NeededLen = GetShortPathNameW(ToRetW, nullptr, 0);
	if (NeededLen)
	{
		tf_CWindows ShortPathW;
		mint NeededLen2 = GetShortPathNameW(ToRetW, ShortPathW.f_GetStr(NeededLen), NeededLen);
		NeededLen2;
		DMibSafeCheck(NeededLen2 <= NeededLen, "");
		auto TempW = ShortPathW;
		if (TempW.f_CmpNoCase("\\\\?\\UNC\\", 8) == 0)
		{
			TempW = "\\\\" + TempW.f_Extract(8);
		}
		else if (TempW.f_CmpNoCase("\\\\?\\", 4) == 0)
		{
			TempW = TempW.f_Extract(4);
		}
		return TempW;
	}
	return ToRetW;
}


CWStr fg_ConvertToShortWindowsPath(const CStr &_Path, bint _bAddCurrentDir)
{
	return fg_ConvertToShortWindowsPath<CWStr, CWStr>(_Path, _bAddCurrentDir);
}

template <typename tf_CWindows, typename tf_CRet, typename tf_CSource>
tf_CRet fg_ConvertToLongWindowsPath(const tf_CSource &_Path, bint _bAddCurrentDir)
{
	if (_Path.f_IsEmpty())
		return tf_CRet();
	auto ToRet = NFile::CFile::fs_GetExpandedPath(_Path, _bAddCurrentDir);
	fg_StrReplaceChar(ToRet, '\\', '/');

	if (ToRet.f_Cmp("//?/", 4) == 0)
	{
		ToRet = fg_ConvertFromWindowsPath<tf_CWindows, tf_CRet>(ToRet);
	}

	auto ToRetW = fg_StrToWindows<tf_CWindows>(ToRet);
	fg_StrReplaceChar(ToRetW, '/', '\\');
	mint NeededLen = GetLongPathNameW(ToRetW, nullptr, 0);
	if (NeededLen)
	{
		tf_CWindows LongPathW;
		mint NeededLen2 = GetLongPathNameW(ToRetW, LongPathW.f_GetStr(NeededLen), NeededLen);
		NeededLen2;
		DMibSafeCheck(NeededLen2 <= NeededLen, "");
		return LongPathW;
	}
	return ToRetW;
}


CWStr fg_ConvertToLongWindowsPath(const CStr &_Path, bint _bAddCurrentDir)
{
	return fg_ConvertToLongWindowsPath<CWStr, CWStr>(_Path, _bAddCurrentDir);
}

CWStr fg_ConvertToWindowsPath(const CStr &_Path, bint _bAddCurrentDir, aint _MaxLen)
{
	return fg_ConvertToWindowsPath<CWStr, CWStr>(_Path, _bAddCurrentDir, _MaxLen, true);
}

CWStr fg_ConvertToWindowsPath(const CStr &_Path, bint _bAddCurrentDir, aint _MaxLen, bool _bTryShorten)
{
	return fg_ConvertToWindowsPath<CWStr, CWStr>(_Path, _bAddCurrentDir, _MaxLen, _bTryShorten);
}

CStr fg_StrFromWindowsAnsi(const CAnsiStr &_Str)
{
	CStr To;
	NSys::NStr::fg_SystemDecodeAnsiStr(_Str, To);
	return To;
}

CStr fg_ConvertFromWindowsPath(const CWStr &_Path)
{
	CStr ToRet = fg_StrFromWindows(_Path);
	return fg_ConvertFromWindowsPath(ToRet);
}

CWStr fg_StrToWindows(const CStr &_Str)
{
	CWStr Ret = _Str;
	return Ret;
}

CWStr fg_StrToWindows(const CWStr &_Str)
{
	CWStr Ret = _Str;
	return Ret;
}

CWStr fg_StrToWindows(const CUStr &_Str)
{
	CWStr Ret = _Str;
	return Ret;
}

template <typename tf_CRet, typename tf_CSrc>
tf_CRet fg_StrToWindows(const tf_CSrc &_Str)
{
	tf_CRet Ret = _Str;
	return Ret;
}

CStr fg_StrFromWindows(const CWStr &_Str)
{
	return _Str;
}



HINSTANCE fg_Win32_GetInstance(const void *_pCode)
{
	MEMORY_BASIC_INFORMATION MemInfo;
	if (VirtualQuery(_pCode, &MemInfo, sizeof(MemInfo)))
	{
		return (HINSTANCE)MemInfo.AllocationBase;
	}
	return nullptr;
}

CStr fg_ConvertFromWindowsPath(const CStr &_Path)
{
	return fg_ConvertFromWindowsPathInternal<CStr>(fg_ConvertToWindowsPath<CWStr, CWStr, CStr>(fg_ConvertFromWindowsPathInternal<CStr>(_Path), false, -1));
}


template <typename tf_CWindows, typename tf_CRet, typename tf_CSrc>
tf_CRet fg_ConvertFromWindowsPath(const tf_CSrc &_Path)
{
	return fg_ConvertFromWindowsPathInternal<tf_CRet>(fg_ConvertToWindowsPath<tf_CWindows, tf_CRet>(fg_ConvertFromWindowsPathInternal<tf_CRet>(_Path), false, -1));
}

void *NSys::NFile::fg_FindOpen(const CStr &_FindPattern)
{
	CWStr FindPattern = fg_ConvertToWindowsPathLocal(_FindPattern);

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

	CWin32FileFind *pFind = DMibNew CWin32FileFind;

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
	delete pFind;
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

bint  NSys::fg_System_GetOperatingSystemVersion(int& _oMajor, int& _oMinor, int& _oFix, NProcess::EOperatingSystemArch& _Arch)
{
	OSVERSIONINFOEXW VersionInfo;
	fg_MemClear(VersionInfo);
	VersionInfo.dwOSVersionInfoSize = sizeof(VersionInfo);
	if (!GetVersionExW((OSVERSIONINFO *)&VersionInfo))
		return false;

	_oMajor = VersionInfo.dwMajorVersion;
	_oMinor = VersionInfo.dwMinorVersion;
	_oFix = VersionInfo.dwBuildNumber;

	SYSTEM_INFO SystemInfo;
	GetSystemInfo(&SystemInfo);

	switch (SystemInfo.dwProcessorType)
	{
	case PROCESSOR_ARCHITECTURE_INTEL:
		_Arch = NProcess::EOperatingSystemArch_x86;
		break;
	case PROCESSOR_ARCHITECTURE_AMD64:
	case PROCESSOR_ARCHITECTURE_IA64:
		_Arch = NProcess::EOperatingSystemArch_x64;
		break;
	default:
		_Arch = NProcess::EOperatingSystemArch_Unknown;
		break;
	}

	return true;
}

NMib::NStr::CStr NSys::fg_Process_GetOperatingSystemTag(int32 _MajorMax, int32 _MinorMax)
{
	return "Win32";
}

NMib::NStr::CStr NSys::fg_Process_GetOperatingSystemDescription()
{
	OSVERSIONINFOEXW VersionInfo;
	fg_MemClear(VersionInfo);
	VersionInfo.dwOSVersionInfoSize = sizeof(VersionInfo);
	if (!GetVersionExW((OSVERSIONINFO *)&VersionInfo))
		return "";

	if (VersionInfo.dwMajorVersion == 6)
	{			
		if (VersionInfo.wProductType == VER_NT_WORKSTATION)
		{
			if (VersionInfo.dwMinorVersion == 0)
				return "Windows Vista";
			else if (VersionInfo.dwMinorVersion == 1)
				return "Windows 7";
			else if (VersionInfo.dwMinorVersion == 2)
				return "Windows 8";
			else
				return "Window 8"; // For anything in the future assume Windows 8 compatibility.
		}
		else if (VersionInfo.wProductType == VER_NT_SERVER)
		{ // OS is Windows Server 2008 R2, Windows Server 2008, Windows Server 2003, or Windows 2000 Server.
			if (VersionInfo.dwMinorVersion == 1)
				return "Windows Server 2008 R2";
			else if (VersionInfo.dwMinorVersion >= 2)
				return "Windows Server 2012";
			else
				return "Windows Server 2008";
		}
		else if (VersionInfo.wProductType == VER_NT_DOMAIN_CONTROLLER)
		{ // OS is Windows Server 2008 R2, Windows Server 2008, Windows Server 2003, or Windows 2000 Server.
			return "Windows Vista";
		}
		else
			return  "Windows Unknown";
	}
	if (VersionInfo.dwMajorVersion == 5)
	{
		if (VersionInfo.dwMinorVersion == 0)
			return "Windows 2000";
		else if (VersionInfo.dwMinorVersion == 1)
			return "Windows XP";
		else if (VersionInfo.dwMinorVersion == 2)
		{
			SYSTEM_INFO SystemInfo;
			fg_MemClear(SystemInfo);
			GetNativeSystemInfo(&SystemInfo);

			if (GetSystemMetrics(SM_SERVERR2) != 0)
				return "Windows Server 2003 R2";
			else if (VersionInfo.wSuiteMask & VER_SUITE_WH_SERVER)
				return "Windows Home Server";
			else if (GetSystemMetrics(SM_SERVERR2) == 0)
				return "Windows Server 2003";
			else if ((VersionInfo.wProductType == VER_NT_WORKSTATION) && (SystemInfo.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64))
				return "Windows XP Professional x64 Edition";
			else
				return  "Windows Unknown";
		}
		else
			return  "Windows Unknown";

	}
	else
	{
		return "Windows";
	}

	DMibSafeCheck(false, "This is not implemented on Windows yet");
	return "Windows";
}

void NSys::NFile::fg_Copy(const CStr &_FileFrom, const CStr &_FileTo, NMib::NFile::CFileProgress &_Progress)
{
	BOOL Cancel = false;
	uint32 Flags = 0;
	if (NLocal::g_VersionInfo.dwMajorVersion > 5 || (NLocal::g_VersionInfo.dwMajorVersion == 5 && NLocal::g_VersionInfo.dwMinorVersion >= 1))
		Flags |= COPY_FILE_ALLOW_DECRYPTED_DESTINATION;

	if (!CopyFileExW(fg_ConvertToWindowsPathLocal(_FileFrom), fg_ConvertToWindowsPathLocal(_FileTo), fsg_CopyProgressRoutine, &_Progress, &Cancel, Flags))
		DMibErrorFile((CStr::CFormat("Windows returned an error from CopyFile({}, {}): {}") << _FileFrom << _FileTo << fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NSys::NFile::fg_Copy(const CStr &_FileFrom, const CStr &_FileTo)
{
	if (!CopyFileW(fg_ConvertToWindowsPathLocal(_FileFrom), fg_ConvertToWindowsPathLocal(_FileTo), false))
		DMibErrorFile((CStr::CFormat("Windows returned an error from CopyFile({}, {}): {}") << _FileFrom << _FileTo << fg_Win32_GetLastErrorStr()).f_GetStr());
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
	if (NLocal::g_fCreateSymbolicLinkW)
	{
		if (_Type & EFileAttrib_Directory)
			Flags |= 1;

		CWStr ToMount;
		if (_Flags & ESymbolicLinkFlag_Relative)
			ToMount = CWStr(_FileFrom).f_ReplaceChar('/', '\\');
		else if (_Flags & ESymbolicLinkFlag_ConvertToDevicePath)
		{
			ToMount = L"\\\\?\\GLOBALROOT" + fg_ConvertToDevicePath(_FileFrom);
			//ToMount.f_SetAt(1, '\\');
		}
		else
			ToMount = fg_ConvertToWindowsPathLocal(_FileFrom);

		if (NLocal::g_fCreateSymbolicLinkW(fg_ConvertToWindowsPathLocal(_FileTo), ToMount, Flags))
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
			ToMount = fg_ConvertToDevicePath(_FileFrom);
		else
		{
			ToMount = fg_ConvertToWindowsPathLocal(_FileFrom, true);
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
		REPARSE_DATA_BUFFER *pReparseData = (REPARSE_DATA_BUFFER *)NMem::fg_Alloc(Size);
		fg_MemClear(pReparseData, Size);
		auto Cleanup = fg_OnScopeExit([&]{NMem::fg_Free(pReparseData);});

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
			DMibDTrace("DeviceIoControl(FSCTL_SET_REPARSE_POINT): {}\r\n", fg_Win32_GetLastErrorStr());
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

	if (!NLocal::g_fCreateSymbolicLinkW)
		DMibErrorFile("CreateSymbolicLink is not support on this version of Windows");

	DMibErrorFile((CStr::CFormat("Windows returned an error from CreateSymbolicLinkW({}, {}, {}): {}") << _FileTo << _FileFrom << Flags << fg_Win32_GetLastErrorStr(CreateSymbolicLinkWError)).f_GetStr());
}

NMib::NStr::CStr NSys::NFile::fg_ResolveSymbolicLink(const NMib::NStr::CStr &_FileFrom)
{
//	fg_GetLocalSys()->f_EnableBackupSupport();
	auto Attribs = fg_GetAttributesInternal<CWStr, CStr>(fg_ConvertToWindowsPathLocal(_FileFrom), false);
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

	EFileOpen TargetFileOpenFlags = EFileOpen_Link;
	if (Attribs & EFileAttrib_Directory)
		TargetFileOpenFlags |= EFileOpen_Directory;

	CFile TargetFile;
	TargetFile.f_Open(_FileFrom, TargetFileOpenFlags);

	mint Size = sizeof(REPARSE_DATA_BUFFER) + 65536 * sizeof(ch16);
	REPARSE_DATA_BUFFER *pReparseData = (REPARSE_DATA_BUFFER *)NMem::fg_Alloc(Size);
	fg_MemClear(pReparseData, Size);
	auto Cleanup = fg_OnScopeExit([&]{NMem::fg_Free(pReparseData);});

	mint nIOControlBytes = Size;

	DWORD Return;

	if (!DeviceIoControl(TargetFile.f_GetOSFile(), FSCTL_GET_REPARSE_POINT, nullptr, 0, pReparseData, nIOControlBytes, &Return, nullptr))
	{
		DMibErrorFile((CStr::CFormat("Windows returned an error from DeviceIoControl(FSCTL_GET_REPARSE_POINT, {}): {}") << _FileFrom << fg_Win32_GetLastErrorStr()).f_GetStr());
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
		return fg_ConvertFromWindowsPath(ReturnString);
	return ReturnString.f_ReplaceChar('\\', '/');
}


void NSys::NFile::fg_CreateHardLink(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo)
{
	if (!NLocal::g_fCreateHardLinkW)
		DMibErrorFile("CreateHardLink is not support on this version of Windows");

	if (!NLocal::g_fCreateHardLinkW(fg_ConvertToWindowsPathLocal(_FileTo), fg_ConvertToWindowsPathLocal(_FileFrom), nullptr))
		DMibErrorFile((CStr::CFormat("Windows returned an error from CreateHardLinkW({}, {}): {}") << _FileTo << _FileFrom << fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NSys::NFile::fg_Rename(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo, NMib::NFile::CFileProgress &_Progress)
{
	if (!MoveFileWithProgressW(fg_ConvertToWindowsPathLocal(_FileFrom), fg_ConvertToWindowsPathLocal(_FileTo), fsg_CopyProgressRoutine, &_Progress, MOVEFILE_WRITE_THROUGH | MOVEFILE_COPY_ALLOWED))
		DMibErrorFile((CStr::CFormat("Windows returned an error from MoveFile({}, {}): {}") << _FileFrom << _FileTo << fg_Win32_GetLastErrorStr()).f_GetStr());
}

void NSys::NFile::fg_Rename(const CStr &_FileFrom, const CStr &_FileTo)
{
	if (!MoveFileW(fg_ConvertToWindowsPathLocal(_FileFrom), fg_ConvertToWindowsPathLocal(_FileTo)))
		DMibErrorFile((CStr::CFormat("Windows returned an error from MoveFile({}, {}): {}") << _FileFrom << _FileTo << fg_Win32_GetLastErrorStr()).f_GetStr());
}

NMib::NStream::CFilePos NSys::NFile::fg_GetFreeSpace(const NMib::NStr::CStr &_Path)
{
	ULARGE_INTEGER FreeSpace;
	CWStr Path = fg_ConvertToWindowsPathLocal(_Path);
	if (Path.f_GetAt(Path.f_GetLen() - 1) == ':')
		Path += "\\";
	if (!GetDiskFreeSpaceExW(Path, &FreeSpace, nullptr, nullptr))
		DMibErrorFile((CStr::CFormat("Windows returned an error from GetDiskFreeSpaceExW({}): {}") << Path << fg_Win32_GetLastErrorStr()).f_GetStr());

	return FreeSpace.QuadPart;
}

NMib::NStream::CFilePos NSys::NFile::fg_GetUsedSpace(const NMib::NStr::CStr &_Path)
{
	ULARGE_INTEGER FreeSpace;
	ULARGE_INTEGER TotalSpace;
	CWStr Path = fg_ConvertToWindowsPathLocal(_Path);
	if (Path.f_GetAt(Path.f_GetLen() - 1) == ':')
		Path += "\\";
	if (!GetDiskFreeSpaceExW(Path, &FreeSpace, &TotalSpace, nullptr))
		DMibErrorFile((CStr::CFormat("Windows returned an error from GetDiskFreeSpaceExW({}): {}") << Path << fg_Win32_GetLastErrorStr()).f_GetStr());

	return TotalSpace.QuadPart - FreeSpace.QuadPart;
}


namespace
{
	bint fg_DirectoryExists(const ch16 *_pFileDirectory)
	{
		uint32 Attribs = GetFileAttributesW(_pFileDirectory);

		if (Attribs == INVALID_FILE_ATTRIBUTES)
			return false;

		if ((Attribs & FILE_ATTRIBUTE_DIRECTORY))
			return true;

		return false;
	}
	bint fg_ReparsePointDirectoryExists(const ch16 *_pFileDirectory)
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
	tf_CWinStr NewPath = fg_ConvertToWindowsPath<tf_CWinStr, tf_CWinStr>(_FileDirectory, true, -1, false);

	ch16 *pDir = NewPath.f_GetStrUniqueWritable();

	ch16 *pDirCheck;
	bint bUNC = false;
	if (fg_StrCmpNoCase(pDir, "\\\\?\\UNC\\", 8) == 0)
	{
		pDirCheck = pDir + 8;
		bUNC = true;
	}
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
					DMibErrorFile((tf_CErrorStr::CFormat("Windows returned an error from CreateDirectory({}, {}): {}") << Current << _FileDirectory << fg_Win32_GetLastErrorStr(Error)).f_GetStr());
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

void NSys::NFile::fg_Delete(const CStr &_File)
{
	auto FileName = fg_ConvertToWindowsPathLocal(_File);
	if (!DeleteFileW(FileName))
	{
		if (fg_ReparsePointDirectoryExists(FileName))
			return fg_DeleteDirectory(_File);
		DMibErrorFile((CStr::CFormat("Windows returned an error from DeleteFile({}): {}") << _File << fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}

void NSys::NFile::fg_DeleteDirectory(const CStr &_File)
{
	if (!RemoveDirectoryW(fg_ConvertToWindowsPathLocal(_File)))
		DMibErrorFile((CStr::CFormat("Windows returned an error from RemoveDirectory({}): {}") << _File << fg_Win32_GetLastErrorStr()).f_GetStr());
}


void NSys::NFile::fg_Delete(const CStrNonTracked &_File)
{
	auto FileName = fg_ConvertToWindowsPathLocal<CWStrNonTracked, CWStrNonTracked>(_File);
	if (!DeleteFileW(FileName))
	{
		if (fg_ReparsePointDirectoryExists(FileName))
			return fg_DeleteDirectory(_File);
		DMibErrorFile((CStrNonTracked::CFormat("Windows returned an error from DeleteFile({}): {}") << _File << fg_Win32_GetLastErrorStr()).f_GetStr());
	}
}

void NSys::NFile::fg_DeleteDirectory(const CStrNonTracked &_File)
{
	if (!RemoveDirectoryW(fg_ConvertToWindowsPathLocal<CWStrNonTracked, CWStrNonTracked>(_File)))
		DMibErrorFile((CStrNonTracked::CFormat("Windows returned an error from RemoveDirectory({}): {}") << _File << fg_Win32_GetLastErrorStr()).f_GetStr());
}


void NSys::NFile::fg_SetCurrentDirectory(const NMib::NStr::CStr &_Directory)
{
	if (!SetCurrentDirectoryW(fg_ConvertToWindowsPathLocal(_Directory)))
		DMibErrorFile((CStr::CFormat("Windows returned an error from SetCurrentDirectoryW({}): {}") << _Directory << fg_Win32_GetLastErrorStr(0)).f_GetStr());
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
	if (!GetCurrentDirectoryW(65536, Ret.f_GetStr(65536)))
		DMibErrorFile((CStr::CFormat("Windows returned an error from GetCurrentDirectoryW: {}") << fg_Win32_GetLastErrorStr(0)).f_GetStr());
	Ret.f_SetModified();
	return fg_ConvertFromWindowsPath(Ret);
}

#include <shlobj.h>
NStr::CStr NSys::NFile::fg_GetUserProgramDirectory()
{
	WCHAR szPath[MAX_PATH];
	if(SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA|CSIDL_FLAG_CREATE, nullptr, 0, szPath))) 
	{
		CStr FileName = NMib::NFile::CFile::fs_GetFileNoExt(fg_GetProgramPath());
		aint iFind = FileName.f_FindReverse("_x");
		if (iFind >= 0)
			FileName = FileName.f_Left(iFind);

		CStr Ret = fg_ConvertFromWindowsPath(CWStr(szPath));
		Ret += "/";
		Ret += FileName;
		return Ret;
	}	

	return fg_GetProgramDirectory();
}

NStr::CStr NSys::NFile::fg_GetUserLocalProgramDirectory()
{
	WCHAR szPath[MAX_PATH];
	if(SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA|CSIDL_FLAG_CREATE, nullptr, 0, szPath))) 
	{
		CStr FileName = NMib::NFile::CFile::fs_GetFileNoExt(fg_GetProgramPath());
		aint iFind = FileName.f_FindReverse("_x");
		if (iFind >= 0)
			FileName = FileName.f_Left(iFind);
		CStr Ret = fg_ConvertFromWindowsPath(CWStr(szPath));
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
	if(SUCCEEDED(GetTempPath(MAX_PATH, szPath))) 
	{
		CStr FileName = NMib::NFile::CFile::fs_GetFileNoExt(fg_GetProgramPath());
		aint iFind = FileName.f_FindReverse("_x");
		if (iFind >= 0)
			FileName = FileName.f_Left(iFind);
		CStr Ret = fg_ConvertFromWindowsPath(CWStr(szPath));
		Ret += "/";
		Ret += FileName;
		return Ret;
	}	

	return fg_GetProgramDirectory();
}

NMib::NStr::CStr NSys::NFile::fg_GetUserHomeDirectory()
{
	WCHAR szPath[MAX_PATH];
	if(SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROFILE|CSIDL_FLAG_CREATE, nullptr, 0, szPath))) 
	{
		CStr FileName = NMib::NFile::CFile::fs_GetFileNoExt(fg_GetProgramPath());
		aint iFind = FileName.f_FindReverse("_x");
		if (iFind >= 0)
			FileName = FileName.f_Left(iFind);
		CStr Ret = fg_ConvertFromWindowsPath(CWStr(szPath));
		return Ret;
	}	

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
			DMibError((CStr::CFormat("Windows returned an error from GetModuleFileName(0x{nfh,sj*2,sf0}): {}") << (mint)MemInfo.AllocationBase << fg_Win32_GetLastErrorStr() << sizeof(mint)*2).f_GetStr());
		}
		return fg_ConvertFromWindowsPath(BaseName);
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
	if (!GetCurrentDirectoryW(65536, Ret.f_GetStr(65536)))
		DMibErrorFile((CStr::CFormat("Windows returned an error from GetCurrentDirectoryW: {}") << fg_Win32_GetLastErrorStr(0)).f_GetStr());
	Ret.f_SetModified();
	return fg_ConvertFromWindowsPath<CWStrNonTracked, CStrNonTracked>(Ret);
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

		CStrNonTracked Ret = fg_ConvertFromWindowsPath<CWStrNonTracked, CStrNonTracked>(CWStrNonTracked(szPath));
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
		CStrNonTracked Ret = fg_ConvertFromWindowsPath<CWStrNonTracked, CStrNonTracked>(CWStrNonTracked(szPath));
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
		CStrNonTracked Ret = fg_ConvertFromWindowsPath<CWStrNonTracked, CStrNonTracked>(CStrNonTracked(szPath));
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
		CStrNonTracked Ret = fg_ConvertFromWindowsPath<CWStrNonTracked, CStrNonTracked>(CStrNonTracked(szPath));
		Ret += "/";
		Ret += FileName;
		return Ret;
	}	

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
			DMibError((CStr::CFormat("Windows returned an error from GetModuleFileName(0x{nfh,sj*2,sf0}): {}") << (mint)MemInfo.AllocationBase << fg_Win32_GetLastErrorStr() << sizeof(mint)*2).f_GetStr());
		}
		return fg_ConvertFromWindowsPath<CWStrNonTracked, CStrNonTracked>(BaseName);
	}

	return "";
}


// *************************************************************************************************************************
// Net Implementation
// *************************************************************************************************************************

NSys::NNet::CAddress NSys::NNet::fg_CreateAddress(::NMib::NNet::ENetAddressType _Type, void const* _pData, mint _nDataBytes)
{
	return (NSys::NNet::CAddress)fg_GetLocalSys()->m_SocketContext->f_CreateAddress(_Type, _pData, _nDataBytes);
}

NSys::NNet::CAddress NSys::NNet::fg_DuplicateAddress(NSys::NNet::CAddress _ToCopy)
{
	return (NSys::NNet::CAddress)fg_GetLocalSys()->m_SocketContext->f_DuplicateAddress((CWindowsAddress*)_ToCopy);
}

::NMib::NNet::ENetAddressType NSys::NNet::fg_GetAddressType(NSys::NNet::CAddress _Address)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_GetAddressType(*(CWindowsAddress*)_Address);
}

bint NSys::NNet::fg_GetAddressRaw(NSys::NNet::CAddress _Address, ::NMib::NNet::ENetAddressType _ExpectedType, void* _opRawData, mint _nDataBytes)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_GetAddressRaw(*(CWindowsAddress*)_Address, _ExpectedType, _opRawData, _nDataBytes);
}

NSys::NNet::CAddress NSys::NNet::fg_SetAddressRaw(NSys::NNet::CAddress _Address, ::NMib::NNet::ENetAddressType _Type, void const* _pRawData, mint _nDataBytes)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return (NSys::NNet::CAddress)fg_GetLocalSys()->m_SocketContext->f_SetAddressRaw((CWindowsAddress*)_Address, _Type, _pRawData, _nDataBytes);
}

NSys::NNet::CAddress NSys::NNet::fg_ResolveAddress(const NMib::NStr::CStr &_Address, ::NMib::NNet::ENetAddressType _PreferType)
{
	return fg_GetLocalSys()->m_SocketContext->f_ResolveAddress(_Address, _PreferType);
}

void *NSys::NNet::fg_AsyncResolveAddress_Open(const NMib::NStr::CStr &_Address, ::NMib::NNet::ENetAddressType _PreferType, NMib::NFunction::TCFunction<void ()>&& _fOnFinish)
{
	return fg_GetLocalSys()->m_SocketContext->f_AsyncResolveAddress_Open(_Address, _PreferType, fg_Move(_fOnFinish));
}

bint NSys::NNet::fg_AsyncResolveAddress_GetResult(void *_pResolver, NSys::NNet::CAddress& _opAddress, NMib::NStr::CStr &_Error)
{
	return fg_GetLocalSys()->m_SocketContext->f_AsyncResolveAddress_GetResult(_pResolver, (CWindowsAddress*&)_opAddress, _Error);
}

void NSys::NNet::fg_AsyncResolveAddress_Close(void *_pResolver)
{
	fg_GetLocalSys()->m_SocketContext->f_AsyncResolveAddress_Close(_pResolver);
}

int NSys::NNet::fg_CompareAddresses(NSys::NNet::CAddress _pFirst, NSys::NNet::CAddress _pSecond)
{
	DMibSafeCheck(_pFirst != nullptr, "Address is null!");
	DMibSafeCheck(_pSecond != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_CompareAddresses(*(CWindowsAddress*)_pFirst, *(CWindowsAddress*)_pSecond);
}

void NSys::NNet::fg_FreeAddress(NSys::NNet::CAddress _Address) // It is OK to free a nullptr address
{
	return fg_GetLocalSys()->m_SocketContext->f_FreeAddress((CWindowsAddress*)_Address);
}

NMib::NStr::CStr NSys::NNet::fg_GetAddressString(NSys::NNet::CAddress _Address, bint _bIncludeType)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_GetAddressString(*(CWindowsAddress*)_Address, _bIncludeType);
}

// Connection Operations
void *NSys::NNet::fg_Connect(NSys::NNet::CAddress _Address, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange, NSys::NNet::CAddress _BindAddress) // Report to the supplied event when new data is received or when we are ready to send new dat
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_Connect(*(CWindowsAddress*)_Address, fg_Move(_OnStateChange), (CWindowsAddress *)_BindAddress);
}

void *NSys::NNet::fg_AsyncConnect(NSys::NNet::CAddress _Address, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange, NSys::NNet::CAddress _BindAddress) // Report to the supplied event when new data is received or when we are ready to send new data and when the connection is connecte
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_AsyncConnect(*(CWindowsAddress*)_Address, fg_Move(_OnStateChange), (CWindowsAddress *)_BindAddress);
}

void *NSys::NNet::fg_Listen(NSys::NNet::CAddress _Address, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange) // Report to the supplied event when a new connection has arrive
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_Listen(*(CWindowsAddress*)_Address, fg_Move(_OnStateChange));
}

void *NSys::NNet::fg_ListenDatagram(NSys::NNet::CAddress _Address, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange)
{
	DMibSafeCheck(_Address != nullptr, "Address is null!");
	return fg_GetLocalSys()->m_SocketContext->f_ListenDatagram(*(CWindowsAddress*)_Address, fg_Move(_OnStateChange));
}

void *NSys::NNet::fg_Accept(void *_pSocket, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange) // Report to the supplied event when new data is received or when we are ready to send new dat
{
	return fg_GetLocalSys()->m_SocketContext->f_Accept((CWindowsSocket*)_pSocket, fg_Move(_OnStateChange));
}

void NSys::NNet::fg_Close(void *_pSocket) // Closes the socket and connectio
{
	fg_GetLocalSys()->m_SocketContext->f_Close((CWindowsSocket*)_pSocket);
}

mint NSys::NNet::fg_Receive(void *_pSocket, void *_pData, mint _DataLen) // Returns bytes receive
{
	return fg_GetLocalSys()->m_SocketContext->f_Receive((CWindowsSocket*)_pSocket, _pData, _DataLen);
}

mint NSys::NNet::fg_Send(void *_pSocket, const void *_pData, mint _DataLen) // Returns bytes sen
{
	return fg_GetLocalSys()->m_SocketContext->f_Send((CWindowsSocket*)_pSocket, _pData, _DataLen);
}

mint NSys::NNet::fg_SendDatagram(void *_pSocket, NSys::NNet::CAddress _Address, const void *_pData, mint _DataLen) // Returns bytes sen
{
	return fg_GetLocalSys()->m_SocketContext->f_SendDatagram((CWindowsSocket*)_pSocket, *((CWindowsAddress*)_Address), _pData, _DataLen);
}

mint NSys::NNet::fg_ReceiveDatagram(void *_pSocket, NSys::NNet::CAddress _Address, void *_pData, mint _DataLen) // Returns bytes sen
{
	return fg_GetLocalSys()->m_SocketContext->f_ReceiveDatagram((CWindowsSocket*)_pSocket, *((CWindowsAddress*)_Address), _pData, _DataLen);
}

// Socket Properties & State

void NSys::NNet::fg_SetOnStateChange(void *_pSocket, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange) // Report to the supplied event when new data is received or when we are ready to send new data			
{
	fg_GetLocalSys()->m_SocketContext->f_SetOnStateChange((CWindowsSocket*)_pSocket, fg_Move(_OnStateChange));
}

NMib::NNet::ENetTCPState NSys::NNet::fg_GetState(void *_pSocket) // Get the state of data availabl
{
	return fg_GetLocalSys()->m_SocketContext->f_GetState((CWindowsSocket*)_pSocket);
}

NMib::NStr::CStr NSys::NNet::fg_GetCloseReason(void *_pSocket)
{
	return fg_GetLocalSys()->m_SocketContext->f_GetCloseReason((CWindowsSocket*)_pSocket);
}

void *NSys::NNet::fg_InheritHandle2(void *_pSocket, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange)
{
	return fg_GetLocalSys()->m_SocketContext->f_InheritHandle2((CWindowsSocket*)_pSocket, fg_Move(_OnStateChange));
}

void *NSys::NNet::fg_GiveUpForInherit(void *_pSocket)
{
	return fg_GetLocalSys()->m_SocketContext->f_GiveUpForInherit((CWindowsSocket*)_pSocket);
}

void *NSys::NNet::fg_GetOSSocket(void *_pSocket)
{
	return fg_GetLocalSys()->m_SocketContext->f_GetOSSocket((CWindowsSocket*)_pSocket);
}

NSys::NNet::CAddress NSys::NNet::fg_GetPeerAddress(void *_pSocket)
{
	return (NSys::NNet::CAddress)fg_GetLocalSys()->m_SocketContext->f_GetPeerAddress((CWindowsSocket*)_pSocket);
}

uint32 NSys::NNet::fg_GetListenPort(void *_pSocket)
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

HMODULE g_hNtDll = nullptr;
HMODULE g_hKernel32 = nullptr;
HMODULE g_hAdvAPI32 = nullptr;

bint g_bValidExitProcess = false;
void __cdecl fg_ValidExitProcess()
{
	g_bValidExitProcess = true;
}

bint g_bValidDestroyModule = false;
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

bint g_bPatchedExitProcess = false;
bint g_bPatchedTerminateProcess = false;
bint g_bSystemCreated = false;

void NSys::fg_DestroySystem()
{
}

void NSys::fg_PreDestroyHeap()
{
}

namespace NMib
{
	
	mint __declspec(align(8)) g_SystemMemory[sizeof(CSystemWindowsMSVC) / sizeof(mint)];
	static_assert(__alignof(g_SystemMemory) == 8, "Alignment didn't work");
	mint g_bCreatingSystemDone = false;
	mint g_bCanUseSystemMalloc = true;
	mint g_bCanStartThreads = false;
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
	CFStr256 EnvironName;

	GetSystemInfo(&gs_SysInfo);     // Get the system info structure. Used by various functions. Let it reside in every module for speed of access.

	NPrivate::g_PageSizeLarge = gs_SysInfo.dwPageSize;

	NPrivate::g_VirtualAllocGranularity = gs_SysInfo.dwAllocationGranularity;

	DMibFastCheck(gs_SysInfo.dwPageSize == 4096);

	if (NLocal::g_fLargePageMinimum)
		NPrivate::g_PageSizeLarge = fg_Max(NLocal::g_fLargePageMinimum(), gs_SysInfo.dwPageSize);

	NPrivate::g_VirtualAllocGranularityLarge = fg_Max(NPrivate::g_PageSizeLarge, NPrivate::g_VirtualAllocGranularity);

	gs_ThreadLocalParentThread = NSys::fg_Thread_AllocLocal();

//	if (!g_bIsDll)
	{
		if (S_OK == fg_PatchIAT(g_hDllInstance, "KERNEL32.dll", "ExitProcess", (PVOID) fg_HookExitProcess, (PVOID *) &g_fOrgExitProcess))
			g_bPatchedExitProcess = true;
		if (S_OK == fg_PatchIAT(g_hDllInstance, "KERNEL32.dll", "TerminateProcess", (PVOID) fg_HookTerminateProcess, (PVOID *) &g_fOrgTerminateProcess))
			g_bPatchedTerminateProcess = true;
	}

#ifdef DMibPIR
	fg_MalterlibInitStdLib();
#endif

	//	float Test = 0.0;

	CSystemWindowsMSVC *pLocalSys;
	{
		static_assert(NTraits::TCAlignmentOf<CSystemWindowsMSVC>::mc_Value == 8, "Aligment error");
		g_bCreatingSystemDone = true;
		pLocalSys = new(NMib::g_SystemMemory) CSystemWindowsMSVC();
		pLocalSys->f_Init();
	}

	if (pLocalSys)
		pLocalSys->f_InitModule();

}

bint g_bSysDeleted = false;

void NSys::fg_Process_AllowInvalidExit(bool _bAllow)
{
	g_bAllowInvalidExit = _bAllow;
}

void NSys::fg_Process_Pause(mint _ProcessID)
{
	DMibError("fg_Process_Pause not implemented");
}

void NSys::fg_Process_Resume(mint _ProcessID)
{
	DMibError("fg_Process_Resume not implemented");
}

void NSys::fg_Process_Terminate(mint _ProcessID)
{
	HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, false, _ProcessID);
	if (!hProcess)
		DMibError(fg_Format("When terminating process Windows returned an error from OpenProcess: {}", fg_Win32_GetLastErrorStr()));

	NMib::g_OnScopeExit > [&]
		{
			CloseHandle(hProcess);
		}
	;

	if (!TerminateProcess(hProcess, 255))
		DMibError(fg_Format("When terminating process Windows returned an error from TerminateProcess: {}", fg_Win32_GetLastErrorStr()));
}

void NSys::fg_Process_Stop(mint _ProcessID)
{
	if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, _ProcessID))
	{
		DMibError(fg_Format("Windows returned an error from GenerateConsoleCtrlEvent: {}", fg_Win32_GetLastErrorStr()));
		DMibTrace("GenerateConsoleCtrlEvent: {}{\n}", fg_Win32_GetLastErrorStr());
	}
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

		fg_GetLocalSys()->f_PreDestroy();
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
		fg_GetLocalSys()->f_DestroyAggregates();
	}
}

void fg_DestroySystem()
{
	NSys::fg_Thread_FreeLocal(gs_ThreadLocalParentThread);

}

extern int g_AllowDebugNewErrorGlobalStatic;

#if !defined(DMibPOverrideOperatorNew)
void DDefaultCallingConv operator delete(void *pUserData)
{
	NMib::NMem::fg_Free(pUserData);
}

void * DDefaultCallingConv operator new(size_t _Size)
{
	return NMib::NMem::fg_Alloc(_Size);
}
#endif


void NMib::NSys::fg_HW_GetProcessorInfo(NMib::CProcessorInfo& _Info)
{ // Should probably be moved to a file Malterlib_x86_MSVC.cpp or similar.
	int CPUInfo[4];

	_Info.m_Architecture = NMib::EProcessorArchitecture_Unknown;
	_Info.m_Features = NMib::EProcessorFeature_None;	

	__cpuid(CPUInfo, 0);

	int MaxInfoType = CPUInfo[0];

	if (MaxInfoType >= 1)
	{
		__cpuid(CPUInfo, 1);

		_Info.m_Features |= 	( (CPUInfo[3] & DMibBit(23)) ? EProcessorFeature_MMX : EProcessorFeature_None) 
							|	( (CPUInfo[3] & DMibBit(25)) ? EProcessorFeature_SSE : EProcessorFeature_None) 
							|	( (CPUInfo[3] & DMibBit(26)) ? EProcessorFeature_SSE2 : EProcessorFeature_None)
							|	( (CPUInfo[2] & DMibBit(0)) ? EProcessorFeature_SSE3 : EProcessorFeature_None)
							|	( (CPUInfo[2] & DMibBit(9)) ? EProcessorFeature_SSSE3 : EProcessorFeature_None)
							|	( (CPUInfo[2] & DMibBit(19)) ? EProcessorFeature_SSE4_1 : EProcessorFeature_None)
							|	( (CPUInfo[2] & DMibBit(20)) ? EProcessorFeature_SSE4_2 : EProcessorFeature_None)
							|	( (CPUInfo[2] & DMibBit(31)) ? EProcessorFeature_HyperVisor : EProcessorFeature_None);
	}

	_Info.m_Architecture = (sizeof(void*) == 4) ? EProcessorArchitecture_x86 : EProcessorArchitecture_x86_64;
}

bint NMib::NSys::fg_HW_GetVirtualMachineInfo(CVirtualMachineInfo& _Info)
{
	_Info.m_bDetected = false;
	_Info.m_pName = nullptr;

	CProcessorInfo ProcInfo;
	fg_HW_GetProcessorInfo(ProcInfo);

	if ((ProcInfo.m_Features & EProcessorFeature_HyperVisor))
		_Info.m_bDetected = true;

	NRuntimeMSVC::CWin32_Registry Registry(NRuntimeMSVC::CWin32_Registry::ERegRoot_LocalMachine);
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

NMib::NSys::ESecurePassword NMib::NSys::fg_SecurePassword_SetLocation(NMib::NStr::CStr const& _Location)
{
	fg_GetLocalSys()->m_SecurePasswordLocation = _Location;
	return NMib::NSys::ESecurePassword_OK;
}

NMib::NSys::ESecurePassword NMib::NSys::fg_SecurePassword_Store(NMib::NStr::CStr const& _Key, NMib::NStr::CStrSecure const& _Password)
{
	DMibSafeCheck(!fg_GetLocalSys()->m_SecurePasswordLocation.f_IsEmpty(), "You must have set the location for secure passwords.");

	DATA_BLOB DataIn;
	DATA_BLOB DataOut;
	DATA_BLOB Entropy;

	DataIn.pbData = (BYTE*)_Password.f_GetStr();    
	DataIn.cbData = _Password.f_GetLen();

	NDataProcessing::CHashDigest_SHA1 KeyDigest;
	{
		NDataProcessing::CHash_SHA1 Hash;
		Hash.f_AddData(_Key.f_GetStr(), _Key.f_GetLen());
		KeyDigest = Hash;
	}
	Entropy.pbData = (BYTE*)KeyDigest.f_GetData();
	Entropy.cbData = NDataProcessing::CHashDigest_SHA1::fs_GetSize();

	if(!CryptProtectData(
		&DataIn
		,NULL 			// Description
		,&Entropy 			// Optional entropy
		,NULL 			// Reserved
		,NULL 			// Prompt info
		,0 				// Flags
		,&DataOut))
	{
		return NMib::NSys::ESecurePassword_Failure;
	}

	TCVector<uint8> Encrypted;

	Encrypted.f_SetLen(DataOut.cbData);
	fg_MemCopy(Encrypted.f_GetArray(), DataOut.pbData, Encrypted.f_GetLen());

	SecureZeroMemory(DataOut.pbData, DataOut.cbData);
	LocalFree(DataOut.pbData);

	auto Cleanup = fg_OnScopeExit(
			[&]()
			{
				SecureZeroMemory(Encrypted.f_GetArray(), Encrypted.f_GetLen());
			}
		);

	NRuntimeMSVC::CWin32_Registry Reg(NRuntimeMSVC::CWin32_Registry::ERegRoot_CurrentUser, fg_GetLocalSys()->m_SecurePasswordLocation);

	try
	{
		Reg.f_Write("", _Key, Encrypted);
	}
	catch (const NException::CException &)
	{
		return NMib::NSys::ESecurePassword_Failure;
	}

	return NMib::NSys::ESecurePassword_OK;
}

NMib::NSys::ESecurePassword NMib::NSys::fg_SecurePassword_Remove(NMib::NStr::CStr const& _Key)
{
	DMibSafeCheck(!fg_GetLocalSys()->m_SecurePasswordLocation.f_IsEmpty(), "You must have set the location for secure passwords.");

	NRuntimeMSVC::CWin32_Registry Reg(NRuntimeMSVC::CWin32_Registry::ERegRoot_CurrentUser, fg_GetLocalSys()->m_SecurePasswordLocation);

	try
	{
		if (Reg.f_ValueExists("", _Key))
		{
			Reg.f_DeleteValue("", _Key);
		}
	}
	catch(NException::CException const&)
	{
		return NMib::NSys::ESecurePassword_Failure;
	}

	return NMib::NSys::ESecurePassword_OK;
}

NMib::NSys::ESecurePassword NMib::NSys::fg_SecurePassword_Get(NMib::NStr::CStr const& _Key, NMib::NStr::CStrSecure& _oPassword)
{
	DMibSafeCheck(!fg_GetLocalSys()->m_SecurePasswordLocation.f_IsEmpty(), "You must have set the location for secure passwords.");

	NRuntimeMSVC::CWin32_Registry Reg(NRuntimeMSVC::CWin32_Registry::ERegRoot_CurrentUser, fg_GetLocalSys()->m_SecurePasswordLocation);

	TCVector<uint8> Encrypted;

	try
	{
		Encrypted = Reg.f_Read_Bin("", _Key);
	}
	catch(NException::CException const&)
	{
		return NMib::NSys::ESecurePassword_NotFound;
	}

	DATA_BLOB DataIn;
	DATA_BLOB DataOut = {0};

	DataIn.pbData = (BYTE*)Encrypted.f_GetArray();
	DataIn.cbData = Encrypted.f_GetLen();

	auto Cleanup = fg_OnScopeExit(
			[&]()
			{
				SecureZeroMemory(Encrypted.f_GetArray(), Encrypted.f_GetLen());
				if (DataOut.pbData)
				{
					SecureZeroMemory(DataOut.pbData, DataOut.cbData);
					LocalFree(DataOut.pbData);
				}
			}
		);

	NDataProcessing::CHashDigest_SHA1 KeyDigest;
	{
		NDataProcessing::CHash_SHA1 Hash;
		Hash.f_AddData(_Key.f_GetStr(), _Key.f_GetLen());
		KeyDigest = Hash;
	}

	DATA_BLOB Entropy;
	Entropy.pbData = (BYTE*)KeyDigest.f_GetData();
	Entropy.cbData = NDataProcessing::CHashDigest_SHA1::fs_GetSize();


	if (!CryptUnprotectData(
				&DataIn
			,	NULL 	// Desc
			,	&Entropy 	// Entropy
			,	NULL 	// Reserved
			,	NULL 	// Prompt
			,	0 		// Flags
			, 	&DataOut
		))
	{
		return NMib::NSys::ESecurePassword_Failure;
	}

	_oPassword = NMib::NStr::CStrSecure( (char*)DataOut.pbData, DataOut.cbData );

	return NMib::NSys::ESecurePassword_OK;

}

bool NMib::NSys::fg_SecurePassword_Supported()
{
	return true;
}


NMib::NSys::ESecurePassword NMib::NSys::fg_SecurePassword_Exists(NMib::NStr::CStr const& _Key)
{
	DMibSafeCheck(!fg_GetLocalSys()->m_SecurePasswordLocation.f_IsEmpty(), "You must have set the location for secure passwords.");

	NRuntimeMSVC::CWin32_Registry Reg(NRuntimeMSVC::CWin32_Registry::ERegRoot_CurrentUser, fg_GetLocalSys()->m_SecurePasswordLocation);

	try
	{
		if (Reg.f_ValueExists("", _Key))
		{
			return NMib::NSys::ESecurePassword_OK;
		}
	}
	catch(NException::CException const&)
	{
		return NMib::NSys::ESecurePassword_Failure;
	}

	return NMib::NSys::ESecurePassword_NotFound;
}

NMib::NSys::EDesktopEnvironment NMib::NSys::fg_DesktopEnvironment_Get()
{
	return NMib::NSys::EDesktopEnvironment_Windows;
}

