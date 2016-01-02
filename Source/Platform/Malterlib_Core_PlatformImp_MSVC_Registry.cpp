// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
extern NMib::NStr::CFStr256 fg_Win32_GetLastErrorStr(uint32 _Error);

#include "Malterlib_Core_PlatformImp_MSVC_Registry.h"

using namespace NMib;
using namespace NStr;
using namespace NContainer;

CWStr fg_StrToWindows(const CStr &_Str);

namespace NMib
{

	namespace NRuntimeMSVC
	{


		/*************************************************************************************************\
		|¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
		| CWin32_Registry
		|__________________________________________________________________________________________________
		\*************************************************************************************************/

		CWin32_Registry::CWin32_Registry(ERegRoot _Root, const CStr &_RootPath)
		{
			mp_Root = _RootPath;
			mp_RegRoot = _Root;
			mp_hRootKey = HKEY_LOCAL_MACHINE;
			switch(_Root)
			{
			case ERegRoot_LocalMachine:
			case ERegRoot_Win64_LocalMachine:
				mp_hRootKey = HKEY_LOCAL_MACHINE;
				break;
			case ERegRoot_CurrentUser:
			case ERegRoot_Win64_CurrentUser:
				mp_hRootKey = HKEY_CURRENT_USER;
				break;
			case ERegRoot_Classes:
			case ERegRoot_Win64_Classes:
				mp_hRootKey = HKEY_CLASSES_ROOT;
				break;
			}
		}

		CWin32_Registry::CWin32_Registry(CWin32_Registry const& _Root, const CStr &_RootPath)
		{
			if (_RootPath.f_IsEmpty())
				mp_Root = _Root.mp_Root;
			else
				mp_Root = _Root.mp_Root + "\\" + _RootPath;

			mp_hRootKey = _Root.mp_hRootKey;
			mp_RegRoot = _Root.mp_RegRoot;
		}


		uint32 CWin32_Registry::fp_GetAccess(uint32 _Base)
		{
			if 
			(
				mp_RegRoot == ERegRoot_Win64_LocalMachine
				|| mp_RegRoot == ERegRoot_Win64_CurrentUser
				|| mp_RegRoot == ERegRoot_Win64_Classes
			)
				_Base |= KEY_WOW64_64KEY;
			return _Base;
		}


		uint32 CWin32_Registry::f_Read_uint32(const CStr &_SubKey, const CStr &_KeyValue)
		{
			DWORD Temp = 0;
	
			fp_ReadRegKey(_SubKey, _KeyValue, &Temp, nullptr, nullptr, nullptr);
	
			return Temp;
		}

		CStr CWin32_Registry::f_Read_Str(const CStr &_SubKey, const CStr &_KeyValue)
		{
			CStr Temp;
	
			fp_ReadRegKey(_SubKey, _KeyValue, nullptr, &Temp, nullptr, nullptr);
	
			return Temp;
		}

		TCVector<CStr> CWin32_Registry::f_Read_StrMulti(const CStr &_SubKey, const CStr &_KeyValue)
		{
			TCVector<CStr> Temp;
	
			fp_ReadRegKey(_SubKey, _KeyValue, nullptr, nullptr, nullptr, &Temp);
	
			return Temp;
		}

		TCVector<uint8> CWin32_Registry::f_Read_Bin(const CStr &_SubKey, const CStr &_KeyValue)
		{
			TCVector<uint8> Temp;
	
			fp_ReadRegKey(_SubKey, _KeyValue, nullptr, nullptr, &Temp, nullptr);
	
			return Temp;
		}

	
		uint32 CWin32_Registry::f_Read_uint32(const CStr &_SubKey, const CStr &_KeyValue, uint32 _Default)
		{
			if (f_ValueExists(_SubKey, _KeyValue))
				return f_Read_uint32(_SubKey, _KeyValue);
			return _Default;
		}

