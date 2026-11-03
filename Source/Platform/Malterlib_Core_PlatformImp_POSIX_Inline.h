// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

namespace NMib
{
	namespace NSys
	{
		// *************************************************************************************************************************
		// POSIX virtual memory allocation
		// *************************************************************************************************************************

		namespace NPrivate
		{
			extern umint g_PageSize;
		}

		inline_always umint fg_Mem_VirtualGranularityAlloc(bool _bLargePages);
		inline_always umint fg_Mem_VirtualGranularityCommit(bool _bLargePages);
		inline_always umint fg_Mem_VirtualGranularityProtect(bool _bLargePages);
		inline_always fp32 fg_Mem_VirtualOverhead(void const *_pMem);
		constexpr inline_always bool fg_Mem_VirtualCanCommit();
		inline_always bool fg_Mem_VirtualCanProtect();

		// *************************************************************************************************************************
		// POSIX Thread Implementation
		// *************************************************************************************************************************

#ifdef DPlatformFamily_macOS
		extern umint g_ThreadSelfOffset;
		extern umint g_ThreadLocalOffset;
#elif defined(DPlatformFamily_Linux) && defined(DArchitecture_arm64)
		extern umint g_ThreadSelfOffset;
#endif
#if defined(DPlatformFamily_Linux) && defined(DMibConfig_LinuxOptimizeGlibcThreadLocals) && !defined(DMibStaticThreadLocals)
		extern smint g_GlibcThreadLocalOffsetThreadPointer;
		extern umint const *g_pGlibcThreadKeys;
#endif

		umint fg_GetThreadSelf_Safe();

#if defined(__aarch64__) || (defined(DPlatformFamily_Linux) && (defined(__i386__) || defined(__x86_64__)))
		inline_always umint fg_GetThreadPointer();
#endif

		inline_always umint fg_GetThreadSelf();

#if defined(DPlatformFamily_Linux) && defined(DMibConfig_LinuxOptimizeGlibcThreadLocals) && !defined(DMibStaticThreadLocals)
		namespace NPrivate
		{
			// Allocation validates that glibc uses 32 two-word entries per level 2.
			// Keeping the real pthread key makes its high and low bits the block and entry indices.
			constexpr umint gc_nGlibcThreadLocalLevel2Shift = 5;
			constexpr umint gc_nGlibcThreadLocalsPerLevel2 = umint(1) << gc_nGlibcThreadLocalLevel2Shift;
			constexpr umint gc_nGlibcThreadLocalWordsPerKey = 2;
			constexpr umint gc_iGlibcThreadLocalSequence = 0;
			constexpr umint gc_iGlibcThreadLocalData = 1;

			inline_always umint fg_GetThreadLocal_Glibc(umint _iVariable);
			inline_always umint fg_GetThreadLocalAlwaysSet_Glibc(umint _iVariable);
			inline_always umint fg_GetThreadLocalFast_Glibc(umint _iVariable);
			inline_always umint fg_GetThreadLocalAlwaysSetFast_Glibc(umint _iVariable);
		}
#endif

		umint fg_GetThreadLocal_Safe(umint _iVariable);

		inline_always umint fg_GetThreadLocal(umint _iVariable);
		inline_always void *fg_Thread_GetLocal(umint _iStorage);
		inline_always void *fg_Thread_GetLocalAlwaysSet(umint _iStorage);

#if !defined(DPlatformFamily_Linux) || !defined(DMibConfig_LinuxOptimizeGlibcThreadLocals) || defined(DMibStaticThreadLocals)
		inline_always umint fg_Thread_AllocLocalFast();
#endif

		inline_always void fg_Thread_FreeLocalFast(umint _iStorage);
		inline_always void fg_Thread_SetLocalFast(umint _iStorage, void *_pData);
		inline_always void fg_Thread_SetLocalFast(umint _ThreadID, umint _iStorage, void *_pData);
		inline_always void *fg_Thread_GetLocalFast(umint _iVariable);
		inline_always void *fg_Thread_GetLocalAlwaysSetFast(umint _iStorage);
		inline_always void *fg_Thread_GetCurrent();
		inline_always umint fg_Thread_GetCurrentUID();

		umint fg_Thread_GetCurrentUIDAlternate();
	}
}

#include "Malterlib_Core_PlatformImp_POSIX_Inline.hpp"
