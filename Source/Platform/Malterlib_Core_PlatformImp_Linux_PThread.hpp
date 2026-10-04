// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#ifdef DMibStaticThreadLocals

#ifdef DArchitecture_arm64
#define DAddThreadSelfOffset + g_ThreadSelfOffset
#else
#define DAddThreadSelfOffset
#endif

#endif

#ifdef DMibConfig_LinuxPThreadMonitoring

using namespace NMib;

#include "Malterlib_Core_ThreadNotificationCrossModule.h"
#include <Mib/Container/MapWithPool>
#include <Mib/Container/SetWithPool>

extern NMib::NAtomic::TCAtomic<bool> g_bSysDeleted;

namespace
{
#ifdef DMibDynamicLibrary
	constinit NThread::CLowLevelRecursiveLockAggregate g_ThreadCreationNotificationLock = {DAggregateInit};
#endif

	void fg_MalterlibThreadCreatedNotificationLocal(umint _ThreadID, umint _ParentThreadID)
	{
#ifdef DMibDynamicLibrary
		DMibLock(g_ThreadCreationNotificationLock);
#endif
		if (g_bSysDeleted)
		{
			// The thread gets no locals, and the memory manager must not expect them until the context is destroyed
			if (_ThreadID == NSys::fg_Thread_GetCurrentUID())
				NSys::fg_Thread_SetLocalsDestroyed(true);

			return;
		}

		fg_GetLocalSys()->f_OnThreadCreated(_ThreadID, _ParentThreadID);
	}

	void fg_MalterlibThreadTerminatedNotificationLocal(umint _ThreadID)
	{
		fg_GetLocalSys()->f_OnThreadDestroyed();
		NSys::fg_Thread_SetLocalsDestroyed(true);
	}
}

#ifndef DMibDynamicLibrary

namespace
{
	struct CThreadNotificationRegistration
	{
		auto f_GetModule() const -> NSys::NPrivate::CThreadNotificationModule *
		{
			using CNotifications = NContainer::TCMapWithPool
				<
					NSys::NPrivate::CThreadNotificationModule *
					, CThreadNotificationRegistration
					, CSort_Default
					, NMemory::CAllocator_VirtualNoTracking
				>
			;
			return CNotifications::fs_GetKey(*this);
		}

		DMibListLinkDS_Link(CThreadNotificationRegistration, m_Link);
	};

	struct CThreadNotificationState
	{
		using CNotifications = NContainer::TCMapWithPool
			<
				NSys::NPrivate::CThreadNotificationModule *
				, CThreadNotificationRegistration
				, CSort_Default
				, NMemory::CAllocator_VirtualNoTracking
			>
		;

		NContainer::TCSetWithPool<umint, CSort_Default, NMemory::CAllocator_VirtualNoTracking> m_LiveThreads;
		CNotifications m_Notifications;
		DMibListLinkDS_List(CThreadNotificationRegistration, m_Link) m_NotificationOrder;
	};

	struct CPThreadOverrideStartParams
	{
		void *(*m_fStart)(void *);
		void *m_pArgument;
		umint m_ParentThreadID;
		DMibListLinkDS_Link(CPThreadOverrideStartParams, m_Link);
	};

	struct CPThreadStartPool
	{
		NMemory::TCPool<CPThreadOverrideStartParams, 64, NThread::CNoLock, NMemory::CPoolType_FreeableSmall, NMemory::CAllocator_VirtualNoTracking> m_Pool;
		DMibListLinkDS_List(CPThreadOverrideStartParams, m_Link) m_Pending;
	};

	constinit NStorage::TCAggregateSimple<CPThreadStartPool> g_PThreadStartPool = {DAggregateInit};
	constinit NThread::CLowLevelRecursiveLockAggregate g_ThreadNotificationLock = {DAggregateInit};
	constinit NStorage::TCAggregateSimple<CThreadNotificationState> g_ThreadNotificationState = {DAggregateInit};
	constinit NSys::NPrivate::CThreadNotificationModule g_LocalThreadNotificationModule =
		{
			.m_Version = NSys::NPrivate::EThreadNotificationCrossModule_Version
			, .m_Reserved = {}
			, .m_fCreated = &fg_MalterlibThreadCreatedNotificationLocal
			, .m_fTerminated = &fg_MalterlibThreadTerminatedNotificationLocal
			, .m_fForkPrepare = nullptr
			, .m_fForkParent = nullptr
			, .m_fForkChild = nullptr
		}
	;
	constinit NAtomic::TCAtomic<bool> g_bThreadNotificationsInitialized{false};

