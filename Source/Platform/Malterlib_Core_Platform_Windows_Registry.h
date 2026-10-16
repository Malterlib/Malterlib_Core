// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>

#ifdef DPlatformFamily_Windows
#include <windows.h>

namespace NMib
{
	namespace NPlatform
	{
		/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
			Class:		A helper class for reading and writing to the windows
						registry

			Comments:	Create the class with the root key, and the root path
						you want to manipulate the registry.

						If you want to manipulate the default value of a key
						send nullptr to _KeyValue.

						_pRoot sent to constructor should NOT have \ at the
						end.

		\*____________________________________________________________________*/
		class CWin32_Registry
		{
		public:
			enum ERegRoot
			{
				ERegRoot_LocalMachine
				, ERegRoot_CurrentUser
				, ERegRoot_Classes
				, ERegRoot_Win64_LocalMachine
				, ERegRoot_Win64_CurrentUser
				, ERegRoot_Win64_Classes
			};

		private:
			HKEY mp_hRootKey;
			ERegRoot mp_RegRoot;
			NStr::CStr mp_Root;
			void fp_DeleteKey(NStr::CStr const &_Key);
			void fp_EnumValues(NStr::CStr const &_Key, NContainer::TCVector<NStr::CStr> &_Values);
			void fp_EnumKeys(NStr::CStr const &_Key, NContainer::TCVector<NStr::CStr> &_Keys);
			uint32 fp_GetAccess(uint32 _Base);
			void fp_ReadRegKey
				(
					NStr::CStr const &_SubKey
					, NStr::CStr const &_KeyValue
					, DWORD *_pDword
					, NStr::CStr *_pStr
					, NContainer::CByteVector *_pMemory
					, NContainer::TCVector<NStr::CStr> *_pMulti
				)
			;
			NStr::CStr fp_GetPath(NStr::CStr const &_SubKey);
			void fp_WriteRegKey
				(
					NStr::CStr const &_SubKey
					, NStr::CStr const &_KeyValue
					, DWORD const *_pDword
					, NStr::CStr const *_pStr
					, NContainer::CByteVector const *_pMemory
					, NContainer::TCVector<NStr::CStr> const *_pMulti
				)
			;

		public:
			CWin32_Registry(ERegRoot _Root = ERegRoot_LocalMachine, NStr::CStr const &_RootPath = NStr::CStr());
			CWin32_Registry(CWin32_Registry const &_Root, NStr::CStr const &_RootPath = NStr::CStr());

			template <typename tf_CReturnType>
			tf_CReturnType f_Read(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue);

			template <>
			uint32 f_Read<uint32>(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue)
			{
				DWORD Temp = 0;

				fp_ReadRegKey(_SubKey, _KeyValue, &Temp, nullptr, nullptr, nullptr);

				return Temp;
			}

			template <>
			NStr::CStr f_Read<NStr::CStr>(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue)
			{
				NStr::CStr Temp;

				fp_ReadRegKey(_SubKey, _KeyValue, nullptr, &Temp, nullptr, nullptr);

				return Temp;
			}

			template <>
			NContainer::CByteVector f_Read<NContainer::CByteVector>(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue)
			{
				NContainer::CByteVector Temp;

				fp_ReadRegKey(_SubKey, _KeyValue, nullptr, nullptr, &Temp, nullptr);

				return Temp;
			}

			template <>
			NContainer::TCVector<NStr::CStr> f_Read<NContainer::TCVector<NStr::CStr>>(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue)
			{
				NContainer::TCVector<NStr::CStr> Temp;

				fp_ReadRegKey(_SubKey, _KeyValue, nullptr, nullptr, nullptr, &Temp);

				return Temp;
			}

			bool f_KeyExists(NStr::CStr const &_SubKey);
			bool f_ValueExists(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue);

			uint32 f_Read_uint32(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue);
			NStr::CStr f_Read_Str(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue);
			NContainer::TCVector<NStr::CStr> f_Read_StrMulti(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue);
			NContainer::CByteVector f_Read_Bin(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue);

			uint32 f_Read_uint32(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue, uint32 _Default);
			NStr::CStr f_Read_Str(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue, NStr::CStr const &_Default);
			NContainer::TCVector<NStr::CStr> f_Read_StrMulti(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue, NContainer::TCVector<NStr::CStr> const &_Default);
			NContainer::CByteVector f_Read_Bin(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue, NContainer::CByteVector const &_Default);

			bool f_IsBinary(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue);

			void f_DeleteValue(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue);
			void f_DeleteKey(NStr::CStr const &_SubKey);

			void f_Write(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue, uint32 _Value);
			void f_Write(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue, NStr::CStr const &_Value);
			void f_Write(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue, NContainer::CByteVector const &_Data);
			void f_Write(NStr::CStr const &_SubKey, NStr::CStr const &_KeyValue, NContainer::TCVector<NStr::CStr> const &_Multi);

			void f_EnumValues(NStr::CStr const &_SubKey, NContainer::TCVector<NStr::CStr> &_Values);

			void f_EnumKeys(NStr::CStr const &_SubKey, NContainer::TCVector<NStr::CStr> &_Keys);
		};
	}
}

#endif
