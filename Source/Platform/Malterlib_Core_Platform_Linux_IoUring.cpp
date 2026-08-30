// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Linux_IoUring.h"

#include <sys/resource.h>

int CIoUringRing::fs_Setup(uint32 _nEntries, CIoUringParams *_pParams)
{
	return (int)syscall(gc_IoUringSyscall_Setup, _nEntries, _pParams);
}

int CIoUringRing::fs_Enter(int _Fd, uint32 _nToSubmit, uint32 _nMinComplete, uint32 _Flags)
{
	return (int)syscall(gc_IoUringSyscall_Enter, _Fd, _nToSubmit, _nMinComplete, _Flags, nullptr, 0);
}

int CIoUringRing::fs_Register(int _Fd, uint32 _Opcode, void *_pArg, uint32 _nArgs)
{
	return (int)syscall(gc_IoUringSyscall_Register, _Fd, _Opcode, _pArg, _nArgs);
}

// nullptr when the submission ring is full; flush first and retry
CIoUringSqe *CIoUringRing::f_GetSqe()
{
	uint32 Head = __atomic_load_n(m_pSqHead, __ATOMIC_ACQUIRE);
	if (m_SqTailLocal - Head >= m_nSqEntries)
		return nullptr;

	CIoUringSqe *pSqe = &m_pSqes[m_SqTailLocal & m_SqRingMask];
	NMib::NMemory::fg_MemClear(pSqe, sizeof(*pSqe));

	++m_SqTailLocal;
	++m_nPendingSubmit;

	return pSqe;
}

// Called once by the driving thread before first use; a no-op for rings created enabled
bool CIoUringRing::f_EnableRings()
{
	if (!m_bNeedsEnable)
		return true;

	m_bNeedsEnable = false;
	return fs_Register(m_RingFd, gc_IoUringRegister_EnableRings, nullptr, 0) == 0;
}

CIoUringCqe *CIoUringRing::f_PeekCqe()
{
	uint32 Head = *m_pCqHead;
	uint32 Tail = __atomic_load_n(m_pCqTail, __ATOMIC_ACQUIRE);
	if (Head == Tail)
		return nullptr;

	return &m_pCqes[Head & m_CqRingMask];
}

void CIoUringRing::f_AdvanceCq()
{
	__atomic_store_n(m_pCqHead, *m_pCqHead + 1, __ATOMIC_RELEASE);
}

// Completions overflowed the ring and wait in the kernel-side backlog; an enter with
// get-events flushes them into the ring once there is room
bool CIoUringRing::f_CqOverflowPending() const
{
	return (__atomic_load_n(m_pSqFlags, __ATOMIC_ACQUIRE) & gc_IoUringSq_CqOverflow) != 0;
}

// Set as a side effect of the fs_Available probe; futex waits in the ring need 6.7
bool &CIoUringRing::fs_FutexWaitSupported()
{
	static bool s_bSupported = false;
	return s_bSupported;
}

// Set as a side effect of the fs_Available probe. Zero copy sends hand the kernel the caller's
// pages instead of copying them, at the cost of a second completion per operation and of the
// pages staying pinned until it arrives.
//
// MalterlibIoUringSendZeroCopy=0 keeps ordinary sends
bool &CIoUringRing::fs_SendZeroCopySupported()
{
	static bool s_bSupported = false;
	return s_bSupported;
}

// Set as a side effect of the fs_Available probe: completion transfers submit vectored sends
// as ring operations, and cancel them through async cancel on teardown.
// MalterlibIoUringCompletion=0 keeps the readiness backend while leaving polls on the ring
bool &CIoUringRing::fs_CompletionSupported()
{
	static bool s_bSupported = false;
	return s_bSupported;
}

// Set as a side effect of the fs_Available probe: inbound payload as one standing multishot
// receive per socket over a provided-buffer ring, needing kernel 6.0 — probed through the
// send-zc opcode that release introduced, plus an actual provided-buffer ring registration so
// a filtered register syscall fails the probe rather than every socket.
// MalterlibIoUringMultishot=0 keeps receives on readiness for comparison
bool &CIoUringRing::fs_ReceiveStreamSupported()
{
	static bool s_bSupported = false;
	return s_bSupported;
}

// Whether provided buffer rings support incremental consumption (6.12+), probed by
// registering a ring with the flag set
bool &CIoUringRing::fs_ReceiveStreamIncremental()
{
	static bool s_bSupported = false;
	return s_bSupported;
}

