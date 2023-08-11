// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Core_Platform_MacOS_ObjC.h"

namespace NMib
{

#ifndef DMibObjC_Arc
	CAutoReleasePool::CAutoReleasePool()
		: m_pPool(nullptr)
	{
		m_pPool = [[NSAutoreleasePool alloc] init];
	}
	
	CAutoReleasePool::~CAutoReleasePool()
	{
		[m_pPool drain];
	}
#else
	CAutoReleasePool::CAutoReleasePool()
	{
	}
	CAutoReleasePool::~CAutoReleasePool()
	{
	}
#endif

	namespace NPlatform
	{
		NSString* fg_MacOS_GetString(NStr::CStr const& _Str)
		{
			return [[NSString alloc] initWithUTF8String:_Str.f_GetStr()];
		}

		NStr::CStr fg_MacOS_GetString(NSString *_pStr)
		{
			if (!_pStr)
				return NStr::CStr();

			NStr::CStr Return(_pStr.UTF8String);

			return Return;
		}
		
		NStr::CStr fg_MacOS_GetString(CFStringRef _pKey)
		{
			mint MaxNeededSize = mint(CFStringGetLength(_pKey)) * 4u + 1u;

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
			mint MaxNeededSize = mint(CFStringGetLength(_pKey)) * 4u + 1u;

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
