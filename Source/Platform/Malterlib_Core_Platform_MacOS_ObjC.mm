// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

#include "Malterlib_Core_Platform_MacOS_ObjC.h"

// The runtime's pool entry points are what @autoreleasepool compiles to; they are not declared
// by the SDK headers, and NSAutoreleasePool cannot be named under ARC
extern "C" void *objc_autoreleasePoolPush(void);
extern "C" void objc_autoreleasePoolPop(void *_pPool);

namespace NMib
{
	CAutoReleasePool::CAutoReleasePool()
		: mp_pPool(objc_autoreleasePoolPush())
	{
	}

	CAutoReleasePool::~CAutoReleasePool()
	{
		objc_autoreleasePoolPop(mp_pPool);
	}

	namespace NPlatform
	{
		NSString* fg_MacOS_GetString(NStr::CStr const& _Str)
		{
			return [[NSString alloc] initWithUTF8String:_Str.f_GetStr()];
		}

		NStr::CStr fg_MacOS_GetString(NSString *_pStr)
		{
			if (!_pStr)
				return {};

			auto pString = _pStr.UTF8String;
			if (!pString)
				return {};

			NStr::CStr Return(pString);

			return Return;
		}

		NStr::CStrNonTracked fg_MacOS_GetStringUntracked(NSString *_pStr)
		{
			if (!_pStr)
				return {};

			auto pString = _pStr.UTF8String;
			if (!pString)
				return {};

			NStr::CStrNonTracked Return(pString);

			return Return;
		}

		NStr::CStr fg_MacOS_GetString(CFStringRef _pKey)
		{
			umint MaxNeededSize = umint(CFStringGetLength(_pKey)) * 4u + 1u;

			auto pUTF8 = CFStringGetCStringPtr(_pKey, kCFStringEncodingUTF8);
			if (pUTF8)
				return NStr::CStr(pUTF8);

			NStr::CStr Return;
			if (!CFStringGetCString(_pKey, Return.f_GetStr(MaxNeededSize), MaxNeededSize, kCFStringEncodingUTF8))
				DMibError("Failed to convert NSString to CStr");

			Return.f_TrimSize();

			return Return;
		}

		NStr::CStrNonTracked fg_MacOS_GetStringUntracked(CFStringRef _pKey)
		{
			umint MaxNeededSize = umint(CFStringGetLength(_pKey)) * 4u + 1u;

			auto pUTF8 = CFStringGetCStringPtr(_pKey, kCFStringEncodingUTF8);
			if (pUTF8)
				return NStr::CStrNonTracked(pUTF8);

			NStr::CStrNonTracked Return;
			if (!CFStringGetCString(_pKey, Return.f_GetStr(MaxNeededSize), MaxNeededSize, kCFStringEncodingUTF8))
				DMibError("Failed to convert NSString to CStr");

			Return.f_TrimSize();

			return Return;
		}
	}
}
