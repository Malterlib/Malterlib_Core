// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include "Malterlib_Core_PlatformInterfaceTemp.h"

namespace NMib::NSystem
{
	CSystemCPUUsage::CSystemCPUUsage()
		: m_User(0.0f)
		, m_Kernel(0.0f)
		, m_Idle(0.0f)
	{
	}

	CSystemCPUUsage::CSystemCPUUsage(CSystemCPUUsage const &_Other)
		: m_User(_Other.m_User)
		, m_Kernel(_Other.m_Kernel)
		, m_Idle(_Other.m_Idle)
	{
	}

	CSystemCPUUsage::CSystemCPUUsage(fp32 _Init)
		: m_User(_Init)
		, m_Kernel(_Init)
		, m_Idle(_Init)
	{
	}

	fp32 CSystemCPUUsage::f_GetUsage() const
	{
		return m_User + m_Kernel;
	}
}
