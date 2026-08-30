// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "../Malterlib_Core_IoSubSystem.h"

// This platform has no io knobs of its own yet; the subsystem carries the shared ones
struct CIoSubSystem_MacOS : NMib::NSys::CIoSubSystem
{
};
