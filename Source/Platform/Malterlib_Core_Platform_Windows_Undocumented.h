// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <winternl.h>

typedef void (*PPEBLOCKROUTINE)( PVOID PebLock ); 

typedef struct _PEB_FREE_BLOCK {
	struct _PEB_FREE_BLOCK	*Next;
	ULONG					Size;
} PEB_FREE_BLOCK, *PPEB_FREE_BLOCK;

typedef PVOID * PPVOID;

typedef struct _RTL_BITMAP
{
     ULONG SizeOfBitMap;
     ULONG * Buffer;
} RTL_BITMAP, *PRTL_BITMAP;

typedef struct _CURDIR
{
	UNICODE_STRING DosPath;
	HANDLE Handle;
} CURDIR, *PCURDIR;

#define RTL_MAX_DRIVE_LETTERS 32

typedef struct _RTL_DRIVE_LETTER_CURDIR
{
	USHORT Flags;
	USHORT Length;
	ULONG TimeStamp;
	UNICODE_STRING DosPath;
} RTL_DRIVE_LETTER_CURDIR, *PRTL_DRIVE_LETTER_CURDIR;

typedef struct _Uncodumented_RTL_USER_PROCESS_PARAMETERS
{
	ULONG MaximumLength;
	ULONG Length;
	ULONG Flags;
	ULONG DebugFlags;
	HANDLE ConsoleHandle;
	ULONG ConsoleFlags;
	HANDLE StandardInput;
	HANDLE StandardOutput;
	HANDLE StandardError;
	CURDIR CurrentDirectory;
	UNICODE_STRING DllPath;
	UNICODE_STRING ImagePathName;
	UNICODE_STRING CommandLine;
	PWSTR Environment;
	ULONG StartingX;
	ULONG StartingY;
	ULONG CountX;
	ULONG CountY;
	ULONG CountCharsX;
	ULONG CountCharsY;
	ULONG FillAttribute;
	ULONG WindowFlags;
	ULONG ShowWindowFlags;
	UNICODE_STRING WindowTitle;
	UNICODE_STRING DesktopInfo;
	UNICODE_STRING ShellInfo;
	UNICODE_STRING RuntimeData;
	RTL_DRIVE_LETTER_CURDIR CurrentDirectories[RTL_MAX_DRIVE_LETTERS];
#if (NTDDI_VERSION >= NTDDI_LONGHORN)
	SIZE_T EnvironmentSize;
#endif
#if (NTDDI_VERSION >= NTDDI_WIN7)
	SIZE_T EnvironmentVersion;
#endif
} Uncodumented_RTL_USER_PROCESS_PARAMETERS, *PUncodumented_RTL_USER_PROCESS_PARAMETERS;

