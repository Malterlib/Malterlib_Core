// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_POSIX_IoLoop.h"
#include "Malterlib_Core_PlatformImp_POSIX.h"
#include "Malterlib_Core_Platform_POSIX_ErrNo.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#if defined(DPlatformFamily_Linux)
	#include "Malterlib_Core_Platform_Linux_Optional.h"
#endif

#if defined(DPlatformFamily_macOS)
CSystem_POSIX *fg_GetSys_POSIX();
#endif

using namespace NMib;
using namespace NMib::NMemory;

static void fsg_MakePipeNonBlockingCloExec(int (&_Pipe)[2])
{
	for (int Fd : _Pipe)
	{
		fcntl(Fd, F_SETFL, fcntl(Fd, F_GETFL) | O_NONBLOCK);
		fcntl(Fd, F_SETFD, fcntl(Fd, F_GETFD) | FD_CLOEXEC);
	}
}

CIoLoop_POSIXBase::CIoLoop_POSIXBase()
{
	int PipeRet;
#if defined(DPlatformFamily_Linux)
	if (NLocal::g_f_pipe2)
		PipeRet = NLocal::g_f_pipe2(mp_ReadWritePipe, O_NONBLOCK | O_CLOEXEC);
	else
	{
		PipeRet = pipe(mp_ReadWritePipe);
		if (!PipeRet)
			fsg_MakePipeNonBlockingCloExec(mp_ReadWritePipe);
	}
#else
	{
		// The pipes must not leak into other processes, and without an atomic pipe2 the close on
		// exec flags go on under the fork lock
		DMibLock(fg_GetSys_POSIX()->m_ForkLock);
		PipeRet = pipe(mp_ReadWritePipe);
		if (!PipeRet)
			fsg_MakePipeNonBlockingCloExec(mp_ReadWritePipe);
	}
#endif
	if (PipeRet)
		DMibErrorNet(NMib::NPlatform::fg_FormatErrno("pipe (io loop)", errno));

	CIoLoopChange CurChange;
	CurChange.m_bInternal = true;
	CurChange.m_Handle = mp_ReadWritePipe[0];
	mp_ChangeQueue.f_Push(fg_Move(CurChange));
}

CIoLoop_POSIXBase::~CIoLoop_POSIXBase()
{
	close(mp_ReadWritePipe[0]);
	close(mp_ReadWritePipe[1]);
}

void CIoLoop_POSIXBase::fp_WakeKernel()
{
	// The pending bit is already latched, so no later waker will retry this write — an
	// interrupted attempt has to be retried right here or the wake is lost until unrelated
	// I/O arrives. A full pipe needs no retry: its unread bytes already owe the owner a wake
	char Byte = 1;
	while (write(mp_ReadWritePipe[1], &Byte, 1) == -1 && errno == EINTR)
		;
}
