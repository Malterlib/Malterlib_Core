// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

//#define DKTrace(...) DMibTrace("***" __VA_ARGS__)
//#define DKTraceRaw(...) DMibTraceRaw("***" __VA_ARGS__)

#define DKTrace(...)
#define DKTraceRaw(...)

class CKEventQueue
{
private:
	NThread::CMutual mp_Lock;
	NContainer::TCVector<struct kevent> mp_lQueue;

public:

	void f_Push(struct kevent* _pEvents, mint _nEvents)
	{

		DMibLock(mp_Lock);

		struct kevent* pEnd = _pEvents + _nEvents;

		while(_pEvents < pEnd)
		{
			mp_lQueue.f_Insert(*_pEvents);
			++_pEvents;
		}
	}

	NContainer::TCVector<struct kevent> f_Take()
	{
		DMibLock(mp_Lock);
		return fg_Move(mp_lQueue);
	}
};


class CPOSIXImpSpecificSocketPoller::CInternal
{
public:
	int m_KQueue;

	CKEventQueue m_ChangeQueue;

	int m_ReadWritePipe[2];		// Used to wake the kqueue thread up.

	NAtomic::TCAtomic<smint> m_bBreak;

	bool f_PushSocketEvents(CPOSIXSocket* _pSocket, bool _bForRemoval);

};

bool CPOSIXImpSpecificSocketPoller::CInternal::f_PushSocketEvents(CPOSIXSocket *_pSocket, bool _bForRemoval)
{
	int nSocketEvents = 0;
	struct kevent lSocketEvents[2];

	static uint16_t const AddFlags = EV_CLEAR | EV_ADD;
	static uint16_t const RemoveFlags = EV_CLEAR | EV_DELETE;

	uint16_t Flags = _bForRemoval ? RemoveFlags : AddFlags;

	EPOSIXSocketEvent EventsToRegister = _pSocket->m_RegisteredEvents;

	if (_bForRemoval)
	{
		DKTrace("KQueue: Removing {}.\n", _pSocket->m_FD);
	}
	else
	{
		DKTrace("KQueue: Adding {} for {}{}.\n", _pSocket->m_FD << ((EventsToRegister & EPOSIXSocketEvent_Write) ? "Write " : " ") << ((EventsToRegister & EPOSIXSocketEvent_Read) ? "Read" : "") );
	}

	if (EventsToRegister & EPOSIXSocketEvent_Read)
	{
		struct kevent& CurEvent = lSocketEvents[nSocketEvents];
		++nSocketEvents;

		fg_MemClear(&CurEvent, sizeof(struct kevent));
		CurEvent.ident = _pSocket->m_FD;
		CurEvent.filter = EVFILT_READ;
		CurEvent.flags = Flags;
		CurEvent.udata = _pSocket;
	}

	if (EventsToRegister & EPOSIXSocketEvent_Write)
	{
		struct kevent& CurEvent = lSocketEvents[nSocketEvents];
		++nSocketEvents;

		fg_MemClear(&CurEvent, sizeof(struct kevent));
		CurEvent.ident = _pSocket->m_FD;
		CurEvent.filter = EVFILT_WRITE;
		CurEvent.flags = Flags;
		CurEvent.udata = _pSocket;
	}

	if (nSocketEvents)
	{
		m_ChangeQueue.f_Push(&lSocketEvents[0], nSocketEvents);

		// Wake up thread.
		char Byte = 1;
		write(m_ReadWritePipe[1], &Byte, 1);

		return true;
	}
	else
	{
		return false;
	}
}