	// The caller holds g_ThreadNotificationLock.
	void fg_ReleasePThreadStartParams(CPThreadOverrideStartParams *_pParams)
	{
		auto &Pool = *g_PThreadStartPool;
		_pParams->m_Link.f_Unlink();
		Pool.m_Pool.f_Delete(_pParams);

		// A child may enter its wrapper after the notification registry has shut down.
		if (!g_bThreadNotificationsInitialized && Pool.m_Pending.f_IsEmpty())
			g_PThreadStartPool.f_Destruct();
	}

	void fg_PThreadTerminated(void *_pThread)
	{
		umint ThreadID = reinterpret_cast<umint>(_pThread);
		DMibLock(g_ThreadNotificationLock);
		if (!g_bThreadNotificationsInitialized)
			return;

		auto &State = *g_ThreadNotificationState;
		State.m_LiveThreads.f_Remove(ThreadID);

		auto iRegistration = State.m_NotificationOrder.f_GetIterator();
		iRegistration.f_Reverse(State.m_NotificationOrder);
		for (; iRegistration; --iRegistration)
		{
			auto pModule = iRegistration->f_GetModule();
			pModule->m_fTerminated(ThreadID);
		}
	}

	void *fg_PThreadOverrideStart(void *_pParams)
	{
		auto *pParams = static_cast<CPThreadOverrideStartParams *>(_pParams);
		auto fStart = pParams->m_fStart;
		void *pArgument = pParams->m_pArgument;
		umint ParentThreadID = pParams->m_ParentThreadID;

		umint ThreadID = NSys::fg_Thread_GetCurrentUID();

	#ifdef DUseGlibcDummyThreadLocalLevel2
		CGlibcDummyThreadLocalLevel2 DummyThreadLocalLevel2;
		fg_Glibc_InstallDummyThreadLocalLevel2(DummyThreadLocalLevel2);
	#endif

		{
			DMibLock(g_ThreadNotificationLock);
			fg_ReleasePThreadStartParams(pParams);

			if (g_bThreadNotificationsInitialized)
			{
				auto &State = *g_ThreadNotificationState;
				if (!State.m_LiveThreads.f_Exists(ThreadID))
				{
					for (auto &Registration : State.m_NotificationOrder)
						Registration.f_GetModule()->m_fCreated(ThreadID, ParentThreadID);

					State.m_LiveThreads.f_Insert(ThreadID);
				}
			}
		}

	#ifdef DUseGlibcDummyThreadLocalLevel2
		fg_Glibc_ReplaceDummyThreadLocalLevel2(DummyThreadLocalLevel2);
	#endif

		void *pResult;
		// This runs before glibc destroys pthread-specific data on normal return,
		// pthread_exit and cancellation.
		pthread_cleanup_push(&fg_PThreadTerminated, reinterpret_cast<void *>(ThreadID));
		pResult = fStart(pArgument);
		pthread_cleanup_pop(1);
		return pResult;
	}

	void fg_InstallPThreadOverride()
	{
		DMibFastCheck(NLocal::g_f_pthread_create);
		DMibFastCheck(!g_bThreadNotificationsInitialized);
		g_PThreadStartPool.f_Construct();
		g_ThreadNotificationState.f_Construct();

		{
			DMibLock(g_ThreadNotificationLock);

			auto &State = *g_ThreadNotificationState;
			State.m_LiveThreads.f_Insert(NSys::fg_Thread_GetCurrentUID());
			auto &Registration = State.m_Notifications[&g_LocalThreadNotificationModule];
			State.m_NotificationOrder.f_Insert(Registration);
		}

		g_bThreadNotificationsInitialized = true;
	}

	void fg_DestroyPThreadOverride()
	{
		DMibLock(g_ThreadNotificationLock);
		if (!g_bThreadNotificationsInitialized)
			return;

		g_bThreadNotificationsInitialized = false;
		g_ThreadNotificationState.f_Destruct();
		if (g_PThreadStartPool->m_Pending.f_IsEmpty())
			g_PThreadStartPool.f_Destruct();
	}

	void fg_ThreadNotificationsForkPrepare()
	{
		if (!g_bThreadNotificationsInitialized)
			return;

		g_ThreadNotificationLock.f_Lock();
		auto &State = *g_ThreadNotificationState;
		for (auto &Registration : State.m_NotificationOrder)
		{
			auto pModule = Registration.f_GetModule();
			if (pModule->m_fForkPrepare)
				pModule->m_fForkPrepare();
		}
	}

	void fg_ThreadNotificationsForkParent()
	{
		if (!g_bThreadNotificationsInitialized)
			return;

		auto &State = *g_ThreadNotificationState;
		auto iRegistration = State.m_NotificationOrder.f_GetIterator();
		iRegistration.f_Reverse(State.m_NotificationOrder);
		for (; iRegistration; --iRegistration)
		{
			auto pModule = iRegistration->f_GetModule();
			if (pModule->m_fForkParent)
				pModule->m_fForkParent();
		}

		g_ThreadNotificationLock.f_Unlock();
	}

