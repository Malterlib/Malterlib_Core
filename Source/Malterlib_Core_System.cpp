// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Debug/RemoteDebugger>
#include <Mib/Log/Log>
#include <Mib/Log/Destinations>

void fg_MalterlibMallocOverride_CanStartThreads();
void fg_MalterlibMallocOverride_DestroyThreads();
void fg_MalterlibMallocOverride_PreDestroyNonTrackedMemoryManager();

namespace NMib
{

	namespace NMemory
	{
		void fg_Mem_InitSubsystem();
	}

	mint g_bMemoryManagerNeededAfterDestroy = false;
	
	namespace NSys
	{
		
		bool fg_Compiler_MakeActive(int _Dummy, ...)
		{
			CMibArgList Args;
			DMibPArgListStart(Args, _Dummy);
			bool bRet = fg_Compiler_MakeActive(DMibPArgListNextArg(Args, void *));
			DMibPArgListEnd(Args);

			return bRet;
		}

		bool fg_HW_MeetsMinimumRequirements()
		{
			NMib::CProcessorInfo CPUInfo;
			fg_HW_GetProcessorInfo(CPUInfo);

			if (	CPUInfo.m_Architecture == EProcessorArchitecture_x86
				||	CPUInfo.m_Architecture == EProcessorArchitecture_x86_64)
			{
				NMib::EProcessorFeature RequiredFeatures = 
							NMib::EProcessorFeature_MMX
						|	NMib::EProcessorFeature_SSE
						|	NMib::EProcessorFeature_SSE2;

				return (CPUInfo.m_Features & RequiredFeatures) == RequiredFeatures;
			}
			else if (	CPUInfo.m_Architecture == EProcessorArchitecture_ARMv5
					||	CPUInfo.m_Architecture == EProcessorArchitecture_ARMv6
					||	CPUInfo.m_Architecture == EProcessorArchitecture_ARMv7
					||	CPUInfo.m_Architecture == EProcessorArchitecture_ARMv8
				)
			{
				NMib::EProcessorFeature RequiredFeatures = 
							NMib::EProcessorFeature_VFP;

				return (CPUInfo.m_Features & RequiredFeatures) == RequiredFeatures;
			}

			return false;
		}

	}

	constinit NStorage::TCAggregate<NThread::TCThreadLocal<CSystemThreadLocal>, 64> g_SystemThreadLocal = {DAggregateInit};

	/************************************************************************************************\
	||¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯||
	|| CSystem
	||______________________________________________________________________________________________||
	\************************************************************************************************/

	// Main System object. Created at start of 
	CSystem *g_pSys = nullptr;

	uint32 CSystem::ms_PlatformVersion = 0;

	CSystem::CSystem(bool _bIsDll)
		: m_bIsDll(_bIsDll)
#ifdef DCompiler_clang
		, m_CommandLineData{DAggregateInit}
#endif

	{
		// Things set in motion might need the g_pSys pointer
		m_bInitDone = false;
		g_pSys = this;
		m_bRunningAsDaemon = false;

#if DMibSysLogSeverities
		m_pSystemLog = nullptr;
		m_pDefaultLogFile = nullptr;
#endif

		// Construct things

		fp_ThreadLocalCreate();
		
		fp_CreateNonTrackedMemoryManager();
#if DMibConfig_Memory_Shims_EnableGlobal
		fp_CreateGlobalMemoryReporter();
#endif

		m_CommandLineData.f_Construct();
#if DMibRemoteDebugger_Enabled
		if (NSys::fg_Process_GetEnvironmentVariable_NonProtected(NStr::CStrNonTracked("Malterlib_RemoteDebugger")) != "")
		{
			NDebug::NRemoteDebugger::fg_RD_InitializeClient();
		}
#endif
		fp_CreateMemoryManager();

		NMemory::fg_Mem_InitSubsystem();
	}
	
	CSystem::~CSystem()
	{
		m_ProgramNameNonTracked.f_Clear();
		m_SupportEmailNonTracked.f_Clear();
		m_ProgramRootNonTracked.f_Clear();

#if DMibRemoteDebugger_Enabled
		NDebug::NRemoteDebugger::fg_RD_DeinitializeClient();
#endif
		
		fp_SubSystem_DestroyThreadLocal();

#if DMibConfig_Memory_Shims_EnableGlobal
		fp_DestroyGlobalMemoryReporter();
#endif
		fp_SubSystem_DestroyBeforeNonTrackedMemoryManager();
		fg_MalterlibMallocOverride_PreDestroyNonTrackedMemoryManager();
		fp_DestroyNonTrackedMemoryManager();

		fp_SubSystem_DestroyBeforeThreadLocals();

		fp_ThreadLocalDestroy();
		
		fp_SubSystem_Destroy();
	}

