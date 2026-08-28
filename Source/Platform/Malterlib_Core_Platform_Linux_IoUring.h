// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once


#include <Mib/Core/Core>

#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>

#ifndef DMibConfig_IoUring_Enable
#	define DMibConfig_IoUring_Enable 1
#endif

struct CIoUringSqOffsets
{
	uint32 m_Head;
	uint32 m_Tail;
	uint32 m_RingMask;
	uint32 m_RingEntries;
	uint32 m_Flags;
	uint32 m_Dropped;
	uint32 m_Array;
	uint32 m_Resv1;
	uint64 m_UserAddr;
};

struct CIoUringCqOffsets
{
	uint32 m_Head;
	uint32 m_Tail;
	uint32 m_RingMask;
	uint32 m_RingEntries;
	uint32 m_Overflow;
	uint32 m_Cqes;
	uint32 m_Flags;
	uint32 m_Resv1;
	uint64 m_UserAddr;
};

struct CIoUringParams
{
	uint32 m_nSqEntries;
	uint32 m_nCqEntries;
	uint32 m_Flags;
	uint32 m_SqThreadCpu;
	uint32 m_SqThreadIdle;
	uint32 m_Features;
	uint32 m_WqFd;
	uint32 m_Resv[3];
	CIoUringSqOffsets m_SqOff;
	CIoUringCqOffsets m_CqOff;
};

struct CIoUringSqe
{
	uint8 m_Opcode;
	uint8 m_Flags;
	uint16 m_IoPrio;
	int32 m_Fd;
	uint64 m_Off;
	uint64 m_Addr;
	uint32 m_Len;
	uint32 m_OpFlags;		// rw_flags / poll32_events / ...
	uint64 m_UserData;
	uint16 m_BufIndex;
	uint16 m_Personality;
	int32 m_SpliceFdIn;
	uint64 m_Addr3;
	uint64 m_Pad2;
};

struct CIoUringCqe
{
	uint64 m_UserData;
	int32 m_Res;
	uint32 m_Flags;
};

struct CIoUringProbeOp
{
	uint8 m_Op;
	uint8 m_Resv;
	uint16 m_Flags;
	uint32 m_Resv2;
};

struct CIoUringProbe
{
	uint8 m_LastOp;
	uint8 m_nOps;
	uint16 m_Resv;
	uint32 m_Resv2[3];
	CIoUringProbeOp m_Ops[64];
};

constexpr uint32 gc_IoUringSetup_SqPoll = 1 << 1;
constexpr uint32 gc_IoUringSetup_CqSize = 1 << 3;
constexpr uint32 gc_IoUringSetup_Clamp = 1 << 4;
constexpr uint32 gc_IoUringSetup_RDisabled = 1 << 6;
constexpr uint32 gc_IoUringSetup_SingleIssuer = 1 << 12;

constexpr uint32 gc_IoUringFeat_SingleMmap = 1 << 0;
constexpr uint32 gc_IoUringFeat_NoDrop = 1 << 1;
constexpr uint32 gc_IoUringFeat_SubmitStable = 1 << 2;
constexpr uint32 gc_IoUringFeat_ExtArg = 1 << 8;
constexpr uint32 gc_IoUringFeat_SendBufSelect = 1 << 14;

constexpr uint8 gc_IoUringOp_PollAdd = 6;
constexpr uint8 gc_IoUringOp_PollRemove = 7;
constexpr uint8 gc_IoUringOp_SendMsg = 9;
constexpr uint8 gc_IoUringOp_Send = 26;
constexpr uint8 gc_IoUringOp_AsyncCancel = 14;
constexpr uint8 gc_IoUringOp_Recv = 27;
constexpr uint8 gc_IoUringOp_SendMsgZc = 48;
constexpr uint8 gc_IoUringOp_SendZc = 47; // Probe marker for Linux 6.0 multishot receive support.
constexpr uint8 gc_IoUringOp_Socket = 45; // Probe marker for the Linux 5.19 readiness baseline.
constexpr uint8 gc_IoUringOp_FutexWait = 51;

constexpr uint32 gc_IoUringCancelFlag_All = 1 << 0;
constexpr uint32 gc_IoUringCancelFlag_Fd = 1 << 1;

// Private futex flags must match semaphore wakes or they hash into a different wait bucket.
constexpr uint32 gc_IoUringFutex2_SizeU32 = 0x2;
constexpr uint32 gc_IoUringFutex2_Private = 128;
constexpr uint64 gc_IoUringFutex_BitsetMatchAny = 0xFFFFFFFFull;

constexpr uint32 gc_IoUringPoll_AddMulti = 1 << 0;

// Send/recv operation flags; these ride the SQE's ioprio field for the network opcodes
constexpr uint16 gc_IoUringRecv_Multishot = 1 << 1;
// Bundles finish published entries in ring order; short entry retirement would lose unsent data (Linux 6.10+).
constexpr uint16 gc_IoUringRecvSend_Bundle = 1 << 4;

// The SQE flag that lets the kernel pick the operation's buffer from a provided-buffer ring; the
// group is named in the SQE's buf_index field
constexpr uint8 gc_IoUringSqeFlag_BufferSelect = 1 << 5;