CPOSIXImpSpecificSocketPoller::CPOSIXImpSpecificSocketPoller()
{
	mp_pInternal = fg_Construct();

	mp_pInternal->m_bBreak.f_Store(0);

	{
		// We use this pipe so we can wake up the kqueue when it is waiting.

		int PipeRet;
		do
		{
			{
				// We need to make sure that we protect the pipes against being included in other processes
				DMibLock(fg_GetSys_POSIX()->m_ForkLock);
				PipeRet = pipe(mp_pInternal->m_ReadWritePipe);
				if (PipeRet)
					break;

				fcntl(mp_pInternal->m_ReadWritePipe[0], F_SETFL, fcntl(mp_pInternal->m_ReadWritePipe[0], F_GETFL) | O_NONBLOCK);
				fcntl(mp_pInternal->m_ReadWritePipe[1], F_SETFL, fcntl(mp_pInternal->m_ReadWritePipe[1], F_GETFL) | O_NONBLOCK);

				fcntl(mp_pInternal->m_ReadWritePipe[0], F_SETFD, fcntl(mp_pInternal->m_ReadWritePipe[0], F_GETFD) | FD_CLOEXEC);
				fcntl(mp_pInternal->m_ReadWritePipe[1], F_SETFD, fcntl(mp_pInternal->m_ReadWritePipe[1], F_GETFD) | FD_CLOEXEC);

			}
		}
		while (false);

		(void)PipeRet;
		DMibSafeCheck(PipeRet == 0, "Failed to create pipe");

		struct kevent CurEvent;
		fg_MemClear(&CurEvent, sizeof(struct kevent));
		CurEvent.ident = mp_pInternal->m_ReadWritePipe[0];
		CurEvent.filter = EVFILT_READ;
		CurEvent.flags = EV_CLEAR | EV_ADD;
		CurEvent.udata = nullptr;

		mp_pInternal->m_ChangeQueue.f_Push(&CurEvent, 1);
	}

	mp_pInternal->m_KQueue = kqueue();

	DMibSafeCheck(mp_pInternal->m_KQueue != -1, "Failed to create kqueue");
}

CPOSIXImpSpecificSocketPoller::~CPOSIXImpSpecificSocketPoller()
{
	close(mp_pInternal->m_ReadWritePipe[0]);
	close(mp_pInternal->m_ReadWritePipe[1]);
	close(mp_pInternal->m_KQueue);
}

void CPOSIXImpSpecificSocketPoller::f_RegisterSocket(CPOSIXSocket* _pSocket)
{
	if (_pSocket->m_bIsRegistered)
		DMibErrorNet("POSIX socket already registered");

	_pSocket->m_bIsRegistered = true;

	if (!mp_pInternal->f_PushSocketEvents(_pSocket, false))
		DMibErrorNet("Failed to register POSIX socket");
}

void CPOSIXImpSpecificSocketPoller::f_DeregisterSocket(CPOSIXSocket* _pSocket)
{
	if (!_pSocket->m_bIsRegistered)
		return;

	NMib::NThread::CEvent DestroyEvent;
	DestroyEvent.f_ResetSignaled();
	_pSocket->m_pDestructionReportTo = &DestroyEvent;
	_pSocket->m_bIsRegistered = false;

	if (!mp_pInternal->f_PushSocketEvents(_pSocket, true))
		DMibErrorNet("Failed to register POSIX socket.");
	else
		DestroyEvent.f_Wait();
}

