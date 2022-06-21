// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/OnScopeExitShared>

namespace NMib
{
	enum EProtect
	{
		 EProtect_Read			= DMibBit(0)
		,EProtect_Write			= DMibBit(1)
		,EProtect_Exec			= DMibBit(2)
		,EProtect_NoCache		= DMibBit(3)
		,EProtect_WriteCombine	= DMibBit(4)
		,EProtect_ReadWrite		= EProtect_Read | EProtect_Write
		,EProtect_ReadExec		= EProtect_Read | EProtect_Exec
		,EProtect_WriteExec		= EProtect_Write | EProtect_Exec
		,EProtect_All			= EProtect_Read | EProtect_Write | EProtect_Exec
	};
	
	enum EDebugCheckFailureAction
	{
		EDebugContractFailureAction_Continue,
		EDebugContractFailureAction_Break,
		EDebugContractFailureAction_Abort,
		EDebugContractFailureAction_NotHandled,
	};

	typedef aint (FThreadProc)(void *_pContext);
	
	namespace NCryptography
	{
		struct CUniversallyUniqueIdentifier;
	}

	namespace NSystem
	{
		struct CSystemCPUUsage
		{
			// 
			fp32 m_User;
			fp32 m_Kernel;
			fp32 m_Idle;
			CSystemCPUUsage()
				: m_User(0.0f)
				, m_Kernel(0.0f)
				, m_Idle(0.0f)
			{
			}
			CSystemCPUUsage(CSystemCPUUsage const &_Other)
				: m_User(_Other.m_User)
				, m_Kernel(_Other.m_Kernel)
				, m_Idle(_Other.m_Idle)
			{
			}
			CSystemCPUUsage(fp32 _Init)
				: m_User(_Init)
				, m_Kernel(_Init)
				, m_Idle(_Init)
			{
			}

			fp32 f_GetUsage() const
			{
				return m_User + m_Kernel;
			}
			
		};

	}
	
	enum EOperatingSystemArch
	{
		EOperatingSystemArch_Unknown = -1,
		EOperatingSystemArch_x86,
		EOperatingSystemArch_x64,
		EOperatingSystemArch_PPC,
		EOperatingSystemArch_PPC64,
		EOperatingSystemArch_le32,
		EOperatingSystemArch_arm64,
	};
	
	namespace NSys
	{		
		void fg_FreeLibrary(void *_pModule);
		void* fg_LoadLibrary(NMib::NStr::CStr const& _Library);
		void* fg_LoadLibrary(NMib::NStr::CStrNonTracked const& _Library);
		void* fg_LoadLibrary(NMib::NStr::CFStr256 const& _Library);
		void* fg_GetLibrarySymbol(void* _pModule, char const* _pSymbol);
		void* fg_GetExeData(char const* _pSegment, char const* _pSection, unsigned long long& _nDataBytes); // Currently only implemented on OSX, a NOP returning nullptr on all others,

		void fg_System_ReportContractViolation(const NMib::NStr::CStrNonTracked &_Message);
		NMib::NStr::CStrNonTracked fg_System_GetContractViolationMessage();
		
		bool fg_System_GetOperatingSystemVersion(int& o_Major, int& o_Minor, int& o_Fix, EOperatingSystemArch &o_Arch);
		
		void fg_System_ExitProcess(aint _ExitCode);

		void fg_ConsoleOutputRaw(const NMib::NStr::CStrNonTracked &_Str);
		void fg_ConsoleOutputBinary(NMib::NContainer::CSecureByteVector const &_Buffer);
		void fg_ConsoleOutput(ch8 const *_pStr, mint _Len);
		void fg_ConsoleOutput(NMib::NStr::CStrNonTracked const &_Str);
		void fg_ConsoleOutput(NMib::NStr::CStrSecure const &_Str);
		void fg_ConsoleOutputFlush();
		void fg_ConsoleErrorOutput(NMib::NStr::CStrSecure const &_Str);
		void fg_ConsoleErrorOutput(NMib::NStr::CStrNonTracked const &_Str);
		void fg_ConsoleErrorOutputFlush();
		bool fg_ConsoleOutputValid();
		bool fg_ConsoleErrorOutputValid();
		bool fg_ConsoleInputValid();
		
		struct CConsoleProperties
		{
			uint32 m_Width = 0;
			uint32 m_Height = 0;
		};
		
		CConsoleProperties fg_GetConsoleProperties();

