// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <sys/resource.h>
#include <linux/capability.h>
#include <signal.h>

#ifndef SCHED_RESET_ON_FORK
#	define SCHED_RESET_ON_FORK 0x40000000
#endif

namespace
{
	constexpr int gc_LinuxNiceBest = -20;
	constexpr int gc_LinuxNiceWorst = 19;
	constexpr int gc_LinuxNiceNotGranted = 20; // One past the worst nice, so no request satisfies it
	constexpr int gc_LinuxRealTimePriorityCeiling = 20; // Below the threaded interrupt handlers at 50
	constexpr umint gc_LinuxFairBandStart = 0x1FFF;
	constexpr umint gc_LinuxRealTimeBandStart = 0xE000;

	struct CLinuxSchedule
	{
		int m_Policy = SCHED_OTHER;
		int m_RealTimePriority = 0;
		int m_Nice = 0; // Under SCHED_IDLE this decides whether the thread may leave the policy again
	};

	struct CLinuxPriorityLimits
	{
		int m_StartupNice = 0;
		int m_GrantNice = gc_LinuxNiceNotGranted; // The best nice a thread may raise itself to
		int m_RealTimePriorityLimit = 0; // 0 when real-time policies are refused
	};

	constinit CLinuxPriorityLimits g_LinuxPriorityLimits;

	bool fg_Linux_HasSysNiceCapability()
	{
		__user_cap_header_struct Header = {};
		Header.version = _LINUX_CAPABILITY_VERSION_3;
		__user_cap_data_struct Data[_LINUX_CAPABILITY_U32S_3] = {};
		if (syscall(SYS_capget, &Header, Data) != 0)
			return false;

		return (Data[CAP_TO_INDEX(CAP_SYS_NICE)].effective & CAP_TO_MASK(CAP_SYS_NICE)) != 0;
	}

	// Must run after fg_SetupLimits and before any thread is created
	void fg_Linux_InitThreadPriorityLimits()
	{
		auto &Limits = g_LinuxPriorityLimits;

		errno = 0;
		int Nice = getpriority(PRIO_PROCESS, 0);
		if (Nice != -1 || errno == 0)
			Limits.m_StartupNice = Nice;

		if (fg_Linux_HasSysNiceCapability())
		{
			Limits.m_GrantNice = gc_LinuxNiceBest;
			Limits.m_RealTimePriorityLimit = sched_get_priority_max(SCHED_RR);
			return;
		}

		rlimit Limit;
		if (!getrlimit(RLIMIT_NICE, &Limit))
			Limits.m_GrantNice = 20 - (int)fg_Min(Limit.rlim_cur, rlim_t(40));

		if (!getrlimit(RLIMIT_RTPRIO, &Limit))
			Limits.m_RealTimePriorityLimit = (int)fg_Min(Limit.rlim_cur, rlim_t(sched_get_priority_max(SCHED_RR)));
	}

	int fg_Linux_GetCeilingNice()
	{
		auto &Limits = g_LinuxPriorityLimits;
		return fg_Min(Limits.m_StartupNice, Limits.m_GrantNice);
	}

	// Every thread can then reach the ceiling by itself, so creation is never handed off
	bool fg_Linux_IsPriorityGranted()
	{
		return g_LinuxPriorityLimits.m_GrantNice <= fg_Linux_GetCeilingNice();
	}

	int fg_Linux_GetFairNice(EExecutionPriority _Priority)
	{
		// The named priorities are 0x2AAB apart and five nice levels apart
		int Scaled = (int(EExecutionPriority_Normal) - int(_Priority)) * 5;
		int Rounding = 0x2AAB / 2;
		int Nice = (Scaled + (Scaled < 0 ? -Rounding : Rounding)) / 0x2AAB;

		return fg_Clamp(Nice, gc_LinuxNiceBest, gc_LinuxNiceWorst);
	}

	int fg_Linux_GetCeilingNiceForPriority(EExecutionPriority _Priority)
	{
		if (umint(_Priority) < gc_LinuxFairBandStart)
			return gc_LinuxNiceWorst;

		if (umint(_Priority) >= gc_LinuxRealTimeBandStart)
			return gc_LinuxNiceBest;

		return fg_Linux_GetFairNice(_Priority);
	}

	int fg_Linux_GetRealTimePriority(EExecutionPriority _Priority)
	{
		umint BandSize = umint(EExecutionPriority_Highest) - gc_LinuxRealTimeBandStart;
		umint iBand = fg_Min(umint(_Priority) - gc_LinuxRealTimeBandStart, BandSize);

		return 1 + int((iBand * umint(gc_LinuxRealTimePriorityCeiling - 1) + BandSize / 2) / BandSize);
	}

