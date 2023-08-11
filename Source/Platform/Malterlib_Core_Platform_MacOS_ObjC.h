// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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
	}
}
