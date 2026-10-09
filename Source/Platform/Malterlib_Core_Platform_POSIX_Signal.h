// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

namespace NMib
{
	namespace NPlatform
	{
		auto fg_SetSignalHandler(int _Signal, void (*_fHandler)(int)) -> void (*)(int);
	}
}
