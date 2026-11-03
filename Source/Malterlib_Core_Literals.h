// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

constexpr inline uint64 operator ""_uint64 (uint64 _Test);
constexpr inline uint32 operator ""_uint32 (uint64 _Test);
constexpr inline uint16 operator ""_uint16 (uint64 _Test);
constexpr inline uint8 operator ""_uint8 (uint64 _Test);
constexpr inline int64 operator ""_int64 (uint64 _Test);
constexpr inline int32 operator ""_int32 (uint64 _Test);
constexpr inline int16 operator ""_int16 (uint64 _Test);
constexpr inline int8 operator ""_int8 (uint64 _Test);

#include "Malterlib_Core_Literals.hpp"