// _bDeferEnable creates the ring disabled: pollers are constructed on whichever thread starts
// the connection while a different thread drives the loop, and single issuer binds the ring to
// the enabling task, so the driving thread claims ownership by enabling it before first use
bool CIoUringRing::f_Create(uint32 _nSqEntries, uint32 _nCqEntries, bool _bDeferEnable, bool _bSqPoll)
{
	CIoUringParams Params;

	// Single issuer needs 6.0 and creating disabled needs 5.10; the readiness backend works
	// without either, so each rejection drops down a rung
	uint32 FlagAttempts[] =
		{
			gc_IoUringSetup_SingleIssuer | gc_IoUringSetup_RDisabled
			, gc_IoUringSetup_RDisabled
			, 0
		}
	;

	m_bSqPoll = _bSqPoll;

	for (uint32 AttemptFlags : FlagAttempts)
	{
		if (!_bDeferEnable && (AttemptFlags & gc_IoUringSetup_RDisabled))
			continue;

		NMib::NMemory::fg_MemClear(&Params, sizeof(Params));
		Params.m_Flags = gc_IoUringSetup_CqSize | gc_IoUringSetup_Clamp | AttemptFlags;
		if (_bSqPoll)
		{
			// The kernel poller issues operations itself — submissions become a tail store,
			// and the copies an issue runs land on the poller's thread, not the submitter's.
			// Experimental, off by default: MalterlibIoUringSqPoll=1
			Params.m_Flags |= gc_IoUringSetup_SqPoll;
			Params.m_SqThreadIdle = 1000;
		}
		Params.m_nCqEntries = _nCqEntries;

		m_RingFd = fs_Setup(_nSqEntries, &Params);
		if (m_RingFd >= 0)
		{
			m_bNeedsEnable = (AttemptFlags & gc_IoUringSetup_RDisabled) != 0;
			break;
		}
		if (errno != EINVAL)
			return false;
	}
	if (m_RingFd < 0)
		return false;

	m_Features = Params.m_Features;
	if
	(
		!(m_Features & gc_IoUringFeat_NoDrop)
		|| !(m_Features & gc_IoUringFeat_SubmitStable)
		|| !(m_Features & gc_IoUringFeat_ExtArg)
	)
	{
		f_Destroy();
		return false;
	}

	m_SqRingSize = Params.m_SqOff.m_Array + Params.m_nSqEntries * sizeof(uint32);
	m_CqRingSize = Params.m_CqOff.m_Cqes + Params.m_nCqEntries * sizeof(CIoUringCqe);

	if (m_Features & gc_IoUringFeat_SingleMmap)
	{
		if (m_CqRingSize > m_SqRingSize)
			m_SqRingSize = m_CqRingSize;
		m_CqRingSize = m_SqRingSize;
	}

	m_pSqRing = mmap(nullptr, m_SqRingSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, m_RingFd, gc_IoUringOff_SqRing);
	if (m_pSqRing == MAP_FAILED)
	{
		m_pSqRing = nullptr;
		f_Destroy();
		return false;
	}

	if (m_Features & gc_IoUringFeat_SingleMmap)
		m_pCqRing = m_pSqRing;
	else
	{
		m_pCqRing = mmap(nullptr, m_CqRingSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, m_RingFd, gc_IoUringOff_CqRing);
		if (m_pCqRing == MAP_FAILED)
		{
			m_pCqRing = nullptr;
			f_Destroy();
			return false;
		}
	}

	m_SqesSize = Params.m_nSqEntries * sizeof(CIoUringSqe);
	m_pSqes = (CIoUringSqe *)mmap(nullptr, m_SqesSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, m_RingFd, gc_IoUringOff_Sqes);
	if (m_pSqes == MAP_FAILED)
	{
		m_pSqes = nullptr;
		f_Destroy();
		return false;
	}

	m_pSqHead = (uint32 *)((uint8 *)m_pSqRing + Params.m_SqOff.m_Head);
	m_pSqTail = (uint32 *)((uint8 *)m_pSqRing + Params.m_SqOff.m_Tail);
	m_SqRingMask = *(uint32 *)((uint8 *)m_pSqRing + Params.m_SqOff.m_RingMask);
	m_nSqEntries = *(uint32 *)((uint8 *)m_pSqRing + Params.m_SqOff.m_RingEntries);
	m_pSqArray = (uint32 *)((uint8 *)m_pSqRing + Params.m_SqOff.m_Array);
	m_pSqFlags = (uint32 *)((uint8 *)m_pSqRing + Params.m_SqOff.m_Flags);
	m_SqTailLocal = *m_pSqTail;

	m_pCqHead = (uint32 *)((uint8 *)m_pCqRing + Params.m_CqOff.m_Head);
	m_pCqTail = (uint32 *)((uint8 *)m_pCqRing + Params.m_CqOff.m_Tail);
	m_CqRingMask = *(uint32 *)((uint8 *)m_pCqRing + Params.m_CqOff.m_RingMask);
	m_pCqes = (CIoUringCqe *)((uint8 *)m_pCqRing + Params.m_CqOff.m_Cqes);

	// Identity map once; the kernel reads the index array to find the sqes
	for (uint32 i = 0; i < m_nSqEntries; ++i)
		m_pSqArray[i] = i;

	return true;
}

