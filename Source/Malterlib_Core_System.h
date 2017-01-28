// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Container/LinkedList>

#include "Malterlib_Core_PlatformInterface.h"
#include "../../Log/Source/Malterlib_Log_Configuration.h"
#include "../../Memory/Source/Malterlib_Memory_MemoryManager_Checkout.h"

#include "Malterlib_Core_SubSystem.h"

namespace NMib
{
	namespace NException
	{
		class CExceptionFilter
		{
		public:
			virtual void f_Exception(void *_pExceptionData) = 0;
		};

		extern NAggregate::TCAggregate<NThread::TCThreadLocal<TCAutoClear<CExceptionFilter *>>, 64> g_ExceptionFilter;

		class CExceptionFilterScope
		{
			CExceptionFilter *m_pOldFilter;
		public:

			CExceptionFilterScope(CExceptionFilter &_Filter)
			{
				m_pOldFilter = **g_ExceptionFilter;
				**g_ExceptionFilter = &_Filter;
			}

			~CExceptionFilterScope()
			{
				**g_ExceptionFilter = m_pOldFilter;
			}
		};

#		define DMibExceptionFilter(_Filter) NMib::NException::CExceptionFilterScope ScopeExceptionFilter(_Filter)
	}
}

namespace NMib
{
	namespace NLog
	{
		typedef NStr::CStrNonTracked CLogStr;
	}

#if DMibSysLogSeverities
	namespace NLog
	{
		class CLogger;
		struct CLogFile;
	};
#endif

	namespace NMem
	{
#ifdef DMibDebug
		extern NAggregate::TCAggregate<NThread::TCThreadLocal<NAtomic::TCAtomic<smint>>, 64> g_AllowDebugNewError;
		extern NAtomic::TCAtomic<smint> g_AllowDebugNewErrorGlobal;

		NThread::TCThreadLocal<NAtomic::TCAtomic<smint>> & fg_AccessAllowDebugNewErrorGlobalSingleton();

		class CAllowDebugNewErrorScope
		{
		public:
			CAllowDebugNewErrorScope()
			{
				++(*fg_AccessAllowDebugNewErrorGlobalSingleton());
			}
			~CAllowDebugNewErrorScope()
			{
				--(*fg_AccessAllowDebugNewErrorGlobalSingleton());
			}
		};
#define DMibMemAllowDebugNewError CAllowDebugNewErrorScope AllowDebugNewErrorScope
#	ifndef DMibPNoShortCuts
#		define DAllowDebugNewError DMibMemAllowDebugNewError
#	endif
#else
#define DMibMemAllowDebugNewError 
#	ifndef DMibPNoShortCuts
#		define DAllowDebugNewError DMibMemAllowDebugNewError
#	endif
#endif
	}

	// The global system object... Can be overridden, its not recommended if you don't know what you are doing though
	typedef void (calling_convention_c FConstruct)(void);
	typedef void (calling_convention_c FDestruct)(void);

	class CSystemShared;

	namespace NSys
	{
		NMib::NStr::CStr fg_CommandLineParameters();
	}

	struct CVirtualDestructor
	{
		virtual ~CVirtualDestructor()
		{
		}
	};
	
//	#ifdef DPlatformFamily_OSX
//		typedef NMem::CAllocator_VirtualNoCommit CMainHeapVirtualAllocator;
//	#else
		typedef NMem::CAllocator_Virtual CMainHeapVirtualAllocator;
//	#endif

#ifdef DPlatformFamily_Windows
	using CSystemEnvironment = NContainer::TCMap<NStr::CStr, NStr::CStr, NStr::CCompareNoCase>;
#else
	using CSystemEnvironment = NContainer::TCMap<NStr::CStr, NStr::CStr>;
#endif
			
	class CSystem
	{	
		friend class CRunTimeObjectInfo;
		friend class NThread::CSemaphoreReportableAggregate;

	private:

		NThread::CMutual mp_SubSystemsLock;
		DMibListLinkD_List(CSubSystem, m_Link) mp_SubSystems;

		class CCommandLineData
		{
		public:
			NStr::CStr m_CommandLine;
			NContainer::TCVector<NStr::CStr> m_lCommandLineParameters;
		};