		mint fg_Mem_GetNumNumaNodes();
		void fg_Mem_GetNumaNodes(ENumaNode *_pNodens, mint _nNodes);

		NMib::COnScopeExitShared fg_System_RegisterForSignal(int _Signal, NFunction::TCFunctionMutable<void ()> &&_fOnSignal);
		NMib::COnScopeExitShared fg_System_RegisterForThreadSignal(int _Signal, NFunction::TCFunctionMutable<void ()> &&_fOnSignal);

		void *fg_InterProcess_MemAlloc(ch8 const *_pName, mint _Size, void * &_pMemory);
		void fg_InterProcess_MemFree(void *_pHandle, void *_pMemory);

		void fg_Thread_Sleep(fp32 _Seconds);
		void *fg_Thread_Create
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
		;
		void fg_Thread_SetPriority(void *_pThread, EExecutionPriority _Priority);
		void fg_Thread_SetAffinity(void *_pThread, mint _Affinity);
		void fg_Thread_SetNumaAffinity(void *_pThread, ENumaNode _NumaNode);
		void fg_Thread_Destroy(void *_pThread);
		void fg_Thread_Suspend(void *_pThread);
		void fg_Thread_Resume(void *_pThread);
		void fg_Thread_SmallestSleep();
		void fg_Thread_EnumOtherThreadsInProcess(NFunction::TCFunctionNoAlloc<void (mint _ThreadID)> const &_fOnThread);

		mint fg_Thread_GetPhysicalCores();
		mint fg_Thread_GetVirtualCores();

		void *fg_Thread_BeginDestroy(void *_pThread);
		void fg_Thread_BlockUntilExit(void *_pThreadDestroyContext);
		void fg_Thread_WillNotBlockUntilExit(void *_pThreadDestroyContext);
		void fg_Thread_EndDestroy(void *_pThreadDestroyContext);

		bool fg_System_BeingDebugged();

		void fg_Debug_DiffStrings(const NMib::NStr::CStr &_FirstStr, const NMib::NStr::CStr &_SecondStr, const NMib::NStr::CStr &_FirstName = "", const NMib::NStr::CStr &_SecondName = "");

		/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
		|	Function:			FDeadlockUserNotify															|
		|																									|
		|	Description:		Should display a message telling the user that the program has probably		|
		|						deadlocked and that the user can decide to create a crash dump.				|
		\*_________________________________________________________________________________________________*/
		typedef 
		bool // Return true if the user choose to create a crash dump.
		(FDeadlockUserNotify)
		(
		);

		
		void fg_Debug_SetDeadlockNotifyFunction(FDeadlockUserNotify *_pCrashDumpUserNotify);
		void fg_Debug_StartDeadlockDetector(fp64 _Timeout);
		void fg_Debug_NotDeadlocked();
		void fg_Debug_StopDeadlockDetector();
		bool fg_Debug_IsDeadlocked();
		void fg_Debug_EnableCrashDumpCaches();
		void fg_Debug_EnableCrashDumps();
		EDebugCheckFailureAction fg_Debug_ReportContractFailure(const ch8 *_pFileName, int32 _Line, void *_pCodePointer, const NMib::NStr::CStrNonTracked &_ErrorMessage);


		CMibCodeAddress fg_System_GetStackTrace(aint _iDepth);
		mint fg_System_GetStackTrace(CMibCodeAddress *_pStack, mint _nMaxDepth);
		void fg_Debug_GenerateCrashDump(const NMib::NStr::CStr &_Message, const NMib::NStr::CStr &_ExtraLog, NContainer::TCVector<NMib::NStr::CStr> &_GeneratedLogs, bool _bDisplayGUI);
		void fg_Debug_GenerateMemoryDump
			(
			 	NMib::NContainer::TCVector<void*, NMib::NMemory::CAllocator_NonTrackedHeap> const& _Locations
			 	, NMib::NContainer::TCVector<mint, NMib::NMemory::CAllocator_NonTrackedHeap> const& _Sizes
			)
		;

		void fg_Debug_BlockingMessage(NMib::NStr::CStr const &_Heading, NMib::NStr::CStr const &_Message);

		void fg_Debug_UndecorateName(const ch8 *_pName, NMib::NStr::CStr &_Destination);
		void fg_Debug_UndecorateName(const ch8 *_pName, NMib::NStr::CStrNonTracked &_Destination);
		void fg_Debug_UndecorateName(const ch8 *_pName, ch8 *_pDestination, mint _MaxLen);

