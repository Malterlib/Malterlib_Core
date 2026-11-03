// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

CAddressResolver::CResolveRequest::~CResolveRequest()
{
	NMib::NSys::NNetwork::fg_FreeAddress(m_pAddress);
}

CAddressResolver::~CAddressResolver()
{
	f_Stop();
}

// Stop and join the worker before owner teardown that its requests may still access.
void CAddressResolver::f_Stop()
{
	NThread::CThreadObject *pThread;
	{
		DMibLock(mp_Lock);
		mp_bStopped = true;
		pThread = mp_pThread.f_Get();
	}

	if (pThread)
	{
		pThread->f_Stop(true);
		DMibLock(mp_Lock);
		mp_pThread.f_Clear();
	}

	while (true)
	{
		CResolveRequest *pRequest;
		{
			DMibLock(mp_Lock);
			pRequest = mp_PendingList.f_Pop();
			if (!pRequest)
				pRequest = mp_DoneOrInProgressList.f_Pop();
		}

		if (!pRequest)
			break;

		fsp_CloseRequest(pRequest);
	}
}

void *CAddressResolver::f_Open(NMib::NStr::CStr const &_Name, ::NMib::NNetwork::ENetAddressType _PreferType, NMib::NFunction::TCFunctionMutable<void ()> &&_fOnFinish)
{
	NStorage::TCUniquePointer<CResolveRequest> pReq = fg_Construct();

	pReq->m_Name = _Name;
	pReq->m_Flags = EFlag_Pending;
	pReq->m_fOnFinish = fg_Move(_fOnFinish);
	pReq->m_pAddress = nullptr;
	pReq->m_PreferType = _PreferType;

	CResolveRequest *pRet = nullptr;

	{
		DMibLock(mp_Lock);
		if (mp_bStopped)
			DMibErrorNet("Address resolver has stopped");

		if (!mp_pThread)
		{
			mp_pThread = NThread::CThreadObject::fs_StartThread
				(
					[this](NThread::CThreadObject *_pThread) -> aint
					{
						return fp_ResolveWorker(_pThread);
					}
					, "Async Resolver Worker"
				)
			;
		}

		mp_PendingList.f_Push(pRet = pReq.f_Detach());
		mp_pThread->m_EventWantQuit.f_Signal();
	}

	return pRet;
}

bool CAddressResolver::f_GetResult(void *_pResolver, NMib::NSys::NNetwork::CAddress &o_pAddress, NMib::NStr::CStr &_Error)
{
	CResolveRequest *pReq = (CResolveRequest *)_pResolver;

	DMibLock(mp_Lock);
	if (pReq->m_Flags & EFlag_Done)
	{
		o_pAddress = pReq->m_pAddress;
		pReq->m_pAddress = nullptr;
		return true;
	}
	else if (pReq->m_Flags & EFlag_Error)
	{
		_Error = pReq->m_ErrorString;
		return false;
	}
	else
	{
		_Error = "Name resolution not yet complete.";
		return false;
	}
}

void CAddressResolver::f_Close(void *_pResolver)
{
	{
		DMibLock(mp_Lock);
		if (mp_pThread && mp_pThread->f_CallingFromThread())
			DMibErrorNet("Use f_CloseAsync from a resolver callback");
	}

	NThread::CEventAutoReset Closed;
	f_CloseAsync
		(
			_pResolver
			, [&Closed]
			{
				Closed.f_Signal();
			}
		)
	;

	Closed.f_Wait();
}

