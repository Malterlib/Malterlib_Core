// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#import <Cocoa/Cocoa.h>
#include <CoreFoundation/CoreFoundation.h>

namespace NMib
{
#ifndef DMibObjC_Arc
	struct CAutoReleasePool
	{
		NSAutoreleasePool* m_pPool;
		CAutoReleasePool();
		~CAutoReleasePool();
	};
#else
	struct CAutoReleasePool
	{
		CAutoReleasePool();
		~CAutoReleasePool();
	};
#endif

	namespace NPlatform
	{
		NSString* fg_MacOS_GetString(NStr::CStr const& _Str);
		NStr::CStr fg_MacOS_GetString(NSString *_pStr);
		NStr::CStr fg_MacOS_GetString(CFStringRef _pKey);
		NStr::CStrNonTracked fg_MacOS_GetStringUntracked(NSString *_pStr);
		NStr::CStrNonTracked fg_MacOS_GetStringUntracked(CFStringRef _pKey);
	}
}