		NMib::NAggregate::TCAggregateSimple<CCommandLineData> m_CommandLineData;

		CVirtualMachineInfo mp_VMInfo;

		void fs_ParseCommandLine();

		NThread::CMutual m_Lock;

		NThread::CMutual m_EventMember_Lock;
		
		NStr::CStr m_ProgramName;
		
		NStr::CStr m_CrashHandlerPath;
		NStr::CStr m_CrashHandlerExePath;
		NStr::CStr m_CrashHandlerServer;
		
		NStr::CStr m_SupportEmail;
		NStr::CStrNonTracked m_ProgramNameNonTracked;
		NStr::CStrNonTracked m_SupportEmailNonTracked;
		bint m_bRunningAsService;

		bint m_bInitDone;
		bool m_bIsDll;
		
#if DMibConfig_Memory_Shims_EnableGlobal
		void fp_CreateGlobalMemoryReporter();
		void fp_DestroyGlobalMemoryReporter();
#endif
		
		void fp_CreateNonTrackedMemoryManager();
		void fp_DestroyNonTrackedMemoryManager();
		void fp_CreateMemoryManager();
		void fp_DestroyMemoryManager();

		void fp_SubSystem_PrepareFork();
		void fp_SubSystem_ForkedChild();
		void fp_SubSystem_ForkedParent();
		
		void fp_SubSystem_DestroyThreadLocal();
		void fp_SubSystem_DestroyThreadSpecific();
		void fp_SubSystem_PreDestroyThreadSpecific();
		void fp_SubSystem_DestroyBeforeMemoryManager();
		void fp_SubSystem_DestroyBeforeNonTrackedMemoryManager();
		void fp_SubSystem_Destroy();
		void fp_SubSystem_ExitModule();
		
	protected:
		NStr::CStr m_ProgramRoot;
		NStr::CStrNonTracked m_ProgramRootNonTracked;

#if DMibSysLogSeverities
		NLog::CLogger* m_pSystemLog;
		NLog::CLogFile* m_pDefaultLogFile;
#if DMibEnableTrace > 0
		mint m_TraceLoggerDestination = 0;
#endif
		mint m_StdErrLoggerDestination = 0;
		mint m_FileLoggerDestination = 0;
#endif

		void fp_InitComplete();	// Called by subclasses to signify they have finished constructing.

	public:	
		
		void f_AddSubSystem(CSubSystem &_SubSystem);

		static bool ms_bDisableMemoryManagerLeakReport;
		
		CVirtualMachineInfo const& f_GetVirtualMachineInfo() const
		{
			return mp_VMInfo;
		}

		static NContainer::TCVector<NStr::CStr> fs_ParseCommandLine(NStr::CStr const &_CommandLine);

		void f_SetDefaultLogFileName(NLog::CLogStr const &_Name);
		void f_SetDefaultLogFileDirectory(NLog::CLogStr const &_Directory);
		void f_RemoveTraceLogger();
		void f_RemoveAllLoggers();
		void f_AddStdErrLogger();
		
		void f_RegisterProgram(const NStr::CStr &_ProgramName, const NStr::CStr &_SupportEmail, bint _bRunningAsService)
		{
			m_ProgramName = _ProgramName;
			m_ProgramNameNonTracked = _ProgramName;
			m_SupportEmail = _SupportEmail;
			m_SupportEmailNonTracked = _SupportEmail;
			m_bRunningAsService = _bRunningAsService;
		}

		bool f_IsDll() const
		{
			return m_bIsDll;
		}

		NStr::CStr const &f_GetSupportEmail() const
		{
			return m_SupportEmail;
		}

		NStr::CStrNonTracked const &f_GetSupportEmailNonTracked() const
		{
			return m_SupportEmailNonTracked;
		}

		NStr::CStr const &f_GetProgramName() const
		{
			return m_ProgramName;
		}
		
		void f_SetCrashHandler(NStr::CStr const& _Path, NStr::CStr const& _Server);
		
		NStr::CStr const &f_GetCrashHandlerPath() const
		{
			return m_CrashHandlerPath;
		}
		
