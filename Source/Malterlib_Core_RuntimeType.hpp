// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib
{
	inline auto CRunTimeObjectInfo::CCompare::operator () (ch8 const *_pLeft, ch8 const *_pRight)
	{
		return NStr::fg_StrCmp(_pLeft, _pRight) <=> 0;
	}

	template <typename t_CObject, typename t_CCastClass>
	TCRuntimeClassNamed<t_CObject, t_CCastClass>::TCRuntimeClassNamed(ch8 const *_pName, ch8 const *_pParent)
	{
		f_Construct(_pName, _pParent);
	}

	template <typename t_CObject, typename t_CCastClass>
	void *TCRuntimeClassNamed<t_CObject, t_CCastClass>::f_CreateObject() const
	{
		if constexpr (NTraits::cIsAbstract<t_CObject>)
			DMibError("Cannot construct an abstract class");
		else
			return fg_ConstructObject<t_CObject>(NMemory::CDefaultAllocator());
	}

	template <typename tf_CType>
	NStorage::TCUniquePointer<tf_CType> fg_CreateRuntimeType(ch8 const *_pObjectName)
	{
		return fg_Explicit(static_cast<tf_CType *>(fg_CreateRuntimeTypeRawPtr(_pObjectName)));
	}
}
