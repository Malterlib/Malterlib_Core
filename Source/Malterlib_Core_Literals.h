// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

constexpr inline uint64 operator ""_uint64(uint64 _Test)
{
	return _Test;
}

constexpr inline uint32 operator ""_uint32(uint64 _Test)
{
	if (_Test > uint64(NMib::TCLimitsInt<uint32>::mc_Max))
		throw "Literal out of range";

	return _Test;
}

constexpr inline uint16 operator ""_uint16(uint64 _Test)
{
	if (_Test > uint64(NMib::TCLimitsInt<uint16>::mc_Max))
		throw "Literal out of range";

	return _Test;
}

constexpr inline uint8 operator ""_uint8(uint64 _Test)
{
	if (_Test > uint64(NMib::TCLimitsInt<uint8>::mc_Max))
		throw "Literal out of range";

	return _Test;
}

constexpr inline int64 operator ""_int64(uint64 _Test)
{
	if (_Test == uint64(NMib::TCLimitsInt<int64>::mc_Max) + 1)
		return int64(_Test);

	if (_Test > uint64(NMib::TCLimitsInt<int64>::mc_Max))
		throw "Literal out of range";

	return _Test;
}

constexpr inline int32 operator ""_int32(uint64 _Test)
{
	if (_Test == uint64(NMib::TCLimitsInt<int32>::mc_Max) + 1)
		return int32(_Test);

	if (_Test > uint64(NMib::TCLimitsInt<int32>::mc_Max))
		throw "Literal out of range";

	return _Test;
}

constexpr inline int16 operator ""_int16(uint64 _Test)
{
	if (_Test == uint64(NMib::TCLimitsInt<int16>::mc_Max) + 1)
		return int16(_Test);

	if (_Test > uint64(NMib::TCLimitsInt<int16>::mc_Max))
		throw "Literal out of range";

	return _Test;
}

constexpr inline int8 operator ""_int8(uint64 _Test)
{
	if (_Test == uint64(NMib::TCLimitsInt<int8>::mc_Max) + 1)
		return -int8(_Test);

	if (_Test > uint64(NMib::TCLimitsInt<int8>::mc_Max))
		throw "Literal out of range";

	return _Test;
}
