// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

namespace NMib
{
	namespace NSys
	{
		// *************************************************************************************************************************
		// POSIX virtual memory allocation
		// *************************************************************************************************************************
		
		namespace NPrivate
		{
			extern mint g_PageSize;
		}

		inline_always mint fg_Mem_VirtualGranularityAlloc(bool _bLargePages)
		{
			return NPrivate::g_PageSize;
		}
		
		inline_always mint fg_Mem_VirtualGranularityCommit(bool _bLargePages)
		{
			return NPrivate::g_PageSize;
		}
		
		inline_always mint fg_Mem_VirtualGranularityProtect(bool _bLargePages)
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

#ifdef DPlatformFamily_OSX
		extern mint g_ThreadSelfOffset;
		extern mint g_ThreadLocalOffset;
#elif defined(DPlatformFamily_Linux) && defined(DArchitecture_arm64)
		extern mint g_ThreadSelfOffset;
#endif
		
		mint fg_GetThreadSelf_Safe();

		inline_always mint fg_GetThreadSelf()
		{
		#ifdef DMibSafeThreadLocals
			return fg_GetThreadSelf_Safe();
		#elif defined(DPlatformFamily_OSX)
			#if DPlatformVersion >= 1070
				mint Return;
				#if defined(__i386__)
					asm ("mov %%gs:0x0,%0" : "=r"(Return) : );
				#elif defined(__x86_64__)
					asm ("mov %%gs:0x0,%0" : "=r"(Return) : );
				#elif defined(__aarch64__)
					asm volatile ("mrs %0, tpidrro_el0" : "=r" (Return));
					Return &= 0xfffffffffffffff8ul;
					Return -= sizeof(void *) * 28;
				#else
					#error "Not Implemented"
				#endif

				DMibFastCheck(Return == (mint)fg_GetThreadSelf_Safe());
				return Return;
				
			#elif DPlatformVersion >= 1050
				mint Return;
				
				#if defined(__i386__)
					asm ("mov %%gs:0x0(%1),%0" : "=r"(Return) : "r"(g_ThreadSelfOffset) );
				#elif defined(__x86_64__)
					asm ("mov %%gs:0x0(%1),%0" : "=r"(Return) : "r"(g_ThreadSelfOffset) );
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
				mint Return;
				#if defined(__i386__)
					asm ("mov %%gs:0x8,%0" : "=r"(Return) : );
				#elif defined(__x86_64__)
					asm ("mov %%fs:0x10,%0" : "=r"(Return) : );
				#elif defined(__aarch64__)
					asm volatile ("mrs %0, TPIDR_EL0" : "=r" (Return));
					Return -= g_ThreadSelfOffset;
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

		mint fg_GetThreadLocal_Safe(mint _iVariable);

		inline_always mint fg_GetThreadLocal(mint _iVariable)
		{
		#ifdef DMibSafeThreadLocals
			return fg_GetThreadLocal_Safe(_iVariable);
		#elif defined(DPlatformFamily_OSX)
			#if DPlatformVersion >= 1070
				mint Return;
				#if defined(__i386__)
					asm ("mov %%gs:0x0(,%1,4),%0" : "=r"(Return) : "r"(_iVariable));
				#elif defined(__x86_64__)
					asm ("mov %%gs:0x0(,%1,8),%0" : "=r"(Return) : "r"(_iVariable));
				#elif defined(__aarch64__)
					mint ThreadLocals;
					asm volatile ("mrs %0, tpidrro_el0" : "=r" (ThreadLocals));
					ThreadLocals &= 0xfffffffffffffff8ul;
					Return = ((mint *)ThreadLocals)[_iVariable];
					//asm ("mrs %0, tpidr_el0" : "=r" (Return) : );
				#else
					#error "Not Implemented"
				#endif
				DMibFastCheck(Return == fg_GetThreadLocal_Safe(_iVariable));
				return Return;
			#elif DPlatformVersion >= 1050
				mint Return;
				#if defined(__i386__)
					asm ("mov %%gs:0x0(%2,%1,4),%0" : "=r"(Return) : "r"(_iVariable), "r"(g_ThreadLocalOffset) );
				#elif defined(__x86_64__)
					asm ("mov %%gs:0x0(%2,%1,8),%0" : "=r"(Return) : "r"(_iVariable), "r"(g_ThreadLocalOffset) );
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
				mint Return;
				#if defined(__i386__)
					asm ("mov %%gs:0x0(, %1),%0" : "=r"(Return) : "r"(_iVariable));
				#elif defined(__x86_64__)
					asm ("mov %%fs:0x0(, %1),%0" : "=r"(Return) : "r"(_iVariable));
				#elif defined(__aarch64__)
					mint ThreadLocals;
					asm volatile ("mrs %0, TPIDR_EL0" : "=r" (ThreadLocals));
					Return = *((mint *)(ThreadLocals + _iVariable));
				#else
					#error "Not Implemented"
				#endif
				return Return;
/*			#elif defined(DMibAssumeGlibc)
				mint Return;
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

		inline_always void *fg_Thread_GetLocal(mint _iStorage)
		{
			return (void *)fg_GetThreadLocal(_iStorage);
		}

		inline_always void *fg_Thread_GetLocalAlwaysSet(mint _iStorage)
		{
			return fg_Thread_GetLocal(_iStorage);
		}

		inline_always mint fg_Thread_AllocLocalFast()
		{
			return fg_Thread_AllocLocal();
		}

		inline_always void fg_Thread_FreeLocalFast(mint _iStorage)
		{
			return fg_Thread_FreeLocal(_iStorage);
		}

		inline_always void fg_Thread_SetLocalFast(mint _iStorage, void *_pData)
		{
			return fg_Thread_SetLocal(_iStorage, _pData);
		}

		inline_always void fg_Thread_SetLocalFast(mint _ThreadID, mint _iStorage, void *_pData)
		{
			return fg_Thread_SetLocal(_ThreadID, _iStorage, _pData);
		}

		inline_always void *fg_Thread_GetLocalFast(mint _iVariable)
		{
#if defined(DPlatformFamily_Linux)
			mint Return;
			#ifdef DMibStaticThreadLocals
				#if defined(__i386__)
					asm ("mov %%gs:0x0(, %1),%0" : "=r"(Return) : "r"(_iVariable));
				#elif defined(__x86_64__)
					asm ("mov %%fs:0x0(, %1),%0" : "=r"(Return) : "r"(_iVariable));
				#elif defined(__aarch64__)
					mint ThreadLocals;
					asm volatile ("mrs %0, TPIDR_EL0" : "=r" (ThreadLocals));
					Return = *((mint *)(ThreadLocals + _iVariable));
				#else
					#error "Not Implemented"
				#endif
/*			#elif defined(DMibAssumeGlibc)
				mint Return;
				#if defined(__i386__)
					asm ("mov %%gs:0x0(%2,%1,4),%0" : "=r"(Return) : "r"(_iVariable * 2 + 1), "r"(DMibPOffsetOf(pthread, specific_1stblock)));
				#elif defined(__x86_64__)
					asm ("mov %%fs:0x318(,%1,8),%0" : "=r"(Return) : "r"(_iVariable * 2));
				#else
					#error "Not Implemented"
				#endif
				DMibFastCheck(Return == fg_GetThreadLocal_Safe(_iVariable));*/
			#else
				return fg_Thread_GetLocal(_iVariable);
			#endif
			return (void *)Return;
#else
			return fg_Thread_GetLocal(_iVariable);
#endif
		}

		inline_always void *fg_Thread_GetLocalAlwaysSetFast(mint _iStorage)
		{
			return fg_Thread_GetLocalAlwaysSet(_iStorage);
		}

		inline_always void *fg_Thread_GetCurrent()
		{
			return (void *)fg_GetThreadSelf();
		}

		inline_always mint fg_Thread_GetCurrentUID()
		{	
			return fg_GetThreadSelf();
		}

		mint fg_Thread_GetCurrentUIDAlternate();
	}
}