	void fg_ThreadNotificationsForkChild()
	{
		if (!g_bThreadNotificationsInitialized)
			return;

		auto &State = *g_ThreadNotificationState;
		auto iRegistration = State.m_NotificationOrder.f_GetIterator();
		iRegistration.f_Reverse(State.m_NotificationOrder);
		for (; iRegistration; --iRegistration)
		{
			auto pModule = iRegistration->f_GetModule();
			if (pModule->m_fForkChild)
				pModule->m_fForkChild();
		}

		// Pending starts belong to parent threads; the forking thread has already returned its own record.
		{
			auto &Pool = *g_PThreadStartPool;
			while (auto *pParams = Pool.m_Pending.f_GetFirst())
			{
				pParams->m_Link.f_Unlink();
				Pool.m_Pool.f_Delete(pParams);
			}
		}

		State.m_LiveThreads.f_Clear();
		State.m_LiveThreads.f_Insert(NSys::fg_Thread_GetCurrentUID());

		g_ThreadNotificationLock.f_ForkedChildLocked();
		g_ThreadNotificationLock.f_Unlock();
	}

}

extern "C" assure_used module_export int pthread_create
	(
		pthread_t *__restrict _pThread
		, pthread_attr_t const *__restrict _pAttributes
		, void * (*_fStart)(void *)
		, void *__restrict _pArgument
	) __THROWNL
{
	// The Malterlib executable must initialize before any dependent DSO constructor
	// uses Malterlib; memory interposition and cross-module interfaces rely on this
	// process-wide ordering. In particular, neither the executable nor a load-time
	// DSO may create a thread before CSystem::f_InitModuleThreaded(), so normal
	// initialization has resolved this pointer before any caller can enter.
	DMibFastCheck(NLocal::g_f_pthread_create);

	// A foreign thread can lower itself and later create one of ours, so the spawn helper has to exist before it
	if (!g_bLinuxOwnThreadCreate)
		fg_Linux_EnsureThreadSpawnHelper();

	if (!g_bThreadNotificationsInitialized)
		return NLocal::g_f_pthread_create(_pThread, _pAttributes, _fStart, _pArgument);

	CPThreadOverrideStartParams *pParams = nullptr;
	{
		DMibLock(g_ThreadNotificationLock);
		if (g_bThreadNotificationsInitialized)
		{
			auto &Pool = *g_PThreadStartPool;
			pParams = Pool.m_Pool.f_New();
			pParams->m_fStart = _fStart;
			pParams->m_pArgument = _pArgument;
			pParams->m_ParentThreadID = NSys::fg_Thread_GetCurrentUID();
			Pool.m_Pending.f_Insert(pParams);
		}
	}
	if (!pParams)
		return NLocal::g_f_pthread_create(_pThread, _pAttributes, _fStart, _pArgument);

	auto Cleanup = g_OnScopeExit / [pParams]
		{
			DMibLock(g_ThreadNotificationLock);
			fg_ReleasePThreadStartParams(pParams);
		}
	;

	int Result = NLocal::g_f_pthread_create(_pThread, _pAttributes, &fg_PThreadOverrideStart, pParams);
	if (!Result)
		Cleanup.f_Clear();

	return Result;
}

// Threads that Malterlib does not know about only fail where its memory manager replaces malloc. Sanitizers replace
// malloc themselves, and look symbols up while they initialize, before what this calls can run
#if defined(DMibConfig_OverrideSystemMalloc) && !defined(DMibSanitizerEnabled_Address) && !defined(DMibSanitizerEnabled_Thread)
namespace
{
	using FDlsym = void *(void *__restrict _pHandle, char const *__restrict _pName) noexcept;
	using FDlvsym = void *(void *__restrict _pHandle, char const *__restrict _pName, char const *__restrict _pVersion) noexcept;

	constinit NAtomic::TCAtomic<FDlsym *> g_fRealDlsym{nullptr};
	constinit NAtomic::TCAtomic<FDlvsym *> g_fRealDlvsym{nullptr};

	// The dynamic linker relocates the addresses in the dynamic section in place on most architectures, and leaves
	// them relative to the library on some
	ElfW(Addr) fg_GetDynamicAddress(dl_phdr_info const &_Info, ElfW(Addr) _Address)
	{
		return _Address < _Info.dlpi_addr ? _Address + _Info.dlpi_addr : _Address;
	}

