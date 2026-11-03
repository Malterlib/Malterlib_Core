// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <unwind.h>
#include <sys/socket.h>

namespace NLocal
{
	extern int (*g_fPipe2)(int _PipeDescriptors[2], int _Flags) __THROW __wur;
	extern int (*g_fAccept4)(int _FileDescriptor, __SOCKADDR_ARG _pAddress, socklen_t * __restrict _pAddressLength, int _Flags);
	extern int (*g_fInotifyInit)(void) __THROW;
	extern int (*g_fInotifyInit1)(int _Flags) __THROW;
	extern int (*g_fInotifyAddWatch)(int _FileDescriptor, char const *_pName, uint32_t _Mask) __THROW;
	extern int (*g_fInotifyRmWatch)(int _FileDescriptor, int _WatchDescriptor) __THROW;
	extern int (*g_fPThreadSetnameNP)(pthread_t _TargetThread, __const char *_pName);
}
