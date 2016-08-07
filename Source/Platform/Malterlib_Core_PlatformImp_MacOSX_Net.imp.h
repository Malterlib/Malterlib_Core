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


class CPOSIXImpSpecificSocketPoller::CDetails
{
public:
	int m_KQueue;

	CKEventQueue m_ChangeQueue;

	int m_ReadWritePipe[2];		// Used to wake the kqueue thread up.

 	NAtomic::TCAtomic<smint> m_bBreak;

	bint f_PushSocketEvents(CPOSIXSocket* _pSocket, bint _bForRemoval);

};

bint CPOSIXImpSpecificSocketPoller::CDetails::f_PushSocketEvents(CPOSIXSocket* _pSocket, bint _bForRemoval)
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
	mp_pD = fg_Construct();

	mp_pD->m_bBreak.f_Store(0);

	{
		// We use this pipe so we can wake up the kqueue when it is waiting.

		int PipeRet;
		do
		{
			{
				// We need to make sure that we protect the pipes against being included in other processes
				DMibLock(fg_GetSys_POSIX()->m_ForkLock);
				PipeRet = pipe(mp_pD->m_ReadWritePipe);
				if (PipeRet)
					break;
				
				fcntl(mp_pD->m_ReadWritePipe[0], F_SETFL, fcntl(mp_pD->m_ReadWritePipe[0], F_GETFL) | O_NONBLOCK);
				fcntl(mp_pD->m_ReadWritePipe[1], F_SETFL, fcntl(mp_pD->m_ReadWritePipe[1], F_GETFL) | O_NONBLOCK);

				fcntl(mp_pD->m_ReadWritePipe[0], F_SETFD, fcntl(mp_pD->m_ReadWritePipe[0], F_GETFD) | FD_CLOEXEC);
				fcntl(mp_pD->m_ReadWritePipe[1], F_SETFD, fcntl(mp_pD->m_ReadWritePipe[1], F_GETFD) | FD_CLOEXEC);
				
			}
		}
		while (false);
		
		(void)PipeRet;
		DMibSafeCheck(PipeRet == 0, "Failed to create pipe");

		struct kevent CurEvent;
		fg_MemClear(&CurEvent, sizeof(struct kevent));
		CurEvent.ident = mp_pD->m_ReadWritePipe[0];
		CurEvent.filter = EVFILT_READ;
		CurEvent.flags = EV_CLEAR | EV_ADD;
		CurEvent.udata = nullptr;

		mp_pD->m_ChangeQueue.f_Push(&CurEvent, 1);
	}

	mp_pD->m_KQueue = kqueue();

	DMibSafeCheck(mp_pD->m_KQueue != -1, "Failed to create kqueue");
}

CPOSIXImpSpecificSocketPoller::~CPOSIXImpSpecificSocketPoller()
{
	close(mp_pD->m_ReadWritePipe[0]);
	close(mp_pD->m_ReadWritePipe[1]);
	close(mp_pD->m_KQueue);
}

void CPOSIXImpSpecificSocketPoller::f_RegisterSocket(CPOSIXSocket* _pSocket)
{
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

	if (!mp_pD->f_PushSocketEvents(_pSocket, true))
		DMibErrorNet("Failed to register POSIX socket.");
	else
		DestroyEvent.f_Wait();
}