	void CSystem::fp_InitComplete()
	{
		m_bInitDone = true;	
	}

	void CSystem::f_PreDestructThreadSpecific()
	{
		fp_SubSystem_PreDestroyThreadSpecific();
	}

	void CSystem::f_DestructThreadSpecific()
	{
		g_bCanStartThreads = false;
		
#if DMibSysLogSeverities
		m_pSystemLog->f_SetDispatcher(nullptr);
#endif
		
		fp_SubSystem_DestroyThreadSpecific();

		fg_MalterlibMallocOverride_DestroyThreads();
		f_MemoryManager_DestroyThreads();
	}


	void CSystem::f_Destruct()
	{
#if DMibSysLogSeverities
		delete m_pSystemLog;
		m_pSystemLog = nullptr;

		delete m_pDefaultLogFile;
		m_pDefaultLogFile = nullptr;
#endif

		m_ProgramName.f_Clear();
		m_CrashHandlerPath.f_Clear();
		m_CrashHandlerExePath.f_Clear();
		m_CrashHandlerServer.f_Clear();
		m_SupportEmail.f_Clear();
		m_ProgramRoot.f_Clear();
		m_CommandLineData.f_Destruct();
		fp_SubSystem_DestroyBeforeMemoryManager();
		fp_DestroyMemoryManager();
	}

	void CSystem::f_Init()
	{
	}

	void CSystem::f_RemoveAllLoggers()
	{
#if DMibSysLogSeverities
		while (m_pSystemLog->f_PopGlobalDestination())
			;
#if DMibEnableTrace > 0
		m_TraceLoggerDestination = 0;
#endif
		m_StdErrLoggerDestination = 0;
		m_FileLoggerDestination = 0;
#endif
	}
	
	void CSystem::f_RemoveTraceLogger()
	{
#if DMibEnableTrace > 0
#if DMibSysLogSeverities
		if (m_TraceLoggerDestination)
		{			
			m_pSystemLog->f_RemoveGlobalDestination(m_TraceLoggerDestination);
			m_TraceLoggerDestination = 0;
		}
#endif
#endif
	}
	
	void CSystem::f_AddStdErrLogger()
	{
#if DMibSysLogSeverities
		if (!m_StdErrLoggerDestination)
			m_StdErrLoggerDestination = m_pSystemLog->f_PushGlobalDestination(&NLog::fg_LogTo_StdErr);
#endif
	}

	void CSystem::f_InitModule()
	{
		g_SystemModule.f_Init(this);
		fs_ParseCommandLine();
		// Set the default program root.
		m_ProgramRoot = NSys::NFile::fg_GetProgramDirectory();
		m_ProgramRootNonTracked = m_ProgramRoot;

		NSys::fg_HW_GetVirtualMachineInfo(mp_VMInfo);

#if DMibSysLogSeverities
		m_pSystemLog = DMibNew NLog::CLogger();

		if (!m_pSystemLog->f_ReadConfig("Malterlib_Log_Config.txt"))
		{
			bool bDebugOut = false;

#if !defined(DConfig_Release)
			bDebugOut = true;
#endif

			bool bDisableSystemLog = NSys::fg_Process_GetEnvironmentVariable_NonProtected(NStr::CStrNonTracked("MalterlibDisableSystemLog")) == "true";
			if (bDisableSystemLog)
				bDebugOut = false;

#if DMibEnableTrace > 0
			if (bDebugOut)
				m_TraceLoggerDestination = m_pSystemLog->f_PushGlobalDestination(&NLog::fg_LogTo_DebugOut);
#endif

#if DMibSysLogStdErr
			m_StdErrLoggerDestination = m_pSystemLog->f_PushGlobalDestination(&NLog::fg_LogTo_StdErr);
#endif

			if (!bDisableSystemLog)
				f_AddFileLogger();
		}
#endif
	}

