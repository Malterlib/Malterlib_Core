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

namespace NLocal
{
	extern int (* g_f_inotify_init1)(int __flags) __THROW;
	extern int (* g_f_pipe2)(int __pipedes[2], int __flags) __THROW __wur;
}

#include <Mib/Core/PlatformSpecific/PosixErrNo>

class CFileChangeNotificationContext
{
public:
	CFileChangeNotificationContext()
		: m_NotifyDescriptor(-1)
		, m_PollDescriptor(-1)

	{
		m_WakeupPipe[0] = -1;
		m_WakeupPipe[1] = -1;
		
		if (NLocal::g_f_inotify_init1)
			m_NotifyDescriptor = NLocal::g_f_inotify_init1(IN_NONBLOCK | IN_CLOEXEC); // This should never block, epoll takes care of that.
		else
			m_NotifyDescriptor = inotify_init();

		if (m_NotifyDescriptor < 0)
		{
			switch(errno)
			{
			case EINVAL:
				DMibError("inotify_init(): EINVAL: Invalid flag");
			case EMFILE:
				DMibError("inotify_init(): EMFILE: The user limit on the total number of inotify instances has been reached");
			case ENFILE:
				DMibError("inotify_init(): ENFILE: The system limit on the total number of inotify instances has been reached");
			case ENOMEM:
				DMibError("inotify_init(): ENOMEM: Insufficient kernel memory is available");
			default:
				DMibError(NMib::NStr::CStrNonTracked::CFormat("inotify_init(): Unknown error {}") << errno);
			}
		}
		
		
		if (!NLocal::g_f_inotify_init1)
		{
			fcntl(m_NotifyDescriptor, F_SETFL, fcntl(m_NotifyDescriptor, F_GETFL) | O_NONBLOCK);
			fcntl(m_NotifyDescriptor, F_SETFD, fcntl(m_NotifyDescriptor, F_GETFD) | FD_CLOEXEC);
		}

		int (* fLocal_epoll_create1)(int __flags) __THROW;
		
		(void * &)fLocal_epoll_create1 = dlsym(RTLD_DEFAULT, "epoll_create1");
		
		if (fLocal_epoll_create1)
			m_PollDescriptor = fLocal_epoll_create1(EPOLL_CLOEXEC);
		else
			m_PollDescriptor = epoll_create(1);

		if (m_PollDescriptor < 0)
		{
			switch(errno)
			{
			case EINVAL:
				DMibError("epoll_create(): EINVAL: Invalid value specified in flags");
			case EMFILE:
				DMibError("epoll_create(): EMFILE: The user limit on the total number of epoll instances has been reached");
			case ENFILE:
				DMibError("epoll_create(): ENFILE: The system limit on the total number of open files has been reached");
			case ENOMEM:
				DMibError("epoll_create(): ENOMEM: Insufficient kernel memory is available");
			default:
				DMibError(NMib::NStr::CStrNonTracked::CFormat("epoll_create(): Unknown error {}") << errno);
			}
		}

		if (!fLocal_epoll_create1)
			fcntl(m_PollDescriptor, F_SETFD, fcntl(m_PollDescriptor, F_GETFD) | FD_CLOEXEC);

		{
			epoll_event Event;
			Event.events = EPOLLIN;
			Event.data.fd = m_NotifyDescriptor;
			int Result = epoll_ctl(m_PollDescriptor, EPOLL_CTL_ADD, m_NotifyDescriptor, &Event);

			if (Result)
			{
				switch(errno)
				{
				case EBADF:
					DMibError(NMib::NStr::CStrNonTracked::CFormat("epoll_ctl({},{}): EBADF: epfd or fd is not a valid file descriptor") << m_PollDescriptor << m_NotifyDescriptor);
				case EEXIST:
					DMibError(NMib::NStr::CStrNonTracked::CFormat("epoll_ctl({},{}): EEXIST: op was EPOLL_CTL_ADD, and the supplied file descriptor fd is already registered with this epoll instance") << m_PollDescriptor << m_NotifyDescriptor);
				case EINVAL:
					DMibError(NMib::NStr::CStrNonTracked::CFormat("epoll_ctl({},{}): EINVAL: Not an epoll file descriptor, or fd is the same as epfd, or the requested operation op is not supported by this interface") << m_PollDescriptor << m_NotifyDescriptor);
				case EMFILE:
					DMibError(NMib::NStr::CStrNonTracked::CFormat("epoll_ctl({},{}): EMFILE: The user limit on the total number of epoll instances has been reached") << m_PollDescriptor << m_NotifyDescriptor);
				case ENFILE:
					DMibError(NMib::NStr::CStrNonTracked::CFormat("epoll_ctl({},{}): ENFILE: The system limit on the total number of open files has been reached") << m_PollDescriptor << m_NotifyDescriptor);
				case ENOMEM:
					DMibError(NMib::NStr::CStrNonTracked::CFormat("epoll_ctl({},{}): ENOMEM: Insufficient kernel memory is available") << m_PollDescriptor << m_NotifyDescriptor);
				case ENOENT:
					DMibError(NMib::NStr::CStrNonTracked::CFormat("epoll_ctl({},{}): ENOENT: op was EPOLL_CTL_MOD or EPOLL_CTL_DEL, and fd is not registered with this epoll instance") << m_PollDescriptor << m_NotifyDescriptor);
				case ENOSPC:
					DMibError(NMib::NStr::CStrNonTracked::CFormat("epoll_ctl({},{}): ENOSPC: The limit imposed by /proc/sys/fs/epoll/max_user_watches was encountered while trying to register (EPOLL_CTL_ADD) a new file descriptor on an epoll instance") << m_PollDescriptor << m_NotifyDescriptor);
				case EPERM:
					DMibError(NMib::NStr::CStrNonTracked::CFormat("epoll_ctl({},{}): EPERM: The target file fd does not support epoll") << m_PollDescriptor << m_NotifyDescriptor);
				default:
					DMibError(NMib::NStr::CStrNonTracked::CFormat("epoll_ctl({},{}): Unknown error {}") << m_PollDescriptor << m_NotifyDescriptor << errno);
				}
			}
		}
		
		{
			int PipeRet;
			if (NLocal::g_f_pipe2)
				PipeRet = NLocal::g_f_pipe2(m_WakeupPipe, O_NONBLOCK | O_CLOEXEC);
			else
			{
				PipeRet = pipe(m_WakeupPipe);
				if (!PipeRet)
				{
					fcntl(m_WakeupPipe[0], F_SETFL, fcntl(m_WakeupPipe[0], F_GETFL) | O_NONBLOCK);
					fcntl(m_WakeupPipe[1], F_SETFL, fcntl(m_WakeupPipe[1], F_GETFL) | O_NONBLOCK);

					fcntl(m_WakeupPipe[0], F_SETFD, fcntl(m_WakeupPipe[0], F_GETFD) | FD_CLOEXEC);
					fcntl(m_WakeupPipe[1], F_SETFD, fcntl(m_WakeupPipe[1], F_GETFD) | FD_CLOEXEC);
				}
			}
			(void)PipeRet;
			if (PipeRet != 0)
				DMibError(NMib::NPlatform::fg_FormatErrno("pipe (file notification wakeup)", errno));
			
			epoll_event Event;
			Event.events = EPOLLIN;
			Event.data.fd = m_WakeupPipe[0];
			int Result = epoll_ctl(m_PollDescriptor, EPOLL_CTL_ADD, m_WakeupPipe[0], &Event);
			if (Result)
				DMibError(NMib::NPlatform::fg_FormatErrno(NMib::NStr::CStrNonTracked::CFormat("epoll_ctl({}, {})") << m_PollDescriptor << m_WakeupPipe[0], errno));
			
		}

		m_pNotificationThread = fg_Construct(this);
		m_pNotificationThread->f_Start();
	}
	
