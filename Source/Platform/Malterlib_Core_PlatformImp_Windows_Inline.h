// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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

		inline_always umint fg_Mem_VirtualGranularityAlloc(bool _bLargePages);
		inline_always umint fg_Mem_VirtualGranularityCommit(bool _bLargePages);
		inline_always umint fg_Mem_VirtualGranularityProtect(bool _bLargePages);
		inline_always fp32 fg_Mem_VirtualOverhead(void const *_pMem);
		constexpr inline_always bool fg_Mem_VirtualCanCommit();
		inline_always bool fg_Mem_VirtualCanProtect();

		///
		/// Thread locals
		/// =============

		namespace NPrivate
		{
			template <typename tf_CType>
			inline_always tf_CType fg_GetTebData(umint _iIndex);
		}

		inline_always void *fg_Thread_GetLocal(umint _iStorage);
		inline_always void *fg_Thread_GetLocalAlwaysSet(umint _iStorage);
		inline_always void *fg_Thread_GetLocalFast(umint _iStorage);
		inline_always void *fg_Thread_GetLocalAlwaysSetFast(umint _iStorage);
		inline_always umint fg_Thread_GetCurrentUID();
		inline_always umint fg_Thread_GetCurrentUIDAlternate();
	}
}

#include "Malterlib_Core_PlatformImp_Windows_Inline.hpp"