	// The default version of a function that a library defines, found in its hash table, since dlsym is what is being
	// found
	void *fg_FindDefinedFunction(dl_phdr_info const &_Info, char const *_pName)
	{
		ElfW(Dyn) const *pDynamic = nullptr;
		for (umint iHeader = 0; iHeader < _Info.dlpi_phnum; ++iHeader)
		{
			if (_Info.dlpi_phdr[iHeader].p_type == PT_DYNAMIC)
				pDynamic = reinterpret_cast<ElfW(Dyn) const *>(_Info.dlpi_addr + _Info.dlpi_phdr[iHeader].p_vaddr);
		}

		if (!pDynamic)
			return nullptr;

		uint32 const *pGnuHash = nullptr;
		ElfW(Word) const *pHash = nullptr;
		ElfW(Sym) const *pSymbols = nullptr;
		char const *pStrings = nullptr;
		ElfW(Half) const *pVersions = nullptr;
		for (auto *pEntry = pDynamic; pEntry->d_tag != DT_NULL; ++pEntry)
		{
			switch (pEntry->d_tag)
			{
			case DT_GNU_HASH: pGnuHash = reinterpret_cast<uint32 const *>(fg_GetDynamicAddress(_Info, pEntry->d_un.d_ptr)); break;
			case DT_HASH: pHash = reinterpret_cast<ElfW(Word) const *>(fg_GetDynamicAddress(_Info, pEntry->d_un.d_ptr)); break;
			case DT_SYMTAB: pSymbols = reinterpret_cast<ElfW(Sym) const *>(fg_GetDynamicAddress(_Info, pEntry->d_un.d_ptr)); break;
			case DT_STRTAB: pStrings = reinterpret_cast<char const *>(fg_GetDynamicAddress(_Info, pEntry->d_un.d_ptr)); break;
			case DT_VERSYM: pVersions = reinterpret_cast<ElfW(Half) const *>(fg_GetDynamicAddress(_Info, pEntry->d_un.d_ptr)); break;
			}
		}

		if (!pSymbols || !pStrings)
			return nullptr;

		// The high bit of the version hides versions that are not the default one
		auto fIsDefinedFunction = [&](uint32 _iSymbol)
			{
				auto &Symbol = pSymbols[_iSymbol];

				return ELF64_ST_TYPE(Symbol.st_info) == STT_FUNC
					&& Symbol.st_shndx != SHN_UNDEF
					&& (!pVersions || (pVersions[_iSymbol] & 0x8000) == 0)
					&& strcmp(pStrings + Symbol.st_name, _pName) == 0
				;
			}
		;

		// Libraries without a GNU hash table have the hash table of System V, whose chain has an entry for each symbol
		if (!pGnuHash || !pGnuHash[0])
		{
			if (!pHash)
				return nullptr;

			ElfW(Word) nSymbols = pHash[1];
			for (ElfW(Word) iSymbol = 0; iSymbol < nSymbols; ++iSymbol)
			{
				if (fIsDefinedFunction(iSymbol))
					return reinterpret_cast<void *>(_Info.dlpi_addr + pSymbols[iSymbol].st_value);
			}

			return nullptr;
		}

		uint32 nBuckets = pGnuHash[0];
		uint32 iFirstSymbol = pGnuHash[1];
		uint32 nBloomWords = pGnuHash[2];
		auto *pBuckets = reinterpret_cast<uint32 const *>(reinterpret_cast<ElfW(Addr) const *>(pGnuHash + 4) + nBloomWords);
		auto *pChain = pBuckets + nBuckets;

		uint32 Hash = 5381;
		for (char const *pCharacter = _pName; *pCharacter; ++pCharacter)
			Hash = Hash * 33 + uint8(*pCharacter);

		uint32 iSymbol = pBuckets[Hash % nBuckets];
		if (iSymbol < iFirstSymbol)
			return nullptr;

		for (;; ++iSymbol)
		{
			uint32 ChainHash = pChain[iSymbol - iFirstSymbol];
			if ((ChainHash | 1) == (Hash | 1) && fIsDefinedFunction(iSymbol))
				return reinterpret_cast<void *>(_Info.dlpi_addr + pSymbols[iSymbol].st_value);

			if (ChainHash & 1)
				return nullptr;
		}
	}

