// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_PlatformImp_POSIX_Net.h"
#include <Mib/Process/Platform>

#include <netinet/tcp.h>
#if defined(DPlatformFamily_macOS)
#	include <sys/sysctl.h>
#endif
#include <sys/uio.h>
#include <sys/stat.h>

#if defined(DPlatformFamily_Linux)
#	include <sys/stat.h>
#	include <sys/vfs.h>
#	include <sys/syscall.h>
#	include <fcntl.h>
#	include <unistd.h>
#	include <errno.h>
#	include <stdio.h>
#	include <stdlib.h>
#	include <string.h>
#	include "Malterlib_Core_Platform_Linux_TcpInfo.h"

	// Build headers may predate SO_PEERPIDFD (Linux 6.5) and pidfs (6.9); supply absent constants.
#	ifndef SO_PEERPIDFD
#		define SO_PEERPIDFD 77 // NOLINT(malterlib-macro-naming)
#	endif
#	ifndef PID_FS_MAGIC
#		define PID_FS_MAGIC 0x50494446 // NOLINT(malterlib-macro-naming)
#	endif
#	ifndef SYS_pidfd_open
#		define SYS_pidfd_open 434 // NOLINT(malterlib-macro-naming): the name of the Linux system call
#	endif
#endif

CPOSIXSocketContext::~CPOSIXSocketContext() = default;

#ifdef DPlatformFamily_macOS
#	ifndef s6_addr16
#		define s6_addr16 __u6_addr.__u6_addr16
#	endif
#	ifndef s6_addr32
#		define s6_addr32 __u6_addr.__u6_addr32
#	endif
#endif

#include "Malterlib_Core_PlatformImp_POSIX_Net.imp.hpp"

#include "Malterlib_Core_PlatformImp_Net.imp.h"

#include "Malterlib_Core_PlatformImp_POSIX_Resolver.hpp"