	void CSystem::f_AddFileLogger()
	{
#if DMibSysLogSeverities
		if (!m_FileLoggerDestination)
		{
			NLog::CLogStr ProgramName = fg_GetSys()->f_GetProgramNameNonTracked();
			if (ProgramName.f_IsEmpty())
			{
				ProgramName = NFile::CFile::fs_GetFileNoExt(NFile::CFile::fs_GetProgramPathNonTracked());

				{ // Condition the program name
					mint UnderPos = ProgramName.f_FindReverse("_");
					if (UnderPos != -1)
					{
						ProgramName = ProgramName.f_Extract(0, UnderPos);
					}

					mint Len = ProgramName.f_GetLen();

					mint DIPos = ProgramName.f_FindNoCase("DI");
					if (DIPos == (Len - 2))
					{
						ProgramName = ProgramName.f_Extract(0, Len - 2);
					}
					else
					{
						mint DPos = ProgramName.f_FindNoCase("D");
						if (DPos == Len -1)
						{
							ProgramName = ProgramName.f_Extract(0, Len - 1);
						}
					}
				}
				if (ProgramName.f_IsEmpty())
					ProgramName = "Malterlib";
			}

			m_pDefaultLogFile = DMibNew NLog::CLogFile();
			//m_pDefaultLogFile->m_Filename = ProgramName + "_%TIME%.txt";
			m_pDefaultLogFile->m_Filename = ProgramName + ".log";
			m_FileLoggerDestination = m_pSystemLog->f_PushGlobalDestination(NLog::CFileLogger(m_pDefaultLogFile));
		}
#endif
	}
	
	void CSystem::f_SetDefaultLogFileName(NLog::CLogStr const &_Name)
	{
#if DMibSysLogSeverities
		m_pDefaultLogFile->m_Filename = _Name;
#endif
	}
	
	void CSystem::f_SetDefaultLogFileDirectory(NLog::CLogStr const &_Directory)
	{
#if DMibSysLogSeverities
		m_pDefaultLogFile->m_Directory = _Directory;
#endif
	}

	void CSystem::f_InitModuleThreaded()
	{
		g_bCanStartThreads = true;		
#if DMibRemoteDebugger_Enabled
		NDebug::NRemoteDebugger::fg_RD_NetworkAvailableForClient();
#endif
		f_MemoryManager_CanStartThreads();
		fg_MalterlibMallocOverride_CanStartThreads();
	}
		
	void CSystem::f_InitModule(FConstruct **_pCConstructorsStart, FConstruct **_pCConstructorsEnd, FConstruct **_pCppConstructors, FConstruct **_pCppConstructorsEnd)
	{
		// Call c constructors
		while (_pCConstructorsStart < _pCConstructorsEnd)
		{
			if ( *_pCConstructorsStart != nullptr )
				(**_pCConstructorsStart)();
			++_pCConstructorsStart;
		}
		
		// Call C++ constructors
		while (_pCppConstructors < _pCppConstructorsEnd)
		{
			if ( *_pCppConstructors != nullptr )
				(**_pCppConstructors)();
			++_pCppConstructors;
		}
	}


	void CSystem::f_ExitModule()
	{
		// Last thing
		fp_SubSystem_ExitModule();

		g_SystemModule.f_Destroy();
	}
	
	void CSystem::f_PrepareFork()
	{
#if DMibSysLogSeverities
		if (m_pSystemLog)
			m_pSystemLog->f_PrepareFork();

		if (m_pDefaultLogFile)
			m_pDefaultLogFile->f_PrepareFork();
#endif
		f_ThreadLocal_PrepareFork();
		fp_SubSystem_PrepareFork_BeforeMemoryManager();
		f_MemoryManager_PrepareFork();
		fp_SubSystem_PrepareFork_AfterMemoryManager();
		m_EventMember_Lock.f_Lock();
		m_EventMember_Lock.f_PrepareFork();
	}
	void CSystem::f_ForkedChild()
	{
		m_EventMember_Lock.f_ForkedChild();
		m_EventMember_Lock.f_Unlock();
		fp_SubSystem_ForkedChild_AfterMemoryManager();
		f_MemoryManager_ForkedChild();
		fp_SubSystem_ForkedChild_BeforeMemoryManager();
		f_ThreadLocal_ForkedChild();
#if DMibSysLogSeverities
		if (m_pSystemLog)
			m_pSystemLog->f_ForkedChild();

		if (m_pDefaultLogFile)
			m_pDefaultLogFile->f_ForkedChild();
#endif
	}
	void CSystem::f_ForkedParent()
	{
		m_EventMember_Lock.f_ForkedParent();
		m_EventMember_Lock.f_Unlock();
		fp_SubSystem_ForkedParent_AfterMemoryManager();
		f_MemoryManager_ForkedParent();
		fp_SubSystem_ForkedParent_BeforeMemoryManager();
		f_ThreadLocal_ForkedParent();
#if DMibSysLogSeverities
		if (m_pSystemLog)
			m_pSystemLog->f_ForkedParent();

		if (m_pDefaultLogFile)
			m_pDefaultLogFile->f_ForkedParent();
#endif
	}
	