void CIoUringRing::f_Destroy()
{
	if (m_pSqes)
		munmap(m_pSqes, m_SqesSize);
	if (m_pCqRing && m_pCqRing != m_pSqRing)
		munmap(m_pCqRing, m_CqRingSize);
	if (m_pSqRing)
		munmap(m_pSqRing, m_SqRingSize);
	if (m_RingFd >= 0)
		close(m_RingFd);

	m_pSqes = nullptr;
	m_pCqRing = nullptr;
	m_pSqRing = nullptr;
	m_RingFd = -1;
}


// Returns errno-style negative on failure, submitted count otherwise
int CIoUringRing::f_Submit(uint32 _nMinComplete, bool _bGetEvents)
{
	// Publish every SQE filled since the last submit; the release pairs with the kernel's
	// acquire of the tail, so no partially initialized request is ever visible
	__atomic_store_n(m_pSqTail, m_SqTailLocal, __ATOMIC_RELEASE);

	uint32 nToSubmit = m_nPendingSubmit;
	uint32 Flags = (_bGetEvents || _nMinComplete) ? gc_IoUringEnter_GetEvents : 0;

	if (m_bSqPoll)
	{
		// The poller consumes the tail on its own; the enter is only for waking it after
		// idle decay, or for waiting on completions
		m_nPendingSubmit = 0;
		bool bNeedWakeup = (__atomic_load_n(m_pSqFlags, __ATOMIC_ACQUIRE) & gc_IoUringSq_NeedWakeup) != 0;
		if (bNeedWakeup)
			Flags |= gc_IoUringEnter_SqWakeup;
		else if (!Flags)
			return (int)nToSubmit;
	}

	int Ret;
	do
	{
		Ret = fs_Enter(m_RingFd, nToSubmit, _nMinComplete, Flags);
	}
	while (Ret < 0 && errno == EINTR)
		;

	if (Ret < 0)
		return -errno;

	uint32 nSubmitted = (uint32)Ret;
	if (nSubmitted > m_nPendingSubmit)
		nSubmitted = m_nPendingSubmit;
	m_nPendingSubmit -= nSubmitted;

	return Ret;
}

// Probed once per process: ring creation with the required features plus the ops the
// readiness backend needs. Covers ENOSYS, EPERM, seccomp and kernel.io_uring_disabled.
// MalterlibIoUring=0 vetoes for environments that ban io_uring outright; the veto is only
// honored in builds with the io debugging overrides enabled (MalterlibIoDebug_Enable)
umint CIoUringRing::fs_RingBytes(uint32 _nSqEntries, uint32 _nCqEntries)
{
	CIoUringRing Probe;
	if (!Probe.f_Create(_nSqEntries, _nCqEntries, true))
		return 0;

	umint PageSize = NMib::NSys::fg_Mem_VirtualGranularityCommit(false);
	auto fPageRound = [&](umint _nBytes) -> umint
		{
			return (_nBytes + PageSize - 1) & ~(PageSize - 1);
		}
	;

	umint nBytes = fPageRound(Probe.m_SqRingSize) + fPageRound(Probe.m_SqesSize);
	if (!(Probe.m_Features & gc_IoUringFeat_SingleMmap))
		nBytes += fPageRound(Probe.m_CqRingSize);

	Probe.f_Destroy();

	return nBytes;
}

