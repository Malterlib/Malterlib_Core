// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#define DMibAllowCodeStandardViolations 1

#include <Mib/Concurrency/ThreadSafeQueue>

#include <malloc.h>

using namespace NMib;
using namespace NMib::NStr;
using namespace NMib::NTime;
using namespace NMib::NMem;
using namespace NMib::NContainer;

bint g_bIsSharedLibrary = false;

mint g_MainModuleBase = 0;

void fg_ForkPrepare();
void fg_ForkParentOrChild();

#include <Mib/Core/PlatformSpecific/PosixErrNo>

// *************************************************************************************************************************
// POSIX Implementation specific headers
// *************************************************************************************************************************

#if !defined(DMibPOverrideOperatorNew)
	void operator delete(void *pUserData)
	{
		NMib::NMem::fg_Free(pUserData);
	}

	void * operator new(size_t _Size)
	{
		return NMib::NMem::fg_Alloc(_Size);
	}

#endif

// *************************************************************************************************************************
// POSIX Implementation
// *************************************************************************************************************************

#define DMibConfig_SemaphoreImplemented

#include "Malterlib_Core_PlatformImp_POSIX_PThread.hpp"
#include "Malterlib_Core_PlatformImp_POSIX.imp.h"
#include "Malterlib_Core_PlatformImp_POSIX_File.hpp"
#include "Malterlib_Core_PlatformImp_POSIX_Console.hpp"
#include "Malterlib_Core_PlatformImp_POSIX_Environment.hpp"
#include "Malterlib_Core_PlatformImp_POSIX_Module.hpp"
#include "Malterlib_Core_PlatformImp_POSIX_Process.hpp"
#include "Malterlib_Core_PlatformImp_POSIX_User.hpp"
#include "Malterlib_Core_PlatformImp_POSIX_VirtualMemory.hpp"
//#include "Malterlib_Core_PlatformImp_POSIX_ProcessLaunch.imp.h"
//#include "Malterlib_Core_PlatformImp_POSIX_StdInReader.imp.h"
//#include "Malterlib_Core_PlatformImp_POSIX_Net.imp.h"

// *************************************************************************************************************************
// Emscripten Implementation
// *************************************************************************************************************************

#include <sys/mman.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/mount.h>	
#include <sys/sysctl.h>
#include <sys/epoll.h>
#include <semaphore.h>

#ifndef DPlatformFamily_Emscripten
#	include <execinfo.h>
#endif

#include <dlfcn.h>
#include <sys/time.h>

#include <string.h>
#include <dlfcn.h>
#include <cxxabi.h>

#include <uuid/uuid.h>	// For UUID gen

static inline_small class CSystemEmscripten *fg_GetLocalSys();

void calling_convention_c fg_Malterlib_MakeActive()
{

}

CStr fg_EscapeString(CStr _In)
{
	return _In.f_Replace("\"","\\\"");
}


void *NSys::fg_Semaphore_Alloc(mint _InitialCount, mint _MaximumCount)
{
	return (void *)1;
}

void NSys::fg_Semaphore_ForkedChild(void * _pSemaphore)
{
}

void NSys::fg_Semaphore_Free(void *_pSemaphore)
{
}

void NSys::fg_Semaphore_Increase(void * _pSemaphore, mint _Count)
{
}

void NSys::fg_Semaphore_Wait(void * _pSemaphore)
{
}

bint NSys::fg_Semaphore_WaitTimeout(void * _pSemaphore, fp64 _Timeout)
{
	return true;
}

bint NSys::fg_Semaphore_TryWait(void * _pSemaphore)
{
	return true;
}

namespace NMib
{
	namespace NSys
	{
		extern const ch8* g_LinuxProgramIdentifier;
	}
}

class CSystemEmscripten : public CSystem
{
public:
	
	CSystem_POSIX m_Posix;

	CSystemEmscripten()
		: CSystem(g_bIsSharedLibrary)
	{
		fp_InitComplete();
	}

	void f_InitModule()
	{
		CSystem::f_InitModule();

	}

	void f_DestroyThreadSpecific()
	{
		CSystem::f_PreDestructThreadSpecific();
		
		m_Posix.f_DestroyThreadSpecific();

		CSystem::f_DestructThreadSpecific();
	}
	
	void f_Destruct()
	{
		m_Posix.f_Destruct();
		
		CSystem::f_Destruct();

	}
	
	void f_LoadLibraries()
	{
	}
	
};

static inline_small CSystemEmscripten *fg_GetLocalSys()
{
	return (CSystemEmscripten *)fg_GetSys();
}

CSystem_POSIX *fg_GetSys_POSIX()
{
	return &fg_GetLocalSys()->m_Posix;
}

typedef char uuid_string_t[256];

void NSys::fg_System_GenerateUUID(NDataProcessing::CUniversallyUniqueIdentifier &_UUID)
{
	static_assert(sizeof(uuid_t) == sizeof(_UUID), "");
	uuid_generate((unsigned char *)&_UUID);
#	if DMibEnableSafeCheck > 0
	uuid_string_t RetStr;
	uuid_unparse((unsigned char *)&_UUID, RetStr);
#	endif
	_UUID.m_TimeLow = fg_ByteSwapBE(_UUID.m_TimeLow);
	_UUID.m_TimeMid = fg_ByteSwapBE(_UUID.m_TimeMid);
	_UUID.m_TimeHiAndVersion = fg_ByteSwapBE(_UUID.m_TimeHiAndVersion);
#	if DMibEnableSafeCheck > 0
	DMibFastCheck(_UUID.f_GetAsStaticString(NDataProcessing::EUniversallyUniqueIdentifierFormat_Bare).f_CmpNoCase(RetStr) == 0);
#	endif
}


NStr::CStr NSys::fg_System_GenerateUUID()
{
	uuid_t Ret;
	uuid_generate(Ret);
	uuid_string_t RetStr;
	uuid_unparse(Ret, RetStr);

	// {7EE072A7-458D-491f-ACCF-447AD4BE8DBF}
	return CStr(CStr::CFormat("{{{}}") << RetStr);
}


struct CCodePageCache
{
	TCMap<ch32, ch8, CSort_Default, CAllocator_NonTrackedHeap> m_Cache;
	CCodePageCache()
	{
		for (int i = 0; i < 256; ++i)
		{
			ch32 Char = NMib::NTraits::TCUnsigned<ch32>::CType(NMib::NTraits::TCUnsigned<ch16>::CType(NMib::NStr::TCCharEncodingConverter<NMib::NStr::ECharacterEncoding_Windows_1252>::ms_Table[i].m_UnicodeChar));
			if (Char != 0xFFFF)
				m_Cache[Char] = i;
		}
			
	}
	
};

NMib::NAggregate::TCAggregate<CCodePageCache> g_CodePageCache = { DAggregateInit };

void NMib::NSys::NStr::fg_SystemEncodeAnsiStr(NMib::NStr::CStr const &_In, NMib::NStr::CAnsiStr &_Out, ch8 _ErrorChar)
{
	CStr Utf8 = _In;
	
	auto &Cache = *g_CodePageCache;
	
	_Out.f_Clear();
	auto Iter = Utf8.f_GetUnicodeIterator();
	while (Iter)
	{
		ch8 *pChar = Cache.m_Cache.f_FindEqual(*Iter);
		if (pChar)
		{
			_Out.f_AddChar(*pChar);
		}
		else
			_Out.f_AddChar(_ErrorChar);

		++Iter;
	}
}