typedef struct _UndocumentedPEB
{                                                                 /* win32/win64 */
	BOOLEAN                      InheritedAddressSpace;             /* 000/000 */
	BOOLEAN                      ReadImageFileExecOptions;          /* 001/001 */
	BOOLEAN                      BeingDebugged;                     /* 002/002 */
	BOOLEAN                      SpareBool;                         /* 003/003 */
	HANDLE                       Mutant;                            /* 004/008 */
	HMODULE                      ImageBaseAddress;                  /* 008/010 */
	PPEB_LDR_DATA                LdrData;                           /* 00c/018 */
	Uncodumented_RTL_USER_PROCESS_PARAMETERS *ProcessParameters;                 /* 010/020 */
	PVOID                        SubSystemData;                     /* 014/028 */
	HANDLE                       ProcessHeap;                       /* 018/030 */
	PRTL_CRITICAL_SECTION        FastPebLock;                       /* 01c/038 */
	PPEBLOCKROUTINE				FastPebLockRoutine;                /* 020/040 */
	PPEBLOCKROUTINE				FastPebUnlockRoutine;              /* 024/048 */
	ULONG                        EnvironmentUpdateCount;            /* 028/050 */
	PVOID                        KernelCallbackTable;               /* 02c/058 */
	ULONG                        Reserved[2];                       /* 030/060 */
	PPEB_FREE_BLOCK					FreeList;                          /* 038/068 */
	ULONG                        TlsExpansionCounter;               /* 03c/070 */
	PRTL_BITMAP                  TlsBitmap;                         /* 040/078 */
	ULONG                        TlsBitmapBits[2];                  /* 044/080 */
	PVOID                        ReadOnlySharedMemoryBase;          /* 04c/088 */
	PVOID                        ReadOnlySharedMemoryHeap;          /* 050/090 */
	PVOID                       *ReadOnlyStaticServerData;          /* 054/098 */
	PVOID                        AnsiCodePageData;                  /* 058/0a0 */
	PVOID                        OemCodePageData;                   /* 05c/0a8 */
	PVOID                        UnicodeCaseTableData;              /* 060/0b0 */
	ULONG                        NumberOfProcessors;                /* 064/0b8 */
	ULONG                        NtGlobalFlag;                      /* 068/0bc */
	LARGE_INTEGER                CriticalSectionTimeout;            /* 070/0c0 */
	SIZE_T                       HeapSegmentReserve;                /* 078/0c8 */
	SIZE_T                       HeapSegmentCommit;                 /* 07c/0d0 */
	SIZE_T                       HeapDeCommitTotalFreeThreshold;    /* 080/0d8 */
	SIZE_T                       HeapDeCommitFreeBlockThreshold;    /* 084/0e0 */
	ULONG                        NumberOfHeaps;                     /* 088/0e8 */
	ULONG                        MaximumNumberOfHeaps;              /* 08c/0ec */
	PVOID                       *ProcessHeaps;                      /* 090/0f0 */
	PVOID                        GdiSharedHandleTable;              /* 094/0f8 */
	PVOID                        ProcessStarterHelper;              /* 098/100 */
	PVOID                        GdiDCAttributeList;                /* 09c/108 */
	PRTL_CRITICAL_SECTION        LoaderLock;                        /* 0a0/110 */
	ULONG                        OSMajorVersion;                    /* 0a4/118 */
	ULONG                        OSMinorVersion;                    /* 0a8/11c */
	ULONG                        OSBuildNumber;                     /* 0ac/120 */
	ULONG                        OSPlatformId;                      /* 0b0/124 */
	ULONG                        ImageSubSystem;                    /* 0b4/128 */
	ULONG                        ImageSubSystemMajorVersion;        /* 0b8/12c */
	ULONG                        ImageSubSystemMinorVersion;        /* 0bc/130 */
	ULONG                        ImageProcessAffinityMask;          /* 0c0/134 */
	HANDLE                       GdiHandleBuffer[28];               /* 0c4/138 */
	ULONG                        unknown[6];                        /* 134/218 */
	PVOID                        PostProcessInitRoutine;            /* 14c/230 */
	PRTL_BITMAP                  TlsExpansionBitmap;                /* 150/238 */
	ULONG                        TlsExpansionBitmapBits[32];        /* 154/240 */
	ULONG                        SessionId;                         /* 1d4/2c0 */
	ULARGE_INTEGER               AppCompatFlags;                    /* 1d8/2c8 */
	ULARGE_INTEGER               AppCompatFlagsUser;                /* 1e0/2d0 */
	PVOID                        ShimData;                          /* 1e8/2d8 */
	PVOID                        AppCompatInfo;                     /* 1ec/2e0 */
	UNICODE_STRING               CSDVersion;                        /* 1f0/2e8 */
	PVOID                        ActivationContextData;             /* 1f8/2f8 */
	PVOID                        ProcessAssemblyStorageMap;         /* 1fc/300 */
	PVOID                        SystemDefaultActivationData;       /* 200/308 */
	PVOID                        SystemAssemblyStorageMap;          /* 204/310 */
	SIZE_T                       MinimumStackCommit;                /* 208/318 */
	PVOID                       *FlsCallback;                       /* 20c/320 */
	LIST_ENTRY                   FlsListHead;                       /* 210/328 */
	PRTL_BITMAP                  FlsBitmap;                         /* 218/338 */
	ULONG                        FlsBitmapBits[4];                  /* 21c/340 */
} UndocumentedPEB;

