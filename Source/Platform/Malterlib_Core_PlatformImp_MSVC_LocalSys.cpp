// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

//#include "sal.h"

static bool fg_DefaultCrashDumpUserNotify(	const NStr::CStrNonTracked &_CustomMessage,
	const NStr::CStrNonTracked &_ProgramName,
	const NStr::CStrNonTracked &_SupportEmail,
	const NStr::CStrNonTracked &_FileName,
	const NStr::CStrNonTracked &_FileNameDumpMini,
	const NStr::CStrNonTracked &_FileNameDump,
	bool _bAllowContinue);

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

class CMSVCSystemContext
{
public:
	virtual ~CMSVCSystemContext()
	{
	}
};

class CProcessLaunchLink
{
public:
	DMibListLinkDS_Link(CProcessLaunchLink, m_Link);	
};

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
	bint m_bVectoredUnhandled;

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
								
								pChange->m_Path = fg_ConvertFromWindowsPath(CWStr::fs_Create(pNotification->FileName, pNotification->FileNameLength/sizeof(pNotification->FileName[0])));

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

			CWStr WindowStr = fg_ConvertToWindowsPathLocal(_FileName);

			HANDLE Handle = CreateFile(WindowStr, FILE_LIST_DIRECTORY, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);

			if (Handle == INVALID_HANDLE_VALUE)
			{
				DMibErrorFile((CStr::CFormat("Windows returned an error from CreateFile({}): {}") << WindowStr << fg_Win32_GetLastErrorStr()).f_GetStr());
			}
			
			BY_HANDLE_FILE_INFORMATION Info;
			
			if (!GetFileInformationByHandle(Handle, &Info))
			{
				CloseHandle(Handle);
				DMibErrorFile((CStr::CFormat("Windows returned an error from GetFileInformationByHandle({}): {}") << WindowStr << fg_Win32_GetLastErrorStr()).f_GetStr());
			}

			if (!(Info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				CloseHandle(Handle);
				DMibErrorFile((CStr::CFormat("You can get notifications for file changes on directories, not files ({})") << WindowStr).f_GetStr());
			}

			{
				DMibLock(m_Lock);

				CNotificationBundle *pBundle = m_FreeBundles.f_GetFirst();
				if (!pBundle)
				{
					pBundle = DMibNew CNotificationBundle(this);
					pBundle->f_Start();
				}
				CNotification *pNot = pBundle->m_Free.f_Pop();
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
				return pNot;
			}
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

	enum
	{
		EWindowCache = 10
	};

	HWND m_CacheWindows[EWindowCache];

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
			DMibTrace("OpenProcessToken failed: {}\n", fg_Win32_GetLastErrorStr(LastError));
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
						DMibTrace("CloseHandle failed: {}\n", fg_Win32_GetLastErrorStr(LastError));
					}
				}
			)
		;

		// get the luid
		if (!LookupPrivilegeValue(NULL, pszPrivilege, &tp.Privileges[0].Luid))
		{
			uint32 LastError = GetLastError();
			DMibTrace("LookupPrivilegeValue failed: {}\n", fg_Win32_GetLastErrorStr(LastError));
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
			DMibTrace("AdjustTokenPrivileges failed: {}\n", fg_Win32_GetLastErrorStr(LastError));
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

	NStr::CStr m_SecurePasswordLocation;

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
		m_ProgramPath_CStr = fg_ConvertFromWindowsPath(BaseName); // Make sure unicode conversion is correct

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
		m_CrashDumpUserNotifyFormat_CustomMessage.f_Clear();
		m_CrashDumpUserNotifyFormat_CanContinueMessage.f_Clear();
		m_CrashDumpUserNotifyFormat_NoContinueMessage.f_Clear();
	}

	CSystemWindowsMSVC()
		: CSystem(g_bIsDll)
	{
		m_FileTimeBase = NTime::CTimeConvert::fs_CreateTime(1601, 1, 1, 0, 0, 0, 0);
		m_bVectoredUnhandled = true;
		m_pCrashDumpUserNotifyFunction = nullptr;
		m_pDeadlockNotifyFunction = nullptr;

		fg_MemClear(m_SocketContext);
		fg_MemClear(m_FileChangeNoticationContext);

		m_bPollCheckExceptionFilter = true;
		m_pDllNotificationCookie = nullptr;
		m_ExceptionFilterPoller.m_pSystem = this;
		m_pSetAssertInfo = nullptr;

//		m_LocalShared.f_Construct();

		WNDCLASSA WndClass;
		memset(&WndClass, 0, sizeof(WndClass));
		WndClass.lpszClassName = "MalterlibCrashDumpWindowCache";
		WndClass.lpfnWndProc = DefWindowProc;
		WndClass.hInstance = g_hDllInstance;
		RegisterClassA(&WndClass);

		for (mint i = 0; i < EWindowCache; ++i)
			m_CacheWindows[i] = nullptr;

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

	void f_DebugReportContractViolation(const NStr::CStrNonTracked &_Message)
	{
		if (m_pSetAssertInfo)
			m_pSetAssertInfo(1, _Message);
	}

	void f_InitModuleThreaded()
	{
		__super::f_InitModuleThreaded();

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

			m_pPrevExceptionFilter = SetUnhandledExceptionFilter(&fsp_UnhandledException);
			f_InstallExceptionFilterCallback(true);
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

			f_InstallExceptionFilterCallback(false);
		}
	}

	void f_DestroyThreadSpecific()
	{

		m_pMemoryToucher.f_Clear();
		NSys::fg_ProcessLaunch_CancelAll();

		{
			DMibLock(m_StdInReaderImpLock);
			DMibFastCheck(m_pStdInReaderImp.f_IsEmpty()); // Should have been deleted when the last std in reader was closed
			m_pStdInReaderImp.f_Clear();
		}


		if (m_FileChangeNoticationContext.m_bConstructed)
			m_FileChangeNoticationContext.f_Destruct();
		if (m_SocketContext.m_bConstructed)
			m_SocketContext.f_Destruct();

		m_ExceptionFilterPoller.f_Stop();

		CSystem::f_DestructThreadSpecific();
	}

	void f_PreDestroy()
	{
		for (mint i = 0; i < EWindowCache; ++i)
		{
			if (m_CacheWindows[i])
			{
				DestroyWindow(m_CacheWindows[i]);
				m_CacheWindows[i] = nullptr;
			}
		}
		UnregisterClassA("MalterlibCrashDumpWindowCache", g_hDllInstance);
	}
	NThread::CMutual m_LaunchesLock;
	DMibListLinkDS_List(CProcessLaunchLink, m_Link) m_Launches;

	NThread::CMutual m_StdInReaderImpLock;
	TCUniquePointer<CMSVCSystemContext> m_pStdInReaderImp;

	void f_Destruct()
	{
		{
			DMibLock(m_LaunchesLock);
			DMibFastCheck(m_Launches.f_IsEmpty());
		}

		{
			DMibLock(m_StdInReaderImpLock);
			DMibFastCheck(m_pStdInReaderImp.f_IsEmpty());
		}

		f_UninstallExceptionFilterCallback();
		m_bVectoredUnhandled = true;
		m_ProgramDir_CStr.f_Clear();
		m_ProgramPath_CStr.f_Clear();
		m_ContractAssertInfo.f_Clear();
		m_SecurePasswordLocation.f_Clear();
		//		m_LocalShared.f_Destruct();
		CSystem::f_Destruct();
		m_bDestroying = true;

		f_UninstallVistaExceptionHack();
	}


	CStrNonTracked f_DumpObjects()
	{

		typedef BOOL (WINAPI fEnumProcesses)(DWORD * lpidProcess, DWORD   cb, DWORD * cbNeeded);
		typedef DWORD (WINAPI fGetModuleBaseNameW)(HANDLE hProcess, HMODULE hModule, LPWSTR lpBaseName, DWORD nSize);
		typedef DWORD (WINAPI fGetModuleFileNameExW)(HANDLE hProcess, HMODULE hModule, LPWSTR lpFilename, DWORD nSize);
		typedef DWORD (WINAPI fGetProcessImageFileNameW)(HANDLE hProcess, LPWSTR lpImageFileName, DWORD nSize);


		HMODULE hPSAPI = LoadLibrary(str_utf16("psapi.dll"));
		if (hPSAPI)
		{
			fEnumProcesses *pEnumProcesses = (fEnumProcesses *)GetProcAddress(hPSAPI, "EnumProcesses");
			fGetModuleBaseNameW *pGetModuleBaseName = (fGetModuleBaseNameW *)GetProcAddress(hPSAPI, "GetModuleBaseNameW");
			fGetModuleFileNameExW *pGetModuleFileNameEx = (fGetModuleFileNameExW *)GetProcAddress(hPSAPI, "GetModuleFileNameExW");
			fGetProcessImageFileNameW *pGetProcessImageFileName = (fGetProcessImageFileNameW *)GetProcAddress(hPSAPI, "GetProcessImageFileNameW");
			
			if (pEnumProcesses)
			{
				CStrNonTracked Ret = "\r\n\r\nDump of GDI and User Objects\r\n\r\n";
				const ch8 * pFormatStr = "{sj128,a-}{sj18,a-}{sj18,a-}\r\n";
				Ret += CStrNonTracked::CFormat(pFormatStr) << "Process" << "GDI Objects" << "User Objects";
				TCVector<DWORD, CAllocator_NonTrackedHeap> ProcessIDs;

				ProcessIDs.f_SetLen(65536);

				uint32 nTotalGDI = 0;
				uint32 nTotalUser = 0;

				DWORD nEnum;
				if (pEnumProcesses(ProcessIDs.f_GetArray(), 65536, &nEnum))
				{
					for (mint i = 0; i < nEnum; ++i)
					{
						HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, false, ProcessIDs[i]);
						if (hProcess)
						{
							uint32 nGDI = GetGuiResources(hProcess, GR_GDIOBJECTS);
							uint32 nUser = GetGuiResources(hProcess, GR_USEROBJECTS);

							nTotalGDI += nGDI;
							nTotalUser += nUser;

							CWStrNonTracked BaseName;
#if 1
							if (pGetProcessImageFileName)
								pGetProcessImageFileName(hProcess, BaseName.f_GetStr(1024), 1024);
							else 
#endif
								if (pGetModuleFileNameEx)
								pGetModuleFileNameEx(hProcess, nullptr, BaseName.f_GetStr(1024), 1024);
							else if (pGetModuleBaseName)
								pGetModuleBaseName(hProcess, nullptr, BaseName.f_GetStr(1024), 1024);

							Ret += CStrNonTracked::CFormat(pFormatStr) << BaseName << nGDI << nUser;

							CloseHandle(hProcess);
						}
					}
				}
				FreeLibrary(hPSAPI);
				Ret += "\r\n";
				Ret += CStrNonTracked::CFormat(pFormatStr) << "Total" << nTotalGDI << nTotalUser;
				Ret += "\r\n";
				return Ret;
			}
			FreeLibrary(hPSAPI);
		}

		return "";
	}

	CStrNonTracked f_DumpModules()
	{
		HANDLE hProcess = GetCurrentProcess();
		TCVector<HMODULE, CAllocator_NonTrackedHeap> Modules;
		Modules;
		DWORD NeededBytes = 0;
		EnumProcessModules(hProcess, 0, 0, &NeededBytes);
		Modules.f_SetLen((NeededBytes * 2) / sizeof(HMODULE));
		CStrNonTracked Ret = "\r\n\r\nDump of Process modules\r\n\r\n";
		const ch8 * pFormatStr = "{sj128,a-}{sj19,a-}{sj19,a-}{sj19,a-}\r\n";
		Ret += CStrNonTracked::CFormat(pFormatStr) << "Module" << "Start" << "End" << "Size";
		if (EnumProcessModules(hProcess, Modules.f_GetArray(), Modules.f_GetLen() * sizeof(HMODULE), &NeededBytes))
		{
			mint nModules = NeededBytes / sizeof(HMODULE);
			for (mint i = 0; i < nModules; ++i)
			{
				HMODULE hModule = Modules[i];
				CWStrNonTracked ModuleName;
				if (GetModuleFileNameEx(hProcess, hModule, ModuleName.f_GetStr(1024), 1024))
				{
					ModuleName.f_SetModified();
				}
				else
					ModuleName.f_Clear();

				MODULEINFO ModuleInfo;

				if (GetModuleInformation(hProcess, hModule, &ModuleInfo, sizeof(ModuleInfo)))
				{
					Ret 
						+= CStrNonTracked::CFormat(pFormatStr) 
						<< ModuleName 
						<< CFStr64(CFStr64::CFormat("0x{}") << ModuleInfo.lpBaseOfDll)
						<< CFStr64(CFStr64::CFormat("0x{}") << (void *)((mint)ModuleInfo.lpBaseOfDll + ModuleInfo.SizeOfImage))
						<< CFStr64(CFStr64::CFormat("{}") << ModuleInfo.SizeOfImage)
					;
				}
			}
		}

		Ret += "\r\n";
		return Ret;
	}

	LPTOP_LEVEL_EXCEPTION_FILTER m_pPrevExceptionFilter;

	template <typename tf_CStr>
	static tf_CStr fs_FormatTimeFileName(const NTime::CTime &_Time)
	{
		NTime::CTimeConvert::CDateTime DateTime;
		NTime::CTimeConvert(_Time).f_ExtractDateTime(DateTime);

		return tf_CStr::CFormat("{}-{sj2,sf0}-{sj2,sf0}_{sj2,sf0}.{sj2,sf0}.{sj2,sf0}.{sj3,sf0,fe3}") << DateTime.m_Year << DateTime.m_Month << DateTime.m_DayOfMonth << DateTime.m_Hour << DateTime.m_Minute << DateTime.m_Second << DateTime.m_Fraction * 1000.0;
	}

	NThread::CMutual m_RandomLock;
	CAutoRandom m_Random;

	uint32 f_GetRandom()
	{
		DMibLock(m_RandomLock);
		return m_Random.f_GetValue<uint32>();
	}

	template <typename tf_CStr>
	static bint fs_CheckAccessRights(tf_CStr &_Path)
	{
		try 
		{
			auto pLocalSys = fg_GetLocalSys();
			uint32 RandomValue = pLocalSys->f_GetRandom();
			tf_CStr GUID = tf_CStr::CFormat("TestAccessRights.{}.{}") << RandomValue << fs_FormatTimeFileName<tf_CStr>(NTime::CTime::fs_NowUTC());
			NFile::CFile::fs_CreateDirectory(_Path);
			NFile::CFile::fs_CreateDirectory(_Path + "/" + GUID);
			NFile::CFile::fs_DeleteDirectory(_Path + "/" + GUID);

			tf_CStr FileName = _Path + "/" + GUID + ".file";
			{
				NFile::CFile File;
				File.f_Open(FileName, EFileOpen_Write | EFileOpen_NoLocalCache | EFileOpen_ShareAll);

				uint32 Test = 1;
				File.f_Write(&Test, sizeof(Test));
			}
			NFile::CFile::fs_DeleteFile(FileName);

			return true;

		}
		catch (NFile::CExceptionFile)
		{
			return false;
		}
	}			
	

	static LONG WINAPI fsp_GenerateCrashDumpHandler(struct _EXCEPTION_POINTERS *_pExceptionInfo, const CStr &_Message, const CStr &_ExtraLog)
	{

	}

	class CExceptionData
	{
	public:
		bint m_bDisplayGUI;
		NContainer::TCVector<NMib::NStr::CStr> &m_GeneratedLogs;
		CExceptionData(NContainer::TCVector<NMib::NStr::CStr> &_GeneratedLogs, bint _bDisplayGUI) 
			: m_GeneratedLogs(_GeneratedLogs)
			, m_bDisplayGUI(_bDisplayGUI)
		{
		}
		CStr m_Message;
		CStr m_ExtraLog;
	};

	static LONG fsp_ExceptionGenerateHandler(struct _EXCEPTION_POINTERS *_pExceptionInfo, void *_pData)
	{
		CExceptionData *pData = (CExceptionData *)_pData;
		fsp_DumpExceptionInformation(_pExceptionInfo, pData->m_Message, pData->m_ExtraLog, &pData->m_GeneratedLogs, pData->m_bDisplayGUI);

		return EXCEPTION_EXECUTE_HANDLER;
	}

	void f_EnableCrashDumpCaches()
	{
		if (!g_bIsDll)
		{
			for (mint i = 0; i < EWindowCache; ++i)
			{
				if (!m_CacheWindows[i])
					m_CacheWindows[i] = CreateWindowA("MalterlibCrashDumpWindowCache", "MalterlibCrashDumpWindowCache", 0, 0, 0, 0, 0, HWND_MESSAGE, 0, 0, 0);
			}
		}
	}

	void f_GenerateCrashDump(const CStr &_Message, const CStr &_ExtraLog, TCVector<NMib::NStr::CStr> &_GeneratedLogs, bint _bDisplayGUI)
	{

		CExceptionData Data(_GeneratedLogs, _bDisplayGUI);
		Data.m_Message = _Message;
		Data.m_ExtraLog = _ExtraLog;

		fg_GenerateExcetionHandler(&Data, &fsp_ExceptionGenerateHandler);
	}

	class CExceptionMemoryData
	{
	public:
		NMib::NContainer::TCVector<void*, NMem::CAllocator_NonTrackedHeap> const& m_Locations;
		NMib::NContainer::TCVector<mint, NMib::NMem::CAllocator_NonTrackedHeap> const& m_Sizes;
		mint m_iCurrentLocation;

		CExceptionMemoryData
			(
				NMib::NContainer::TCVector<void*, NMem::CAllocator_NonTrackedHeap> const& _Locations
				, NMib::NContainer::TCVector<mint, NMib::NMem::CAllocator_NonTrackedHeap> const& _Sizes
			) 
			: m_Locations(_Locations)
			, m_Sizes(_Sizes)
			, m_iCurrentLocation(0)
		{
		}
	};

	static LONG fsp_ExceptionGenerateHandlerMemory(struct _EXCEPTION_POINTERS *_pExceptionInfo, void *_pData)
	{
		CExceptionMemoryData *pData = (CExceptionMemoryData *)_pData;
		fsp_DumpExceptionMemory(_pExceptionInfo, pData);

		return EXCEPTION_EXECUTE_HANDLER;
	}

	void f_GenerateMemoryDump
		(
			NMib::NContainer::TCVector<void*, NMem::CAllocator_NonTrackedHeap> const& _Locations
			, NMib::NContainer::TCVector<mint, NMib::NMem::CAllocator_NonTrackedHeap> const& _Sizes
		)
	{
		CExceptionMemoryData Data(_Locations, _Sizes);
		fg_GenerateExcetionHandler(&Data, &fsp_ExceptionGenerateHandlerMemory);
	}


	NSys::FCrashDumpUserNotify *m_pCrashDumpUserNotifyFunction;
	
	void f_SetCrashDumpUserNotifyFunction(NSys::FCrashDumpUserNotify *_pCrashDumpUserNotify)
	{
		m_pCrashDumpUserNotifyFunction = _pCrashDumpUserNotify;
	}

	CStrNonTracked m_CrashDumpUserNotifyFormat_CustomMessage;
	CStrNonTracked m_CrashDumpUserNotifyFormat_CanContinueMessage;
	CStrNonTracked m_CrashDumpUserNotifyFormat_NoContinueMessage;

	void f_SetCrashDumpUserNotifyFormats( const CStrNonTracked &_CustomMessage, const CStrNonTracked &_CanContinueMessage, const CStrNonTracked &_NoContinueMessage)
	{
		m_CrashDumpUserNotifyFormat_CustomMessage = _CustomMessage;
		m_CrashDumpUserNotifyFormat_CanContinueMessage = _CanContinueMessage;
		m_CrashDumpUserNotifyFormat_NoContinueMessage = _NoContinueMessage;
	}

	static LONG WINAPI fsp_UnhandledException(struct _EXCEPTION_POINTERS *_pExceptionInfo)
	{
		return fsp_DumpExceptionInformation(_pExceptionInfo, CStr(), CStr(), nullptr, true);
	}

	static CStr fs_FixLineEndings(CStr const &_In)
	{
		const ch8 *pParse = _In;
		CStr Ret;
		while (*pParse)
		{
			if (*pParse == '\n')
			{
				Ret.f_AddStr("\r\n");
				++pParse;
				continue;
			}
			else if (*pParse == '\r')
			{
				Ret.f_AddStr("\r\n");
				++pParse;
				if (*pParse == '\n')
					++pParse;
				continue;
			}
			else
				Ret.f_AddChar(*pParse);

			++pParse;
		}
		return Ret;
	}

	NThread::CMutual m_DumpExceptionInfoLock;

	static LONG WINAPI fsp_DumpExceptionInformation(struct _EXCEPTION_POINTERS *_pExceptionInfo, const CStr &_Message, const CStr &_ExtraLog, TCVector<NMib::NStr::CStr> *_pGeneratedLogs, bint _bDisplayGUI)
	{
		DMibDeadlockDetectorPause;

		CSystemWindowsMSVC *pLocalSys = fg_GetLocalSys();
		DMibLock(pLocalSys->m_DumpExceptionInfoLock);

		auto fl_GenerateException
			= [&_Message, &_ExtraLog, _pExceptionInfo, _pGeneratedLogs, _bDisplayGUI] (CThreadObjectNonTracked *_pThread) -> aint
			{
				mint nCache = CSystemWindowsMSVC::EWindowCache;
				CSystemWindowsMSVC *pLocalSys = fg_GetLocalSys();

				CStrNonTracked CrashDumpPath = fg_ConvertFromWindowsPath<CWStrNonTracked, CWStrNonTracked>(NSys::fg_Process_GetEnvironmentVariable(CStrNonTracked("MalterlibCrashDumpDir")));
				if (CrashDumpPath.f_IsEmpty() || !CSystemWindowsMSVC::fs_CheckAccessRights(CrashDumpPath))
				{
					CrashDumpPath = NMib::NFile::CFile::fs_AppendPath(NMib::fg_GetSys()->f_GetProgramRootNonTracked(), CStrNonTracked("CrashDumps"));
					if (!CSystemWindowsMSVC::fs_CheckAccessRights(CrashDumpPath))
					{
						CrashDumpPath = NMib::NFile::CFile::fs_AppendPath(NFile::CFile::fs_GetProgramDirectoryNonTracked(), CStrNonTracked("CrashDumps"));
						if (!CSystemWindowsMSVC::fs_CheckAccessRights(CrashDumpPath))
						{
							CrashDumpPath = NMib::NFile::CFile::fs_AppendPath(NFile::CFile::fs_GetUserLocalProgramDirectoryNonTracked(), CStrNonTracked("CrashDumps"));
							if (!CSystemWindowsMSVC::fs_CheckAccessRights(CrashDumpPath))
							{
								return EXCEPTION_CONTINUE_SEARCH;
							}
						}
					}
				}

				CStrNonTracked FileName;
				CStrNonTracked FileNameDumpMini;
				CStrNonTracked FileNameDump;

				{
					NTime::CTimeConvert::CDateTime DateTime;
					NTime::CTimeConvert(NTime::CTime::fs_NowLocal()).f_ExtractDateTime(DateTime);

					int32 Fraction = (DateTime.m_Fraction*1000.0).f_ToIntRound();
					if (Fraction >= 1000)
						Fraction = 999;

					uint32 RandomValue = pLocalSys->f_GetRandom();

					FileName = CrashDumpPath + (CStrNonTracked::CFormat("/CrashLog_{}-{sj2,sf0}-{sj2,sf0}_{sj2,sf0}.{sj2,sf0}.{sj2,sf0}.{sj3,sf0}.{sj8,sf0,nfh}.txt")
						<< DateTime.m_Year << DateTime.m_Month << DateTime.m_DayOfMonth << DateTime.m_Hour << DateTime.m_Minute << DateTime.m_Second << Fraction << RandomValue).f_GetStr();
					FileNameDump = CrashDumpPath + (CStrNonTracked::CFormat("/FullDump_{}-{sj2,sf0}-{sj2,sf0}_{sj2,sf0}.{sj2,sf0}.{sj2,sf0}.{sj3,sf0}.{sj8,sf0,nfh}.dmp")
						<< DateTime.m_Year << DateTime.m_Month << DateTime.m_DayOfMonth << DateTime.m_Hour << DateTime.m_Minute << DateTime.m_Second << Fraction << RandomValue).f_GetStr();
					FileNameDumpMini = CrashDumpPath + (CStrNonTracked::CFormat("/MiniDump_{}-{sj2,sf0}-{sj2,sf0}_{sj2,sf0}.{sj2,sf0}.{sj2,sf0}.{sj3,sf0}.{sj8,sf0,nfh}.dmp")
						<< DateTime.m_Year << DateTime.m_Month << DateTime.m_DayOfMonth << DateTime.m_Hour << DateTime.m_Minute << DateTime.m_Second << Fraction << RandomValue).f_GetStr();
				}
				// Mini dump
				CStrNonTracked ExceptionInfo;
				if (!_Message.f_IsEmpty())
				{
					ExceptionInfo += "\r\n";
					ExceptionInfo += _Message;
					ExceptionInfo += "\r\n\r\n";
				}
				{
					CStrNonTracked StackTraceError;
					bint bRet;
					if (_pThread)
						bRet = pLocalSys->m_StackTrace.f_Init(StackTraceError);
					else
					{
						CFStr256 Info;
						bRet = pLocalSys->m_StackTrace.f_InitDll(Info);
						StackTraceError = Info;
					}
					
					if (bRet)
					{
						if (pLocalSys->m_StackTrace.MiniDumpWriteDump)
						{
							MINIDUMP_EXCEPTION_INFORMATION Info;
							Info.ClientPointers = false;
							Info.ExceptionPointers = _pExceptionInfo;
							Info.ThreadId = GetCurrentThreadId();
							CWin32File *pFile = (CWin32File *)NSys::NFile::fg_Open(FileNameDumpMini, NFile::EFileOpen_Write);
							if (pFile)
							{
								MINIDUMP_TYPE DumpType = (MINIDUMP_TYPE )(MiniDumpWithHandleData | MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithProcessThreadData);
								if (!pLocalSys->m_StackTrace.MiniDumpWriteDump(pLocalSys->m_StackTrace.m_hProcess, GetCurrentProcessId(), pFile->m_pFile, DumpType, &Info, nullptr, nullptr))
								{
									CFStr256 ErrorStr = CFStr256::CFormat("Could not write mini dump. The error was: {}") << fg_Win32_GetLastErrorStr(GetLastError());
									DMibDTrace("{}\n", ErrorStr);
									ExceptionInfo += ErrorStr + "\r\n\r\n";
									FileNameDumpMini.f_Clear();
								}
								else if (_pGeneratedLogs)
									_pGeneratedLogs->f_Insert(FileNameDumpMini);
								NSys::NFile::fg_Close(pFile);
							}
							pFile = (CWin32File *)NSys::NFile::fg_Open(FileNameDump, NFile::EFileOpen_Write);
							if (pFile)
							{
								MINIDUMP_TYPE DumpType = (MINIDUMP_TYPE)(MiniDumpWithDataSegs | MiniDumpWithFullMemory | MiniDumpWithHandleData | MiniDumpWithUnloadedModules
									| MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithProcessThreadData | MiniDumpWithPrivateReadWriteMemory);

								if (!pLocalSys->m_StackTrace.MiniDumpWriteDump(pLocalSys->m_StackTrace.m_hProcess, GetCurrentProcessId(), pFile->m_pFile, DumpType, &Info, nullptr, nullptr))
								{
									CFStr256 ErrorStr = CFStr256::CFormat("Could not write full dump. The error was: {}") << fg_Win32_GetLastErrorStr(GetLastError());
									DMibDTrace("{}\n", ErrorStr);
									ExceptionInfo += ErrorStr + "\r\n\r\n";
									FileNameDump.f_Clear();
								}
								else if (_pGeneratedLogs)
									_pGeneratedLogs->f_Insert(FileNameDump);
								NSys::NFile::fg_Close(pFile);
							}
						}
					}
					else
					{
						CFStr256 ErrorStr = CFStr256::CFormat("Could not initialize debug help context. The error was: {}") << StackTraceError;
						DMibDTrace("{}\n", ErrorStr);
						ExceptionInfo += ErrorStr + "\r\n\r\n";
				
					}
				} 

				ExceptionInfo += "Unhandled exception\r\n\r\n";

				// 
				// Type
				//
				CStrNonTracked Code;
				switch (_pExceptionInfo->ExceptionRecord->ExceptionCode)
				{		
				case EXCEPTION_ACCESS_VIOLATION:
					{
						if (_pExceptionInfo->ExceptionRecord->ExceptionInformation[0])
							Code = CStrNonTracked::CFormat("Access violation trying to write to address: 0x{nfh,sf0,sj*}") << 
							((mint)_pExceptionInfo->ExceptionRecord->ExceptionInformation[1]) << (sizeof(mint) * 2)
							;
						else
							Code = CStrNonTracked::CFormat("Access violation trying to read to address: 0x{nfh,sf0,sj*}") <<
							((mint)_pExceptionInfo->ExceptionRecord->ExceptionInformation[1]) << (sizeof(mint) * 2)
							;

					}
					break;
				case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: Code = "Array bounds exceeded";break;
				case EXCEPTION_BREAKPOINT: Code = "Breakpoint";break;
				case EXCEPTION_DATATYPE_MISALIGNMENT: Code = "Datatype misalignment";break;
				case EXCEPTION_FLT_DENORMAL_OPERAND: Code = "Float denormal operand";break;
				case EXCEPTION_FLT_DIVIDE_BY_ZERO: Code = "Float divide by zero";break;
				case EXCEPTION_FLT_INEXACT_RESULT: Code = "Float inexact result";break;
				case EXCEPTION_FLT_INVALID_OPERATION: Code = "Float invalid operation";break;
				case EXCEPTION_FLT_OVERFLOW: Code = "Float overflow";break;
				case EXCEPTION_FLT_STACK_CHECK: Code = "Float stack check";break;
				case EXCEPTION_FLT_UNDERFLOW: Code = "Float underflow";break;
				case EXCEPTION_ILLEGAL_INSTRUCTION: Code = "Illegal instruction";break;
				case EXCEPTION_IN_PAGE_ERROR: Code = "In page error";break;
				case EXCEPTION_INT_DIVIDE_BY_ZERO: Code = "Integer divide by zero";break;
				case EXCEPTION_INT_OVERFLOW: Code = "Integer overflow";break;
				case EXCEPTION_INVALID_DISPOSITION: Code = "Invalid disposition";break;
				case EXCEPTION_NONCONTINUABLE_EXCEPTION: Code = "Noncontinuable exception";break;
				case EXCEPTION_PRIV_INSTRUCTION: Code = "Priviledged instruction";break;
				case EXCEPTION_SINGLE_STEP: Code = "Single step";break;
				case EXCEPTION_STACK_OVERFLOW: Code = "Stack overflow";break;
				default:
					{
						if (_pExceptionInfo->ExceptionRecord && _pExceptionInfo->ExceptionRecord->NumberParameters >= 3 && _pExceptionInfo->ExceptionRecord->ExceptionInformation[0] == 0x19930520)
						{
							NException::CException *pException = (NException::CException *)_pExceptionInfo->ExceptionRecord->ExceptionInformation[1];
							if (pException->f_IsValid())
							{
								Code = CStrNonTracked::CFormat("{} in {}\r\n" DMibPFileLineFormat " {}") << pException->f_GetClass() << pException->f_GetFunction() << pException->f_GetFile() << pException->f_GetLine() << pException->f_GetErrorStrNonTracked();
							}
						}
						
					}
				}

				if (Code.f_IsEmpty())
					Code = CStrNonTracked::CFormat("Unknown ({nfh,sf0,sj8})") << _pExceptionInfo->ExceptionRecord->ExceptionCode;

				ExceptionInfo += "Exception type: " + Code + "\r\n\r\n";

				#if defined(DMibContract_AnyEnabled) || DMibEnableSafeCheck > 0
					NStr::CStrNonTracked LastContract;
					{
						DMibLock(pLocalSys->m_ContractInfoLock);
						LastContract = pLocalSys->m_ContractAssertInfo;
					}
					if (!LastContract.f_IsEmpty())
					{
						ExceptionInfo += "Last contract violation: \r\n";
						ExceptionInfo += LastContract;
						ExceptionInfo += "\r\n\r\n";
					}
				#endif


				bool bCanContinue = !(_pExceptionInfo->ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE) && _pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT;
		//		if (!bCanContinue)
		//			ExceptionInfo += "Exception is noncontinuable\r\n";

				//
				// Exception address
				//
				CStackTraceInfo *pAddressInfo = _pThread ? pLocalSys->f_AquireStackTraceInfo((CMibCodeAddress)_pExceptionInfo->ExceptionRecord->ExceptionAddress) : nullptr;

				if (pAddressInfo)
				{
					ExceptionInfo += CStrNonTracked::CFormat("Exception address: 0x{nfh,sf0,sj*} ({}!{})\r\n\r\n") << ((mint)_pExceptionInfo->ExceptionRecord->ExceptionAddress) << (sizeof(mint) * 2)
						 << (pAddressInfo->m_pModuleName) << (pAddressInfo->m_pFunctionName);
					pLocalSys->f_ReleaseStackTraceInfo(pAddressInfo);
				}
				else
				{
					ExceptionInfo += CStrNonTracked::CFormat("Exception address: 0x{nfh,sf0,sj*1}\r\n\r\n") << ((mint)_pExceptionInfo->ExceptionRecord->ExceptionAddress) << (sizeof(mint) * 2);
				}		

				//
				// Register information
				//
				if (_pExceptionInfo->ContextRecord->ContextFlags & CONTEXT_INTEGER)
				{
		#ifdef DArchX86_64
					ExceptionInfo += CStrNonTracked::CFormat("Integer registers:\r\n"
						"rdi=0x{} rsi=0x{} rax=0x{}\r\n"
						"rbx=0x{} rcx=0x{} rdx=0x{}\r\n"
						"r8=0x{}  r9=0x{}  r10=0x{}\r\n"
						"r11=0x{} r12=0x{} r13=0x{}\r\n"
						"r14=0x{} r15=0x{}\r\n"
						"\r\n")
						<< ((void *)_pExceptionInfo->ContextRecord->Rdi)
						<< ((void *)_pExceptionInfo->ContextRecord->Rsi)
						<< ((void *)_pExceptionInfo->ContextRecord->Rax)
						<< ((void *)_pExceptionInfo->ContextRecord->Rbx)
						<< ((void *)_pExceptionInfo->ContextRecord->Rcx)
						<< ((void *)_pExceptionInfo->ContextRecord->Rdx)
						<< ((void *)_pExceptionInfo->ContextRecord->R8)
						<< ((void *)_pExceptionInfo->ContextRecord->R9)
						<< ((void *)_pExceptionInfo->ContextRecord->R10)
						<< ((void *)_pExceptionInfo->ContextRecord->R11)
						<< ((void *)_pExceptionInfo->ContextRecord->R12)
						<< ((void *)_pExceptionInfo->ContextRecord->R13)
						<< ((void *)_pExceptionInfo->ContextRecord->R14)
						<< ((void *)_pExceptionInfo->ContextRecord->R15)
						;
		#else
					ExceptionInfo += CStrNonTracked::CFormat("Integer registers:\r\n"
						"edi=0x{} esi=0x{} eax=0x{}\r\n"
						"ebx=0x{} ecx=0x{} edx=0x{}\r\n\r\n")
						<< ((void *)_pExceptionInfo->ContextRecord->Edi)
						<< ((void *)_pExceptionInfo->ContextRecord->Esi)
						<< ((void *)_pExceptionInfo->ContextRecord->Eax)
						<< ((void *)_pExceptionInfo->ContextRecord->Ebx)
						<< ((void *)_pExceptionInfo->ContextRecord->Ecx)
						<< ((void *)_pExceptionInfo->ContextRecord->Edx)
						;
		#endif
				}
				if (_pExceptionInfo->ContextRecord->ContextFlags & CONTEXT_CONTROL)
				{
		#ifdef DArchX86_64
					ExceptionInfo 
						+= CStrNonTracked::CFormat("Control registers:\r\n"
						"rip=0x{} rbp=0x{} rsp=0x{}\r\n"
						"SegCs=0x{nfh,sj4,sf0} SegDs=0x{nfh,sj4,sf0}\r\n"
						"SegEs=0x{nfh,sj4,sf0} SegFs=0x{nfh,sj4,sf0}\r\n"
						"SegGs=0x{nfh,sj4,sf0} SegSs=0x{nfh,sj4,sf0}\r\n"
						"EFlags=0x{nfh,sj8,sf0}\r\n\r\n")
						<< ((void *)_pExceptionInfo->ContextRecord->Rip)
						<< ((void *)_pExceptionInfo->ContextRecord->Rbp)
						<< ((void *)_pExceptionInfo->ContextRecord->Rsp)
						<< ((uint16)_pExceptionInfo->ContextRecord->SegCs)
						<< ((uint16)_pExceptionInfo->ContextRecord->SegDs)
						<< ((uint16)_pExceptionInfo->ContextRecord->SegEs)
						<< ((uint16)_pExceptionInfo->ContextRecord->SegFs)
						<< ((uint16)_pExceptionInfo->ContextRecord->SegGs)
						<< ((uint16)_pExceptionInfo->ContextRecord->SegSs)
						<< ((uint32)_pExceptionInfo->ContextRecord->EFlags)
					;
		#else
					ExceptionInfo += CStrNonTracked::CFormat("Control registers:\r\n"
						"eip=0x{} ebp=0x{} esp=0x{}\r\n"
						"SegCs=0x{nfh,sj8,sf0} SegSs=0x{nfh,sj8,sf0} EFlags=0x{nfh,sj8,sf0}\r\n\r\n")
						<< ((void *)_pExceptionInfo->ContextRecord->Eip)
						<< ((void *)_pExceptionInfo->ContextRecord->Ebp)
						<< ((void *)_pExceptionInfo->ContextRecord->Esp)
						<< ((uint32)_pExceptionInfo->ContextRecord->SegCs)
						<< ((uint32)_pExceptionInfo->ContextRecord->SegSs)
						<< ((uint32)_pExceptionInfo->ContextRecord->EFlags)
						;
		#endif
				}

				if (_pExceptionInfo->ContextRecord->ContextFlags & CONTEXT_DEBUG_REGISTERS)
				{
					ExceptionInfo += CStrNonTracked::CFormat("Debug registers:\r\n"
						"Dr0=0x{} Dr1=0x{} Dr2=0x{}\r\n"
						"Dr3=0x{} Dr6=0x{} Dr7=0x{}\r\n\r\n")
						<< ((mint)_pExceptionInfo->ContextRecord->Dr0)
						<< ((mint)_pExceptionInfo->ContextRecord->Dr1)
						<< ((mint)_pExceptionInfo->ContextRecord->Dr2)
						<< ((mint)_pExceptionInfo->ContextRecord->Dr3)
						<< ((mint)_pExceptionInfo->ContextRecord->Dr6)
						<< ((mint)_pExceptionInfo->ContextRecord->Dr7)
						;
				}

		#ifndef DArchX86_64
				if (_pExceptionInfo->ContextRecord->ContextFlags & CONTEXT_SEGMENTS)
				{
					ExceptionInfo += CStrNonTracked::CFormat("Segment registers:\r\n"
						"SegGs=0x{} SegFs=0x{}\r\n"
						"SegEs=0x{} SegDs=0x{}\r\n\r\n")
						<< ((mint)_pExceptionInfo->ContextRecord->SegGs)
						<< ((mint)_pExceptionInfo->ContextRecord->SegFs)
						<< ((mint)_pExceptionInfo->ContextRecord->SegEs)
						<< ((mint)_pExceptionInfo->ContextRecord->SegDs)
						;
				}

				if (_pExceptionInfo->ContextRecord->ContextFlags & CONTEXT_FLOATING_POINT)
				{
					FLOATING_SAVE_AREA &FloatSaveArea = _pExceptionInfo->ContextRecord->FloatSave;

					ExceptionInfo += CStrNonTracked::CFormat("Floating point registers:\r\n"
						"ControlWord=0x{nfh,sf0,sj4} StatusWord=0x{nfh,sf0,sj4} TagWord=0x{nfh,sf0,sj4}\r\n"
						"ErrorOffset=0x{} ErrorSelector=0x{} DataOffset=0x{}\r\n"
						"DataSelector=0x{} \r\n")
						<< ((uint16)FloatSaveArea.ControlWord&0xffff)
						<< ((uint16)FloatSaveArea.StatusWord&0xffff)
						<< ((uint64)FloatSaveArea.TagWord&0xffff)
						<< ((mint)FloatSaveArea.ErrorOffset)
						<< ((mint)FloatSaveArea.ErrorSelector)
						<< ((mint)FloatSaveArea.DataOffset)
						<< ((mint)FloatSaveArea.DataSelector)
						;

					ExceptionInfo += CStrNonTracked::CFormat(
						"St0=0x{nfh,sf0,sj4}{nfh,sf0,sj16} St1=0x{nfh,sf0,sj4}{nfh,sf0,sj16}\r\n"
						"St2=0x{nfh,sf0,sj4}{nfh,sf0,sj16} St3=0x{nfh,sf0,sj4}{nfh,sf0,sj16}\r\n"
						"St4=0x{nfh,sf0,sj4}{nfh,sf0,sj16} St5=0x{nfh,sf0,sj4}{nfh,sf0,sj16}\r\n"
						"St6=0x{nfh,sf0,sj4}{nfh,sf0,sj16} St7=0x{nfh,sf0,sj4}{nfh,sf0,sj16}\r\n\r\n")
						<< (*((uint16 *)&FloatSaveArea.RegisterArea[0*10+8]))
						<< (*((uint64 *)&FloatSaveArea.RegisterArea[0*10]))
						<< (*((uint16 *)&FloatSaveArea.RegisterArea[1*10+8]))
						<< (*((uint64 *)&FloatSaveArea.RegisterArea[1*10]))
						<< (*((uint16 *)&FloatSaveArea.RegisterArea[2*10+8]))
						<< (*((uint64 *)&FloatSaveArea.RegisterArea[2*10]))
						<< (*((uint16 *)&FloatSaveArea.RegisterArea[3*10+8]))
						<< (*((uint64 *)&FloatSaveArea.RegisterArea[3*10]))
						<< (*((uint16 *)&FloatSaveArea.RegisterArea[4*10+8]))
						<< (*((uint64 *)&FloatSaveArea.RegisterArea[4*10]))
						<< (*((uint16 *)&FloatSaveArea.RegisterArea[5*10+8]))
						<< (*((uint64 *)&FloatSaveArea.RegisterArea[5*10]))
						<< (*((uint16 *)&FloatSaveArea.RegisterArea[6*10+8]))
						<< (*((uint64 *)&FloatSaveArea.RegisterArea[6*10]))
						<< (*((uint16 *)&FloatSaveArea.RegisterArea[7*10+8]))
						<< (*((uint64 *)&FloatSaveArea.RegisterArea[7*10]))
						;
				}
		#endif

		#ifndef DArchX86_64
				if (_pExceptionInfo->ContextRecord->ContextFlags & CONTEXT_EXTENDED_REGISTERS)
				{
					ExceptionInfo += CStrNonTracked::CFormat("SSE registers:\r\n"
						"MXCSR=0x{}\r\n"
						"Xmm0=0x{nfh,sf0,sj16}{nfh,sf0,sj16} Xmm1=0x{nfh,sf0,sj16}{nfh,sf0,sj16}\r\n"
						"Xmm2=0x{nfh,sf0,sj16}{nfh,sf0,sj16} Xmm3=0x{nfh,sf0,sj16}{nfh,sf0,sj16}\r\n"
						"Xmm4=0x{nfh,sf0,sj16}{nfh,sf0,sj16} Xmm5=0x{nfh,sf0,sj16}{nfh,sf0,sj16}\r\n"
						"Xmm6=0x{nfh,sf0,sj16}{nfh,sf0,sj16} Xmm7=0x{nfh,sf0,sj16}{nfh,sf0,sj16}\r\n\r\n")
						<< (*((uint32 *)&_pExceptionInfo->ContextRecord->ExtendedRegisters[24]))
						<< (*((uint64 *)&_pExceptionInfo->ContextRecord->ExtendedRegisters[10*16+8]))
						<< (*((uint64 *)&_pExceptionInfo->ContextRecord->ExtendedRegisters[10*16]))
						<< (*((uint64 *)&_pExceptionInfo->ContextRecord->ExtendedRegisters[11*16+8]))
						<< (*((uint64 *)&_pExceptionInfo->ContextRecord->ExtendedRegisters[11*16]))
						<< (*((uint64 *)&_pExceptionInfo->ContextRecord->ExtendedRegisters[12*16+8]))
						<< (*((uint64 *)&_pExceptionInfo->ContextRecord->ExtendedRegisters[12*16]))
						<< (*((uint64 *)&_pExceptionInfo->ContextRecord->ExtendedRegisters[13*16+8]))
						<< (*((uint64 *)&_pExceptionInfo->ContextRecord->ExtendedRegisters[13*16]))
						<< (*((uint64 *)&_pExceptionInfo->ContextRecord->ExtendedRegisters[14*16+8]))
						<< (*((uint64 *)&_pExceptionInfo->ContextRecord->ExtendedRegisters[14*16]))
						<< (*((uint64 *)&_pExceptionInfo->ContextRecord->ExtendedRegisters[15*16+8]))
						<< (*((uint64 *)&_pExceptionInfo->ContextRecord->ExtendedRegisters[15*16]))
						<< (*((uint64 *)&_pExceptionInfo->ContextRecord->ExtendedRegisters[16*16+8]))
						<< (*((uint64 *)&_pExceptionInfo->ContextRecord->ExtendedRegisters[16*16]))
						<< (*((uint64 *)&_pExceptionInfo->ContextRecord->ExtendedRegisters[17*16+8]))
						<< (*((uint64 *)&_pExceptionInfo->ContextRecord->ExtendedRegisters[17*16]))
						;
				}
		#else
				if (_pExceptionInfo->ContextRecord->ContextFlags & CONTEXT_FLOATING_POINT)
				{
					ExceptionInfo 
						+= CStrNonTracked::CFormat
						(
							"Floating point registers:\r\n"
							"MXCSR=0x{}\r\n"
							"Xmm0=0x{nfh,sf0,sj16}{nfh,sf0,sj16} Xmm1=0x{nfh,sf0,sj16}{nfh,sf0,sj16}\r\n"
							"Xmm2=0x{nfh,sf0,sj16}{nfh,sf0,sj16} Xmm3=0x{nfh,sf0,sj16}{nfh,sf0,sj16}\r\n"
							"Xmm4=0x{nfh,sf0,sj16}{nfh,sf0,sj16} Xmm5=0x{nfh,sf0,sj16}{nfh,sf0,sj16}\r\n"
							"Xmm6=0x{nfh,sf0,sj16}{nfh,sf0,sj16} Xmm7=0x{nfh,sf0,sj16}{nfh,sf0,sj16}\r\n"
							"Xmm8=0x{nfh,sf0,sj16}{nfh,sf0,sj16} Xmm9=0x{nfh,sf0,sj16}{nfh,sf0,sj16}\r\n"
							"Xmm10=0x{nfh,sf0,sj16}{nfh,sf0,sj16} Xmm11=0x{nfh,sf0,sj16}{nfh,sf0,sj16}\r\n"
							"Xmm12=0x{nfh,sf0,sj16}{nfh,sf0,sj16} Xmm13=0x{nfh,sf0,sj16}{nfh,sf0,sj16}\r\n"
							"Xmm14=0x{nfh,sf0,sj16}{nfh,sf0,sj16} Xmm15=0x{nfh,sf0,sj16}{nfh,sf0,sj16}\r\n"
							"\r\n"
							"Vector registers:\r\n"
							"Vec0=0x{nfh,sf0,sj16}{nfh,sf0,sj16} Vec1=0x{nfh,sf0,sj16}{nfh,sf0,sj16}\r\n"
							"Vec2=0x{nfh,sf0,sj16}{nfh,sf0,sj16} Vec3=0x{nfh,sf0,sj16}{nfh,sf0,sj16}\r\n"
							"Vec4=0x{nfh,sf0,sj16}{nfh,sf0,sj16} Vec5=0x{nfh,sf0,sj16}{nfh,sf0,sj16}\r\n"
							"Vec6=0x{nfh,sf0,sj16}{nfh,sf0,sj16} Vec7=0x{nfh,sf0,sj16}{nfh,sf0,sj16}\r\n"
							"Vec8=0x{nfh,sf0,sj16}{nfh,sf0,sj16} Vec9=0x{nfh,sf0,sj16}{nfh,sf0,sj16}\r\n"
							"Vec10=0x{nfh,sf0,sj16}{nfh,sf0,sj16} Vec11=0x{nfh,sf0,sj16}{nfh,sf0,sj16}\r\n"
							"Vec12=0x{nfh,sf0,sj16}{nfh,sf0,sj16} Vec13=0x{nfh,sf0,sj16}{nfh,sf0,sj16}\r\n"
							"Vec14=0x{nfh,sf0,sj16}{nfh,sf0,sj16} Vec15=0x{nfh,sf0,sj16}{nfh,sf0,sj16}\r\n"
							"Vec16=0x{nfh,sf0,sj16}{nfh,sf0,sj16} Vec17=0x{nfh,sf0,sj16}{nfh,sf0,sj16}\r\n"
							"Vec18=0x{nfh,sf0,sj16}{nfh,sf0,sj16} Vec19=0x{nfh,sf0,sj16}{nfh,sf0,sj16}\r\n"
							"Vec20=0x{nfh,sf0,sj16}{nfh,sf0,sj16} Vec21=0x{nfh,sf0,sj16}{nfh,sf0,sj16}\r\n"
							"Vec22=0x{nfh,sf0,sj16}{nfh,sf0,sj16} Vec23=0x{nfh,sf0,sj16}{nfh,sf0,sj16}\r\n"
							"Vec24=0x{nfh,sf0,sj16}{nfh,sf0,sj16} Vec25=0x{nfh,sf0,sj16}{nfh,sf0,sj16}\r\n"
							"\r\n"
						)
						<< (void *)(mint)_pExceptionInfo->ContextRecord->FltSave.MxCsr
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[0].High
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[0].Low
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[1].High
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[1].Low
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[2].High
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[2].Low
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[3].High
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[3].Low
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[4].High
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[4].Low
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[5].High
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[5].Low
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[6].High
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[6].Low
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[7].High
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[7].Low
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[8].High
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[8].Low
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[9].High
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[9].Low
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[10].High
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[10].Low
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[11].High
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[11].Low
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[12].High
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[12].Low
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[13].High
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[13].Low
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[14].High
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[14].Low
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[15].High
						<< _pExceptionInfo->ContextRecord->FltSave.XmmRegisters[15].Low

						<< _pExceptionInfo->ContextRecord->VectorRegister[0].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[0].Low
						<< _pExceptionInfo->ContextRecord->VectorRegister[1].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[1].Low
						<< _pExceptionInfo->ContextRecord->VectorRegister[2].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[2].Low
						<< _pExceptionInfo->ContextRecord->VectorRegister[3].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[3].Low
						<< _pExceptionInfo->ContextRecord->VectorRegister[4].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[4].Low
						<< _pExceptionInfo->ContextRecord->VectorRegister[5].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[5].Low
						<< _pExceptionInfo->ContextRecord->VectorRegister[6].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[6].Low
						<< _pExceptionInfo->ContextRecord->VectorRegister[7].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[7].Low
						<< _pExceptionInfo->ContextRecord->VectorRegister[8].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[8].Low
						<< _pExceptionInfo->ContextRecord->VectorRegister[9].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[9].Low
						<< _pExceptionInfo->ContextRecord->VectorRegister[10].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[10].Low
						<< _pExceptionInfo->ContextRecord->VectorRegister[11].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[11].Low
						<< _pExceptionInfo->ContextRecord->VectorRegister[12].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[12].Low
						<< _pExceptionInfo->ContextRecord->VectorRegister[13].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[13].Low
						<< _pExceptionInfo->ContextRecord->VectorRegister[14].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[14].Low
						<< _pExceptionInfo->ContextRecord->VectorRegister[15].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[15].Low
						<< _pExceptionInfo->ContextRecord->VectorRegister[16].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[16].Low
						<< _pExceptionInfo->ContextRecord->VectorRegister[17].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[17].Low
						<< _pExceptionInfo->ContextRecord->VectorRegister[18].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[18].Low
						<< _pExceptionInfo->ContextRecord->VectorRegister[19].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[19].Low
						<< _pExceptionInfo->ContextRecord->VectorRegister[20].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[20].Low
						<< _pExceptionInfo->ContextRecord->VectorRegister[21].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[21].Low
						<< _pExceptionInfo->ContextRecord->VectorRegister[22].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[22].Low
						<< _pExceptionInfo->ContextRecord->VectorRegister[23].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[23].Low
						<< _pExceptionInfo->ContextRecord->VectorRegister[24].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[24].Low
						<< _pExceptionInfo->ContextRecord->VectorRegister[25].High
						<< _pExceptionInfo->ContextRecord->VectorRegister[25].Low


					;
				}

		#endif

				//
				// Stack trace
				//
				{
					ExceptionInfo += "StackTrace: \r\n";
					int iMaxDepth = 1024;

					CUndocumentedTEB *pTEB = fg_GetTEB();
					mint StackStart = (mint)pTEB->Tib.StackBase;
					mint StackEnd = (mint)pTEB->Tib.StackLimit;


					try
					{
		#ifdef DArchX86_64
						mint StackFrame = _pExceptionInfo->ContextRecord->Rbp;
		#else
						mint StackFrame = _pExceptionInfo->ContextRecord->Ebp;
		#endif
						mint LastCode = (mint)_pExceptionInfo->ExceptionRecord->ExceptionAddress;
						while (iMaxDepth)
						{
							if (!fg_IsGoodStackPtr((void *)StackFrame, sizeof(mint) * 2, StackStart, StackEnd))
								break;
							mint CodePtr = *((mint *)(StackFrame + sizeof(mint)));

							CStackTraceInfo *pAddressInfo = _pThread ? pLocalSys->f_AquireStackTraceInfo((CMibCodeAddress)LastCode) : nullptr;

							if (pAddressInfo)
							{
								ExceptionInfo += CStrNonTracked::CFormat("0x{nfh,sf0,sj*} {}!{}\r\n{}:{}\r\n") << (mint)LastCode << sizeof(mint) * 2
									<< pAddressInfo->m_pModuleName << pAddressInfo->m_pFunctionName
									<< pAddressInfo->m_pSourceFileName << pAddressInfo->m_SourceLine
									;
								pLocalSys->f_ReleaseStackTraceInfo(pAddressInfo);
							}
							else
							{
								ExceptionInfo += CStrNonTracked::CFormat("0x{nfh,sf0,sj*1}\r\n") << (LastCode) << (sizeof(mint) * 2);
							}		
							ExceptionInfo += CStrNonTracked::CFormat("StackFrame: 0x{nfh,sf0,sj*1}\r\n") << (StackFrame) << (sizeof(mint) * 2);
							ExceptionInfo += "\r\n";

							LastCode = CodePtr;

							StackFrame = *((mint *)(StackFrame));
							--iMaxDepth;			
						}
					}
					catch(...)
					{
					}

					//
					// Stack
					//

					try
					{
		//				DMibDTrace("StackEnd {nfh,sj8,sf0} StackStart {nfh,sj8,sf0}", StackEnd << StackStart);
						CStrNonTracked Stack;
						int iRowSize = 32;
						int iRow = iRowSize;			
						Stack += CStrNonTracked::CFormat("0x{nfh,sf0,sj*1}: ") << (StackEnd) << (sizeof(mint) * 2);
						while (StackEnd < StackStart)
						{
							int iMax = fg_Min((int)StackStart - (int)StackEnd, iRow);
							if (iMax >= 8)
							{
								Stack += CStrNonTracked::CFormat("{nfh,sf0,sj16} ") << (fg_ByteSwap(*((uint64 *)StackEnd)));
								StackEnd += 8;
								iRow -= 8;
							}
							else if (iMax >= 4)
							{
								Stack += CStrNonTracked::CFormat("{nfh,sf0,sj8}") << (fg_ByteSwap(*((uint32 *)StackEnd)));
								StackEnd += 4;
								iRow -= 4;
							}
							else if (iMax >= 2)
							{
								Stack += CStrNonTracked::CFormat("{nfh,sf0,sj4}") << (fg_ByteSwap(*((uint16 *)StackEnd)));
								StackEnd += 2;
								iRow -= 2;
							}
							else if (iMax >= 1)
							{
								Stack += CStrNonTracked::CFormat("{nfh,sf0,sj2}") << (*((uint8 *)StackEnd));
								StackEnd += 1;
								iRow -= 1;
							}

							if (iRow <= 0)
							{
								iRow = iRowSize;
								Stack += "\r\n";
								Stack += CStrNonTracked::CFormat("0x{nfh,sf0,sj*1}: ") << (StackEnd) << (sizeof(mint) * 2);
							}
						}

						ExceptionInfo += Stack;
					}
					catch (...)
					{
					}
				}

				CStrNonTracked GDIDump = pLocalSys->f_DumpObjects();

				ExceptionInfo += GDIDump;

				CStrNonTracked ModuleDump = pLocalSys->f_DumpModules();

				ExceptionInfo += ModuleDump;

				if (!_ExtraLog.f_IsEmpty())
				{
					ExceptionInfo += fs_FixLineEndings(_ExtraLog);
				}

				{ 
					NFile::CFile::fs_WriteStringToFile(FileName, ExceptionInfo);
					if (_pGeneratedLogs)
						_pGeneratedLogs->f_Insert(FileName);
				}

				//
				// Message box
				//
				CStrNonTracked ProgramName = fg_GetSys()->f_GetProgramNameNonTracked();
				if (ProgramName.f_IsEmpty())
					ProgramName = "The program";

				CStrNonTracked ProgramNameCopy = ProgramName;
				ProgramName = CStrNonTracked::CFormat("{} ({})") << ProgramNameCopy << (mint)GetCurrentProcessId();
				CStrNonTracked SupportEmail = fg_GetSys()->f_GetSupportEmailNonTracked();
				if (SupportEmail.f_IsEmpty())
					SupportEmail = "unknown@example.com";
				bint bService = fg_GetSys()->f_GetRunningAsService();
		
				bint bContinue = (!_Message.f_IsEmpty() || _pGeneratedLogs != nullptr);
				if (!bService)
				{
					for (mint i = 0; i < nCache; ++i)
					{
						if (pLocalSys->m_CacheWindows[i])
						{
							DestroyWindow(pLocalSys->m_CacheWindows[i]);
							pLocalSys->m_CacheWindows[i] = nullptr;
						}
					}

					if (_bDisplayGUI)
					{

						if (pLocalSys->m_pCrashDumpUserNotifyFunction && !NMib::NSys::fg_ConsoleErrorOutputValid())
						{
							bContinue = (*(pLocalSys->m_pCrashDumpUserNotifyFunction))(_Message, ProgramName, SupportEmail, FileName, FileNameDumpMini, FileNameDump, bCanContinue);
						}
						else
						{
							bContinue = fg_DefaultCrashDumpUserNotify(_Message, ProgramName, SupportEmail, FileName, FileNameDumpMini, FileNameDump, bCanContinue);
						}
					}
				}

				if (bContinue)
				{
					if (!_pGeneratedLogs)
					{
			#ifdef DArchX86_64
						_pExceptionInfo->ContextRecord->Rip++;
			#else
						_pExceptionInfo->ContextRecord->Eip++;
			#endif
					}
					return EXCEPTION_CONTINUE_EXECUTION;
				}
				else
				{
					if (g_fOrgTerminateProcess)
						g_fOrgTerminateProcess(GetCurrentProcess(), 201);
					else
						TerminateProcess(GetCurrentProcess(), 201);
				}



				return EXCEPTION_CONTINUE_SEARCH;
			}
		;
		
		BOOL bDllHeld;
		AuxUlibIsDLLSynchronizationHeld(&bDllHeld);

		if (bDllHeld  || g_bDoneMalterlibInitAll.f_Load() < 3)
		{
			// If Dll lock is held we will get a deadlock here
			return fl_GenerateException(nullptr);
		}
		else
		{
			TCUniquePointer<CThreadObjectNonTracked, NMem::CAllocator_NonTrackedHeap> pThread = CThreadObjectNonTracked::fs_StartThread(fl_GenerateException, "DumpExceptionsThread");
			pThread->f_Stop();
			return pThread->f_GetReturnValue();
		}
	}	

	static BOOL CALLBACK fp_DumpExceptionMemoryCallback
		(
			PVOID _pParam, 
			const PMINIDUMP_CALLBACK_INPUT _pInput, 
			PMINIDUMP_CALLBACK_OUTPUT _pOutput 
		) 
	{
		if (_pInput->CallbackType == MemoryCallback)
		{
			CExceptionMemoryData & Data = *(CExceptionMemoryData *)_pParam;

			if (Data.m_iCurrentLocation < Data.m_Locations.f_GetLen())
			{
				_pOutput->MemoryBase = (ULONG64)Data.m_Locations[Data.m_iCurrentLocation];
				_pOutput->MemorySize = Data.m_Sizes[Data.m_iCurrentLocation];

				++Data.m_iCurrentLocation;
				return true;
			}
			else
				return false;
		}

		return true;
	}

	static LONG WINAPI fsp_DumpExceptionMemory
		(
			struct _EXCEPTION_POINTERS *_pExceptionInfo
			, CExceptionMemoryData * _pExceptionMemoryData
		)
	{
		DMibDeadlockDetectorPause;

		CSystemWindowsMSVC *pLocalSys = fg_GetLocalSys();
		DMibLock(pLocalSys->m_DumpExceptionInfoLock);

		auto fl_GenerateException
			= [&_pExceptionMemoryData, _pExceptionInfo] (CThreadObjectNonTracked *_pThread) -> aint
			{
				mint nCache = CSystemWindowsMSVC::EWindowCache;
				CSystemWindowsMSVC *pLocalSys = fg_GetLocalSys();

				CStrNonTracked CrashDumpPath = fg_ConvertFromWindowsPath<CWStrNonTracked, CWStrNonTracked>(NSys::fg_Process_GetEnvironmentVariable(CStrNonTracked("IdsCrashDumpDir")));
				if (CrashDumpPath.f_IsEmpty() || !CSystemWindowsMSVC::fs_CheckAccessRights(CrashDumpPath))
				{
					CrashDumpPath = NMib::NFile::CFile::fs_AppendPath(NMib::fg_GetSys()->f_GetProgramRootNonTracked(), CStrNonTracked("CrashDumps"));
					if (!CSystemWindowsMSVC::fs_CheckAccessRights(CrashDumpPath))
					{
						CrashDumpPath = NMib::NFile::CFile::fs_AppendPath(NFile::CFile::fs_GetProgramDirectoryNonTracked(), CStrNonTracked("CrashDumps"));
						if (!CSystemWindowsMSVC::fs_CheckAccessRights(CrashDumpPath))
						{
							CrashDumpPath = NMib::NFile::CFile::fs_AppendPath(NFile::CFile::fs_GetUserLocalProgramDirectoryNonTracked(), CStrNonTracked("CrashDumps"));
							if (!CSystemWindowsMSVC::fs_CheckAccessRights(CrashDumpPath))
							{
								return EXCEPTION_CONTINUE_SEARCH;
							}
						}
					}
				}

				CStrNonTracked FileNameDumpMini;

				{
					NTime::CTimeConvert::CDateTime DateTime;
					NTime::CTimeConvert(NTime::CTime::fs_NowLocal()).f_ExtractDateTime(DateTime);

					int32 Fraction = (DateTime.m_Fraction*1000.0).f_ToIntRound();
					if (Fraction >= 1000)
						Fraction = 999;

					uint32 RandomValue = pLocalSys->f_GetRandom();

					FileNameDumpMini = CrashDumpPath + (CStrNonTracked::CFormat("/MemoryDump_{}-{sj2,sf0}-{sj2,sf0}_{sj2,sf0}.{sj2,sf0}.{sj2,sf0}.{sj3,sf0}.{sj8,sf0,nfh}.dmp")
						<< DateTime.m_Year << DateTime.m_Month << DateTime.m_DayOfMonth << DateTime.m_Hour << DateTime.m_Minute << DateTime.m_Second << Fraction << RandomValue).f_GetStr();
				}
				// Mini dump
				{
					CStrNonTracked StackTraceError;
					bint bRet;
					if (_pThread)
						bRet = pLocalSys->m_StackTrace.f_Init(StackTraceError);
					else
					{
						CFStr256 Info;
						bRet = pLocalSys->m_StackTrace.f_InitDll(Info);
						StackTraceError = Info;
					}
					
					if (bRet)
					{
						if (pLocalSys->m_StackTrace.MiniDumpWriteDump)
						{
							MINIDUMP_EXCEPTION_INFORMATION Info;
							Info.ClientPointers = false;
							Info.ExceptionPointers = _pExceptionInfo;
							Info.ThreadId = GetCurrentThreadId();
							
							MINIDUMP_CALLBACK_INFORMATION CallbackInfo; 
							CallbackInfo.CallbackRoutine = (MINIDUMP_CALLBACK_ROUTINE)CSystemWindowsMSVC::fp_DumpExceptionMemoryCallback; 
							CallbackInfo.CallbackParam = (void*)_pExceptionMemoryData;

							CWin32File *pFile = (CWin32File *)NSys::NFile::fg_Open(FileNameDumpMini, NFile::EFileOpen_Write);
							if (pFile)
							{
								MINIDUMP_TYPE DumpType = (MINIDUMP_TYPE )(MiniDumpWithHandleData | MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithProcessThreadData);
								if (!pLocalSys->m_StackTrace.MiniDumpWriteDump(pLocalSys->m_StackTrace.m_hProcess, GetCurrentProcessId(), pFile->m_pFile, DumpType, &Info, nullptr, &CallbackInfo))
								{
									CFStr256 ErrorStr = CFStr256::CFormat("Could not write mini dump. The error was: {}") << fg_Win32_GetLastErrorStr(GetLastError());
									DMibDTrace("{}\n", ErrorStr);
								}
								NSys::NFile::fg_Close(pFile);
							}
						}
					}
				} 

				return EXCEPTION_CONTINUE_EXECUTION;
			}
		;
		
		BOOL bDllHeld;
		AuxUlibIsDLLSynchronizationHeld(&bDllHeld);

		if (bDllHeld  || g_bDoneMalterlibInitAll.f_Load() < 3)
		{
			// If Dll lock is held we will get a deadlock here
			return fl_GenerateException(nullptr);
		}
		else
		{
			TCUniquePointer<CThreadObjectNonTracked, NMem::CAllocator_NonTrackedHeap> pThread = CThreadObjectNonTracked::fs_StartThread(fl_GenerateException, "DumpExceptionsThread");
			pThread->f_Stop();
			return pThread->f_GetReturnValue();
		}
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

		HMODULE pNtDll = g_hNtDll;

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

	typedef struct _LDR_DLL_LOADED_NOTIFICATION_DATA 
	{
		ULONG Flags;                    //Reserved.
		PCUNICODE_STRING FullDllName;   //The full path name of the DLL module.
		PCUNICODE_STRING BaseDllName;   //The base file name of the DLL module.
		PVOID DllBase;                  //A pointer to the base address for the DLL in memory.
		ULONG SizeOfImage;              //The size of the DLL image, in bytes.
	} LDR_DLL_LOADED_NOTIFICATION_DATA, *PLDR_DLL_LOADED_NOTIFICATION_DATA;

	typedef struct _LDR_DLL_UNLOADED_NOTIFICATION_DATA 
	{
		ULONG Flags;                    //Reserved.
		PCUNICODE_STRING FullDllName;   //The full path name of the DLL module.
		PCUNICODE_STRING BaseDllName;   //The base file name of the DLL module.
		PVOID DllBase;                  //A pointer to the base address for the DLL in memory.
		ULONG SizeOfImage;              //The size of the DLL image, in bytes.
	} LDR_DLL_UNLOADED_NOTIFICATION_DATA, *PLDR_DLL_UNLOADED_NOTIFICATION_DATA;

	typedef union _LDR_DLL_NOTIFICATION_DATA 
	{
		LDR_DLL_LOADED_NOTIFICATION_DATA Loaded;
		LDR_DLL_UNLOADED_NOTIFICATION_DATA Unloaded;
	} LDR_DLL_NOTIFICATION_DATA, *PLDR_DLL_NOTIFICATION_DATA;

	
	typedef VOID (NTAPI *PLDR_DLL_NOTIFICATION_FUNCTION )(ULONG NotificationReason, const PLDR_DLL_NOTIFICATION_DATA NotificationData, PVOID Context);

	static VOID NTAPI DllLoadedCallback(ULONG NotificationReason, const PLDR_DLL_NOTIFICATION_DATA NotificationData, PVOID Context)
	{
		CSystemWindowsMSVC *pThis = (CSystemWindowsMSVC *)Context;
		pThis->m_bPollCheckExceptionFilterTimes = 1000/25;
		pThis->m_ExceptionFilterPoller.m_EventWantQuit.f_Signal();
		if (pThis->m_bExceptionFilterPollerInstalled.f_Exchange(1) == 0)
			pThis->m_ExceptionFilterPoller.f_Start();

		/*if (NotificationReason == 1)
			DMibTraceSafe("Loaded: {}\r\n", NotificationData->Loaded.FullDllName->Buffer);
		else if (NotificationReason == 2)
			DMibTraceSafe("Unlaoded: {}\r\n", NotificationData->Unloaded.FullDllName->Buffer);
		else
			DMibTraceSafe("Unknown loaded code", 0);*/
	}

	bint m_bPollCheckExceptionFilter;
	aint m_bPollCheckExceptionFilterTimes;
	void *m_pDllNotificationCookie;

	class CExceptionFilterPoller : public NMib::NThread::CThread
	{
	public:
		virtual NStr::CStr f_GetThreadName()
		{
			return "Malterlib_Core_PlatformImp_ExceptionFilterPoller";
		}
		CSystemWindowsMSVC *m_pSystem;

		CExceptionFilterPoller()
		{
		}
		
		~CExceptionFilterPoller()
		{
		}

		inline_never void f_PollFilter()
		{
			if (m_pSystem->m_bPollCheckExceptionFilter || m_pSystem->m_bPollCheckExceptionFilterTimes)
			{
				int64 Timer = NMib::fg_GetSys()->f_GetTimerValue();
				//DMibDTrace("Checking Exception Filter {} {}\r\n", m_pSystem->m_bPollCheckExceptionFilterTimes << Timer);
				if (m_pSystem->m_bPollCheckExceptionFilterTimes)
					--m_pSystem->m_bPollCheckExceptionFilterTimes;
				LPTOP_LEVEL_EXCEPTION_FILTER pTop = SetUnhandledExceptionFilter(&CSystemWindowsMSVC::fsp_UnhandledException);
				if (pTop != &CSystemWindowsMSVC::fsp_UnhandledException)
				{
					DMibDTrace("ATTENTION: ATTENTION: ATTENTION: ATTENTION: ATTENTION: ATTENTION: Unhandled exception filter was lost({} != {}): {}\n", pTop << &CSystemWindowsMSVC::fsp_UnhandledException << Timer);
					//m_pSystem->m_pPrevExceptionFilter = pTop;
				}
			}
		}

		aint f_Main()
		{
			while (f_GetState() != NThread::EThreadState_EventWantQuit)
			{
				f_PollFilter();

				if (m_pSystem->m_bPollCheckExceptionFilterTimes)
					m_EventWantQuit.f_WaitTimeout(0.025);
				else if (m_pSystem->m_bPollCheckExceptionFilter)
					m_EventWantQuit.f_WaitTimeout(5.0);
				else
					m_EventWantQuit.f_Wait();
			}

			return 0;
		}
	};
	CExceptionFilterPoller m_ExceptionFilterPoller;
	NAtomic::TCAtomic<smint> m_bExceptionFilterPollerInstalled;

	void f_InstallExceptionFilterCallback(bint _bDoInstall)
	{
		m_ExceptionFilterPoller.m_pSystem = this;
		m_bPollCheckExceptionFilter = true;
		m_pDllNotificationCookie = nullptr;
		if (_bDoInstall)
		{
			HMODULE pNTDll = g_hNtDll;
			if (pNTDll)
			{
				NTSTATUS (NTAPI *pLdrRegisterDllNotification)(ULONG Flags, PLDR_DLL_NOTIFICATION_FUNCTION NotificationFunction, PVOID Context, PVOID *Cookie);

				(FARPROC &)pLdrRegisterDllNotification = GetProcAddress(pNTDll, "LdrRegisterDllNotification");

				if (pLdrRegisterDllNotification)
				{
					m_bPollCheckExceptionFilter = false;
					pLdrRegisterDllNotification(0, DllLoadedCallback, this, &m_pDllNotificationCookie);
				}
			}

			if (m_bPollCheckExceptionFilter)
			{
				if (m_bExceptionFilterPollerInstalled.f_Exchange(1) == 0)
					m_ExceptionFilterPoller.f_Start();
			}
		}
	}

	void f_UninstallExceptionFilterCallback()
	{
		m_ExceptionFilterPoller.f_Stop();
		if (m_pDllNotificationCookie)
		{
			HMODULE pNTDll = g_hNtDll;
			if (pNTDll)
			{
				NTSTATUS (NTAPI *pLdrUnregisterDllNotification)(PVOID Cookie);

				(FARPROC &)pLdrUnregisterDllNotification = GetProcAddress(pNTDll, "LdrUnregisterDllNotification");

				if (pLdrUnregisterDllNotification)
				{
					pLdrUnregisterDllNotification(m_pDllNotificationCookie);
					m_pDllNotificationCookie = nullptr;
				}
			}
		}

	}

	class CWindowsDeadlockDetector : public CDeadlockDetector
	{
	public:

		CSystemWindowsMSVC *m_pSystem;
		HWND m_CurrentDialogWindow;

		CWindowsDeadlockDetector()
		{
			// This needs to be named excatly like this to be compatibly with old versions of the library (when Malterlib was named Ids)
			if (!FindAtom(str_utf16("IdsDeadLockAtom")))
			{
				AddAtom(str_utf16("IdsDeadLockAtom"));
				mp_pPause = (NAtomic::TCAtomicAggregate<aint> *)HeapAlloc(GetProcessHeap(), 0, sizeof(NAtomic::TCAtomicAggregate<aint>));
				mp_pPause->f_Store(0);
				mint PausePointer = (mint)mp_pPause;
				for (mint i = 0; i < sizeof(mint) * 8; ++i)
				{
					if (PausePointer & (mint(1) << i))
					{
						AddAtom(CFWStr128(CFWStr128::CFormat(str_utf16("IdsDeadLockAtom{}")) << i));
					}
				}
			}
			else
			{
				mint PausePointer = 0;
				for (mint i = 0; i < sizeof(mint) * 8; ++i)
				{
					if (FindAtom(CFWStr128(CFWStr128::CFormat(str_utf16("IdsDeadLockAtom{}")) << i)))
					{
						PausePointer |= (mint(1) << i);
						
					}
				}
				mp_pPause = (NAtomic::TCAtomicAggregate<aint> *)PausePointer;
			}

			m_CurrentDialogWindow = nullptr;
		}
		
		~CWindowsDeadlockDetector()
		{
		}

		HFONT f_CreatePointFontIndirect(const LOGFONT* lpLogFont)
		{
			HDC hDC;
			hDC = ::GetDC(nullptr);

			// convert nPointSize to logical units based on pDC
			LOGFONT logFont = *lpLogFont;
			POINT pt;
			pt.y = ::GetDeviceCaps(hDC, LOGPIXELSY) * logFont.lfHeight;
			pt.y /= 720;    // 72 points/inch, 10 decipoints/point
			pt.x = 0;
			::DPtoLP(hDC, &pt, 1);
			POINT ptOrg = { 0, 0 };
			::DPtoLP(hDC, &ptOrg, 1);
			logFont.lfHeight = -fg_Abs(pt.y - ptOrg.y);

			ReleaseDC(nullptr, hDC);

			return CreateFontIndirect(&logFont);
		}

		HFONT f_CreatePointFont(int nPointSize, LPCTSTR lpszFaceName)
		{

			LOGFONT logFont;
			memset(&logFont, 0, sizeof(LOGFONT));
			logFont.lfCharSet = DEFAULT_CHARSET;
			logFont.lfHeight = nPointSize;
			fg_StrCopy(logFont.lfFaceName, lpszFaceName, sizeof(logFont.lfFaceName) / sizeof(logFont.lfFaceName[0]));

			return f_CreatePointFontIndirect(&logFont);
		}

		enum
		{
			ID_HELP = 150,
			ID_TEXT = 200,
			IDB_Yes,
			IDB_No
		};

		HFONT m_NormalFont;

		static INT_PTR CALLBACK fsp_HandleMessages(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			CSystemWindowsMSVC *pSys = fg_GetLocalSys();
			switch(uMsg)
			{
			case WM_SYSCOMMAND:
				{
					switch (wParam)
					{
					case SC_CLOSE:

						EndDialog(hwndDlg, 1);
						return 1;
					};
				}
				break;
			case WM_TIMER:
				{
					bint bDeadlocked = pSys->m_DealockDetector.f_IsDeadlocked();
					if (!bDeadlocked)
					{
						EndDialog(hwndDlg, 1);
					}
					return 1;
				}
				break;
			case WM_INITDIALOG:
				{
					
					POINT Pnt;
					GetCursorPos(&Pnt);
					HMONITOR hMon = MonitorFromPoint(Pnt, MONITOR_DEFAULTTONEAREST);
					MONITORINFO MonInfo;
					fg_MemClear(MonInfo);
					MonInfo.cbSize = sizeof(MonInfo);
					GetMonitorInfo(hMon, &MonInfo);

					RECT Rect;
					GetWindowRect(hwndDlg, &Rect);

					int Width = Rect.right - Rect.left;
					int Height = Rect.bottom - Rect.top;
					int MonWidth = MonInfo.rcWork.right - MonInfo.rcWork.left;
					int MonHeight = MonInfo.rcWork.bottom - MonInfo.rcWork.top;

					SendMessage(hwndDlg, WM_SETFONT, (WPARAM)pSys->m_DealockDetector.m_NormalFont, 0);
					HWND hTmp = GetDlgItem(hwndDlg, IDB_Yes);
					SendMessage(hTmp, WM_SETFONT, (WPARAM)pSys->m_DealockDetector.m_NormalFont, 0);
					hTmp = GetDlgItem(hwndDlg, IDB_No);
					SendMessage(hTmp, WM_SETFONT, (WPARAM)pSys->m_DealockDetector.m_NormalFont, 0);
					hTmp = GetDlgItem(hwndDlg, ID_TEXT);
					SendMessage(hTmp, WM_SETFONT, (WPARAM)pSys->m_DealockDetector.m_NormalFont, 0);
					
//					SendDlgItemMessage(hwndDlg, IDB_Yes, BM_SETSTYLE, BS_PUSHBUTTON, (LONG)TRUE);
//					SendDlgItemMessage(hwndDlg, IDB_No, BM_SETSTYLE, BS_DEFPUSHBUTTON, (LONG)TRUE);
					SendMessage(hwndDlg, DM_SETDEFID, IDB_Yes,0L);

					SetWindowPos(hwndDlg, nullptr, MonInfo.rcWork.left + MonWidth / 2 - Width / 2, MonInfo.rcWork.top + MonHeight / 2 - Height / 2, 0, 0, SWP_NOSIZE|SWP_NOZORDER);

					SetTimer(hwndDlg, 0, 100, nullptr);
					BringWindowToTop(hwndDlg);
					pSys->m_DealockDetector.m_CurrentDialogWindow = hwndDlg;
					return 1;
				}
				break;
			case WM_COMMAND:
				{
					int Control = LOWORD(wParam);
					int Message = HIWORD(wParam);
//					HWND Window = (HWND)lParam;
					if (Message == BN_CLICKED)
					{
						if (Control == IDB_Yes)
						{
							EndDialog(hwndDlg, 1);
							return 1;
						}
						else if (Control == IDB_No)
						{
							EndDialog(hwndDlg, 335);
							return 1;
						}
					}

				}
				break;

			}
			return 0;
		}


		LRESULT f_DisplayMyMessage(HINSTANCE hinst, HWND hwndOwner, 
			LPCSTR lpszMessage, LPCSTR lpszTitle)
		{
			HGLOBAL hgbl;
			LPDLGTEMPLATE lpdt;
			LPDLGITEMTEMPLATE lpdit;
			LPWORD lpw;
			LPWSTR lpwsz;
			LRESULT ret;
			int nchar;

			hgbl = GlobalAlloc(GMEM_ZEROINIT, 2048);
			if (!hgbl)
				return -1;
		 
			lpdt = (LPDLGTEMPLATE)GlobalLock(hgbl);
		 
			// Define a dialog box.
		 
			lpdt->style = WS_POPUP | WS_BORDER | WS_SYSMENU | DS_MODALFRAME | WS_CAPTION;
			lpdt->cdit = 3;  // number of controls
			lpdt->x  = 10;  lpdt->y  = 10;
			lpdt->cx = 200; lpdt->cy = 75;

			lpw = (LPWORD) (lpdt + 1);
			*lpw++ = 0;   // no menu
			*lpw++ = 0;   // predefined dialog box class (by default)

			lpwsz = (LPWSTR) lpw;
			nchar = MultiByteToWideChar (CP_ACP, 0, lpszTitle, 
											-1, lpwsz, 50);
			lpw += nchar;

			int32 ButtonWidth = 60;
			int32 ButtonGap = 5;
			int32 ButtonStart = (lpdt->cx - (ButtonWidth*2 + ButtonGap)) / 2;
			//-----------------------
			// Define an Yes button.
			//-----------------------
			lpw = fg_AlignUp (lpw, 4); // align DLGITEMTEMPLATE on DWORD boundary
			lpdit = (LPDLGITEMTEMPLATE) lpw;
			lpdit->x  = ButtonStart; lpdit->y  = 55;
			lpdit->cx = ButtonWidth; lpdit->cy = 12;
			lpdit->id = IDB_Yes;  // OK button identifier
			lpdit->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON;

			lpw = (LPWORD) (lpdit + 1);
			*lpw++ = 0xFFFF;
			*lpw++ = 0x0080;    // button class

			lpwsz = (LPWSTR) lpw;
			nchar = MultiByteToWideChar (CP_ACP, 0, "Continue waiting", -1, lpwsz, 50);
			lpw   += nchar;
			*lpw++ = 0x0000; // Creation Data

			//-----------------------
			// Define an No button.
			//-----------------------
			lpw = fg_AlignUp (lpw, 4); // align DLGITEMTEMPLATE on DWORD boundary
			lpdit = (LPDLGITEMTEMPLATE) lpw;
			lpdit->x  = ButtonStart + ButtonWidth + ButtonGap; lpdit->y  = 55;
			lpdit->cx = ButtonWidth; lpdit->cy = 12;
			lpdit->id = IDB_No;  // OK button identifier
			lpdit->style = WS_CHILD | WS_VISIBLE;

			lpw = (LPWORD) (lpdit + 1);
			*lpw++ = 0xFFFF;
			*lpw++ = 0x0080;    // button class

			lpwsz = (LPWSTR) lpw;
			nchar = MultiByteToWideChar (CP_ACP, 0, "Create error report", -1, lpwsz, 50);
			lpw   += nchar;
			*lpw++ = 0x0000; // Creation Data

			//-----------------------
			// Define a static text control.
			//-----------------------
			lpw = fg_AlignUp (lpw, 4); // align DLGITEMTEMPLATE on DWORD boundary
			lpdit = (LPDLGITEMTEMPLATE) lpw;
			lpdit->x  = 7; lpdit->y  = 7;
			lpdit->cx = 180; lpdit->cy = 45;
			lpdit->id = ID_TEXT;  // text identifier
			lpdit->style = WS_CHILD | WS_VISIBLE | SS_LEFT;

			lpw = (LPWORD) (lpdit + 1);
			*lpw++ = 0xFFFF;
			*lpw++ = 0x0082;                         // static class

			lpw = (LPWORD)NStr::fg_StrCopy((ch16 *)lpw, lpszMessage);
			*lpw++ = 0x0000; // Creation Data
	
			GlobalUnlock(hgbl);
 
			m_NormalFont = f_CreatePointFont(80, str_utf16("MS Sans Serif"));
			ret = DialogBoxIndirect(hinst, (LPDLGTEMPLATE) hgbl, hwndOwner, (DLGPROC) fsp_HandleMessages);
			m_CurrentDialogWindow = nullptr;
			DeleteObject(m_NormalFont);

			GlobalFree(hgbl);
 
			return ret; 
		}


	protected:

		inline_never bint fp_DisplayMessage(CStr const& _Title, CStr const& _Message)
		{
			if (fg_GetLocalSys()->m_pDeadlockNotifyFunction)
			{
				return !fg_GetLocalSys()->m_pDeadlockNotifyFunction();
			}
			else
			{
				int Return = f_DisplayMyMessage((HINSTANCE)nullptr, nullptr, _Message, _Title);

				if (Return == -1)
				{
					DMibDTrace("{}\n", fg_Win32_GetLastErrorStr());
				}

				if (Return == 335)
					return 0;
				else
					return 1;
			}
		}

	};
	CWindowsDeadlockDetector m_DealockDetector;

	NSys::FDeadlockUserNotify *m_pDeadlockNotifyFunction;
	
	void f_Debug_SetDeadlockUserNotifyFunction(NSys::FDeadlockUserNotify *_pDeadlockNotifyFunction)
	{
		m_pDeadlockNotifyFunction = _pDeadlockNotifyFunction;
	}
		

	void f_Debug_StartDeadlockDetector(fp64 _Timeout)
	{
		m_DealockDetector.m_pSystem = this;
		m_DealockDetector.f_SetTimeout(_Timeout);
		if (m_DealockDetector.f_GetState() != NThread::EThreadState_Running)
			m_DealockDetector.f_Start();
	}


	void f_Debug_NotDeadlocked()
	{
		m_DealockDetector.f_Pulse();
	}

	void f_Debug_StopDeadlockDetector()
	{
		m_DealockDetector.f_Stop(false);
		if (m_DealockDetector.m_CurrentDialogWindow)
			EndDialog(m_DealockDetector.m_CurrentDialogWindow, 1);
		m_DealockDetector.f_Stop(true);
	}

	void f_Debug_PauseDeadlockDetector()
	{
		m_DealockDetector.f_AddPauseValue();
	}
	void f_Debug_ResumeDeadlockDetector()
	{
		aint LastValue = m_DealockDetector.f_LastPauseValue();
		if (LastValue == 1 && m_TimeInternal->m_bTimeInitDone)
		{
			m_DealockDetector.f_Pulse();
		}
		DMibSafeCheck(LastValue > 0, "Pause error");
	}
	bint f_Debug_IsDeadlocked()
	{
		return m_DealockDetector.f_IsDeadlocked();
	}

	CStackTraceContext m_StackTrace;

	void f_UndecorateName(const ch8 *_pName, NStr::CStr &_Destination)
	{
		return m_StackTrace.f_UndecorateName(_pName, _Destination);
	}

	void f_UndecorateName(const ch8 *_pName, NStr::CStrNonTracked &_Destination)
	{
		return m_StackTrace.f_UndecorateName(_pName, _Destination);
	}

	void f_UndecorateName(const ch8 *_pName, ch8 *_pDestination, mint _MaxLen)
	{
		return m_StackTrace.f_UndecorateName(_pName, _pDestination, _MaxLen);
	}

	CStackTraceInfo *f_AquireStackTraceInfo(CMibCodeAddress _Address)
	{
		return m_StackTrace.f_AquireStackTraceInfo((mint)_Address);
	}

	void f_ReleaseStackTraceInfo(CStackTraceInfo *_pInfo)
	{
		return m_StackTrace.f_ReleaseStackTraceInfo((CStackTraceContext::CLocalStackTraceInfo *)_pInfo);                
	}

	//

	NTime::CTime m_FileTimeBase;

};