	~CFileChangeNotificationContext()
	{
		
		if (m_pNotificationThread)
		{
			// We need to stop this first, otherwise it will have gone out of scope when it's stopped in the super class
			m_pNotificationThread->f_Stop(false);
			char Byte = 1;
			write(m_WakeupPipe[1], &Byte, 1);
			m_pNotificationThread->f_Stop(true);				
			m_pNotificationThread.f_Clear();
		}
		
		m_Notifications.f_DeleteAll();

		if (m_WakeupPipe[0] >= 0)
			close(m_WakeupPipe[0]);
		if (m_WakeupPipe[1] >= 0)
			close(m_WakeupPipe[1]);
		if (m_NotifyDescriptor)
			close(m_NotifyDescriptor);
		if (m_PollDescriptor)
			close(m_PollDescriptor);
	}
		

	int f_Inotify_AddWatch(CStr const &_Path)
	{
		uint32_t Mask = IN_ONLYDIR | IN_ALL_EVENTS;
		int WatchDescriptor = inotify_add_watch(m_NotifyDescriptor, _Path.f_GetStr(), Mask);
		if (WatchDescriptor < 0)
			DMibErrorFile(CStr::CFormat("inotify_add_watch({}) returned an error ({})") << _Path << errno);
		return WatchDescriptor;
	}

