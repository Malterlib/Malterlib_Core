// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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
using namespace NMib::NMem;
using namespace NMib::NContainer;
using namespace NMib::NFile;
using namespace NMib::NThread;
using namespace NMib::NPtr;

#include <Mib/Core/PlatformSpecific/PosixErrNo>
#include "Malterlib_Core_Platform_Linux_Optional.h"

class CFileChangeNotificationContext
{
public:
	CFileChangeNotificationContext();
	~CFileChangeNotificationContext();
		
	class CNotification;
	class CWatch : public TCSharedPointerIntrusiveBase<>
	{
	public:
		CWatch(int _Descriptor, CStr const &_Path, CWatch *_pParent)
			: mp_Descriptor(_Descriptor)
			, mp_Path(_Path)
			, mp_FileName(CFile::fs_GetFile(_Path))
			, mp_pParent(_pParent)
		{
			if (mp_pParent)
				mp_pParent->mp_Children.f_Insert(*this);
		}
		
		~CWatch()
		{
		}
		
		CWatch *f_GetParent() const
		{
			return mp_pParent;
		}
		
		void f_SetParent(CWatch *_pParent, CStr const &_Path)
		{
			mp_Link.f_Unlink();
			mp_pParent = _pParent;
			if (mp_pParent)
			{
				mp_FileName = CFile::fs_GetFile(_Path);
				mp_Path = _Path;
				mp_pParent->mp_Children.f_Insert(this);
			}
		}
		
		auto &f_GetChildren()
		{
			return mp_Children;
		}
		
		void f_AddReference(CNotification *_pReference)
		{
			mp_References[_pReference] = _pReference;
		}
		
		void f_RemoveReference(CNotification *_pReference)
		{
			mp_References.f_Remove(_pReference);
		}
		
		bool f_IsReferenced()
		{
			return !mp_References.f_IsEmpty();
		}
		
		int f_GetDescriptor()
		{
			return mp_Descriptor;
		}
		
		TCSet<CNotification*>::CIterator f_GetReferenceIterator()
		{
			return mp_References.f_GetIterator();
		}
		
		int f_GetReferencesCount()
		{
			return mp_References.f_GetLen();
		}
		
		CStr f_GetPath() const
		{
			return mp_Path;
		}
		
		CWatch *f_GetChild(CStr const &_FileName)
		{
			for (auto &Child : mp_Children)
			{
				if (Child.mp_FileName == _FileName)
					return &Child;
			}
			return nullptr;
		}

		TCMap<CStr, bool> m_ChildFiles;

		template <typename tf_FOnFile>
		void fr_ForEachChildFile(CStr const &_BasePath, tf_FOnFile &&_fOnFile, bool _bRecursive)
		{
			for (auto &bIsDir : m_ChildFiles)
				_fOnFile(CFile::fs_AppendPath(_BasePath, m_ChildFiles.fs_GetKey(bIsDir)), bIsDir);
			
			if (!_bRecursive)
				return;
			
			for (auto &Child : mp_Children)
				Child.fr_ForEachChildFile(CFile::fs_AppendPath(_BasePath, Child.mp_FileName), _fOnFile, true);
		}
		
		template <typename tf_FOnFile>
		void f_ForEachChildFile(tf_FOnFile &&_fOnFile, bool _bRecursive)
		{
			fr_ForEachChildFile("", _fOnFile, _bRecursive);
		}
		
	public:
		DMibListLinkDS_Link(CWatch, mp_Link);
		DMibListLinkDS_List(CWatch, mp_Link) mp_Children;
		CWatch *mp_pParent;
		CStr mp_FileName;
		CStr mp_Path;
		TCSet<CNotification*> mp_References;
		int mp_Descriptor;
	};

	class CNotificationThread;
	
	class CNotification
	{
	public:
		class CChange
		{
		public:
			EFileChangeNotification m_Notification = EFileChangeNotification_Undefined;
			CStr m_Path;
			NStr::CStr m_PathFrom;
			