typedef struct _GDI_TEB_BATCH
{
	ULONG  Offset;
	HANDLE HDC;
	ULONG  Buffer[0x136];
} GDI_TEB_BATCH;

typedef struct _RTL_ACTIVATION_CONTEXT_STACK_FRAME
{
	struct _RTL_ACTIVATION_CONTEXT_STACK_FRAME *Previous;
	struct _ACTIVATION_CONTEXT                 *ActivationContext;
	ULONG                                       Flags;
} RTL_ACTIVATION_CONTEXT_STACK_FRAME, *PRTL_ACTIVATION_CONTEXT_STACK_FRAME;


typedef struct _ACTIVATION_CONTEXT_STACK
{
	ULONG                               Flags;
	ULONG                               NextCookieSequenceNumber;
	RTL_ACTIVATION_CONTEXT_STACK_FRAME *ActiveFrame;
	LIST_ENTRY                          FrameListCache;
} ACTIVATION_CONTEXT_STACK, *PACTIVATION_CONTEXT_STACK;

struct CUndocumentedTEB
{                                                                 /* win32/win64 */
	NT_TIB                       Tib;                               /* 000/0000 */
	PVOID                        EnvironmentPointer;                /* 01c/0038 */
	CLIENT_ID                    ClientId;                          /* 020/0040 */
	PVOID                        ActiveRpcHandle;                   /* 028/0050 */
	PVOID                        ThreadLocalStoragePointer;         /* 02c/0058 */
	UndocumentedPEB             *Peb;                               /* 030/0060 */
	ULONG                        LastErrorValue;                    /* 034/0068 */
	ULONG                        CountOfOwnedCriticalSections;      /* 038/006c */
	PVOID                        CsrClientThread;                   /* 03c/0070 */
	PVOID                        Win32ThreadInfo;                   /* 040/0078 */
	ULONG                        Win32ClientInfo[31];               /* 044/0080 used for user32 private data in Wine */
	PVOID                        WOW32Reserved;                     /* 0c0/0100 */
	ULONG                        CurrentLocale;                     /* 0c4/0108 */
	ULONG                        FpSoftwareStatusRegister;          /* 0c8/010c */
	PVOID                        SystemReserved1[54];               /* 0cc/0110 used for kernel32 private data in Wine */
	LONG                         ExceptionCode;                     /* 1a4/02c0 */
	ACTIVATION_CONTEXT_STACK     ActivationContextStack;            /* 1a8/02c8 */
	BYTE                         SpareBytes1[24];                   /* 1bc/02e8 used for ntdll private data in Wine */
	PVOID                        SystemReserved2[10];               /* 1d4/0300 used for ntdll private data in Wine */
	GDI_TEB_BATCH                GdiTebBatch;                       /* 1fc/0350 used for vm86 private data in Wine */
	HANDLE                       gdiRgn;                            /* 6dc/0838 */
	HANDLE                       gdiPen;                            /* 6e0/0840 */
	HANDLE                       gdiBrush;                          /* 6e4/0848 */
	CLIENT_ID                    RealClientId;                      /* 6e8/0850 */
	HANDLE                       GdiCachedProcessHandle;            /* 6f0/0860 */
	ULONG                        GdiClientPID;                      /* 6f4/0868 */
	ULONG                        GdiClientTID;                      /* 6f8/086c */
	PVOID                        GdiThreadLocaleInfo;               /* 6fc/0870 */
	ULONG                        UserReserved[5];                   /* 700/0878 */
	PVOID                        glDispachTable[280];               /* 714/0890 */
	PVOID                        glReserved1[26];                   /* b74/1150 */
	PVOID                        glReserved2;                       /* bdc/1220 */
	PVOID                        glSectionInfo;                     /* be0/1228 */
	PVOID                        glSection;                         /* be4/1230 */
	PVOID                        glTable;                           /* be8/1238 */
	PVOID                        glCurrentRC;                       /* bec/1240 */
	PVOID                        glContext;                         /* bf0/1248 */
	ULONG                        LastStatusValue;                   /* bf4/1250 */
	UNICODE_STRING               StaticUnicodeString;               /* bf8/1258 used by advapi32 */
	WCHAR                        StaticUnicodeBuffer[261];          /* c00/1268 used by advapi32 */
	PVOID                        DeallocationStack;                 /* e0c/1478 */
	PVOID                        TlsSlots[64];                      /* e10/1480 */
	LIST_ENTRY                   TlsLinks;                          /* f10/1680 */
	PVOID                        Vdm;                               /* f18/1690 */
	PVOID                        ReservedForNtRpc;                  /* f1c/1698 */
	PVOID                        DbgSsReserved[2];                  /* f20/16a0 */
	ULONG                        HardErrorDisabled;                 /* f28/16b0 */
	PVOID                        Instrumentation[16];               /* f2c/16b8 */
	PVOID                        WinSockData;                       /* f6c/1738 */
	ULONG                        GdiBatchCount;                     /* f70/1740 */
	ULONG                        Spare2;                            /* f74/1744 */
	PVOID                        Spare3;                            /* f78/1748 */
	PVOID                        Spare4;                            /* f7c/1750 */
	PVOID                        ReservedForOle;                    /* f80/1758 */
	ULONG                        WaitingOnLoaderLock;               /* f84/1760 */
	PVOID                        Reserved5[3];                      /* f88/1768 */
	PVOID                       *TlsExpansionSlots;                 /* f94/1780 */
	ULONG                        ImpersonationLocale;               /* f98/1788 */
	ULONG                        IsImpersonating;                   /* f9c/178c */
	PVOID                        NlsCache;                          /* fa0/1790 */
	PVOID                        ShimData;                          /* fa4/1798 */
	ULONG                        HeapVirtualAffinity;               /* fa8/17a0 */
	PVOID                        CurrentTransactionHandle;          /* fac/17a8 */
	PVOID                        ActiveFrame;                       /* fb0/17b0 */
#if defined(_M_X64)
	PVOID                        unknown[2];                        /*     17b8 */
#endif
	PVOID                       *FlsSlots;                          /* fb4/17c8 */
	PVOID					PreferredLanguages;						/*    /17d0 */
	PVOID					UserPrefLanguages;						/*    /17d8 */
	PVOID					MergedPrefLanguages;					/*    /17e0 */
	ULONG					MuiImpersonation;						/*    /17e8 */
	union
	{
		volatile USHORT		CrossTebFlags;							/*    /17ec */
		USHORT				SpareCrossTebBits: 0x10;				/*    /17ec */
	};
	union
	{
		USHORT				SameTebFlags;							/*    /17ee */
		struct
		{
			USHORT          SafeThunkCall: 1;
			USHORT          InDbgPrint: 1;
			USHORT          HasFiberData: 1;
			USHORT          SkipThreadAttach: 1;
			USHORT          WerInShipAssertCode: 1;
			USHORT          RanProcessInit: 1;
			USHORT          ClonedThread: 1;
			USHORT          SuppressDebugMsg: 1;
			USHORT          DisableUserStackWalk: 1;
			USHORT          RtlExceptionAttached: 1;
			USHORT          InitialThread: 1;
			USHORT          SessionAware: 1;
			USHORT          LoadOwner: 1;
			USHORT          LoaderWorker: 1;
			USHORT          SpareSameTebBits: 2;
		};
	};
	PVOID					TxnScopeEnterCallback;					/*    /17f0 */
	PVOID					TxnScopeExitCallback;					/*    /17f8 */
	PVOID					TxnScopeContext;						/*    /1800 */
	ULONG					LockCount;								/*    /1808 */
	LONG					WowTebOffset;							/*    /180c */
	PVOID					ResourceRetValue;						/*    /1810 */
	PVOID					ReservedForWdf;							/*    /1818 */
	uint64					ReservedForCrt;							/*    /1820 */
	_GUID					EffectiveContainerId;					/*    /1828 */
};