	// The next definition of the function after the executable in the order that the libraries were loaded, which is what
	// RTLD_NEXT finds from the executable: one that a preloaded library interposes, or the one of libc, whatever its file is
	// named. Malterlib needs glibc 2.34 or later, where libc has the functions of libdl and libpthread
	[[nodiscard]] void *fg_FindRealDynamicLinkerFunction(char const *_pName)
	{
		struct CFind
		{
			char const *m_pName;
			void *m_pFunction = nullptr;
			bool m_bExecutable = true;
		};

		CFind Find{.m_pName = _pName};

		dl_iterate_phdr
			(
				[](dl_phdr_info *_pInfo, size_t, void *_pContext) -> int
				{
					auto &Find = *static_cast<CFind *>(_pContext);

					// The executable comes first, and has the functions that are looked for
					if (Find.m_bExecutable)
					{
						Find.m_bExecutable = false;
						return 0;
					}

					Find.m_pFunction = fg_FindDefinedFunction(*_pInfo, Find.m_pName);
					return Find.m_pFunction ? 1 : 0;
				}
				, &Find
			)
		;

		if (Find.m_pFunction)
			return Find.m_pFunction;

		// The process cannot look up symbols without them
		std::abort();
	}

	FDlsym *fg_GetRealDlsym()
	{
		auto *fReal = g_fRealDlsym.f_Load(NAtomic::gc_MemoryOrder_Relaxed);
		if (fReal)
			return fReal;

		fReal = reinterpret_cast<FDlsym *>(fg_FindRealDynamicLinkerFunction("dlsym"));
		g_fRealDlsym.f_Store(fReal, NAtomic::gc_MemoryOrder_Relaxed);
		return fReal;
	}

	FDlvsym *fg_GetRealDlvsym()
	{
		auto *fReal = g_fRealDlvsym.f_Load(NAtomic::gc_MemoryOrder_Relaxed);
		if (fReal)
			return fReal;

		fReal = reinterpret_cast<FDlvsym *>(fg_FindRealDynamicLinkerFunction("dlvsym"));
		g_fRealDlvsym.f_Store(fReal, NAtomic::gc_MemoryOrder_Relaxed);
		return fReal;
	}

	// RTLD_NEXT is how a library that interposes pthread_create finds the one it wraps, and RTLD_DEFAULT finds ours
	// already. A library that looks pthread_create up in a library of its own choosing gets ours, as if it had
	// imported it
	bool fg_IsPThreadCreateLookup(void *_pHandle, char const *_pName)
	{
		return _pHandle != RTLD_NEXT && _pHandle != RTLD_DEFAULT && _pName && strcmp(_pName, "pthread_create") == 0;
	}

	// Only what ours calls is replaced, which is the one of a library that interposes pthread_create, or the default
	// version of glibc. A library that interposes it, and finds the one of glibc in its library, would otherwise get ours,
	// which calls it, and the two would call each other. Older versions take attributes of other sizes, which glibc
	// converts. Nothing is replaced before Malterlib has found what ours calls
	void *fg_ReplacePThreadCreate(void *_pFound)
	{
		return _pFound && _pFound == reinterpret_cast<void *>(NLocal::g_f_pthread_create) ? reinterpret_cast<void *>(&pthread_create) : _pFound;
	}
}

// Libraries that look pthread_create up, as the drivers of NVIDIA do, would otherwise create threads that Malterlib
// does not know about, and that use its memory manager without thread locals. Everything else is passed on as a tail
// call, so the dynamic linker sees the address that the caller returns to, which RTLD_NEXT is resolved from
extern "C" assure_used module_export void *dlsym(void *__restrict _pHandle, char const *__restrict _pName) noexcept
{
	FDlsym *fReal = fg_GetRealDlsym();

	// The lookup is made, so that what dlerror reports is what it reports for a lookup that was not replaced
	if (fg_IsPThreadCreateLookup(_pHandle, _pName))
		return fg_ReplacePThreadCreate(fReal(_pHandle, _pName));

	[[clang::musttail]] return fReal(_pHandle, _pName);
}

// A lookup in a library of its own choosing does not depend on the caller, so it is made here
extern "C" assure_used module_export void *dlvsym(void *__restrict _pHandle, char const *__restrict _pName, char const *__restrict _pVersion) noexcept
{
	FDlvsym *fReal = fg_GetRealDlvsym();

	if (fg_IsPThreadCreateLookup(_pHandle, _pName))
		return fg_ReplacePThreadCreate(fReal(_pHandle, _pName, _pVersion));

	[[clang::musttail]] return fReal(_pHandle, _pName, _pVersion);
}
#endif

void DMibCrossmoduleAPI fg_ThreadNotificationRegister(NSys::NPrivate::CThreadNotificationModule *_pModule)
{
	if (g_bSysDeleted)
		return;
	DMibFastCheck(_pModule->m_Version >= NSys::NPrivate::EThreadNotificationCrossModule_Version_Min);

	DMibLock(g_ThreadNotificationLock);

	auto &State = *g_ThreadNotificationState;
	auto &Registration = State.m_Notifications[_pModule];
	DMibFastCheck(!Registration.m_Link.f_IsInList());
	State.m_NotificationOrder.f_Insert(Registration);

	umint CurrentThread = NSys::fg_Thread_GetCurrentUID();
	for (auto &Thread : State.m_LiveThreads)
	{
		if (Thread != CurrentThread)
			_pModule->m_fCreated(Thread, 0);
	}
}