		CStr CWin32_Registry::f_Read_Str(const CStr &_SubKey, const CStr &_KeyValue, const CStr &_Default)
		{
			if (f_ValueExists(_SubKey, _KeyValue))
				return f_Read_Str(_SubKey, _KeyValue);
			return _Default;
		}

		TCVector<CStr> CWin32_Registry::f_Read_StrMulti(const CStr &_SubKey, const CStr &_KeyValue, const TCVector<CStr> &_Default)
		{
			if (f_ValueExists(_SubKey, _KeyValue))
				return f_Read_StrMulti(_SubKey, _KeyValue);
			return _Default;
		}


		TCVector<uint8> CWin32_Registry::f_Read_Bin(const CStr &_SubKey, const CStr &_KeyValue, const TCVector<uint8> &_Default)
		{
			if (f_ValueExists(_SubKey, _KeyValue))
				return f_Read_Bin(_SubKey, _KeyValue);
			return _Default;
		}

		CStr CWin32_Registry::fp_GetPath(const CStr &_SubKey)
		{
			if (!_SubKey.f_IsEmpty() && mp_Root.f_GetLen())
			{
				return mp_Root + "\\" + _SubKey;
			}
			else if (!_SubKey.f_IsEmpty())
				return _SubKey;

			return mp_Root;
		}

		bint CWin32_Registry::f_KeyExists(const CStr &_SubKey)
		{
			HKEY PathKey = nullptr;
			CWStr SubKey = fg_StrToWindows(_SubKey);
	
			if (RegOpenKeyExW(mp_hRootKey,SubKey,0,fp_GetAccess(KEY_READ),&PathKey) != ERROR_SUCCESS)
				return false;
			auto Cleanup
				= fg_OnScopeExit
				(
					[&]
					{
						RegCloseKey(PathKey);			
					}
				)
			;
			return true;
		}

		bint CWin32_Registry::f_ValueExists(const CStr &_SubKey, const CStr &_KeyValue)
		{
			HKEY PathKey = nullptr;
			DWORD ValueType;
			CStr SubKey = fp_GetPath(_SubKey);
			CWStr KeyValue;
			const ch16 *pKeyValue;
			if (_KeyValue.f_IsEmpty())
				pKeyValue = nullptr;
			else
			{
				KeyValue = fg_StrToWindows(_KeyValue);
				pKeyValue = KeyValue;
			}
	
			if (RegOpenKeyExW(mp_hRootKey, fg_StrToWindows(SubKey),0,fp_GetAccess(KEY_READ),&PathKey) != ERROR_SUCCESS)
				return false;

			auto Cleanup
				= fg_OnScopeExit
				(
					[&]
					{
						RegCloseKey(PathKey);			
					}
				)
			;
	
			if (RegQueryValueExW(PathKey,pKeyValue,0,&ValueType,nullptr,nullptr) != ERROR_SUCCESS)
			{
				return false;
			}
	
			return true;
		}


		bint CWin32_Registry::f_IsBinary(const CStr &_SubKey, const CStr &_KeyValue)
		{
			HKEY PathKey = nullptr;
			DWORD ValueType;
			CStr SubKey = fp_GetPath(_SubKey);
			CWStr KeyValue;
			const ch16 *pKeyValue;
			if (_KeyValue.f_IsEmpty())
				pKeyValue = nullptr;
			else
			{
				KeyValue = fg_StrToWindows(_KeyValue);
				pKeyValue = KeyValue;
			}
	
			if (RegOpenKeyExW(mp_hRootKey, fg_StrToWindows(SubKey),0,fp_GetAccess(KEY_READ),&PathKey) != ERROR_SUCCESS)
				return false;

			auto Cleanup
				= fg_OnScopeExit
				(
					[&]
					{
						RegCloseKey(PathKey);			
					}
				)
			;
	
			if (RegQueryValueExW(PathKey,pKeyValue,0,&ValueType,nullptr,nullptr) != ERROR_SUCCESS)
			{
				return false;
			}
	
			return ValueType == REG_BINARY;
		}

