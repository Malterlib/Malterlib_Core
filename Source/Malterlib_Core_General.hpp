// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib
{
	template <typename tf_CType>
	ch8 const *fg_GetTypeName()
	{
#ifdef DCompiler_MSVC
		static ch8 s_ReturnName[sizeof(DMibPFunctionSignature)];
		static bint s_bInit = false;
		if (s_bInit)
			return s_ReturnName;
		ch8 const *pParseStart = DMibPFunctionSignature;
		ch8 const *pParse = pParseStart;
		while (*pParse && *pParse != '<')
			++pParse;
		if (*pParse == '<')
			++pParse;
		ch8 const *pStartType = pParse;
		mint nStart = 0;
		mint nStartParen = 0;

		while (*pParse)
		{
			if (nStartParen)
			{
				if (*pParse == '(')
				{
					++nStartParen;
				}
				else if (*pParse == ')')
				{
					--nStartParen;
				}
			}
			else
			{
				if (*pParse == '<')
				{
					++nStart;
				}
				else if (*pParse == '>')
				{
					if (nStart == 0)
						break;
					--nStart;
				}
			}
			++pParse;
		}
		NStr::fg_StrCopy(s_ReturnName, pStartType, (pParse - pStartType) + 1);
		NMib::NAtomic::fg_MemoryFence();
		s_bInit = true;
		NMib::NAtomic::fg_MemoryFence();
		return s_ReturnName;
#else
		static ch8 s_ReturnName[sizeof(DMibPFunctionSignature)];
		static bint s_bInit = false;
		if (s_bInit)
			return s_ReturnName;
		ch8 const *pParseStart = DMibPFunctionSignature;
		ch8 const *pParse = pParseStart;
		while (*pParse && *pParse != '=')
			++pParse;
		if (*pParse == '=')
			++pParse;
		if (*pParse == ' ')
			++pParse;
		ch8 const *pStartType = pParse;
		mint nStart = 1;

		while (*pParse)
		{
			if (*pParse == '[')
			{
				++nStart;
			}
			else if (*pParse == ']')
			{
				if (--nStart == 0)
					break;
			}
			++pParse;
		}
		NStr::fg_StrCopy(s_ReturnName, pStartType, (pParse - pStartType) + 1);
		NMib::NAtomic::fg_MemoryFence();
		s_bInit = true;
		NMib::NAtomic::fg_MemoryFence();
		return s_ReturnName;

#endif
	}
}

