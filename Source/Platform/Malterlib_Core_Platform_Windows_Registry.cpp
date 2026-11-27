// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Core/PlatformSpecific/WindowsString>
#include <Mib/Core/PlatformSpecific/WindowsError>

#include "Malterlib_Core_Platform_Windows_Registry.h"

namespace NMib
{
	namespace NPlatform
	{
		namespace
		{
			// Whether a value of the type is read as its bytes or as strings: the types of strings and of binary data.
			bool fg_IsStoredAsBytes(DWORD _Type)
			{
				return _Type == REG_SZ || _Type == REG_MULTI_SZ || _Type == REG_EXPAND_SZ || _Type == REG_BINARY || _Type == REG_RESOURCE_REQUIREMENTS_LIST || _Type == REG_RESOURCE_LIST;
			}

			// The names of a key's values or subkeys, which _fEnum, the call named _pFunction, gives by index, starting with room for
			// _Capacity characters. They are enumerated until there are no more, and a name that grew since the longest was asked for is
			// asked for again with room for it. A name is taken by the length the enumeration gives, as a failed call leaves the buffer
			// unended.
			template <typename tf_FEnum>
			void fg_EnumNames(DWORD _Capacity, ch8 const *_pFunction, tf_FEnum &&_fEnum, NContainer::TCVector<NStr::CStr> &o_Names)
			{
				o_Names.f_Clear();
				DWORD Capacity = _Capacity;
				for (DWORD iName = 0;; ++iName)
				{
					NContainer::TCVector<NStr::CWStr::CChar> Buffer;
					DWORD Length = 0;
					LSTATUS Error;
					while (true)
					{
						Buffer.f_SetLen(Capacity);
						Length = Capacity;
						Error = _fEnum(iName, Buffer.f_GetArray(), &Length);
						if (Error != ERROR_MORE_DATA)
							break;
						Capacity *= 2;
					}

					if (Error == ERROR_NO_MORE_ITEMS)
						break;
					if (Error != ERROR_SUCCESS)
						DMibError(NStr::CStr::CFormat("{} failed with: {}") << _pFunction << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));

					o_Names.f_Insert() = NStr::CWStr(Buffer.f_GetArray(), Length);
				}
			}
		}

		/*************************************************************************************************\
		|¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
		| CWin32_Registry
		|__________________________________________________________________________________________________
		\*************************************************************************************************/

		CWin32_Registry::CWin32_Registry(ERegRoot _Root, NStr::CStr const &_RootPath, EAccess _Access)
		{
			mp_Root = _RootPath;
			mp_RegRoot = _Root;
			mp_Access = _Access;
			mp_pRootKey = HKEY_LOCAL_MACHINE;
			switch (_Root)
			{
			case ERegRoot_LocalMachine:
			case ERegRoot_Win64_LocalMachine:
			case ERegRoot_Win32_LocalMachine:
				mp_pRootKey = HKEY_LOCAL_MACHINE;
				break;
			case ERegRoot_CurrentUser:
			case ERegRoot_Win64_CurrentUser:
			case ERegRoot_Win32_CurrentUser:
				mp_pRootKey = HKEY_CURRENT_USER;
				break;
			case ERegRoot_Classes:
			case ERegRoot_Win64_Classes:
			case ERegRoot_Win32_Classes:
				mp_pRootKey = HKEY_CLASSES_ROOT;
				break;
			}
		}

		CWin32_Registry::CWin32_Registry(CWin32_Registry const &_Root, NStr::CStr const &_RootPath)
		{
			if (_RootPath.f_IsEmpty())
				mp_Root = _Root.mp_Root;
			else
				mp_Root = _Root.mp_Root + "\\" + _RootPath;

			mp_pRootKey = _Root.mp_pRootKey;
			mp_RegRoot = _Root.mp_RegRoot;
			mp_Access = _Root.mp_Access;
		}

		uint32 CWin32_Registry::fp_GetAccess(uint32 _Base)
		{
			_Base |= fp_GetView();
			if (mp_Access == EAccess::mc_ReadWrite)
				_Base |= KEY_READ | KEY_WRITE;
			return _Base;
		}