namespace NLocal
{
	typedef enum _THREAD_INFORMATION_CLASS
	{
		ThreadBasicInformation,
		ThreadTimes,
		ThreadPriority,
		ThreadBasePriority,
		ThreadAffinityMask,
		ThreadImpersonationToken,
		ThreadDescriptorTableEntry,
		ThreadEnableAlignmentFaultFixup,
		ThreadEventPair,
		ThreadQuerySetWin32StartAddress,
		ThreadZeroTlsCell,
		ThreadPerformanceCount,
		ThreadAmILastThread,
		ThreadIdealProcessor,
		ThreadPriorityBoost,
		ThreadSetTlsArrayAddress,
		ThreadIsIoPending,
		ThreadHideFromDebugger
	} THREAD_INFORMATION_CLASS, *PTHREAD_INFORMATION_CLASS;
	typedef LONG KPRIORITY;

	typedef struct _THREAD_BASIC_INFORMATION
	{
		NTSTATUS                ExitStatus;
		PVOID                   TebBaseAddress;
		CLIENT_ID               ClientId;
		KAFFINITY               AffinityMask;
		KPRIORITY               Priority;
		KPRIORITY               BasePriority;
	} THREAD_BASIC_INFORMATION, *PTHREAD_BASIC_INFORMATION;