		void CWin32_Registry::fp_ReadRegKey(const CStr &_SubKey, const CStr &_KeyValue, DWORD *_pDword, CStr *_pStr, TCVector<uint8> *_pMemory, TCVector<CStr> *_pMulti)
		{		
			DWORD ValueSize = 0;
			HKEY PathKey = nullptr;
			DWORD ValueType;
			CStr SubKey = fp_GetPath(_SubKey);
			CWStr KeyValue;
			const ch16 *pKeyValue;
			if (_KeyValue.f_IsEmpty())
				pKeyValue = nullptr;
			else
			{
				KeyValue = fg_StrToWindows(_KeyValue);
				pKeyValue = KeyValue;
			}

			HRESULT Error;
			if ((Error = RegOpenKeyExW(mp_hRootKey, fg_StrToWindows(SubKey),0,fp_GetAccess(KEY_READ),&PathKey)) != ERROR_SUCCESS)
				DMibError((CStr::CFormat("RegOpenKeyExW failed with: {}") << fg_Win32_GetLastErrorStr(Error)).f_GetStr());

			auto Cleanup
				= fg_OnScopeExit
				(
					[&]
					{
						RegCloseKey(PathKey);			
					}
				)
			;

			if ((Error = RegQueryValueExW(PathKey,pKeyValue,0,&ValueType,nullptr,nullptr)) != ERROR_SUCCESS)
				DMibError((CStr::CFormat("RegQueryValueExW failed with: {}") << fg_Win32_GetLastErrorStr(Error)).f_GetStr());
		
			if ((ValueType == REG_DWORD) || (ValueType == REG_DWORD_LITTLE_ENDIAN) || (ValueType == REG_DWORD_BIG_ENDIAN))
			{
				if (!_pDword)
					DMibError("Wrong valuetype");
			
				ValueSize=4;
				if ((Error = RegQueryValueExW(PathKey,pKeyValue,0,nullptr,(unsigned char*)_pDword,&ValueSize)) != ERROR_SUCCESS)
					DMibError((CStr::CFormat("RegQueryValueExW failed with: {}") << fg_Win32_GetLastErrorStr(Error)).f_GetStr());
			}
			else if ((ValueType==REG_SZ || ValueType==REG_MULTI_SZ || ValueType==REG_EXPAND_SZ) || (ValueType == REG_BINARY)  || (ValueType == REG_RESOURCE_REQUIREMENTS_LIST)|| (ValueType == REG_RESOURCE_LIST))
			{
				if ((Error = RegQueryValueExW(PathKey,pKeyValue,0,nullptr,nullptr,&ValueSize))==ERROR_SUCCESS)
				{
					if (_pMulti)
					{
						TCVector<ch16> Temp;
						ValueSize = (((ValueSize+1)/2) + 2) * 2;
						Temp.f_SetLen(ValueSize);
						if ((Error = RegQueryValueExW(PathKey,pKeyValue,0,nullptr,(uint8 *)Temp.f_GetArray(),&ValueSize)) != ERROR_SUCCESS)
							DMibError((CStr::CFormat("RegQueryValueExW failed with: {}") << fg_Win32_GetLastErrorStr(Error)).f_GetStr());

						const ch16 *pParse = Temp.f_GetArray();
						const ch16 *pParseEnd = pParse + ValueSize/2;
						while(1)
						{
							const ch16 *pParseStart = pParse;
							while (*pParse && pParse < pParseEnd)						
							{
								++pParse;
							}
							if (pParseStart != pParse)
							{
								CWStr ToInsert;
								ToInsert.f_AddStr(pParseStart, pParse - pParseStart);
								_pMulti->f_Insert(CStr(ToInsert));
								++pParse;
							}
							else
								break; // End
						}
					}
					else if (_pStr)
					{
						CWStr Temp;
						ValueSize = (((ValueSize+1)/2) + 2) * 2;
						if ((Error = RegQueryValueExW(PathKey,pKeyValue,0,nullptr,(uint8 *)Temp.f_GetStr(ValueSize/2 + 1),&ValueSize)) != ERROR_SUCCESS)
							DMibError((CStr::CFormat("RegQueryValueExW failed with: {}") << fg_Win32_GetLastErrorStr(Error)).f_GetStr());
						Temp.f_SetAt(ValueSize/2, 0);
						*_pStr = Temp;
					}
					else if (_pMemory)
					{
						_pMemory->f_SetLen(ValueSize);
						if ((Error = RegQueryValueExW(PathKey,pKeyValue,0,nullptr,_pMemory->f_GetArray(),&ValueSize)) != ERROR_SUCCESS)
							DMibError((CStr::CFormat("RegQueryValueExW failed with: {}") << fg_Win32_GetLastErrorStr(Error)).f_GetStr());
					}
					else
						DMibError("Wrong valuetype");
				
				}
				else
					DMibError((CStr::CFormat("RegQueryValueExW failed with: {}") << fg_Win32_GetLastErrorStr(Error)).f_GetStr());
			}
			else
				DMibError("Unknown valuetype");
		
	
		}