void CPOSIXImpSpecificSocketPoller::f_Run(NThread::CThread* _pThread)
{
	static const int nMaxEvents = 64;
	struct kevent IncomingEvents[nMaxEvents];
	timespec PollTimeout = {0, 0};

	while (mp_pInternal->m_bBreak.f_Load() == 0 && _pThread->f_GetState() != NThread::EThreadState_EventWantQuit)
	{
		NContainer::TCVector<struct kevent> KQueueChanges = mp_pInternal->m_ChangeQueue.f_Take();

		int nEvents = kevent
			(
				mp_pInternal->m_KQueue
				, KQueueChanges.f_GetArray()
				, KQueueChanges.f_GetLen()
				, IncomingEvents
				, nMaxEvents
				, KQueueChanges.f_IsEmpty() ? nullptr : &PollTimeout
			)
		;

		for (int iEvent = 0; iEvent < nEvents; ++iEvent)
		{
			auto const &Event = IncomingEvents[iEvent];

			if (Event.ident == mp_pInternal->m_ReadWritePipe[0])
			{
				// This was just a wake up event.
				if (Event.filter == EVFILT_READ)
				{
					char Buf[16];

					int ReadRet;
					do
					{
						ReadRet = read(mp_pInternal->m_ReadWritePipe[0], Buf, sizeof(Buf));
					}
					while(ReadRet > 0)
						;
				}
				continue;
			}

			CPOSIXSocket *pSocket = (CPOSIXSocket *)Event.udata;

			DMibLock(pSocket->m_Lock);

			if (pSocket->m_CloseError || pSocket->m_bNonErrorClose)
				continue;

			auto fAddState = [&](ENetTCPState _State)
				{
					pSocket->m_StateAtomic.f_FetchOr(_State);
					if (pSocket->m_fOnStateChange)
						pSocket->m_fOnStateChange(_State);
				}
			;

			if (Event.flags & EV_ERROR)
			{
				pSocket->m_CloseError = Event.data;
				if (!pSocket->m_CloseError)
					pSocket->m_CloseError = -1;
				fAddState(ENetTCPState_Closed);
			}
			else if (Event.flags & EV_EOF)
			{
				if (Event.filter == EVFILT_WRITE)
				{
					int ErrorCode = 0;
					socklen_t ErrorCodeSize = sizeof(ErrorCode);
					int GetRet = getsockopt(pSocket->m_FD, SOL_SOCKET, SO_ERROR, &ErrorCode, &ErrorCodeSize);

					if (GetRet)
						pSocket->m_CloseError = errno;
					else if (ErrorCode)
						pSocket->m_CloseError = ErrorCode;
					else if (!pSocket->m_bNonErrorClose)
						pSocket->m_bNonErrorClose = true;

					fAddState(ENetTCPState_Closed);
				}
				else if (Event.filter == EVFILT_READ)
				{
					if (!pSocket->m_bRemoteCloseSignalled)
					{
						pSocket->m_bRemoteCloseSignalled = true;
						fAddState(ENetTCPState_RemoteClosed | ENetTCPState_Read);
					}
				}
				else
					DMibFastCheck(false); // Never get here

			}
			else if (Event.filter == EVFILT_READ)
			{
				if (pSocket->m_Mode == EPOSIXSocketMode_Connect)
					fAddState(ENetTCPState_Read);
				else if (pSocket->m_Mode == EPOSIXSocketMode_Listen)
					fAddState(ENetTCPState_Connection);
				else if (pSocket->m_Mode == EPOSIXSocketMode_Connecting)
					;
				else
					DMibFastCheck(false); // Never get here
			}
			else if (Event.filter == EVFILT_WRITE)
			{
				if (pSocket->m_Mode == EPOSIXSocketMode_Connect)
					fAddState(ENetTCPState_Write);
				else if (pSocket->m_Mode == EPOSIXSocketMode_Connecting)
				{
					if (Event.flags & EV_ADD)
					{
						// At this point the connection may have been successful or not so we check the
						// socket error code to find out which it is.

						int ErrorCode = 0;
						socklen_t ErrorCodeSize = sizeof(ErrorCode);
						int GetRet = getsockopt(pSocket->m_FD, SOL_SOCKET, SO_ERROR, &ErrorCode, &ErrorCodeSize);

						if (GetRet || ErrorCode)
						{
							if (GetRet)
								pSocket->m_CloseError = errno;
							else if (ErrorCode)
								pSocket->m_CloseError = ErrorCode;

							fAddState(ENetTCPState_Closed);
						}
						else
						{
							pSocket->m_Mode = EPOSIXSocketMode_Connect;
							fAddState(ENetTCPState_Connected);
						}
					}
				}
				else if (pSocket->m_Mode == EPOSIXSocketMode_Listen)
					;
				else
					DMibFastCheck(false); // Never get here
			}
		}

		// Process changes
		{
			uintptr_t LastDeletedFD = -1;

			for (auto const &Change : KQueueChanges)
			{
				if (Change.filter != EVFILT_READ && Change.filter != EVFILT_WRITE)
					continue;

				if (Change.flags & EV_DELETE)
				{
					if (Change.ident != LastDeletedFD)
					{
						CPOSIXSocket* pSocket = (CPOSIXSocket*)Change.udata;

						auto pToSignal = pSocket->m_pDestructionReportTo.f_Exchange(nullptr);
						if (pToSignal)
							pToSignal->f_SetSignaled();
						LastDeletedFD = Change.ident;
					}
				}
				else
				{
					if (Change.filter == EVFILT_READ && Change.udata)
					{
						CPOSIXSocket* pSocket = (CPOSIXSocket*)Change.udata;
						DMibLock(pSocket->m_Lock);
						if (pSocket->m_fOnStateChange)
							pSocket->m_fOnStateChange((NMib::NNetwork::ENetTCPState)pSocket->m_StateAtomic.f_Load());
					}
				}
			}
		}
	}
}