	void CSystem::f_DestroyAggregates()
	{
		g_SystemModule.f_DestroyAggregates(false);
	}
	void CSystem::f_Exit(aint _ExitCode)
	{
		f_ExitModule();

		NSys::fg_System_ExitProcess(_ExitCode);
	}
	

	void CSystem::f_FatalError(const ch8 *_pMessage)
	{
		NSys::fg_Message("Fatal Error", _pMessage);
		f_Exit(255);
		//NSys::fg_System_ExitProcess(255);
	}

	static aint fsg_ParseCommandLine(NStr::CStr const &_Source, NStr::CStr *_pDest)
	{
		int iCurrent = 0;

		const ch8 *pCommandLine = _Source.f_GetStr();

		int Mode = 0;

		NStr::CStr Current;

		while (*pCommandLine)
		{
			ch8 Temp = *pCommandLine;

			if (NStr::fg_CharIsWhiteSpace(Temp))
			{
				if (Mode == 0)
				{
					if (_pDest)
						_pDest[iCurrent] = Current;

					Current = "";

					++iCurrent;
					while (NStr::fg_CharIsWhiteSpace(*pCommandLine))
						++pCommandLine;
					continue;
				}
				else
				{
					Current.f_AddChar(Temp);
				}
			}
			else
			{
				switch (Temp)
				{
				case '\'':
					if (Mode == 0)
						Mode = 2;
					else if (Mode == 2)
						Mode = 0;
					else if (Mode == 1)
						Current.f_AddChar(Temp);
					break;
				case '"':
					if (Mode == 0)
						Mode = 1;
					else if (Mode == 1)
						Mode = 0;
					else if (Mode == 2)
						Current.f_AddChar(Temp);
					break;
				case '\\':
					{
						if (Mode == 1)
						{
							ch8 Next = *(pCommandLine + 1);
							if (Next == '\\')
							{
								++pCommandLine;
								Current.f_AddChar(Next);
							}
							else if (Next == '"')
							{
								++pCommandLine;
								Current.f_AddChar(Next);
							}
							else if (Next == 'n')
							{
								++pCommandLine;
								Current.f_AddChar('\n');
							}
							else
							{
								Current.f_AddChar(Temp);
							}
							break;
						}
						else if (Mode == 2)
						{
							ch8 Next = *(pCommandLine + 1);
							if (Next == '\'')
							{
								++pCommandLine;
								Current.f_AddChar(Next);
							}
							else
								Current.f_AddChar(Temp);
							break;
						}
					}
				default:
					Current.f_AddChar(Temp);
					break;
				}
			}

			++pCommandLine;
		}


		if (Current.f_GetLen())
		{
			if (_pDest)
				_pDest[iCurrent] = Current;

			++iCurrent;
		}
		return iCurrent;
	}

	void CSystem::fs_ParseCommandLine()
	{
		m_CommandLineData->m_CommandLine = NSys::fg_CommandLineParameters();

		int nParams = fsg_ParseCommandLine(m_CommandLineData->m_CommandLine, nullptr);
		if (nParams)
		{
			m_CommandLineData->m_lCommandLineParameters.f_SetLen(nParams);
			fsg_ParseCommandLine(m_CommandLineData->m_CommandLine, m_CommandLineData->m_lCommandLineParameters.f_GetArray());
		}
	}
	
	void CSystem::f_SetCrashHandler(NStr::CStr const& _Path, NStr::CStr const& _Server)
	{
		m_CrashHandlerPath = _Path;
		m_CrashHandlerServer = _Server;
		m_CrashHandlerExePath = NFile::CFile::fs_GetProgramPath();
	}

	NContainer::TCVector<NStr::CStr> CSystem::fs_ParseCommandLine(NStr::CStr const &_CommandLine)
	{
		NContainer::TCVector<NStr::CStr> Ret;
		int nParams = fsg_ParseCommandLine(_CommandLine, nullptr);
		if (nParams)
		{
			Ret.f_SetLen(nParams);
			fsg_ParseCommandLine(_CommandLine, Ret.f_GetArray());
		}
		return Ret;
	}

	NStr::CStr CSystem::f_CommandLineParameters()
	{
		return m_CommandLineData->m_CommandLine;
	}
	
	NContainer::TCVector<NMib::NStr::CStr> CSystem::f_GetCommandLineArgs() const
	{
		NContainer::TCVector<NMib::NStr::CStr> CommandLineArgs;
		NSys::fg_Process_GetCommandLineArgs(CommandLineArgs);
		return CommandLineArgs;
	}


