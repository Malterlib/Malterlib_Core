// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

namespace NMib
{
	constexpr CExplicitHelper g_ExplicitInit{};
	CExplicitHelper const &g_Explicit = g_ExplicitInit;

	CVoidTag const g_Void;

#ifdef DMibFileLineOnDebugBreak
	void fg_OutputDebugBreakFileLine(ch8 const *_pFile, int _Line)
	{
		NMib::NSys::fg_ConsoleErrorOutput((NStr::CFStr1024::CFormat(DMibPFileLineFormat " Debug Break\n") << _pFile << _Line).f_GetStr().f_Span());
	}
#endif
}
