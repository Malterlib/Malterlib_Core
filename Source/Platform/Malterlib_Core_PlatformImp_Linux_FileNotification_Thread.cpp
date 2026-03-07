// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_PlatformImp_Linux_FileNotification.h"

NStr::CStr CFileChangeNotificationContext::CNotificationThread::f_GetThreadName()
{
	return "Malterlib_Core_PlatformImp_FileChangeNot";
}

CFileChangeNotificationContext::CNotificationThread::CNotificationThread(CFileChangeNotificationContext *_pContext)
{
	m_pContext = _pContext;
	mint MinSize = sizeof(struct inotify_event) + NAME_MAX + 1;
	m_ChangesBuffer.f_SetLen(fg_Max(64*1024, MinSize));
}

CFileChangeNotificationContext::CNotificationThread::~CNotificationThread()
{
}

void CFileChangeNotificationContext::CNotificationThread::f_HandleRenameTimeouts(TCMap<CNotification *, CNotification::CFindChangesContext> &_NotificationContexts)
{
	for (auto &Notification : m_pContext->m_Notifications)
	{
		for (auto iRename = Notification.m_PendingRenames.f_GetIterator(); iRename;)
		{
			auto &Rename = *iRename;
			if (Rename.m_Stopwatch.f_GetTime() < 1.0)
			{
				++iRename;
				continue;
			}
			auto &Context = _NotificationContexts[&Notification];
			Notification.f_OnRemovedFromRename(Context, *iRename);
			iRename.f_Remove();
		}
	}

	auto &PendingRenames = m_pContext->m_PendingRenames;
	for (auto iRename = PendingRenames.f_GetIterator(); iRename;)
	{
		auto &Rename = *iRename;
		if (Rename.m_Stopwatch.f_GetTime() < 1.0)
		{
			++iRename;
			continue;
		}

		if (Rename.m_pWatch)
			Rename.m_pWatch->f_SetParent(nullptr, CStr());
		iRename.f_Remove();
	}

	// Clean up pending re-parent entries that never received an IN_MOVED_FROM event
	auto &PendingReParents = m_pContext->m_PendingReParents;
	for (auto iReParent = PendingReParents.f_GetIterator(); iReParent;)
	{
		if (iReParent->m_Stopwatch.f_GetTime() < 1.0)
		{
			++iReParent;
			continue;
		}
		iReParent.f_Remove();
	}
}

