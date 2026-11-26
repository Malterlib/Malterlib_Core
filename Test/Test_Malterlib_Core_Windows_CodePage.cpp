// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Test/Exception>

#ifdef DPlatformFamily_Windows
#include <Mib/Core/PlatformSpecific/WindowsString>
#include <Windows.h>

namespace
{
	using namespace NMib;
	using namespace NMib::NStr;

	struct CCodePage_Tests : NTest::CTest
	{
		void f_DoTests();
	};

	void CCodePage_Tests::f_DoTests()
	{
		DMibTestSuite("UTF8")
		{
			// UTF-8 as the code page, as also the active one of a process with a UTF-8 locale is: Windows rejects a default character
			// for it.
			CStr Text = "café 漢";
			{
				DMibTestPath("Tracked");
				CAnsiStr Encoded;
				NStr::NPlatform::fg_SystemEncodeCodePageStr(Text, Encoded, CP_UTF8, '?');
				DMibExpect(Encoded, ==, CAnsiStr(Text.f_GetStr(), Text.f_GetLen()));
				CStr Decoded;
				NStr::NPlatform::fg_SystemDecodeCodePageStr(Encoded, Decoded, CP_UTF8);
				DMibExpect(Decoded, ==, Text);
			}
			{
				DMibTestPath("NonTracked");
				CAnsiStrNonTracked Encoded;
				NStr::NPlatform::fg_SystemEncodeCodePageStr(CStrNonTracked(Text), Encoded, CP_UTF8, '?');
				DMibExpect(CAnsiStr(Encoded.f_GetStr(), Encoded.f_GetLen()), ==, CAnsiStr(Text.f_GetStr(), Text.f_GetLen()));
			}
			if (GetACP() == CP_UTF8)
			{
				DMibTestPath("ActiveCodePage");
				CAnsiStr Encoded;
				NStr::NPlatform::fg_SystemEncodeAnsiStr(Text, Encoded, '?');
				DMibExpect(Encoded, ==, CAnsiStr(Text.f_GetStr(), Text.f_GetLen()));
			}
			{
				// CP_THREAD_ACP is the code page of the thread's locale, here English (United States), whose code page is Windows-1252: a
				// character outside it takes the caller's default character, also in a process whose code page is UTF-8.
				DMibTestPath("ThreadCodePage");
				LCID PreviousLocale = GetThreadLocale();
				auto RestoreLocale = g_OnScopeExit / [&]
					{
						SetThreadLocale(PreviousLocale);
					}
				;
				DMibAssertTrue(SetThreadLocale(MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT)));

				CAnsiStr Encoded;
				NStr::NPlatform::fg_SystemEncodeCodePageStr(CStr("a\u6f22"), Encoded, CP_THREAD_ACP, '#');
				DMibExpect(Encoded, ==, CAnsiStr("a#"));
			}
		};

		// Windows compares the names of its environment and its registry regardless of case, also of letters outside ASCII and Latin-1.
		DMibTestSuite("UpperCaseName")
		{
			DMibExpect(NStr::NPlatform::fg_WindowsUpperCaseName("p4_d\u00e9\u0434\u03c3_charset"), ==, "P4_D\u00c9\u0414\u03a3_CHARSET");
			DMibExpect(NStr::NPlatform::fg_WindowsUpperCaseName(""), ==, "");
		};
	}

	DMibTestRegister(CCodePage_Tests, Malterlib::Core);
}
#endif