		void CWin32_Registry::f_DeleteValue(const CStr &_SubKey, const CStr &_KeyValue)
		{
			HKEY PathKey = nullptr;
			CStr SubKey = fp_GetPath(_SubKey);
			CWStr KeyValue;
			const ch16 *pKeyValue;
			if (_KeyValue.f_IsEmpty())
				pKeyValue = nullptr;
			else
			{
				KeyValue = fg_StrToWindows(_KeyValue);
				pKeyValue = KeyValue;
			}
	
			HRESULT Error;
			if ((Error = RegOpenKeyExW(mp_hRootKey, fg_StrToWindows(SubKey),0,fp_GetAccess(KEY_SET_VALUE),&PathKey)) != ERROR_SUCCESS)
				DMibError(CStr::CFormat("RegOpenKeyExW failed with: {}") << fg_Win32_GetLastErrorStr(Error));
		
			auto Cleanup
				= fg_OnScopeExit
				(
					[&]
					{
						RegCloseKey(PathKey);			
					}
				)
			;

			if ((Error = RegDeleteValueW(PathKey, pKeyValue)) != ERROR_SUCCESS)
				DMibError(CStr::CFormat("RegDeleteValueW failed with: {}") << fg_Win32_GetLastErrorStr(Error));
		}

		void CWin32_Registry::fp_DeleteKey(const CStr &_Key)
		{
			TCVector<CStr> Children;
			fp_EnumKeys(_Key, Children);
			for (mint i = 0; i < Children.f_GetLen(); ++i)
			{
				fp_DeleteKey(_Key + "\\" + Children[i]);
			}

			HRESULT Error;
			if ((Error = RegDeleteKeyW(mp_hRootKey, fg_StrToWindows(_Key))) != ERROR_SUCCESS)
				DMibError(CStr::CFormat("RegDeleteKeyW failed with: {}") << fg_Win32_GetLastErrorStr(Error));

		}


		void CWin32_Registry::f_DeleteKey(const CStr &_SubKey)
		{
			CStr SubKey = fp_GetPath(_SubKey);

			fp_DeleteKey(SubKey);
		}


		void CWin32_Registry::f_Write(const CStr &_SubKey, const CStr &_KeyValue, uint32 _Value)
		{
			fp_WriteRegKey(_SubKey, _KeyValue, &_Value, nullptr, nullptr, nullptr);
		}

