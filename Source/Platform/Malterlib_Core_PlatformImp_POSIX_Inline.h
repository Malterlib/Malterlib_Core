// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

extern "C"
{
	#include <pthread.h>

#ifdef DPlatformFamily_OSX
	#include <sys/syscall.h>
	#include <unistd.h>
#endif
#ifdef DPlatformFamily_Linux
	#include <sys/syscall.h>
	#include <unistd.h>
#endif
}

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
#endif
		
		inline_always volatile mint fg_GetThreadSelf()
		{
		#ifdef DMibSafeThreadLocals
			return  (mint)pthread_self();
		#elif defined(DPlatformFamily_OSX)
			#if DPlatformVersion >= 1070
				mint Return;
				#if defined(__i386__)
					asm ("mov %%gs:0x0,%0" : "=r"(Return) : );
				#elif defined(__x86_64__)
					asm ("mov %%gs:0x0,%0" : "=r"(Return) : );
				#else
					#error "Not Implemented"
				#endif
				
				DMibFastCheck(Return == (mint)pthread_self());
				return Return;
				
			#elif DPlatformVersion >= 1050
				mint Return;
				
				#if defined(__i386__)
					asm ("mov %%gs:0x0(%1),%0" : "=r"(Return) : "r"(g_ThreadSelfOffset) );
				#elif defined(__x86_64__)
					asm ("mov %%gs:0x0(%1),%0" : "=r"(Return) : "r"(g_ThreadSelfOffset) );
				#elif defined(__ppc__) || defined(__ppc64__)
					return  (mint)pthread_self();
				#else
					#error "Not Implemented"
				#endif
				
				DMibFastCheck(Return == (mint)pthread_self());
				return Return;
			#else
				#warning "Should be implemented"
				return  (mint)pthread_self();
			#endif
		#elif defined(DPlatformFamily_Linux)
			#ifdef DMibAssumeGlibc
				mint Return;
				#if defined(__i386__)
					asm ("mov %%gs:0x8,%0" : "=r"(Return) : );
				#elif defined(__x86_64__)
					asm ("mov %%fs:0x10,%0" : "=r"(Return) : );
				#else
					#error "Not Implemented"
				#endif
				
				DMibFastCheck(Return == (mint)pthread_self());
				return Return;
			#else
				return (mint)pthread_self();
			#endif
		#elif defined(DPlatformFamily_Emscripten)
			return  (mint)pthread_self();
		#else
			#warning "Should be implemented"
			return  (mint)pthread_self();
		#endif
		}

		inline_always volatile mint fg_GetThreadLocal(mint _iVariable)
		{
		#ifdef DMibSafeThreadLocals
			return (mint)pthread_getspecific((pthread_key_t)_iVariable);
		#elif defined(DPlatformFamily_OSX)
			#if DPlatformVersion >= 1070
				mint Return;
				#if defined(__i386__)
					asm ("mov %%gs:0x0(,%1,4),%0" : "=r"(Return) : "r"(_iVariable));
				#elif defined(__x86_64__)
					asm ("mov %%gs:0x0(,%1,8),%0" : "=r"(Return) : "r"(_iVariable));
				#else
					#error "Not Implemented"
				#endif
				DMibFastCheck(Return == (mint)pthread_getspecific((pthread_key_t)_iVariable));
				return Return;
			#elif DPlatformVersion >= 1050
				mint Return;
				#if defined(__i386__)
					asm ("mov %%gs:0x0(%2,%1,4),%0" : "=r"(Return) : "r"(_iVariable), "r"(g_ThreadLocalOffset) );
				#elif defined(__x86_64__)
					asm ("mov %%gs:0x0(%2,%1,8),%0" : "=r"(Return) : "r"(_iVariable), "r"(g_ThreadLocalOffset) );
				#elif defined(__ppc__) || defined(__ppc64__)
					return (mint)pthread_getspecific((pthread_key_t)_iVariable);
				#else
					#error "Not Implemented"
				#endif
				DMibFastCheck(Return == (mint)pthread_getspecific((pthread_key_t)_iVariable));
				return Return;
			#else
				#warning "Should be implemented"
				return (mint)pthread_getspecific((pthread_key_t)_iVariable);
			#endif
		#elif defined(DPlatformFamily_Linux)
			#ifdef DMibStaticThreadLocals
				mint Return;
				#if defined(__i386__)
					asm ("mov %%gs:0x0(, %1),%0" : "=r"(Return) : "r"(_iVariable));
				#elif defined(__x86_64__)
					asm ("mov %%fs:0x0(, %1),%0" : "=r"(Return) : "r"(_iVariable));
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
				DMibFastCheck(Return == (mint)pthread_getspecific((pthread_key_t)_iVariable));
				return Return;*/
			#else
				return (mint)pthread_getspecific((pthread_key_t)_iVariable);
			#endif
		#elif defined(DPlatformFamily_Emscripten)
			return (mint)pthread_getspecific((pthread_key_t)_iVariable);
		#else
			#warning "Should be implemented"
			return (mint)pthread_getspecific((pthread_key_t)_iVariable);
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
				DMibFastCheck(Return == (mint)pthread_getspecific((pthread_key_t)_iVariable));*/
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
		inline_always mint fg_Thread_GetCurrentUIDAlternate()
		{
#ifdef DPlatformFamily_OSX
#		if DPlatformVersion < 1060
			if (CSystem::ms_PlatformVersion < 10'06'00)
				return fg_Thread_GetCurrentUID();
#		endif
DMibDeprecatedSupressStart;
			return syscall(SYS_thread_selfid);
DMibDeprecatedSupressStop;
#elif defined(DPlatformFamily_Linux)
			return syscall(SYS_gettid);
#elif defined(DPlatformFamily_Emscripten)
			return fg_Thread_GetCurrentUID();
#endif
		}		
	}
}