void NMib::NSys::NStr::fg_SystemEncodeAnsiStr(NMib::NStr::CStrNonTracked const &_In, NMib::NStr::CAnsiStrNonTracked &_Out, ch8 _ErrorChar)
{
	CStrNonTracked Utf8 = _In;
	
	auto &Cache = *g_CodePageCache;
	
	_Out.f_Clear();
	auto Iter = Utf8.f_GetUnicodeIterator();
	while (Iter)
	{
		ch8 *pChar = Cache.m_Cache.f_FindEqual(*Iter);
		if (pChar)
		{
			_Out.f_AddChar(*pChar);
		}
		else
			_Out.f_AddChar(_ErrorChar);
		
		++Iter;
	}
}


void NMib::NSys::NStr::fg_SystemDecodeAnsiStr(NMib::NStr::CAnsiStr const &_In, NMib::NStr::CStr &_Out)
{
	_Out = NMib::NStr::fg_DecodeCharacterEncoding<NMib::NStr::ECharacterEncoding_Windows_1252>(_In);

void NMib::NSys::NStr::fg_SystemDecodeAnsiStr(ch8 const *_pIn, NMib::NStr::CStr &_Out)
{
	_Out = NMib::NStr::fg_DecodeCharacterEncoding<NMib::NStr::ECharacterEncoding_Windows_1252>(_pIn);
}

void NMib::NSys::NStr::fg_SystemDecodeAnsiStr(NMib::NStr::CStrNonTracked const &_In, NMib::NStr::CStrNonTracked &_Out)
{
	const NMib::NStr::CStrNonTracked::CChar *pIn = _In.f_GetStr();
	_Out = NMib::NStr::fg_DecodeCharacterEncoding<NMib::NStr::ECharacterEncoding_Windows_1252>(pIn);
}

void NMib::NSys::NStr::fg_SystemDecodeAnsiStr(ch8 const *_pIn, NMib::NStr::CStrNonTracked &_Out)
{
	_Out = NMib::NStr::fg_DecodeCharacterEncoding<NMib::NStr::ECharacterEncoding_Windows_1252>(_pIn);

}

void NMib::NSys::NStr::fg_SystemEncodeCodePageStr(NMib::NStr::CStrNonTracked const &_In, NMib::NStr::CAnsiStrNonTracked &_Out, uint32 _CodePage, ch8 _ErrorChar)
{
	if (_CodePage != 1252)
		DMibError(NMib::NStr::CStrNonTracked::CFormat("Codepage {} not supported") << _CodePage);
	fg_SystemEncodeAnsiStr(_In, _Out, _ErrorChar);
}

void NMib::NSys::NStr::fg_SystemDecodeCodePageStr(NMib::NStr::CStrNonTracked const &_In, NMib::NStr::CStrNonTracked &_Out, uint32 _CodePage)
{
	if (_CodePage != 1252)
		DMibError(NMib::NStr::CStrNonTracked::CFormat("Codepage {} not supported") << _CodePage);
	fg_SystemDecodeAnsiStr(_In, _Out);
}

void NMib::NSys::NStr::fg_SystemDecodeCodePageStr(ch8 const *_pIn, NMib::NStr::CStrNonTracked &_Out, uint32 _CodePage)
{
	if (_CodePage != 1252)
		DMibError(NMib::NStr::CStrNonTracked::CFormat("Codepage {} not supported") << _CodePage);
	fg_SystemDecodeAnsiStr(_pIn, _Out);
}

void NMib::NSys::NStr::fg_SystemEncodeCodePageStr(NMib::NStr::CStr const &_In, NMib::NStr::CAnsiStr &_Out, uint32 _CodePage, ch8 _ErrorChar)
{
	if (_CodePage != 1252)
		DMibError(NMib::NStr::CStrNonTracked::CFormat("Codepage {} not supported") << _CodePage);
	fg_SystemEncodeAnsiStr(_In, _Out, _ErrorChar);
}

void NMib::NSys::NStr::fg_SystemDecodeCodePageStr(NMib::NStr::CStr const &_In, NMib::NStr::CStr &_Out, uint32 _CodePage)
{
	if (_CodePage != 1252)
		DMibError(NMib::NStr::CStrNonTracked::CFormat("Codepage {} not supported") << _CodePage);
	fg_SystemDecodeAnsiStr(_In, _Out);
}

void NMib::NSys::NStr::fg_SystemDecodeCodePageStr(ch8 const *_pIn, NMib::NStr::CStr &_Out, uint32 _CodePage)
{
	if (_CodePage != 1252)
		DMibError(NMib::NStr::CStrNonTracked::CFormat("Codepage {} not supported") << _CodePage);
	fg_SystemDecodeAnsiStr(_pIn, _Out);
}

NContainer::TCMap<NMib::NStr::CStr, NMib::NStr::CStr> NMib::NSys::fg_Process_GetEnvironmentVariables_NonProtected()
{
	NContainer::TCMap<NMib::NStr::CStr, NMib::NStr::CStr> Vars;

	CStr Key;

	for 
		(
			char **pEnvironment = environ
			; *pEnvironment
			; ++pEnvironment
		)
	{
		CStr VarStr(*pEnvironment);
		Key = NMib::NStr::fg_GetStrSep(VarStr, "=");

		Vars[Key] = VarStr;
	}

	return fg_Move(Vars);
}

inline_never bint NSys::fg_Compiler_AlwaysFalse()
{
	return false;
}

assure_used inline_never bint NSys::fg_Compiler_MakeActive(const void *_Reference)
{
	(void)_Reference;
	return true;
}

void NSys::fg_Thread_Suspend(void *_pThread)
{
	DMibError("Thread supension not available on linux");
}

void NSys::fg_Thread_Resume(void *_pThread)
{
	DMibError("Thread supension not available on linux");
}

void NSys::fg_Debug_BlockingMessage(NMib::NStr::CStr const &_Heading, NMib::NStr::CStr const &_Message)
{

}

void NSys::fg_Debug_DiffStrings(const NMib::NStr::CStr &_FirstStr, const NMib::NStr::CStr &_SecondStr, const NMib::NStr::CStr &_FirstName, const NMib::NStr::CStr &_SecondName)
{
	DMibError("Not implemented - fg_Debug_DiffStrings");
}

void NSys::fg_Debug_GenerateCrashDump(const NMib::NStr::CStr &_Message, const NMib::NStr::CStr &_ExtraLog, NContainer::TCVector<NMib::NStr::CStr> &_GeneratedLogs, bint _bDisplayGUI)
{
	
}

void NSys:::fg_Debug_GenerateMemoryDump(NMib::NContainer::TCVector<void*, NMib::NMem::CAllocator_NonTrackedHeap> const& _Locations, NMib::NContainer::TCVector<mint, NMib::NMem::CAllocator_NonTrackedHeap> const& _Sizes)
{

}

// Libunwind may buy us some more info, but the output from that (even Apple's variant) is pretty
// much the same as from dladdr as far as I can tell.

namespace
{
	bool g_bCanStackTrace = false;
}

inline_never mint NSys::fg_System_GetStackTrace(mint *_pStack, mint _nMaxDepth)
{
	return 0;
}

mint NSys::fg_System_GetStackTrace(aint _iDepth)
{
	return 0;
}

bool NSys::fg_Debug_AquireStackTraceInfo(CStackTraceInfo & _oInfo, mint _Address, bool _bCanAllocNonTracked)
{
	return false;
}


CStackTraceInfo *NSys::fg_Debug_AquireStackTraceInfo(mint _Address)
{
	return nullptr;
}

void NSys::fg_Debug_ReleaseStackTraceInfo(CStackTraceInfo *_pInfo)
{
}

bint NSys::fg_Debug_IsDeadlocked()
{
	return false;
}

void NSys::fg_Debug_SetDeadlockNotifyFunction(FDeadlockUserNotify *_pCrashDumpUserNotify)
{
	
}

void NSys::fg_Debug_PauseDeadlockDetector()
{
}

void NSys::fg_Debug_ResumeDeadlockDetector()
{
}

void NSys::fg_System_ReportContractViolation(const NMib::NStr::CStrNonTracked &_Message)
{
}

EDebugCheckFailureAction NSys::fg_Debug_ReportContractFailure(const ch8 *_pFileName, int32 _Line, void *_pCodePointer, const NMib::NStr::CStrNonTracked &_ErrorMessage)
{
	return EDebugContractFailureAction_NotHandled;
}


void NSys::fg_Thread_SetNumaAffinity(void *_pThread, ENumaNode _NumaNode)
{
	// TODO: Implement
}

mint NSys::fg_Mem_GetNumNumaNodes()
{
  // TODO: Implement
  return 0;
}

void NSys::fg_Mem_GetNumaNodes(ENumaNode *_pNodes, mint _nNodes)
{
  // TODO: Implement
}

void *NSys::fg_InterProcess_MemAlloc(ch8 const *_pName, mint _Size, void * &_pMemory)
{
	// TODO: Implement
	DMibError("Not implemented");
	return nullptr;
}

void NSys::fg_InterProcess_MemFree(void *_pHandle, void *_pMemory)
{
	// TODO: Implement
}


void NSys::fg_Thread_SmallestSleep()
{
	fg_Thread_Sleep(0.001);
}

#include <stdio.h>

namespace
{
	template <typename tf_CStr>
	TCVector<tf_CStr> fg_GetCommandLineParams()
	{
		TCVector<tf_CStr> Return;

		tf_CStr CmdlinePath = typename tf_CStr::CFormat("/proc/{}/cmdline") << (mint)getpid();
		auto FileData = fg_ReadProcFS<tf_CStr>(CmdlinePath);
		
		auto pParse = FileData.f_GetArray();
		auto pParseEnd = pParse + FileData.f_GetLen() - 1;
		
		while (pParse < pParseEnd)
		{
			auto *pStart = pParse;
			fg_ParseToEndOfLine(pParse);
			if (pParse-pStart)
			{
				Return.f_Insert(tf_CStr(pStart, pParse-pStart));
			}
			fg_ParseEndOfLine(pParse);
			if (!(*pParse) && pParse < pParseEnd)
			{
				++pParse;
			}
		}

		return Return;
	}
}


void NSys::fg_Security_GenerateHighEntropyData(uint8 *_pData, mint _nBytes)
{
	if (fg_ReadProcFS("/dev/urandom", _pData, _nBytes) != _nBytes)
		DMibPDebugBreak;
}

NMib::NStr::CStr NSys::fg_Process_GetCommandLine()
{
	NMib::NStr::CStr Return;
	
	TCVector<CStr> Parameters = fg_GetCommandLineParams<CStr>();

	auto iParam = Parameters.f_GetIterator();
	for (; iParam; ++iParam)
	{
		if (!iParam->f_StartsWith("--OutputPID"))
			fg_AddStrSepEscaped(Return, *iParam, ' ');
	}
	
	return Return;
}


bint NSys::fg_HW_GetVirtualMachineInfo(CVirtualMachineInfo& _Info)
{
	_Info.m_bDetected = false;
	_Info.m_pName = nullptr;

	return _Info.m_bDetected;
}

void NSys::fg_Process_GetCommandLineArgs(NContainer::TCVector<NMib::NStr::CStr> &_List)
{
	_List.f_Clear();

	TCVector<CStr> Parameters = fg_GetCommandLineParams<CStr>();

	auto iParam = Parameters.f_GetIterator();
	for (; iParam; ++iParam)
	{
		if (!iParam->f_StartsWith("--OutputPID"))
			_List.f_Insert(*iParam);
	}
	
}

NMib::NStr::CStr NSys::fg_CommandLineParameters()
{
	NMib::NStr::CStr Return;
	
	TCVector<CStr> Parameters = fg_GetCommandLineParams<CStr>();
	
	auto iParam = Parameters.f_GetIterator();
	if (iParam)
		++iParam; // The first param should not be retured for this variant
	for (; iParam; ++iParam)
	{
		if (!iParam->f_StartsWith("--OutputPID"))
			fg_AddStrSepEscaped(Return, *iParam, ' ');
	}
	
	return Return;
}

void NSys::fg_Message(const ch8 *_pMessageType, const ch8 *_pToOutput)
{
	DMibTraceRaw(_pToOutput);
}

void NSys::fg_Message(const ch16 *_pMessageType, const ch16 *_pToOutput)
{
}

void fg_DestroySystemAtExit()
{
	NSys::fg_DestroySystem();
}

namespace NMib
{
	mint align_cacheline g_SystemMemory[sizeof(CSystemEmscripten) / sizeof(uint64)];
	mint g_bCreatingSystemDone = false;
	mint g_bCanUseSystemMalloc = false;
	mint g_bCanStartThreads = false;
}

void fg_ForkPrepare()
{
}

void fg_ForkParentOrChild()
{
}

namespace NMib
{
	
	namespace NSys
	{
		// This allows
		void __attribute__((weak)) fg_InitBreakpad()
		{
		}
		void __attribute__((weak)) fg_DestroyBreakpad()
		{
		}
	}
	
} // Namespace NMib

#include <link.h>

using namespace NFunction;

namespace NMib
{
	namespace NSys
	{
		int g_OperatingSystemMajor = -1;
		int g_OperatingSystemMinor = 0;
		int g_OperatingSystemFix = 0;
		NProcess::EOperatingSystemArch g_OperatingSystemArch = NProcess::EOperatingSystemArch_Unknown;
	}
}

#include <sys/utsname.h>


bint NSys::fg_System_GetOperatingSystemVersion(int& _oMajor, int& _oMinor, int& _oFix, NProcess::EOperatingSystemArch& _Arch)
{
	if (g_OperatingSystemMajor >= 0)
	{
		_oMajor = g_OperatingSystemMajor;
		_oMinor = g_OperatingSystemMinor;
		_oFix = g_OperatingSystemFix;
		_Arch = g_OperatingSystemArch;
		return g_OperatingSystemMajor != 0;
	}

	g_OperatingSystemMajor = 0;
	
	utsname NameInfo;
	if (uname(&NameInfo))
	{
		return false;
	}
	
	(CStr::CParse("{}.{}.{}") >> g_OperatingSystemMajor >> g_OperatingSystemMinor >> g_OperatingSystemFix).f_Parse(NameInfo.release);

	g_OperatingSystemArch = NProcess::EOperatingSystemArch_le32;
	
	return true;
}

namespace NMib
{
	namespace NSys
	{
		namespace NPrivate
		{
		#pragma clang diagnostic push
		#pragma clang diagnostic ignored "-Wdeprecated-declarations"
			void fg_InitBaseModuleAddress()
			{
				Dl_info Info;
				dladdr((void *)&fg_CreateSystem, &Info);
				
				g_MainModuleBase = (mint)((uint8 const *)Info.dli_fbase);
				
			}
		#pragma clang diagnostic pop
			
		}
	}
}


extern "C"
{
	void *nontracked_malloc(size_t __size)
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
#		if DMibConfig_MalterlibMemoryManager_Debug
			return NMib::NMem::CAllocator_NonTrackedHeap::f_AllocDebug(__size, DMibPFile, DMibPLine, EHeapDebugFlag_Ignore);
#		else
			return NMib::NMem::CAllocator_NonTrackedHeap::f_Alloc(__size);
#		endif
	}

	void *nontracked_calloc (size_t __nmemb, size_t __size)
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
		mint Size = __nmemb * __size;
#		if DMibConfig_MalterlibMemoryManager_Debug
			auto pMem = NMib::NMem::CAllocator_NonTrackedHeap::f_AllocDebug(Size, DMibPFile, DMibPLine, EHeapDebugFlag_Ignore);
#		else
			auto pMem = NMib::NMem::CAllocator_NonTrackedHeap::f_Alloc(Size);
#		endif
		fg_MemClear(pMem, Size);
		return pMem;
	}	

	void *nontracked_realloc (void *__ptr, size_t __size)
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
#		if DMibConfig_MalterlibMemoryManager_Debug
			return NMib::NMem::CAllocator_NonTrackedHeap::f_ReallocDebug(__ptr, __size, 0, DMibPFile, DMibPLine, EHeapDebugFlag_Ignore);