static inline_small CSystemWindowsMSVC *fg_GetLocalSys()
{
	return (CSystemWindowsMSVC *)fg_GetSys();
}


static bool fg_DefaultCrashDumpUserNotify(	const NStr::CStrNonTracked &_CustomMessage,
											const NStr::CStrNonTracked &_ProgramName,
											const NStr::CStrNonTracked &_SupportEmail,
											const NStr::CStrNonTracked &_FileName,
											const NStr::CStrNonTracked &_FileNameDumpMini,
											const NStr::CStrNonTracked &_FileNameDump,
											bool _bAllowContinue)
{
	CStrNonTracked MessageText;

	if (_CustomMessage.f_IsEmpty())
	{
		if (_bAllowContinue)
		{
			if (fg_GetLocalSys()->m_CrashDumpUserNotifyFormat_CanContinueMessage.f_IsEmpty())
				MessageText = CStrNonTracked::CFormat("{0} has encountered an unhandled exception. Please send the following crash log files to {1}"\
				" along with a description of what you were doing when the program crashed.\r\n\r\n"\
				"{2}\r\n"\
				"{3}\r\n"\
				"\r\nAlso please save the following crash log file for future reference:\r\n\r\n"\
				"{4}\r\n"\
				"\r\nDo you want to continue execution?") << _ProgramName << _SupportEmail << _FileName << _FileNameDumpMini << _FileNameDump;
			else
				MessageText = CStrNonTracked::CFormat(fg_GetLocalSys()->m_CrashDumpUserNotifyFormat_CanContinueMessage)
					<< _ProgramName << _SupportEmail << _FileName << _FileNameDumpMini << _FileNameDump;
		}
		else
		{
			if (fg_GetLocalSys()->m_CrashDumpUserNotifyFormat_NoContinueMessage.f_IsEmpty())
				MessageText = CStrNonTracked::CFormat("{0} has encountered an unhandled exception. Please send the following crash log files to {1}"\
				" along with a description of what you were doing when the program crashed.\r\n\r\n"\
				"{2}\r\n"\
				"{3}\r\n"\
				"\r\nAlso please save the following crash log file for future reference:\r\n\r\n"\
				"{4}\r\n") << _ProgramName << _SupportEmail << _FileName << _FileNameDumpMini << _FileNameDump;
			else
				MessageText = CStrNonTracked::CFormat(fg_GetLocalSys()->m_CrashDumpUserNotifyFormat_NoContinueMessage)
					<< _ProgramName << _SupportEmail << _FileName << _FileNameDumpMini << _FileNameDump;
		}
	}
	else
	{
		if (fg_GetLocalSys()->m_CrashDumpUserNotifyFormat_CustomMessage.f_IsEmpty())
			MessageText = CStrNonTracked::CFormat(
			"{0}\r\n\r\n"\
			"{1}\r\n"\
			"{2}\r\n"\
			"\r\nAlso please save the following crash log file for future reference:\r\n\r\n" \
			"{3}") << _CustomMessage << _FileName << _FileNameDumpMini << _FileNameDump;
		else
			MessageText = CStrNonTracked::CFormat( fg_GetLocalSys()->m_CrashDumpUserNotifyFormat_CustomMessage ) 
				<< _CustomMessage << _FileName << _FileNameDumpMini << _FileNameDump;
	}

	if (NMib::NSys::fg_ConsoleErrorOutputValid())
	{
		DMibConErrOut("{}" DMibNewLine, MessageText);
		if (_CustomMessage.f_IsEmpty())
			return false;
		else
			return true;
	}
	else
	{

		class CMessageBoxThread : public NMib::NThread::CThread
		{
		public:
			virtual ch8 const *f_GetThreadNameRaw()
			{
				return "Malterlib_MessageBoxThread";
			}
			virtual CStr f_GetThreadName()
			{
				return "Malterlib_MessageBoxThread";
			}
			CStrNonTracked m_MessageBoxText;
			CStrNonTracked m_MessageBoxHeading;
			uint32 m_MessageBoxFlags;
			uint32 m_bRet;

			aint f_Main()
			{
				m_bRet = MessageBoxW(nullptr, fg_StrToWindows<CWStrNonTracked>(m_MessageBoxText), fg_StrToWindows<CWStrNonTracked>(m_MessageBoxHeading), m_MessageBoxFlags);
				return 0;
			}
			void f_Run()
			{
				BOOL bDllHeld;
				AuxUlibIsDLLSynchronizationHeld(&bDllHeld);

				if (bDllHeld  || g_bDoneMalterlibInitAll.f_Load() < 3)
					f_Main();
				else
				{
					f_Start();
					f_Stop();
				}
			}
		};

		if (_CustomMessage.f_IsEmpty())
		{
			if (_bAllowContinue)
			{
				CMessageBoxThread MessageBox;

				MessageBox.m_MessageBoxHeading = "Exception";
				MessageBox.m_MessageBoxText = MessageText;
				MessageBox.m_MessageBoxFlags = MB_YESNO | MB_ICONERROR;
				MessageBox.m_bRet = 0;
				MessageBox.f_Run();
				return MessageBox.m_bRet == IDYES;
			}
			else
			{
				CMessageBoxThread MessageBox;

				MessageBox.m_MessageBoxHeading = "Exception";
				MessageBox.m_MessageBoxText = MessageText;
				MessageBox.m_MessageBoxFlags = MB_OK | MB_ICONERROR;
				MessageBox.m_bRet = 0;
				MessageBox.f_Run();
				return false;
			}
		}
		else
		{
			CMessageBoxThread MessageBox;

			MessageBox.m_MessageBoxHeading = "Exception";
			MessageBox.m_MessageBoxText = MessageText;
			MessageBox.m_MessageBoxFlags = MB_OK | MB_ICONERROR;
			MessageBox.m_bRet = 0;
			MessageBox.f_Run();

			return true;
		}
	}
}
