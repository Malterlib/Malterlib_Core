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
				,ERegRoot_CurrentUser
				,ERegRoot_Classes
				,ERegRoot_Win64_LocalMachine
				,ERegRoot_Win64_CurrentUser
				,ERegRoot_Win64_Classes
			};

		private:
			HKEY mp_hRootKey;
			ERegRoot mp_RegRoot;
			NStr::CStr mp_Root;
			void fp_DeleteKey(const NStr::CStr &_Key);
			void fp_EnumValues(const NStr::CStr &_Key, NContainer::TCVector<NStr::CStr> &_Values);
			void fp_EnumKeys(const NStr::CStr &_Key, NContainer::TCVector<NStr::CStr> &_Keys);
			uint32 fp_GetAccess(uint32 _Base);
			void fp_ReadRegKey(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue, DWORD *_pDword, NStr::CStr *_pStr, NContainer::CByteVector *_pMemory, NContainer::TCVector<NStr::CStr> *_pMulti);
			NStr::CStr fp_GetPath(const NStr::CStr &_SubKey);
			void fp_WriteRegKey(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue, const DWORD *_pDword, const NStr::CStr *_pStr, const NContainer::CByteVector *_pMemory, const NContainer::TCVector<NStr::CStr> *_pMulti);

		public:

			CWin32_Registry(ERegRoot _Root = ERegRoot_LocalMachine, const NStr::CStr &_RootPath = NStr::CStr());
			CWin32_Registry(CWin32_Registry const& _Root, const NStr::CStr &_RootPath = NStr::CStr());

			template <typename tf_CReturnType>
			tf_CReturnType f_Read(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue);

			template <>
			uint32 f_Read<uint32>(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue)
			{
				DWORD Temp = 0;

				fp_ReadRegKey(_SubKey, _KeyValue, &Temp, nullptr, nullptr, nullptr);

				return Temp;
			}

			template <>
			NStr::CStr f_Read<NStr::CStr>(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue)
			{
				NStr::CStr Temp;

				fp_ReadRegKey(_SubKey, _KeyValue, nullptr, &Temp, nullptr, nullptr);

				return Temp;
			}

			template <>
			NContainer::CByteVector f_Read<NContainer::CByteVector >(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue)
			{
				NContainer::CByteVector Temp;

				fp_ReadRegKey(_SubKey, _KeyValue, nullptr, nullptr, &Temp, nullptr);

				return Temp;
			}

			template <>
			NContainer::TCVector<NStr::CStr> f_Read<NContainer::TCVector<NStr::CStr> >(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue)
			{
				NContainer::TCVector<NStr::CStr> Temp;

				fp_ReadRegKey(_SubKey, _KeyValue, nullptr, nullptr, nullptr, &Temp);

				return Temp;
			}

			bool f_KeyExists(const NStr::CStr &_SubKey);
			bool f_ValueExists(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue);

			uint32 f_Read_uint32(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue);
			NStr::CStr f_Read_Str(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue);
			NContainer::TCVector<NStr::CStr> f_Read_StrMulti(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue);
			NContainer::CByteVector f_Read_Bin(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue);

			uint32 f_Read_uint32(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue, uint32 _Default);
			NStr::CStr f_Read_Str(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue, const NStr::CStr &_Default);
			NContainer::TCVector<NStr::CStr> f_Read_StrMulti(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue, const NContainer::TCVector<NStr::CStr> &_Default);
			NContainer::CByteVector f_Read_Bin(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue, const NContainer::CByteVector &_Default);

			bool f_IsBinary(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue);

			void f_DeleteValue(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue);
			void f_DeleteKey(const NStr::CStr &_SubKey);

			void f_Write(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue, uint32 _Value);
			void f_Write(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue, const NStr::CStr &_Value);
			void f_Write(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue, const NContainer::CByteVector &_Data);
			void f_Write(const NStr::CStr &_SubKey, const NStr::CStr &_KeyValue, const NContainer::TCVector<NStr::CStr> &_Multi);

			void f_EnumValues(const NStr::CStr &_SubKey, NContainer::TCVector<NStr::CStr> &_Values);

			void f_EnumKeys(const NStr::CStr &_SubKey, NContainer::TCVector<NStr::CStr> &_Keys);


		};
	}
}

#endif
