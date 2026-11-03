// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_PlatformImp_Net.h"

CAddressResolver::CAddressResolver() = default;

namespace
{
	constexpr aint gc_StagedUnixSocketSuffixLength = 8;
	constexpr ch8 const gc_StagedUnixSocketHashCharacters[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
}

#include "Malterlib_Core_PlatformImp_Net.imp.hpp"

#include "Malterlib_Core_PlatformImp_Net_Resolver.hpp"