#		else
			return NMib::NMem::CAllocator_NonTrackedHeap::f_Realloc(__ptr, __size);
#		endif
	}

	void nontracked_free (void *__ptr)
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
		return NMib::NMem::CAllocator_NonTrackedHeap::f_Free(__ptr);
	}

	void nontracked_cfree (void *__ptr)
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
		return NMib::NMem::CAllocator_NonTrackedHeap::f_Free(__ptr);
	}
	void *nontracked_memalign (size_t __alignment, size_t __size)
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
#		if DMibConfig_MalterlibMemoryManager_Debug
			return NMib::NMem::CAllocator_NonTrackedHeap::f_AllocAlignedDebug(__size, __alignment, DMibPFile, DMibPLine, EHeapDebugFlag_Ignore);
#		else
			return NMib::NMem::CAllocator_NonTrackedHeap::f_AllocAligned(__size, __alignment);
#		endif
	}
	void *nontracked_valloc (size_t __size)
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
#		if DMibConfig_MalterlibMemoryManager_Debug
			return NMib::NMem::CAllocator_NonTrackedHeap::f_AllocAlignedDebug(__size, NMib::NSys::NPrivate::g_PageSize, DMibPFile, DMibPLine, EHeapDebugFlag_Ignore);
#		else
			return NMib::NMem::CAllocator_NonTrackedHeap::f_AllocAligned(__size, NMib::NSys::NPrivate::g_PageSize);
