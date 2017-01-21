// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

//#include "sal.h"

#ifdef DMibDebug
//	#define DTCPDelayEmulation
#endif

#ifdef DTCPDelayEmulation
	// These are vars so you can tweak them on the fly.
	bint bDTCPDelayEmulation = false;
	mint DTCPDelayEmulation_Rate = 200*1024;
	double DTCPDelayEmulation_MinDelay = 0.1;
	//#define DTCPDelayEmulation_Rate 200*1024
	//#define DTCPDelayEmulation_MinDelay 0.1
	#define DTCPDelayEmulation_MaxQueue (DTCPDelayEmulation_MinDelay * DTCPDelayEmulation_Rate)
#endif 

#include "Malterlib_Core_PlatformImp_MSVC_Net.cpp"

class CCPUUsageMonitorLink
{
public:
	DMibListLinkDS_Link(CCPUUsageMonitorLink, m_Link);	

	virtual ~CCPUUsageMonitorLink()
	{
	}
};

class CSystemWindowsMSVC : public CSystem
{
public:

	bint m_bDestroying;

	NMib::NAggregate::TCAggregate<CWindowsSocketContext, 64> m_SocketContext;

	class CFileChangeNoticationContext
	{
	public:

		CFileChangeNoticationContext()
		{
		}

		~CFileChangeNoticationContext()
		{
			m_FreeBundles.f_DeleteAll();
			m_FullBundles.f_DeleteAll();
		}

		class CNotificationBundle;
		class CNotification
		{
		public:
			CNotification()
			{
				m_pBundle = nullptr;
				m_Handle = INVALID_HANDLE_VALUE;
				m_pReportTo = nullptr;
				m_Flags = EFileChange_None;
				m_bDoneRead = false;
				m_bCancelled = false;
			}
			~CNotification()
			{
				f_Clear();
			}
			void f_Clear()
			{
				{
					DMibLock(m_pBundle->m_UpdateLock);
					m_bCancelled = true;
					if (m_bDoneRead)
					{
						{
							m_pBundle->m_ToCancel.f_Insert(this);
							m_pBundle->m_Event.f_Signal();
						}

						if (m_pBundle)
						{
							while (1)
							{
								{
									if (!m_bDoneRead)
										break;
								}
								{
									DMibUnlock(m_pBundle->m_UpdateLock);
									SleepEx(0, true);
								}
							}
						}
					}
					else
						m_LinkUpdate.f_Unlink();
				}

				if (m_Handle != INVALID_HANDLE_VALUE)
				{
					CloseHandle(m_Handle);
					m_Handle = INVALID_HANDLE_VALUE;
				}

				m_Changes.f_DeleteAll();
			}

			void f_Cancel()
			{
				CancelIo(m_Handle);
			}

			DMibListLinkDS_Link(CNotification, m_Link);
			DMibListLinkDS_Link(CNotification, m_LinkUpdate);
			HANDLE m_Handle;
			CNotificationBundle *m_pBundle;
			bint m_bDoneRead;
			bint m_bCancelled;

			NMib::NFile::EFileChange m_Flags;
			
			class CChange
			{
			public:
				CChange()
				{
					m_Notification = NFile::EFileChangeNotification_Undefined;
				}
				NFile::EFileChangeNotification m_Notification;
				CStr m_Path;
				DMibListLinkDS_Link(CChange, m_Link);
			};
			DMibListLinkDS_List(CChange, m_Link) m_Changes;
			NThread::CMutual m_ChangesLock;
			NThread::CEvent m_FirstReadDoneEvent;

			TCVector<uint8> m_ChangesBuffer;
			OVERLAPPED m_ChangesOverlapped;

			NMib::NThread::CSemaphoreReportableAggregate *m_pReportTo;