	typedef struct _THREAD_TIMES_INFORMATION
	{
		LARGE_INTEGER           CreationTime;
		LARGE_INTEGER           ExitTime;
		LARGE_INTEGER           KernelTime;
		LARGE_INTEGER           UserTime;
	} THREAD_TIMES_INFORMATION, *PTHREAD_TIMES_INFORMATION;
}

#if _M_X64

#ifndef UNWIND_HISTORY_TABLE_SIZE
	#define UNWIND_HISTORY_TABLE_SIZE 12

	typedef struct _UNWIND_HISTORY_TABLE_ENTRY {
			ULONG64           ImageBase;
			PRUNTIME_FUNCTION FunctionEntry;
	} UNWIND_HISTORY_TABLE_ENTRY,
	*PUNWIND_HISTORY_TABLE_ENTRY;

	#define UNWIND_HISTORY_TABLE_NONE 0
	#define UNWIND_HISTORY_TABLE_GLOBAL 1
	#define UNWIND_HISTORY_TABLE_LOCAL 2
	typedef struct _UNWIND_HISTORY_TABLE {
			ULONG                      Count;
			UCHAR                      Search;
			ULONG64                    LowAddress;
			ULONG64                    HighAddress;
			UNWIND_HISTORY_TABLE_ENTRY
			   Entry[ UNWIND_HISTORY_TABLE_SIZE ];
	} UNWIND_HISTORY_TABLE, *PUNWIND_HISTORY_TABLE;
	extern "C" PRUNTIME_FUNCTION WINAPI RtlLookupFunctionEntry (
		IN ULONG64 ControlPc,
		OUT PULONG64 ImageBase,
		IN OUT PUNWIND_HISTORY_TABLE HistoryTable OPTIONAL
		);