bool CIoUringRing::fs_Available()
{
#if !DMibConfig_IoUring_Enable
	return false;
#else
	bool bAvailable =
		(
			[]() -> bool
			{
#if DMibConfig_IoDebug_Enable
				auto Setting = NMib::NSys::fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStrNonTracked("MalterlibIoUring"));
				if (Setting.f_IsEmpty())
				{
					// The old name from when the loops were socket only is honored
					Setting = NMib::NSys::fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStrNonTracked("MalterlibNetworkIoUring"));
				}
				if (Setting == "0")
					return false;
#endif

				// Created disabled like the real loops, then enabled and entered: a sandbox
				// can filter io_uring_enter or the enable registration separately from
				// setup, and availability has to mean the whole startup path works — not
				// just setup and the op probe. An enter with nothing to submit or wait for
				// returns immediately
				CIoUringRing Probe;
				if (!Probe.f_Create(8, 8, true))
					return false;

				umint PageSize = NMib::NSys::fg_Mem_VirtualGranularityCommit(false);

				if (!Probe.f_EnableRings() || fs_Enter(Probe.m_RingFd, 0, 0, gc_IoUringEnter_GetEvents) < 0)
				{
					Probe.f_Destroy();
					return false;
				}

				static CIoUringProbe s_ProbeResult;
				NMib::NMemory::fg_MemClear(&s_ProbeResult, sizeof(s_ProbeResult));

				bool bOps = false;
				if (fs_Register(Probe.m_RingFd, gc_IoUringRegister_Probe, &s_ProbeResult, 64) == 0)
				{
					auto fSupported = [&](uint8 _Op) -> bool
						{
							return _Op < s_ProbeResult.m_nOps && (s_ProbeResult.m_Ops[_Op].m_Flags & 1);
						}
					;

					// The kernel floor is a policy rather than a per-op need: instead of
					// reasoning about every opcode's vintage separately, kernels older than
					// 5.19 — probed through the socket op that release introduced — fall
					// back to epoll wholesale, so the backend only ever runs on one known
					// baseline
					bOps =
						fSupported(gc_IoUringOp_PollAdd)
						&& fSupported(gc_IoUringOp_AsyncCancel)
						&& fSupported(gc_IoUringOp_Socket)
					;
					fs_FutexWaitSupported() = bOps && fSupported(gc_IoUringOp_FutexWait);

					bool bCompletionVetoed = false;
#if DMibConfig_IoDebug_Enable
					auto CompletionSetting = NMib::NSys::fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStrNonTracked("MalterlibIoUringCompletion"));
					bCompletionVetoed = CompletionSetting == "0";
#endif
					fs_CompletionSupported() =
						bOps
						&& !bCompletionVetoed
						&& fSupported(gc_IoUringOp_Recv)
						&& fSupported(gc_IoUringOp_SendMsg)
						&& fSupported(gc_IoUringOp_AsyncCancel)
					;

					bool bZeroCopyVetoed = false;
#if DMibConfig_IoDebug_Enable
					auto ZeroCopySetting = NMib::NSys::fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStrNonTracked("MalterlibIoUringSendZeroCopy"));
					bZeroCopyVetoed = ZeroCopySetting == "0";
#endif
					fs_SendZeroCopySupported() =
						fs_CompletionSupported()
						&& !bZeroCopyVetoed
						&& fSupported(gc_IoUringOp_SendMsgZc)
					;

					bool bMultishotVetoed = false;
#if DMibConfig_IoDebug_Enable
					auto MultishotSetting = NMib::NSys::fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStrNonTracked("MalterlibIoUringMultishot"));
					bMultishotVetoed = MultishotSetting == "0";