	CLinuxSchedule fg_Linux_MapThreadPriority(EExecutionPriority _Priority)
	{
		auto &Limits = g_LinuxPriorityLimits;
		int CeilingNice = fg_Min(fg_Linux_GetCeilingNice(), gc_LinuxNiceWorst);

		CLinuxSchedule Schedule;
		Schedule.m_Nice = CeilingNice;

		if (umint(_Priority) < gc_LinuxFairBandStart)
		{
			Schedule.m_Policy = SCHED_IDLE;
			return Schedule;
		}

		if (umint(_Priority) >= gc_LinuxRealTimeBandStart)
		{
			int Limit = fg_Min(Limits.m_RealTimePriorityLimit, gc_LinuxRealTimePriorityCeiling);
			if (Limit >= 1)
			{
				Schedule.m_Policy = SCHED_RR;
				Schedule.m_RealTimePriority = fg_Min(fg_Linux_GetRealTimePriority(_Priority), Limit);
				return Schedule;
			}

			return Schedule; // Real-time is not granted, which is the normal case and therefore not traced
		}

		Schedule.m_Nice = fg_Max(fg_Linux_GetFairNice(_Priority), CeilingNice);
		return Schedule;
	}

	// _ThreadID 0 is the calling thread
	CLinuxSchedule fg_Linux_GetSchedule(pid_t _ThreadID)
	{
		CLinuxSchedule Schedule;

		int Policy = sched_getscheduler(_ThreadID);
		if (Policy >= 0)
			Schedule.m_Policy = Policy & ~SCHED_RESET_ON_FORK;

		if (Schedule.m_Policy == SCHED_RR || Schedule.m_Policy == SCHED_FIFO)
		{
			sched_param Param = {};
			if (!sched_getparam(_ThreadID, &Param))
				Schedule.m_RealTimePriority = Param.sched_priority;
		}

		errno = 0;
		int Nice = getpriority(PRIO_PROCESS, _ThreadID);
		if (Nice != -1 || errno == 0)
			Schedule.m_Nice = Nice;

		return Schedule;
	}

	// Mirrors the kernel's user_check_sched_setscheduler and set_one_prio for a thread that inherited _Inherited
	bool fg_Linux_CanSelfApply(CLinuxSchedule const &_Desired, CLinuxSchedule const &_Inherited)
	{
		int GrantNice = g_LinuxPriorityLimits.m_GrantNice;

		if (_Inherited.m_Policy == SCHED_IDLE && _Desired.m_Policy != SCHED_IDLE && _Inherited.m_Nice < GrantNice)
			return false;

		if (_Desired.m_Policy == SCHED_IDLE || _Desired.m_Policy == SCHED_RR)
			return true; // The real-time priority was already clamped to what the process may request

		return _Desired.m_Nice >= _Inherited.m_Nice || _Desired.m_Nice >= GrantNice;
	}

	bool fg_Linux_GetKernelThreadID(void *_pThread, pid_t &o_ThreadID)
	{
		if (_pThread == NSys::fg_Thread_GetCurrent())
		{
			o_ThreadID = fg_Malterlib_Thread_GetTID_Local();
			return true;
		}

		// The CPU clock id encodes the kernel thread id as kernel ABI; pthread_gettid_np needs glibc 2.42
		clockid_t Clock;
		if (pthread_getcpuclockid((pthread_t)_pThread, &Clock) != 0)
			return false;

		o_ThreadID = (pid_t)~(Clock >> 3);
		return true;
	}

