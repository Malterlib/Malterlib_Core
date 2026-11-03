// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

template <typename tf_CAddrType>
tf_CAddrType const &CRuntimeNetAddress::f_GetAsType(NMib::NNetwork::ENetAddressType _ExpectedType) const
{
	DMibSafeCheck(mp_Type == _ExpectedType, "Address is not of the expected type.");
	return *(tf_CAddrType const *)mp_Data.f_GetArray();
}