		/*
		void CWin32_Registry::f_Write(const CStr &_SubKey, const CStr &_KeyValue, const CStr &_Value)
		{
			CStr Temp;
			Temp = _Value;
	
			fp_WriteRegKey(_SubKey, _KeyValue, nullptr, &Temp, nullptr, nullptr);
		}
		*/

		void CWin32_Registry::f_Write(const CStr &_SubKey, const CStr &_KeyValue, const TCVector<uint8> &_Data)
		{
			fp_WriteRegKey(_SubKey, _KeyValue, nullptr, nullptr, &_Data, nullptr);
		}

		void CWin32_Registry::f_Write(const CStr &_SubKey, const CStr &_KeyValue, const CStr &_Value)
		{
			fp_WriteRegKey(_SubKey, _KeyValue, nullptr, &_Value, nullptr, nullptr);
		}

		void CWin32_Registry::f_Write(const CStr &_SubKey, const CStr &_KeyValue, const TCVector<CStr> &_Multi)
		{
			fp_WriteRegKey(_SubKey, _KeyValue, nullptr, nullptr, nullptr, &_Multi);
		}

		void CWin32_Registry::fp_WriteRegKey(const CStr &_SubKey, const CStr &_KeyValue, const DWORD *_pDword, const CStr *_pStr, const TCVector<uint8> *_pMemory, const TCVector<CStr> *_pMulti)
		{
			HKEY PathKey = nullptr;
			CStr SubKey = fp_GetPath(_SubKey);
			CWStr KeyValue;
			const ch16 *pKeyValue;
			if (_KeyValue.f_IsEmpty())
				pKeyValue = nullptr;
			else
			{
				KeyValue = fg_StrToWindows(_KeyValue);
				pKeyValue = KeyValue;
			}

			HRESULT Error; 
			if ((Error = RegCreateKeyExW(mp_hRootKey, fg_StrToWindows(SubKey),0,nullptr,REG_OPTION_NON_VOLATILE,fp_GetAccess(KEY_WRITE),nullptr,&PathKey,nullptr)) != ERROR_SUCCESS)
				DMibError(CStr::CFormat("RegCreateKeyEx failed with: {}") << fg_Win32_GetLastErrorStr(Error));

			auto Cleanup
				= fg_OnScopeExit
				(
					[&]
					{
						RegCloseKey(PathKey);			
					}
				)
			;

			if (_pDword)
			{
				if ((Error = RegSetValueExW(PathKey,pKeyValue,0,REG_DWORD,(unsigned char*)_pDword,4)) != ERROR_SUCCESS)
					DMibError(CStr::CFormat("RegSetValueExW failed with: {}") << fg_Win32_GetLastErrorStr(Error));
			}
			else if (_pMulti)
			{
				mint NeededLen = 1;
				TCVector<CWStr> TempMulti;
				TempMulti.f_SetLen(_pMulti->f_GetLen());
				for (mint i = 0; i < _pMulti->f_GetLen(); ++i)
				{
					TempMulti[i] = fg_StrToWindows((*_pMulti)[i]);
					mint Len = TempMulti[i].f_GetLen();
					if (Len)
					{
						NeededLen += Len + 1;
					}
				}

				TCVector<ch16> Temp;
				Temp.f_SetLen(NeededLen);
				ch16 *pParse = Temp.f_GetArray();

				for (mint i = 0; i < _pMulti->f_GetLen(); ++i)
				{
					mint Len = TempMulti[i].f_GetLen();
					if (Len)
					{
						NMib::NMem::fg_MemCopy(pParse, TempMulti[i].f_GetStr(), (Len + 1)*2);
						pParse += Len + 1;
					}
				}
				*pParse = 0;

				if ((Error = RegSetValueExW(PathKey,pKeyValue,0,REG_MULTI_SZ,(uint8 *)Temp.f_GetArray(),NeededLen*2)) != ERROR_SUCCESS)
				{
					DMibError(CStr::CFormat("RegSetValueEx failed with: {}") << fg_Win32_GetLastErrorStr(Error));
				}
			}
			else if (_pStr)
			{				
				CWStr Temp = fg_StrToWindows(*_pStr);

				if ((Error = RegSetValueExW(PathKey,pKeyValue,0,REG_SZ,(uint8 *)Temp.f_GetStr(),Temp.f_GetLen()*2)) != ERROR_SUCCESS)
					DMibError(CStr::CFormat("RegSetValueEx failed with: {}") << fg_Win32_GetLastErrorStr(Error));
			}
			else if (_pMemory)
			{
				if ((Error = RegSetValueExW(PathKey,pKeyValue,0,REG_BINARY,_pMemory->f_GetArray(),_pMemory->f_GetLen())) != ERROR_SUCCESS)
					DMibError(CStr::CFormat("RegSetValueEx failed with: {}") << fg_Win32_GetLastErrorStr(Error));
			}
			else
			{
				DMibError("Nothing to write");
			}
		}

