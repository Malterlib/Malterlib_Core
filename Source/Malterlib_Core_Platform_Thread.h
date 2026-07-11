// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

// Possible values:
// DMibPSupportAlwaysCreatedThreadLocal
// DMibPSupportThreadCreateNotification
// DMibPSupportThreadDestroyNotification
// DMibPSupportThreadLocalDestructors

#if defined(DPlatformFamily_macOS) || defined(DPlatformFamily_Linux)
#	define DMibPSupportThreadDestroyNotification
#	define DMibPSupportThreadLocalDestructors
	// The pthread introspection hook notifies about every thread in the
	// process, which enables always-created thread locals on macOS
#	if defined(DPlatformFamily_macOS) && defined(DMibConfig_PThreadIntrospection)
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

