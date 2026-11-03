// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#import <Breakpad/Breakpad.h>
#include <Mib/Cryptography/UUID>

namespace NMib
{
	namespace NSys
	{
		BreakpadRef g_pBreakpad = nullptr;

		static BreakpadRef fsg_InitBreakpad(void)
		{
			if (!BreakpadVersion)
				return nullptr;

			if (BreakpadVersion() < 1)
				return nullptr;

			if (!BreakpadCreate)
				return nullptr;

			NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];
			BreakpadRef pBreakpad = 0;
			NSMutableDictionary *pPropertyList = [NSMutableDictionary dictionaryWithDictionary: [[NSBundle mainBundle] infoDictionary]];
			if (pPropertyList)
			{
				NMib::NStr::CStr TempDir
					= NMib::NFile::CFile::fs_GetTemporaryDirectory()
					+ "/CrashDumps/"
					+ NMib::NCryptography::fg_GetSecureUuidString(NCryptography::EUniversallyUniqueIdentifierFormat_AlphaNum)
				;

				try
				{
					NMib::NFile::CFile::fs_CreateDirectory(TempDir);

					[pPropertyList setObject: [[NSString alloc] initWithUTF8String: TempDir.f_GetStr()] forKey: @"BreakpadMinidumpLocation"];

					/*for (id key in plist) {

						NSLog(@"key: %@, value: %@", key, [plist objectForKey:key]);

					}*/

					// Note: version 1.0.0.4 of the framework changed the type of the argument
					// from CFDictionaryRef to NSDictionary * on the next line:
					pBreakpad = BreakpadCreate(pPropertyList);
				}
				catch (NMib::NFile::CExceptionFile const &)
				{
				}
			}
			[pPool release];
			return pBreakpad;
		}

		void fg_InitBreakpad()
		{
			//DMibTraceSafe("Init breakpad called\n", 0);
			g_pBreakpad = fsg_InitBreakpad();
		}

		void fg_DestroyBreakpad()
		{
			//DMibTraceSafe("Destroy breakpad called\n", 0);
			if (g_pBreakpad && BreakpadRelease)
				BreakpadRelease(g_pBreakpad);
		}
	}
} // Namespace NMib