		// The registry view of the root, the process's own for the roots without one.
		uint32 CWin32_Registry::fp_GetView()
		{
			if (mp_RegRoot == ERegRoot_Win64_LocalMachine || mp_RegRoot == ERegRoot_Win64_CurrentUser || mp_RegRoot == ERegRoot_Win64_Classes)
				return KEY_WOW64_64KEY;
			if (mp_RegRoot == ERegRoot_Win32_LocalMachine || mp_RegRoot == ERegRoot_Win32_CurrentUser || mp_RegRoot == ERegRoot_Win32_Classes)
				return KEY_WOW64_32KEY;
			return 0;
		}

		uint32 CWin32_Registry::f_Read_uint32(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue)
		{
			DWORD Temp = 0;

			fp_ReadRegKey(_SubKey, _KeyValue, &Temp, nullptr, nullptr);

			return Temp;
		}

		NStr::CStr CWin32_Registry::f_Read_Str(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue)
		{
			NStr::CStr Temp;

			fp_ReadRegKey(_SubKey, _KeyValue, nullptr, &Temp, nullptr);

			return Temp;
		}

		NContainer::TCVector<NStr::CStr> CWin32_Registry::f_Read_StrMulti(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue)
		{
			NContainer::TCVector<NStr::CStr> Temp;

			fp_ReadRegKey(_SubKey, _KeyValue, nullptr, nullptr, &Temp);

			return Temp;
		}

		// The value's data as it is stored, of a type of strings or binary data.
		NContainer::CByteVector CWin32_Registry::f_Read_Bin(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue)
		{
			CRawValue Value = fp_ReadValue(_SubKey, _KeyValue);
			if (!fg_IsStoredAsBytes(Value.m_Type))
				DMibError("Wrong valuetype");

			return fg_Move(Value.m_Data);
		}

		uint32 CWin32_Registry::f_Read_uint32(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue, uint32 _Default)
		{
			if (f_ValueExists(_SubKey, _KeyValue))
				return f_Read_uint32(_SubKey, _KeyValue);
			return _Default;
		}

		NStr::CStr CWin32_Registry::f_Read_Str(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue, NStr::CStr const &_Default)
		{
			if (f_ValueExists(_SubKey, _KeyValue))
				return f_Read_Str(_SubKey, _KeyValue);
			return _Default;
		}

		NContainer::TCVector<NStr::CStr> CWin32_Registry::f_Read_StrMulti(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue, NContainer::TCVector<NStr::CStr> const &_Default)
		{
			if (f_ValueExists(_SubKey, _KeyValue))
				return f_Read_StrMulti(_SubKey, _KeyValue);
			return _Default;
		}

		NContainer::CByteVector CWin32_Registry::f_Read_Bin(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue, NContainer::CByteVector const &_Default)
		{
			if (f_ValueExists(_SubKey, _KeyValue))
				return f_Read_Bin(_SubKey, _KeyValue);
			return _Default;
		}

		NStr::CStr CWin32_Registry::fp_GetPath(NStr::CStr const &_SubKey)
		{
			if (!_SubKey.f_IsEmpty() && mp_Root.f_GetLen())
				return mp_Root + "\\" + _SubKey;
			else if (!_SubKey.f_IsEmpty())
				return _SubKey;

			return mp_Root;
		}

		bool CWin32_Registry::f_KeyExists(NStr::CStr const &_SubKey)
		{
			HKEY pPathKey = nullptr;
			NStr::CWStr SubKey = NStr::NPlatform::fg_StrToWindows(fp_GetPath(_SubKey));

			if (RegOpenKeyExW(mp_pRootKey, SubKey, 0, fp_GetAccess(KEY_READ), &pPathKey) != ERROR_SUCCESS)
				return false;
			auto Cleanup = fg_OnScopeExit
				(
					[&]
					{
						RegCloseKey(pPathKey);
					}
				)
			;
			return true;
		}

		bool CWin32_Registry::f_ValueExists(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue)
		{
			HKEY pPathKey = nullptr;
			DWORD ValueType;
			NStr::CStr SubKey = fp_GetPath(_SubKey);
			NStr::CWStr KeyValue;
			ch16 const *pKeyValue;
			if (_KeyValue.f_IsEmpty())
				pKeyValue = nullptr;
			else
			{
				KeyValue = NStr::NPlatform::fg_StrToWindows(_KeyValue);
				pKeyValue = KeyValue;
			}

			if (RegOpenKeyExW(mp_pRootKey, NStr::NPlatform::fg_StrToWindows(SubKey), 0, fp_GetAccess(KEY_READ), &pPathKey) != ERROR_SUCCESS)
				return false;

			auto Cleanup = fg_OnScopeExit
				(
					[&]
					{
						RegCloseKey(pPathKey);
					}
				)
			;

			if (RegQueryValueExW(pPathKey, pKeyValue, 0, &ValueType, nullptr, nullptr) != ERROR_SUCCESS)
				return false;

			return true;
		}

