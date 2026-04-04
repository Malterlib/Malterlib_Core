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

		umint fg_GetThreadSelf_Safe();

#if defined(__aarch64__)
		inline_always umint fg_GetThreadPointer()
		{
#if defined(DPlatformFamily_macOS)
			return (umint)__builtin_thread_pointer() & ~umint(0x7);
#endif
			return (umint)__builtin_thread_pointer();
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
/*			#elif defined(DMibAssumeGlibc)
				umint Return;
				#if defined(__i386__)
					asm ("mov %%gs:0x0(%2,%1,4),%0" : "=r"(Return) : "r"(_iVariable * 2 + 1), "r"(DMibPOffsetOf(pthread, specific_1stblock)));
				#elif defined(__x86_64__)
					asm ("mov %%fs:0x318(,%1,8),%0" : "=r"(Return) : "r"(_iVariable * 2));
				#else
					#error "Not Implemented"
				#endif
				DMibFastCheck(Return == fg_GetThreadLocal_Safe(_iVariable));
				return Return;*/
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
			return (void *)fg_GetThreadLocal(_iStorage);
		}

		inline_always umint fg_Thread_AllocLocalFast()
		{
			return fg_Thread_AllocLocal();
		}

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
/*			#elif defined(DMibAssumeGlibc)
				umint Return;
				#if defined(__i386__)
					asm ("mov %%gs:0x0(%2,%1,4),%0" : "=r"(Return) : "r"(_iVariable * 2 + 1), "r"(DMibPOffsetOf(pthread, specific_1stblock)));
				#elif defined(__x86_64__)
					asm ("mov %%fs:0x318(,%1,8),%0" : "=r"(Return) : "r"(_iVariable * 2));
				#else
					#error "Not Implemented"
				#endif
				DMibFastCheck(Return == fg_GetThreadLocal_Safe(_iVariable));*/
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
			return fg_Thread_GetLocalAlwaysSet(_iStorage);
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
