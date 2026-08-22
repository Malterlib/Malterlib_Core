// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

// Possible values:
// DMibPSupportAlwaysCreatedThreadLocal
// DMibPSupportThreadCreateNotification
// DMibPSupportThreadDestroyNotification
// DMibPSupportThreadLocalDestructors
// DMibPSupportSetThreadLocalForOtherThread

#if defined(DPlatformFamily_macOS) || defined(DPlatformFamily_Windows) || (defined(DPlatformFamily_Linux) && (defined(DMibStaticThreadLocals) || defined(DMibAssumeGlibc)))
// The platform can update storage belonging to another live thread.
#	define DMibPSupportSetThreadLocalForOtherThread
#endif

#if defined(DPlatformFamily_macOS) || defined(DPlatformFamily_Linux)
#	define DMibPSupportThreadDestroyNotification
#	define DMibPSupportThreadLocalDestructors
	// The pthread introspection hook on macOS and the host pthread override on
	// Linux notify every participating thread before it runs user code.
#	if defined(DPlatformFamily_macOS) && defined(DMibConfig_PThreadIntrospection)
#		define DMibPSupportAlwaysCreatedThreadLocal
#		define DMibPSupportThreadCreateNotification
#	elif defined(DPlatformFamily_Linux) && defined(DMibConfig_LinuxPThreadMonitoring) && (defined(DMibStaticThreadLocals) || defined(DMibAssumeGlibc))
#		define DMibPSupportAlwaysCreatedThreadLocal
#		define DMibPSupportThreadCreateNotification
#	endif
#elif defined(DPlatformFamily_Windows)
#	define DMibPSupportAlwaysCreatedThreadLocal
#	define DMibPSupportThreadCreateNotification
#	define DMibPSupportThreadDestroyNotification
#elif defined(DPlatformFamily_Emscripten)
#	define DMibSingleThreaded
#else
#	error "Implement this"
#endif
