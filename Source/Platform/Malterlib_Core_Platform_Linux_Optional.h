// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <unwind.h>
#include <sys/socket.h>

namespace NLocal
{
	extern int (*g_f_pipe2)(int _PipeDescriptors[2], int _Flags) __THROW __wur;
	extern int (*g_f_accept4)(int _FileDescriptor, __SOCKADDR_ARG _pAddress, socklen_t * __restrict _pAddressLength, int _Flags);
	extern int (*g_f_inotify_init)(void) __THROW;
	extern int (*g_f_inotify_init1)(int _Flags) __THROW;
	extern int (*g_f_inotify_add_watch)(int _FileDescriptor, char const *_pName, uint32_t _Mask) __THROW;
	extern int (*g_f_inotify_rm_watch)(int _FileDescriptor, int _WatchDescriptor) __THROW;
	extern int (*g_f_pthread_setname_np)(pthread_t _TargetThread, __const char *_pName);
}
