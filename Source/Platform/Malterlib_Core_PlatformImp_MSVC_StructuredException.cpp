// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "windows.h"
#include <Mib/Core/Core>

namespace NMib
{
	namespace NPlatform
	{
		void fg_GenerateExcetionHandler(void *_pData, LONG (*_pCallback)(struct _EXCEPTION_POINTERS *_pExceptionInfo, void *_pData))
		{
			__try
			{
				*((int *)0) = 0;
			}
			__except(_pCallback(GetExceptionInformation(), _pData))
			{
			}
		}
	}
}

class CMemoryToucher : public NMib::CVirtualDestructor
{
	class CInternal;
	NMib::NPtr::TCUniquePointer<CInternal> m_pInternal;
public:
	CMemoryToucher(fp64 _CPUUsage);
	~CMemoryToucher();		
};

class CMemoryToucher::CInternal : public NMib::NThread::CThread
{

	fp64 m_CPUUsage;
public:
	CInternal(fp64 _CPUUsage)
		: m_CPUUsage(NMib::fg_Clamp(_CPUUsage, 0.0, 0.99))
	{
	};

private:
	
	void fp_Touch(void *_pAddress, mint _Size)
	{
		try
		{
			volatile uint8 *pAddress = (uint8 *)_pAddress;
			volatile uint8 ReadTo;
			volatile uint8 *pEndAddress = pAddress + _Size;
			for (; pAddress < pEndAddress; pAddress += 4096)
			{
				ReadTo = *pAddress;
			}
		}
		catch (...)
		{
		}
	}

	aint f_Main() override
	{
		mint StartAddress = NMib::TCLimitsInt<mint>::mc_Min;
		mint EndAddress = NMib::TCLimitsInt<mint>::mc_Max;
		mint CurrentAddress = StartAddress;
		mint MaxPerTime = 1*1024*1024;
		mint TotalScannedMemory = 0;
		bool bUseRunTime = m_CPUUsage != 0.0;
		fp64 RunTime = 0.0;
		if (bUseRunTime)
			RunTime = -(m_CPUUsage / (fp64(10.0) * m_CPUUsage - fp64(10.0)));
		NMib::NTime::CClock TotalTimer;
		TotalTimer.f_Start();
		while (f_GetState() != NMib::NThread::EThreadState_EventWantQuit)
		{
			mint ScannedMemory = 0;
			NMib::NTime::CClock Timer;
			Timer.f_Start();
			for (mint i = 0; true; ++i)
			{
				MEMORY_BASIC_INFORMATION MemInfo;
				if (!VirtualQuery((void *)CurrentAddress, &MemInfo, sizeof(MemInfo)))
				{
#ifdef DConfig_ReleaseTesting
					DMibTrace("Scanned {} bytes in {} seconds!!!\r\n", TotalScannedMemory << TotalTimer.f_GetTime());
#endif
					TotalScannedMemory = 0;
					CurrentAddress = StartAddress;
					TotalTimer.f_Start();
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
				if (CurrentAddress == (mint)MemInfo.BaseAddress + MemInfo.RegionSize)
				{
#ifdef DConfig_ReleaseTesting
					DMibTrace("Scanned {} bytes in {} seconds 2!!!\r\n", TotalScannedMemory << TotalTimer.f_GetTime());
#endif
					TotalScannedMemory = 0;
					CurrentAddress = StartAddress;
					TotalTimer.f_Start();
				}
				else
					CurrentAddress = (mint)MemInfo.BaseAddress + MemInfo.RegionSize;
				if ((ScannedMemory > MaxPerTime || i > 1000) && (!bUseRunTime || Timer.f_GetTime() > RunTime))
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
	m_pInternal = NMib::fg_Construct(_CPUUsage);
	m_pInternal->f_Start();
}

CMemoryToucher::~CMemoryToucher()
{
	m_pInternal->f_Stop();
	m_pInternal.f_Clear();
}
