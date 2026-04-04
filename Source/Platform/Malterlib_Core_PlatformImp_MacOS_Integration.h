// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib
{
	namespace NRuntime
	{
		void fg_MacOS_NativeHideMainWindow(void* _pNativeWindowHandle);
		void fg_MacOS_SetBadgeLabel(NStr::CStr const& _Label);
		void fg_MacOS_ClearBadgeLabel();
		bool fg_MacOS_PlaySound(uint8 const* _pWaveform, umint _nBytes);

	} // Namespace NRuntime

} // Namespace NMib
