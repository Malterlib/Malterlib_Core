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
			extern mint g_VirtualAllocGranularity;
			extern mint g_VirtualAllocGranularityLarge;
			extern mint g_PageSizeLarge;
		}
		inline_always mint fg_Mem_VirtualGranularityAlloc(bool _bLargePages)
		{
			if (_bLargePages)
				return NPrivate::g_VirtualAllocGranularityLarge;
			else
				return NPrivate::g_VirtualAllocGranularity;
		}
		
		inline_always mint fg_Mem_VirtualGranularityCommit(bool _bLargePages)
		{
			if (_bLargePages)
				return NPrivate::g_PageSizeLarge;
			else
				return 4096;
		}
		
		inline_always mint fg_Mem_VirtualGranularityProtect(bool _bLargePages)
		{
			if (_bLargePages)
				return NPrivate::g_PageSizeLarge;
			else
				return 4096;
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

		///
		/// Thread locals
		/// =============


		namespace NPrivate
		{
			template <typename tf_CType>
			inline_always tf_CType fg_GetTebData(mint _iIndex)
			{
#				if defined(DArchitecture_arm64)
					return (tf_CType)__readx18qword(_iIndex);
#				elif defined(DArchitecture_x64)
					return (tf_CType)__readgsqword(_iIndex);
#				else
					return (tf_CType)__readfsdword(_iIndex);
#				endif
			}

		}

		inline_always void *fg_Thread_GetLocal(mint _iStorage)
		{
			DMibFastCheck(_iStorage <= 0x400);
#			if defined(DArchitecture_arm64)
				mint *pThreadLocalBlock = NPrivate::fg_GetTebData<mint *>(0x1780);
#			elif defined(DArchitecture_x64)
				mint *pThreadLocalBlock = NPrivate::fg_GetTebData<mint *>(0x1780);
#			else
				mint *pThreadLocalBlock = NPrivate::fg_GetTebData<mint *>(0xf94);
#			endif
			if (pThreadLocalBlock)
				return (void *)pThreadLocalBlock[_iStorage];
			return nullptr;
		}

		inline_always void *fg_Thread_GetLocalAlwaysSet(mint _iStorage)
		{
			DMibFastCheck(_iStorage <= 0x400);
#			if defined(DArchitecture_arm64)
				mint *pThreadLocalBlock = NPrivate::fg_GetTebData<mint *>(0x1780);
#			elif defined(DArchitecture_x64)
				mint *pThreadLocalBlock = NPrivate::fg_GetTebData<mint *>(0x1780);
#			else
				mint *pThreadLocalBlock = NPrivate::fg_GetTebData<mint *>(0xf94);
#			endif
			DMibFastCheck(pThreadLocalBlock);
			void *pRet = (void *)pThreadLocalBlock[_iStorage];
			return pRet;
		}

		inline_always void *fg_Thread_GetLocalFast(mint _iStorage)
		{
#			if defined(DArchitecture_arm64)
				return NPrivate::fg_GetTebData<void *>(_iStorage*8 + 0x1480);
#			elif defined(DArchitecture_x64)
				return NPrivate::fg_GetTebData<void *>(_iStorage*8 + 0x1480);
#			else
				return NPrivate::fg_GetTebData<void *>(_iStorage*4 + 900*4);
#			endif
		}

		inline_always void *fg_Thread_GetLocalAlwaysSetFast(mint _iStorage)
		{
			void *pRet =
#			if defined(DArchitecture_arm64)
				NPrivate::fg_GetTebData<void *>(_iStorage*8 + 0x1480);
#			elif defined(DArchitecture_x64)
				NPrivate::fg_GetTebData<void *>(_iStorage*8 + 0x1480);
#			else
				NPrivate::fg_GetTebData<void *>(_iStorage*4 + 900*4);
#			endif
			return pRet;
		}

		
		inline_always mint fg_Thread_GetCurrentUID()
		{	
			// Read directly from TIB (Thread Information Block)
#			if defined(DArchitecture_arm64)
				return NPrivate::fg_GetTebData<mint>(0x48);
#			elif defined(DArchitecture_x64)
				return NPrivate::fg_GetTebData<mint>(0x48);
#			else
				return NPrivate::fg_GetTebData<mint>(0x24);
#			endif
		}		

		inline_always mint fg_Thread_GetCurrentUIDAlternate()
		{
			return fg_Thread_GetCurrentUID();
		}		
		
	}
}

