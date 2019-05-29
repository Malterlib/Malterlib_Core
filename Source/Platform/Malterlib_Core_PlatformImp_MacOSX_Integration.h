// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NRuntime
	{
		void fg_MacOSX_NativeHideMainWindow(void* _pNativeWindowHandle);
		void fg_MacOSX_SetBadgeLabel(NStr::CStr const& _Label);
		void fg_MacOSX_ClearBadgeLabel();
		bool fg_MacOSX_PlaySound(uint8 const* _pWaveform, mint _nBytes);
		
	} // Namespace NRuntime

} // Namespace NMib