		void CWin32_Registry::fp_EnumValues(const CStr &_Key, TCVector<CStr> &_Values)
		{
			HKEY PathKey = nullptr;
			CStr SubKey = _Key;

			if (RegOpenKeyW(mp_hRootKey, fg_StrToWindows(SubKey),&PathKey) != ERROR_SUCCESS)
				return;

			auto Cleanup
				= fg_OnScopeExit
				(
					[&]
					{
						RegCloseKey(PathKey);			
					}
				)
			;
		
			DWORD nValues;
			DWORD LongestValueName, TmpLength;
			RegQueryInfoKey(PathKey, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &nValues, &LongestValueName, nullptr, nullptr, nullptr);
			_Values.f_SetLen(nValues);
			LongestValueName++;

			for (mint i = 0; i < nValues; ++i)
			{
				CWStr Temp;
				TmpLength = LongestValueName;
				if(RegEnumValueW(PathKey, i, Temp.f_GetStr(LongestValueName), &TmpLength, nullptr, nullptr, nullptr, nullptr) == ERROR_NO_MORE_ITEMS)
					break;
				Temp.f_SetModified();
				_Values[i] = Temp;
			}
		}

		void CWin32_Registry::f_EnumValues(const CStr &_SubKey, TCVector<CStr> &_Values)
		{
			fp_EnumValues(fp_GetPath(_SubKey), _Values);
		}

		void CWin32_Registry::fp_EnumKeys(const CStr &_Key, TCVector<CStr> &_Keys)
		{
			HKEY PathKey = nullptr;
			CStr SubKey = _Key;

			HRESULT Error;
			if ((Error = RegOpenKeyW(mp_hRootKey, fg_StrToWindows(SubKey),&PathKey)) != ERROR_SUCCESS)
				DMibError(CStr::CFormat("RegOpenKeyW failed with: {}") << fg_Win32_GetLastErrorStr(Error));

			auto Cleanup
				= fg_OnScopeExit
				(
					[&]
					{
						RegCloseKey(PathKey);			
					}
				)
			;

			DWORD nSubKeys;
			DWORD LongestSubKey;
			RegQueryInfoKey(PathKey, nullptr, nullptr, nullptr, &nSubKeys, &LongestSubKey, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
			_Keys.f_SetLen(nSubKeys);
			LongestSubKey++;

			for (mint i = 0; i < nSubKeys; ++i)
			{
				CWStr Temp;
				if(RegEnumKeyW(PathKey, i, Temp.f_GetStr(LongestSubKey), LongestSubKey) == ERROR_NO_MORE_ITEMS)
					break;
				Temp.f_SetModified();
				_Keys[i] = Temp;
			} 
		}


		void CWin32_Registry::f_EnumKeys(const CStr &_SubKey, TCVector<CStr> &_Keys)
		{
			fp_EnumKeys(fp_GetPath(_SubKey), _Keys);
		}

	} // Namespace NRuntimeMSVC

} // Namespace NMib
