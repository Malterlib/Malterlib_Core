// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Core_Platform_OSX_ObjC.h"

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
		NSString* fg_MaxOSX_GetString(NStr::CStr const& _Str)
		{
			return [[NSString alloc] initWithUTF8String:_Str.f_GetStr()];
		}

		NStr::CStr fg_MaxOSX_GetString(NSString *_pStr)
		{
			if (!_pStr)
				return NStr::CStr();

			NSData *pData = [_pStr dataUsingEncoding: NSUTF8StringEncoding];
			NStr::CStr Return{(ch8 const *)pData.bytes, pData.length};
			
			return Return;
		}
	} 
}
