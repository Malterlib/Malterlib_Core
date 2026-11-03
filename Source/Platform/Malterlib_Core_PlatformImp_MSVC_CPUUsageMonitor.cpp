// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

namespace NMSVCRuntime
{
	struct CCPUUsageMonitor : public CCPUUsageMonitorLink
	{
		CCPUUsageMonitor()
		{
			GetSystemTimes(&mp_LastIdle, &mp_LastKernel, &mp_LastUser);
			mp_LastRet.m_User = 0.0f;
			mp_LastRet.m_Kernel = 0.0f;
			mp_LastRet.m_Idle = 0.0f;
		}

		virtual ~CCPUUsageMonitor()
		{
		}

	public:
		NMib::NSystem::CSystemCPUUsage f_GetUsage(bool &_bChanged)
		{
			// NOLINTBEGIN(malterlib-conceptual-prefix): FILETIME is a struct, which the check takes for a function type by its name
			FILETIME LastIdle;
			FILETIME LastUser;
			FILETIME LastKernel;
			// NOLINTEND(malterlib-conceptual-prefix)
			GetSystemTimes(&LastIdle, &LastKernel, &LastUser);

			uint64 IdleTime = uint64(LastIdle.dwHighDateTime) << 32 | uint64(LastIdle.dwLowDateTime);
			uint64 UserTime = uint64(LastUser.dwHighDateTime) << 32 | uint64(LastUser.dwLowDateTime);
			uint64 KernelTime = (uint64(LastKernel.dwHighDateTime) << 32 | uint64(LastKernel.dwLowDateTime)) - IdleTime;

			uint64 LastIdleTime = uint64(mp_LastIdle.dwHighDateTime) << 32 | uint64(mp_LastIdle.dwLowDateTime);
			uint64 LastUserTime = uint64(mp_LastUser.dwHighDateTime) << 32 | uint64(mp_LastUser.dwLowDateTime);
			uint64 LastKernelTime = (uint64(mp_LastKernel.dwHighDateTime) << 32 | uint64(mp_LastKernel.dwLowDateTime)) - LastIdleTime;

			uint64 DiffIdleTime = IdleTime - LastIdleTime;
			uint64 DiffUserTime = UserTime - LastUserTime;
			uint64 DiffKernelTime = KernelTime - LastKernelTime;

			uint64 TotalTime = DiffIdleTime + DiffUserTime + DiffKernelTime;

			if (TotalTime < 100)
			{
				_bChanged = false;
				return mp_LastRet; // Guarantee 1 % resolution
			}

			_bChanged = true;

			NMib::NSystem::CSystemCPUUsage Ret;
			Ret.m_Idle = fp64(DiffIdleTime) / fp64(TotalTime);
			Ret.m_User = fp64(DiffUserTime) / fp64(TotalTime);
			Ret.m_Kernel = fp64(DiffKernelTime) / fp64(TotalTime);

			mp_LastIdle = LastIdle;
			mp_LastUser = LastUser;
			mp_LastKernel = LastKernel;

			mp_LastRet = Ret;
			return Ret;
		}

	private:
		// NOLINTBEGIN(malterlib-conceptual-prefix): FILETIME is a struct, which the check takes for a function type by its name
		FILETIME mp_LastIdle;
		FILETIME mp_LastUser;
		FILETIME mp_LastKernel;
		// NOLINTEND(malterlib-conceptual-prefix)

		NMib::NSystem::CSystemCPUUsage mp_LastRet;
	};
}

namespace NMib
{
	namespace NSys
	{
		void *fg_System_CPUUsageMonitor_Open()
		{
			TCUniquePointer<NMSVCRuntime::CCPUUsageMonitor> pMonitor = fg_Construct();

			return pMonitor.f_Detach();
		}

		void fg_System_CPUUsageMonitor_Close(void *_pHandle)
		{
			TCUniquePointer<NMSVCRuntime::CCPUUsageMonitor> pMonitor = fg_Explicit((NMSVCRuntime::CCPUUsageMonitor *)_pHandle);
		}

		NSystem::CSystemCPUUsage fg_System_CPUUsageMonitor_GetUsage(void *_pHandle, bool &_bChanged)
		{
			return ((NMSVCRuntime::CCPUUsageMonitor *)_pHandle)->f_GetUsage(_bChanged);
		}
	}
}