#endif
	typedef
	LONG
	(NTAPI * PC_LANGUAGE_EXCEPTION_HANDLER)(
	   __in    PEXCEPTION_POINTERS    ExceptionPointers,
	   __in    ULONG64                EstablisherFrame
	   );

	enum
	{
#ifndef UNW_FLAG_NHANDLER
		UNW_FLAG_NHANDLER = 0,
#endif
#ifndef UNW_FLAG_EHANDLER
		UNW_FLAG_EHANDLER = 1,
#endif
#ifndef UNW_FLAG_UHANDLER
		UNW_FLAG_UHANDLER = 2,
#endif
#ifndef UNW_FLAG_CHAININFO
		UNW_FLAG_CHAININFO = 4
#endif

	};
	typedef EXCEPTION_DISPOSITION (*PEXCEPTION_ROUTINE) (
		IN struct _EXCEPTION_RECORD *ExceptionRecord,
		IN PVOID EstablisherFrame,
		IN OUT struct _CONTEXT *ContextRecord,
		IN OUT PVOID DispatcherContext
		);

#pragma warning(push)
#pragma warning(disable:4201)
	typedef union _UNWIND_CODE {
		struct {
			BYTE CodeOffset;
			BYTE UnwindOp : 4;
			BYTE OpInfo   : 4;
		};
		USHORT FrameOffset;
	} UNWIND_CODE, *PUNWIND_CODE;
#pragma warning(pop)


	typedef struct _UNWIND_INFO {
		BYTE Version       : 3;
		BYTE Flags         : 5;
		BYTE SizeOfProlog;
		BYTE CountOfCodes;
		BYTE FrameRegister : 4;
		BYTE FrameOffset   : 4;
		UNWIND_CODE UnwindCode[1];
	} UNWIND_INFO, *PUNWIND_INFO;
	// NEW CODE

#endif


#define MS_VC_EXCEPTION 0x406D1388

