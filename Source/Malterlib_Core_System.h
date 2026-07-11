// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>
#include <Mib/Container/LinkedList>
#include <Mib/Core/CoroutineHandler>
#include <Mib/Core/CoroutineFlags>

#include "Malterlib_Core_PlatformInterface.h"
#include "../../Log/Source/Malterlib_Log_Configuration.h"
#include "../../Memory/Source/Malterlib_Memory_MemoryManager_Checkout.h"

#include "Malterlib_Core_SubSystem.h"

namespace NMib
{
	namespace NException
	{
		class CExceptionFilter;
	}

	struct CPromiseKeepAlive
	{
		CPromiseKeepAlive(umint _Allocsize)
			: m_VirtualAllocSize(_Allocsize)
		{
		}

		virtual ~CPromiseKeepAlive() = 0;

		umint m_VirtualAllocSize = 0;
	};

	namespace NConcurrency
	{
		struct CConcurrencyThreadLocal;
	}

	struct CPromiseThreadLocal
	{
#if DMibEnableSafeCheck > 0
		~CPromiseThreadLocal();
#endif

		NConcurrency::CConcurrencyThreadLocal &f_ConcurrencyThreadLocal();
		void f_PushAllocation(void *_pAllocation);
		void *f_PopAllocation();

		void *m_pOnResultSet = nullptr;
		void *m_pUsePromise = nullptr;
		void *m_pCurrentActorCalled = nullptr;
		void *m_pCoroutinePromiseAllocation = nullptr;
		NContainer::TCGrowingVector<void *> m_PreviousCoroutinePromiseAllocations;
		CPromiseKeepAlive *m_pKeepAlive = nullptr;
		NConcurrency::CConcurrencyThreadLocal *m_pConcurrencyThreadLocal = nullptr;
		bool m_bSupendOnInitialSuspend = false;

#if DMibEnableSafeCheck > 0
		void const *m_pOnResultSetConsumedBy = nullptr;
		void const *m_pExpectCoroutineCallConsumedBy = nullptr;
		void const *m_pUsePromiseConsumedBy = nullptr;
		uint32 m_OnResultSetTypeHash = 0;
		uint32 m_UsePromiseTypeHash = 0;
		bool m_bCaptureDebugException = false;
		bool m_bExpectCoroutineCall = false;
		bool m_bSafeCall = false;
#endif

	private:
		void fp_PushAllocationSlowPath(void *_pAllocation);
		void *fp_PopAllocationSlowPath();
	};

	struct CSystemThreadLocal
	{
		NException::CExceptionFilter *m_pExceptionFilter = nullptr;
		CCoroutineHandler *m_pCurrentCoroutineHandler = nullptr;
#if DMibConfig_Tests_Enable
		NConcurrency::ECoroutineFlag m_ExtraCoroutineFlags = NConcurrency::ECoroutineFlag_None;
#endif
		CPromiseThreadLocal m_PromiseThreadLocal;
		DMibListLinkDS_List(CCrossActorCallStateScope, m_Link) m_CrossActorStateScopes;

#if DMibEnableSafeCheck > 0
		bool m_bDispatchWithReturnIsIndirection = false;
#endif
	};

	bool fg_SystemThreadLocalWasCreated();
	mark_nodebug inline_always_lto CSystemThreadLocal &fg_SystemThreadLocal();
	void fg_SystemThreadInit();
	void fg_MaybeSystemThreadInit();

	namespace NException
	{
		class CExceptionFilter
		{
		public:
			virtual void f_Exception(void *_pExceptionData) = 0;
		};

		class CExceptionFilterScope
		{
		public:

			CExceptionFilterScope(CExceptionFilter &_Filter)
			{
				auto &ThreadLocal = fg_SystemThreadLocal();
				m_pOldFilter = ThreadLocal.m_pExceptionFilter;
				ThreadLocal.m_pExceptionFilter = &_Filter;
			}

			~CExceptionFilterScope()
			{
				auto &ThreadLocal = fg_SystemThreadLocal();
				ThreadLocal.m_pExceptionFilter = m_pOldFilter;
			}
		private:
			DMibThreadLocalScopeDebugMember;
			CExceptionFilter *m_pOldFilter;
		};

		class CDisableExceptionFilterScope
		{
		public:

			CDisableExceptionFilterScope()
			{
				auto &ThreadLocal = fg_SystemThreadLocal();
				m_pOldFilter = ThreadLocal.m_pExceptionFilter;
				ThreadLocal.m_pExceptionFilter = nullptr;
			}

			~CDisableExceptionFilterScope()
			{
				auto &ThreadLocal = fg_SystemThreadLocal();
				ThreadLocal.m_pExceptionFilter = m_pOldFilter;
			}
		private:
			DMibThreadLocalScopeDebugMember;
			CExceptionFilter *m_pOldFilter;
		};

#		define DMibExceptionFilter(_Filter) NMib::NException::CExceptionFilterScope ScopeExceptionFilter(_Filter)
	}
}

namespace NMib
{
	namespace NLog
	{
		using CLogStr = NStr::CStrNonTracked;
	}

#if DMibSysLogSeverities
	namespace NLog
	{
		class CLogger;
		struct CLogFile;
	};
#endif

	// The global system object... Can be overridden, its not recommended if you don't know what you are doing though
	using FConstruct = void calling_convention_c (void);
	using FDestruct = void calling_convention_c (void);

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

#if defined(DCompiler_MSVC) || defined(DCompiler_clang_cl)
#define DWorkaroundVirtualLayout : CVirtualDestructor
#else
#define DWorkaroundVirtualLayout
#endif


//	#ifdef DPlatformFamily_macOS
//		using CMainHeapVirtualAllocator = NMemory::CAllocator_VirtualNoCommit;
//	#else
		using CMainHeapVirtualAllocator = NMemory::CAllocator_Virtual;
//	#endif

#ifdef DPlatformFamily_Windows
	using CSystemEnvironment = NContainer::TCMap<NStr::CStr, NStr::CStr, NStr::CCompareNoCase>;
#else
	using CSystemEnvironment = NContainer::TCMap<NStr::CStr, NStr::CStr>;
#endif

	class CSystem
	{
		friend struct CRunTimeObjectInfo;
		friend class NThread::CSemaphoreAggregate;
		friend class CSystemModule;

	private:

		NThread::CMutual mp_SubSystemsLock;
		DMibListLinkD_List(CSubSystem, m_Link) mp_SubSystems;

		class CCommandLineData
		{
		public:
			NStr::CStr m_CommandLine;
			NContainer::TCVector<NStr::CStr> m_lCommandLineParameters;
		};

		NMib::NStorage::TCAggregateSimple<CCommandLineData> m_CommandLineData;

		CVirtualMachineInfo mp_VMInfo;

		void fs_ParseCommandLine();

		NThread::CMutual m_Lock;

		NStr::CStr m_ProgramName;

		NStr::CStr m_CrashHandlerPath;
		NStr::CStr m_CrashHandlerExePath;
		NStr::CStr m_CrashHandlerServer;

		NStr::CStr m_SupportEmail;
		NStr::CStrNonTracked m_ProgramNameNonTracked;
		NStr::CStrNonTracked m_SupportEmailNonTracked;
		bool m_bRunningAsDaemon;

		bool m_bInitDone;
		bool m_bIsDll;

		NAtomic::TCAtomic<bool> m_bDestroyingThreadSpecific = false;

#if DMibConfig_Memory_Shims_EnableGlobal
		void fp_CreateGlobalMemoryReporter();
		void fp_DestroyGlobalMemoryReporter();
#endif

		void fp_CreateNonTrackedMemoryManager();
		void fp_DestroyNonTrackedMemoryManager();
		void fp_CreateMemoryManager();
		void fp_DestroyMemoryManager();

		void fp_SubSystem_PrepareFork_BeforeMemoryManager();
		void fp_SubSystem_ForkedChild_BeforeMemoryManager();
		void fp_SubSystem_ForkedParent_BeforeMemoryManager();

		void fp_SubSystem_PrepareFork_AfterMemoryManager();
		void fp_SubSystem_ForkedChild_AfterMemoryManager();
		void fp_SubSystem_ForkedParent_AfterMemoryManager();
		void fp_SubSystem_ForkedChild_AfterThreadLocal();
		void fp_SubSystem_ForkedParent_AfterThreadLocal();