void DMibCrossmoduleAPI fg_ThreadNotificationUnregister(NSys::NPrivate::CThreadNotificationModule *_pModule, void (*_fDestroyLocals)(void *), void *_pContext)
{
	DMibLock(g_ThreadNotificationLock);

	// Keep registered threads alive until their native TLS slots have been cleared.
	_fDestroyLocals(_pContext);

	auto &State = *g_ThreadNotificationState;
	auto pRegistration = State.m_Notifications.f_FindEqual(_pModule);
	if (!pRegistration)
		return;

	State.m_Notifications.f_Remove(_pModule);
}

void DMibCrossmoduleAPI fg_ThreadNotificationEnum(NSys::NPrivate::FThreadEnumCallback *_fThread, void *_pContext)
{
	DMibLock(g_ThreadNotificationLock);
	if (!g_bThreadNotificationsInitialized)
		return;

	umint CurrentThread = NSys::fg_Thread_GetCurrentUID();
	for (auto &Thread : g_ThreadNotificationState->m_LiveThreads)
	{
		if (Thread != CurrentThread)
			_fThread(Thread, _pContext);
	}
}

constinit NSys::NPrivate::CThreadNotificationCrossModule g_ThreadNotificationCrossModule =
	{
		.m_Version = NSys::NPrivate::EThreadNotificationCrossModule_Version
		, .m_Reserved = {}
		, .m_fRegister = &fg_ThreadNotificationRegister
		, .m_fUnregister = &fg_ThreadNotificationUnregister
		, .m_fEnum = &fg_ThreadNotificationEnum
	}
;

extern "C" assure_used module_export NSys::NPrivate::CThreadNotificationCrossModule *fg_MalterlibGetThreadNotificationCrossModule(uint32 _Version)
{
	if (_Version < NSys::NPrivate::EThreadNotificationCrossModule_Version_Min)
		return nullptr;

	// Malterlib executable initialization precedes every dependent Malterlib DSO
	// constructor. Returning this interface before the host has constructed its
	// dispatcher would violate the same ordering required by memory interposition.
	DMibFastCheck(g_bThreadNotificationsInitialized);
	return &g_ThreadNotificationCrossModule;
}

void fg_InitializePThreadNotifications()
{
	fg_InstallPThreadOverride();
}

void NSys::fg_Thread_DestroyLocalContext(void (*_fDestroy)())
{
	DMibLock(g_ThreadNotificationLock);
	_fDestroy();
	g_bThreadLocalContextDestroyed.f_Store(true, NAtomic::gc_MemoryOrder_Relaxed);
	fg_DestroyPThreadOverride();
}

void NSys::fg_Thread_EnumOtherThreadsInProcess(NFunction::TCFunctionNoAlloc<void (umint _ThreadID)> const &_fOnThread)
{
	fg_ThreadNotificationEnum
		(
			[](umint _ThreadID, void *_pContext)
			{
				(*reinterpret_cast<NFunction::TCFunctionNoAlloc<void (umint _ThreadID)> const *>(_pContext))(_ThreadID);
			}
			, const_cast<void *>(reinterpret_cast<void const *>(&_fOnThread))
		)
	;
}

#else

namespace
{
	NSys::NPrivate::CThreadNotificationCrossModule *g_pHostThreadNotificationCrossModule = nullptr;

	void fg_MalterlibThreadForkPrepareLocal()
	{
		g_ThreadCreationNotificationLock.f_Lock();
		fg_GetLocalSys()->f_ThreadLocal_PrepareFork();
	}

	void fg_MalterlibThreadForkParentLocal()
	{
		fg_GetLocalSys()->f_ThreadLocal_ForkedParent();
		g_ThreadCreationNotificationLock.f_Unlock();
	}

	void fg_MalterlibThreadForkChildLocal()
	{
		fg_GetLocalSys()->f_ThreadLocal_ForkedChild();
		g_ThreadCreationNotificationLock.f_ForkedChildLocked();
		g_ThreadCreationNotificationLock.f_Unlock();
	}

	constinit NSys::NPrivate::CThreadNotificationModule g_ThreadNotificationModule =
		{
			.m_Version = NSys::NPrivate::EThreadNotificationCrossModule_Version
			, .m_Reserved = {}
			, .m_fCreated = &fg_MalterlibThreadCreatedNotificationLocal
			, .m_fTerminated = &fg_MalterlibThreadTerminatedNotificationLocal
			, .m_fForkPrepare = &fg_MalterlibThreadForkPrepareLocal
			, .m_fForkParent = &fg_MalterlibThreadForkParentLocal
			, .m_fForkChild = &fg_MalterlibThreadForkChildLocal
		}
	;