	aint CSystem::f_NumCommandLineParameters()
	{
		return m_CommandLineData->m_lCommandLineParameters.f_GetLen();
	}

	NStr::CStr CSystem::f_CommandLineParameter(aint _iIndex)
	{
		return m_CommandLineData->m_lCommandLineParameters[_iIndex];
	}

	/************************************************************************************************\
	||¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯||
	|| CSystemModule
	||______________________________________________________________________________________________||
	\************************************************************************************************/
	
#ifndef DMibNoAggregateConstexpr
	constexpr CSystemModule::CSystemModule(EAggregateInitialization _Init)
		: m_pSystem{nullptr}
		, m_Aggregates{_Init}
		, m_Lock{_Init}
	{
	}
#endif
	
	// Make sure that our system module is aggregate
	constinit CSystemModule g_SystemModule = {DAggregateInit};

	void CSystemModule::f_DestroyAggregates(bool _bDestroySystem)
	{
		fg_GetSys()->fp_SubSystem_DestroyAggregates(_bDestroySystem);
		// Destroy aggregates in reverse order of construction
		class CSort
		{
		public:
			typedef aint CRet;
			static inline_small CRet fs_Compare(void *_pContext, NStorage::CAggregate *_pFirst, NStorage::CAggregate *_pSecond)
			{
				if (_pFirst->m_Priority < _pSecond->m_Priority)
					return -1;
				else if (_pFirst->m_Priority > _pSecond->m_Priority)
					return 1;
				return 0;
			}
		};
		m_Aggregates.f_MergeSort<CSort>();
		NStorage::CAggregate * pAggregate = m_Aggregates.f_GetLast();
		while (pAggregate)
		{
			if (!_bDestroySystem && pAggregate->m_Priority < 128)
				break;
			pAggregate->m_fDestruct(pAggregate);
			pAggregate = m_Aggregates.f_GetLast();
		}
	}

	void CSystemModule::f_Destroy()
	{
		f_DestroyAggregates(true);
		
		m_Lock.f_Destruct();
		m_Aggregates.f_Destruct();

	}

	void CSystemModule::f_Init(CSystem *_pSystem)
	{
		m_Aggregates.f_Construct();
		m_Lock.f_Construct();
		m_pSystem = _pSystem;
	}

	void CSystemModule::f_AddAggregate(NStorage::CAggregate *_pAggregate)
	{
		DMibLock(m_Lock);
		DMibFastCheck(m_pSystem); // System must exist
		m_Aggregates.f_Insert(_pAggregate);
	}

	void CSystemModule::f_RemoveAggregate(NStorage::CAggregate *_pAggregate)
	{
		DMibLock(m_Lock);
		DMibFastCheck(m_pSystem); // System must exist
		m_Aggregates.f_Remove(_pAggregate);
	}

	CCoroutineThreadLocalHandler::CCoroutineThreadLocalHandler()
	{
		if (!g_bCanStartThreads)
			return;

		auto &ThreadLocal = **g_SystemThreadLocal;
		if (ThreadLocal.m_pCurrentCoroutineHandler)
			ThreadLocal.m_pCurrentCoroutineHandler->m_ThreadLocalHandlers.f_Insert(this);
	}

	CCoroutineThreadLocalHandler::~CCoroutineThreadLocalHandler() = default;

	CCoroutineHandler::~CCoroutineHandler()
	{
		DMibFastCheck(m_nThreadLocalScopes == 0); // Outstanding scope
	}

#if DMibEnableSafeCheck > 0
	CDebugThreadLocalScope::CDebugThreadLocalScope()
	{
		auto &ThreadLocal = **g_SystemThreadLocal;
		if (ThreadLocal.m_pCurrentCoroutineHandler)
			++ThreadLocal.m_pCurrentCoroutineHandler->m_nThreadLocalScopes;
		m_pCoroutineHandler = ThreadLocal.m_pCurrentCoroutineHandler;
	}

	CDebugThreadLocalScope::~CDebugThreadLocalScope()
	{
		auto &ThreadLocal = **g_SystemThreadLocal;

		DMibFastCheck(m_pCoroutineHandler == ThreadLocal.m_pCurrentCoroutineHandler);

		if (ThreadLocal.m_pCurrentCoroutineHandler)
		{
			DMibFastCheck(ThreadLocal.m_pCurrentCoroutineHandler->m_nThreadLocalScopes > 0);
			--ThreadLocal.m_pCurrentCoroutineHandler->m_nThreadLocalScopes;
		}
	}
#endif
}