		void fp_SubSystem_DestroyThreadLocal();
		void fp_SubSystem_DestroyThreadSpecific();
		void fp_SubSystem_PreDestroyThreadSpecific();
		void fp_SubSystem_DestroyBeforeMemoryManager();
		void fp_SubSystem_DestroyBeforeNonTrackedMemoryManager();
		void fp_SubSystem_DestroyBeforeThreadLocals();
		void fp_SubSystem_Destroy();
		void fp_SubSystem_ExitModule();
		void fp_SubSystem_DestroyAggregates(bool _bDestroySystem);

		void fp_SubSystem_DestroySubsystems(ESubSystemDestruction _ToDestroy);

	protected:
		NStr::CStr m_ProgramRoot;
		NStr::CStrNonTracked m_ProgramRootNonTracked;

#if DMibSysLogSeverities
		NLog::CLogger* m_pSystemLog;
		NLog::CLogFile* m_pDefaultLogFile;
#if DMibEnableTrace > 0
		umint m_TraceLoggerDestination = 0;
#endif
		umint m_StdErrLoggerDestination = 0;
		umint m_FileLoggerDestination = 0;
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
		bool f_HasTraceLogger() const;
		void f_RemoveTraceLogger();
		void f_RemoveAllLoggers();
		void f_AddStdErrLogger();
		void f_AddFileLogger();

		void f_RegisterProgram(const NStr::CStr &_ProgramName, const NStr::CStr &_SupportEmail, bool _bRunningAsDaemon)
		{
			m_ProgramName = _ProgramName;
			m_ProgramNameNonTracked = _ProgramName;
			m_SupportEmail = _SupportEmail;
			m_SupportEmailNonTracked = _SupportEmail;
			m_bRunningAsDaemon = _bRunningAsDaemon;
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

		bool f_GetRunningAsDaemon() const
		{
			return m_bRunningAsDaemon;
		}

		void f_SetRunningAsDaemon(bool _bRunningAsDaemon)
		{
			m_bRunningAsDaemon = _bRunningAsDaemon;
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

		bool f_InitDone()
		{
			return m_bInitDone;
		}

		bool f_DestroyingThreadSpecific()
		{
			return m_bDestroyingThreadSpecific.f_Load();
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

		void f_MemoryManager_OnThreadCreated(umint _ThreadID, umint _ParentID);

		NMemory::CMemoryManagerCheckout f_MemoryManager_Checkout();

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

		void *f_ThreadLocalAlloc(NThread::CThreadLocalInterface &_Key, umint &_ThreadLocalLocal);
		void f_ThreadLocalFree(NThread::CThreadLocalInterface &_Key, void *_pStorageIndex);
		void f_ThreadLocalFreeThread();
		void f_ThreadLocalFreeThreadFromNotification();
		void f_ThreadLocalCreateThread(umint _ThreadID, umint _ParentThreadID);
		void *f_ThreadLocalGet(void *_pStorageIndex);
		void f_ThreadLocalReinitForThread(void *_pStorageIndex);
		void f_ThreadLocalDestroyForThread(void *_pStorageIndex);
		void f_ThreadLocalSet(void *_pStorageIndex, void *_pValue);
		void f_OnThreadCreated(umint _ThreadID, umint _ParentID);
		void f_OnThreadDestroyed();

		void f_ThreadLocal_PrepareFork();
		void f_ThreadLocal_ForkedChild();
		void f_ThreadLocal_ForkedParent();

		void f_ThreadEnum(NFunction::TCFunction<void (umint _ThreadID)> const &_EnumFunc);
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

#ifdef DMibPAutomaticSystemCreation
	extern umint g_SystemMemory[];
#else
	extern CSystem *g_pSys;
#endif
	extern umint g_bCreatingSystemDone;
	extern umint g_bCanUseSystemMalloc;
	extern NAtomic::TCAtomic<umint> g_bCanStartThreads;
	extern umint g_bMemoryManagerNeededAfterDestroy;
	extern umint g_bCreatedSystem;

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

		DMibListLinkDA_List(NStorage::CAggregate, m_Link) m_Aggregates;

		NThread::CMutualAggregate m_Lock;

		constexpr CSystemModule(EAggregateInitialization _Init);

		void f_Destroy();
		void f_DestroyAggregates(bool _bDestroySystem);

		void f_Init(CSystem *_pSystem);

		void f_AddAggregate(NStorage::CAggregate *_pAggregate);
		void f_RemoveAggregate(NStorage::CAggregate *_pAggregate);
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

#include "Malterlib_Core_SubSystem.hpp"

