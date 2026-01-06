// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

namespace NMSVCRuntime
{

	class CCPUUsageMonitor : public CCPUUsageMonitorLink
	{
		FILETIME m_LastIdle;
		FILETIME m_LastUser;
		FILETIME m_LastKernel;

		NMib::NSystem::CSystemCPUUsage m_LastRet;
	public:
		CCPUUsageMonitor()
		{
			GetSystemTimes(&m_LastIdle, &m_LastKernel, &m_LastUser);
			m_LastRet.m_User = 0.0f;
			m_LastRet.m_Kernel = 0.0f;
			m_LastRet.m_Idle = 0.0f;
		}
		virtual ~CCPUUsageMonitor()
		{
		}

	public:
		NMib::NSystem::CSystemCPUUsage f_GetUsage(bool &_bChanged)
		{
			FILETIME LastIdle;
			FILETIME LastUser;
			FILETIME LastKernel;
			GetSystemTimes(&LastIdle, &LastKernel, &LastUser);

			uint64 IdleTime = uint64(LastIdle.dwHighDateTime) << 32 | uint64(LastIdle.dwLowDateTime);
			uint64 UserTime = uint64(LastUser.dwHighDateTime) << 32 | uint64(LastUser.dwLowDateTime);
			uint64 KernelTime = (uint64(LastKernel.dwHighDateTime) << 32 | uint64(LastKernel.dwLowDateTime)) - IdleTime;

			uint64 LastIdleTime = uint64(m_LastIdle.dwHighDateTime) << 32 | uint64(m_LastIdle.dwLowDateTime);
			uint64 LastUserTime = uint64(m_LastUser.dwHighDateTime) << 32 | uint64(m_LastUser.dwLowDateTime);
			uint64 LastKernelTime = (uint64(m_LastKernel.dwHighDateTime) << 32 | uint64(m_LastKernel.dwLowDateTime)) - LastIdleTime;

			uint64 DiffIdleTime = IdleTime - LastIdleTime;
			uint64 DiffUserTime = UserTime - LastUserTime;
			uint64 DiffKernelTime = KernelTime - LastKernelTime;

			uint64 TotalTime = DiffIdleTime + DiffUserTime + DiffKernelTime;

			if (TotalTime < 100)
			{
				_bChanged = false;
				return m_LastRet; // Guarantee 1 % resolution
			}

			_bChanged = true;

			NMib::NSystem::CSystemCPUUsage Ret;
			Ret.m_Idle = fp64(DiffIdleTime) / fp64(TotalTime);
			Ret.m_User = fp64(DiffUserTime) / fp64(TotalTime);
			Ret.m_Kernel = fp64(DiffKernelTime) / fp64(TotalTime);

			m_LastIdle = LastIdle;
			m_LastUser = LastUser;
			m_LastKernel = LastKernel;

			m_LastRet = Ret;
			return Ret;
		}
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