#		endif
	}
	void * nontracked_pvalloc (size_t __size)
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
#		if DMibConfig_MalterlibMemoryManager_Debug
			return NMib::NMem::CAllocator_NonTrackedHeap::f_AllocAlignedDebug(__size, NMib::NSys::NPrivate::g_PageSize, DMibPFile, DMibPLine, EHeapDebugFlag_Ignore);
#		else
			return NMib::NMem::CAllocator_NonTrackedHeap::f_AllocAligned(__size, NMib::NSys::NPrivate::g_PageSize);
#		endif
	}
	size_t nontracked_malloc_usable_size (void *__ptr)
	{
		DMibFastCheck(g_bCanUseSystemMalloc);
		return NMib::NMem::CAllocator_NonTrackedHeap::f_Size(__ptr);
	}
}

extern "C" void fg_InitMalterlib();

namespace NMib
{
	mint g_bCreatedSystem = false;
}

void NSys::fg_CreateSystem()
{
	if (g_bCreatedSystem)
		return;
	
#ifdef DMibConfig_OverrideSystemMalloc
	g_bMemoryManagerNeededAfterDestroy = true;
#endif
	g_bCreatedSystem = true;

	NMib::NSys::NPrivate::g_PageSize = getpagesize();

	// Cannot use malloc here
	
#ifdef DMibDynamicLibrary
	g_bIsSharedLibrary = true;
#else
	g_bIsSharedLibrary = false;
#endif

	if (!g_bIsSharedLibrary)
		signal(SIGPIPE,SIG_IGN);
	
	g_VirtualMap.f_Construct();
	
	if (g_OperatingSystemMajor < 0)
	{
		int Dummy;
		EOperatingSystemArch Arch;
		
		if (!fg_System_GetOperatingSystemVersion(Dummy, Dummy, Dummy, Arch))
			DMibPDebugBreak; // Not supported
	}

	g_bCreatingSystemDone = true;
	auto pSystem = new(NMib::g_SystemMemory) CSystemEmscripten();
	DMibFastCheck((void *)pSystem == (void *)&NMib::g_SystemMemory);
	
#ifndef DMibPAutomaticSystemCreation
	NMib::g_pSys = pSystem;
#endif
	
	g_bCanUseSystemMalloc = true;

	g_bCanStackTrace = true;
	
	// Can use malloc from here on
	
	NPrivate::fg_InitBaseModuleAddress();
	NPrivate::fg_SetupLimits();
	
	//atexit(&fg_DestroySystemAtExit);

	static_assert(NTraits::TCAlignmentOf<CSystemEmscripten>::mc_Value <= sizeof(uint64), "Aligment error");
	
	fg_InitBreakpad();
	
//	setlinebuf(stdout); // Default to line buffered output
//	setlinebuf(stderr); // Default to line buffered output
	
	{
		TCVector<CStr> Parameters = fg_GetCommandLineParams<CStr>();
		
		auto iParam = Parameters.f_GetIterator();
		for (; iParam; ++iParam)
		{			
			if (iParam->f_StartsWith("--OutputPID"))
			{			
				CStr Error;
				if (iParam->f_StartsWith("--OutputPID "))
				{				
					CStr PipeNames = iParam->f_Extract(fg_StrLen("--OutputPID "));					
					CStr StdErrPipeName = NMib::NFile::CFile::fs_GetPath(PipeNames) + "/NamePipeStdErr_" + NMib::NFile::CFile::fs_GetFile(PipeNames);
					CStr StdInPipeName = NMib::NFile::CFile::fs_GetPath(PipeNames) + "/NamePipeStdIn_" + NMib::NFile::CFile::fs_GetFile(PipeNames);

					// Std in pipe has to be opened first, otherwise it might hang
					if (!StdErrPipeName.f_IsEmpty())
					{					
						int Pipe = open(StdErrPipeName.f_GetStr(), O_CLOEXEC|O_WRONLY, S_IWUSR);
						
						if (Pipe == -1)
						{
							Error = fg_FormatErrno("open (named stderr pipe)", errno);
						}
						else
						{						
							if (dup2(Pipe, 2) == -1)
							{
								Error = fg_FormatErrno("dup2 (named stderr pipe)", errno);
							}
						}
					}
					
					DMibConOutRaw((CFStr256::CFormat("bdda0079-b6eb-41ac-88d0-01b50e8be939 {nfh} {}\n") << (mint)getpid() << Error.f_ReplaceChar('\n', '\r')).f_GetStr());

					if (!StdInPipeName.f_IsEmpty())
					{
						int Pipe = open(StdInPipeName.f_GetStr(), O_CLOEXEC|O_RDONLY, S_IRUSR);
							
						if (Pipe == -1)
						{
							Error = fg_FormatErrno("open (named stdin pipe)", errno);
						}
						else
						{
							if (dup2(Pipe, 0) == -1)
							{
								Error = fg_FormatErrno("dup2 (named stdin pipe)", errno);
							}
						}
					}


				}
				else
					DMibConOutRaw((CFStr256::CFormat("bdda0079-b6eb-41ac-88d0-01b50e8be939 {nfh} {}\n") << (mint)getpid() << Error.f_ReplaceChar('\n', '\r')).f_GetStr());

				
				break;
			}
		}
	}
	
	pSystem->f_LoadLibraries();
	
	pSystem->f_Init();
	
	pSystem->f_InitModule();
	pSystem->f_InitModuleThreaded();
	
}

bool g_bSysDeleted = false;

void NSys::fg_PreDestroyHeap()
{
}

void NSys::fg_DestroySystem()
{
	if (g_bCreatedSystem && !g_bSysDeleted)
	{
		g_bSysDeleted = true;

		auto pSys = fg_GetLocalSys();

		pSys->f_DestroyThreadSpecific();
		
		pSys->f_ExitModule();
		
		// We need to flush these before the buffer memory is deleted
		fflush(stdout);
		fflush(stderr);
		
		fg_DestroyBreakpad();
		
		pSys->f_Destruct();
		pSys->~CSystemEmscripten();

		g_VirtualMap.f_Destruct();
		g_VirtualMapLock.f_Destruct();
		//		g_pSys = nullptr;
	}
}

