// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once 

#include <Mib/Core/Core>

#include "Malterlib_Core_PlatformImp_Net.h"

typedef CRuntimeNetAddress CWindowsAddress;

class CWindowsSocket
{
public:
	uint32 m_Magic;
	uint32 m_Version;
	void *m_pSocket;
	TCAtomic<uint32> m_StateAtomic;

	NMib::NThread::CMutual m_Lock;

	class CAVLCompare_CTCPSocket
	{
	public:
		inline_small void const *operator () (CWindowsSocket const &_Node) const
		{
			return _Node.m_pSocket;
		}
	};

	DMibIntrusiveLink(CWindowsSocket, NIntrusive::TCAVLLink<>, m_TreeLink);

	NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)> m_OnStateChange;

	CStr m_CloseReason;

	mint m_BindAddressSize = 0;
	ENetAddressType m_BindAddressType = ENetAddressType_None;

#ifdef DTCPDelayEmulation

	NMib::NThread::CMutual m_DelayedLock;
	class CDelayedPacket
	{
	public:
		CDelayedPacket()
		{
			m_SentData = 0;
		}
		TCVector<uint8> m_Data;
		mint m_SentData;
		NTime::CTime m_SendTime;
		DMibListLinkDS_Link(CDelayedPacket, m_Link);
	};
	DMibListLinkDS_List(CDelayedPacket, m_Link) m_DelayedPackets;
	mint m_DelayedData;
	bint m_bDelayedStuffed;

	void f_UpdateDelayedSend(const NTime::CTime &_Now);
#endif

	CWindowsSocket();
	~CWindowsSocket();
	
};


class CWindowsSocketContext : public NMib::NThread::CThread
{
protected:

	// A simple async name resolver.
	class CResolver
	{
	private:
		enum EFlag
		{
			EFlag_None		= 0,
			EFlag_Done		= DMibBit(0),
			EFlag_Unwanted	= DMibBit(1),
			EFlag_Error		= DMibBit(2),
			EFlag_InProgress = DMibBit(3),
		};

		struct CResolveRequest
		{
			NStr::CStr m_Name;

			NThread::CMutual m_Lock;
				EFlag m_Flags;
				NPtr::TCUniquePointer<CWindowsAddress> m_pAddress;
				NMib::NThread::CSemaphoreReportableAggregate* m_pReportTo;
				NMib::NStr::CStr m_ErrorString;

			// Protected by CResolveThread::mp_Lock.
			CResolveRequest* m_pNext;
		};	

		CWindowsSocketContext* mp_pContext;

		NThread::CMutual mp_Lock;
			CResolveRequest* mp_pHead;	// Take from here.
			CResolveRequest* mp_pTail; 	// Add Here
	
		NMib::NThread::CEventAutoResetReportable mp_TerminateEvent;
		NMib::NThread::CEventAutoResetReportable mp_WakeEvent;

		typedef NPtr::TCUniquePointer<NThread::CThreadObject, NMem::CDefaultAllocator, TCDynamicPtr<typename NMem::CDefaultAllocator::CPtrHolder, NThread::CThreadObject>, void>  CThreadPointer;

		enum
		{
			nWorkerThreads = 2,
		};

		NContainer::TCVector<CThreadPointer> mp_lThreads;

		CResolveRequest* fp_Pop();
		aint fp_ResolveWorker(NThread::CThreadObject* _pThread);

	public:
		CResolver(CWindowsSocketContext* _pContext);
		~CResolver();

		void* f_Open(NMib::NStr::CStr const& _Name, NMib::NThread::CSemaphoreReportableAggregate* _pReportTo);
		bint f_GetResult(void *_pResolver, CWindowsAddress*& _opAddress, NMib::NStr::CStr &_Error);
		void f_Close(void* _pResolver);
	};

protected:

	void *mp_hThread;
	uint32 mp_ThreadID;
	NMib::NThread::CEvent mp_ThreadStartEvent;
	NMib::NThread::CMutual mp_ThreadStartLock;
	bint mp_bInitFailed;
	HWND mp_hReportWnd;

	NMib::NThread::CMutual mp_Lock;
		NIntrusive::TCAVLTree<CWindowsSocket::CLinkTraits_m_TreeLink, CWindowsSocket::CAVLCompare_CTCPSocket> mp_SocketTree;

	CAddressResolver mp_Resolver;

	enum
	{
		EDefaultSocketBufSize = 32*1024
	};

	void fp_ToNative(NMib::NNet::CNetAddressTCPv4 const& _InAddr, sockaddr_in& _OutAddr) const
	{
		fg_MemClear(_OutAddr);
		_OutAddr.sin_family = AF_INET;
		
		_OutAddr.sin_addr.s_addr = _InAddr.m_IP[3] << 24 | _InAddr.m_IP[2] << 16 | _InAddr.m_IP[1] << 8 |	_InAddr.m_IP[0];
		
		_OutAddr.sin_port = htons(_InAddr.m_Port);
	}

	void fp_ToNative(NMib::NNet::CNetAddressTCPv6 const& _InAddr, sockaddr_in6& _OutAddr) const
	{
		fg_MemClear(_OutAddr);
		_OutAddr.sin6_family = AF_INET6;
		
		fg_MemCopy(&_OutAddr.sin6_addr.s6_addr, &_InAddr.m_IP, sizeof(uint8) * 16);
		
		_OutAddr.sin6_port = htons(_InAddr.m_Port);
	}