		NStr::CStr const &f_GetCrashHandlerServer() const
		{
			return m_CrashHandlerServer;
		}
		
		NStr::CStr const &f_GetCrashHandlerExePath() const
		{
			return m_CrashHandlerExePath;
		}
		
		NStr::CStrNonTracked const &f_GetProgramNameNonTracked() const
		{
			return m_ProgramNameNonTracked;
		}

		bint f_GetRunningAsService() const
		{
			return m_bRunningAsService;
		}

		void f_SetRunningAsService(bint _bRunningAsService)
		{
			m_bRunningAsService = _bRunningAsService;
		}

		// Thread safety?
		void f_SetProgramRoot(NStr::CStr const& _Root)
		{
			m_ProgramRoot = _Root;
			m_ProgramRootNonTracked = _Root;
		}

		NStr::CStr const &f_GetProgramRoot() const
		{
			return m_ProgramRoot; 
		}
		NStr::CStrNonTracked const &f_GetProgramRootNonTracked() const
		{
			return m_ProgramRootNonTracked; 
		}


#if DMibSysLogSeverities
		NLog::CLogger& f_GetLogger() { return *m_pSystemLog; }
#endif

		CSystem(bool _bIsDll);
		~CSystem();

		void f_PreDestructThreadSpecific();
		void f_DestructThreadSpecific();
		void f_Destruct();

		NThread::CMutual &f_GetLock()
		{
			return m_Lock;
		}

        /************************************************************************************************\
        ||¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯||
        || Local stuff
        ||______________________________________________________________________________________________||
        \************************************************************************************************/

		void f_Exit(aint _ExitCode);
		void f_Init();

		bint f_InitDone()
		{
			return m_bInitDone;
		}

		void f_InitModule(FConstruct **_pCConstructorsStart, FConstruct **_pCConstructorsEnd, FConstruct **_pCppConstructors, FConstruct **_pCppConstructorsEnd);
		void f_InitModule();
		void f_InitModuleThreaded();
		void f_ExitModule();
		void f_DestroyAggregates();

		aint f_RunApplication();

		/************************************************************************************************\
        ||¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯||
        || Memory manager
        ||______________________________________________________________________________________________||
        \************************************************************************************************/

		bool f_MemoryManager_Check(bool _bBreak = true);
		void f_MemoryManager_PrepareFork();
		void f_MemoryManager_ForkedParent();
		void f_MemoryManager_ForkedChild();
		void f_MemoryManager_DestroyThreads();
		void f_MemoryManager_CanStartThreads();

		void f_MemoryManager_GarbageCollect();
		
		void f_MemoryManager_SetNumaNode(ENumaNode _NumaNode);

		void f_MemoryManager_OnThreadCreated(mint _ThreadID, mint _ParentID);
		
		NMem::CMemoryManagerCheckout f_MemoryManager_Checkout();

		void f_MemoryManager_DisableLeakReport(bool _bDisable);

		bool f_MemoryManager_ReportingLeaks();

		/************************************************************************************************\
        ||¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯||
        || Misc
        ||______________________________________________________________________________________________||
        \************************************************************************************************/

		void f_PrepareFork();
		void f_ForkedChild();
		void f_ForkedParent();

        /************************************************************************************************\
        ||¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯||
        || Debug
        ||______________________________________________________________________________________________||
        \************************************************************************************************/

		void f_FatalError(const ch8 *_pMessage);

		/************************************************************************************************\
        ||¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯||
        || Thread Storage
        ||______________________________________________________________________________________________||
        \************************************************************************************************/

		void *f_ThreadLocalAlloc(NThread::CThreadLocalInterface &_Key, mint &_ThreadLocalLocal);
		void f_ThreadLocalFree(NThread::CThreadLocalInterface &_Key, void *_pStorageIndex);
		void f_ThreadLocalFreeThread();
		void f_ThreadLocalCreateThread(mint _ThreadID, mint _ParentThreadID);
		void *f_ThreadLocalGet(void *_pStorageIndex);
		void f_ThreadLocalReinitForThread(void *_pStorageIndex);
		void f_ThreadLocalSet(void *_pStorageIndex, void *_pValue);
		void f_OnThreadCreated(mint _ThreadID, mint _ParentID);
		void f_OnThreadDestroyed();