bint NSys::fg_System_BeingDebugged()
{
	CStrNonTracked StatusPath = CStrNonTracked::CFormat("/proc/{}/status") << (mint)getpid();
	auto FileData = fg_ReadProcFS<CStrNonTracked>(StatusPath);
	
	auto pParse = FileData.f_GetArray();
	
	while (*pParse)
	{
		auto *pStart = pParse;
		fg_ParseToEndOfLine(pParse);
		if (fg_StrStartsWith(pStart, "TracerPid:"))
		{
			pStart += fg_StrLen("TracerPid:");
			if (pStart < pParse)
			{
				fg_ParseWhiteSpace(pStart);
				if (fg_CharIsNumber(*pStart))
				{
					CFStr256 Str(pStart, pParse-pStart);
					
					uint32 PID = Str.f_ToInt(uint32(0));
					
					if (PID)
						return true;
				}
			}
		}
		fg_ParseEndOfLine(pParse);
	}
	
	return false;
}

bool NSys::fg_Process_IsRunning(mint _ProcessID)
{
	try
	{
		CStrNonTracked FileName = CStrNonTracked::CFormat("/proc/{}/stat") << _ProcessID;

		if (!NMib::NFile::CFile::fs_FileExists(FileName))
			return false;

		auto FileData = fg_ReadProcFS<CStrNonTracked>(FileName);
		
		NMib::NStr::CStrPtr Data;
		Data.f_SetConstPtr(FileData.f_GetArray(), FileData.f_GetLen());
		
		aint nParsed;
		int32 pid = 0;
		CStr comm;
		CStr state;
		
		(
			NMib::NStr::CStrPtr::CParse("{} ({}) {} ") 
			>> pid 
			>> comm 
			>> state 
		).f_Parse(Data, nParsed);
		
		if (nParsed != 3)
			return false;
		
		if (state == "Z")
			return false;
		return true;
	}
	catch (NException::CException const &)
	{
	}
	
	return false;
}



////file handling
namespace NMib
{
	namespace NSys
	{
		NMib::NStr::CStr fg_Process_GetOperatingSystemTag(int32 _MajorMax, int32 _MinorMax)
		{
			int Major, Minor, Fix;
			NProcess::EOperatingSystemArch Arch;
			fg_System_GetOperatingSystemVersion(Major, Minor, Fix, Arch);
			
			if (Major > _MajorMax || (Major == _MajorMax && Minor > _MinorMax))
			{
				Major = _MajorMax;
				Minor = _MinorMax;
			}

			return (CStr::CFormat("Linux{}.{}") << Major << Minor).f_GetStr();
		}

		NMib::NStr::CStr fg_Process_GetOperatingSystemDescription()
		{
			int Major, Minor, Fix;
			EOperatingSystemArch Arch;
			fg_System_GetOperatingSystemVersion(Major, Minor, Fix, Arch);

			return (CStr::CFormat("Linux {}.{}") << Major << Minor).f_GetStr();
		}
	} // Namespace NSys
} // Namespace NMib

namespace
{
	CStr fg_GetProgramUserName()
	{
		NMib::NStr::CStr FileName = NMib::NFile::CFile::fs_GetFileNoExt(NSys::NFile::fg_GetProgramPath());
		aint iFind = FileName.f_FindReverse("_x");
		if (iFind >= 0)
			FileName = FileName.f_Left(iFind);
		
		return "." + FileName;
	}

	CStrNonTracked fg_GetProgramUserNameNonTracked()
	{
		NMib::NStr::CStrNonTracked FileName = NMib::NFile::CFile::fs_GetFileNoExt(NSys::NFile::fg_GetProgramPathNonTracked());
		aint iFind = FileName.f_FindReverse("_x");
		if (iFind >= 0)
			FileName = FileName.f_Left(iFind);
		
		return "." + FileName;
	}
}

#include <pwd.h>

NMib::NStr::CStr NSys::NFile::fg_GetUserHomeDirectory()
{
	NMib::NStr::CStr HomeDir = fg_GetSys()->f_GetEnvironmentVariable("HOME");
	
	if (!HomeDir.f_IsEmpty())
		return HomeDir;

	struct passwd *pPasswd = getpwuid(getuid());
	if (pPasswd && pPasswd->pw_dir && pPasswd->pw_dir[0])
		return CStr(pPasswd->pw_dir);
	
	return fg_GetProgramDirectory();
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
	NMib::NStr::CStrNonTracked HomeDir = fg_Process_GetEnvironmentVariable_NonProtected(CStrNonTracked("HOME"));
	
	if (!HomeDir.f_IsEmpty())
		return HomeDir;
	
	struct passwd *pPasswd = getpwuid(getuid());
	if (pPasswd && pPasswd->pw_dir && pPasswd->pw_dir[0])
		return CStrNonTracked(pPasswd->pw_dir);

	return fg_GetProgramDirectoryNonTracked();
}

NMib::NStr::CStr NSys::NFile::fg_GetUserLocalProgramDirectory()
{
	NMib::NStr::CStr FileName = fg_GetProgramUserName();

	return NMib::NFile::CFile::fs_AppendPath(fg_GetUserHomeDirectory(), FileName);
}

NMib::NStr::CStr NSys::NFile::fg_GetUserLocalProgramCacheDirectory()
{
	NMib::NStr::CStr FileName = fg_GetProgramUserName();

	return NMib::NFile::CFile::fs_AppendPath(fg_GetUserHomeDirectory(), FileName);
}

NMib::NStr::CStrNonTracked NSys::NFile::fg_GetUserLocalProgramDirectoryNonTracked()
{
	NMib::NStr::CStrNonTracked FileName = fg_GetProgramUserNameNonTracked();

	return NMib::NFile::CFile::fs_AppendPath(fg_GetUserHomeDirectoryNonTracked(), FileName);
}

NMib::NStr::CStrNonTracked NSys::NFile::fg_GetUserLocalProgramCacheDirectoryNonTracked()
{
	NMib::NStr::CStrNonTracked FileName = fg_GetProgramUserNameNonTracked();

	return NMib::NFile::CFile::fs_AppendPath(fg_GetUserHomeDirectoryNonTracked(), FileName);
}


NStr::CStr NSys::NFile::fg_GetUserProgramDirectory()
{
	return NSys::NFile::fg_GetUserLocalProgramDirectory();
}

NStr::CStrNonTracked NSys::NFile::fg_GetUserProgramDirectoryNonTracked()
{
	return NSys::NFile::fg_GetUserLocalProgramDirectoryNonTracked();
}


NStr::CStr NSys::NFile::fg_GetTemporaryDirectory()
{
	NMib::NStr::CStr TmpDir = fg_GetSys()->f_GetEnvironmentVariable("TMPDIR");
	if (TmpDir.f_IsEmpty())
		TmpDir = fg_GetSys()->f_GetEnvironmentVariable("TMP");
	if (TmpDir.f_IsEmpty())
		TmpDir = fg_GetSys()->f_GetEnvironmentVariable("TEMP");
	if (TmpDir.f_IsEmpty())
		TmpDir = fg_GetSys()->f_GetEnvironmentVariable("TEMPDIR");
	if (!TmpDir.f_IsEmpty())
		return NMib::NFile::CFile::fs_AppendPath(TmpDir, fg_GetProgramUserName());
	return "/tmp";
}