		bool CWin32_Registry::f_IsBinary(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue)
		{
			HKEY pPathKey = nullptr;
			DWORD ValueType;
			NStr::CStr SubKey = fp_GetPath(_SubKey);
			NStr::CWStr KeyValue;
			ch16 const *pKeyValue;
			if (_KeyValue.f_IsEmpty())
				pKeyValue = nullptr;
			else
			{
				KeyValue = NStr::NPlatform::fg_StrToWindows(_KeyValue);
				pKeyValue = KeyValue;
			}

			if (RegOpenKeyExW(mp_pRootKey, NStr::NPlatform::fg_StrToWindows(SubKey), 0, fp_GetAccess(KEY_READ), &pPathKey) != ERROR_SUCCESS)
				return false;

			auto Cleanup = fg_OnScopeExit
				(
					[&]
					{
						RegCloseKey(pPathKey);
					}
				)
			;

			if (RegQueryValueExW(pPathKey, pKeyValue, 0, &ValueType, nullptr, nullptr) != ERROR_SUCCESS)
				return false;

			return ValueType == REG_BINARY;
		}

		// The value's type and data as they are stored, whatever the type, or false when the key can't be opened or has no such value.
		bool CWin32_Registry::f_ReadRaw(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue, CRawValue &o_Value)
		{
			ch8 const *pFunction = nullptr;
			return fp_ReadRaw(_SubKey, _KeyValue, o_Value, pFunction) == ERROR_SUCCESS;
		}

		// The value as fp_ReadRaw reads it, with one opening of the key, or an error naming the call that failed with Windows' text of its
		// status.
		auto CWin32_Registry::fp_ReadValue(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue) -> CRawValue
		{
			CRawValue Value;
			ch8 const *pFunction = nullptr;
			if (LSTATUS Status = fp_ReadRaw(_SubKey, _KeyValue, Value, pFunction); Status != ERROR_SUCCESS)
				DMibError((NStr::CStr::CFormat("{} failed with: {}") << pFunction << NMib::NPlatform::fg_Win32_GetLastErrorStr(uint32(Status))).f_GetStr());

			return Value;
		}

		// f_ReadRaw with the status of the call that failed, and in o_pFunction its name. The value can change between the queries of
		// its size and its data, which are retried.
		LSTATUS CWin32_Registry::fp_ReadRaw(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue, CRawValue &o_Value, ch8 const * &o_pFunction)
		{
			HKEY pPathKey = nullptr;
			NStr::CStr SubKey = fp_GetPath(_SubKey);
			NStr::CWStr KeyValue;
			ch16 const *pKeyValue = nullptr;
			if (!_KeyValue.f_IsEmpty())
			{
				KeyValue = NStr::NPlatform::fg_StrToWindows(_KeyValue);
				pKeyValue = KeyValue;
			}

			o_pFunction = "RegOpenKeyExW";
			if (LSTATUS Status = RegOpenKeyExW(mp_pRootKey, NStr::NPlatform::fg_StrToWindows(SubKey), 0, fp_GetAccess(KEY_READ), &pPathKey); Status != ERROR_SUCCESS)
				return Status;

			auto Cleanup = fg_OnScopeExit
				(
					[&]
					{
						RegCloseKey(pPathKey);
					}
				)
			;

			o_pFunction = "RegQueryValueExW";
			while (true)
			{
				DWORD Type = REG_NONE;
				DWORD Size = 0;
				if (LSTATUS Status = RegQueryValueExW(pPathKey, pKeyValue, nullptr, &Type, nullptr, &Size); Status != ERROR_SUCCESS)
					return Status;

				// A buffer even for a value without data, so one that grew since the query reports ERROR_MORE_DATA rather than its size alone.
				o_Value.m_Data.f_SetLen(fg_Max(Size, DWORD(1)));
				Size = DWORD(o_Value.m_Data.f_GetLen());
				LSTATUS Status = RegQueryValueExW(pPathKey, pKeyValue, nullptr, &Type, o_Value.m_Data.f_GetArray(), &Size);
				if (Status == ERROR_MORE_DATA)
					continue;
				if (Status != ERROR_SUCCESS)
					return Status;

				o_Value.m_Data.f_SetLen(Size);
				o_Value.m_Type = Type;
				return ERROR_SUCCESS;
			}
		}

