// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Core/PlatformSpecific/WindowsString>
#include <Mib/Core/PlatformSpecific/WindowsError>

#include "Malterlib_Core_Platform_Windows_Registry.h"

namespace NMib
{
	namespace NPlatform
	{
		/*************************************************************************************************\
		|¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
		| CWin32_Registry
		|__________________________________________________________________________________________________
		\*************************************************************************************************/

		CWin32_Registry::CWin32_Registry(ERegRoot _Root, const NStr::CStr &_RootPath)
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

		CWin32_Registry::CWin32_Registry(CWin32_Registry const& _Root, const NStr::CStr &_RootPath)
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


		uint32 CWin32_Registry::f_Read_uint32(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue)
		{
			DWORD Temp = 0;
	
			fp_ReadRegKey(_SubKey, _KeyValue, &Temp, nullptr, nullptr, nullptr);
	
			return Temp;
		}

		NStr::CStr CWin32_Registry::f_Read_Str(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue)
		{
			NStr::CStr Temp;
	
			fp_ReadRegKey(_SubKey, _KeyValue, nullptr, &Temp, nullptr, nullptr);
	
			return Temp;
		}

		NContainer::TCVector<NStr::CStr> CWin32_Registry::f_Read_StrMulti(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue)
		{
			NContainer::TCVector<NStr::CStr> Temp;
	
			fp_ReadRegKey(_SubKey, _KeyValue, nullptr, nullptr, nullptr, &Temp);
	
			return Temp;
		}

		NContainer::TCVector<uint8> CWin32_Registry::f_Read_Bin(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue)
		{
			NContainer::TCVector<uint8> Temp;
	
			fp_ReadRegKey(_SubKey, _KeyValue, nullptr, nullptr, &Temp, nullptr);
	
			return Temp;
		}

	
		uint32 CWin32_Registry::f_Read_uint32(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue, uint32 _Default)
		{
			if (f_ValueExists(_SubKey, _KeyValue))
				return f_Read_uint32(_SubKey, _KeyValue);
			return _Default;
		}

		NStr::CStr CWin32_Registry::f_Read_Str(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue, const NStr::CStr &_Default)
		{
			if (f_ValueExists(_SubKey, _KeyValue))
				return f_Read_Str(_SubKey, _KeyValue);
			return _Default;
		}

		NContainer::TCVector<NStr::CStr> CWin32_Registry::f_Read_StrMulti(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue, const NContainer::TCVector<NStr::CStr> &_Default)
		{
			if (f_ValueExists(_SubKey, _KeyValue))
				return f_Read_StrMulti(_SubKey, _KeyValue);
			return _Default;
		}


		NContainer::TCVector<uint8> CWin32_Registry::f_Read_Bin(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue, const NContainer::TCVector<uint8> &_Default)
		{
			if (f_ValueExists(_SubKey, _KeyValue))
				return f_Read_Bin(_SubKey, _KeyValue);
			return _Default;
		}

		NStr::CStr CWin32_Registry::fp_GetPath(const NStr::CStr &_SubKey)
		{
			if (!_SubKey.f_IsEmpty() && mp_Root.f_GetLen())
			{
				return mp_Root + "\\" + _SubKey;
			}
			else if (!_SubKey.f_IsEmpty())
				return _SubKey;

			return mp_Root;
		}