NStr::CStrNonTracked NSys::NFile::fg_GetTemporaryDirectoryNonTracked()
{
	NMib::NStr::CStrNonTracked TmpDir = fg_Process_GetEnvironmentVariable_NonProtected(CStrNonTracked("TMPDIR"));
	if (TmpDir.f_IsEmpty())
		TmpDir = fg_Process_GetEnvironmentVariable_NonProtected(CStrNonTracked("TMP"));
	if (TmpDir.f_IsEmpty())
		TmpDir = fg_Process_GetEnvironmentVariable_NonProtected(CStrNonTracked("TEMP"));
	if (TmpDir.f_IsEmpty())
		TmpDir = fg_Process_GetEnvironmentVariable_NonProtected(CStrNonTracked("TEMPDIR"));
	if(!TmpDir.f_IsEmpty()) 
		return NMib::NFile::CFile::fs_AppendPath(TmpDir, fg_GetProgramUserNameNonTracked());
	return "/tmp";
}


namespace NMib
{
	namespace NSys
	{
		namespace NFile
		{
			template <typename tf_CStr>
			tf_CStr fg_GetProgramDirectoryGeneral()
			{
				tf_CStr ExePath = typename tf_CStr::CFormat("/proc/{}/exe") << (mint)getpid();
				tf_CStr FullPath = fg_ResolveSymbolicLink<tf_CStr>(ExePath);
				return NMib::NFile::CFile::fs_GetPath(FullPath);
			}
			template <typename tf_CStr>
			tf_CStr fg_GetProgramPathGeneral()
			{
				tf_CStr ExePath = typename tf_CStr::CFormat("/proc/{}/exe") << (mint)getpid();
				tf_CStr FullPath = fg_ResolveSymbolicLink<tf_CStr>(ExePath);
				return FullPath;
			}
		}
	}
}

CStr NSys::NFile::fg_GetProgramDirectory()
{
	return fg_GetProgramDirectoryGeneral<CStr>();
}

CStrNonTracked NSys::NFile::fg_GetProgramDirectoryNonTracked()
{
	return fg_GetProgramDirectoryGeneral<CStrNonTracked>();
}


CStr NSys::NFile::fg_GetProgramPath()
{
	return fg_GetProgramPathGeneral<CStr>();
}

CStrNonTracked NSys::NFile::fg_GetProgramPathNonTracked()
{
	return fg_GetProgramPathGeneral<CStrNonTracked>();
}

NMib::NStr::CStr NSys::NFile::fg_GetModulePath(void *_pCode)
{
	Dl_info ModuleInfo;
	if (dladdr(_pCode, &ModuleInfo) != 0)
	{
		if ((mint)ModuleInfo.dli_fbase == g_MainModuleBase)
			return fg_GetProgramPath();
		return ModuleInfo.dli_fname;
	}
	else
		return NMib::NStr::CStr();
}

NMib::NStr::CStrNonTracked NSys::NFile::fg_GetModulePathNonTracked(void *_pCode)
{
	Dl_info ModuleInfo;
	if (dladdr(_pCode, &ModuleInfo) != 0)
	{
		if ((mint)ModuleInfo.dli_fbase == g_MainModuleBase)
			return fg_GetProgramPathNonTracked();
		return ModuleInfo.dli_fname;
	}
	else
		return NMib::NStr::CStrNonTracked();
}

void NSys::NFile::fg_Copy(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo)
{
	NMib::NFile::CFile::fs_CopyFileRaw(_FileFrom, _FileTo); // No good way to do this on Linux unless you have kernel 2.6.33 or later (sendfile)
	
	// Copy the attributes
	{
		NMib::NFile::EFileAttrib Attribs = NMib::NFile::CFile::fs_GetAttributes(_FileFrom);
		NMib::NFile::CFile::fs_SetAttributes(_FileTo, Attribs);
	}
}


void NSys::NFile::fg_Rename(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo)
{
	if (rename(_FileFrom, _FileTo))
		DMibErrorFile(fg_FormatErrno(CStr::CFormat("rename('{}', '{}')") << _FileFrom << _FileTo, errno));
}


void NSys::NFile::fg_Copy(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo, NMib::NFile::CFileProgress &_Progress)
{
	NMib::NFile::CFile::fs_CopyFileRaw(_FileFrom, _FileTo); // No good way to do this on Linux unless you have kernel 2.6.33 or later (sendfile)
	
	// Copy the attributes
	{
		NMib::NFile::EFileAttrib Attribs = NMib::NFile::CFile::fs_GetAttributes(_FileFrom);
		NMib::NFile::CFile::fs_SetAttributes(_FileTo, Attribs);
	}
}

void NSys::NFile::fg_Rename(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo, NMib::NFile::CFileProgress &_Progress)
{
	if (rename(_FileFrom, _FileTo))
		DMibErrorFile(fg_FormatErrno(CStr::CFormat("rename('{}', '{}')") << _FileFrom << _FileTo, errno));
}

void *NSys::NFile::fg_ChangeNotification_Open(const CStr &_FileName, NMib::NFile::EFileChange _OpenFlags, NMib::NThread::CSemaphoreReportableAggregate *_pReportTo)
{
	DMibError("Not implemented");
	return nullptr;
}

void NSys::NFile::fg_ChangeNotification_Close(void *_pNotification)
{
	
}

bint NSys::NFile::fg_ChangeNotification_Changed(void *_pNotification)
{
	return false;
}

bint NSys::NFile::fg_ChangeNotification_GetNotification(void *_pNotification, NMib::NStr::CStr &_Path, NMib::NFile::EFileChangeNotification &_Notification, NMib::NStr::CStr &_PathFrom)
{
	return false;
}
	
bool NSys::NFile::fg_ChangeNotification_Supported()
{
	return false;
}

// *************************************************************************************************************************
// Net Implementation
// *************************************************************************************************************************

NSys::NNet::CAddress NSys::NNet::fg_CreateAddress(::NMib::NNet::ENetAddressType _Type, void const* _pData, mint _nDataBytes)
{
	DMibError("Not implemented");
	return nullptr;
}

NSys::NNet::CAddress NSys::NNet::fg_DuplicateAddress(NSys::NNet::CAddress _Address)
{
	DMibError("Not implemented");
	return nullptr;
}

::NMib::NNet::ENetAddressType NSys::NNet::fg_GetAddressType(NSys::NNet::CAddress _Address)
{
	DMibError("Not implemented");
	return ::NMib::NNet::ENetAddressType_None;
}

bint NSys::NNet::fg_GetAddressRaw(NSys::NNet::CAddress _Address, ::NMib::NNet::ENetAddressType _ExpectedType, void* _opRawData, mint _nDataBytes)
{
	DMibError("Not implemented");
	return false;
}

NSys::NNet::CAddress NSys::NNet::fg_SetAddressRaw(NSys::NNet::CAddress _Address, ::NMib::NNet::ENetAddressType _Type, void const* _pRawData, mint _nDataBytes)
{
	DMibError("Not implemented");
	return nullptr;
}

NSys::NNet::CAddress NSys::NNet::fg_ResolveAddress(const NMib::NStr::CStr &_Address, ::NMib::NNet::ENetAddressType _PreferType)
{
	DMibError("Not implemented");
	return nullptr;
}

void *NSys::NNet::fg_AsyncResolveAddress_Open(const NMib::NStr::CStr &_Address, ::NMib::NNet::ENetAddressType _PreferType, NMib::NFunction::TCFunction<void ()>&& _fOnFinish)
{
	DMibError("Not implemented");
	return nullptr;
}

bint NSys::NNet::fg_AsyncResolveAddress_GetResult(void *_pResolver, NSys::NNet::CAddress& _opAddress, NMib::NStr::CStr &_Error)
{
	DMibError("Not implemented");
	return false;
}

void NSys::NNet::fg_AsyncResolveAddress_Close(void *_pResolver)
{
	DMibError("Not implemented");
}

int NSys::NNet::fg_CompareAddresses(NSys::NNet::CAddress _pFirst, NSys::NNet::CAddress _pSecond)
{
	DMibError("Not implemented");
	return 0;
}