		void f_ThreadEnum(NFunction::TCFunction<void (mint _ThreadID)> const &_EnumFunc);
		bool f_ThreadDestroyed() const;
		bool f_ThreadCreated() const;

		void fp_ThreadLocalCreate();
		void fp_ThreadLocalDestroy();

		///
		/// Command line
		/// ============
		
		NStr::CStr f_CommandLineParameters();
		aint f_NumCommandLineParameters();
		NStr::CStr f_CommandLineParameter(aint _iIndex);
		
		NContainer::TCVector<NMib::NStr::CStr> f_GetCommandLineArgs() const;

		///
		/// Environment
		/// ============
		
		CSystemEnvironment f_Environment() const;
		void f_SetEnvironmentVariable(NStr::CStr const &_Name, NStr::CStr const &_Value);
		NStr::CStr f_GetEnvironmentVariable(NStr::CStr const &_Name, NStr::CStr const &_Default = {}, bool *o_pExists = nullptr) const;

		CSystemEnvironment f_ProtectedEnvironment() const; // Whole environment, with and without protected variables
		void f_ProtectEnvironmentVariable(NStr::CStr const &_Variable);
		NStr::CStr f_GetProtectedEnvironmentVariable(NStr::CStr const &_Name, NStr::CStr const &_Default = {}, bool *o_pExists = nullptr) const;
		
		static uint32 ms_PlatformVersion;
	};

	enum EExecutionPriority
	{
		 EExecutionPriority_Lowest		= 0
		,EExecutionPriority_Low			= 0x2AAA
		,EExecutionPriority_BelowNormal	= 0x5555
		,EExecutionPriority_Normal		= 0x8000
		,EExecutionPriority_AboveNormal	= 0xAAAA
		,EExecutionPriority_High		= 0xD555
		,EExecutionPriority_Highest		= 0xFFFF
		,EExecutionPriority_Default		= -1
	};

#ifdef DMibPAutomaticSystemCreation
	extern mint g_SystemMemory[];
#else
	extern CSystem *g_pSys;
#endif
	extern mint g_bCreatingSystemDone;
	extern mint g_bCanUseSystemMalloc;
	extern mint g_bCanStartThreads;
	extern mint g_bMemoryManagerNeededAfterDestroy;
	extern mint g_bCreatedSystem;
							
	static inline_small CSystem *fg_GetSys()
	{
#		ifdef DMibPAutomaticSystemCreation
			DMibFastCheck(g_bCreatingSystemDone);
			return (CSystem *)g_SystemMemory;
#		else

			if (!g_pSys)
				NSys::fg_CreateSystem();

			return g_pSys;
#		endif
	}
	
	class CSystemModule
	{	
	public:

		CSystem *m_pSystem;

		DMibListLinkDA_List(NAggregate::CAggregate, m_Link) m_Aggregates;

		NThread::CMutualAggregate m_Lock;

#ifndef DMibNoAggregateConstexpr
		constexpr CSystemModule(EAggregateInitialization _Init);
#endif
		
		void f_Destroy();
		void f_DestroyAggregates(bint _bDestroySystem);

		void f_Init(CSystem *_pSystem);

		void f_AddAggregate(NAggregate::CAggregate *_pAggregate);
		void f_RemoveAggregate(NAggregate::CAggregate *_pAggregate);
	};
	
	extern CSystemModule g_SystemModule;

	static inline_small CSystemModule *fg_GetModule()
	{
		return &g_SystemModule;
	}

	class CAppClasses
	{
	public:
		const ch8 *m_pAppClass;
	};
	
	// These classes the app has to overide to tell the runtime which classes the app should be. m_pSysImp does not have to be definde
#	ifndef DMibModule
		extern CAppClasses g_AppClasses;
#	endif

#	define DMibAppNoClass NMib::CAppClasses NMib::g_AppClasses = {nullptr};

#	ifndef DMibPNoShortCuts
#		define DAppNoClass DMibAppNoClass
#	endif
	
};

#include <Mib/Time/Timer>

#include "Malterlib_Core_SubSystem.hpp"