typedef struct tagTHREADNAME_INFO
{
	DWORD dwType; // Must be 0x1000.
	LPCSTR szName; // Pointer to name (in user addr space).
	DWORD dwThreadID; // Thread ID (-1=caller thread).
	DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;


#define IDLE_PRIORITY_CLASS         0x00000040
#define BELOW_NORMAL_PRIORITY_CLASS 0x00004000
#define NORMAL_PRIORITY_CLASS       0x00000020
#define ABOVE_NORMAL_PRIORITY_CLASS 0x00008000
#define HIGH_PRIORITY_CLASS         0x00000080
#define REALTIME_PRIORITY_CLASS     0x00000100

typedef struct _REPARSE_DATA_BUFFER {
	ULONG  ReparseTag;
	USHORT ReparseDataLength;
	USHORT Reserved;
	union 
	{
		struct 
		{
			USHORT SubstituteNameOffset;
			USHORT SubstituteNameLength;
			USHORT PrintNameOffset;
			USHORT PrintNameLength;
			ULONG  Flags;
			WCHAR  PathBuffer[1];
		} SymbolicLinkReparseBuffer;
		struct 
		{
			USHORT SubstituteNameOffset;
			USHORT SubstituteNameLength;
			USHORT PrintNameOffset;
			USHORT PrintNameLength;
			WCHAR  PathBuffer[1];
		} MountPointReparseBuffer;
		struct 
		{
			UCHAR DataBuffer[1];
		} GenericReparseBuffer;
	};
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

enum Undocumented_PROCESSINFOCLASS
{
	Undocumented_ProcessBasicInformation,
	ProcessQuotaLimits,
	ProcessIoCounters,
	ProcessVmCounters,
	ProcessTimes,
	ProcessBasePriority, // invalid for query
	ProcessRaisePriority, // invalid for query
	Undocumented_ProcessDebugPort,
	ProcessExceptionPort, // invalid for query
	ProcessAccessToken, // invalid for query
	ProcessLdtInformation,
	ProcessLdtSize, // invalid for query
	ProcessDefaultHardErrorMode,
	ProcessIoPortHandlers,          // Note: this is kernel mode only, invalid for query
	ProcessPooledUsageAndLimits,
	ProcessWorkingSetWatch,
	ProcessUserModeIOPL, // invalid class
	ProcessEnableAlignmentFaultFixup, // invalid class
	ProcessPriorityClass,
	ProcessWx86Information,
	ProcessHandleCount, 
	ProcessAffinityMask, // invalid for query
	ProcessPriorityBoost,
	ProcessDeviceMap,
	ProcessSessionInformation,
	ProcessForegroundInformation, // invalid for query
	Undocumented_ProcessWow64Information,
	Undocumented_ProcessImageFileName,
	ProcessLUIDDeviceMapsEnabled,
	Undocumented_ProcessBreakOnTermination,
	ProcessDebugObjectHandle,
	ProcessDebugFlags, // EProcess->Flags.NoDebugInherit
	ProcessHandleTracing, 
	ProcessIoPriority,
	ProcessExecuteFlags,
	ProcessTlsInformation, // invalid class
	ProcessCookie,
	ProcessImageInformation, // last available on XPSP3
	ProcessCycleTime,
	ProcessPagePriority,
	ProcessInstrumentationCallback, // invalid class
	ProcessThreadStackAllocation, // invalid class
	ProcessWorkingSetWatchEx,
	ProcessImageFileNameWin32, // buffer is a UNICODE_STRING
	ProcessImageFileMapping, // buffer is a pointer to a file handle open with SYNCHRONIZE | FILE_EXECUTE access, return value is whether the handle is the same used to start the process
	ProcessAffinityUpdateMode,
	ProcessMemoryAllocationMode,
	ProcessGroupInformation,
	ProcessTokenVirtualizationEnabled, // invalid class
	ProcessConsoleHostProcess, // retrieves the pid for the process' corresponding conhost process
	ProcessWindowInformation, // returns the windowflags and windowtitle members of the process' peb->rtl_user_process_params
	MaxProcessInfoClass // MaxProcessInfoClass should always be the last enum
};

inline_always CUndocumentedTEB *fg_GetTEB()
{
#if defined(_M_X64)
	return (CUndocumentedTEB *)__readgsqword(6*sizeof(void*));
#else
	return (CUndocumentedTEB *)__readfsdword(6*sizeof(void*));
#endif

}

template <typename tf_CType>
inline_always tf_CType fg_GetTebData(mint _iIndex)
{
#if defined(_M_X64)
	return (tf_CType)__readgsqword(_iIndex);
#else
	return (tf_CType)__readfsdword(_iIndex);
#endif
}


inline_always UndocumentedPEB *fg_GetPEB(CUndocumentedTEB *_pTeb)
{
	return _pTeb->Peb;
}

enum Undocumented_FILE_INFO_BY_HANDLE_CLASS
{
    Undocumented_FileBasicInfo,
    Undocumented_FileStandardInfo,
    Undocumented_FileNameInfo,
    Undocumented_FileRenameInfo,
    Undocumented_FileDispositionInfo,
    Undocumented_FileAllocationInfo,
    Undocumented_FileEndOfFileInfo,
    Undocumented_FileStreamInfo,
    Undocumented_FileCompressionInfo,
    Undocumented_FileAttributeTagInfo,
    Undocumented_FileIdBothDirectoryInfo,
    Undocumented_FileIdBothDirectoryRestartInfo,
    Undocumented_FileIoPriorityHintInfo,
    Undocumented_FileRemoteProtocolInfo,
    Undocumented_FileFullDirectoryInfo,
    Undocumented_FileFullDirectoryRestartInfo,
    Undocumented_FileStorageInfo,
    Undocumented_FileAlignmentInfo,
    Undocumented_FileIdInfo,
    Undocumented_FileIdExtdDirectoryInfo,
    Undocumented_FileIdExtdDirectoryRestartInfo,
    Undocumented_FileDispositionInfoEx,
    Undocumented_FileRenameInfoEx,
    Undocumented_MaximumFileInfoByHandleClass
};

struct Undocumented_FILE_ID_INFO 
{
    ULONGLONG VolumeSerialNumber;
    FILE_ID_128 FileId;
};
