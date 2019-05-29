// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

struct CEpollEvent
{
	int m_Fd;
	uint32 m_Op:4;
	uint32 m_bSocket:1;
	epoll_event m_EpollEvent;
};

class CEpollQueue
{
private:
	NThread::CMutual mp_Lock;
	NContainer::TCVector<struct CEpollEvent> mp_lQueue;
	
public:
	
	void f_Push(struct CEpollEvent* _pEvents, mint _nEvents)
	{
		DMibLock(mp_Lock);
		
		struct CEpollEvent* pEnd = _pEvents + _nEvents;
		
		while(_pEvents < pEnd)
		{
			mp_lQueue.f_Insert(*_pEvents);
			++_pEvents;
		}
	}
	
	NContainer::TCVector<struct CEpollEvent> f_Take()
	{
		DMibLock(mp_Lock);
		return fg_Move(mp_lQueue);
	}
};


class CPOSIXImpSpecificSocketPoller::CInternal
{
public:
	CEpollQueue m_ChangeQueue;
 	NAtomic::TCAtomic<smint> m_bBreak;
	int m_EpollFd;
	int m_ReadWritePipe[2];		// Used to wake the epoll thread up.

	bool f_PushSocketEvents(CPOSIXSocket* _pSocket, bool _bForRemoval);
};

bool CPOSIXImpSpecificSocketPoller::CInternal::f_PushSocketEvents(CPOSIXSocket* _pSocket, bool _bForRemoval)
{
	EPOSIXSocketEvent EventsToRegister = _pSocket->m_RegisteredEvents;
	
	if ((EventsToRegister & EPOSIXSocketEvent_Read) || (EventsToRegister & EPOSIXSocketEvent_Write))
	{		
		struct CEpollEvent CurEvent;
		
		fg_MemClear(&CurEvent, sizeof(struct CEpollEvent));
		CurEvent.m_Fd = _pSocket->m_FD;
		CurEvent.m_EpollEvent.events = 
			EPOLLET
			| EPOLLRDHUP
			| ((EventsToRegister & EPOSIXSocketEvent_Read) ? EPOLLIN : 0)
			| ((EventsToRegister & EPOSIXSocketEvent_Write) ? EPOLLOUT : 0)
		;
		CurEvent.m_Op = _bForRemoval ? EPOLL_CTL_DEL : EPOLL_CTL_ADD;
		CurEvent.m_bSocket = true;
		CurEvent.m_EpollEvent.data.ptr = _pSocket;

		m_ChangeQueue.f_Push(&CurEvent, 1);
		
		// Wake up thread.
		char Byte = 1;
		write(m_ReadWritePipe[1], &Byte, 1);
		
		return true;
	}
	else
		return false;
}

CPOSIXImpSpecificSocketPoller::CPOSIXImpSpecificSocketPoller()
{
	mp_pInternal = fg_Construct();
	
	mp_pInternal->m_bBreak.f_Store(0);
	
	{
		// We use this pipe so we can wake up the epoll when it is waiting.
		
		int PipeRet;
		if (NLocal::g_f_pipe2)
			PipeRet = NLocal::g_f_pipe2(mp_pInternal->m_ReadWritePipe, O_NONBLOCK | O_CLOEXEC);
		else
		{
			PipeRet = pipe(mp_pInternal->m_ReadWritePipe);
			if (!PipeRet)
			{
				fcntl(mp_pInternal->m_ReadWritePipe[0], F_SETFL, fcntl(mp_pInternal->m_ReadWritePipe[0], F_GETFL) | O_NONBLOCK);
				fcntl(mp_pInternal->m_ReadWritePipe[1], F_SETFL, fcntl(mp_pInternal->m_ReadWritePipe[1], F_GETFL) | O_NONBLOCK);

				fcntl(mp_pInternal->m_ReadWritePipe[0], F_SETFD, fcntl(mp_pInternal->m_ReadWritePipe[0], F_GETFD) | FD_CLOEXEC);
				fcntl(mp_pInternal->m_ReadWritePipe[1], F_SETFD, fcntl(mp_pInternal->m_ReadWritePipe[1], F_GETFD) | FD_CLOEXEC);
			}
		}
		if (PipeRet)
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("pipe (socket poller)", errno));
		
		struct CEpollEvent CurEvent;
		fg_MemClear(&CurEvent, sizeof(struct CEpollEvent));
		CurEvent.m_Fd = mp_pInternal->m_ReadWritePipe[0];
		CurEvent.m_Op = EPOLL_CTL_ADD;
		CurEvent.m_bSocket = false;
		CurEvent.m_EpollEvent.events = EPOLLIN;
		CurEvent.m_EpollEvent.data.fd = mp_pInternal->m_ReadWritePipe[0];

		
		mp_pInternal->m_ChangeQueue.f_Push(&CurEvent, 1);
	}
	
	int (* fLocal_epoll_create1)(int __flags) __THROW;
	
	(void * &)fLocal_epoll_create1 = dlsym(RTLD_DEFAULT, "epoll_create1");
	
	if (fLocal_epoll_create1)
		mp_pInternal->m_EpollFd = fLocal_epoll_create1(EPOLL_CLOEXEC);
	else
		mp_pInternal->m_EpollFd = epoll_create(1);
	
	if (mp_pInternal->m_EpollFd == -1)
		DMibErrorNet(NMib::NPlatform::fg_FormatErrno("epoll_create (socket poller)", errno));
	
	if (!fLocal_epoll_create1)
		fcntl(mp_pInternal->m_EpollFd, F_SETFD, fcntl(mp_pInternal->m_EpollFd, F_GETFD) | FD_CLOEXEC);
}