		bint CWin32_Registry::f_KeyExists(const NStr::CStr &_SubKey)
		{
			HKEY PathKey = nullptr;
			NStr::CWStr SubKey = NStr::NPlatform::fg_StrToWindows(_SubKey);
	
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

		bint CWin32_Registry::f_ValueExists(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue)
		{
			HKEY PathKey = nullptr;
			DWORD ValueType;
			NStr::CStr SubKey = fp_GetPath(_SubKey);
			NStr::CWStr KeyValue;
			const ch16 *pKeyValue;
			if (_KeyValue.f_IsEmpty())
				pKeyValue = nullptr;
			else
			{
				KeyValue = NStr::NPlatform::fg_StrToWindows(_KeyValue);
				pKeyValue = KeyValue;
			}
	
			if (RegOpenKeyExW(mp_hRootKey, NStr::NPlatform::fg_StrToWindows(SubKey),0,fp_GetAccess(KEY_READ),&PathKey) != ERROR_SUCCESS)
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


		bint CWin32_Registry::f_IsBinary(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue)
		{
			HKEY PathKey = nullptr;
			DWORD ValueType;
			NStr::CStr SubKey = fp_GetPath(_SubKey);
			NStr::CWStr KeyValue;
			const ch16 *pKeyValue;
			if (_KeyValue.f_IsEmpty())
				pKeyValue = nullptr;
			else
			{
				KeyValue = NStr::NPlatform::fg_StrToWindows(_KeyValue);
				pKeyValue = KeyValue;
			}
	
			if (RegOpenKeyExW(mp_hRootKey, NStr::NPlatform::fg_StrToWindows(SubKey),0,fp_GetAccess(KEY_READ),&PathKey) != ERROR_SUCCESS)
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

		void CWin32_Registry::fp_ReadRegKey(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue, DWORD *_pDword, NStr::CStr *_pStr, NContainer::TCVector<uint8> *_pMemory, NContainer::TCVector<NStr::CStr> *_pMulti)
		{		
			DWORD ValueSize = 0;
			HKEY PathKey = nullptr;
			DWORD ValueType;
			NStr::CStr SubKey = fp_GetPath(_SubKey);
			NStr::CWStr KeyValue;
			const ch16 *pKeyValue;
			if (_KeyValue.f_IsEmpty())
				pKeyValue = nullptr;
			else
			{
				KeyValue = NStr::NPlatform::fg_StrToWindows(_KeyValue);
				pKeyValue = KeyValue;
			}

			HRESULT Error;
			if ((Error = RegOpenKeyExW(mp_hRootKey, NStr::NPlatform::fg_StrToWindows(SubKey),0,fp_GetAccess(KEY_READ),&PathKey)) != ERROR_SUCCESS)
				DMibError((NStr::CStr::CFormat("RegOpenKeyExW failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());

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
				DMibError((NStr::CStr::CFormat("RegQueryValueExW failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
		
			if ((ValueType == REG_DWORD) || (ValueType == REG_DWORD_LITTLE_ENDIAN) || (ValueType == REG_DWORD_BIG_ENDIAN))
			{
				if (!_pDword)
					DMibError("Wrong valuetype");
			
				ValueSize=4;
				if ((Error = RegQueryValueExW(PathKey,pKeyValue,0,nullptr,(unsigned char*)_pDword,&ValueSize)) != ERROR_SUCCESS)
					DMibError((NStr::CStr::CFormat("RegQueryValueExW failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
			}
			else if ((ValueType==REG_SZ || ValueType==REG_MULTI_SZ || ValueType==REG_EXPAND_SZ) || (ValueType == REG_BINARY)  || (ValueType == REG_RESOURCE_REQUIREMENTS_LIST)|| (ValueType == REG_RESOURCE_LIST))
			{
				if ((Error = RegQueryValueExW(PathKey,pKeyValue,0,nullptr,nullptr,&ValueSize))==ERROR_SUCCESS)
				{
					if (_pMulti)
					{
						NContainer::TCVector<ch16> Temp;
						ValueSize = (((ValueSize+1)/2) + 2) * 2;
						Temp.f_SetLen(ValueSize);
						if ((Error = RegQueryValueExW(PathKey,pKeyValue,0,nullptr,(uint8 *)Temp.f_GetArray(),&ValueSize)) != ERROR_SUCCESS)
							DMibError((NStr::CStr::CFormat("RegQueryValueExW failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());

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
								NStr::CWStr ToInsert;
								ToInsert.f_AddStr(pParseStart, pParse - pParseStart);
								_pMulti->f_Insert(NStr::CStr(ToInsert));
								++pParse;
							}
							else
								break; // End
						}
					}
					else if (_pStr)
					{
						NStr::CWStr Temp;
						ValueSize = (((ValueSize+1)/2) + 2) * 2;
						if ((Error = RegQueryValueExW(PathKey,pKeyValue,0,nullptr,(uint8 *)Temp.f_GetStr(ValueSize/2 + 1),&ValueSize)) != ERROR_SUCCESS)
							DMibError((NStr::CStr::CFormat("RegQueryValueExW failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
						Temp.f_SetAt(ValueSize/2, 0);
						*_pStr = Temp;
					}
					else if (_pMemory)
					{
						_pMemory->f_SetLen(ValueSize);
						if ((Error = RegQueryValueExW(PathKey,pKeyValue,0,nullptr,_pMemory->f_GetArray(),&ValueSize)) != ERROR_SUCCESS)
							DMibError((NStr::CStr::CFormat("RegQueryValueExW failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
					}
					else
						DMibError("Wrong valuetype");
				
				}
				else
					DMibError((NStr::CStr::CFormat("RegQueryValueExW failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)).f_GetStr());
			}
			else
				DMibError("Unknown valuetype");
		
	
		}

		void CWin32_Registry::f_DeleteValue(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue)
		{
			HKEY PathKey = nullptr;
			NStr::CStr SubKey = fp_GetPath(_SubKey);
			NStr::CWStr KeyValue;
			const ch16 *pKeyValue;
			if (_KeyValue.f_IsEmpty())
				pKeyValue = nullptr;
			else
			{
				KeyValue = NStr::NPlatform::fg_StrToWindows(_KeyValue);
				pKeyValue = KeyValue;
			}
	
			HRESULT Error;
			if ((Error = RegOpenKeyExW(mp_hRootKey, NStr::NPlatform::fg_StrToWindows(SubKey),0,fp_GetAccess(KEY_SET_VALUE),&PathKey)) != ERROR_SUCCESS)
				DMibError(NStr::CStr::CFormat("RegOpenKeyExW failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
		
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
				DMibError(NStr::CStr::CFormat("RegDeleteValueW failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
		}

		void CWin32_Registry::fp_DeleteKey(const NStr::CStr &_Key)
		{
			NContainer::TCVector<NStr::CStr> Children;
			fp_EnumKeys(_Key, Children);
			for (mint i = 0; i < Children.f_GetLen(); ++i)
			{
				fp_DeleteKey(_Key + "\\" + Children[i]);
			}

			HRESULT Error;
			if ((Error = RegDeleteKeyW(mp_hRootKey, NStr::NPlatform::fg_StrToWindows(_Key))) != ERROR_SUCCESS)
				DMibError(NStr::CStr::CFormat("RegDeleteKeyW failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));

		}


		void CWin32_Registry::f_DeleteKey(const NStr::CStr &_SubKey)
		{
			NStr::CStr SubKey = fp_GetPath(_SubKey);

			fp_DeleteKey(SubKey);
		}


		void CWin32_Registry::f_Write(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue, uint32 _Value)
		{
			fp_WriteRegKey(_SubKey, _KeyValue, &_Value, nullptr, nullptr, nullptr);
		}

		/*
		void CWin32_Registry::f_Write(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue, const NStr::CStr &_Value)
		{
			NStr::CStr Temp;
			Temp = _Value;
	
			fp_WriteRegKey(_SubKey, _KeyValue, nullptr, &Temp, nullptr, nullptr);
		}
		*/

		void CWin32_Registry::f_Write(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue, const NContainer::TCVector<uint8> &_Data)
		{
			fp_WriteRegKey(_SubKey, _KeyValue, nullptr, nullptr, &_Data, nullptr);
		}

		void CWin32_Registry::f_Write(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue, const NStr::CStr &_Value)
		{
			fp_WriteRegKey(_SubKey, _KeyValue, nullptr, &_Value, nullptr, nullptr);
		}

		void CWin32_Registry::f_Write(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue, const NContainer::TCVector<NStr::CStr> &_Multi)
		{
			fp_WriteRegKey(_SubKey, _KeyValue, nullptr, nullptr, nullptr, &_Multi);
		}

		void CWin32_Registry::fp_WriteRegKey(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue, const DWORD *_pDword, const NStr::CStr *_pStr, const NContainer::TCVector<uint8> *_pMemory, const NContainer::TCVector<NStr::CStr> *_pMulti)
		{
			HKEY PathKey = nullptr;
			NStr::CStr SubKey = fp_GetPath(_SubKey);
			NStr::CWStr KeyValue;
			const ch16 *pKeyValue;
			if (_KeyValue.f_IsEmpty())
				pKeyValue = nullptr;
			else
			{
				KeyValue = NStr::NPlatform::fg_StrToWindows(_KeyValue);
				pKeyValue = KeyValue;
			}

			HRESULT Error; 
			if ((Error = RegCreateKeyExW(mp_hRootKey, NStr::NPlatform::fg_StrToWindows(SubKey),0,nullptr,REG_OPTION_NON_VOLATILE,fp_GetAccess(KEY_WRITE),nullptr,&PathKey,nullptr)) != ERROR_SUCCESS)
				DMibError(NStr::CStr::CFormat("RegCreateKeyEx failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));

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
					DMibError(NStr::CStr::CFormat("RegSetValueExW failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
			}
			else if (_pMulti)
			{
				mint NeededLen = 1;
				NContainer::TCVector<NStr::CWStr> TempMulti;
				TempMulti.f_SetLen(_pMulti->f_GetLen());
				for (mint i = 0; i < _pMulti->f_GetLen(); ++i)
				{
					TempMulti[i] = NStr::NPlatform::fg_StrToWindows((*_pMulti)[i]);
					mint Len = TempMulti[i].f_GetLen();
					if (Len)
					{
						NeededLen += Len + 1;
					}
				}

				NContainer::TCVector<ch16> Temp;
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
					DMibError(NStr::CStr::CFormat("RegSetValueEx failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
				}
			}
			else if (_pStr)
			{				
				NStr::CWStr Temp = NStr::NPlatform::fg_StrToWindows(*_pStr);

				if ((Error = RegSetValueExW(PathKey,pKeyValue,0,REG_SZ,(uint8 *)Temp.f_GetStr(),Temp.f_GetLen()*2)) != ERROR_SUCCESS)
					DMibError(NStr::CStr::CFormat("RegSetValueEx failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
			}
			else if (_pMemory)
			{
				if ((Error = RegSetValueExW(PathKey,pKeyValue,0,REG_BINARY,_pMemory->f_GetArray(),_pMemory->f_GetLen())) != ERROR_SUCCESS)
					DMibError(NStr::CStr::CFormat("RegSetValueEx failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
			}
			else
			{
				DMibError("Nothing to write");
			}
		}

		void CWin32_Registry::fp_EnumValues(const NStr::CStr &_Key, NContainer::TCVector<NStr::CStr> &_Values)
		{
			HKEY PathKey = nullptr;
			NStr::CStr SubKey = _Key;

			if (RegOpenKeyW(mp_hRootKey, NStr::NPlatform::fg_StrToWindows(SubKey),&PathKey) != ERROR_SUCCESS)
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
				NStr::CWStr Temp;
				TmpLength = LongestValueName;
				if(RegEnumValueW(PathKey, i, Temp.f_GetStr(LongestValueName), &TmpLength, nullptr, nullptr, nullptr, nullptr) == ERROR_NO_MORE_ITEMS)
					break;
				Temp.f_SetModified();
				_Values[i] = Temp;
			}
		}

		void CWin32_Registry::f_EnumValues(const NStr::CStr &_SubKey, NContainer::TCVector<NStr::CStr> &_Values)
		{
			fp_EnumValues(fp_GetPath(_SubKey), _Values);
		}

		void CWin32_Registry::fp_EnumKeys(const NStr::CStr &_Key, NContainer::TCVector<NStr::CStr> &_Keys)
		{
			HKEY PathKey = nullptr;
			NStr::CStr SubKey = _Key;

			HRESULT Error;
			if ((Error = RegOpenKeyW(mp_hRootKey, NStr::NPlatform::fg_StrToWindows(SubKey),&PathKey)) != ERROR_SUCCESS)
				DMibError(NStr::CStr::CFormat("RegOpenKeyW failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));

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
				NStr::CWStr Temp;
				if(RegEnumKeyW(PathKey, i, Temp.f_GetStr(LongestSubKey), LongestSubKey) == ERROR_NO_MORE_ITEMS)
					break;
				Temp.f_SetModified();
				_Keys[i] = Temp;
			} 
		}


		void CWin32_Registry::f_EnumKeys(const NStr::CStr &_SubKey, NContainer::TCVector<NStr::CStr> &_Keys)
		{
			fp_EnumKeys(fp_GetPath(_SubKey), _Keys);
		}

	} // Namespace NRuntimeMSVC

} // Namespace NMib