void CPOSIXImpSpecificSocketPoller::f_Run(NThread::CThread* _pThread)
{
	static const int nMaxEvents = 64;
	struct kevent lIncomingEvents[nMaxEvents];
	int nEvents;
	int const KQueue = mp_pD->m_KQueue;

	timespec Timeout;
	NMem::fg_MemClear(&Timeout, sizeof(Timeout)); // 0 == Poll
//	Timeout.tv_nsec = 1000000000 / 2000; // Half a millisecond

	while (		mp_pD->m_bBreak.f_Load() == 0 
			&&	_pThread->f_GetState() != NThread::EThreadState_EventWantQuit)
	{
		// Get changes
		NContainer::TCVector<struct kevent> lChanges = fg_Move(mp_pD->m_ChangeQueue.f_Take());

		timespec *pTimeout = nullptr; // By default block
		
		if (!lChanges.f_IsEmpty())
			pTimeout = &Timeout; // If we have changes, just poll to be able to finish delete changes
		
		// Query the kqueue
		nEvents = kevent(		KQueue
							,	lChanges.f_GetArray(), lChanges.f_GetLen()	// Changes
							,	lIncomingEvents, nMaxEvents // Receive events
							,	pTimeout // &Timeout (Block)
							);

		// Process any events that have occured.
		bint bError;
		for (int iE = 0
			;iE < nEvents
			;++iE)
		{
			struct kevent const& CurEvent = lIncomingEvents[iE];

			bError = (CurEvent.flags & EV_ERROR) ? true : false;


			if (CurEvent.filter == EVFILT_READ)
			{
				if (CurEvent.ident == mp_pD->m_ReadWritePipe[0])
				{ // This was just a wake up event.
					char Buf[16];
					int ReadRet;
					do
					{
						ReadRet = read(mp_pD->m_ReadWritePipe[0], Buf, sizeof(Buf));
					} while(ReadRet > 0);					
				}
				else
				{
					CPOSIXSocket* pSocket = (CPOSIXSocket*)CurEvent.udata;

					{
						DKTrace("KQueue: Read Event for {}\n", pSocket->m_FD);

						DMibLock(pSocket->m_Lock);
						
						ENetTCPState AddedState = ENetTCPState_None;

						if (pSocket->m_Mode == EPOSIXSocketMode_Connect)
						{
							if (!bError)
							{
								AddedState |= ENetTCPState_Read;
							}
							else
							{
								// Read failed
								if (pSocket->m_CloseError == 0) // We may have already errored out.
								{
									pSocket->m_CloseError = CurEvent.data;
									AddedState |= ENetTCPState_Closed;
								}
							}
						}
						else if (pSocket->m_Mode == EPOSIXSocketMode_Listen)
						{
							if (!bError)							
							{	
								AddedState |= ENetTCPState_Connection;
							}
							else
							{
								// Listening failed.
								if (pSocket->m_CloseError == 0) // We may have already errored out.
								{
									pSocket->m_CloseError = CurEvent.data;
									AddedState |= ENetTCPState_Closed;								
								}
							}
						}

/*						if (	CurEvent.flags & EV_EOF
							&& 	pSocket->m_CloseError == 0) // We may have already errored out.
						{
							pSocket->m_CloseError = CurEvent.fflags;
							AddedState |= ENetTCPState_Closed;
						}*/
						
						if (AddedState)
						{
							pSocket->m_State |= AddedState;
							if (pSocket->m_OnStateChange)
								pSocket->m_OnStateChange(AddedState);
						}
					}
				}
			}
			else if (CurEvent.filter == EVFILT_WRITE)
			{
				CPOSIXSocket* pSocket = (CPOSIXSocket*)CurEvent.udata;

				{
					DKTrace("KQueue: Write Event for {}, data: {}\n", pSocket->m_FD << CurEvent.data);

					DMibLock(pSocket->m_Lock);

					ENetTCPState AddedState = ENetTCPState_None;
					if (pSocket->m_Mode == EPOSIXSocketMode_Connect)
					{
						if (!bError)
						{
							AddedState |= ENetTCPState_Write;
						}
						else
						{
							// Write failed.
							if (pSocket->m_CloseError == 0) // We may have already errored out.
							{
								pSocket->m_CloseError = CurEvent.data;
								AddedState |= ENetTCPState_Closed;
							}
						}

						if (	CurEvent.flags & EV_EOF
							&& 	pSocket->m_CloseError == 0) // We may have already errored out.
						{
							pSocket->m_CloseError = CurEvent.fflags;
							AddedState |= ENetTCPState_Closed;
						}

					}
					else if (pSocket->m_Mode == EPOSIXSocketMode_Connecting)
					{
						if (!bError)
						{
							if (CurEvent.flags & EV_ADD)
							{
								// At this point the connection may have been successful or not so we check the
								// socket error code to find out which it is.
								
								int ErrorCode = 0;
								socklen_t ErrorCodeSize = sizeof(ErrorCode);
								int GetRet = getsockopt(pSocket->m_FD, SOL_SOCKET, SO_ERROR, &ErrorCode, &ErrorCodeSize);

								if (GetRet || ErrorCode)
								{ // Failure
									if (pSocket->m_CloseError == 0)
									{
										pSocket->m_CloseError = GetRet ? errno : ErrorCode;
										AddedState |= ENetTCPState_Closed;
									}
								}
								else
								{
									AddedState |= ENetTCPState_Connected;
									pSocket->m_Mode = EPOSIXSocketMode_Connect;
								}

							}
						}
						else
						{
							// Async connect failed
							pSocket->m_CloseError = CurEvent.data;
							AddedState |= ENetTCPState_Closed;							
						}

					}

					pSocket->m_State |= AddedState;
					
					if (pSocket->m_bInitialWriteNotification)
					{
						pSocket->m_bInitialWriteNotification = false;
					}
					else
					{
						if (AddedState)
						{
							if (AddedState && pSocket->m_OnStateChange)
								pSocket->m_OnStateChange(AddedState);
						}
					}
				}
			}
		}

		// Process changes
		{
			uintptr_t LastDeletedFD = -1;

			for (auto CIter = lChanges.f_GetIterator()
				;CIter
				;++CIter)
			{
				struct kevent const& CurEvent = *CIter;

				if (	CurEvent.filter == EVFILT_READ
					||	CurEvent.filter == EVFILT_WRITE)
				{
					if (CurEvent.flags & EV_DELETE)
					{
						if (CurEvent.ident != LastDeletedFD)
						{
							CPOSIXSocket* pSocket = (CPOSIXSocket*)CurEvent.udata;

							auto pToSignal = pSocket->m_pDestructionReportTo.f_Exchange(nullptr);
							if (pToSignal)
								pToSignal->f_SetSignaled();
							LastDeletedFD = CurEvent.ident;
						}
					}
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

bint CPOSIXImpSpecificSocketContext::f_CreateAddress(CPOSIXAddress& _oAddr, NMib::NNet::ENetAddressType _Type, void const* _pData, mint _nDataBytes)
{
	return false;
}

bool CPOSIXImpSpecificSocketContext::f_GetSocketCreateParams(::NMib::NNet::ENetAddressType _ExpectedType, CSocketCreateParams &_oParams)
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

bint CPOSIXImpSpecificSocketContext::f_ResolveAddress(CPOSIXAddress& _oAddr, const NMib::NStr::CStr &_Address, NMib::NNet::ENetAddressType _PreferType)
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
	 
		_oAddr.f_Set((NMib::NNet::ENetAddressType)MalterlibSocketType, &SockAddr, sizeof(SockAddr));
		
		return true;
	}
	
	return false;
}

bint CPOSIXImpSpecificSocketContext::f_GetAddressRaw(CPOSIXAddress const& _Address, ENetAddressType _ExpectedType, void* _opRawData, mint _nDataBytes)
{
	return false;
}

CPOSIXAddress* CPOSIXImpSpecificSocketContext::f_SetAddressRaw(CPOSIXAddress* _Address, ::NMib::NNet::ENetAddressType _ExpectedType, void const* _pRawData, mint _nDataBytes)
{
	NMib::NSys::NNet::fg_FreeAddress(_Address);
	return nullptr;
}


int CPOSIXImpSpecificSocketContext::f_Connect(CPOSIXAddress const& _Address) // Returns a FD or -1
{
	return -1;
}