CPOSIXImpSpecificSocketPoller::~CPOSIXImpSpecificSocketPoller()
{
	close(mp_pInternal->m_ReadWritePipe[0]);
	close(mp_pInternal->m_ReadWritePipe[1]);
	close(mp_pInternal->m_EpollFd);
}

void CPOSIXImpSpecificSocketPoller::f_RegisterSocket(CPOSIXSocket* _pSocket)
{
	if (_pSocket->m_bIsRegistered)
		DMibErrorNet("POSIX socket already registered");

	_pSocket->m_bIsRegistered = true;

	if (!mp_pInternal->f_PushSocketEvents(_pSocket, false))
		DMibErrorNet("Failed to register POSIX socket.");
}

void CPOSIXImpSpecificSocketPoller::f_DeregisterSocket(CPOSIXSocket* _pSocket)
{
	if (!_pSocket->m_bIsRegistered)
		return;

	NMib::NThread::CEvent DestroyEvent;
	DestroyEvent.f_ResetSignaled();
	_pSocket->m_pDestructionReportTo = &DestroyEvent;
	_pSocket->m_bIsRegistered = true;

	if (!mp_pInternal->f_PushSocketEvents(_pSocket, true))
		DMibErrorNet("Failed to register POSIX socket.");
	else
		DestroyEvent.f_Wait();
}

