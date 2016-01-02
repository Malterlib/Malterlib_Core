// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#import <Breakpad/Breakpad.h>
#include <Mib/Cryptography/UUID>

namespace NMib
{
	
	namespace NSys
	{
		
		BreakpadRef g_Breakpad = nullptr;
		
		static BreakpadRef InitBreakpad(void)
		{
			if (!BreakpadVersion)
				return nullptr;
			
			if (BreakpadVersion() < 1)
				return nullptr;
			
			if (!BreakpadCreate)
				return nullptr;

			NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
			BreakpadRef breakpad = 0;
			NSMutableDictionary *plist = [NSMutableDictionary dictionaryWithDictionary:[[NSBundle mainBundle] infoDictionary]] ;
			if (plist)
			{
				
				NMib::NStr::CStr TempDir = NMib::NFile::CFile::fs_GetTemporaryDirectory() + "/CrashDumps/" + NMib::NDataProcessing::fg_GetSecureUuidString(NDataProcessing::EUniversallyUniqueIdentifierFormat_AlphaNum);
				
				try
				{
					NMib::NFile::CFile::fs_CreateDirectory(TempDir);
					
					[plist setObject:[[NSString alloc] initWithUTF8String:TempDir.f_GetStr()] forKey:@"BreakpadMinidumpLocation"];
					
					/*for (id key in plist) {
					
						NSLog(@"key: %@, value: %@", key, [plist objectForKey:key]);
						
					}*/
					
					// Note: version 1.0.0.4 of the framework changed the type of the argument
					// from CFDictionaryRef to NSDictionary * on the next line:
					breakpad = BreakpadCreate(plist);

				}
				catch(NMib::NFile::CExceptionFile const&){}
				
			}
			[pool release];
			return breakpad;
		}
		
		void fg_InitBreakpad()
		{
			//DMibTraceSafe("Init breakpad called\n", 0);
			g_Breakpad = InitBreakpad();
		}
		void fg_DestroyBreakpad()
		{
			//DMibTraceSafe("Destroy breakpad called\n", 0);
			if (g_Breakpad && BreakpadRelease)
				BreakpadRelease(g_Breakpad);
		}

	}

} // Namespace NMib