	void f_Inotify_RemoveWatch(int _Descriptor)
	{
		int Result = inotify_rm_watch(m_NotifyDescriptor, _Descriptor);
		if (Result < 0)
		{
			switch (errno)
			{
				case EBADF:
					DMibError(NMib::NStr::CStrNonTracked::CFormat("inotify_rm_watch({},{}): EBADF Not a valid file descriptor") << m_NotifyDescriptor << _Descriptor);
				case EINVAL:
					DMibError(NMib::NStr::CStrNonTracked::CFormat("inotify_rm_watch({},{}): EINVAL: The inotify watch descriptor or file descriptor is not valid") << m_NotifyDescriptor << _Descriptor);
				default:
					DMibError(NMib::NStr::CStrNonTracked::CFormat("inotify_rm_watch({},{}): Unknown error") << m_NotifyDescriptor << _Descriptor);
			}
		}

	}
	
	ssize_t f_Inotify_Read(TCVector<uint8> &_Buffer)
	{
		ssize_t ReadResult = read(m_NotifyDescriptor, _Buffer.f_GetArray(), _Buffer.f_GetLen());
		
		if (ReadResult < 0)
		{
			switch (errno)
			{
				case EINVAL:
					DMibError(NMib::NStr::CStrNonTracked::CFormat("read({}): EINVAL Buffer too small") << m_NotifyDescriptor);
				case EAGAIN:
					DMibError(NMib::NStr::CStrNonTracked::CFormat("read({}): EAGAIN") << m_NotifyDescriptor);
					// The file descriptor fd refers to a file other than a socket and has
					// been marked nonblocking (O_NONBLOCK), and the read would block.
				case EBADF:
					DMibError(NMib::NStr::CStrNonTracked::CFormat("read({}): EBADF: Not a valid file descriptor or is not open for reading") << m_NotifyDescriptor);
				case EFAULT:
					DMibError(NMib::NStr::CStrNonTracked::CFormat("read({}): EFAULT: buf is outside your accessible address space") << m_NotifyDescriptor);
				case EINTR:
					DMibError(NMib::NStr::CStrNonTracked::CFormat("read({}): EINTR: The call was interrupted by a signal before any data was read") << m_NotifyDescriptor);
				case EIO:
					DMibError(NMib::NStr::CStrNonTracked::CFormat("read({}): EIO: I/O error") << m_NotifyDescriptor);
				case EISDIR:
					DMibError(NMib::NStr::CStrNonTracked::CFormat("read({}): EISDIR fd refers to a directory") << m_NotifyDescriptor);
				default:
					DMibError(NMib::NStr::CStrNonTracked::CFormat("read({}) error {}") << m_NotifyDescriptor << errno);
			}
		}
		return ReadResult;
	}
	
	class CNotification;
	class CWatch
	{
	public:
		CWatch(int _Descriptor, CStr const &_Path):
			mp_Descriptor(_Descriptor)
			, mp_Path(_Path)
		{
		}
		~CWatch()
		{
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

	private:
		int mp_Descriptor;
		CStr mp_Path;
		TCSet<CNotification*> mp_References;
	};

	void f_LinkWatch(int WatchDescriptor, CStr const &_Path, CNotification *_pNotification)
	{
		CWatch* pWatch = nullptr;
		m_Watches.f_Lookup(WatchDescriptor, pWatch);
		if (!pWatch)
		{
			pWatch = DMibNew CWatch(WatchDescriptor, _Path);
			m_Watches[WatchDescriptor] = pWatch;
		}
		
		_pNotification->m_Watches[WatchDescriptor] = pWatch;
		pWatch->f_AddReference(_pNotification);
	}

