// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

struct CEpollEvent
{
	int m_Fd;
	int m_Op;
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


class CPOSIXImpSpecificSocketPoller::CDetails
{
public:
	int m_EpollFd;
	
	CEpollQueue m_ChangeQueue;
	
	int m_ReadWritePipe[2];		// Used to wake the epoll thread up.
	
 	NAtomic::TCAtomic<smint> m_bBreak;
	
	bint f_PushSocketEvents(CPOSIXSocket* _pSocket, bint _bForRemoval);
};

bint CPOSIXImpSpecificSocketPoller::CDetails::f_PushSocketEvents(CPOSIXSocket* _pSocket, bint _bForRemoval)
{
	EPOSIXSocketEvent EventsToRegister = _pSocket->m_RegisteredEvents;
	
//	DMibTrace("f_PushSocketEvents({}, {})" DMibNewLine, (void*)_pSocket << _bForRemoval);

	if (	EventsToRegister & EPOSIXSocketEvent_Read
		||	EventsToRegister & EPOSIXSocketEvent_Write)
	{		
		struct CEpollEvent CurEvent;
		
		fg_MemClear(&CurEvent, sizeof(struct CEpollEvent));
		CurEvent.m_Fd = _pSocket->m_FD;
		CurEvent.m_EpollEvent.events = 
					EPOLLET 		// Edge triggered
				|	EPOLLRDHUP 		// Peer closed
				|	( (EventsToRegister & EPOSIXSocketEvent_Read) ? EPOLLIN : 0 )	// Read available
				|	( (EventsToRegister & EPOSIXSocketEvent_Write) ? EPOLLOUT : 0 ); // Write available
		CurEvent.m_Op = _bForRemoval ? EPOLL_CTL_DEL : EPOLL_CTL_ADD;
		CurEvent.m_EpollEvent.data.ptr = _pSocket;

		m_ChangeQueue.f_Push(&CurEvent, 1);
		
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
	mp_pD = fg_Construct();
	
	mp_pD->m_bBreak.f_Store(0);
	
	{
		// We use this pipe so we can wake up the epoll when it is waiting.
		
		int PipeRet;
		if (NLocal::g_f_pipe2)
			PipeRet = NLocal::g_f_pipe2(mp_pD->m_ReadWritePipe, O_NONBLOCK | O_CLOEXEC);
		else
		{
			PipeRet = pipe(mp_pD->m_ReadWritePipe);
			if (!PipeRet)
			{
				fcntl(mp_pD->m_ReadWritePipe[0], F_SETFL, fcntl(mp_pD->m_ReadWritePipe[0], F_GETFL) | O_NONBLOCK);
				fcntl(mp_pD->m_ReadWritePipe[1], F_SETFL, fcntl(mp_pD->m_ReadWritePipe[1], F_GETFL) | O_NONBLOCK);

				fcntl(mp_pD->m_ReadWritePipe[0], F_SETFD, fcntl(mp_pD->m_ReadWritePipe[0], F_GETFD) | FD_CLOEXEC);
				fcntl(mp_pD->m_ReadWritePipe[1], F_SETFD, fcntl(mp_pD->m_ReadWritePipe[1], F_GETFD) | FD_CLOEXEC);
			}
		}
		if (PipeRet)
			DMibErrorNet(NMib::NPlatform::fg_FormatErrno("pipe (socket poller)", errno));
		
		struct CEpollEvent CurEvent;
		fg_MemClear(&CurEvent, sizeof(struct CEpollEvent));
		CurEvent.m_Fd = mp_pD->m_ReadWritePipe[0];
		CurEvent.m_Op = EPOLL_CTL_ADD;
		CurEvent.m_EpollEvent.events = EPOLLIN;
		CurEvent.m_EpollEvent.data.fd = mp_pD->m_ReadWritePipe[0];
		
		mp_pD->m_ChangeQueue.f_Push(&CurEvent, 1);
	}
	
	int (* fLocal_epoll_create1)(int __flags) __THROW;
	
	(void * &)fLocal_epoll_create1 = dlsym(RTLD_DEFAULT, "epoll_create1");
	
	if (fLocal_epoll_create1)
		mp_pD->m_EpollFd = fLocal_epoll_create1(EPOLL_CLOEXEC);
	else
		mp_pD->m_EpollFd = epoll_create(1);
	
	if (mp_pD->m_EpollFd == -1)
		DMibErrorNet(NMib::NPlatform::fg_FormatErrno("epoll_create (socket poller)", errno));
	
	if (!fLocal_epoll_create1)
		fcntl(mp_pD->m_EpollFd, F_SETFD, fcntl(mp_pD->m_EpollFd, F_GETFD) | FD_CLOEXEC);
}

CPOSIXImpSpecificSocketPoller::~CPOSIXImpSpecificSocketPoller()
{
	close(mp_pD->m_ReadWritePipe[0]);
	close(mp_pD->m_ReadWritePipe[1]);
	close(mp_pD->m_EpollFd);
}

void CPOSIXImpSpecificSocketPoller::f_RegisterSocket(CPOSIXSocket* _pSocket)
{
//	DMibTrace("f_RegisterSocket for {}{\n}", (void*)_pSocket);
	
	if (!mp_pD->f_PushSocketEvents(_pSocket, false))
	{
		DMibErrorNet("Failed to register POSIX socket.");
	}
}

void CPOSIXImpSpecificSocketPoller::f_DeregisterSocket(CPOSIXSocket* _pSocket)
{
	NMib::NThread::CEvent DestroyEvent;
	DestroyEvent.f_ResetSignaled();
	_pSocket->m_pDestructionReportTo = &DestroyEvent;

	//DMibTrace("f_DeregisterSocket for {}{\n}", (void*)_pSocket);
	
	if (!mp_pD->f_PushSocketEvents(_pSocket, true))
		DMibErrorNet("Failed to register POSIX socket.");
	else
		DestroyEvent.f_Wait();

	//DMibTrace("FINISHED f_DeregisterSocket for {}{\n}", (void*)_pSocket);
}

void CPOSIXImpSpecificSocketPoller::f_Run(NThread::CThread* _pThread)
{
	static const int nMaxEvents = 64;
	struct epoll_event lIncomingEvents[nMaxEvents];
	int nEvents;
	int const EPollFd = mp_pD->m_EpollFd;
	
	NContainer::TCVector<struct CEpollEvent> lChanges;
	
	while (		mp_pD->m_bBreak.f_Load() == 0
		   &&	_pThread->f_GetState() != NThread::EThreadState_EventWantQuit)
	{
		// Get changes
		lChanges = fg_Move(mp_pD->m_ChangeQueue.f_Take());
		
		// Process changes
		{
			uintptr_t LastDeleteFD = -1;
			
			for (auto CIter = lChanges.f_GetIterator()
				 ;CIter
				 ;++CIter)
			{
//				DMibTrace("Processing change{\n}", 0);
				struct CEpollEvent const& CurEvent = *CIter;
				
				struct epoll_event Ev = CurEvent.m_EpollEvent;
				int Return = epoll_ctl(EPollFd, CurEvent.m_Op, CurEvent.m_Fd, &Ev);
				if (Return == -1)
				{
					//DMibTrace("{}\n", NMib::NPlatform::fg_FormatErrno("epoll_ctl", errno));
					// TODO. Handle error
				}
				if (   CurEvent.m_EpollEvent.events & EPOLLIN
					|| CurEvent.m_EpollEvent.events & EPOLLOUT)
				{
					/*
					if (CurEvent.m_Op == EPOLL_CTL_ADD)
					{
						DMibTrace("EPOLL_CTL_ADD for {}" DMibNewLine, (void*)CurEvent.m_EpollEvent.data.ptr);
					}
					else 
					 */
					if (CurEvent.m_Op == EPOLL_CTL_DEL)
					{
						if (CurEvent.m_Fd != LastDeleteFD)
						{
							CPOSIXSocket* pSocket = (CPOSIXSocket*)CurEvent.m_EpollEvent.data.ptr;
							//DMibTrace("EPOLL_CTL_DEL for {}" DMibNewLine, (void*)pSocket);
							
							auto pDestructionReportTo = pSocket->m_pDestructionReportTo.f_Exchange(nullptr);
							if (pDestructionReportTo)
								pDestructionReportTo->f_SetSignaled();
							LastDeleteFD = CurEvent.m_Fd;
						}
					}
				}
			}
		}
		
		// Wait for events
		do {
			nEvents = epoll_wait(EPollFd, lIncomingEvents, nMaxEvents, -1);
//			DMibTrace("epoll_wait woke up with nEvents: {}{\n}", nEvents);
		}
		while (nEvents == -1 && errno == EINTR);
		

		if (nEvents == -1)
		{
			// TODO. Handle error
//			DMibTraceRaw("epoll_wait error" DMibNewLine);
		}
		// Process any events that have occured.
		for (int iE = 0
			 ;iE < nEvents
			 ;++iE)
		{
			struct epoll_event const& CurEvent = lIncomingEvents[iE];
			

			if (CurEvent.events & (EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLERR | EPOLLHUP))
			{
				if (	CurEvent.events & EPOLLIN 
					&& 	CurEvent.data.fd == mp_pD->m_ReadWritePipe[0])
				{ // This was just a wake up event.
//					DMibTraceRaw("Wakeup event." DMibNewLine);
					char Buf[16];
					int ReadRet;
					do
					{
						ReadRet = read(mp_pD->m_ReadWritePipe[0], Buf, sizeof(Buf));
					} while(ReadRet > 0);

					continue;
				}

				CPOSIXSocket* pSocket = (CPOSIXSocket*)CurEvent.data.ptr;

				DMibLock(pSocket->m_Lock);

/*
								DMibTrace("***EPoll result: {} {} {} {} {}\n"
						,	( (CurEvent.events & EPOLLERR) ? "EPOLLERR" : "" )
						<<	( (CurEvent.events & EPOLLIN) ? "EPOLLIN" : "" )
						<<	( (CurEvent.events & EPOLLOUT) ? "EPOLLOUT" : "" )
						<<	( (CurEvent.events & EPOLLRDHUP) ? "EPOLLRDHUP" : "" )
						<<	( (CurEvent.events & EPOLLHUP) ? "EPOLLHUP" : "" )
						 );
*/

				ENetTCPState AddedState = ENetTCPState_None;
				// Error occured.
				if (CurEvent.events & EPOLLERR)
				{
					int Error = 0;
					socklen_t ErrorLen = sizeof(Error);
					if (getsockopt(CurEvent.data.fd, SOL_SOCKET, SO_ERROR, (void *)&Error, &ErrorLen) == 0)
					{
					    pSocket->m_CloseError = Error;
					}
					else
					{
						pSocket->m_CloseError = -1;
					}

//					DMibTrace("***EPOLLERR occured: {}\n", pSocket->m_CloseError);

					AddedState |= ENetTCPState_Closed;
				}

				// If we are not closed.
				if (! ((pSocket->m_State | AddedState) & ENetTCPState_Closed) )
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

				// Connection closed
				if (CurEvent.events & (EPOLLRDHUP | EPOLLHUP) && pSocket->m_CloseError == 0)
				{
					// DMibTrace("*** EPOLLRDHUP received for {}\n", pSocket->m_FD);
					
					AddedState |= ENetTCPState_Closed;
				}

				if (AddedState)
				{ 
					pSocket->m_State |= AddedState;
					if (pSocket->m_fOnStateChange)
						pSocket->m_fOnStateChange(AddedState);
				}
			}

		}
		
	}
}

void CPOSIXImpSpecificSocketPoller::f_Break()
{
	mp_pD->m_bBreak.f_Store(1);
	
	char Byte = 1;
	write(mp_pD->m_ReadWritePipe[1], &Byte, 1);
}

class CPOSIXImpSpecificSocketContext::CDetails
{
private:
public:
};

CPOSIXImpSpecificSocketContext::CPOSIXImpSpecificSocketContext()
{
	mp_pD = fg_Construct();
}

CPOSIXImpSpecificSocketContext::~CPOSIXImpSpecificSocketContext()
{
}

bint CPOSIXImpSpecificSocketContext::f_CreateAddress(CPOSIXAddress& _oAddr, NMib::NNetwork::ENetAddressType _Type, void const* _pData, mint _nDataBytes)
{
	return false;
}

bint CPOSIXImpSpecificSocketContext::f_ResolveAddress(CPOSIXAddress& _oAddr, const NMib::NStr::CStr &_Address, NMib::NNetwork::ENetAddressType _PreferType)
{
	return false;
}

bint CPOSIXImpSpecificSocketContext::f_GetAddressRaw(CPOSIXAddress const &_Address, ENetAddressType _ExpectedType, void* _opRawData, mint _nDataBytes)
{
	return false;
}

CPOSIXAddress* CPOSIXImpSpecificSocketContext::f_SetAddressRaw(CPOSIXAddress* _Address, ::NMib::NNetwork::ENetAddressType _ExpectedType, void const* _pRawData, mint _nDataBytes)
{
	NMib::NSys::NNetwork::fg_FreeAddress(_Address);
	return nullptr;
}


int CPOSIXImpSpecificSocketContext::f_Connect(CPOSIXAddress const &_Address) // Returns a FD or -1
{
	return -1;
}

bool CPOSIXImpSpecificSocketContext::f_GetSocketCreateParams(::NMib::NNetwork::ENetAddressType _ExpectedType, CSocketCreateParams &_oParams)
{
	return false;
}

