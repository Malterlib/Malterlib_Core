// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

/*
Author:			Michael Wynne

Contents:		POSIX implementation of platform specific functions

Comments:		When using this to support a POSIX compliant platform ensure that you
				implement the methods declared in the "Implementation specific headers"
				section below, PRIOR to including this cpp file.
*/
using namespace NMib;

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mman.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#include <uuid/uuid.h>
#include <poll.h>

#if (__x86_64__ || __i386__)
#	include <cpuid.h>		// Used for __get_cpuid
#endif

#include <sys/param.h>

#include <Mib/Container/MapWithPool>

#include "Malterlib_Core_PlatformImp_POSIX.h"

// *************************************************************************************************************************
// POSIX Implementation Misc Types
// *************************************************************************************************************************

constinit NThread::CMutualAggregate g_VirtualMapLock = {DAggregateInit};
constinit NMib::NStorage::TCAggregateSimple<TCMapWithPool<umint, umint, NMib::CSort_Default, NMib::NMemory::CAllocator_VirtualNoTracking>> g_VirtualMap = {DAggregateInit};

constinit NAtomic::TCAtomic<umint> g_ForceMmapSequence{0};

CSystem_POSIX *fg_GetSys_POSIX();

#ifdef DPlatformFamily_macOS
#	include <sys/sysctl.h>
#endif

#include "Malterlib_Core_PlatformImp_POSIX.imp.hpp"