	CSetPriorityError fg_Linux_SetThreadPriority(void *_pThread, EExecutionPriority _Priority)
	{
		if (_Priority == EExecutionPriority_Default)
			return {};

		pid_t ThreadID;
		if (!fg_Linux_GetKernelThreadID(_pThread, ThreadID))
			return {"pthread_getcpuclockid (set thread priority)", ESRCH};

		CLinuxSchedule Desired = fg_Linux_MapThreadPriority(_Priority);

		// The helper inherits from its creator, so it has to exist before any thread is lowered
		bool bLowers = Desired.m_Policy == SCHED_IDLE || (Desired.m_Policy == SCHED_OTHER && Desired.m_Nice > fg_Linux_GetCeilingNice());
		if (bLowers)
			fg_Linux_EnsureThreadSpawnHelper();

		if (Desired.m_Policy == SCHED_IDLE)
		{
			// Leaving SCHED_IDLE is checked against the nice the thread holds, and only the ceiling is always granted
			errno = 0;
			int CurrentNice = getpriority(PRIO_PROCESS, ThreadID);
			bool bKnown = CurrentNice != -1 || errno == 0;
			if (bKnown && CurrentNice != Desired.m_Nice && (CurrentNice < Desired.m_Nice || Desired.m_Nice >= g_LinuxPriorityLimits.m_GrantNice))
			{
				if (setpriority(PRIO_PROCESS, ThreadID, Desired.m_Nice) != 0)
					return {"setpriority (set thread priority)", errno};
			}
		}

		sched_param ScheduleParams = {};
		ScheduleParams.sched_priority = Desired.m_RealTimePriority;

		int Result = pthread_setschedparam((pthread_t)_pThread, Desired.m_Policy, &ScheduleParams);
		if (Result != 0)
			return {"pthread_setschedparam (set thread priority)", Result};

		if (Desired.m_Policy == SCHED_OTHER && setpriority(PRIO_PROCESS, ThreadID, Desired.m_Nice) != 0)
			return {"setpriority (set thread priority)", errno};

		return {};
	}
}

// *************************************************************************************************************************
// Priority-correct thread creation
// *************************************************************************************************************************

namespace
{
	enum ELinuxThreadSpawnRequestState
	{
		ELinuxThreadSpawnRequestState_Pending
		, ELinuxThreadSpawnRequestState_Done
		, ELinuxThreadSpawnRequestState_Abandoned // Nobody will serve the request, the requester creates the thread itself
	};

	struct CLinuxThreadSpawnRequest
	{
		DMibListLinkDS_Link(CLinuxThreadSpawnRequest, m_Link);

		CThreadCreateParams m_Params;
		cpu_set_t m_Affinity;
		sigset_t m_SignalMask;
		NThread::CEventAutoResetAggregate m_Finished = {DAggregateInit}; // Signaled under the lock, which the requester takes before it reads the result
		ELinuxThreadSpawnRequestState m_State = ELinuxThreadSpawnRequestState_Pending;
		void *m_pThread = nullptr;
		umint m_ThreadID = 0;
		NStr::CStrNonTracked m_Error;
		bool m_bFailed = false;
	};

	// Never allocate under m_Lock: an allocation can start the memory manager's cleanup thread and re-enter here
	struct CLinuxThreadSpawnState
	{
		NThread::CLowLevelLockAggregate m_Lock = {DAggregateInit};
		NThread::CEventAutoResetAggregate m_RequestAdded = {DAggregateInit}; // Wakes the helper
		NThread::CEventAutoResetAggregate m_ServerWakeDone = {DAggregateInit}; // Wakes an unregistering server
		NThread::CEventAggregate m_HelperCreateDone = {DAggregateInit}; // Wakes threads that found the helper half created

		DMibListLinkDSA_List(CLinuxThreadSpawnRequest, m_Link) m_Requests = {DAggregateInit};

		NSys::FThreadSpawnServerWake *m_fServerWake = nullptr;
		void *m_pServerContext = nullptr;
		umint m_ServerThreadUID = 0;
		pid_t m_ServerThreadID = 0;
		umint m_nServerWakers = 0;

		void *m_pHelperThread = nullptr;
		umint m_HelperThreadUID = 0;
		pid_t m_HelperThreadID = 0; // 0 while the helper is being created
		umint m_HelperCreatorUID = 0;
		NAtomic::TCAtomic<bool> m_bHelperCreated{false};
		bool m_bHelperWanted = false; // Asked for while a server was registered or threads could not start
		bool m_bHelperQuit = false;

#ifdef DMibDynamicLibrary
		bool m_bDisabled = true; // Every module would run its own helper
#else
		bool m_bDisabled = false;
#endif
	};

	constinit CLinuxThreadSpawnState g_LinuxThreadSpawn;

	// Must run before any thread is created
	void fg_Linux_InitThreadSpawn()
	{
		g_LinuxThreadSpawn.m_Requests.f_Construct(); // An aggregate list is not usable in its constant initialized state
	}

	// Requires the lock
	void fg_Linux_AbandonSpawnRequests()
	{
		while (auto *pRequest = g_LinuxThreadSpawn.m_Requests.f_Pop())
		{
			pRequest->m_State = ELinuxThreadSpawnRequestState_Abandoned;
			pRequest->m_Finished.f_Signal();
		}
	}