	void f_UnlinkWatch(CWatch *_pWatch, CNotification *_pNotification, bool _bDescriptorInvalid)
	{
		_pNotification->m_Watches.f_Remove(_pWatch->f_GetDescriptor());
		_pWatch->f_RemoveReference(_pNotification);
		
		if (!_pWatch->f_IsReferenced())
		{
			int WatchDescriptor = _pWatch->f_GetDescriptor();
			if (!_bDescriptorInvalid)
				f_Inotify_RemoveWatch(WatchDescriptor);
			m_Watches.f_Remove(WatchDescriptor);
			delete _pWatch;
		}
	}

	class CNotificationThread;
	class CNotification
	{
	public:
		CNotification(CFileChangeNotificationContext* _pNotificationContext, CStr const &_BasePath):
			m_pContext(_pNotificationContext)
			, m_BasePath(_BasePath)
		{
			m_pReportTo = nullptr;
			m_Flags = NMib::NFile::EFileChange_None;
		}
		~CNotification()
		{
			f_Clear();
		}
		void f_Clear()
		{
			f_Cancel();
			m_Changes.f_DeleteAll();
		}
		void f_Cancel()
		{
			while (!m_Watches.f_IsEmpty())
				m_pContext->f_UnlinkWatch(*m_Watches.f_GetIterator(), this, false);
		}

		DMibListLinkDS_Link(CNotification, m_Link);
		TCMap<int, CWatch*> m_Watches;
		CStr m_BasePath;
		CFileChangeNotificationContext *m_pContext;
		
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
		
		NMib::NThread::CSemaphoreReportableAggregate *m_pReportTo;
		
		void f_WatchPath(CStr const &_Path, bool _bThrow)
		{
			try
			{
				int WatchDescriptor = m_pContext->f_Inotify_AddWatch(_Path);
				m_pContext->f_LinkWatch(WatchDescriptor, _Path, this);
			}
			catch (NException::CException const &_Exception)
			{
				if (_bThrow)
					throw;
				DMibTrace("Error add watch on sub path {} ({})", _Path << _Exception.f_GetErrorStr());
				return;
			}

			if ((m_Flags & NFile::EFileChange_Recursive) == 0)
				return;

			try
			{
				// We can't do a subdirectory search directly here, that could cause a race condition, because we must add the watch before scanning a directory
				NContainer::TCVector<NFile::CFile::CFoundFile> SourceDirectories;
				SourceDirectories = NFile::CFile::fs_FindFilesEx(_Path + "/*", NFile::EFileAttrib_Directory, false, false);

				for (auto iFile = SourceDirectories.f_GetIterator(); iFile; ++iFile)
					f_WatchPath(iFile->m_Path, false);
			}
			catch (NException::CException const &_Exception)
			{
				DMibTrace("Failed to find files in sub watch {} ({})", _Path << _Exception.f_GetErrorStr());
			}
		}
		
		void f_RegisterChange(CStr const &_Path, NMib::NFile::EFileChangeNotification _Notification)
		{
			CChange *pChange = DMibNew CChange;
			pChange->m_Path = _Path;
			pChange->m_Notification = _Notification;
			DMibLock(m_ChangesLock);
			m_Changes.f_Insert(pChange);
		}
		
		void f_OnEvent(inotify_event* _pEvent, CWatch* _pWatch)
		{
			if (!_pEvent || !_pWatch)
				return;

			
			CStr EventPath = _pWatch->f_GetPath();
			
			if (_pEvent->len)
				EventPath += CStr::CFormat("/{}") << CStr(_pEvent->name, _pEvent->len);
			
			CStr RelativePath = EventPath.f_Delete(0, m_BasePath.f_GetLen()+1);

			if (m_Flags & NFile::EFileChange_Recursive)
			{
				if (_pEvent->mask & IN_CREATE)
				{
					struct stat FileStat;
					if (stat(EventPath.f_GetStr(), &FileStat) != -1 && S_ISDIR(FileStat.st_mode & S_IFMT))
					{
						f_WatchPath(EventPath, false);
					}
				}
			}
			if (_pEvent->mask & IN_DELETE_SELF)
			{
				m_pContext->f_UnlinkWatch(_pWatch, this, true);
			}
			
			if (_pEvent->mask & IN_CREATE)
			{
				f_RegisterChange(RelativePath, NMib::NFile::EFileChangeNotification_Added);
			}
			
			if (_pEvent->mask & IN_DELETE)
			{
				f_RegisterChange(RelativePath, NMib::NFile::EFileChangeNotification_Removed);
			}
			
			if (_pEvent->mask & IN_MODIFY && m_Flags & NFile::EFileChange_Write)
			{
				f_RegisterChange(RelativePath, NMib::NFile::EFileChangeNotification_Modified);
			}
			
			if (_pEvent->mask & IN_MOVED_FROM && m_Flags & NFile::EFileChange_FileName)
			{
				f_RegisterChange(RelativePath, NMib::NFile::EFileChangeNotification_RenamedFrom);
			}
			
			if (_pEvent->mask & IN_MOVED_TO && m_Flags & NFile::EFileChange_FileName)
			{
				f_RegisterChange(RelativePath, NMib::NFile::EFileChangeNotification_RenamedTo);
			}

			if (m_pReportTo)
				m_pReportTo->f_Signal();
		}
	};