aint CAddressResolver::fp_ResolveWorker(NThread::CThreadObject *_pThread)
{
	while (_pThread->f_GetState() != NThread::EThreadState_EventWantQuit)
	{
		while (true)
		{
			CResolveRequest *pReq;

			{
				DMibLock(mp_Lock);
				pReq = mp_PendingList.f_Pop();
				if (!pReq)
					break;

				mp_DoneOrInProgressList.f_Push(pReq);
				pReq->m_Flags &= ~EFlag_Pending;
				pReq->m_Flags |= EFlag_Running;
			}

			auto Complete = g_OnScopeExit / [this, pReq]
				{
					{
						DMibLock(mp_Lock);
						pReq->m_Flags &= ~EFlag_Running;
						if (!(pReq->m_Flags & EFlag_Closing))
							return;

						mp_DoneOrInProgressList.f_Remove(pReq);
					}

					fsp_CloseRequest(pReq);
				}
			;

			NMib::NSys::NNetwork::CAddress pAddress = nullptr;
			NStr::CStr Error;

			try
			{
				pAddress = NMib::NSys::NNetwork::fg_ResolveAddress(pReq->m_Name, pReq->m_PreferType);
			}
			catch (NMib::NNetwork::CExceptionNet const &_Error)
			{
				Error = _Error.f_GetErrorStr();
			}

			NMib::NFunction::TCFunctionMutable<void ()> fOnFinish;

			{
				DMibLock(mp_Lock);
				pReq->m_pAddress = pAddress;
				pReq->m_ErrorString = fg_Move(Error);
				pReq->m_Flags |= pAddress ? EFlag_Done : EFlag_Error;

				if (!(pReq->m_Flags & EFlag_Closing))
					fOnFinish = fg_Move(pReq->m_fOnFinish);
			}

			if (fOnFinish)
				fOnFinish();
		}

		_pThread->m_EventWantQuit.f_Wait();
	}

	return 0;
}

bool CAddressResolver::f_IsEmpty()
{
	DMibLock(mp_Lock);
	return mp_PendingList.f_IsEmpty() && mp_DoneOrInProgressList.f_IsEmpty();
}

ch8 const *CUnixAddress::f_GetPath() const
{
	return m_UnixAddress.sun_path;
}

NStorage::TCOptional<CUnixAddress> CUnixAddress::fs_Parse(NMib::NStr::CStr const &_Address, bool _bThrowOnError)
{
	using namespace NMib::NFile;

	EFileAttrib Permissions = EFileAttrib_None;
	CStr Address;

	if (_Address.f_StartsWith("UNIX("))
	{
		auto *pParse = _Address.f_GetStr() + 5;
		{
			bool bFailed = false;
			uint32 UnixPermissions = fg_StrToIntParse(pParse, uint32(01000), "),:", false, EStrToIntParseMode_Octal, &bFailed);

			if (bFailed)
			{
				if (_bThrowOnError)
					DMibErrorNet("Failed to parse unix permissions");
				else
					return {};
			}

			if (UnixPermissions >= uint32(01000))
			{
				if (_bThrowOnError)
					DMibErrorNet("Invalid permissions specified");
				else
					return {};
			}

			if (UnixPermissions & 0100)
				Permissions |= EFileAttrib_UserExecute;
			if (UnixPermissions & 0200)
				Permissions |= EFileAttrib_UserWrite;
			if (UnixPermissions & 0400)
				Permissions |= EFileAttrib_UserRead;

			if (UnixPermissions & 010)
				Permissions |= EFileAttrib_GroupExecute;
			if (UnixPermissions & 020)
				Permissions |= EFileAttrib_GroupWrite;
			if (UnixPermissions & 040)
				Permissions |= EFileAttrib_GroupRead;

			if (UnixPermissions & 01)
				Permissions |= EFileAttrib_EveryoneExecute;
			if (UnixPermissions & 02)
				Permissions |= EFileAttrib_EveryoneWrite;
			if (UnixPermissions & 04)
				Permissions |= EFileAttrib_EveryoneRead;
		}

		{
			if (pParse[0] != ')' || pParse[1] != ':')
			{
				if (_bThrowOnError)
					DMibErrorNet("Failed to parse unix permissions");
				else
					return {};
			}

			pParse += 2;
			Address = CStr{pParse};
		}
	}
	else
		Address = _Address.f_Extract(fg_StrLen("UNIX:"));

	if (Address.f_GetLen() > CUnixAddress::mc_MaxAddressLength)
	{
		if (_bThrowOnError)
			DMibErrorNet(fg_Format("Unix sockets support a maximum path length of {} characters. Invalid path '{}'", CUnixAddress::mc_MaxAddressLength, Address));
		else
			return {};
	}

	CUnixAddress AddressWithPermissions;
	NMemory::fg_MemClear(AddressWithPermissions);
	AddressWithPermissions.m_Permissions = Permissions;

	auto &AddressUn = AddressWithPermissions.m_UnixAddress;

	AddressUn.sun_family = AF_UNIX;
	NMib::NStr::fg_StrCopy(AddressUn.sun_path, Address, CUnixAddress::mc_MaxAddressLength + 1);

#if defined(DPlatformFamily_macOS)
	AddressUn.sun_len = sizeof(AddressUn);
#endif

	return fg_Move(AddressWithPermissions);
}