void NSys::NNet::fg_FreeAddress(NSys::NNet::CAddress _Address) // It is OK to free a nullptr address
{
	DMibError("Not implemented");
}

NMib::NStr::CStr NSys::NNet::fg_GetAddressString(NSys::NNet::CAddress _Address, bint _bIncludeType)
{
	DMibError("Not implemented");
	return "";
}

// Connection Operations
void *NSys::NNet::fg_Connect(NSys::NNet::CAddress _Address, NMib::NThread::CSemaphoreReportableAggregate *_pReportTo, NSys::NNet::CAddress _BindAddress) // Report to the supplied event when new data is received or when we are ready to send new dat
{
	DMibError("Not implemented");
	return nullptr;
}

void *NSys::NNet::fg_AsyncConnect(NSys::NNet::CAddress _Address, NMib::NThread::CSemaphoreReportableAggregate *_pReportTo, NSys::NNet::CAddress _BindAddress) // Report to the supplied event when new data is received or when we are ready to send new data and when the connection is connecte
{
	DMibError("Not implemented");
	return nullptr;
}

void *NSys::NNet::fg_Listen(NSys::NNet::CAddress _Address, NMib::NThread::CSemaphoreReportableAggregate *_pReportTo, NMib::NNet::ENetFlag _Flags) // Report to the supplied event when a new connection has arrive
{
	DMibError("Not implemented");
	return nullptr;
}
void *NSys::NNet::fg_ListenDatagram(NSys::NNet::CAddress _Address, NMib::NThread::CSemaphoreReportableAggregate *_pReportTo, NMib::NNet::ENetFlag _Flags)
{
	DMibError("Not implemented");
	return nullptr;
}

void *NSys::NNet::fg_Accept(void *_pSocket, NMib::NThread::CSemaphoreReportableAggregate *_pReportTo) // Report to the supplied event when new data is received or when we are ready to send new dat
{
	DMibError("Not implemented");
	return nullptr;
}

void NSys::NNet::fg_Close(void *_pSocket) // Closes the socket and connectio
{
	DMibError("Not implemented");
}

mint NSys::NNet::fg_Receive(void *_pSocket, void *_pData, mint _DataLen) // Returns bytes receive
{
	DMibError("Not implemented");
	return 0;
}

mint NSys::NNet::fg_Send(void *_pSocket, const void *_pData, mint _DataLen) // Returns bytes sen
{
	DMibError("Not implemented");
	return 0;
}

mint NSys::NNet::fg_SendDatagram(void *_pSocket, NSys::NNet::CAddress _Address, const void *_pData, mint _DataLen) // Returns bytes sen
{
	DMibError("Not implemented");
	return 0;
}

mint NSys::NNet::fg_ReceiveDatagram(void *_pSocket, NSys::NNet::CAddress _Address, void *_pData, mint _DataLen) // Returns bytes sen
{
	DMibError("Not implemented");
	return 0;
}

// Socket Properties & State

void NSys::NNet::fg_SetReportTo(void *_pSocket, NMib::NThread::CSemaphoreReportableAggregate *_pReportTo) // Report to the supplied event when new data is received or when we are ready to send new data			
{
	DMibError("Not implemented");
}

NMib::NNet::ENetTCPState NSys::NNet::fg_GetState(void *_pSocket) // Get the state of data availabl
{
	DMibError("Not implemented");
	return NMib::NNet::ENetTCPState_None;
}

NMib::NStr::CStr NSys::NNet::fg_GetCloseReason(void *_pSocket)
{
	DMibError("Not implemented");
	return "";
}

void *NSys::NNet::fg_InheritHandle(void *_pSocket, NMib::NThread::CSemaphoreReportableAggregate *_pReportTo)
{
	DMibError("Not implemented");
	return nullptr;
}

void *NSys::NNet::fg_InheritHandle2(void *_pSocket, NMib::NThread::CSemaphoreReportableAggregate *_pReportTo)
{
	DMibError("Not implemented");
	return nullptr;
}

void *NSys::NNet::fg_GiveUpForInherit(void *_pSocket)
{
	DMibError("Not implemented");
	return nullptr;
}

void *NSys::NNet::fg_GetOSSocket(void *_pSocket)
{
	DMibError("Not implemented");
	return nullptr;
}

NSys::NNet::CAddress NSys::NNet::fg_GetPeerAddress(void *_pSocket)
{
	DMibError("Not implemented");
	return nullptr;
}

uint32 NSys::NNet::fg_GetListenPort(void *_pSocket)
{
	DMibError("Not implemented");
	return 0;
}

void NSys::NFile::fg_FileEnumOtherHandles(const NMib::NStr::CStr &_FileName, NContainer::TCVector<NMib::NFile::CFileHandle> &_HandleInfo)
{
	DMibPDebugBreak; // Not implemented
}

void NSys::NFile::fg_FileEnumOtherHandles(void *_pFile, NContainer::TCVector<NMib::NFile::CFileHandle> &_HandleInfo)
{
	DMibPDebugBreak; // Not implemented
}

void NSys::fg_Thread_Yield()
{
	sched_yield();
}

#include <locale.h>

uint16 NSys::fg_Langague_GetSystemLanguage(NMib::NStr::CStr &_Language)
{
	_Language = setlocale(LC_CTYPE, NULL);
	aint DotPos = _Language.f_Find(".");
	if (DotPos != -1)
		_Language = _Language.f_Left(DotPos);
	_Language = _Language.f_Replace("_", "-");
	return 0;
}

void* NSys::fg_GetExeData(char const* _pSegment, char const* _pSection, unsigned long long& _nDataBytes)
{
	DMibPDebugBreak;
	
	return nullptr;
}


void NSys::fg_Debug_SetCrashDumpUserNotifyFunction(NMib::NSys::FCrashDumpUserNotify *_pCrashDumpUserNotify)
{

}

void NSys::fg_Debug_SetCrashDumpUserNotifyFormats(NMib::NStr::CStrNonTracked const &_CustomMessage, NMib::NStr::CStrNonTracked const &_CanContinueMessage, NMib::NStr::CStrNonTracked const &_NoContinueMessage)
{

}

mint NSys::fg_Thread_GetVirtualCores()
{
	return 1;
}

mint NSys::fg_Thread_GetPhysicalCores()
{
	return 1;
}

NMib::NStr::CStr NSys::fg_System_GetCPUName()
{
	return "Emscripten";
}

namespace NMib
{
	namespace NSys
	{
		void *fg_System_CPUUsageMonitor_Open()
		{
			DMibError("Not implemented");
			return nullptr;
		}
		
		void fg_System_CPUUsageMonitor_Close(void *_pHandle)
		{
			DMibError("Not implemented");
		}
		
		NSystem::CSystemCPUUsage fg_System_CPUUsageMonitor_GetUsage(void *_pHandle, bool &_bChanged)
		{
			DMibError("Not implemented");
			return NSystem::CSystemCPUUsage();
		}
		
	}
}

void NSys::fg_Mem_EnableMemoryToucher(bool _bEnabled, fp64 _CPUUsage)
{
}

void NSys::fg_TerminateProcess(aint _ExitCode)
{
//	fflush(stdout);
//	fflush(stderr);
//	_exit(_ExitCode);
	raise(SIGKILL);
}

void NSys::fg_Debug_StartDeadlockDetector(fp64 _Timeout)
{
}

void NSys::fg_Debug_NotDeadlocked()
{
}

void NSys::fg_Debug_StopDeadlockDetector()
{
}

uint64 NSys::fg_Process_GetPhysicalMemory()
{
    return uint64(sysconf(_SC_PHYS_PAGES)) * uint64(sysconf(_SC_PAGE_SIZE));
}