#endif
					bool bStream = fs_CompletionSupported() && !bMultishotVetoed && fSupported(gc_IoUringOp_SendZc);
					if (bStream)
					{
						// The register syscall can be filtered separately from setup and
						// enter, so support means an actual ring registers — not just that
						// the kernel is new enough
						void *pRing = mmap(nullptr, PageSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
						if (pRing == MAP_FAILED)
							bStream = false;
						else
						{
							CIoUringBufReg Reg;
							NMib::NMemory::fg_MemClear(&Reg, sizeof(Reg));
							Reg.m_RingAddr = (uint64)(umint)pRing;
							Reg.m_nRingEntries = 8;
							Reg.m_Bgid = 0;

							if (fs_Register(Probe.m_RingFd, gc_IoUringRegister_PbufRing, &Reg, 1) != 0)
								bStream = false;
							else
							{
								CIoUringBufReg Unreg;
								NMib::NMemory::fg_MemClear(&Unreg, sizeof(Unreg));
								Unreg.m_Bgid = 0;
								fs_Register(Probe.m_RingFd, gc_IoUringRegister_UnregisterPbufRing, &Unreg, 1);

								// Incremental consumption is its own probe rung: an old
								// kernel rejects the flag and the plain ring carries on
								bool bIncrementalVetoed = false;
#if DMibConfig_IoDebug_Enable
								auto IncrementalSetting = NMib::NSys::fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStrNonTracked("MalterlibIoUringIncremental"));
								bIncrementalVetoed = IncrementalSetting == "0";
#endif
								if (!bIncrementalVetoed)
								{
									Reg.m_Flags = gc_IoUringPbufRing_Incremental;
									if (fs_Register(Probe.m_RingFd, gc_IoUringRegister_PbufRing, &Reg, 1) == 0)
									{
										fs_ReceiveStreamIncremental() = true;
										fs_Register(Probe.m_RingFd, gc_IoUringRegister_UnregisterPbufRing, &Unreg, 1);
									}
								}
							}

							munmap(pRing, PageSize);
						}
					}
					fs_ReceiveStreamSupported() = bStream;

					// Send bundles are a flag on the send opcode with no probe entry of its
					// own, so probe functionally — a socketpair, a ring with one published
					// buffer, one bundle send; an old kernel answers -EINVAL. Completion
					// sends require it: there is exactly one completion send path, and a
					// kernel without bundles keeps the whole readiness backend
					if (fs_CompletionSupported())
					{
						bool bBundle = false;
						int Sockets[2];
						if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, Sockets) == 0)
						{
							void *pRing = mmap(nullptr, PageSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
							if (pRing != MAP_FAILED)
							{
								CIoUringBufReg Reg;
								NMib::NMemory::fg_MemClear(&Reg, sizeof(Reg));
								Reg.m_RingAddr = (uint64)(umint)pRing;
								Reg.m_nRingEntries = 8;
								Reg.m_Bgid = 1;

								if (fs_Register(Probe.m_RingFd, gc_IoUringRegister_PbufRing, &Reg, 1) == 0)
								{
									static uint8 s_ProbeBytes[64];
									auto *pEntries = (CIoUringBuf *)pRing;
									pEntries[0].m_Addr = (uint64)(umint)s_ProbeBytes;
									pEntries[0].m_Len = sizeof(s_ProbeBytes);
									pEntries[0].m_Bid = 0;
									__atomic_store_n((uint16 *)((uint8 *)pRing + 14), 1, __ATOMIC_RELEASE);

									CIoUringSqe *pSqe = Probe.f_GetSqe();
									NMib::NMemory::fg_MemClear(pSqe, sizeof(*pSqe));
									pSqe->m_Opcode = gc_IoUringOp_Send;
									pSqe->m_Fd = Sockets[0];
									pSqe->m_Flags = gc_IoUringSqeFlag_BufferSelect;
									pSqe->m_IoPrio = gc_IoUringRecvSend_Bundle;
									pSqe->m_BufIndex = 1;
									pSqe->m_OpFlags = MSG_NOSIGNAL;
									pSqe->m_UserData = 1;

									if (Probe.f_Submit(1, true) >= 0)
									{
										if (CIoUringCqe *pCqe = Probe.f_PeekCqe())
										{
											bBundle = pCqe->m_Res == (int32)sizeof(s_ProbeBytes);
											Probe.f_AdvanceCq();
										}
									}

									CIoUringBufReg Unreg;
									NMib::NMemory::fg_MemClear(&Unreg, sizeof(Unreg));
									Unreg.m_Bgid = 1;
									fs_Register(Probe.m_RingFd, gc_IoUringRegister_UnregisterPbufRing, &Unreg, 1);
								}

								munmap(pRing, PageSize);
							}

							close(Sockets[0]);
							close(Sockets[1]);
						}

						fs_CompletionSupported() = bBundle;
						if (!bBundle)
						{
							fs_SendZeroCopySupported() = false;
							fs_ReceiveStreamSupported() = false;
						}
					}
				}

				Probe.f_Destroy();
				return bOps;
			}
			()
		)
	;
	return bAvailable;
#endif
}
