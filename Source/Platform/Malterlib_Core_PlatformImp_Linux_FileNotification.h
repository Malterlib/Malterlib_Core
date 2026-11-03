// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once
#include <Mib/Core/Core>

#include <sys/inotify.h>
#include <sys/epoll.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <dlfcn.h>
#include <limits.h>

using namespace NMib;
using namespace NMib::NStr;
using namespace NMib::NTime;
using namespace NMib::NMemory;
using namespace NMib::NContainer;
using namespace NMib::NFile;
using namespace NMib::NThread;
using namespace NMib::NStorage;

#include <Mib/Core/PlatformSpecific/PosixErrNo>
#include "Malterlib_Core_Platform_Linux_Optional.h"

struct CFileChangeNotificationContext
{
	CFileChangeNotificationContext();
	~CFileChangeNotificationContext();

	struct CNotification;
	struct CWatch
	{
		CWatch(int _Descriptor, CStr const &_Path, CWatch *_pParent);
		~CWatch();

		CWatch *f_GetParent() const;
		void f_SetParent(CWatch *_pParent, CStr const &_Path);
		auto &f_GetChildren();
		void f_AddReference(CNotification *_pReference);
		void f_RemoveReference(CNotification *_pReference);
		bool f_IsReferenced();
		int f_GetDescriptor();
		TCSet<CNotification *> f_GetReferences();
		int f_GetReferencesCount();
		CStr f_GetPath() const;
		CWatch *f_GetChild(CStr const &_FileName);
		bool f_HasNotification(CNotification *_pNotification);

		template <typename tf_FOnFile>
		void f_ForEachChildFileRecursive(CStr const &_BasePath, tf_FOnFile &&_fOnFile, bool _bRecursive);

		template <typename tf_FOnFile>
		void f_ForEachChildFile(tf_FOnFile &&_fOnFile, bool _bRecursive);

		CIntrusiveRefCount m_RefCount;

	private:
		friend struct CFileChangeNotificationContext;

		TCMap<CStr, bool> mp_ChildFiles;
		DMibListLinkDS_Link(CWatch, mp_Link);
		DMibListLinkDS_List(CWatch, mp_Link) mp_Children;
		CWatch *mp_pParent;
		CStr mp_FileName;
		CStr mp_Path;
		TCSet<CNotification *> mp_References;
		int mp_Descriptor;
	};

	struct CNotificationThread;

	struct CNotification
	{
		struct CChange
		{
			EFileChangeNotification m_Notification = EFileChangeNotification_Undefined;
			CStr m_Path;
			NStr::CStr m_PathFrom;

			auto operator <=> (CChange const &_Right) const noexcept = default;
		};

		struct CFindChangesContext
		{
			TCLinkedList<CChange> m_ChangesFileName;
			TCLinkedList<CChange> m_Changes;
			TCSet<CChange> m_ChangesSet;
		};

		struct CPendingRename
		{
			NTime::CStopwatch m_Stopwatch{true};
			CStr m_RelativePath;
			TCSharedPointer<CWatch> m_pWatch;
			bool m_bIsDir = false;
			TCMap<CStr, bool> m_ChildFilesSnapshot;
		};

		CNotification(CFileChangeNotificationContext *_pNotificationContext, CStr const &_BasePath);
		~CNotification();

		void f_Clear();
		void f_Cancel();
		CWatch *f_WatchPath(CWatch *_pParentWatch, CStr const &_Path, bool _bThrow);
		void f_RegisterChange(CFindChangesContext &o_Context, CStr const &_Path, EFileChangeNotification _Notification, CStr const &_RenameFrom = {});
		void f_OnEvent(CFindChangesContext &o_Context, inotify_event const &_Event, TCSharedPointer<CWatch> const &_pWatch);
		void f_OnAdded(CFindChangesContext &o_Context, CStr const &_Path, bool _bIsDir, CWatch *_pWatch);
		void f_OnRemovedFromRename(CFindChangesContext &o_Context, CPendingRename const &_PendingRename);

		DMibListLinkDS_Link(CNotification, m_Link);
		TCMap<int, TCSharedPointer<CWatch>> m_Watches;
		CStr m_BasePath;
		CFileChangeNotificationContext *m_pContext;

		NFile::CUniqueFileIdentifier m_SelfUniqueID;

		TCLinkedList<CChange> m_Changes;
		CMutual m_ChangesLock;

		TCMap<uint32, CPendingRename> m_PendingRenames;

		CSemaphoreAggregate *m_pReportTo;

		EFileChange m_Flags;
		bool m_bSelfValid = true;
	};

	struct CNotificationThread : public NMib::NThread::CThread
	{
		CNotificationThread(CFileChangeNotificationContext *_pContext);
		~CNotificationThread();

		virtual NStr::CStr f_GetThreadName();
		bool f_ReadEvents();
		void f_HandleRenameTimeouts(TCMap<CNotification *, CNotification::CFindChangesContext> &_NotificationContexts);
		aint f_Main();

		CFileChangeNotificationContext *m_pContext;
		CByteVector m_ChangesBuffer;
	};

	struct CPendingRename
	{
		TCSharedPointer<CWatch> m_pWatch;
		NTime::CStopwatch m_Stopwatch{true};
		bool m_bIsDirectory = false;
	};

	TCMap<uint32, CPendingRename> m_PendingRenames;
	umint m_nPendingNotificationRenames = 0;

	struct CPendingReParent
	{
		NTime::CStopwatch m_Stopwatch{true};
		TCMap<CStr, bool> m_ChildFiles;
	};

	TCMap<CStr, CPendingReParent> m_PendingReParents;

	int f_Inotify_AddWatch(CStr const &_Path);
	void f_Inotify_RemoveWatch(int _Descriptor);
	ssize_t f_Inotify_Read(CByteVector &_Buffer);

	CWatch &f_LinkWatch(int _WatchDescriptor, CStr const &_Path, CNotification *_pNotification, CWatch *_pParentWatch);
	void f_UnlinkWatch(TCSharedPointer<CWatch> const &_pWatch, CNotification *_pNotification, bool _bDescriptorInvalid);

	void *f_Open(CStr const &_FileName, EFileChange _OpenFlags, NMib::NThread::CSemaphoreAggregate *_pReportTo);
	void f_Close(void *_pNotification);
	bool f_Changed(void *_pNotification);
	bool f_GetNotification(void *_pNotification, CStr &_Path, EFileChangeNotification &_Notification, CStr &_PathFrom);

	int m_NotifyDescriptor;
	int m_PollDescriptor;

	int m_WakeupPipe[2]; // Used to wake the epoll thread up.

	TCMap<int, TCSharedPointer<CWatch>> m_Watches;
	DMibListLinkDS_List(CNotification, m_Link) m_Notifications;
	CMutual m_ContextLock;
	TCUniquePointer<CNotificationThread> m_pNotificationThread;
};

#include "Malterlib_Core_PlatformImp_Linux_FileNotification.hpp"
