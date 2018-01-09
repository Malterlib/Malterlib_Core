// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_PlatformImp_Linux_FileNotificationContext.h"

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

void CFileChangeNotificationContext::CNotificationThread::f_ReadEvents()
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
			inotify_event *pEvent = (inotify_event*)(m_ChangesBuffer.f_GetArray() + Processed);
			int EventSize = sizeof(struct inotify_event) + pEvent->len;
			Processed += EventSize;

			auto *pWatchFind = m_pContext->m_Watches.f_FindEqual(pEvent->wd);
			if (!pWatchFind)
				continue;

			auto pWatch = *pWatchFind;
			auto &Watch = *pWatch;

			for (auto iNotification = Watch.f_GetReferenceIterator(); iNotification; ++iNotification)
			{
				CNotification *pNotification = *iNotification;
				auto &Context = NotificationContexts[pNotification];
				pNotification->f_OnEvent(Context, pEvent, pWatch);

				for (auto iRename = pNotification->m_PendingRenames.f_GetIterator(); iRename;)
				{
					if (iRename.f_GetKey() == pEvent->cookie)
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
				if (iRename.f_GetKey() == pEvent->cookie)
					++iRename;
				else
				{
					iRename->m_pWatch->f_SetParent(nullptr, CStr());
					iRename.f_Remove();
				}
			}

			CStr EventPath(pEvent->name, fg_StrLen(pEvent->name, pEvent->len));
			CStr FullEventPath = CFile::fs_AppendPath(Watch.f_GetPath(), EventPath);
			
			if (pEvent->mask & IN_CREATE)
			{
				Watch.m_ChildFiles[EventPath] = CFile::fs_FileExists(FullEventPath, EFileAttrib_Directory);
			}
			else if (pEvent->mask & IN_DELETE)
				Watch.m_ChildFiles.f_Remove(EventPath);
			else if (pEvent->mask & IN_MOVED_FROM)
			{
				bool bIsDirectory = false;
				if (auto *pIsDir = Watch.m_ChildFiles.f_FindEqual(EventPath))
				{
					bIsDirectory = *pIsDir;
					Watch.m_ChildFiles.f_Remove(EventPath);
				}
				auto *pChild = Watch.f_GetChild(EventPath);
				if (pChild)
					PendingRenames[pEvent->cookie] = {fg_Explicit(pChild), bIsDirectory};
			}
			else if (pEvent->mask & IN_MOVED_TO)
			{
				if (auto pRenameFrom = PendingRenames.f_FindEqual(pEvent->cookie))
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
	

	for (auto &Context : NotificationContexts)
	{
		CNotification *pNotification = NotificationContexts.fs_GetKey(Context);
		
		if (!Context.m_ChangesFileName.f_IsEmpty() || !Context.m_Changes.f_IsEmpty())
		{
			DMibLock(pNotification->m_ChangesLock);
			pNotification->m_Changes.f_Insert(fg_Move(Context.m_ChangesFileName));
			pNotification->m_Changes.f_Insert(fg_Move(Context.m_Changes));
			if (pNotification->m_pReportTo)
				pNotification->m_pReportTo->f_Signal();
		}
	}
}

aint CFileChangeNotificationContext::CNotificationThread::f_Main()
{
	while (f_GetState() != NThread::EThreadState_EventWantQuit)
	{
		epoll_event Event;
		int Result = epoll_wait(m_pContext->m_PollDescriptor, &Event, 1, -1);
		
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
		{
			f_ReadEvents();
		}
	}
	return 0;
}