		/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
		|	Function:			FCrashDumpUserNotify														|
		|																									|
		|	Description:		Should display the supplied crash dump information to the user if required.	|
		|																									|
		|	Comments:			_bContinue should be true if the user has the option of continuing, false	|
		|						if not.																		|
		|																									|
		|						Returns:																	|
		|							true if execution should continue 										|
		|							false if execution should end	 										|
		|																									|
		\*_________________________________________________________________________________________________*/
		typedef bool (FCrashDumpUserNotify)(const NMib::NStr::CStr &_CustomMessage,
											const NMib::NStr::CStr &_ProgramName,
											const NMib::NStr::CStr &_SupportEmail,
											const NMib::NStr::CStr &_FileName,
											const NMib::NStr::CStr &FileNameDumpMini,
											const NMib::NStr::CStr &FileNameDump,
											bool _bAllowContinue);

		void fg_Debug_SetCrashDumpUserNotifyFunction(FCrashDumpUserNotify *_pCrashDumpUserNotify);
		void fg_Debug_SetCrashDumpUserNotifyFormats(NMib::NStr::CStrNonTracked const &_CustomMessage, NMib::NStr::CStrNonTracked const &_CanContinueMessage, NMib::NStr::CStrNonTracked const &_NoContinueMessage);

		void fg_Mem_EnableMemoryToucher(bool _bEnabled, fp64 _CPUUsage = 0.0);

		
		void fg_TerminateProcess(aint _ExitCode);

		void fg_Process_AllowInvalidExit(bool _bAllow);

		NMib::NStr::CStr fg_Process_GetCommandLine();
		void fg_Process_GetCommandLineArgs(NContainer::TCVector<NMib::NStr::CStr> &_List);
		
		NMib::NStr::CStr fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStr const &_VariableName);
		NMib::NStr::CStrNonTracked fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStrNonTracked const &_VariableName);
		NMib::NStr::CFStr256 fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CFStr256 const &_VariableName);

		bool fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStr const &_VariableName, NMib::NStr::CStr &_Value);
		bool fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStrNonTracked const &_VariableName, NMib::NStr::CStrNonTracked &_Value);
		void fg_Process_SetEnvironmentVariable_Unsafe(NMib::NStr::CStr const &_VariableName, NMib::NStr::CStr const &_Value);
		void fg_Process_SetEnvironmentVariable_Unsafe(NMib::NStr::CStrNonTracked const &_VariableName, NMib::NStr::CStrNonTracked const &_Value);

		NContainer::TCMap<NMib::NStr::CStr, NMib::NStr::CStr> fg_Process_GetEnvironmentVariables_NonProtected();
		
		NMib::NStr::CStr fg_System_GenerateUUID();
		void fg_System_GenerateUUID(NCryptography::CUniversallyUniqueIdentifier &_UUID);

		NMib::NStr::CStr fg_System_GetCPUName();

		uint16 fg_Langague_GetSystemLanguage(NMib::NStr::CStr &_Language);

		void *fg_Module_Get(mint &_ModuleSize);

		void fg_Message(const ch8 *_pMessageType, const ch8 *_pToOutput);
		void fg_Message(const ch16 *_pMessageType, const ch16 *_pToOutput);

		void *fg_System_CPUUsageMonitor_Open();
		void fg_System_CPUUsageMonitor_Close(void *_pHandle);
		void fg_System_EnableFloatingPointExceptions();
		
		void *fg_Process_GetCrossModuleMemoryManagerInterface();
		void fg_Process_SetCrossModuleMemoryManagerInterface(void *_pInterface);

		NSystem::CSystemCPUUsage fg_System_CPUUsageMonitor_GetUsage(void *_pHandle, bool &_bChanged);


		/*
			Basic interface for storing secure passwords on a per-user, per-application basis.
		*/

		enum ESecurePassword
		{
			ESecurePassword_OK,
			ESecurePassword_Failure,
			ESecurePassword_Duplicate,
			ESecurePassword_NotFound,
			ESecurePassword_TooLarge,
			ESecurePassword_AuthFailed,
		};

		/*
			On OSX Location defaults to Sys ProgramName and is used to name keychain entries.
			On W
		*/
		bool fg_SecurePassword_IsLocked();
		ESecurePassword fg_SecurePassword_SetLocation(NMib::NStr::CStr const& _Location);
		ESecurePassword fg_SecurePassword_Store(NMib::NStr::CStr const& _Key, NMib::NStr::CStrSecure const& _Password);
		ESecurePassword fg_SecurePassword_Remove(NMib::NStr::CStr const& _Key);
		ESecurePassword fg_SecurePassword_Get(NMib::NStr::CStr const& _Key, NMib::NStr::CStrSecure& _oPassword);
		ESecurePassword fg_SecurePassword_Exists(NMib::NStr::CStr const& _Key);
		bool fg_SecurePassword_Supported();
