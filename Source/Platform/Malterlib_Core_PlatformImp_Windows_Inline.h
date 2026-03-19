// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

//#define DMibWindowsUseArbitraryUserPointerForThreadLocals

namespace NMib::NThread::NPlatform
{
	struct CWindowsThreadLocals
	{
		static constexpr umint mc_ThreadLocalSlots = 256; // 2 KiB

#ifdef DMibWindowsUseArbitraryUserPointerForThreadLocals
		static constexpr uint32 ms_ThreadLocalsExtendedLocactionOffset = sizeof(void *) * 5;
#else
		static uint32 ms_ThreadLocalsExtendedLocactionOffset;
#endif
		static umint ms_ThreadLocalsMinOffset;
		static umint ms_ThreadLocalsMaxOffset;

		void *m_ThreadLocals[mc_ThreadLocalSlots];
	};
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
			extern umint g_VirtualAllocGranularity;
			extern umint g_VirtualAllocGranularityLarge;
			extern umint g_PageSizeLarge;
		}
		inline_always umint fg_Mem_VirtualGranularityAlloc(bool _bLargePages)
		{
			if (_bLargePages)
				return NPrivate::g_VirtualAllocGranularityLarge;
			else
				return NPrivate::g_VirtualAllocGranularity;
		}

		inline_always umint fg_Mem_VirtualGranularityCommit(bool _bLargePages)
		{
			if (_bLargePages)
				return NPrivate::g_PageSizeLarge;
			else
				return 4096;
		}

		inline_always umint fg_Mem_VirtualGranularityProtect(bool _bLargePages)
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
			inline_always tf_CType fg_GetTebData(umint _iIndex)
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

		inline_always void *fg_Thread_GetLocal(umint _iStorage)
		{
			using namespace NMib::NThread::NPlatform;

			DMibFastCheck(_iStorage <= CWindowsThreadLocals::mc_ThreadLocalSlots);
			auto *pThreadLocals = NPrivate::fg_GetTebData<CWindowsThreadLocals *>(CWindowsThreadLocals::ms_ThreadLocalsExtendedLocactionOffset);
 			if (pThreadLocals)
				return pThreadLocals->m_ThreadLocals[_iStorage];
			return nullptr;
		}

		inline_always void *fg_Thread_GetLocalAlwaysSet(umint _iStorage)
		{
			using namespace NMib::NThread::NPlatform;

			DMibFastCheck(_iStorage <= CWindowsThreadLocals::mc_ThreadLocalSlots);
			auto *pThreadLocals = NPrivate::fg_GetTebData<CWindowsThreadLocals *>(CWindowsThreadLocals::ms_ThreadLocalsExtendedLocactionOffset);
			DMibFastCheck(pThreadLocals);
			return pThreadLocals->m_ThreadLocals[_iStorage];
		}

		inline_always void *fg_Thread_GetLocalFast(umint _iStorage)
		{
			using namespace NMib::NThread::NPlatform;

			DMibFastCheck(_iStorage >= CWindowsThreadLocals::CWindowsThreadLocals::ms_ThreadLocalsMinOffset);
			DMibFastCheck(_iStorage <= CWindowsThreadLocals::CWindowsThreadLocals::ms_ThreadLocalsMaxOffset);
			return NPrivate::fg_GetTebData<void *>(_iStorage);
		}

		inline_always void *fg_Thread_GetLocalAlwaysSetFast(umint _iStorage)
		{
			using namespace NMib::NThread::NPlatform;

			DMibFastCheck(_iStorage >= CWindowsThreadLocals::CWindowsThreadLocals::ms_ThreadLocalsMinOffset);
			DMibFastCheck(_iStorage <= CWindowsThreadLocals::CWindowsThreadLocals::ms_ThreadLocalsMaxOffset);
			return NPrivate::fg_GetTebData<void *>(_iStorage);
		}

		inline_always umint fg_Thread_GetCurrentUID()
		{
			// Read directly from TIB (Thread Information Block)
#			if defined(DArchitecture_arm64)
				return NPrivate::fg_GetTebData<umint>(0x48);
#			elif defined(DArchitecture_x64)
				return NPrivate::fg_GetTebData<umint>(0x48);
#			else
				return NPrivate::fg_GetTebData<umint>(0x24);
#			endif
		}

		inline_always umint fg_Thread_GetCurrentUIDAlternate()
		{
			return fg_Thread_GetCurrentUID();
		}

	}
}

