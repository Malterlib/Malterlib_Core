// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_PlatformImp_Linux_FileNotificationContext.h"

CFileChangeNotificationContext::CFileChangeNotificationContext()
	: m_NotifyDescriptor(-1)
	, m_PollDescriptor(-1)

{
	m_WakeupPipe[0] = -1;
	m_WakeupPipe[1] = -1;
	
	if (NLocal::g_f_inotify_init1)
		m_NotifyDescriptor = NLocal::g_f_inotify_init1(IN_NONBLOCK | IN_CLOEXEC); // This should never block, epoll takes care of that.
	else if (NLocal::g_f_inotify_init)
	{
		if (!NLocal::g_f_inotify_add_watch)
			DMibError("inotify_init(): Missing inotify_add_watch");
		if (!NLocal::g_f_inotify_rm_watch)
			DMibError("inotify_init(): Missing inotify_rm_watch");
		m_NotifyDescriptor = NLocal::g_f_inotify_init();
	}
	else
		DMibError("inotify_init(): not available on system");

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

CFileChangeNotificationContext::~CFileChangeNotificationContext()
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
	
	m_Notifications.f_DeleteAllDefiniteType();

	if (m_WakeupPipe[0] >= 0)
		close(m_WakeupPipe[0]);
	if (m_WakeupPipe[1] >= 0)
		close(m_WakeupPipe[1]);
	if (m_NotifyDescriptor)
		close(m_NotifyDescriptor);
	if (m_PollDescriptor)
		close(m_PollDescriptor);
}

int CFileChangeNotificationContext::f_Inotify_AddWatch(CStr const &_Path)
{
	uint32_t Mask = IN_ONLYDIR | IN_MODIFY | IN_ATTRIB | IN_MOVED_FROM | IN_MOVED_TO | IN_CREATE | IN_DELETE| IN_DELETE_SELF | IN_MOVE_SELF;
	int WatchDescriptor = NLocal::g_f_inotify_add_watch(m_NotifyDescriptor, _Path.f_GetStr(), Mask);
	if (WatchDescriptor < 0)
		DMibErrorFile(NMib::NPlatform::fg_FormatErrno(CStr::CFormat("inotify_add_watch('{}')") << _Path, errno));
	return WatchDescriptor;
}

void CFileChangeNotificationContext::f_Inotify_RemoveWatch(int _Descriptor)
{
	int Result = NLocal::g_f_inotify_rm_watch(m_NotifyDescriptor, _Descriptor);
	if (Result < 0)
	{
		switch (errno)
		{
			case EBADF:
				DMibError(NMib::NStr::CStrNonTracked::CFormat("inotify_rm_watch({},{}): EBADF Not a valid file descriptor") << m_NotifyDescriptor << _Descriptor);
			case EINVAL:
				return; // This can happen due to race conditions
//				DMibError(NMib::NStr::CStrNonTracked::CFormat("inotify_rm_watch({},{}): EINVAL: The inotify watch descriptor or file descriptor is not valid") << m_NotifyDescriptor << _Descriptor);
			default:
				DMibError(NMib::NPlatform::fg_FormatErrno(NMib::NStr::CStrNonTracked::CFormat("inotify_rm_watch({},{})") << m_NotifyDescriptor << _Descriptor, errno));
		}
	}
}

ssize_t CFileChangeNotificationContext::f_Inotify_Read(CByteVector &_Buffer)
{
	ssize_t ReadResult = read(m_NotifyDescriptor, _Buffer.f_GetArray(), _Buffer.f_GetLen());
	
	if (ReadResult < 0)
	{
		switch (errno)
		{
			case EINVAL:
				DMibError(NMib::NStr::CStrNonTracked::CFormat("read({}): EINVAL Buffer too small") << m_NotifyDescriptor);
			case EAGAIN:
				return 0;
				//DMibError(NMib::NStr::CStrNonTracked::CFormat("read({}): EAGAIN") << m_NotifyDescriptor);
				// The file descriptor fd refers to a file other than a socket and has
				// been marked nonblocking (O_NONBLOCK), and the read would block.
			case EBADF:
				DMibError(NMib::NStr::CStrNonTracked::CFormat("inotify read({}): EBADF: Not a valid file descriptor or is not open for reading") << m_NotifyDescriptor);
			case EFAULT:
				DMibError(NMib::NStr::CStrNonTracked::CFormat("inotify read({}): EFAULT: buf is outside your accessible address space") << m_NotifyDescriptor);
			case EINTR:
				DMibError(NMib::NStr::CStrNonTracked::CFormat("inotify read({}): EINTR: The call was interrupted by a signal before any data was read") << m_NotifyDescriptor);
			case EIO:
				DMibError(NMib::NStr::CStrNonTracked::CFormat("inotify read({}): EIO: I/O error") << m_NotifyDescriptor);
			case EISDIR:
				DMibError(NMib::NStr::CStrNonTracked::CFormat("inotify read({}): EISDIR fd refers to a directory") << m_NotifyDescriptor);
			default:
				DMibError(NMib::NPlatform::fg_FormatErrno(NMib::NStr::CStrNonTracked::CFormat("inotify read({})") << m_NotifyDescriptor, errno));
		}
	}
	return ReadResult;
}

