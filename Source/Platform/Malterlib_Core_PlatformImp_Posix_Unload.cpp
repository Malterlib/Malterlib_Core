// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Linked last into a dynamic library, which makes these the last finalizers the image registers
// and so the first ones it runs, ahead of every static destructor in the image, when the library
// is unloaded and when the process exits with it loaded. No priority can place them there: ELF
// sorts prioritized entries ahead of the plain ones on both sides. The destructor attribute goes
// through the registration list on macOS but through .fini_array on Linux, where it runs before
// anything registered at run time on dlclose and after everything on exit; the static object goes
// through the registration list on both, first among what was registered at load. Destructors
// registered after load, function-scope statics and atexit calls, run before both
#if defined(DPlatformFamily_macOS) || defined(DPlatformFamily_Linux)

extern "C" void fg_MalterlibPrepareUnloadThread();

extern "C" void __attribute__((destructor)) fg_PrepareUnloadMalterlib()
{
	fg_MalterlibPrepareUnloadThread();
}

struct CPrepareUnloadMalterlib
{
	~CPrepareUnloadMalterlib()
	{
		fg_MalterlibPrepareUnloadThread();
	}
};

CPrepareUnloadMalterlib g_PrepareUnloadMalterlib;

#endif
