// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <unwind.h>

namespace NLocal
{
	extern int (* g_f_pipe2)(int __pipedes[2], int __flags) __THROW __wur;
	extern int (* g_f_inotify_init1)(int __flags) __THROW;
	extern int (* g_f_pthread_setname_np)(pthread_t __target_thread, __const char *__name);
	extern _Unwind_Reason_Code (*g_f_unwind_backtrace) (_Unwind_Trace_Fn, void *);
	extern _Unwind_Ptr (*g_f_unwind_getip) (struct _Unwind_Context *);
}