		// The value read as the type its reader takes: a DWORD as the 4 bytes it is stored as, the first the lowest, and a string or the
		// strings of a multi-string as UTF-16 units, each ended by a NUL or the data's end.
		void CWin32_Registry::fp_ReadRegKey(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue, DWORD *_pDword, NStr::CStr *_pStr, NContainer::TCVector<NStr::CStr> *_pMulti)
		{
			CRawValue Value = fp_ReadValue(_SubKey, _KeyValue);
			if (Value.m_Type == REG_DWORD || Value.m_Type == REG_DWORD_LITTLE_ENDIAN || Value.m_Type == REG_DWORD_BIG_ENDIAN)
			{
				if (!_pDword)
					DMibError("Wrong valuetype");

				if (Value.m_Data.f_GetLen() != sizeof(DWORD))
					DMibError((NStr::CStr::CFormat("The registry value '{}' of '{}' holds {} bytes, where a DWORD has 4") << _KeyValue << _SubKey << Value.m_Data.f_GetLen()).f_GetStr());

				DWORD Dword = 0;
				NMemory::fg_MemCopy(&Dword, Value.m_Data.f_GetArray(), sizeof(Dword));
				*_pDword = fg_ByteSwapLE(Dword);
				return;
			}

			if (!fg_IsStoredAsBytes(Value.m_Type))
				DMibError("Unknown valuetype");

			ch16 const *pUnits = (ch16 const *)Value.m_Data.f_GetArray();
			umint nUnits = Value.m_Data.f_GetLen() / 2;
			if (_pMulti)
			{
				for (umint iUnit = 0; iUnit < nUnits && pUnits[iUnit];)
				{
					umint nString = NStr::fg_StrLen(pUnits + iUnit, nUnits - iUnit);
					NStr::CWStr String;
					String.f_AddStr(pUnits + iUnit, aint(nString));
					_pMulti->f_Insert(NStr::CStr(String));
					iUnit += nString + 1;
				}
			}
			else if (_pStr)
			{
				NStr::CWStr String;
				if (umint nString = nUnits ? NStr::fg_StrLen(pUnits, nUnits) : 0)
					String.f_AddStr(pUnits, aint(nString));

				*_pStr = String;
			}
			else
				DMibError("Wrong valuetype");
		}