	void fg_RegisterPThreadNotifications()
	{
		auto fGetInterface = reinterpret_cast<NSys::NPrivate::FGetThreadNotificationCrossModule *>(dlsym(RTLD_DEFAULT, "fg_MalterlibGetThreadNotificationCrossModule"));
		auto pInterface = fGetInterface ? fGetInterface(NSys::NPrivate::EThreadNotificationCrossModule_Version) : nullptr;
		DMibFastCheck(!pInterface || pInterface->m_Version >= NSys::NPrivate::EThreadNotificationCrossModule_Version_Min);
		if (!pInterface || pInterface->m_Version < NSys::NPrivate::EThreadNotificationCrossModule_Version_Min)
			return;

		g_pHostThreadNotificationCrossModule = pInterface;
		pInterface->m_fRegister(&g_ThreadNotificationModule);
	}
}

void fg_InitializePThreadNotifications()
{
	fg_RegisterPThreadNotifications();
}

void NSys::fg_Thread_DestroyLocalContext(void (*_fDestroy)())
{
	if (!g_pHostThreadNotificationCrossModule)
	{
		_fDestroy();
		g_bThreadLocalContextDestroyed.f_Store(true, NAtomic::gc_MemoryOrder_Relaxed);
		return;
	}

	g_pHostThreadNotificationCrossModule->m_fUnregister
		(
			&g_ThreadNotificationModule
			, [](void *_pContext) { (*static_cast<void (**)()>(_pContext))(); }
			, &_fDestroy
		)
	;
	g_bThreadLocalContextDestroyed.f_Store(true, NAtomic::gc_MemoryOrder_Relaxed);
	g_pHostThreadNotificationCrossModule = nullptr;
}

void NSys::fg_Thread_EnumOtherThreadsInProcess(NFunction::TCFunctionNoAlloc<void (umint _ThreadID)> const &_fOnThread)
{
	DMibFastCheck(g_pHostThreadNotificationCrossModule);
	if (!g_pHostThreadNotificationCrossModule)
		return;

	g_pHostThreadNotificationCrossModule->m_fEnum
		(
			[](umint _ThreadID, void *_pContext)
			{
				(*reinterpret_cast<NFunction::TCFunctionNoAlloc<void (umint _ThreadID)> const *>(_pContext))(_ThreadID);
			}
			, const_cast<void *>(reinterpret_cast<void const *>(&_fOnThread))
		)
	;
}

#endif

void fg_InitMalterlibAllEnumOtherThreads()
{
	umint ThisUID = NSys::fg_Thread_GetCurrentUID();
	NSys::fg_Thread_EnumOtherThreadsInProcess
		(
			[&](umint _ThreadID)
			{
				fg_GetLocalSys()->f_ThreadLocalCreateThread(_ThreadID, ThisUID);
			}
		)
	;
}

#endif

#ifdef DMibStaticThreadLocals

using namespace NMib;

#include <unistd.h>
#include <sys/types.h>

#include <Mib/Container/BitArrayHierarchical>

// *************************************************************************************************************************
// POSIX Thread Implementation
// *************************************************************************************************************************


constexpr umint gc_nMalterlibThreadLocals = 256; // 2 KB on 64 bit platforms

constinit NThread::CLowLevelLockAggregate gc_nMalterlibThreadLocalsAllocatedLock = {DAggregateInit};
constinit NContainer::TCBitArrayHierarchical<gc_nMalterlibThreadLocals> gc_nMalterlibThreadLocalsAllocated{};

extern "C" assure_used umint g_MalterlibLinuxThreadLocalsAreThreadPointerRelative = 1;

#ifdef DMibDynamicLibrary
	#ifndef DMibAssumeMalterlibHost
		__thread umint __attribute__((tls_model("initial-exec"))) g_MalterlibThreadLocals[256] = {0};
		__thread pid_t __attribute__((tls_model("initial-exec"))) g_MalterlibCurrentTID = 0;
	#endif
#else
__thread umint __attribute__((tls_model("local-exec"))) g_MalterlibThreadLocals[256] = {0};
__thread pid_t __attribute__((tls_model("local-exec"))) g_MalterlibCurrentTID = 0;
#endif

pid_t fg_Malterlib_Thread_GetTID_Local();