//		ESecurePassword fg_SecurePassword_Enum(NMib::NContainer::TCVector<NMib::NStr::CStr> & _oKeys);
		
		/*
			User management
		*/
		enum EUserManagementCreateUserFlag
		{
			EUserManagementCreateUserFlag_None = 0
			, EUserManagementCreateUserFlag_ShellAccess = DMibBit(0)
			, EUserManagementCreateUserFlag_SupportUILogin = DMibBit(1)
		};

		NMib::NStr::CStr fg_UserManagement_GetProcessRealUser();
		NMib::NStr::CStr fg_UserManagement_GetProcessEffectiveUser();
		
		NMib::NStr::CStr fg_UserManagement_GetProcessRealGroup();
		NMib::NStr::CStr fg_UserManagement_GetProcessEffectiveGroup();

		NMib::NStr::CStr fg_UserManagement_GetProcessRealUserName();
		NMib::NStr::CStr fg_UserManagement_GetProcessEffectiveUserName();
		
		NMib::NStr::CStr fg_UserManagement_GetProcessRealGroupName();
		NMib::NStr::CStr fg_UserManagement_GetProcessEffectiveGroupName();
		
		bool fg_UserManagement_GroupExists(NMib::NStr::CStr const &_GroupName, NMib::NStr::CStr &_ReturnGID);
		void fg_UserManagement_CreateGroup(NMib::NStr::CStr const &_GroupName, NMib::NStr::CStr &_ReturnGID);
		void fg_UserManagement_DeleteGroup(NMib::NStr::CStr const &_GroupName);
		
		bool fg_UserManagement_UserExists(NMib::NStr::CStr const &_UserName, NMib::NStr::CStr &_ReturnUID);
		NMib::NStr::CStr fg_UserManagement_MakeValidUserName(NMib::NStr::CStr const &_UserName);
		NMib::NStr::CStr fg_UserManagement_MakeValidGroupName(NMib::NStr::CStr const &_GroupName);
		void fg_UserManagement_CreateUser
			(
				NMib::NStr::CStr const &_InGroupName
				, NMib::NStr::CStr const &_UserName
				, NMib::NStr::CStrSecure const &_Password
				, NMib::NStr::CStr const &_FullName
				, NMib::NStr::CStr const &_HomeDirectory
				, NMib::NStr::CStr &_ReturnUID
			 	, EUserManagementCreateUserFlag _Flags
			)
		;
		void fg_UserManagement_SetUserPassword
			(
				NMib::NStr::CStr const &_UserName
				, NMib::NStr::CStrSecure const &_Password
			)
		;
		
		void fg_UserManagement_DeleteUser(NMib::NStr::CStr const &_UserName);
		
		void fg_UserManagement_AddUserToGroup(NMib::NStr::CStr const &_GroupName, NMib::NStr::CStr const &_UserName);
		void fg_UserManagement_RemoveUserFromGroup(NMib::NStr::CStr const &_GroupName, NMib::NStr::CStr const &_UserName);
		bool fg_UserManagement_UserIsMemberOfGroup(NMib::NStr::CStr const &_GroupName, NMib::NStr::CStr const &_UserName);
		NMib::NContainer::TCVector<NMib::NStr::CStr> fg_UserManagement_UserGetMemberOfGroups(NMib::NStr::CStr const &_UserName);
		bool fg_UserManagement_IsValidName(NMib::NStr::CStr const &_Name);
		
		/*
		Desktop Environment
		*/
		enum EDesktopEnvironment
		{
				// TODO: Extend these non-linux enums maybe?
				EDesktopEnvironment_Windows
			,	EDesktopEnvironment_OSX

			,	EDesktopEnvironment_Unity
			,	EDesktopEnvironment_KDE3
			,	EDesktopEnvironment_KDE4
			,	EDesktopEnvironment_XCFE
			,	EDesktopEnvironment_GNOME
			,	EDesktopEnvironment_LXDE
			,	EDesktopEnvironment_Linux
			,	EDesktopEnvironment_Emscripten
		};

		EDesktopEnvironment fg_DesktopEnvironment_Get();

	}

}

