// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "windows.h"
#include <Mib/Core/Core>

namespace NMib
{
	namespace NPlatform
	{
		void fg_GenerateExcetionHandler(void *_pData, LONG (*_fCallback)(struct _EXCEPTION_POINTERS *_pExceptionInfo, void *_pData))
		{
#if !defined(DCompiler_clang_cl) // clang-cl-workaround
			__try
			{
				*((int volatile *)0) = 0;
			}
			__except(_fCallback(GetExceptionInformation(), _pData))
			{
			}
#else
			*((int volatile *)0) = 0;
#endif
		}
	}
}

struct CMemoryToucher : public NMib::CVirtualDestructor
{
	CMemoryToucher(fp64 _CPUUsage);
	~CMemoryToucher();

private:
	struct CInternal;
	NMib::NStorage::TCUniquePointer<CInternal> mp_pInternal;
};

struct CMemoryToucher::CInternal : public NMib::NThread::CThread
{
	CInternal(fp64 _CPUUsage)
		: mp_CPUUsage(NMib::fg_Clamp(_CPUUsage, 0.0, 0.99))
	{
	};

private:
	fp64 mp_CPUUsage;

	void fp_Touch(void *_pAddress, umint _Size)
	{
		try
		{
			uint8 volatile *pAddress = (uint8 *)_pAddress;
			[[maybe_unused]] uint8 volatile ReadTo;
			uint8 volatile *pEndAddress = pAddress + _Size;
			for (; pAddress < pEndAddress; pAddress += 4096)
				ReadTo = *pAddress;
		}
		catch (...)
		{
		}
	}

	aint f_Main() override
	{
		umint StartAddress = NMib::TCLimitsInt<umint>::mc_Min;
		umint CurrentAddress = StartAddress;
		umint MaxPerTime = 1 * 1024 * 1024;
		[[maybe_unused]] umint TotalScannedMemory = 0;
		bool bUseRunTime = mp_CPUUsage != 0.0;
		fp64 RunTime = 0.0;
		if (bUseRunTime)
			RunTime = -(mp_CPUUsage / (fp64(10.0) * mp_CPUUsage - fp64(10.0)));
		NMib::NTime::CStopwatch TotalStopwatch;
		TotalStopwatch.f_Start();
		while (f_GetState() != NMib::NThread::EThreadState_EventWantQuit)
		{
			umint ScannedMemory = 0;
			NMib::NTime::CStopwatch Stopwatch;
			Stopwatch.f_Start();
			for (umint iIteration = 0; true; ++iIteration)
			{
				MEMORY_BASIC_INFORMATION MemInfo;
				if (!VirtualQuery((void *)CurrentAddress, &MemInfo, sizeof(MemInfo)))
				{
#ifdef DConfig_ReleaseTesting
					DMibTrace("Scanned {} bytes in {} seconds!!!\r\n", TotalScannedMemory, TotalStopwatch.f_GetTime());
#endif
					TotalScannedMemory = 0;
					CurrentAddress = StartAddress;
					TotalStopwatch.f_Start();
					continue;
				}
				if
				(
					(MemInfo.State & MEM_COMMIT)
					&& (MemInfo.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY | PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY))
					&& !(MemInfo.Protect & (PAGE_GUARD | PAGE_WRITECOMBINE | PAGE_NOCACHE))
				)
				{
					fp_Touch(MemInfo.BaseAddress, MemInfo.RegionSize);
					ScannedMemory += MemInfo.RegionSize;
					TotalScannedMemory += MemInfo.RegionSize;
				}
				if (CurrentAddress == (umint)MemInfo.BaseAddress + MemInfo.RegionSize)
				{
#ifdef DConfig_ReleaseTesting
					DMibTrace("Scanned {} bytes in {} seconds 2!!!\r\n", TotalScannedMemory, TotalStopwatch.f_GetTime());
#endif
					TotalScannedMemory = 0;
					CurrentAddress = StartAddress;
					TotalStopwatch.f_Start();
				}
				else
					CurrentAddress = (umint)MemInfo.BaseAddress + MemInfo.RegionSize;
				if ((ScannedMemory > MaxPerTime || iIteration > 1000) && (!bUseRunTime || Stopwatch.f_GetTime() > RunTime))
					break;
			}
			m_EventWantQuit.f_WaitTimeout(0.1);
		}
		return 0;
	}

	NMib::NStr::CStr f_GetThreadName() override
	{
		return "Memory Toucher";
	}
};

CMemoryToucher::CMemoryToucher(fp64 _CPUUsage)
{
	mp_pInternal = NMib::fg_Construct(_CPUUsage);
	mp_pInternal->f_Start();
}

CMemoryToucher::~CMemoryToucher()
{
	mp_pInternal->f_Stop();
	mp_pInternal.f_Clear();
}