			static void __stdcall CompletionRoutine(DWORD _ErrorCode, DWORD _NumberOfBytesTransfered, LPOVERLAPPED _pOverlapped)
			{
				CNotification *pThis = (CNotification *)_pOverlapped->hEvent;

				DMibLock(pThis->m_pBundle->m_UpdateLock);

				if (_ErrorCode == 0)
				{
					if (_NumberOfBytesTransfered != 0)
					{
						FILE_NOTIFY_INFORMATION *pNotification = (FILE_NOTIFY_INFORMATION *)(pThis->m_ChangesBuffer.f_GetArray());

						while (pNotification)
						{

							CChange *pChange = nullptr;
							switch (pNotification->Action)
							{
							case FILE_ACTION_ADDED:
								{
									pChange = DMibNew CChange;
									pChange->m_Notification = EFileChangeNotification_Added;
								}
								break;
							case FILE_ACTION_REMOVED:
								{
									pChange = DMibNew CChange;
									pChange->m_Notification = EFileChangeNotification_Removed;
								}
								break;
							case FILE_ACTION_MODIFIED:
								{
									pChange = DMibNew CChange;
									pChange->m_Notification = EFileChangeNotification_Modified;
								}
								break;
							case FILE_ACTION_RENAMED_OLD_NAME:
								{
									pChange = DMibNew CChange;
									pChange->m_Notification = EFileChangeNotification_RenamedFrom;
								}
								break;
							case FILE_ACTION_RENAMED_NEW_NAME:
								{
									pChange = DMibNew CChange;
									pChange->m_Notification = EFileChangeNotification_RenamedTo;
								}
								break;
							}

							if (pChange)
							{
								
								pChange->m_Path = NFile::NPlatform::fg_ConvertFromWindowsPath(CWStr::fs_Create(pNotification->FileName, pNotification->FileNameLength/sizeof(pNotification->FileName[0])));

								DMibLock(pThis->m_ChangesLock);
								pThis->m_Changes.f_Insert(pChange);
							}
							
							if (pNotification->NextEntryOffset)
							{
								pNotification = (FILE_NOTIFY_INFORMATION *)((mint)pNotification + pNotification->NextEntryOffset);
							}
							else
								pNotification = nullptr;
						}
					}
					else
					{
						CChange *pChange = nullptr;
						pChange = DMibNew CChange;
						pChange->m_Notification = EFileChangeNotification_Unknown;

						{
							DMibLock(pThis->m_ChangesLock);
							pThis->m_Changes.f_Insert(pChange);
						}
					}
					
					if (pThis->m_pReportTo)
						pThis->m_pReportTo->f_Signal();
					pThis->f_DoRead();
				}
				else
					pThis->m_bDoneRead = false;

			}

			void f_DoRead()
			{
				if (m_bCancelled)
				{
					m_bDoneRead = false;
					return;
				}
				uint32 Flags = 0;
				if (m_Flags & NFile::EFileChange_FileName)
					Flags |= FILE_NOTIFY_CHANGE_FILE_NAME;
				if (m_Flags & NFile::EFileChange_DirectoryName)
					Flags |= FILE_NOTIFY_CHANGE_DIR_NAME;
				if (m_Flags & NFile::EFileChange_Attributes)
					Flags |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
				if (m_Flags & NFile::EFileChange_FileSize)
					Flags |= FILE_NOTIFY_CHANGE_SIZE;
				if (m_Flags & NFile::EFileChange_Write)
					Flags |= FILE_NOTIFY_CHANGE_LAST_WRITE;
				if (m_Flags & NFile::EFileChange_Security)
					Flags |= FILE_NOTIFY_CHANGE_SECURITY;

				m_ChangesBuffer.f_SetLen(64*1024);
				DWORD Dummy;
				NMem::fg_MemClear(m_ChangesOverlapped);
				m_ChangesOverlapped.hEvent = this;

				if (ReadDirectoryChangesW(m_Handle, m_ChangesBuffer.f_GetArray(), m_ChangesBuffer.f_GetLen(), (m_Flags & NFile::EFileChange_Recursive) != 0, Flags, &Dummy, &m_ChangesOverlapped, &CompletionRoutine))
					m_bDoneRead = true;
				else
				{
					m_bDoneRead = false;
				}
				m_FirstReadDoneEvent.f_SetSignaled();
			}
		};
		typedef DMibListLinkDS_Iter(CNotification, m_Link)  CNotificationIter;

		class CNotificationBundle : public NMib::NThread::CThread
		{
		public:
			virtual NStr::CStr f_GetThreadName()
			{
				return "Malterlib_Core_PlatformImp_FileChangeNot";
			}
			enum 
			{
				ENumNotifications = MAXIMUM_WAIT_OBJECTS - 1
			};