NMib::NFile::ECheckFileRights NSys::NFile::fg_CheckFileRights( const CStr & _File, NMib::NFile::EFileRight _Rights)
{
    if (NMib::NFile::CFile::fs_FileExists(_File))
        return NMib::NFile::ECheckFileRights_Access; // TODO
    else
        return NMib::NFile::ECheckFileRights_DoesNotExist; // TODO
}

ch8 const *NSys::NFile::fg_GetDllExtension()
{
	return ".js";
}

void NMib::NSys::fg_UserManagement_CreateUser(
								  NMib::NStr::CStr const &_InGroupName,
								  NMib::NStr::CStr const &_UserName,
								  NMib::NStr::CStr const &_Password,
								  NMib::NStr::CStr const &_FullName,
								  NMib::NStr::CStr const &_HomeDirectory,
								  NMib::NStr::CStr &_ReturnUID)
{
	DMibError("Not implemented");
}

void NMib::NSys::fg_UserManagement_DeleteUser(NMib::NStr::CStr const &_UserName)
{
	DMibError("Not implemented");
}

void NMib::NSys::fg_UserManagement_CreateGroup(NMib::NStr::CStr const &_GroupName, NMib::NStr::CStr &_ReturnGID)
{
	DMibError("Not implemented");
}

void NMib::NSys::fg_UserManagement_DeleteGroup(NMib::NStr::CStr const &_GroupName)
{
	DMibError("Not implemented");
}


#if 0
void fg_UserManagement_SetPrimaryGroup(NMib::NStr::CStr const &_GroupName, NMib::NStr::CStr const &_UserName)
{
	DMibError("Not implemented");
}
#endif

void NMib::NSys::fg_UserManagement_AddUserToGroup(NMib::NStr::CStr const &_GroupName, NMib::NStr::CStr const &_UserName)
{
	DMibError("Not implemented");
}

void NMib::NSys::fg_UserManagement_RemoveUserFromGroup(NMib::NStr::CStr const &_GroupName, NMib::NStr::CStr const &_UserName)
{
	DMibError("Not implemented");
}

bint NMib::NSys::fg_UserManagement_IsValidName(NMib::NStr::CStr const &_Name)
{
	DMibError("Not implemented");
	return true;
}

bint NMib::NSys::fg_ConsoleOutputValid()
{
	return true;
}

bint NMib::NSys::fg_ConsoleInputValid()
{
	return true;
}

bint NMib::NSys::fg_ConsoleErrorOutputValid()
{
	return true;
}

namespace NMib
{
	namespace NSys
	{
		
		/*
		 Basic interface for storing secure passwords on a per-user, per-application basis.
		 */
		
		ESecurePassword fg_SecurePassword_SetLocation(NMib::NStr::CStr const& _Location)
		{
			DMibError("Not implemented");
			return ESecurePassword_Failure;
		}
		
		ESecurePassword fg_SecurePassword_Store(CStr const& _Key, CStrSecure const& _Password)
		{
			DMibError("Not implemented");
			return ESecurePassword_Failure;
		}
		
		ESecurePassword fg_SecurePassword_Remove(CStr const& _Key)
		{
			DMibError("Not implemented");
			return ESecurePassword_Failure;
		}
		
		ESecurePassword fg_SecurePassword_Get(CStr const& _Key, CStrSecure& _oPassword)
		{
			DMibError("Not implemented");
			return ESecurePassword_Failure;
		}
		
		ESecurePassword fg_SecurePassword_Exists(CStr const& _Key)
		{
			DMibError("Not implemented");
			return ESecurePassword_Failure;
		}		

		// Desktop environment
		EDesktopEnvironment fg_DesktopEnvironment_Get()
		{
			return EDesktopEnvironment_Emscripten;
		}

	}
}

namespace NMib
{
	namespace NSys
	{
		void * g_pCrossModuleMemoryManagerInterface = nullptr;
	}
}

#ifndef DMibDynamicLibrary

extern "C" module_export void *fg_MalterlibGetCrossModuleMemoryManagerInterface()
{
	return NMib::NSys::g_pCrossModuleMemoryManagerInterface;
}

#endif

void *NSys::fg_Process_GetCrossModuleMemoryManagerInterface()
{
#ifdef DMibDynamicLibrary
	void * ( *fMalterlibGetCrossModuleMemoryManagerInterface)();
	(void * &)fMalterlibGetCrossModuleMemoryManagerInterface = dlsym(RTLD_DEFAULT, "fg_MalterlibGetCrossModuleMemoryManagerInterface");
	
	auto pRet = g_pCrossModuleMemoryManagerInterface;
	
	if (fMalterlibGetCrossModuleMemoryManagerInterface)
		pRet = fMalterlibGetCrossModuleMemoryManagerInterface();
	
	return pRet;
#else
	return fg_MalterlibGetCrossModuleMemoryManagerInterface();
#endif
}

void NSys::fg_Process_SetCrossModuleMemoryManagerInterface(void *_pInterface)
{
	DMibFastCheck(!fg_GetSys()->f_IsDll());
	
	g_pCrossModuleMemoryManagerInterface = _pInterface;
	
	DMibFastCheck(fg_Process_GetCrossModuleMemoryManagerInterface() == _pInterface);
}


// Process kill tree
namespace NMib
{
	namespace NSys
	{
		NContainer::TCVector<NProcess::CProcessInfo> fg_Process_Enum(NProcess::EProcessInfoFlag _ToGet, NContainer::TCVector<NProcess::CProcessInfo> * _pOldEnum)
		{
			return NContainer::TCVector<NProcess::CProcessInfo>();
		}		
	}
}


void *NSys::fg_Thread_Create(FThreadProc *_pThreadProc, void *_pParam, mint _Priority, mint _StackSize, bint _bSuspended, const ch8 *_pThreadName, mint _Affinity, mint &_ThreadID)
{
	DMibPDebugBreak;
	return nullptr;
}

void NSys::fg_Thread_SetAffinity(void *_pThread, mint _Affinity)
{
	DMibPDebugBreak;
}


void *NSys::fg_Thread_BeginDestroy(void *_pThread)
{
	DMibPDebugBreak;
	return nullptr;
}

void NSys::fg_Thread_WillNotBlockUntilExit(void *_pThreadDestroyContext)
{
	DMibPDebugBreak;
}

void NSys::fg_Thread_BlockUntilExit(void *_pThreadDestroyContext)
{
	DMibPDebugBreak;
}

void NSys::fg_Thread_EndDestroy(void *_pThreadDestroyContext)
{
	DMibPDebugBreak;
}
void NSys::fg_Thread_SetPriority(void *_pThread, mint _Priority)
{
	DMibPDebugBreak;
}

void NSys::fg_Thread_Destroy(void *_pThread)
{
	DMibPDebugBreak;
}

void *NSys::fg_Event_Alloc(bint _bInitialSignal)
{
	DMibPDebugBreak;
	return nullptr;
}

void NSys::fg_Event_Free(void *_pEvent)
{
	DMibPDebugBreak;
}

void NSys::fg_Event_PrepareForFork(void *_pEvent)
{
	DMibPDebugBreak;
}

void NSys::fg_Event_Reinit(void *_pEvent)
{
	DMibPDebugBreak;
}

void NSys::fg_Event_SetSignaled(void * _pEvent)
{
	DMibPDebugBreak;
}

void NSys::fg_Event_ResetSignaled(void * _pEvent)
{
	DMibPDebugBreak;
}

void NSys::fg_Event_Wait(void * _pEvent)
{
	DMibPDebugBreak;
}

bint NSys::fg_Event_WaitTimeout(void * _pEvent, fp64 _Timeout)
{
	DMibPDebugBreak;
	return false;
}

bint NSys::fg_Event_TryWait(void * _pEvent)
{
	DMibPDebugBreak;
	return false;
}
