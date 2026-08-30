// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

// Raw io_uring ABI. The kernel interface is ~400 lines of well specified structs and three
// syscalls, and the codebase already binds optional kernel entry points directly, so this avoids
// an External/ dependency on liburing. Only what the loop backend uses is declared

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

constexpr uint8 gc_IoUringOp_PollAdd = 6;
constexpr uint8 gc_IoUringOp_PollRemove = 7;
constexpr uint8 gc_IoUringOp_SendMsg = 9;
constexpr uint8 gc_IoUringOp_Send = 26;
constexpr uint8 gc_IoUringOp_AsyncCancel = 14;
constexpr uint8 gc_IoUringOp_Recv = 27;
constexpr uint8 gc_IoUringOp_SendMsgZc = 48;
// Not used for io — probed as the 6.0 floor marker for multishot receive, which has no probe
// entry of its own because it is a flag on the recv opcode rather than an opcode
constexpr uint8 gc_IoUringOp_SendZc = 47;
// Not used for io — probed as the kernel floor marker (introduced in 5.19), see fs_Available
constexpr uint8 gc_IoUringOp_Socket = 45;
constexpr uint8 gc_IoUringOp_FutexWait = 51;

constexpr uint32 gc_IoUringCancelFlag_All = 1 << 0;
constexpr uint32 gc_IoUringCancelFlag_Fd = 1 << 1;

// Matches the FUTEX_WAIT_PRIVATE the semaphore wakes use; a mismatch hashes into a different
// bucket and the wake never finds the wait
constexpr uint32 gc_IoUringFutex2_SizeU32 = 0x2;
constexpr uint32 gc_IoUringFutex2_Private = 128;
constexpr uint64 gc_IoUringFutex_BitsetMatchAny = 0xFFFFFFFFull;

constexpr uint32 gc_IoUringPoll_AddMulti = 1 << 0;

// Send/recv operation flags; these ride the SQE's ioprio field for the network opcodes
constexpr uint16 gc_IoUringRecv_Multishot = 1 << 1;
// One send drains every provided-ring entry published at its issue, in ring order, and the
// kernel finishes each one internally — a bundle never reports a short send (6.10+)
constexpr uint16 gc_IoUringRecvSend_Bundle = 1 << 4;

// The SQE flag that lets the kernel pick the operation's buffer from a provided-buffer ring; the
// group is named in the SQE's buf_index field
constexpr uint8 gc_IoUringSqeFlag_BufferSelect = 1 << 5;

// One entry of a provided-buffer ring: a buffer the kernel may pick for an operation submitted
// with buffer select. The ring is an array of these; the tail the producer publishes overlays the
// first entry's reserved field
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

constexpr uint32 gc_IoUringCqe_FMore = 1 << 1;
// The completion consumed a provided buffer; which one is in the upper half of the flags word
constexpr uint32 gc_IoUringCqe_FBuffer = 1 << 0;
constexpr uint32 gc_IoUringCqe_BufferShift = 16;
// A zero copy send answers twice: the result says what reached the wire and carries FMore to
// promise the second, and the notification says the pages are released and may be reused. FMore
// is what tells the two apart on the result — a send that failed before pinning anything sends
// no notification and sets no FMore, so nothing has to be predicted at submit time
constexpr uint32 gc_IoUringCqe_FNotif = 1 << 3;

constexpr uint32 gc_IoUringEnter_GetEvents = 1 << 0;
constexpr uint32 gc_IoUringEnter_SqWakeup = 1 << 1;
// SQ ring flags word (sq_off.flags): the kernel poller has gone idle and needs a wakeup enter
constexpr uint32 gc_IoUringSq_NeedWakeup = 1 << 0;

// SQ ring flags word, kernel written
constexpr uint32 gc_IoUringSq_CqOverflow = 1 << 1;

// Provided buffer ring registration flag: the kernel consumes each buffer incrementally
// across completions instead of one buffer per completion, so consecutive receives land
// adjacently in one buffer (6.12+). IORING_CQE_F_BUF_MORE on a completion says the buffer
// still has room and stays with the kernel
constexpr uint16 gc_IoUringPbufRing_Incremental = 2;
constexpr uint32 gc_IoUringCqe_FBufMore = 1 << 4;

constexpr uint32 gc_IoUringRegister_Buffers = 0;
constexpr uint32 gc_IoUringRegister_Probe = 8;
constexpr uint32 gc_IoUringRegister_EnableRings = 12;
constexpr uint32 gc_IoUringRegister_PbufRing = 22;
constexpr uint32 gc_IoUringRegister_UnregisterPbufRing = 23;

constexpr uint64 gc_IoUringOff_SqRing = 0;
constexpr uint64 gc_IoUringOff_CqRing = 0x8000000ull;
constexpr uint64 gc_IoUringOff_Sqes = 0x10000000ull;

// Same numbers on x86_64 and arm64; these syscalls postdate the unified table
constexpr long gc_IoUringSyscall_Setup = 425;
constexpr long gc_IoUringSyscall_Enter = 426;
constexpr long gc_IoUringSyscall_Register = 427;

// One ring, owned and driven by a single thread (submissions and reaps are unsynchronized).
// Other threads only ever wake the owner through the poller's pipe, never touch the ring
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

	static bool fs_Available();

	// Bytes the kernel maps for a ring with these entry counts — the ring pages plus the
	// submission entries — which it also charges to the user's locked memory limit
	static umint fs_RingBytes(uint32 _nSqEntries, uint32 _nCqEntries);

	static bool &fs_FutexWaitSupported();
	static bool &fs_SendZeroCopySupported();
	static bool &fs_CompletionSupported();
	static bool &fs_ReceiveStreamSupported();
	static bool &fs_ReceiveStreamIncremental();

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

	// The producer tail, published to the kernel-visible tail only in f_Submit: publishing per
	// slot would expose an SQE the caller has not filled yet, and the io_uring protocol requires
	// every SQE write to happen before the tail that covers it
	uint32 m_SqTailLocal = 0;

	bool m_bSqPoll = false;

	// Set when the ring was created disabled so the driving thread can claim single-issuer
	// ownership; the first iterate on that thread enables it
	bool m_bNeedsEnable = false;
};