bool CFileChangeNotificationContext::CNotificationThread::f_ReadEvents()
{
	DMibLock(m_pContext->m_ContextLock);
	TCMap<CNotification *, CNotification::CFindChangesContext> NotificationContexts;
	auto &PendingRenames = m_pContext->m_PendingRenames;

	while (true)
	{
		ssize_t ReadResult = m_pContext->f_Inotify_Read(m_ChangesBuffer);
		if (!ReadResult)
			break;

		ssize_t Processed = 0;
		while (ReadResult > Processed)
		{
			inotify_event &Event = *((inotify_event*)(m_ChangesBuffer.f_GetArray() + Processed));
			int EventSize = sizeof(struct inotify_event) + Event.len;
			Processed += EventSize;

			auto *pWatchFind = m_pContext->m_Watches.f_FindEqual(Event.wd);
			if (!pWatchFind)
				continue;

			auto pWatch = *pWatchFind;
			auto &Watch = *pWatch;

			auto References = Watch.f_GetReferences();

			for (auto *pNotification : References)
			{
				auto &Context = NotificationContexts[pNotification];
				pNotification->f_OnEvent(Context, Event, pWatch);

				for (auto iRename = pNotification->m_PendingRenames.f_GetIterator(); iRename;)
				{
					if (iRename.f_GetKey() == Event.cookie)
						++iRename;
					else
					{
						pNotification->f_OnRemovedFromRename(Context, *iRename);
						iRename.f_Remove();
					}
				}
			}

			for (auto iRename = PendingRenames.f_GetIterator(); iRename;)
			{
				if (iRename.f_GetKey() == Event.cookie)
					++iRename;
				else
				{
					iRename->m_pWatch->f_SetParent(nullptr, CStr());
					iRename.f_Remove();
				}
			}

			CStr EventPath(Event.name, fg_StrLen(Event.name, Event.len));
			CStr FullEventPath = CFile::fs_AppendPath(Watch.f_GetPath(), EventPath);

			if (Event.mask & IN_CREATE)
			{
				Watch.m_ChildFiles[EventPath] = CFile::fs_FileExists(FullEventPath, EFileAttrib_Directory);
			}
			else if (Event.mask & IN_DELETE)
				Watch.m_ChildFiles.f_Remove(EventPath);
			else if (Event.mask & IN_MOVED_FROM)
			{
				bool bIsDirectory = false;
				if (auto *pIsDir = Watch.m_ChildFiles.f_FindEqual(EventPath))
				{
					bIsDirectory = *pIsDir;
					Watch.m_ChildFiles.f_Remove(EventPath);
				}
				auto *pChild = Watch.f_GetChild(EventPath);
				if (pChild)
					PendingRenames[Event.cookie] = {fg_Explicit(pChild), bIsDirectory};
			}
			else if (Event.mask & IN_MOVED_TO)
			{
				if (auto pRenameFrom = PendingRenames.f_FindEqual(Event.cookie))
				{
					Watch.m_ChildFiles[EventPath] = pRenameFrom->m_bIsDirectory;
					pRenameFrom->m_pWatch->f_SetParent(&Watch, FullEventPath);
					PendingRenames.f_Remove(pRenameFrom);
				}
				else
					Watch.m_ChildFiles[EventPath] = CFile::fs_FileExists(FullEventPath, EFileAttrib_Directory);
			}
		}
	}

	f_HandleRenameTimeouts(NotificationContexts);

	for (auto &Context : NotificationContexts)
	{
		CNotification *pNotification = NotificationContexts.fs_GetKey(Context);

		if (!Context.m_ChangesFileName.f_IsEmpty() || !Context.m_Changes.f_IsEmpty())
		{
			DMibLock(pNotification->m_ChangesLock);

#ifdef DMibFileChangeNotificationsDebug
			auto fNotificationToStr = [](EFileChangeNotification _Notification) -> CStr
				{
					switch (_Notification)
					{
					case EFileChangeNotification_Undefined: return "Undefined";
					case EFileChangeNotification_Unknown: return "Unknown";
					case EFileChangeNotification_Added: return "Added";
					case EFileChangeNotification_Removed: return "Removed";
					case EFileChangeNotification_Modified: return "Modified";
					case EFileChangeNotification_Renamed: return "Renamed";
					}

					return "";
				}
			;

			for (auto &Test : Context.m_ChangesFileName)
				DMibFileChangeNotificationsDebugOut("--- {} {} {}", fNotificationToStr(Test.m_Notification), Test.m_Path, Test.m_PathFrom);
			for (auto &Test : Context.m_Changes)
				DMibFileChangeNotificationsDebugOut("--- {} {} {}", fNotificationToStr(Test.m_Notification), Test.m_Path, Test.m_PathFrom);
#endif

			pNotification->m_Changes.f_Insert(fg_Move(Context.m_ChangesFileName));
			pNotification->m_Changes.f_Insert(fg_Move(Context.m_Changes));
			if (pNotification->m_pReportTo)
				pNotification->m_pReportTo->f_Signal();
		}
	}

	return !m_pContext->m_PendingRenames.f_IsEmpty() || m_pContext->m_nPendingNotificationRenames != 0 || !m_pContext->m_PendingReParents.f_IsEmpty();
}

aint CFileChangeNotificationContext::CNotificationThread::f_Main()
{
	bool bNeedTimeout = false;
	while (f_GetState() != NThread::EThreadState_EventWantQuit)
	{
		epoll_event Event;
		int Result = epoll_wait(m_pContext->m_PollDescriptor, &Event, 1, bNeedTimeout ? 1000 : -1);

		if (Result == -1)
		{
			switch(errno)
			{
			case EINTR:	//	The call was interrupted by a signal handler before either any of the requested events occurred or the timeout expired
				continue;
			case EBADF:
				DMibError(NMib::NStr::CStrNonTracked::CFormat("epoll_wait({}): EBADF: Invalid file descriptor") << m_pContext->m_PollDescriptor);
			case EFAULT:
				DMibError(NMib::NStr::CStrNonTracked::CFormat("epoll_wait({}): EFAULT: The memory area pointed to by events is not accessible with write permissions") << m_pContext->m_PollDescriptor);
			case EINVAL:
				DMibError(NMib::NStr::CStrNonTracked::CFormat("epoll_wait({}): EINVAL: epfd is not an epoll file descriptor, or maxevents is less than or equal to zero") << m_pContext->m_PollDescriptor);
			default:
				DMibError(NMib::NStr::CStrNonTracked::CFormat("epoll_wait({}): unknown error {}") << m_pContext->m_PollDescriptor << errno);
			}
		}
		else if (Result && Event.data.fd == m_pContext->m_NotifyDescriptor)
			bNeedTimeout = f_ReadEvents();
		else
			bNeedTimeout = f_ReadEvents();
	}
	return 0;
}