			CFileChangeNoticationContext *m_pContext;
			CNotificationBundle(CFileChangeNoticationContext *_pContext)
			{
				m_pContext = _pContext;
				for (mint i = 0; i < ENumNotifications; ++i)
				{
					m_Notifications[i].m_pBundle = this;
					m_Free.f_Insert(m_Notifications[i]);
				}
			}

			~CNotificationBundle()
			{
			}

			CNotification m_Notifications[ENumNotifications];
			DMibListLinkDS_List(CNotification, m_Link) m_Free;
			DMibListLinkDS_List(CNotification, m_Link) m_Used;

			NThread::CMutual m_UpdateLock;
			DMibListLinkDS_List(CNotification, m_LinkUpdate) m_ToRead;
			DMibListLinkDS_List(CNotification, m_LinkUpdate) m_ToCancel;

			DMibListLinkDS_Link(CNotificationBundle, m_Link);
			NThread::CEventAutoResetReportable m_Event;


			aint f_Main()
			{
				m_EventWantQuit.f_ReportTo(&m_Event);

				while (f_GetState() != NThread::EThreadState_EventWantQuit)
				{
					{
						CNotification *pPop;
						{
							DMibLock(m_UpdateLock);
							pPop = m_ToRead.f_Pop();
							while (pPop)
							{
								pPop->f_DoRead();
								{
									DMibUnlock(m_UpdateLock);
								}
								pPop = m_ToRead.f_Pop();
							}
						}
					}
					{
						CNotification *pPop;
						{
							DMibLock(m_UpdateLock);
							pPop = m_ToCancel.f_Pop();
							while (pPop)
							{
								pPop->f_Cancel();
								pPop = m_ToCancel.f_Pop();
							}
						}
					}

					WaitForSingleObjectEx(m_Event.m_pSemaphore, INFINITE, true);
				}

				return 0;
			}

		};

		DMibListLinkDS_List(CNotificationBundle, m_Link) m_FreeBundles;
		DMibListLinkDS_List(CNotificationBundle, m_Link) m_FullBundles;

		NThread::CMutual m_Lock;

