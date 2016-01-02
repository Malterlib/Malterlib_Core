// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <DbgHelp.h>

#define DMibAllowCodeStandardViolations 1

extern int g_MalterlibDisableStackTraceContext;

class CStackTraceContext
{
public:
	CStackTraceContext()
	{
		m_bInitialized = false;
		m_bInitializedDll = false;
		SymInitialize = nullptr;
		SymCleanup = nullptr;
		m_hDbgHelp = nullptr;
		m_hSymSrv = nullptr;
		m_pSymbolInfo = nullptr;
		MiniDumpWriteDump = nullptr;
		m_bFailedInitialize = false;
		m_bFailedInitializeDll = false;
		m_hProcess = INVALID_HANDLE_VALUE;
		m_Timer.f_Start();
	}

	~CStackTraceContext()
	{
		if (m_bInitialized)
			SymCleanup(m_hProcess);

		if (m_hProcess != INVALID_HANDLE_VALUE)
		{
			CloseHandle(m_hProcess);
		}

		if (m_pSymbolInfo)
			CAllocator_NonTrackedHeap::f_Free(m_pSymbolInfo);

		if (m_hDbgHelp)
			FreeLibrary(m_hDbgHelp);
		if (m_hSymSrv)
			FreeLibrary(m_hSymSrv);

		f_RemoveUnused();

		while (m_TraceInfoTree.f_GetRoot())
		{
			CLocalStackTraceInfo *pInfo = m_TraceInfoTree.f_GetRoot();
			if (pInfo->m_pFunctionName)
				CAllocator_NonTrackedHeap::f_Free((ch8 *)pInfo->m_pFunctionName);
			if (pInfo->m_pModuleName)
				CAllocator_NonTrackedHeap::f_Free((ch8 *)pInfo->m_pModuleName);
			if (pInfo->m_pSourceFileName)
				CAllocator_NonTrackedHeap::f_Free((ch8 *)pInfo->m_pSourceFileName);

			m_TraceInfoTree.f_Remove(pInfo);

			TCUniquePointer<CLocalStackTraceInfo, CAllocator_NonTrackedHeap> pInfoDelete = fg_Explicit(pInfo);
		}

	}

	NTime::CClock m_Timer;

	class CAVLCompare_CLocalStackTraceInfo;

	class CLocalStackTraceInfo : public CStackTraceInfo
	{
	public:
		CLocalStackTraceInfo()
		{
			m_pFunctionName = nullptr;
			m_pModuleName = nullptr;
			m_pSourceFileName = nullptr;
			this->m_pContext = nullptr;
			m_SourceLine = 0;
			m_RefCount = 1;
		}

		DMibIntrusiveLink(CLocalStackTraceInfo, NIntrusive::TCAVLLink<>, m_AvlLink);
		DMibListLinkD_Link(CLocalStackTraceInfo, m_UnusedList);
		mint m_Address;
		mint m_RefCount;
	};

	class CAVLCompare_CLocalStackTraceInfo
	{
	public:
		inline_small const mint &operator () (CLocalStackTraceInfo const &_Node) const
		{
			return _Node.m_Address;
		}
	};

	NIntrusive::TCAVLTree<CLocalStackTraceInfo::CLinkTraits_m_AvlLink, CAVLCompare_CLocalStackTraceInfo> m_TraceInfoTree;
	DMibListLinkD_List(CLocalStackTraceInfo, m_UnusedList) m_Usused;

	NThread::CMutual m_Lock;

/*	typedef struct _IMAGEHLP_SYMBOL64 {
		DWORD                       SizeOfStruct;           // set to sizeof(IMAGEHLP_SYMBOL64)
		DWORD64                     Address;                // virtual address including dll base address
		DWORD                       Size;                   // estimated size of symbol, can be zero
		DWORD                       Flags;                  // info about the symbols, see the SYMF defines
		DWORD                       MaxNameLength;          // maximum size of symbol name in 'Name'
		CHAR                        Name[1];                // symbol name (null terminated string)
	} IMAGEHLP_SYMBOL64, *PIMAGEHLP_SYMBOL64;

	typedef struct _IMAGEHLP_LINE64 {
		DWORD                       SizeOfStruct;           // set to sizeof(IMAGEHLP_LINE64)
		PVOID                       Key;                    // internal
		DWORD                       LineNumber;             // line number in file
		PCHAR                       FileName;               // full filename
		DWORD64                     Address;                // first instruction of line
	} IMAGEHLP_LINE64, *PIMAGEHLP_LINE64;

	typedef enum {
		SymNone = 0,
		SymCoff,
		SymCv,
		SymPdb,
		SymExport,
		SymDeferred,
		SymSym,       // .sym file
		SymDia,
		SymVirtual,
		NumSymTypes
	} SYM_TYPE;

	typedef struct _IMAGEHLP_MODULE64 {
		DWORD                       SizeOfStruct;           // set to sizeof(IMAGEHLP_MODULE64)
		DWORD64                     BaseOfImage;            // base load address of module
		DWORD                       ImageSize;              // virtual size of the loaded module
		DWORD                       TimeDateStamp;          // date/time stamp from pe header
		DWORD                       Checksum;               // checksum from the pe header
		DWORD                       NumSyms;                // number of symbols in the symbol table
		SYM_TYPE                    SymType;                // type of symbols loaded
		CHAR                        ModuleName[32];         // module name
		CHAR                        ImageName[256];         // image name
		CHAR                        LoadedImageName[256];   // symbol file name
	} IMAGEHLP_MODULE64, *PIMAGEHLP_MODULE64;*/