void CPOSIXImpSpecificSocketPoller::f_Break()
{
	mp_pInternal->m_bBreak.f_Store(1);

	char Byte = 1;
	write(mp_pInternal->m_ReadWritePipe[1], &Byte, 1);
}

class CPOSIXImpSpecificSocketContext::CInternal
{
private:
public:
};

CPOSIXImpSpecificSocketContext::CPOSIXImpSpecificSocketContext()
{
	mp_pInternal = fg_Construct();
}

CPOSIXImpSpecificSocketContext::~CPOSIXImpSpecificSocketContext()
{
}

bool CPOSIXImpSpecificSocketContext::f_CreateAddress(CPOSIXAddress& _oAddr, NMib::NNetwork::ENetAddressType _Type, void const* _pData, mint _nDataBytes)
{
	return false;
}

bool CPOSIXImpSpecificSocketContext::f_GetSocketCreateParams(::NMib::NNetwork::ENetAddressType _ExpectedType, CSocketCreateParams &_oParams)
{
	if ((uint32)_ExpectedType == 0x100)
	{
		_oParams.m_Domain = PF_SYSTEM;
		_oParams.m_Type = SOCK_DGRAM;
		_oParams.m_Protocol = SYSPROTO_CONTROL;
		return true;
	}
	else if ((uint32)_ExpectedType == 0x101)
	{
		_oParams.m_Domain = PF_SYSTEM;
		_oParams.m_Type = SOCK_STREAM;
		_oParams.m_Protocol = SYSPROTO_CONTROL;
		return true;
	}

	return false;
}

bool CPOSIXImpSpecificSocketContext::f_ResolveAddress(CPOSIXAddress& _oAddr, const NMib::NStr::CStr &_Address, NMib::NNetwork::ENetAddressType _PreferType)
{
	if (_Address.f_StartsWith("KERN_DGRAM:") || _Address.f_StartsWith("KERN_STREAM:"))
	{
		CStr Address;
		int SocketType;
		uint32 MalterlibSocketType;
		if (_Address.f_StartsWith("KERN_DGRAM:"))
		{
			Address = _Address.f_Extract(fg_StrLen("KERN_DGRAM:"));
			SocketType = SOCK_DGRAM;
			MalterlibSocketType = 0x100;
		}
		else
		{
			Address = _Address.f_Extract(fg_StrLen("KERN_STREAM:"));
			SocketType = SOCK_STREAM;
			MalterlibSocketType = 0x101;
		}

		sockaddr_ctl SockAddr;
		fg_MemClear(SockAddr);

		int fd = socket(PF_SYSTEM, SocketType, SYSPROTO_CONTROL);
		if (fd == -1)
			return false;

		auto Cleanup
			= fg_OnScopeExit
			(
				[&]()
				{
					close(fd);
				}
			)
		;

		SockAddr.sc_len = sizeof(SockAddr);
		SockAddr.sc_family = AF_SYSTEM;
		SockAddr.ss_sysaddr = AF_SYS_CONTROL;
		ctl_info CtlInfo;
		fg_MemClear(CtlInfo);
		fg_StrCopy(CtlInfo.ctl_name, Address.f_GetStr(), sizeof(CtlInfo.ctl_name));

		if (ioctl(fd, CTLIOCGINFO, &CtlInfo))
			return false;

		SockAddr.sc_id = CtlInfo.ctl_id;
		SockAddr.sc_unit = 0;

		_oAddr.f_Set((NMib::NNetwork::ENetAddressType)MalterlibSocketType, &SockAddr, sizeof(SockAddr));

		return true;
	}

	return false;
}

bool CPOSIXImpSpecificSocketContext::f_GetAddressRaw(CPOSIXAddress const &_Address, ENetAddressType _ExpectedType, void* _opRawData, mint _nDataBytes)
{
	return false;
}

CPOSIXAddress* CPOSIXImpSpecificSocketContext::f_SetAddressRaw(CPOSIXAddress* _Address, ::NMib::NNetwork::ENetAddressType _ExpectedType, void const* _pRawData, mint _nDataBytes)
{
	NMib::NSys::NNetwork::fg_FreeAddress(_Address);
	return nullptr;
}