CFileChangeNotificationContext::CWatch &CFileChangeNotificationContext::f_LinkWatch(int _WatchDescriptor, CStr const &_Path, CNotification *_pNotification, CWatch *_pParentWatch)
{
	NStorage::TCSharedPointer<CWatch> pWatch;
	
	if (auto *pExistingWatch = m_Watches.f_FindEqual(_WatchDescriptor))
	{
		pWatch = *pExistingWatch;
		if (!pWatch->f_GetParent() && _pParentWatch)
			pWatch->f_SetParent(_pParentWatch, _Path);
		
		DMibCheck(!_pParentWatch || pWatch->f_GetParent() == _pParentWatch);
	}
	else
	{
		pWatch = m_Watches(_WatchDescriptor, fg_Construct(_WatchDescriptor, _Path, _pParentWatch));
		try
		{
			// We can't do a subdirectory search directly here, that could cause a race condition, because we must add the watch before scanning a directory
			for (auto &File : NFile::CFile::fs_FindFilesEx(_Path + "/*", NFile::EFileAttrib_Directory | NFile::EFileAttrib_File, false, false))
				pWatch->m_ChildFiles[File.m_Path.f_Extract(_Path.f_GetLen() + 1)] = (File.m_Attribs & (NFile::EFileAttrib_Directory | NFile::EFileAttrib_Link)) == NFile::EFileAttrib_Directory;
		}
		catch (NException::CException const &_Exception)
		{
			DMibTrace("Failed to find files in sub watch {} ({})", _Path << _Exception.f_GetErrorStr());
		}
	}

	_pNotification->m_Watches[_WatchDescriptor] = pWatch;
	pWatch->f_AddReference(_pNotification);
	return *pWatch;
}

void CFileChangeNotificationContext::f_UnlinkWatch(TCSharedPointer<CWatch> _pWatch, CNotification *_pNotification, bool _bDescriptorInvalid)
{
	_pNotification->m_Watches.f_Remove(_pWatch->f_GetDescriptor());
	_pWatch->f_RemoveReference(_pNotification);
	
	if (!_pWatch->f_IsReferenced())
	{
		int WatchDescriptor = _pWatch->f_GetDescriptor();
		if (!_bDescriptorInvalid)
			f_Inotify_RemoveWatch(WatchDescriptor);
		m_Watches.f_Remove(WatchDescriptor);
	}
}

void *CFileChangeNotificationContext::f_Open(const CStr &_FileName, NMib::NFile::EFileChange _OpenFlags, NMib::NThread::CSemaphoreReportableAggregate *_pReportTo)
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

	auto SelfUniqueID = NFile::CFile::fs_GetUniqueIdentifier(_FileName);

	{
		DMibLock(m_ContextLock);

		NMib::NStorage::TCUniquePointer<CNotification> pNot = fg_Construct(this, FileName);
		pNot->m_pReportTo = _pReportTo;
		pNot->m_Flags = _OpenFlags;
		pNot->m_SelfUniqueID = SelfUniqueID;

		pNot->f_WatchPath(nullptr, FileName, true);

		m_Notifications.f_Insert(pNot.f_Get());
	
		return pNot.f_Detach();
	}
}

void CFileChangeNotificationContext::f_Close(void *_pNotification)
{
	DMibLock(m_ContextLock);
	CNotification *pNotification = (CNotification *)_pNotification;
	pNotification->f_Clear();

#if DMibEnableSafeCheck > 0
	for (auto &pWatch : m_Watches)
	{
		DMibFastCheck(!pWatch->mp_References.f_FindEqual(pNotification));
	}
#endif
	
	m_Notifications.f_Remove(pNotification);
	delete pNotification;
}

bool CFileChangeNotificationContext::f_Changed(void *_pNotification)
{
	DMibLock(m_ContextLock);
	CNotification *pNotification = (CNotification *)_pNotification;
	bool bChanged = false;
	{
		DMibLock(pNotification->m_ChangesLock);
		bChanged = !pNotification->m_Changes.f_IsEmpty();
		pNotification->m_Changes.f_Clear();
	}
	return bChanged;
}

bool CFileChangeNotificationContext::f_GetNotification(void *_pNotification, CStr &_Path, NFile::EFileChangeNotification &_Notification, CStr &_PathFrom)
{
	DMibLock(m_ContextLock);
	CNotification *pNotification = (CNotification *)_pNotification;
	{
		DMibLock(pNotification->m_ChangesLock);
		if (!pNotification->m_Changes.f_IsEmpty())
		{
			CNotification::CChange &Change = pNotification->m_Changes.f_GetFirst();
			_Notification = Change.m_Notification;
			_Path = fg_Move(Change.m_Path);
			_PathFrom = fg_Move(Change.m_PathFrom);
			pNotification->m_Changes.f_Remove(Change);
			return true;
		}
	}

	return false;
}