#if !defined(DMibDynamicLibrary)
extern "C" assure_used module_export umint fg_Malterlib_Thread_AllocLocal()
{
	return NSys::fg_Thread_AllocLocal();
}
extern "C" assure_used module_export void fg_Malterlib_Thread_FreeLocal(umint _iStorage)
{
	NSys::fg_Thread_FreeLocal(_iStorage);
}
extern "C" assure_used module_export pid_t fg_Malterlib_Thread_GetTID()
{
	return fg_Malterlib_Thread_GetTID_Local();
}
#endif

#if defined(DMibDynamicLibrary) && defined(DMibAssumeMalterlibHost)
extern "C" umint fg_Malterlib_Thread_AllocLocal();
extern "C" void fg_Malterlib_Thread_FreeLocal(umint _iStorage);
extern "C" pid_t fg_Malterlib_Thread_GetTID();

umint NSys::fg_Thread_AllocLocal()
{
	return fg_Malterlib_Thread_AllocLocal();
}

void NSys::fg_Thread_FreeLocal(umint _iStorage)
{
	return fg_Malterlib_Thread_FreeLocal(_iStorage);
}

pid_t fg_Malterlib_Thread_GetTID_Local()
{
	return fg_Malterlib_Thread_GetTID();
}

#else

pid_t fg_Malterlib_Thread_GetTID_Local()
{
	pid_t ThreadID = g_MalterlibCurrentTID;
	if (!ThreadID) [[unlikely]]
		g_MalterlibCurrentTID = ThreadID = syscall(SYS_gettid);
	return ThreadID;
}

umint NSys::fg_Thread_AllocLocal()
{
	aint iThreadLocal;
	{
		DMibLock(gc_nMalterlibThreadLocalsAllocatedLock);
		iThreadLocal = gc_nMalterlibThreadLocalsAllocated.f_FindFreeBitAndSet();
	}
	if (iThreadLocal < 0)
		DMibErrorSystemImp("Out of thread local indices");

	smint Offset = (smint)&g_MalterlibThreadLocals[iThreadLocal] - (smint)(NMib::NSys::fg_GetThreadSelf() DAddThreadSelfOffset);
	return (umint)Offset;
}

void NSys::fg_Thread_FreeLocal(umint _iStorage)
{
	smint StartOffset = (smint)&g_MalterlibThreadLocals[0] - (smint)(NMib::NSys::fg_GetThreadSelf() DAddThreadSelfOffset);
	smint iStorage = (_iStorage - StartOffset) / sizeof(umint);
	if (iStorage < 0 || iStorage >= gc_nMalterlibThreadLocals)
		DMibErrorSystemImp("Thread local index out of range");
	bool bAllocated;
	{
		DMibLock(gc_nMalterlibThreadLocalsAllocatedLock);
		bAllocated = gc_nMalterlibThreadLocalsAllocated.f_GetBit(iStorage);
		if (!bAllocated) [[unlikely]]
			DMibErrorSystemImp("Thread local index has not been allocated");
		gc_nMalterlibThreadLocalsAllocated.f_SetBit(iStorage, false);
	}
}
#endif

void NSys::fg_Thread_SetLocal(umint _iStorage, void *_pData)
{
	auto pAlloc = (void **)((uint8 *)NMib::NSys::fg_GetThreadSelf() DAddThreadSelfOffset + smint(_iStorage));
	*pAlloc = _pData;
}

void NSys::fg_Thread_SetLocal(umint _ThreadID, umint _iStorage, void *_pData)
{
	umint ThisThread = fg_Thread_GetCurrentUID();
	if (ThisThread == _ThreadID)
	{
		fg_Thread_SetLocal(_iStorage, _pData);
		return;
	}
	NAtomic::TCAtomic<umint> *pThreadLocal = (NAtomic::TCAtomic<umint> *)((uint8 *)_ThreadID DAddThreadSelfOffset + smint(_iStorage));
	pThreadLocal->f_Exchange((umint)_pData);
}

void *NSys::fg_Thread_GetLocal(umint _ThreadID, umint _iStorage)
{
	if (NSys::fg_Thread_GetCurrentUID() == _ThreadID)
		return fg_Thread_GetLocal(_iStorage);
	NAtomic::TCAtomic<umint> *pThreadLocal = (NAtomic::TCAtomic<umint> *)((uint8 *)_ThreadID DAddThreadSelfOffset + smint(_iStorage));
	return (void *)pThreadLocal->f_Load();
}

#else
__thread pid_t g_MalterlibCurrentTID = 0;
pid_t fg_Malterlib_Thread_GetTID_Local()
{
	pid_t ThreadID = g_MalterlibCurrentTID;
	if (!ThreadID) [[unlikely]]
		g_MalterlibCurrentTID = ThreadID = syscall(SYS_gettid);
	return ThreadID;
}
#endif
