// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#ifdef DPlatformFamily_Windows

namespace NMib::NPlatform
{
	template <>
	inline uint32 CWin32_Registry::f_Read<uint32>(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue)
	{
		DWORD Temp = 0;

		fp_ReadRegKey(_SubKey, _KeyValue, &Temp, nullptr, nullptr, nullptr);

		return Temp;
	}

	template <>
	inline NStr::CStr CWin32_Registry::f_Read<NStr::CStr>(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue)
	{
		NStr::CStr Temp;

		fp_ReadRegKey(_SubKey, _KeyValue, nullptr, &Temp, nullptr, nullptr);

		return Temp;
	}

	template <>
	inline NContainer::CByteVector CWin32_Registry::f_Read<NContainer::CByteVector>(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue)
	{
		NContainer::CByteVector Temp;

		fp_ReadRegKey(_SubKey, _KeyValue, nullptr, nullptr, &Temp, nullptr);

		return Temp;
	}

	template <>
	inline NContainer::TCVector<NStr::CStr> CWin32_Registry::f_Read<NContainer::TCVector<NStr::CStr>>(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue)
	{
		NContainer::TCVector<NStr::CStr> Temp;

		fp_ReadRegKey(_SubKey, _KeyValue, nullptr, nullptr, nullptr, &Temp);

		return Temp;
	}
}

#endif