void CPOSIXImpSpecificSocketPoller::f_Run(NThread::CThread* _pThread)
{
	static const int nMaxEvents = 64;
	struct epoll_event IncomingEvents[nMaxEvents];
	int nEvents;
	int const EpollFd = mp_pInternal->m_EpollFd;
	
	NContainer::TCVector<CEpollEvent> Changes;
	
	while (mp_pInternal->m_bBreak.f_Load() == 0 &&	_pThread->f_GetState() != NThread::EThreadState_EventWantQuit)
	{
		Changes = fg_Move(mp_pInternal->m_ChangeQueue.f_Take());
		{
			int LastDeleteFD = -1;
			for (auto &CurEvent : Changes)
			{
				struct epoll_event Ev = CurEvent.m_EpollEvent;
				[[maybe_unused]] int Return = epoll_ctl(EpollFd, CurEvent.m_Op, CurEvent.m_Fd, &Ev);
				DMibFastCheck(Return != -1);

				if ((CurEvent.m_EpollEvent.events & EPOLLIN) || (CurEvent.m_EpollEvent.events & EPOLLOUT))
				{
					if (CurEvent.m_Op == EPOLL_CTL_DEL)
					{
						if (CurEvent.m_Fd != LastDeleteFD)
						{
							CPOSIXSocket* pSocket = (CPOSIXSocket*)CurEvent.m_EpollEvent.data.ptr;

							auto pDestructionReportTo = pSocket->m_pDestructionReportTo.f_Exchange(nullptr);
							if (pDestructionReportTo)
								pDestructionReportTo->f_SetSignaled();
							LastDeleteFD = CurEvent.m_Fd;
						}
					}
					else
					{
						if ((CurEvent.m_EpollEvent.events & EPOLLIN) && CurEvent.m_bSocket)
						{
							CPOSIXSocket* pSocket = (CPOSIXSocket*)CurEvent.m_EpollEvent.data.ptr;
							DMibLock(pSocket->m_Lock);
							if (pSocket->m_fOnStateChange)
								pSocket->m_fOnStateChange((ENetTCPState)pSocket->m_StateAtomic.f_Load());
						}
					}
				}
			}
		}
		
		// Wait for events
		do
		{
			nEvents = epoll_wait(EpollFd, IncomingEvents, nMaxEvents, -1);
		}
		while (nEvents == -1 && errno == EINTR)
			;

		// Process any events that have occured.
		for (int iIncomingEvent = 0; iIncomingEvent < nEvents; ++iIncomingEvent)
		{
			struct epoll_event const &CurEvent = IncomingEvents[iIncomingEvent];

			if (CurEvent.events & (EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLERR | EPOLLHUP))
			{
				if ((CurEvent.events & EPOLLIN) && CurEvent.data.fd == mp_pInternal->m_ReadWritePipe[0])
				{
					char Buf[16];
					int ReadRet;
					do
					{
						ReadRet = read(mp_pInternal->m_ReadWritePipe[0], Buf, sizeof(Buf));
					}
					while(ReadRet > 0)
						;

					continue;
				}

				CPOSIXSocket* pSocket = (CPOSIXSocket*)CurEvent.data.ptr;
				DMibLock(pSocket->m_Lock);

				ENetTCPState AddedState = ENetTCPState_None;
				// Error occured.
				if (CurEvent.events & EPOLLERR)
				{
					int Error = 0;
					socklen_t ErrorLen = sizeof(Error);
					if (getsockopt(CurEvent.data.fd, SOL_SOCKET, SO_ERROR, (void *)&Error, &ErrorLen) == 0)
					{
					    pSocket->m_CloseError = Error;
						if (!pSocket->m_CloseError)
							pSocket->m_CloseError = -1;
					}
					else
						pSocket->m_CloseError = -1;

					AddedState |= ENetTCPState_Closed;
				}

				// If we are not closed.
				if (!(AddedState & ENetTCPState_Closed) && !pSocket->m_CloseError && !pSocket->m_bNonErrorClose)
				{
					// Read even handling
					if (CurEvent.events & EPOLLIN)
					{							
						if (pSocket->m_Mode == EPOSIXSocketMode_Connect)
							AddedState |= ENetTCPState_Read;
						else if (pSocket->m_Mode == EPOSIXSocketMode_Listen)
							AddedState |= ENetTCPState_Connection;
					}

					// Write event handling
					if (CurEvent.events & EPOLLOUT)
					{			
						if (pSocket->m_Mode == EPOSIXSocketMode_Connect)
						{
							AddedState |= ENetTCPState_Write;
						}
						else if (pSocket->m_Mode == EPOSIXSocketMode_Connecting)
						{
							AddedState |= ENetTCPState_Connected;
							pSocket->m_Mode = EPOSIXSocketMode_Connect;
						}				
					}
				}

				if ((CurEvent.events & EPOLLRDHUP) && pSocket->m_CloseError == 0)
				{
					if (!pSocket->m_bRemoteCloseSignalled)
					{
						pSocket->m_bRemoteCloseSignalled = true;
						AddedState |= ENetTCPState_RemoteClosed;
					}
				}

				// Connection closed
				if ((CurEvent.events & EPOLLHUP) && pSocket->m_CloseError == 0)
				{
					if (!pSocket->m_bNonErrorClose)
					{
						pSocket->m_bNonErrorClose = true;
						AddedState |= ENetTCPState_Closed;
					}
				}

				if (AddedState)
				{ 
					pSocket->m_StateAtomic.f_FetchOr(AddedState);
					if (pSocket->m_fOnStateChange)
						pSocket->m_fOnStateChange(AddedState);
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

bool CPOSIXImpSpecificSocketContext::f_ResolveAddress(CPOSIXAddress& _oAddr, const NMib::NStr::CStr &_Address, NMib::NNetwork::ENetAddressType _PreferType)
{
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

bool CPOSIXImpSpecificSocketContext::f_GetSocketCreateParams(::NMib::NNetwork::ENetAddressType _ExpectedType, CSocketCreateParams &_oParams)
{
	return false;
}