			bool operator < (CChange const &_Right) const
			{
				return fg_TupleReferences(m_Notification, m_Path, m_PathFrom) < fg_TupleReferences(_Right.m_Notification, _Right.m_Path, _Right.m_PathFrom);
			}
		};
		
		struct CFindChangesContext
		{
			TCLinkedList<CChange> m_ChangesFileName;
			TCLinkedList<CChange> m_Changes;
			TCSet<CChange> m_ChangesSet;
		};
		
		struct CPendingRename
		{
			NTime::CClock m_Clock{true};
			CStr m_RelativePath;
			TCSharedPointer<CWatch> m_pWatch;
			bool m_bIsDir = false;
		};
		
		CNotification(CFileChangeNotificationContext* _pNotificationContext, CStr const &_BasePath);
		~CNotification();
		
		void f_Clear();
		void f_Cancel();
		CWatch *f_WatchPath(CWatch *_pParentWatch, CStr const &_Path, bool _bThrow);
		void f_RegisterChange(CFindChangesContext &o_Context, CStr const &_Path, EFileChangeNotification _Notification, CStr const &_RenameFrom = {});
		void f_OnEvent(CFindChangesContext &o_Context, inotify_event const &_Event, TCSharedPointer<CWatch> const &_pWatch);
		void f_OnAdded(CFindChangesContext &o_Context, CStr const &_Path, bool _bIsDir, CWatch* _pWatch);
		void f_OnRemovedFromRename(CFindChangesContext &o_Context, CPendingRename const &_PendingRename);

		DMibListLinkDS_Link(CNotification, m_Link);
		TCMap<int, TCSharedPointer<CWatch>> m_Watches;
		CStr m_BasePath;
		CFileChangeNotificationContext *m_pContext;
		
		EFileChange m_Flags;
		
		TCLinkedList<CChange> m_Changes;
		CMutual m_ChangesLock;
		
		TCMap<uint32, CPendingRename> m_PendingRenames;
		
		CSemaphoreReportableAggregate *m_pReportTo;
		
	};

	class CNotificationThread : public NMib::NThread::CThread
	{
	public:
		CNotificationThread(CFileChangeNotificationContext *_pContext);		
		~CNotificationThread();
		
		virtual NStr::CStr f_GetThreadName();
		bool f_ReadEvents();
		void f_HandleRenameTimeouts(TCMap<CNotification *, CNotification::CFindChangesContext> &_NotificationContexts);
		aint f_Main();
		
		CFileChangeNotificationContext *m_pContext;
		TCVector<uint8> m_ChangesBuffer;
	};

	struct CPendingRename
	{
		TCSharedPointer<CWatch> m_pWatch;
		NTime::CClock m_Clock{true};
		bool m_bIsDirectory = false;
	};
	TCMap<uint32, CPendingRename> m_PendingRenames;
	mint m_nPendingNotificationRenames = 0;
	
	int f_Inotify_AddWatch(CStr const &_Path);
	void f_Inotify_RemoveWatch(int _Descriptor);
	ssize_t f_Inotify_Read(TCVector<uint8> &_Buffer);
	
	CWatch &f_LinkWatch(int _WatchDescriptor, CStr const &_Path, CNotification *_pNotification, CWatch *_pParentWatch);
	void f_UnlinkWatch(TCSharedPointer<CWatch> _pWatch, CNotification *_pNotification, bool _bDescriptorInvalid);

	void *f_Open(const CStr &_FileName, EFileChange _OpenFlags, NMib::NThread::CSemaphoreReportableAggregate *_pReportTo);
	void f_Close(void *_pNotification);
	bint f_Changed(void *_pNotification);
	bint f_GetNotification(void *_pNotification, CStr &_Path, EFileChangeNotification &_Notification, CStr &_PathFrom);
	
	int m_NotifyDescriptor;
	int m_PollDescriptor;

	int m_WakeupPipe[2];		// Used to wake the epoll thread up.
	
	TCMap<int, TCSharedPointer<CWatch>> m_Watches;
	DMibListLinkDS_List(CNotification, m_Link) m_Notifications;
	CMutual m_ContextLock;
	TCUniquePointer<CNotificationThread> m_pNotificationThread;
};