	class CNotificationThread : public NMib::NThread::CThread
	{
	public:
		virtual NStr::CStr f_GetThreadName()
		{
			return "Malterlib_Core_PlatformImp_FileChangeNot";
		}
		
		CFileChangeNotificationContext *m_pContext;
		TCVector<uint8> m_ChangesBuffer;
		CNotificationThread(CFileChangeNotificationContext *_pContext)
		{
			m_pContext = _pContext;
			mint MinSize = sizeof(struct inotify_event) + NAME_MAX + 1;
			m_ChangesBuffer.f_SetLen(fg_Max(64*1024, MinSize));
		}
		
		~CNotificationThread()
		{
		}
		
		void f_ReadEvents()
		{
			DMibLock(m_pContext->m_ContextLock);
			ssize_t ReadResult = m_pContext->f_Inotify_Read(m_ChangesBuffer);

			ssize_t Processed = 0;
			while (ReadResult > Processed)
			{
				inotify_event *pEvent = (inotify_event*)(m_ChangesBuffer.f_GetArray() + Processed);
				int EventSize = sizeof(struct inotify_event) + pEvent->len;
				Processed += EventSize;

				CWatch *pWatch = nullptr;
				if (!m_pContext->m_Watches.f_Lookup(pEvent->wd, pWatch))
					continue;

				for (auto iNotification = pWatch->f_GetReferenceIterator(); iNotification; ++iNotification)
				{
					CNotification* pNotification = *iNotification;
					pNotification->f_OnEvent(pEvent, pWatch);
				}
			}
		}
		
		aint f_Main()
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
		
	};
	
	int m_NotifyDescriptor;
	int m_PollDescriptor;

	int m_WakeupPipe[2];		// Used to wake the epoll thread up.
	
	TCMap<int, CWatch*> m_Watches;
	DMibListLinkDS_List(CNotification, m_Link) m_Notifications;
	NThread::CMutual m_ContextLock;
	NMib::NPtr::TCUniquePointer<CNotificationThread> m_pNotificationThread;
	
	
	void *f_Open(const CStr &_FileName, NMib::NFile::EFileChange _OpenFlags, NMib::NThread::CSemaphoreReportableAggregate *_pReportTo)
	{

		CStr FileName = _FileName;

		struct stat FileStat;
		if (stat(FileName.f_GetStr(), &FileStat) == -1)
		{
			DMibErrorFile(NMib::NPlatform::fg_FormatErrno(CStr::CFormat("stat('{}') when opening file change notification") << FileName, errno));
		}

		if (!S_ISDIR(FileStat.st_mode & S_IFMT))
		{
			DMibErrorFile(CStr::CFormat("File change notifications are not supported on files, only directories ({})") << FileName);
		}


		{
			DMibLock(m_ContextLock);

			NMib::NPtr::TCUniquePointer<CNotification> pNot = fg_Construct(this, FileName);
			pNot->m_pReportTo = _pReportTo;
			pNot->m_Flags = _OpenFlags;
			
			pNot->f_WatchPath(FileName, true);

			m_Notifications.f_Insert(pNot.f_Get());
		
			return pNot.f_Detach();
		}
	}
	
	void f_Close(void *_pNotification)
	{
		DMibLock(m_ContextLock);
		CNotification *pNotification = (CNotification *)_pNotification;
		pNotification->f_Clear();
		
		m_Notifications.f_Remove(pNotification);
		delete pNotification;
	}
	
	bint f_Changed(void *_pNotification)
	{
		DMibLock(m_ContextLock);
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
		DMibLock(m_ContextLock);
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