	// The signal mask is inherited from the creating thread, so the requester's is borrowed for the creation
	void fg_Linux_ExecuteSpawnRequest(CLinuxThreadSpawnRequest &_Request)
	{
		sigset_t PreviousSignalMask;
		pthread_sigmask(SIG_SETMASK, &_Request.m_SignalMask, &PreviousSignalMask);

		try
		{
			_Request.m_pThread = fg_POSIX_CreateThread(_Request.m_Params, _Request.m_ThreadID);
		}
		catch (NException::CException const &_Exception)
		{
			_Request.m_Error = _Exception.f_GetErrorStr();
			_Request.m_bFailed = true;
		}

		pthread_sigmask(SIG_SETMASK, &PreviousSignalMask, nullptr);
	}

	// Requires the lock, which is released around each creation
	void fg_Linux_ServeSpawnRequests()
	{
		auto &State = g_LinuxThreadSpawn;

		while (auto *pRequest = State.m_Requests.f_Pop())
		{
			{
				DMibUnlock(State.m_Lock);
				fg_Linux_ExecuteSpawnRequest(*pRequest);
			}

			pRequest->m_State = ELinuxThreadSpawnRequestState_Done;
			pRequest->m_Finished.f_Signal();
		}
	}

	aint fg_Linux_ThreadSpawnHelperMain(void *)
	{
		auto &State = g_LinuxThreadSpawn;

		while (true)
		{
			{
				DMibLock(State.m_Lock);
				fg_Linux_ServeSpawnRequests();

				if (State.m_bHelperQuit)
					return 0;
			}

			State.m_RequestAdded.f_Wait();
		}
	}

	void fg_Linux_EnsureThreadSpawnHelper()
	{
		auto &State = g_LinuxThreadSpawn;
		if (State.m_bHelperCreated.f_Load(NAtomic::gc_MemoryOrder_Relaxed) || fg_Linux_IsPriorityGranted())
			return;

		{
			DMibLock(State.m_Lock);
			if (State.m_bDisabled || State.m_bHelperCreated.f_Load())
				return;

			if (State.m_pServerContext || !g_bCanStartThreads.f_Load(NAtomic::gc_MemoryOrder_Relaxed))
			{
				State.m_bHelperWanted = true;
				return;
			}

			State.m_bHelperCreated = true;
			State.m_HelperCreatorUID = NSys::fg_Thread_GetCurrentUID();
			State.m_HelperCreateDone.f_ResetSignaled();
		}

		CLinuxSchedule Schedule = fg_Linux_GetSchedule(0);
		if (Schedule.m_Policy == SCHED_IDLE || (Schedule.m_Policy == SCHED_OTHER && Schedule.m_Nice > fg_Linux_GetCeilingNice()))
			DMibDTraceSafe("The thread spawn helper is created from a lowered thread (policy {}, nice {}) and cannot restore priorities\n", Schedule.m_Policy, Schedule.m_Nice);

		CThreadCreateParams Params;
		Params.m_pThreadProc = &fg_Linux_ThreadSpawnHelperMain;
		Params.m_Priority = EExecutionPriority_Default; // Keeps the inherited priority
		Params.m_pThreadName = "Thread spawner";
		Params.m_ParentThreadID = NSys::fg_Thread_GetCurrentUID();

		void *pThread = nullptr;
		umint ThreadUID = 0;
		pid_t ThreadID = 0;
		try
		{
			pThread = fg_POSIX_CreateThread(Params, ThreadUID);
			fg_Linux_GetKernelThreadID(pThread, ThreadID);
		}
		catch ([[maybe_unused]] NException::CException const &_Exception)
		{
			DMibDTraceSafe("Failed to create the thread spawn helper: {}\n", _Exception.f_GetErrorStr());
		}

		DMibLock(State.m_Lock);
		State.m_pHelperThread = pThread;
		State.m_HelperThreadUID = ThreadUID;
		State.m_HelperThreadID = ThreadID;
		State.m_HelperCreatorUID = 0;
		if (!pThread)
			State.m_bHelperCreated = false;

		State.m_HelperCreateDone.f_SetSignaled();
	}

