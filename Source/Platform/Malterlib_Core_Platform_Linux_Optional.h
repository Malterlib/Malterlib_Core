// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <unwind.h>
#include <sys/socket.h>

namespace NLocal
{
	extern int (* g_f_pipe2)(int __pipedes[2], int __flags) __THROW __wur;
	extern int (* g_f_accept4)(int __fd, __SOCKADDR_ARG __addr, socklen_t *__restrict __addr_len, int __flags);
	extern int (* g_f_inotify_init)(void) __THROW;
	extern int (* g_f_inotify_init1)(int __flags) __THROW;
	extern int (* g_f_inotify_add_watch)(int __fd, const char *__name, uint32_t __mask) __THROW;
	extern int (* g_f_inotify_rm_watch)(int __fd, int __wd) __THROW;
	extern int (* g_f_pthread_setname_np)(pthread_t __target_thread, __const char *__name);
}