		void *f_Open(const CStr &_FileName, NMib::NFile::EFileChange _OpenFlags, NMib::NThread::CSemaphoreReportableAggregate *_pReportTo)
		{

			CWStr WindowStr = NFile::NPlatform::fg_ConvertToWindowsPathLocal(_FileName);

			HANDLE Handle = CreateFile(WindowStr, FILE_LIST_DIRECTORY, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);

			if (Handle == INVALID_HANDLE_VALUE)
			{
				DMibErrorFile((CStr::CFormat("Windows returned an error from CreateFile({}): {}") << WindowStr << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
			}
			
			BY_HANDLE_FILE_INFORMATION Info;
			
			if (!GetFileInformationByHandle(Handle, &Info))
			{
				CloseHandle(Handle);
				DMibErrorFile((CStr::CFormat("Windows returned an error from GetFileInformationByHandle({}): {}") << WindowStr << NMib::NPlatform::fg_Win32_GetLastErrorStr()).f_GetStr());
			}

			if (!(Info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				CloseHandle(Handle);
				DMibErrorFile((CStr::CFormat("You can get notifications for file changes on directories, not files ({})") << WindowStr).f_GetStr());
			}

			CNotification *pNot;
			{
				DMibLock(m_Lock);

				CNotificationBundle *pBundle = m_FreeBundles.f_GetFirst();
				if (!pBundle)
				{
					pBundle = DMibNew CNotificationBundle(this);
					pBundle->f_Start();
				}
				pNot = pBundle->m_Free.f_Pop();
				pNot->m_Handle = Handle;
				pNot->m_pReportTo = _pReportTo;
				pNot->m_Flags = _OpenFlags;
				pBundle->m_Used.f_Insert(pNot);

				if (pBundle->m_Free.f_IsEmpty())
					m_FullBundles.f_Insert(pBundle);				

				{
					DMibLock(pBundle->m_UpdateLock);
					pBundle->m_ToRead.f_Insert(pNot);
				}

				pBundle->m_Event.f_Signal();
			}
			pNot->m_FirstReadDoneEvent.f_Wait();
			return pNot;
		}

		void f_Close(void *_pNotification)
		{
			DMibLock(m_Lock);
			CNotification *pNotification = (CNotification *)_pNotification;
			pNotification->f_Clear();
			CNotificationBundle *pBundle = pNotification->m_pBundle;
			pBundle->m_Free.f_Insert(pNotification);
			pBundle->m_Event.f_Signal();
			if (pBundle->m_Used.f_IsEmpty())
			{
				pBundle->m_Link.f_Unlink();
				{
					DMibUnlock(m_Lock);
					pBundle->f_Stop();
					delete pBundle;
				}
			}
			else
			{
				m_FreeBundles.f_Insert(pBundle);
			}
		}

		bint f_Changed(void *_pNotification)
		{
			DMibLock(m_Lock);
			CNotification *pNotification = (CNotification *)_pNotification;
			bint bChanged = false;
			{
				DMibLock(pNotification->m_ChangesLock);
				bChanged = !pNotification->m_Changes.f_IsEmpty();
				pNotification->m_Changes.f_DeleteAll();
			}
			return bChanged;
		}
		bint f_GetNotification(void *_pNotification, CStr &_Path, NFile::EFileChangeNotification &_Notification)
		{
			DMibLock(m_Lock);
			CNotification *pNotification = (CNotification *)_pNotification;
			bint bChanged = false;
			{
				DMibLock(pNotification->m_ChangesLock);
				CNotification::CChange *pChange = pNotification->m_Changes.f_Pop();					

				if (pChange)
				{
					_Path = pChange->m_Path;
					_Notification = pChange->m_Notification;
					bChanged = true;
					delete pChange;
				}
				else
					bChanged = false;
			}
			return bChanged;
		}

	};

	NMib::NAggregate::TCAggregate<CFileChangeNoticationContext, 64> m_FileChangeNoticationContext;


	bool f_EnablePrivilege(TCHAR* pszPrivilege, BOOL bEnable)
	{
		HANDLE           hToken;
		TOKEN_PRIVILEGES tp;
		BOOL             status;
		DWORD            error;

		// open process token
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
		{
			uint32 LastError = GetLastError();
			DMibTrace("OpenProcessToken failed: {}\n", NMib::NPlatform::fg_Win32_GetLastErrorStr(LastError));
			return false;
		}
		auto Cleanup 
			= fg_OnScopeExit
			(
				[&]()
				{
					// close the handle
					if (!CloseHandle(hToken))
					{
						uint32 LastError = GetLastError();
						DMibTrace("CloseHandle failed: {}\n", NMib::NPlatform::fg_Win32_GetLastErrorStr(LastError));
					}
				}
			)
		;

		// get the luid
		if (!LookupPrivilegeValue(NULL, pszPrivilege, &tp.Privileges[0].Luid))
		{
			uint32 LastError = GetLastError();
			DMibTrace("LookupPrivilegeValue failed: {}\n", NMib::NPlatform::fg_Win32_GetLastErrorStr(LastError));
			return false;
		}

		tp.PrivilegeCount = 1;

		// enable or disable privilege
		if (bEnable)
			tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		else
			tp.Privileges[0].Attributes = 0;

		// enable or disable privilege
		status = AdjustTokenPrivileges(hToken, FALSE, &tp, 0, (PTOKEN_PRIVILEGES)NULL, 0);

		// It is possible for AdjustTokenPrivileges to return TRUE and still not succeed.
		// So always check for the last error value.
		error = GetLastError();
		if (!status || (error != ERROR_SUCCESS))
		{
			uint32 LastError = error;
			DMibTrace("AdjustTokenPrivileges failed: {}\n", NMib::NPlatform::fg_Win32_GetLastErrorStr(LastError));
			return false;
		}

		return true;
	}


	CMutual m_LargeMemorySupportLock;
	zbint m_bEnabledLargeMemorySupport;
	zbint m_bTriedEnableLargeMemorySupport;
	bool f_EnableLargeMemorySupport()
	{
		if (m_bTriedEnableLargeMemorySupport)
			return m_bEnabledLargeMemorySupport;
		DMibLock(m_LargeMemorySupportLock);
		if (m_bTriedEnableLargeMemorySupport)
			return m_bEnabledLargeMemorySupport;

		m_bTriedEnableLargeMemorySupport = true;
		// Grant large page access
		m_bEnabledLargeMemorySupport = f_EnablePrivilege(SE_LOCK_MEMORY_NAME, true);
		return m_bEnabledLargeMemorySupport;
	}


	CMutual m_BackupSupportLock;
	zbint m_bEnabledBackupSupport;
	zbint m_bTriedEnableBackupSupport;
	bool f_EnableBackupSupport()
	{
		if (m_bTriedEnableBackupSupport)
			return m_bEnabledBackupSupport;
		DMibLock(m_BackupSupportLock);
		if (m_bTriedEnableBackupSupport)
			return m_bEnabledBackupSupport;

		m_bTriedEnableBackupSupport = true;
		// Grant large page access
		m_bEnabledBackupSupport = f_EnablePrivilege(SE_BACKUP_NAME, true);
		return m_bEnabledBackupSupport;
	}

	CMutual m_SymLinkSupportLock;
	zbint m_bEnabledSymLinkSupport;
	zbint m_bTriedEnableSymLinkSupport;
	bool f_EnableSymLinkSupport()
	{
		if (m_bTriedEnableSymLinkSupport)
			return m_bEnabledSymLinkSupport;
		DMibLock(m_SymLinkSupportLock);
		if (m_bTriedEnableSymLinkSupport)
			return m_bEnabledSymLinkSupport;

		m_bTriedEnableSymLinkSupport = true;
		// Grant large page access
		m_bEnabledSymLinkSupport = f_EnablePrivilege(SE_CREATE_SYMBOLIC_LINK_NAME, true);
		return m_bEnabledSymLinkSupport;
	}

	CStr m_ProgramPath_CStr;
	CStr m_ProgramDir_CStr;
	CStrNonTracked m_ProgramPath_CStrNonTracked;
	CStrNonTracked m_ProgramDir_CStrNonTracked;

	CMutual m_MemoryToucherLock;
	TCUniquePointer<CVirtualDestructor> m_pMemoryToucher;

	void f_UpdateProgramPath()
	{
		auto pPEB = fg_GetPEB(fg_GetTEB());

		CWStr BaseName;
		/*
		if (!GetModuleFileName(nullptr, BaseName.f_GetStr(65536), 65536))
		{
			DMibPDebugBreak;
		}*/

		BaseName.f_AddStr(pPEB->ProcessParameters->ImagePathName.Buffer, pPEB->ProcessParameters->ImagePathName.Length);

//		DConOut("BaseName: {}" DMibNewLine, BaseName);
		m_ProgramPath_CStr = NFile::NPlatform::fg_ConvertFromWindowsPath(BaseName); // Make sure unicode conversion is correct

		m_ProgramDir_CStr = NFile::CFile::fs_GetPath(m_ProgramPath_CStr);

		m_ProgramDir_CStrNonTracked = m_ProgramDir_CStr;
		m_ProgramPath_CStrNonTracked = m_ProgramPath_CStr;

		m_ProgramRoot = m_ProgramDir_CStr;
		m_ProgramRootNonTracked = m_ProgramRoot;
	}

	~CSystemWindowsMSVC()
	{
		m_ProgramDir_CStrNonTracked.f_Clear();
		m_ProgramPath_CStrNonTracked.f_Clear();
	}

	CSystemWindowsMSVC()
		: CSystem(g_bIsDll)
	{

		fg_MemClear(m_SocketContext);
		fg_MemClear(m_FileChangeNoticationContext);

		m_pSetAssertInfo = nullptr;

		fp_InitComplete();
	}

	void f_InitModule()
	{
		f_UpdateProgramPath();

		__super::f_InitModule();

		if (gs_LibraryRefCount.f_Load() == -1)
			f_InstallVistaExceptionHack();	
	}

	typedef void (FSetAssertInfo)(int32 _AssertType, const ch8 *_pAssertMessage);

	FSetAssertInfo *m_pSetAssertInfo;
	NThread::CMutual m_ContractInfoLock;
	CStrNonTracked m_ContractAssertInfo;
	static void fs_SetAssertInfo(int32 _AssertType, const ch8 *_pAssertMessage)
	{
		fg_GetLocalSys()->f_SetContractInfo(_pAssertMessage);
	}

	void f_SetContractInfo(const ch8 *_pAssertMessage)
	{
		CStrNonTracked AssertInfo; 
		AssertInfo.f_AddStr(_pAssertMessage);
		{
			DMibLock(m_ContractInfoLock);
			m_ContractAssertInfo = AssertInfo;
		}
	}

	CStrNonTracked f_DebugContractViolationMessage()
	{
		DMibLock(m_ContractInfoLock);
		return m_ContractAssertInfo;
	}

	void f_DebugReportContractViolation(const NStr::CStrNonTracked &_Message)
	{
		if (m_pSetAssertInfo)
			m_pSetAssertInfo(1, _Message);
	}

	void f_InitModuleThreaded()
	{
		__super::f_InitModuleThreaded();

		NSys::fg_Debug_EnableCrashDumps();

		if (!g_bIsDll)
		{
			// This needs to be named exactly like this to be compatibly with old versions of the library (when Malterlib was named Ids)
			if (!FindAtom(str_utf16("IdsAssertAtom")))
			{
				AddAtom(str_utf16("IdsAssertAtom"));
				m_pSetAssertInfo = &fs_SetAssertInfo;
				mint PausePointer = (mint)m_pSetAssertInfo;
				for (mint i = 0; i < sizeof(mint) * 8; ++i)
				{
					if (PausePointer & (mint(1) << i))
					{
						AddAtom(CFWStr128(CFWStr128::CFormat(str_utf16("IdsAssertAtom{}")) << i));
					}
				}
			}
			else
			{
				mint PausePointer = 0;
				for (mint i = 0; i < sizeof(mint) * 8; ++i)
				{
					if (FindAtom(CFWStr128(CFWStr128::CFormat(str_utf16("IdsAssertAtom{}")) << i)))
					{
						PausePointer |= (mint(1) << i);
						
					}
				}
				m_pSetAssertInfo = (FSetAssertInfo *)PausePointer;
			}

		}
		else
		{
			if (FindAtom(str_utf16("IdsAssertAtom")))
			{
				mint PausePointer = 0;
				for (mint i = 0; i < sizeof(mint) * 8; ++i)
				{
					if (FindAtom(CFWStr128(CFWStr128::CFormat(str_utf16("IdsAssertAtom{}")) << i)))
					{
						PausePointer |= (mint(1) << i);
						
					}
				}
				m_pSetAssertInfo = (FSetAssertInfo *)PausePointer;
			}
		}
	}

	void f_DestroyThreadSpecific()
	{
		CSystem::f_PreDestructThreadSpecific();

		m_pMemoryToucher.f_Clear();

		if (m_FileChangeNoticationContext.m_bConstructed)
			m_FileChangeNoticationContext.f_Destruct();
		if (m_SocketContext.m_bConstructed)
			m_SocketContext.f_Destruct();

		CSystem::f_DestructThreadSpecific();
	}

	void f_Destruct()
	{
		m_ProgramDir_CStr.f_Clear();
		m_ProgramPath_CStr.f_Clear();
		m_ContractAssertInfo.f_Clear();
		CSystem::f_Destruct();
		m_bDestroying = true;

		f_UninstallVistaExceptionHack();
	}


#ifdef _M_IX86

	class CExceptionHandler
	{
	public:
		CExceptionHandler *m_pNextHandler;
		void *m_pExceptionHandlingFunction;
	};

	static CExceptionHandler *RtlGetRegistrationHead()
	{
		CExceptionHandler * pRet = (CExceptionHandler * )(void *)(size_t)-1;
		__asm
		{
			mov eax, dword ptr fs:[0h]
			mov [pRet], eax
		}
		return pRet;
	}

#pragma warning(disable:4733)
	static void RtlSetRegistrationHead(CExceptionHandler *_pHandler)
	{
		__asm
		{
			mov eax, [_pHandler]		
			mov dword ptr fs:[0h], eax
		}
	}
#endif

	static LONG WINAPI VectoredHandler3(struct _EXCEPTION_POINTERS *pExceptionInfo)
	{
		if (pExceptionInfo->ExceptionRecord && pExceptionInfo->ExceptionRecord->NumberParameters >= 3 && pExceptionInfo->ExceptionRecord->ExceptionInformation[0] == 0x19930520)
		{
			if (g_ExceptionFilter.f_IsConstructed() && (*g_ExceptionFilter).f_IsValid())
			{
				TCAutoClear<CExceptionFilter *> &pPtr = (**g_ExceptionFilter);

				if (pPtr)
				{
					pPtr.f_Get()->f_Exception((void *)pExceptionInfo->ExceptionRecord->ExceptionInformation[1]);
				}		
			}
		}
		return 0;
	}

	static LONG WINAPI VectoredHandler2(struct _EXCEPTION_POINTERS *ExceptionInfo)
	{
		VectoredHandler3(ExceptionInfo);

		HMODULE pNtDll = NLocal::g_hNtDll;

		if (!pNtDll)
			return 0;
		
		// We can't get the address of KiUserCallbackDispatcherHandler without debug symbols, lets just assume it's between KiUserApcDispatcher and KiUserCallbackDispatcher
		void *pStartNtDll = NLocal::g_pKiUserApcDispatcher;
		void *pEndNtDll = NLocal::g_pKiUserCallbackDispatcher;
		if (!pStartNtDll || !pEndNtDll)
			return 0;

	#ifdef _M_X64


		{
			CONTEXT                       Context;
			KNONVOLATILE_CONTEXT_POINTERS NvContext;
			UNWIND_HISTORY_TABLE          UnwindHistoryTable;
			PRUNTIME_FUNCTION             RuntimeFunction;
			PVOID                         HandlerData;
			ULONG64                       EstablisherFrame;
			ULONG64                       ImageBase;

			RtlCaptureContext(&Context);

			RtlZeroMemory(&UnwindHistoryTable, sizeof(UNWIND_HISTORY_TABLE));

			for (ULONG Frame = 0;;Frame++)
			{
				//
				// Try to look up unwind metadata for the current function.
				//

				RuntimeFunction = RtlLookupFunctionEntry(Context.Rip, &ImageBase, &UnwindHistoryTable);

				RtlZeroMemory(&NvContext, sizeof(KNONVOLATILE_CONTEXT_POINTERS));

				if (!RuntimeFunction)
				{
					//
					// If we don't have a RUNTIME_FUNCTION, then we've encountered
					// a leaf function.  Adjust the stack approprately.
					//

					Context.Rip  = (ULONG64)(*(PULONG64)Context.Rsp);
					Context.Rsp += 8;
				}
				else
				{
					// Now lets patch the unwind info structure for the function that uses KiUserCallbackDispatcherHandler as it's exception handler (KiUserCallbackDispatcherContinue).
					UNWIND_INFO *pInfo = (UNWIND_INFO *)(ImageBase + RuntimeFunction->UnwindData);
					ULONG *pExceptionHandlerData = (ULONG *)&pInfo->UnwindCode[pInfo->CountOfCodes];
					void *pExceptionHandler = (void *)(ImageBase + *pExceptionHandlerData);
					//void *pExceptionData = (void *)(ImageBase + pExceptionHandlerData[1]);

					void *pAddress = (void *)((size_t)pInfo & (~size_t(4095)));
					void *pAddressHigh = (void *)(((size_t)pInfo + 4096)  & (~size_t(4095)));

					if (pExceptionHandler >= pStartNtDll && pExceptionHandler <= pEndNtDll)
					{
						MEMORY_BASIC_INFORMATION MemInfo;
						if (VirtualQuery(pAddress, &MemInfo, sizeof(MemInfo)))
						{
							DWORD OldProtect;
							if (MemInfo.Protect != PAGE_READWRITE)
							{
								// We need to change the protection of the page to read write because right now it's a read only portion of ntdll.dll. This seems to work both running as admin and as a normal user under user account control.
								if (VirtualProtect(pAddress, (size_t)pAddressHigh - (size_t)pAddress, PAGE_READWRITE, &OldProtect))
								{
									pInfo->Flags &= ~(UNW_FLAG_EHANDLER | UNW_FLAG_UHANDLER);
								}
							}
						}
					}

					//
					// Otherwise, call upon RtlVirtualUnwind to execute the unwind for
					// us.
					//

					RtlVirtualUnwind(
						UNW_FLAG_EHANDLER,
						ImageBase,
						Context.Rip,
						RuntimeFunction,
						&Context,
						&HandlerData,
						&EstablisherFrame,
						&NvContext);
				}

				//
				// If we reach an RIP of zero, this means that we've walked off the end
				// of the call stack and are done.
				//

				if (!Context.Rip)
					break;
			}

		}

	#else

		// Unlink the exception handler from the list of exception handlers. Is this really safe?
		CExceptionHandler *pExceptionHandler = RtlGetRegistrationHead();
		CExceptionHandler *pLastExceptionHandler = nullptr;
		aint iMaxRecurse = 1024; // Max 1024 recurse to guard against circular lists
		while ((size_t)pExceptionHandler != size_t(-1) && iMaxRecurse > 0)
		{
			--iMaxRecurse;
			if (pExceptionHandler->m_pExceptionHandlingFunction >= pStartNtDll && pExceptionHandler->m_pExceptionHandlingFunction <= pEndNtDll) 
			{
				if (pLastExceptionHandler)
				{
					// Unlink the stupid exception handler!
					pLastExceptionHandler->m_pNextHandler = pExceptionHandler->m_pNextHandler;
				}
				else
				{
					RtlSetRegistrationHead(pExceptionHandler->m_pNextHandler);
				}
			}
			pLastExceptionHandler = pExceptionHandler;
			pExceptionHandler = pExceptionHandler->m_pNextHandler;

		}
	#endif
		return 0;
	}


	void *m_pVistaExceptinoHackVectoredHandler;
	void f_InstallVistaExceptionHack()
	{
		m_pVistaExceptinoHackVectoredHandler = nullptr;
		if (NLocal::g_fAddVectoredExceptionHandler && NLocal::g_fGetNativeSystemInfo)
		{
			bint bInstalled = false;
			if (NLocal::g_fGetProcessUserModeExceptionPolicy)
			{
				DWORD dwFlags;
				if (NLocal::g_fGetProcessUserModeExceptionPolicy(&dwFlags)) 
				{
					dwFlags = dwFlags & ~PROCESS_CALLBACK_FILTER_ENABLED;
					NLocal::g_fSetProcessUserModeExceptionPolicy(dwFlags); 
				}
			}
			else if (NLocal::g_fGetNativeSystemInfo)
			{
				SYSTEM_INFO NativeSystemInfo;
				NLocal::g_fGetNativeSystemInfo(&NativeSystemInfo);
				// Check that we are running Vista SP1 or later
				if 
					(
						NLocal::g_VersionInfo.dwMajorVersion == 6 
						&& NLocal::g_VersionInfo.dwMinorVersion < 2 // Fixed in Windows 8
						&& 
						(
							NLocal::g_VersionInfo.wServicePackMajor >= 1 
							|| NLocal::g_VersionInfo.dwMinorVersion > 0 
						)
					)
				{
					// Check that we are running on AMD64
					if (NativeSystemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
					{
						// We need to add our hack
						m_pVistaExceptinoHackVectoredHandler = NLocal::g_fAddVectoredExceptionHandler(1,VectoredHandler2);
						bInstalled = true;
					}
				}
			}
			if (!bInstalled)
			{
				m_pVistaExceptinoHackVectoredHandler = NLocal::g_fAddVectoredExceptionHandler(1,VectoredHandler3);
			}
		}
	}
	void f_UninstallVistaExceptionHack()
	{
		if (m_pVistaExceptinoHackVectoredHandler)
		{
			if (NLocal::g_fRemoveVectoredExceptionHandler)
				NLocal::g_fRemoveVectoredExceptionHandler(m_pVistaExceptinoHackVectoredHandler);

			m_pVistaExceptinoHackVectoredHandler = nullptr;			
		}
	}

};

static inline_small CSystemWindowsMSVC *fg_GetLocalSys()
{
	return (CSystemWindowsMSVC *)fg_GetSys();
}