	bool fg_Linux_RouteThreadCreate(CThreadCreateParams const &_Params, void *&o_pThread, umint &o_ThreadID)
	{
		if (_Params.m_Priority == EExecutionPriority_Default || fg_Linux_IsPriorityGranted())
			return false;

		CLinuxSchedule Desired = fg_Linux_MapThreadPriority(_Params.m_Priority);
		if (fg_Linux_CanSelfApply(Desired, fg_Linux_GetSchedule(0)))
			return false;

		auto &State = g_LinuxThreadSpawn;
		umint CurrentThreadUID = NSys::fg_Thread_GetCurrentUID();

		CLinuxThreadSpawnRequest Request;
		Request.m_Params = _Params;
		pthread_sigmask(SIG_SETMASK, nullptr, &Request.m_SignalMask);
		if (!_Params.m_Affinity && !sched_getaffinity(0, sizeof(Request.m_Affinity), &Request.m_Affinity))
			Request.m_Params.m_pInheritedAffinity = &Request.m_Affinity;

		{
			DMibLock(State.m_Lock);

			// The thread that creates the helper re-enters here when the creation allocates
			while (State.m_bHelperCreated.f_Load() && !State.m_HelperThreadID && State.m_HelperCreatorUID != CurrentThreadUID && !State.m_bDisabled)
			{
				DMibUnlock(State.m_Lock);
				State.m_HelperCreateDone.f_Wait();
			}

			bool bServer = State.m_pServerContext && State.m_ServerThreadUID != CurrentThreadUID;
			bool bHelper = !bServer && State.m_HelperThreadID && !State.m_bHelperQuit && State.m_HelperThreadUID != CurrentThreadUID;
			pid_t CreatorThreadID = bServer ? State.m_ServerThreadID : State.m_HelperThreadID;

			if (State.m_bDisabled)
				return false;

			if ((!bServer && !bHelper) || !fg_Linux_CanSelfApply(Desired, fg_Linux_GetSchedule(CreatorThreadID)))
			{
				DMibUnlock(State.m_Lock);
				DMibDTraceSafe("No thread can create thread '{}' at priority {}, it will run below it\n", _Params.m_pThreadName, (int)_Params.m_Priority);
				return false;
			}

			State.m_Requests.f_Insert(Request);

			if (bServer)
			{
				auto *fWake = State.m_fServerWake;
				void *pContext = State.m_pServerContext;
				++State.m_nServerWakers;
				{
					DMibUnlock(State.m_Lock);
					fWake(pContext);
				}
				--State.m_nServerWakers;
				State.m_ServerWakeDone.f_Signal();
			}
			else
				State.m_RequestAdded.f_Signal();
		}

		Request.m_Finished.f_Wait();
		{
			DMibLock(State.m_Lock);
		}

		if (Request.m_State == ELinuxThreadSpawnRequestState_Abandoned)
		{
			DMibDTraceSafe("Thread creation is shutting down, thread '{}' will run below priority {}\n", _Params.m_pThreadName, (int)_Params.m_Priority);
			return false;
		}

		if (Request.m_bFailed)
			DMibError(NStr::CStr(Request.m_Error));

		o_pThread = Request.m_pThread;
		o_ThreadID = Request.m_ThreadID;
		return true;
	}

	void fg_Linux_ThreadSpawn_CanStartThreads()
	{
		auto &State = g_LinuxThreadSpawn;
		{
			DMibLock(State.m_Lock);
			if (!State.m_bHelperWanted || State.m_pServerContext)
				return;

			State.m_bHelperWanted = false;
		}

		fg_Linux_EnsureThreadSpawnHelper();
	}

	void fg_Linux_ThreadSpawn_DestroyThreads()
	{
		auto &State = g_LinuxThreadSpawn;
		void *pHelperThread;
		{
			DMibLock(State.m_Lock);
			State.m_bDisabled = true;
			fg_Linux_AbandonSpawnRequests();

			while (State.m_bHelperCreated.f_Load() && !State.m_HelperThreadID)
			{
				DMibUnlock(State.m_Lock);
				State.m_HelperCreateDone.f_Wait();
			}

			if (!State.m_bHelperCreated.f_Load())
				return;

			State.m_bHelperQuit = true;
			pHelperThread = State.m_pHelperThread;
			State.m_RequestAdded.f_Signal();
		}

		void *pDestroyContext = NSys::fg_Thread_BeginDestroy(pHelperThread);
		NSys::fg_Thread_BlockUntilExit(pDestroyContext);
		NSys::fg_Thread_EndDestroy(pDestroyContext);
	}

	void fg_Linux_ThreadSpawn_ForkPrepare()
	{
		g_LinuxThreadSpawn.m_Lock.f_Lock();
	}

	void fg_Linux_ThreadSpawn_ForkParent()
	{
		g_LinuxThreadSpawn.m_Lock.f_Unlock();
	}