	typedef BOOL (__stdcall FSymInitialize)(IN HANDLE hProcess, IN PWSTR UserSearchPath, IN BOOL fInvadeProcess);
	typedef BOOL (__stdcall FSymCleanup)(IN HANDLE hProcess);
	typedef BOOL (__stdcall FSymRefreshModuleList)(__in HANDLE hProcess);

	typedef BOOL (__stdcall FSymGetSymFromAddr64)(IN HANDLE hProcess,IN DWORD64 Address,OUT PDWORD64 Displacement,IN OUT PIMAGEHLP_SYMBOL64 Symbol);
	typedef BOOL (__stdcall FSymGetLineFromAddr64)(IN HANDLE hProcess,IN DWORD64 dwAddr, OUT PDWORD pdwDisplacement, OUT PIMAGEHLP_LINE64 Line);
	typedef BOOL (__stdcall FSymGetModuleInfo64)(IN HANDLE hProcess,IN DWORD64 qwAddr, OUT PIMAGEHLP_MODULE64 ModuleInfo);
	typedef BOOL (__stdcall FMiniDumpWriteDump)(HANDLE hProcess,DWORD ProcessId,HANDLE hFile,MINIDUMP_TYPE DumpType,PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

	typedef DWORD (__stdcall FUnDecorateSymbolName)(PCSTR DecoratedName, PSTR UnDecoratedName, DWORD UndecoratedLength, DWORD Flags);


	FSymInitialize *SymInitialize;
	FSymCleanup *SymCleanup;
	FSymRefreshModuleList *SymRefreshModuleList;
	FSymGetSymFromAddr64 *SymGetSymFromAddr64;
	FSymGetLineFromAddr64 *SymGetLineFromAddr64;
	FSymGetModuleInfo64 *SymGetModuleInfo64;
	FMiniDumpWriteDump *MiniDumpWriteDump;
	FUnDecorateSymbolName *UnDecorateSymbolName;

	HMODULE m_hDbgHelp;
	HMODULE m_hSymSrv;
	IMAGEHLP_SYMBOL64 *m_pSymbolInfo;
	HANDLE m_hProcess;



	bint m_bInitialized;
	bint m_bInitializedDll;
	bint m_bFailedInitialize;
	bint m_bFailedInitializeDll;

	bint f_InitDll(CFStr256 &_Error)
	{
		DMibLockTyped(NThread::CMutual, m_Lock);
		if (m_bInitializedDll)
			return true;

		if (m_bFailedInitializeDll)
			return false;

		auto InitLambda = [&]() -> bint
		{
			#ifdef DArchitecture_x64
				#define DMibPlatformDir "\\x64"
			#else
				#define DMibPlatformDir "\\x86"
			#endif

			CStrNonTracked ProgramRoot = fg_GetSys()->f_GetProgramRootNonTracked();
			fg_StrReplaceChar(ProgramRoot, '/', '\\');

			CStrNonTracked ProgramPath = NMib::NFile::CFile::fs_GetProgramDirectoryNonTracked();
			fg_StrReplaceChar(ProgramPath, '/', '\\');

			if (!m_hDbgHelp)
			{
				CStrNonTracked DebugHelpDLL = "\\DebugHelp" DMibPlatformDir "\\Dbghelp.dll"; 			

				m_hDbgHelp = (HMODULE)NSys::fg_LoadLibrary(ProgramRoot + DebugHelpDLL);

				if (!m_hDbgHelp)
					m_hDbgHelp = (HMODULE)NSys::fg_LoadLibrary(ProgramPath + DebugHelpDLL);

				if (!m_hDbgHelp)
					m_hDbgHelp = (HMODULE)NSys::fg_LoadLibrary(CStrNonTracked("Dbghelp.dll"));
			}

			// Apperantly some bug in Dbghelp.dll causes symsrv.dll to be unloaded while other modules are still holding a handle to it. Try to prevent this here by adding a reference to the library.
			if (!m_hSymSrv)
			{
				CStrNonTracked SymServDLL = "\\DebugHelp" DMibPlatformDir "\\SymSrv.dll"; 
					
				m_hSymSrv = (HMODULE)NSys::fg_LoadLibrary(ProgramRoot + SymServDLL);

				if (!m_hSymSrv)
					m_hSymSrv = (HMODULE)NSys::fg_LoadLibrary(ProgramPath + SymServDLL);

				if (!m_hSymSrv)
					m_hSymSrv = (HMODULE)NSys::fg_LoadLibrary(CStrNonTracked("SymSrv.dll"));
			}

			#undef DMibPlatformDir

			if (!m_hDbgHelp)
			{
				m_bFailedInitializeDll = true;
				_Error = "Dbghelp.dll not found";
				DMibDTrace("StackTrace: Failed to load DbgHelp.dll\n", 0);

				return false;
			}

			this->SymInitialize = (FSymInitialize*)GetProcAddress(m_hDbgHelp, "SymInitializeW");
			this->SymCleanup = (FSymCleanup*)GetProcAddress(m_hDbgHelp, "SymCleanup");
			this->SymRefreshModuleList = (FSymRefreshModuleList*)GetProcAddress(m_hDbgHelp, "SymRefreshModuleList");
			this->SymGetSymFromAddr64 = (FSymGetSymFromAddr64*)GetProcAddress(m_hDbgHelp, "SymGetSymFromAddr64");
			this->SymGetLineFromAddr64 = (FSymGetLineFromAddr64*)GetProcAddress(m_hDbgHelp, "SymGetLineFromAddr64");
			this->SymGetModuleInfo64 = (FSymGetModuleInfo64*)GetProcAddress(m_hDbgHelp, "SymGetModuleInfo64");
			this->MiniDumpWriteDump = (FMiniDumpWriteDump*)GetProcAddress(m_hDbgHelp, "MiniDumpWriteDump");
			this->UnDecorateSymbolName = (FUnDecorateSymbolName*)GetProcAddress(m_hDbgHelp, "UnDecorateSymbolName");
		

			if (!this->SymInitialize || !this->SymCleanup || !this->SymGetSymFromAddr64 || !this->SymGetLineFromAddr64 || !this->SymGetModuleInfo64 || !this->MiniDumpWriteDump || !this->SymRefreshModuleList)
			{
				m_bFailedInitializeDll = true;
				FreeLibrary(m_hDbgHelp);
				m_hDbgHelp = nullptr;
				_Error = "Dbghelp.dll dose not contain !SymInitialize || !SymCleanup || !SymGetSymFromAddr64 || !SymGetLineFromAddr64 || !SymGetModuleInfo64 || !MiniDumpWriteDump";
				DMibDTrace("---------------------------------------------------------------------------------------------------------------------\n", 0);
				for (int i = 0; i < 25; ++i)
				{
					DMibDTrace("StackTrace: DbgHelp.dll does not contain the needed functions\n", 0);

				}
				DMibDTrace("---------------------------------------------------------------------------------------------------------------------\n", 0);
				return false;
			}			
			return true;
		};

		bint bReturn = InitLambda();

		if (!bReturn)
			return false;

		m_bInitializedDll = true;
		return true;	
	}

	bint f_Init(CStrNonTracked &_Error)
	{
		DMibLockTyped(NThread::CMutual, m_Lock);
		if (m_bInitialized)
			return true;

		if (m_bFailedInitialize)
			return false;

		CFStr256 Error;
		if (!f_InitDll(Error))
		{
			_Error = Error;
			return false;
		}

		auto InitLambda = [&]() -> bint
		{
			CWStrNonTracked Strings = NSys::NFile::fg_GetProgramDirectoryNonTracked();

			CWStrNonTracked TempStr;
			GetEnvironmentVariableW(str_utf16("_NT_SYMBOL_PATH"), TempStr.f_GetStr(32768), 32768);

			if (TempStr.f_GetLen())
				Strings = Strings + ";" + TempStr;
			else
			{

				GetEnvironmentVariableW(str_utf16("_NT_ALTERNATE_SYMBOL_PATH"), TempStr.f_GetStr(32768), 32768);

				if (TempStr.f_GetLen())
					Strings = Strings + ";" + TempStr;
				else
				{
					GetEnvironmentVariableW(str_utf16("SystemRoot"), TempStr.f_GetStr(32768), 32768);

					if (TempStr.f_GetLen())
						Strings = Strings + ";" + TempStr + "\\Symbols";

					GetEnvironmentVariableW(str_utf16("PATH"), TempStr.f_GetStr(32768), 32768);

					if (TempStr.f_GetLen())
						Strings = Strings + ";" + TempStr;
				}
			}

			m_hProcess = OpenProcess(PROCESS_ALL_ACCESS, false, GetCurrentProcessId());
			if (m_hProcess == INVALID_HANDLE_VALUE)
			{
				DMibDTrace("StackTrace: SymInitialize failed\n", 0);
				_Error = "Could not open process handle";
				m_bFailedInitialize = true;
				return false;
			}

			if (!g_MalterlibDisableStackTraceContext)
			{
				if (!this->SymInitialize(m_hProcess, (ch16 *)Strings.f_GetStr(), false))
				{
					if (!this->SymInitialize(m_hProcess, nullptr, false))
					{
						_Error = CStrNonTracked::CFormat("SymInitialize failed with {}") << fg_Win32_GetLastErrorStr(GetLastError());
						DMibDTraceSafe("{}\n", _Error);
						m_bFailedInitialize = true;
						return false;
					}
				}

				if (!m_pSymbolInfo)
				{
					mint Size = sizeof(IMAGEHLP_SYMBOL64) + 4096;
					m_pSymbolInfo = (IMAGEHLP_SYMBOL64 *)CAllocator_NonTrackedHeap::f_Alloc(Size);
				}

				this->SymRefreshModuleList(m_hProcess);
			}
			return true;
		};

		bint bReturn = InitLambda();

		if (!bReturn)
			return false;

		m_bInitialized = true;
		return true;
	}


	void f_UndecorateName(const ch8 *_pName, NStr::CStr &_Destination)
	{
		CStr Dest;
		f_UndecorateName(_pName, Dest.f_GetStr(4096), 4096);
		Dest.f_SetModified();
		_Destination = Dest;
	}

	void f_UndecorateName(const ch8 *_pName, NStr::CStrNonTracked &_Destination)
	{
		CStrNonTracked Dest;
		f_UndecorateName(_pName, Dest.f_GetStr(4096), 4096);
		Dest.f_SetModified();
		_Destination = Dest;
	}

	void f_UndecorateName(const ch8 *_pName, ch8 *_pDestination, mint _MaxLen)
	{
		if (!m_bInitializedDll)
		{
			CFStr256 Temp;
			if (!f_InitDll(Temp))
			{
				*_pDestination = 0;
				return;
			}
		}
		if (!UnDecorateSymbolName)
		{
			*_pDestination = 0;
			return;
		}
		uint32 Flags = 0;
#ifndef DArchitecture_x64
		Flags |= UNDNAME_32_BIT_DECODE;
#endif
		mint nChars = UnDecorateSymbolName(_pName, _pDestination, _MaxLen, Flags);
		if (!nChars)
		{
			HRESULT LastError = GetLastError();
			DMibDTraceSafe("UnDecorateSymbolName: {}\r\n", fg_Win32_GetLastErrorStr(LastError));
		}
	}

	CLocalStackTraceInfo *f_AquireStackTraceInfo(mint _Address)
	{
		DMibLockTyped(NThread::CMutual, m_Lock);

		if (!m_bInitialized)
		{
			CStrNonTracked Temp;
			if (!f_Init(Temp))
				return nullptr;
		}

		if (!m_pSymbolInfo)
			return nullptr;

		CLocalStackTraceInfo *pLocalInfo = m_TraceInfoTree.f_FindEqual(_Address);
		if (pLocalInfo)
		{
			if ((++pLocalInfo->m_RefCount) == 1)
			{
				pLocalInfo->m_UnusedList.f_Unlink();
			}
			return pLocalInfo;
		}

		DWORD64 Displacement;			
		m_pSymbolInfo->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
		m_pSymbolInfo->MaxNameLength = 4096;
		if (!SymGetSymFromAddr64(m_hProcess, _Address, &Displacement, m_pSymbolInfo))
			return nullptr;

		pLocalInfo = new(CAllocator_NonTrackedHeap::f_Alloc(sizeof(CLocalStackTraceInfo))) CLocalStackTraceInfo();
		pLocalInfo->m_Address = _Address;
		m_TraceInfoTree.f_Insert(pLocalInfo);

		int Len = fg_StrLen(m_pSymbolInfo->Name);
		ch8 *pStr;
		pLocalInfo->m_pFunctionName = pStr = (ch8 *)CAllocator_NonTrackedHeap::f_Alloc(Len + 1);
		fg_MemCopy(pStr, m_pSymbolInfo->Name, Len);
		pStr[Len] = 0;

		{
			IMAGEHLP_LINE64 LineInfo;
			LineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
			DWORD Displacement;
			if (SymGetLineFromAddr64(m_hProcess, _Address, &Displacement, &LineInfo))
			{
				const ch8 *pName = LineInfo.FileName;
				const ch8 *pCrtStrip[] = {"f:\\rtm\\vctools\\crt_bld\\self_x86\\crt\\src\\", "f:\\sp\\vctools\\crt_bld\\self_x86\\crt\\src\\", "f:\\dd\\vctools\\crt_bld\\self_x86\\crt\\src\\"
//											,"f:\\dd\\vctools\\crt_bld\\self_x86\\crt\\prebuild\\eh\\"
											};
				mint nCrtStrip = sizeof(pCrtStrip) / sizeof(pCrtStrip[0]);
				
				CStr Temp;
				for (mint i = 0; i < nCrtStrip; ++i)
				{
					aint CrtPos = fg_StrFind(pName, pCrtStrip[i]);
					if (CrtPos == 0)
					{
						Temp = "X:\\Apps\\Dev\\VS.2010\\VC\\Crt\\src\\";
						Temp += (pName + fg_StrLen(pCrtStrip[i]));
						pName = Temp;
						break;
					}
				}

				int Len = fg_StrLen(pName);
				pLocalInfo->m_pSourceFileName = pStr = (ch8 *)CAllocator_NonTrackedHeap::f_Alloc(Len + 1);
				fg_MemCopy(pStr, pName, Len);
				pStr[Len] = 0;
				pLocalInfo->m_SourceLine = LineInfo.LineNumber;
			}
			else
			{
				pLocalInfo->m_pSourceFileName = pStr = (ch8 *)CAllocator_NonTrackedHeap::f_Alloc(1);
				pStr[0] = 0;
			}
		}
		{
			IMAGEHLP_MODULE64 ModuleInfo;
			ModuleInfo.SizeOfStruct = sizeof(ModuleInfo);
			if (SymGetModuleInfo64(m_hProcess, _Address, &ModuleInfo))
			{
				int Len = fg_StrLen(ModuleInfo.ImageName);
				pLocalInfo->m_pModuleName = pStr = (ch8 *)CAllocator_NonTrackedHeap::f_Alloc(Len + 1);
				fg_MemCopy(pStr, ModuleInfo.ImageName, Len);
				pStr[Len] = 0;
			}
			else
			{
				pLocalInfo->m_pModuleName = pStr = (ch8 *)CAllocator_NonTrackedHeap::f_Alloc(1);
				pStr[0] = 0;
			}
		}
		return pLocalInfo;                
	}

	void f_RemoveUnused()
	{			
		while (m_Usused.f_GetFirst())
		{		
			CLocalStackTraceInfo *pInfo = m_Usused.f_Pop();

			m_TraceInfoTree.f_Remove(pInfo);

			if (pInfo->m_pFunctionName)
				CAllocator_NonTrackedHeap::f_Free((ch8 *)pInfo->m_pFunctionName);
			if (pInfo->m_pModuleName)
				CAllocator_NonTrackedHeap::f_Free((ch8 *)pInfo->m_pModuleName);
			if (pInfo->m_pSourceFileName)
				CAllocator_NonTrackedHeap::f_Free((ch8 *)pInfo->m_pSourceFileName);

			TCUniquePointer<CLocalStackTraceInfo, CAllocator_NonTrackedHeap> pInfoDel = fg_Explicit(pInfo);
		}			
	}

	void f_ReleaseStackTraceInfo(CLocalStackTraceInfo *_pInfo)
	{
		DMibLockTyped(NThread::CMutual, m_Lock);

		DMibSafeCheck(m_bInitialized, "If we are here we should be initialized");

		if ((--(_pInfo)->m_RefCount) == 0)
			m_Usused.f_Insert(_pInfo);

		if (m_Timer.f_GetTime() > fp64(10.0))
		{
			f_RemoveUnused();
			m_Timer.f_Start();
		}

	}

};

