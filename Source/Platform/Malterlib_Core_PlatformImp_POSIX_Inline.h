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

		inline_always umint fg_Mem_VirtualGranularityAlloc(bool _bLargePages)
		{
			return NPrivate::g_PageSize;
		}

		inline_always umint fg_Mem_VirtualGranularityCommit(bool _bLargePages)
		{
			return NPrivate::g_PageSize;
		}

		inline_always umint fg_Mem_VirtualGranularityProtect(bool _bLargePages)
		{
			return NPrivate::g_PageSize;
		}

		inline_always fp32 fg_Mem_VirtualOverhead(void const *_pMem)
		{
			return 0.0f;
		}

		constexpr inline_always bool fg_Mem_VirtualCanCommit()
		{
			return true;
		}

		inline_always bool fg_Mem_VirtualCanProtect()
		{
			return true;
		}

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
		inline_always umint fg_GetThreadPointer()
		{
		#if defined(DPlatformFamily_macOS)
			return (umint)__builtin_thread_pointer() & ~umint(0x7);
		#else
			return (umint)__builtin_thread_pointer();
		#endif
		}
#endif

		inline_always umint fg_GetThreadSelf()
		{
		#ifdef DMibSafeThreadLocals
			return fg_GetThreadSelf_Safe();
		#elif defined(DPlatformFamily_macOS)
			#if DPlatformVersion >= 1070
				umint Return;
				#if defined(__i386__)
					umint __attribute__((address_space(256))) *pAddress = 0;
					Return = *pAddress;
				#elif defined(__x86_64__)
					umint __attribute__((address_space(256))) *pAddress = 0;
					Return = *pAddress;
				#elif defined(__aarch64__)
					Return = fg_GetThreadPointer() - sizeof(void *) * 28;
				#else
					#error "Not Implemented"
				#endif

				DMibFastCheck(Return == (umint)fg_GetThreadSelf_Safe());
				return Return;

			#elif DPlatformVersion >= 1050
				umint Return;

				#if defined(__i386__)
					umint __attribute__((address_space(256))) *pAddress = (umint __attribute__((address_space(256))) *)g_ThreadSelfOffset;
					Return = *pAddress;
				#elif defined(__x86_64__)
					umint __attribute__((address_space(256))) *pAddress = (umint __attribute__((address_space(256))) *)g_ThreadSelfOffset;
					Return = *pAddress;
				#elif defined(__ppc__) || defined(__ppc64__)
					return fg_GetThreadSelf_Safe();
				#else
					#error "Not Implemented"
				#endif

				DMibFastCheck(Return == fg_GetThreadSelf_Safe());
				return Return;
			#else
				#warning "Should be implemented"
				return  fg_GetThreadSelf_Safe();
			#endif
		#elif defined(DPlatformFamily_Linux)
			#ifdef DMibAssumeGlibc
				umint Return;
				#if defined(__i386__)
					umint __attribute__((address_space(256))) *pAddress = (umint __attribute__((address_space(256))) *)0x8;
					Return = *pAddress;
				#elif defined(__x86_64__)
					umint __attribute__((address_space(257))) *pAddress = (umint __attribute__((address_space(257))) *)0x10;
					Return = *pAddress;
				#elif defined(__aarch64__)
					Return = fg_GetThreadPointer() - g_ThreadSelfOffset;
				#else
					#error "Not Implemented"
				#endif

				DMibFastCheck(Return == fg_GetThreadSelf_Safe());
				return Return;
			#else
				return fg_GetThreadSelf_Safe();
			#endif
		#elif defined(DPlatformFamily_Emscripten)
			return  fg_GetThreadSelf_Safe();
		#else
			#warning "Should be implemented"
			return  fg_GetThreadSelf_Safe();
		#endif
		}

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

			inline_always umint fg_GetThreadLocal_Glibc(umint _iVariable)
			{
				DMibFastCheck(_iVariable > 0);

				auto pLevel2Pointers = reinterpret_cast<uint8 **>(reinterpret_cast<uint8 *>(fg_GetThreadPointer()) + g_GlibcThreadLocalOffsetThreadPointer);
				uint8 *pLevel2 = pLevel2Pointers[_iVariable >> gc_nGlibcThreadLocalLevel2Shift];
				if (!pLevel2) [[unlikely]]
					return 0;

				umint *pKeyData = reinterpret_cast<umint *>(pLevel2) + (_iVariable & (gc_nGlibcThreadLocalsPerLevel2 - 1)) * gc_nGlibcThreadLocalWordsPerKey;
				umint Data = pKeyData[gc_iGlibcThreadLocalData];
				if (!Data)
					return 0;

				if (pKeyData[gc_iGlibcThreadLocalSequence] != g_pGlibcThreadKeys[_iVariable * gc_nGlibcThreadLocalWordsPerKey]) [[unlikely]]
				{
					pKeyData[gc_iGlibcThreadLocalData] = 0;
					return 0;
				}

				return Data;
			}

			inline_always umint fg_GetThreadLocalAlwaysSet_Glibc(umint _iVariable)
			{
				DMibFastCheck(_iVariable > 0);

				auto pLevel2Pointers = reinterpret_cast<uint8 **>(reinterpret_cast<uint8 *>(fg_GetThreadPointer()) + g_GlibcThreadLocalOffsetThreadPointer);
				uint8 *pLevel2 = pLevel2Pointers[_iVariable >> gc_nGlibcThreadLocalLevel2Shift];
				DMibFastCheck(pLevel2);

				umint *pKeyData = reinterpret_cast<umint *>(pLevel2) + (_iVariable & (gc_nGlibcThreadLocalsPerLevel2 - 1)) * gc_nGlibcThreadLocalWordsPerKey;
				umint Data = pKeyData[gc_iGlibcThreadLocalData];
				DMibFastCheck(!Data || pKeyData[gc_iGlibcThreadLocalSequence] == g_pGlibcThreadKeys[_iVariable * gc_nGlibcThreadLocalWordsPerKey]);

				return Data;
			}

			inline_always umint fg_GetThreadLocalFast_Glibc(umint _iVariable)
			{
				DMibFastCheck(_iVariable > 0 && _iVariable < gc_nGlibcThreadLocalsPerLevel2);

				auto pLevel2Pointers = reinterpret_cast<uint8 **>(reinterpret_cast<uint8 *>(fg_GetThreadPointer()) + g_GlibcThreadLocalOffsetThreadPointer);
				uint8 *pLevel2 = pLevel2Pointers[0];
				DMibFastCheck(pLevel2);

				umint *pKeyData = reinterpret_cast<umint *>(pLevel2) + _iVariable * gc_nGlibcThreadLocalWordsPerKey;
				umint Data = pKeyData[gc_iGlibcThreadLocalData];
				if (!Data)
					return 0;

				if (pKeyData[gc_iGlibcThreadLocalSequence] != g_pGlibcThreadKeys[_iVariable * gc_nGlibcThreadLocalWordsPerKey]) [[unlikely]]
				{
					pKeyData[gc_iGlibcThreadLocalData] = 0;
					return 0;
				}

				return Data;
			}

			inline_always umint fg_GetThreadLocalAlwaysSetFast_Glibc(umint _iVariable)
			{
				DMibFastCheck(_iVariable > 0 && _iVariable < gc_nGlibcThreadLocalsPerLevel2);

				auto pLevel2Pointers = reinterpret_cast<uint8 **>(reinterpret_cast<uint8 *>(fg_GetThreadPointer()) + g_GlibcThreadLocalOffsetThreadPointer);
				uint8 *pLevel2 = pLevel2Pointers[0];
				DMibFastCheck(pLevel2);

				umint *pKeyData = reinterpret_cast<umint *>(pLevel2) + _iVariable * gc_nGlibcThreadLocalWordsPerKey;
				umint Data = pKeyData[gc_iGlibcThreadLocalData];
				DMibFastCheck(!Data || pKeyData[gc_iGlibcThreadLocalSequence] == g_pGlibcThreadKeys[_iVariable * gc_nGlibcThreadLocalWordsPerKey]);
				return Data;
			}
		}
	#endif

		umint fg_GetThreadLocal_Safe(umint _iVariable);

		inline_always umint fg_GetThreadLocal(umint _iVariable)
		{
		#ifdef DMibSafeThreadLocals
			return fg_GetThreadLocal_Safe(_iVariable);
		#elif defined(DPlatformFamily_macOS)
			#if DPlatformVersion >= 1070
				umint Return;
				#if defined(__i386__)
					umint __attribute__((address_space(256))) *pAddress = (umint __attribute__((address_space(256))) *)(_iVariable * 4);
					Return = *pAddress;
				#elif defined(__x86_64__)
					umint __attribute__((address_space(256))) *pAddress = (umint __attribute__((address_space(256))) *)(_iVariable * 8);
					Return = *pAddress;
				#elif defined(__aarch64__)
					umint *pThreadLocals = (umint *)fg_GetThreadPointer();
					Return = pThreadLocals[_iVariable];
				#else
					#error "Not Implemented"
				#endif
				DMibFastCheck(Return == fg_GetThreadLocal_Safe(_iVariable));
				return Return;
			#elif DPlatformVersion >= 1050
				umint Return;
				#if defined(__i386__)
					umint __attribute__((address_space(256))) *pAddress = (umint __attribute__((address_space(256))) *)(_iVariable * 4 + g_ThreadLocalOffset);
					Return = *pAddress;
				#elif defined(__x86_64__)
					umint __attribute__((address_space(256))) *pAddress = (umint __attribute__((address_space(256))) *)(_iVariable * 8 + g_ThreadLocalOffset);
					Return = *pAddress;
				#elif defined(__ppc__) || defined(__ppc64__)
					return fg_GetThreadLocal_Safe(_iVariable);
				#else
					#error "Not Implemented"
				#endif
				DMibFastCheck(Return == fg_GetThreadLocal_Safe(_iVariable));
				return Return;
			#else
				#warning "Should be implemented"
				return fg_GetThreadLocal_Safe(_iVariable);
			#endif
		#elif defined(DPlatformFamily_Linux)
			#ifdef DMibStaticThreadLocals
				umint Return;
				#if defined(__i386__)
					umint __attribute__((address_space(256))) *pAddress = (umint __attribute__((address_space(256))) *)(_iVariable);
					Return = *pAddress;
				#elif defined(__x86_64__)
					umint __attribute__((address_space(257))) *pAddress = (umint __attribute__((address_space(257))) *)(_iVariable);
					Return = *pAddress;
				#elif defined(__aarch64__)
					Return = *((umint *)(fg_GetThreadPointer() + _iVariable));
				#else
					#error "Not Implemented"
				#endif
				return Return;
			#elif defined(DMibConfig_LinuxOptimizeGlibcThreadLocals)
				umint Return = NPrivate::fg_GetThreadLocal_Glibc(_iVariable);
				DMibFastCheck(Return == fg_GetThreadLocal_Safe(_iVariable));
				return Return;
			#else
				return fg_GetThreadLocal_Safe(_iVariable);
			#endif
		#elif defined(DPlatformFamily_Emscripten)
			return fg_GetThreadLocal_Safe(_iVariable);
		#else
			#warning "Should be implemented"
			return fg_GetThreadLocal_Safe(_iVariable);
		#endif
		}

		inline_always void *fg_Thread_GetLocal(umint _iStorage)
		{
			return (void *)fg_GetThreadLocal(_iStorage);
		}

		inline_always void *fg_Thread_GetLocalAlwaysSet(umint _iStorage)
		{
	#if defined(DPlatformFamily_Linux) && defined(DMibConfig_LinuxOptimizeGlibcThreadLocals) && !defined(DMibStaticThreadLocals) && !defined(DMibSafeThreadLocals)
			return (void *)NPrivate::fg_GetThreadLocalAlwaysSet_Glibc(_iStorage);
	#else
			return (void *)fg_GetThreadLocal(_iStorage);
	#endif
		}

	#if !defined(DPlatformFamily_Linux) || !defined(DMibConfig_LinuxOptimizeGlibcThreadLocals) || defined(DMibStaticThreadLocals)
		inline_always umint fg_Thread_AllocLocalFast()
		{
			return fg_Thread_AllocLocal();
		}
	#endif

		inline_always void fg_Thread_FreeLocalFast(umint _iStorage)
		{
			return fg_Thread_FreeLocal(_iStorage);
		}

		inline_always void fg_Thread_SetLocalFast(umint _iStorage, void *_pData)
		{
			return fg_Thread_SetLocal(_iStorage, _pData);
		}

		inline_always void fg_Thread_SetLocalFast(umint _ThreadID, umint _iStorage, void *_pData)
		{
			return fg_Thread_SetLocal(_ThreadID, _iStorage, _pData);
		}

		inline_always void *fg_Thread_GetLocalFast(umint _iVariable)
		{
#if defined(DPlatformFamily_Linux)
			umint Return;
			#ifdef DMibStaticThreadLocals
				#if defined(__i386__)
					umint __attribute__((address_space(256))) *pAddress = (umint __attribute__((address_space(256))) *)(_iVariable);
					Return = *pAddress;
				#elif defined(__x86_64__)
					umint __attribute__((address_space(257))) *pAddress = (umint __attribute__((address_space(257))) *)(_iVariable);
					Return = *pAddress;
				#elif defined(__aarch64__)
					Return = *((umint *)(fg_GetThreadPointer() + _iVariable));
				#else
					#error "Not Implemented"
				#endif
			#elif defined(DMibConfig_LinuxOptimizeGlibcThreadLocals)
				Return = NPrivate::fg_GetThreadLocalFast_Glibc(_iVariable);
			#else
				return (void *)fg_GetThreadLocal(_iVariable);
			#endif
			return (void *)Return;
#else
			return (void *)fg_GetThreadLocal(_iVariable);
#endif
		}

		inline_always void *fg_Thread_GetLocalAlwaysSetFast(umint _iStorage)
		{
	#if defined(DPlatformFamily_Linux) && defined(DMibConfig_LinuxOptimizeGlibcThreadLocals) && !defined(DMibStaticThreadLocals)
			return (void *)NPrivate::fg_GetThreadLocalAlwaysSetFast_Glibc(_iStorage);
	#else
			return fg_Thread_GetLocalAlwaysSet(_iStorage);
	#endif
		}

		inline_always void *fg_Thread_GetCurrent()
		{
			return (void *)fg_GetThreadSelf();
		}

		inline_always umint fg_Thread_GetCurrentUID()
		{
			return fg_GetThreadSelf();
		}

		umint fg_Thread_GetCurrentUIDAlternate();
	}
}
