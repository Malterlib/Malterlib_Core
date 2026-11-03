// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

namespace NMib
{
	CLineNumber::CLineNumber(aint _nLine)
		: m_nLine(_nLine)
	{
	}

	CLineNumber::operator aint () const
	{
		return m_nLine;
	}

	constexpr CExplicitHelper gc_ExplicitInit{};
	CExplicitHelper const &g_Explicit = gc_ExplicitInit;

	CVoidTag const g_Void;

#ifdef DMibFileLineOnDebugBreak
	void fg_OutputDebugBreakFileLine(ch8 const *_pFile, int _Line)
	{
		NMib::NSys::fg_ConsoleErrorOutput((NStr::CFStr1024::CFormat(DMibPFileLineFormat " Debug Break\n") << _pFile << _Line).f_GetStr().f_Span());
	}
#endif
}