namespace
{
	aint fg_UnixSocketFileNameStart(NStr::CStr const &_Path)
	{
		for (aint iChar = _Path.f_GetLen() - 1; iChar >= 0; --iChar)
		{
			if (_Path[iChar] == '/' || _Path[iChar] == '\\')
				return iChar + 1;
		}

		return 0;
	}

	bool fg_IsStagedUnixSocketHashCharacter(ch8 _Character)
	{
		return (_Character >= '0' && _Character <= '9') || (_Character >= 'a' && _Character <= 'z') || (_Character >= 'A' && _Character <= 'Z');
	}
}

// A stream listener binds its staged path and renames it to its path once it listens: a socket refuses connects until it listens,
// and sweeps of stale sockets delete the files that refuse connects. The staged path is in the same directory and as long as
// _Path, so it fits the socket path limit wherever _Path does and the rename is atomic. The last bytes of the file name, whole
// code points of at least 8 bytes or the whole name, become '~' and a hash of _Path, so listeners of different paths stage apart.
// Names shorter than 9 bytes have less room for the hash, so listeners of such names in one directory can share a staged path.
// Socket names are not that short in practice.
NStr::CStr NSys::NNetwork::fg_GetStagedUnixSocketPath(NStr::CStr const &_Path)
{
	aint Length = _Path.f_GetLen();
	aint FileNameStart = fg_UnixSocketFileNameStart(_Path);
	DMibRequire(FileNameStart < Length);

	aint SuffixStart = fg_Max(FileNameStart, Length - gc_StagedUnixSocketSuffixLength);
	while (SuffixStart > FileNameStart && (uint8(_Path[SuffixStart]) & 0xC0) == 0x80)
		--SuffixStart;

	// FNV-1a
	uint64 Hash = 14695981039346656037ull;
	for (aint iChar = 0; iChar < Length; ++iChar)
		Hash = (Hash ^ uint8(_Path[iChar])) * 1099511628211ull;

	NStr::CStr Staged = _Path.f_Extract(0, SuffixStart);
	Staged.f_AddChar('~');

	constexpr uint64 c_nHashCharacters = sizeof(gc_StagedUnixSocketHashCharacters) - 1;
	uint64 RemainingHash = Hash;
	for (aint iChar = SuffixStart + 1; iChar < Length; ++iChar)
	{
		if (RemainingHash < c_nHashCharacters)
			RemainingHash = Hash = (Hash ^ uint64(iChar)) * 1099511628211ull;

		Staged.f_AddChar(gc_StagedUnixSocketHashCharacters[RemainingHash % c_nHashCharacters]);
		RemainingHash /= c_nHashCharacters;
	}

	return Staged;
}

// Whether the file name of _Path has the form of a staged path: '~' and only hash characters after it
bool NSys::NNetwork::fg_IsStagedUnixSocketPath(NStr::CStr const &_Path)
{
	aint Length = _Path.f_GetLen();
	aint FileNameStart = fg_UnixSocketFileNameStart(_Path);

	aint iTilde = Length - 1;
	while (iTilde >= FileNameStart && fg_IsStagedUnixSocketHashCharacter(_Path[iTilde]))
		--iTilde;

	return iTilde >= FileNameStart && _Path[iTilde] == '~';
}

CUnixAddress CUnixAddress::f_GetStaged() const
{
	CUnixAddress Staged = *this;
	NMib::NStr::fg_StrCopy(Staged.m_UnixAddress.sun_path, NSys::NNetwork::fg_GetStagedUnixSocketPath(f_GetPath()), CUnixAddress::mc_MaxAddressLength + 1);

	return Staged;
}

CRuntimeNetAddress::CRuntimeNetAddress()
	: mp_Type(NMib::NNetwork::ENetAddressType_None)
{
}

CRuntimeNetAddress::CRuntimeNetAddress(CRuntimeNetAddress const &_ToCopy)
	: mp_Type(_ToCopy.mp_Type)
	, mp_Data(_ToCopy.mp_Data)
{
}

CRuntimeNetAddress::CRuntimeNetAddress(NMib::NNetwork::ENetAddressType _Type, void const *_pData, umint _nDataBytes)
	: mp_Type(NMib::NNetwork::ENetAddressType_None)
{
	f_Set(_Type, _pData, _nDataBytes);
}

