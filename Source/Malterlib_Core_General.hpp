// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib
{
	static constexpr uint32 fg_JenkinsHash(const char * const _pString)
	{
		uint32 Hash = 0;
		for (char const *pStr = _pString; *pStr; ++pStr)
		{
			Hash = (uint64(Hash) + *pStr) & uint64(0xffffffff);
			Hash = (uint64(Hash) + (Hash << 10)) & uint64(0xffffffff);
			Hash ^= (Hash >> 6);
		}
		Hash = (uint64(Hash) + (Hash << 3)) & uint64(0xffffffff);
		Hash ^= (Hash >> 11);
		Hash = (uint64(Hash) + (Hash << 15)) & uint64(0xffffffff);
		return Hash;
	}

	static constexpr uint32 fg_JenkinsHash(const char * const _pString, mint _Len, char _ExtraChar)
	{
		uint32 Hash = 0;
		for (char const *pStr = _pString; pStr < _pString + _Len; ++pStr)
		{
			Hash = (uint64(Hash) + *pStr) & uint64(0xffffffff);
			Hash = (uint64(Hash) + (Hash << 10)) & uint64(0xffffffff);
			Hash ^= (Hash >> 6);
		}
		if (_ExtraChar)
		{
			Hash = (uint64(Hash) + _ExtraChar) & uint64(0xffffffff);
			Hash = (uint64(Hash) + (Hash << 10)) & uint64(0xffffffff);
			Hash ^= (Hash >> 6);
		}
		Hash = (uint64(Hash) + (Hash << 3)) & uint64(0xffffffff);
		Hash ^= (Hash >> 11);
		Hash = (uint64(Hash) + (Hash << 15)) & uint64(0xffffffff);
		return Hash;
	}
	
	static constexpr void fg_ParseUntilCallingConvention(char const *&_pParse)
	{
		mint nStart = 0;

		constexpr char const *c_CallingConventions[] = 
			{
				"__thiscall "
				, "__cdecl "
				, "__clrcall "
				, "__stdcall "
				, "__fastcall "
				, "__thiscall "
				, "__vectorcall "
			}
		;

		auto pStart = _pParse;
		while (*_pParse)
		{
			if (*_pParse == '<')
				++nStart;
			else if (*_pParse == '>')
				--nStart;
			else if (*_pParse == '`')
			{
				while (*_pParse && *_pParse != '\'')
					++_pParse;
				if (*_pParse == '\'' && _pParse[1] == '{')
				{
					_pParse += 2;
					while (*_pParse && *_pParse != '\'')
						++_pParse;
					if (*_pParse == '\'')
					{
						++_pParse;
						continue;
					}
				}
			}
			else if (nStart == 0)
			{
				if (NStr::fg_CharIsAlphabetical(*_pParse) || NStr::fg_CharIsNumber(*_pParse) || *_pParse == ':' || *_pParse == '_')
					;
				else
				{
					if (*_pParse == ' ')
					{
						++_pParse;
						bool bFoundCallingConvention = false;
						for (auto &pCallingConvention : c_CallingConventions)
						{
							if (NStr::fg_StrStartsWith(pStart, pCallingConvention))
							{
								bFoundCallingConvention = true;
								break;
							}
						}
						if (bFoundCallingConvention)
							break;
						pStart = _pParse;
					}
				}
			}
			++_pParse;
		}
	}

	static constexpr void fg_ParseTypeIdentifierConstexpr(char const *&_pParse)
	{
		mint nStart = 0;

		while (*_pParse)
		{
			if (*_pParse == '<')
				++nStart;
			else if (*_pParse == '>')
				--nStart;
			else if (*_pParse == '`')
			{
				while (*_pParse && *_pParse != '\'')
					++_pParse;
				if (*_pParse == '\'' && _pParse[1] == '{')
				{
					_pParse += 2;
					while (*_pParse && *_pParse != '\'')
						++_pParse;
					if (*_pParse == '\'')
					{
						++_pParse;
						continue;
					}
				}
			}
			else if (nStart == 0)
			{
				if (NStr::fg_CharIsAlphabetical(*_pParse) || NStr::fg_CharIsNumber(*_pParse) || *_pParse == ':' || *_pParse == '_')
					;
				else
					break;
			}
			++_pParse;
		}
	}

	template <typename tf_CType>
	static constexpr CConstExprSubStr fg_GetTypeNameConstExpr()
	{
#if defined(DCompiler_MSVC)
		char const *pParseStart = DMibPFunctionSignature;
		char const *pParse = pParseStart;
		while (*pParse && *pParse != '<')
			++pParse;
		if (*pParse == '<')
			++pParse;
		if (NStr::fg_StrStartsWith(pParse, "class "))
			pParse += 6;
		else if (NStr::fg_StrStartsWith(pParse, "struct "))
			pParse += 7;
		char const *pStartType = pParse;
		mint nStart = 0;
		mint nStartParen = 0;

		while (*pParse)
		{
			if (*pParse == '(')
			{
				++nStartParen;
			}
			else if (*pParse == ')')
			{
				--nStartParen;
			}
			else if (!nStartParen)
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

		if (NStr::fg_StrStartsWith(pStartType, "`anonymous-namespace'::"))
			throw "Not portable";

		return CConstExprSubStr(pStartType, (pParse - pStartType));
#else
		char const *pParseStart = DMibPFunctionSignature;
		char const *pParse = pParseStart;
		while (*pParse && *pParse != '=')
			++pParse;
		if (*pParse == '=')
			++pParse;
		if (*pParse == ' ')
			++pParse;
		char const *pStartType = pParse;
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

		if (NStr::fg_StrStartsWith(pStartType, "(anonymous namespace)::"))
			throw "Not portable";

		return CConstExprSubStr(pStartType, (pParse - pStartType));
#endif
	}

	template <typename tf_CType>
	ch8 const *fg_GetTypeName()
	{
#ifdef DCompiler_MSVC
		static ch8 s_ReturnName[sizeof(DMibPFunctionSignature)];
		static bool s_bInit = false;
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
		static bool s_bInit = false;
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

	template <auto tf_pMemberPointer>
	static constexpr CConstExprSubStr fg_GetMemberPointerNameConstExpr()
	{
#if defined(DCompiler_MSVC)
		char const *pParse = DMibPFunctionSignature;
		while (*pParse && *pParse != '<')
			++pParse;
		if (*pParse == '<')
			++pParse;

		// Return type
 		fg_ParseUntilCallingConvention(pParse);

		char const *pStartType = pParse;

		// Function name
		fg_ParseTypeIdentifierConstexpr(pParse);

		return CConstExprSubStr(pStartType, (pParse - pStartType));
#else
		char const *pParseStart = DMibPFunctionSignature;
		char const *pParse = pParseStart;
		while (*pParse && *pParse != '=')
			++pParse;
		if (*pParse == '=')
			++pParse;
		if (*pParse == ' ')
			++pParse;
		char const *pStartType = pParse;
		mint nStart = 1;

		while (*pParse)
		{
			if (*pParse == '[')
				++nStart;
			else if (*pParse == ']')
			{
				if (--nStart == 0)
					break;
			}
			++pParse;
		}
		if (*pStartType == '&')
			++pStartType;
		return CConstExprSubStr(pStartType, (pParse - pStartType));
#endif
	}

#if DMibSupportMemberNameFromMemberPointer

	template <auto tf_pMemberFunction>
	static constexpr uint32 fg_GetMemberFunctionHash()
	{
		using CMemberFunction = decltype(tf_pMemberFunction);
		auto FunctionName = fg_GetMemberPointerNameConstExpr<tf_pMemberFunction>();
		char const *pStartName = nullptr;
		char const *pEnd = FunctionName.m_pString + FunctionName.m_Len - 1;

		mint nOpen = 0;
		for (char const *pParse = pEnd; pParse > FunctionName.m_pString; --pParse)
		{
			if (*pParse == '>' || *pParse == ')' || *pParse == ']')
				++nOpen;
			else if (*pParse == '<' || *pParse == '(' || *pParse == '[')
				--nOpen;

			if (nOpen == 0 && *pParse == ':')
			{
				pStartName = pParse + 1;
				break;
			}
		}

		if (!pStartName)
			pStartName = FunctionName.m_pString;

		auto ClassTypeName = fg_GetTypeNameConstExpr<typename NTraits::TCMemberFunctionPointerTraits<CMemberFunction>::CClass>();
		return fg_JenkinsHash(pStartName, FunctionName.m_Len - (pStartName - FunctionName.m_pString), 0) ^ fg_JenkinsHash(ClassTypeName.m_pString, ClassTypeName.m_Len, ']');
	}

#else

	static constexpr uint32 fg_GetMemberFunctionNameHash(const char * const _pFunctionName)
	{
		char const *pStartName = nullptr;
		char const *pEnd = _pFunctionName + NStr::fg_StrLen(_pFunctionName) - 1;

		mint nOpen = 0;
		for (char const *pParse = pEnd; pParse > _pFunctionName; --pParse)
		{
			if (*pParse == '>' || *pParse == ')' || *pParse == ']')
				++nOpen;
			else if (*pParse == '<' || *pParse == '(' || *pParse == '[')
				--nOpen;

			if (nOpen == 0 && *pParse == ':')
			{
				pStartName = pParse + 1;
				break;
			}
		}

		return fg_JenkinsHash(pStartName);
	}

	template <auto tf_pMemberFunction>
	static constexpr uint32 fg_GetMemberFunctionHash(uint32 _NameHash)
	{
		using CMemberFunction = decltype(tf_pMemberFunction);
		auto ClassTypeName = fg_GetTypeNameConstExpr<typename NTraits::TCMemberFunctionPointerTraits<CMemberFunction>::CClass>();
		return _NameHash ^ fg_JenkinsHash(ClassTypeName.m_pString, ClassTypeName.m_Len, ']');
	}

	template <auto tf_pMemberFunction, uint32 t_NameHash>
	static constexpr uint32 fg_GetMemberFunctionHash()
	{
		return fg_GetMemberFunctionHash<tf_pMemberFunction>(t_NameHash);
	}

#endif

	template <typename tf_CClass>
	static constexpr uint32 fg_GetMemberFunctionHash(const char * const _pFunctionName)
	{
		auto ClassTypeName = fg_GetTypeNameConstExpr<tf_CClass>();
		return fg_JenkinsHash(_pFunctionName) ^ fg_JenkinsHash(ClassTypeName.m_pString, ClassTypeName.m_Len, ']');
	}

	template <typename tf_CType>
	static constexpr uint32 fg_GetTypeHash()
	{
		auto ClassTypeName = fg_GetTypeNameConstExpr<tf_CType>();
		return fg_JenkinsHash(ClassTypeName.m_pString, ClassTypeName.m_Len, ']');
	}
}