// Kernel buffer-ring ABI; producer tail overlays the first entry's reserved field.
struct CIoUringBuf
{
	uint64 m_Addr;
	uint32 m_Len;
	uint16 m_Bid;
	uint16 m_Resv;
};

struct CIoUringBufReg
{
	uint64 m_RingAddr;
	uint32 m_nRingEntries;
	uint16 m_Bgid;
	uint16 m_Flags;
	uint64 m_Resv[3];
};

struct CIoUringBufStatus
{
	uint32 m_Bgid;
	uint32 m_Head;
	uint32 m_Resv[8];
};

constexpr uint32 gc_IoUringCqe_FMore = 1 << 1;
// The completion consumed a provided buffer; which one is in the upper half of the flags word
constexpr uint32 gc_IoUringCqe_FBuffer = 1 << 0;
constexpr uint32 gc_IoUringCqe_BufferShift = 16;
// A send result with F_MORE promises a release notification; a result without it owes none.
constexpr uint32 gc_IoUringCqe_FNotif = 1 << 3;

constexpr uint32 gc_IoUringEnter_GetEvents = 1 << 0;
constexpr uint32 gc_IoUringEnter_SqWakeup = 1 << 1;
// SQ ring flags word (sq_off.flags): the kernel poller has gone idle and needs a wakeup enter
constexpr uint32 gc_IoUringSq_NeedWakeup = 1 << 0;

// SQ ring flags word, kernel written
constexpr uint32 gc_IoUringSq_CqOverflow = 1 << 1;

// Incremental buffer consumption (Linux 6.12+); F_BUF_MORE means the named buffer remains kernel-owned.
constexpr uint16 gc_IoUringPbufRing_Incremental = 2;
constexpr uint32 gc_IoUringCqe_FBufMore = 1 << 4;

constexpr uint32 gc_IoUringRegister_Buffers = 0;
constexpr uint32 gc_IoUringRegister_Probe = 8;
constexpr uint32 gc_IoUringRegister_EnableRings = 12;
constexpr uint32 gc_IoUringRegister_PbufRing = 22;
constexpr uint32 gc_IoUringRegister_UnregisterPbufRing = 23;
constexpr uint32 gc_IoUringRegister_PbufStatus = 26;

constexpr uint64 gc_IoUringOff_SqRing = 0;
constexpr uint64 gc_IoUringOff_CqRing = 0x8000000ull;
constexpr uint64 gc_IoUringOff_Sqes = 0x10000000ull;

// Same numbers on x86_64 and arm64; these syscalls postdate the unified table
constexpr long gc_IoUringSyscall_Setup = 425;
constexpr long gc_IoUringSyscall_Enter = 426;
constexpr long gc_IoUringSyscall_Register = 427;

// Kernel capabilities collected once during startup probing.
struct CIoUringCaps
{
	bool m_bFutexWait = false; // Requires Linux 6.7; permits parking on the queue event inside the ring.
	bool m_bCompletion = false; // Vectored send operations with asynchronous cancellation.
	bool m_bSendZeroCopy = false; // Buffers remain pinned until the release notification after the result.
	bool m_bReceiveStream = false; // Requires multishot receives and successful incremental buffer-ring registration (Linux 6.12+).
};

struct CIoUringRing
{
	static int fs_Setup(uint32 _nEntries, CIoUringParams *_pParams);
	static int fs_Enter(int _Fd, uint32 _nToSubmit, uint32 _nMinComplete, uint32 _Flags);
	static int fs_Register(int _Fd, uint32 _Opcode, void *_pArg, uint32 _nArgs);

	bool f_Create(uint32 _nSqEntries, uint32 _nCqEntries, bool _bDeferEnable, bool _bSqPoll = false);
	void f_Destroy();
	bool f_EnableRings();

	CIoUringSqe *f_GetSqe();
	int f_Submit(uint32 _nMinComplete, bool _bGetEvents);

	CIoUringCqe *f_PeekCqe();
	void f_AdvanceCq();
	bool f_CqOverflowPending() const;

	static bool fs_Available(CIoUringCaps &o_Caps);
	static umint fs_RingBytes(uint32 _nSqEntries, uint32 _nCqEntries);


	void *m_pSqRing = nullptr;
	umint m_SqRingSize = 0;
	void *m_pCqRing = nullptr;
	umint m_CqRingSize = 0;
	CIoUringSqe *m_pSqes = nullptr;
	umint m_SqesSize = 0;

	uint32 *m_pSqHead = nullptr;
	uint32 *m_pSqTail = nullptr;
	uint32 *m_pSqArray = nullptr;
	// Kernel-written ring state; carries the completion-overflow flag
	uint32 *m_pSqFlags = nullptr;

	uint32 *m_pCqHead = nullptr;
	uint32 *m_pCqTail = nullptr;
	CIoUringCqe *m_pCqes = nullptr;

	int m_RingFd = -1;
	uint32 m_Features = 0;
	uint32 m_SqRingMask = 0;
	uint32 m_nSqEntries = 0;
	uint32 m_CqRingMask = 0;

	// Sqes filled but not yet handed to the kernel
	uint32 m_nPendingSubmit = 0;
	uint32 m_SqTailLocal = 0; // Publish only after every covered SQE is initialized; per-slot publication would expose partial requests.

	bool m_bSqPoll = false;
	bool m_bNeedsEnable = false; // The driving thread enables the ring to claim single-issuer ownership.
};