CRuntimeNetAddress::CRuntimeNetAddress(sockaddr_in const &_TCPv4)
	: mp_Type(NMib::NNetwork::ENetAddressType_None)
{
	f_Set(_TCPv4);
}

CRuntimeNetAddress::CRuntimeNetAddress(sockaddr_in6 const &_TCPv6)
	: mp_Type(NMib::NNetwork::ENetAddressType_None)
{
	f_Set(_TCPv6);
}

CRuntimeNetAddress::CRuntimeNetAddress(CUnixAddress const &_Unix)
	: mp_Type(NMib::NNetwork::ENetAddressType_None)
{
	f_Set(_Unix);
}

NMib::NNetwork::ENetAddressType CRuntimeNetAddress::f_GetType() const
{
	return mp_Type;
}

void CRuntimeNetAddress::f_Set(NMib::NNetwork::ENetAddressType _Type, void const *_pData, umint _nDataBytes)
{
	mp_Type = _Type;
	mp_Data.f_SetLen(_nDataBytes);
	fg_MemCopy(mp_Data.f_GetArray(), _pData, _nDataBytes);
}

void *CRuntimeNetAddress::f_GetForWrite(NMib::NNetwork::ENetAddressType _Type, umint _nDataBytes)
{
	mp_Type = _Type;
	mp_Data.f_SetLen(_nDataBytes);
	return mp_Data.f_GetArray();
}

void CRuntimeNetAddress::f_Set(sockaddr_in const &_TCPv4)
{
	f_Set(NMib::NNetwork::ENetAddressType_TCPv4, &_TCPv4, sizeof(sockaddr_in));
}

void CRuntimeNetAddress::f_Set(sockaddr_in6 const &_TCPv6)
{
	f_Set(NMib::NNetwork::ENetAddressType_TCPv6, &_TCPv6, sizeof(sockaddr_in6));
}

void CRuntimeNetAddress::f_Set(CUnixAddress const &_Unix)
{
	f_Set(NMib::NNetwork::ENetAddressType_Unix, &_Unix, sizeof(CUnixAddress));
}

umint CRuntimeNetAddress::f_GetFullDataLen() const
{
	return mp_Data.f_GetLen();
}

umint CRuntimeNetAddress::f_GetSockAddrLen() const
{
	using namespace NMib::NNetwork;
	switch (mp_Type)
	{
	case ENetAddressType_TCPv4: return sizeof(sockaddr_in);
	case ENetAddressType_TCPv6: return sizeof(sockaddr_in6);
	case ENetAddressType_Unix: return sizeof(sockaddr_un);
	default: return f_GetFullDataLen();
	}
}

void const *CRuntimeNetAddress::f_Get() const
{
	return mp_Data.f_GetArray();
}

sockaddr_in const &CRuntimeNetAddress::f_GetTCPv4() const
{
	return *(sockaddr_in const *)mp_Data.f_GetArray();
}

sockaddr_in6 const &CRuntimeNetAddress::f_GetTCPv6() const
{
	return *(sockaddr_in6 const *)mp_Data.f_GetArray();
}

CUnixAddress const &CRuntimeNetAddress::f_GetUnix() const
{
	return *(CUnixAddress const *)mp_Data.f_GetArray();
}

sockaddr_in &CRuntimeNetAddress::f_GetTCPv4()
{
	return *(sockaddr_in *)mp_Data.f_GetArray();
}

sockaddr_in6 &CRuntimeNetAddress::f_GetTCPv6()
{
	return *(sockaddr_in6 *)mp_Data.f_GetArray();
}

CUnixAddress &CRuntimeNetAddress::f_GetUnix()
{
	return *(CUnixAddress *)mp_Data.f_GetArray();
}

int CRuntimeNetAddress::f_Compare(CRuntimeNetAddress const &_Other) const
{
	if (mp_Type < _Other.mp_Type)
		return -1;
	else if (mp_Type > _Other.mp_Type)
		return 1;
	else if (mp_Data.f_GetLen() < _Other.mp_Data.f_GetLen())
		return -1;
	else if (mp_Data.f_GetLen() > _Other.mp_Data.f_GetLen())
		return 1;
	else
		return fg_MemCmp(mp_Data.f_GetArray(), _Other.mp_Data.f_GetArray(), _Other.mp_Data.f_GetLen());
}