	// Neither the server nor the helper thread exists in the child, so creation stays direct there
	void fg_Linux_ThreadSpawn_ForkChild()
	{
		auto &State = g_LinuxThreadSpawn;
		State.m_Lock.f_ForkedChildLocked();
		State.m_RequestAdded.f_ForkedChild();
		State.m_ServerWakeDone.f_ForkedChild();
		State.m_HelperCreateDone.f_ForkedChild();

		while (State.m_Requests.f_Pop())
			;

		State.m_fServerWake = nullptr;
		State.m_pServerContext = nullptr;
		State.m_ServerThreadUID = 0;
		State.m_ServerThreadID = 0;
		State.m_nServerWakers = 0;
		State.m_pHelperThread = nullptr;
		State.m_HelperThreadUID = 0;
		State.m_HelperThreadID = 0;
		State.m_HelperCreatorUID = 0;
		State.m_bHelperCreated = false;
		State.m_bHelperWanted = false;
		State.m_bHelperQuit = false;
		State.m_bDisabled = true;

		State.m_Lock.f_Unlock();
	}
}

// Whether a thread that lowers its priority can raise it again. Without the grant it cannot, so a thread that has
// to be raised later must not be lowered
bool NSys::fg_Thread_CanRestorePriority()
{
	return fg_Linux_IsPriorityGranted();
}

// The calling thread creates threads for lowered threads in place of the spawn helper thread, and may only block
// where _fWake reaches it. _fWake is called from any thread, must not create threads, and has to make the calling
// thread run fg_Thread_ServeSpawnRequests. Returns false when creation is never handed off or the helper already serves
bool NSys::fg_Thread_RegisterSpawnServer(FThreadSpawnServerWake *_fWake, void *_pContext)
{
	if (fg_Linux_IsPriorityGranted())
		return false;

	auto &State = g_LinuxThreadSpawn;

	DMibLock(State.m_Lock);
	if (State.m_bDisabled || State.m_pServerContext || State.m_bHelperCreated.f_Load())
		return false;

	State.m_fServerWake = _fWake;
	State.m_pServerContext = _pContext;
	State.m_ServerThreadUID = NSys::fg_Thread_GetCurrentUID();
	State.m_ServerThreadID = fg_Malterlib_Thread_GetTID_Local();

	return true;
}

// Call on the registered thread. Outstanding requests are served before it returns
void NSys::fg_Thread_UnregisterSpawnServer(void *_pContext)
{
	auto &State = g_LinuxThreadSpawn;
	{
		DMibLock(State.m_Lock);
		if (State.m_pServerContext != _pContext)
			return;

		State.m_pServerContext = nullptr;
		State.m_fServerWake = nullptr;

		while (State.m_nServerWakers)
		{
			DMibUnlock(State.m_Lock);
			State.m_ServerWakeDone.f_Wait();
		}

		fg_Linux_ServeSpawnRequests();

		if (!State.m_bHelperWanted)
			return;
	}

	// Later creations need the helper, and this thread still holds the priority to create it from
	fg_Linux_ThreadSpawn_CanStartThreads();
}

void NSys::fg_Thread_ServeSpawnRequests()
{
	auto &State = g_LinuxThreadSpawn;

	DMibLock(State.m_Lock);
	if (!State.m_pServerContext || State.m_ServerThreadUID != NSys::fg_Thread_GetCurrentUID())
		return;

	fg_Linux_ServeSpawnRequests();
}

// What a process has to be started with for its threads to reach _Priority and nothing better
NSys::CLinuxPriorityGrant NSys::fg_Process_GetLinuxPriorityGrant(EExecutionPriority _Priority)
{
	if (_Priority == EExecutionPriority_Default)
		_Priority = EExecutionPriority_Normal;

	int CeilingNice = fg_Linux_GetCeilingNiceForPriority(_Priority);

	CLinuxPriorityGrant Grant;
	Grant.m_StartNice = uint32(fg_Max(CeilingNice, 0));
	Grant.m_NiceLimit = uint32(20 - CeilingNice);

	if (umint(_Priority) >= gc_LinuxRealTimeBandStart)
		Grant.m_RealTimePriorityLimit = uint32(fg_Linux_GetRealTimePriority(_Priority));

	return Grant;
}

void fg_MalterlibThreadSpawn_CanStartThreads()
{
	fg_Linux_ThreadSpawn_CanStartThreads();
}

void fg_MalterlibThreadSpawn_DestroyThreads()
{
	fg_Linux_ThreadSpawn_DestroyThreads();
}