		void CWin32_Registry::f_DeleteValue(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue)
		{
			HKEY pPathKey = nullptr;
			NStr::CStr SubKey = fp_GetPath(_SubKey);
			NStr::CWStr KeyValue;
			ch16 const *pKeyValue;
			if (_KeyValue.f_IsEmpty())
				pKeyValue = nullptr;
			else
			{
				KeyValue = NStr::NPlatform::fg_StrToWindows(_KeyValue);
				pKeyValue = KeyValue;
			}

			HRESULT Error;
			if ((Error = RegOpenKeyExW(mp_pRootKey, NStr::NPlatform::fg_StrToWindows(SubKey), 0, fp_GetAccess(KEY_SET_VALUE), &pPathKey)) != ERROR_SUCCESS)
				DMibError(NStr::CStr::CFormat("RegOpenKeyExW failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));

			auto Cleanup = fg_OnScopeExit
				(
					[&]
					{
						RegCloseKey(pPathKey);
					}
				)
			;

			if ((Error = RegDeleteValueW(pPathKey, pKeyValue)) != ERROR_SUCCESS)
				DMibError(NStr::CStr::CFormat("RegDeleteValueW failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
		}

		void CWin32_Registry::fp_DeleteKey(NStr::CStr const &_Key)
		{
			NContainer::TCVector<NStr::CStr> Children;
			fp_EnumKeys(_Key, Children);
			for (umint iChild = 0; iChild < Children.f_GetLen(); ++iChild)
				fp_DeleteKey(_Key + "\\" + Children[iChild]);

			HRESULT Error;
			if ((Error = RegDeleteKeyExW(mp_pRootKey, NStr::NPlatform::fg_StrToWindows(_Key), fp_GetView(), 0)) != ERROR_SUCCESS)
				DMibError(NStr::CStr::CFormat("RegDeleteKeyExW failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
		}

		void CWin32_Registry::f_DeleteKey(NStr::CStr const &_SubKey)
		{
			NStr::CStr SubKey = fp_GetPath(_SubKey);

			fp_DeleteKey(SubKey);
		}

		void CWin32_Registry::f_Write(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue, uint32 _Value)
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

		void CWin32_Registry::f_Write(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue, NContainer::CByteVector const &_Data)
		{
			fp_WriteRegKey(_SubKey, _KeyValue, nullptr, nullptr, &_Data, nullptr);
		}

		void CWin32_Registry::f_Write(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue, NStr::CStr const &_Value)
		{
			fp_WriteRegKey(_SubKey, _KeyValue, nullptr, &_Value, nullptr, nullptr);
		}

		void CWin32_Registry::f_Write(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue, NContainer::TCVector<NStr::CStr> const &_Multi)
		{
			fp_WriteRegKey(_SubKey, _KeyValue, nullptr, nullptr, nullptr, &_Multi);
		}

		void CWin32_Registry::fp_WriteRegKey
			(
				NStr::CStr const &_SubKey
				, NStr::CStr const &_KeyValue
				, DWORD const *_pDword
				, NStr::CStr const *_pStr
				, NContainer::CByteVector const *_pMemory
				, NContainer::TCVector<NStr::CStr> const *_pMulti
			)
		{
			HKEY pPathKey = nullptr;
			NStr::CStr SubKey = fp_GetPath(_SubKey);
			NStr::CWStr KeyValue;
			ch16 const *pKeyValue;
			if (_KeyValue.f_IsEmpty())
				pKeyValue = nullptr;
			else
			{
				KeyValue = NStr::NPlatform::fg_StrToWindows(_KeyValue);
				pKeyValue = KeyValue;
			}

			HRESULT Error;
			if
			(
				(Error = RegCreateKeyExW(mp_pRootKey, NStr::NPlatform::fg_StrToWindows(SubKey), 0, nullptr, REG_OPTION_NON_VOLATILE, fp_GetAccess(KEY_WRITE), nullptr, &pPathKey, nullptr))
				!= ERROR_SUCCESS
			)
			{
				DMibError(NStr::CStr::CFormat("RegCreateKeyEx({}) failed with: {}") << SubKey << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
			}

			auto Cleanup = fg_OnScopeExit
				(
					[&]
					{
						RegCloseKey(pPathKey);
					}
				)
			;

			if (_pDword)
			{
				if ((Error = RegSetValueExW(pPathKey, pKeyValue, 0, REG_DWORD, (unsigned char *)_pDword, 4)) != ERROR_SUCCESS)
					DMibError(NStr::CStr::CFormat("RegSetValueExW failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
			}
			else if (_pMulti)
			{
				umint NeededLen = 1;
				NContainer::TCVector<NStr::CWStr> TempMulti;
				TempMulti.f_SetLen(_pMulti->f_GetLen());
				for (umint iString = 0; iString < _pMulti->f_GetLen(); ++iString)
				{
					TempMulti[iString] = NStr::NPlatform::fg_StrToWindows((*_pMulti)[iString]);
					umint Len = TempMulti[iString].f_GetLen();
					if (Len)
						NeededLen += Len + 1;
				}

				NContainer::TCVector<ch16> Temp;
				Temp.f_SetLen(NeededLen);
				ch16 *pParse = Temp.f_GetArray();

				for (umint iString = 0; iString < _pMulti->f_GetLen(); ++iString)
				{
					umint Len = TempMulti[iString].f_GetLen();
					if (Len)
					{
						NMib::NMemory::fg_MemCopy(pParse, TempMulti[iString].f_GetStr(), (Len + 1) * 2);
						pParse += Len + 1;
					}
				}
				*pParse = 0;

				if ((Error = RegSetValueExW(pPathKey, pKeyValue, 0, REG_MULTI_SZ, (uint8 *)Temp.f_GetArray(), NeededLen * 2)) != ERROR_SUCCESS)
					DMibError(NStr::CStr::CFormat("RegSetValueEx failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
			}
			else if (_pStr)
			{
				NStr::CWStr Temp = NStr::NPlatform::fg_StrToWindows(*_pStr);

				if ((Error = RegSetValueExW(pPathKey, pKeyValue, 0, REG_SZ, (uint8 *)Temp.f_GetStr(), Temp.f_GetLen() * 2)) != ERROR_SUCCESS)
					DMibError(NStr::CStr::CFormat("RegSetValueEx failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
			}
			else if (_pMemory)
			{
				if ((Error = RegSetValueExW(pPathKey, pKeyValue, 0, REG_BINARY, _pMemory->f_GetArray(), _pMemory->f_GetLen())) != ERROR_SUCCESS)
					DMibError(NStr::CStr::CFormat("RegSetValueEx failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
			}
			else
				DMibError("Nothing to write");
		}

		void CWin32_Registry::fp_EnumValues(NStr::CStr const &_Key, NContainer::TCVector<NStr::CStr> &_Values)
		{
			HKEY pPathKey = nullptr;
			NStr::CStr SubKey = _Key;

			if (RegOpenKeyExW(mp_pRootKey, NStr::NPlatform::fg_StrToWindows(SubKey), 0, fp_GetAccess(KEY_READ), &pPathKey) != ERROR_SUCCESS)
				return;

			auto Cleanup = fg_OnScopeExit
				(
					[&]
					{
						RegCloseKey(pPathKey);
					}
				)
			;

			DWORD LongestValueName = 0;
			if
			(
				LSTATUS Error = RegQueryInfoKeyW(pPathKey, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &LongestValueName, nullptr, nullptr, nullptr)
				; Error != ERROR_SUCCESS
			)
			{
				DMibError(NStr::CStr::CFormat("RegQueryInfoKeyW failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
			}

			fg_EnumNames
				(
					LongestValueName + 1
					, "RegEnumValueW"
					, [&](DWORD _iValue, NStr::CWStr::CChar *_pName, DWORD *_pLength)
					{
						return RegEnumValueW(pPathKey, _iValue, _pName, _pLength, nullptr, nullptr, nullptr, nullptr);
					}
					, _Values
				)
			;
		}

		void CWin32_Registry::f_EnumValues(NStr::CStr const &_SubKey, NContainer::TCVector<NStr::CStr> &_Values)
		{
			fp_EnumValues(fp_GetPath(_SubKey), _Values);
		}

		void CWin32_Registry::fp_EnumKeys(NStr::CStr const &_Key, NContainer::TCVector<NStr::CStr> &_Keys)
		{
			HKEY pPathKey = nullptr;
			NStr::CStr SubKey = _Key;

			HRESULT Error;
			if ((Error = RegOpenKeyExW(mp_pRootKey, NStr::NPlatform::fg_StrToWindows(SubKey), 0, fp_GetAccess(KEY_READ), &pPathKey)) != ERROR_SUCCESS)
				DMibError(NStr::CStr::CFormat("RegOpenKeyExW failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));

			auto Cleanup = fg_OnScopeExit
				(
					[&]
					{
						RegCloseKey(pPathKey);
					}
				)
			;

			DWORD LongestSubKey = 0;
			if
			(
				LSTATUS QueryError = RegQueryInfoKeyW(pPathKey, nullptr, nullptr, nullptr, nullptr, &LongestSubKey, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)
				; QueryError != ERROR_SUCCESS
			)
			{
				DMibError(NStr::CStr::CFormat("RegQueryInfoKeyW failed with: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(QueryError));
			}

			fg_EnumNames
				(
					LongestSubKey + 1
					, "RegEnumKeyExW"
					, [&](DWORD _iSubKey, NStr::CWStr::CChar *_pName, DWORD *_pLength)
					{
						return RegEnumKeyExW(pPathKey, _iSubKey, _pName, _pLength, nullptr, nullptr, nullptr, nullptr);
					}
					, _Keys
				)
			;
		}

		void CWin32_Registry::f_EnumKeys(NStr::CStr const &_SubKey, NContainer::TCVector<NStr::CStr> &_Keys)
		{
			fp_EnumKeys(fp_GetPath(_SubKey), _Keys);
		}
	} // Namespace NRuntimeMSVC
} // Namespace NMib