	void fp_FromNative(sockaddr_in const& _InAddr, NMib::NNet::CNetAddressTCPv4 & _OutAddr) const
	{
		uint32 IP = _InAddr.sin_addr.s_addr;
		_OutAddr.m_IP[0] = 0xFF & (IP);
		_OutAddr.m_IP[1] = 0xFF & (IP >> 8);
		_OutAddr.m_IP[2] = 0xFF & (IP >> 16);
		_OutAddr.m_IP[3] = 0xFF & (IP >> 24);
		_OutAddr.m_Port = ntohs(_InAddr.sin_port);
	}

	void fp_FromNative(sockaddr_in6 const& _InAddr, NMib::NNet::CNetAddressTCPv6 & _OutAddr) const
	{
		fg_MemCopy(&_OutAddr.m_IP, &_InAddr.sin6_addr.s6_addr, sizeof(uint8) * 16);
		_OutAddr.m_Port = ntohs(_InAddr.sin6_port);
	}

	CWindowsSocket *fp_Connect(CWindowsAddress const& _Address, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange, bint _bAsyncConnect, CWindowsAddress const *_pBindAddress);

public:
	CWindowsSocketContext();
	~CWindowsSocketContext();

	void f_StopThread();
	void f_StartThread();

	void f_CheckFailed();
	void f_CheckDestroy();

	static LRESULT WINAPI fsp_SocketWindowProc(HWND _hWnd, UINT _Message, WPARAM _wParam, LPARAM _lParam);

	virtual NStr::CStr f_GetThreadName();
	virtual aint f_Main();

	bint f_IsEmpty();

	// Address
		CWindowsAddress* f_CreateAddress(NMib::NNet::ENetAddressType _Type, void const* _pData, mint _nDataBytes);
		CWindowsAddress* f_DuplicateAddress(CWindowsAddress* _Address);

		NMib::NNet::ENetAddressType f_GetAddressType(CWindowsAddress const& _pAddress);
		bint f_GetAddressRaw(CWindowsAddress const& _Address, NMib::NNet::ENetAddressType _ExpectedType, void* _opRawData, mint _nDataBytes);
		CWindowsAddress* f_SetAddressRaw(CWindowsAddress* _pAddress, ::NMib::NNet::ENetAddressType _Type, void const* _opRawData, mint _nDataBytes);

		CWindowsAddress* f_ResolveAddress(const NMib::NStr::CStr &_Address, NMib::NNet::ENetAddressType _PreferType = NMib::NNet::ENetAddressType_None);
		CWindowsAddress* f_ResolveAddress(const NMib::NStr::CStr &_Address, NMib::NNet::ENetAddressType _PreferType, bint _bThrowOnError);

		void *f_AsyncResolveAddress_Open(const NMib::NStr::CStr &_Address, ::NMib::NNet::ENetAddressType _PreferType, NMib::NFunction::TCFunction<void ()>&& _fOnFinish);
		bint f_AsyncResolveAddress_GetResult(void *_pResolver, CWindowsAddress*& _opAddress, NMib::NStr::CStr &_Error);
		void f_AsyncResolveAddress_Close(void *_pResolver);

		int f_CompareAddresses(CWindowsAddress const& _pFirst, CWindowsAddress const& _pSecond);

		void f_FreeAddress(CWindowsAddress* _pAddress); // It is OK to free a nullptr address.

		NMib::NStr::CStr f_GetAddressString(CWindowsAddress const& _Address, bint _bIncludeType);

	// Connection Operations	
		CWindowsSocket* f_Connect(CWindowsAddress const& _Address, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange, CWindowsAddress const *_pBindAddress);
		CWindowsSocket* f_AsyncConnect(CWindowsAddress const& _Address, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange, CWindowsAddress const *_pBindAddress);
		
		CWindowsSocket* f_Listen(CWindowsAddress const& _Address, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange);
		CWindowsSocket* f_ListenDatagram(CWindowsAddress const& _Address, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange);
		CWindowsSocket* f_Accept(CWindowsSocket *_pSocket, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange);

		bint f_Close(CWindowsSocket* _pSocket);
		bint f_Shutdown(CWindowsSocket *_pSocket);

		mint f_Receive(CWindowsSocket *_pSocket, void *_pData, mint _DataLen);
		mint f_Send(CWindowsSocket *_pSocket, const void *_pData, mint _DataLen);
		mint f_SendDatagram(CWindowsSocket *_pSocket, CWindowsAddress const& _Address, const void *_pData, mint _DataLen);
		mint f_ReceiveDatagram(CWindowsSocket *_pSocket, CWindowsAddress &_Address, void *_pData, mint _DataLen);

	// Socket Properties & State

		void f_SetOnStateChange(CWindowsSocket* _pSocket, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange);

		NMib::NNet::ENetTCPState f_GetState(CWindowsSocket *_pSocket);

		NMib::NStr::CStr f_GetCloseReason(CWindowsSocket* _pSocket);

		CWindowsSocket* f_InheritHandle2(void *_pOSSocket, NMib::NFunction::TCFunction<void (::NMib::NNet::ENetTCPState _StateAdded)>&& _OnStateChange);
		void *f_GiveUpForInherit(CWindowsSocket *_pSocket);
		void *f_GetOSSocket(CWindowsSocket *_pSocket);
		
		CWindowsAddress* f_GetPeerAddress(CWindowsSocket *_pSocket);	
		uint32 f_GetListenPort(CWindowsSocket *_pSocket);	

};

