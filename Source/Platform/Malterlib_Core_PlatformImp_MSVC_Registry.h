// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#ifdef DPlatformFamily_Windows
#include <windows.h>

namespace NMib
{

	namespace NRuntimeMSVC
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
			NMib::NStr::CStr mp_Root;
			void fp_DeleteKey(const NMib::NStr::CStr &_Key);
			void fp_EnumValues(const NMib::NStr::CStr &_Key, NMib::NContainer::TCVector<NMib::NStr::CStr> &_Values);
			void fp_EnumKeys(const NMib::NStr::CStr &_Key, NMib::NContainer::TCVector<NMib::NStr::CStr> &_Keys);
			uint32 fp_GetAccess(uint32 _Base);
			void fp_ReadRegKey(const NMib::NStr::CStr &_SubKey, const NMib::NStr::CStr &_KeyValue, DWORD *_pDword, NMib::NStr::CStr *_pStr, NMib::NContainer::TCVector<uint8> *_pMemory, NMib::NContainer::TCVector<NMib::NStr::CStr> *_pMulti);
			NMib::NStr::CStr fp_GetPath(const NMib::NStr::CStr &_SubKey);
			void fp_WriteRegKey(const NMib::NStr::CStr &_SubKey, const NMib::NStr::CStr &_KeyValue, const DWORD *_pDword, const NMib::NStr::CStr *_pStr, const NMib::NContainer::TCVector<uint8> *_pMemory, const NMib::NContainer::TCVector<NMib::NStr::CStr> *_pMulti);	

		public:
	
			CWin32_Registry(ERegRoot _Root = ERegRoot_LocalMachine, const NMib::NStr::CStr &_RootPath = NMib::NStr::CStr());
			CWin32_Registry(CWin32_Registry const& _Root, const NMib::NStr::CStr &_RootPath = NMib::NStr::CStr());

			template <typename tf_CReturnType>
			tf_CReturnType f_Read(const NMib::NStr::CStr &_SubKey, const NMib::NStr::CStr &_KeyValue);

			template <>
			uint32 f_Read<uint32>(const NMib::NStr::CStr &_SubKey, const NMib::NStr::CStr &_KeyValue)
			{
				DWORD Temp = 0;
		
				fp_ReadRegKey(_SubKey, _KeyValue, &Temp, nullptr, nullptr, nullptr);
		
				return Temp;
			}

			template <>
			NMib::NStr::CStr f_Read<NMib::NStr::CStr>(const NMib::NStr::CStr &_SubKey, const NMib::NStr::CStr &_KeyValue)
			{
				NMib::NStr::CStr Temp;
		
				fp_ReadRegKey(_SubKey, _KeyValue, nullptr, &Temp, nullptr, nullptr);
		
				return Temp;
			}

			template <>
			NMib::NContainer::TCVector<uint8> f_Read<NMib::NContainer::TCVector<uint8> >(const NMib::NStr::CStr &_SubKey, const NMib::NStr::CStr &_KeyValue)
			{
				NMib::NContainer::TCVector<uint8> Temp;
		
				fp_ReadRegKey(_SubKey, _KeyValue, nullptr, nullptr, &Temp, nullptr);
		
				return Temp;
			}

			template <>
			NMib::NContainer::TCVector<NMib::NStr::CStr> f_Read<NMib::NContainer::TCVector<NMib::NStr::CStr> >(const NMib::NStr::CStr &_SubKey, const NMib::NStr::CStr &_KeyValue)
			{
				NMib::NContainer::TCVector<NMib::NStr::CStr> Temp;
		
				fp_ReadRegKey(_SubKey, _KeyValue, nullptr, nullptr, nullptr, &Temp);
		
				return Temp;
			}

			bint f_KeyExists(const NMib::NStr::CStr &_SubKey);
			bint f_ValueExists(const NMib::NStr::CStr &_SubKey, const NMib::NStr::CStr &_KeyValue);

			uint32 f_Read_uint32(const NMib::NStr::CStr &_SubKey, const NMib::NStr::CStr &_KeyValue);
			NMib::NStr::CStr f_Read_Str(const NMib::NStr::CStr &_SubKey, const NMib::NStr::CStr &_KeyValue);
			NMib::NContainer::TCVector<NMib::NStr::CStr> f_Read_StrMulti(const NMib::NStr::CStr &_SubKey, const NMib::NStr::CStr &_KeyValue);
			NMib::NContainer::TCVector<uint8> f_Read_Bin(const NMib::NStr::CStr &_SubKey, const NMib::NStr::CStr &_KeyValue);
	
			uint32 f_Read_uint32(const NMib::NStr::CStr &_SubKey, const NMib::NStr::CStr &_KeyValue, uint32 _Default);
			NMib::NStr::CStr f_Read_Str(const NMib::NStr::CStr &_SubKey, const NMib::NStr::CStr &_KeyValue, const NMib::NStr::CStr &_Default);
			NMib::NContainer::TCVector<NMib::NStr::CStr> f_Read_StrMulti(const NMib::NStr::CStr &_SubKey, const NMib::NStr::CStr &_KeyValue, const NMib::NContainer::TCVector<NMib::NStr::CStr> &_Default);
			NMib::NContainer::TCVector<uint8> f_Read_Bin(const NMib::NStr::CStr &_SubKey, const NMib::NStr::CStr &_KeyValue, const NMib::NContainer::TCVector<uint8> &_Default);
	
			bint f_IsBinary(const NMib::NStr::CStr &_SubKey, const NMib::NStr::CStr &_KeyValue);

			void f_DeleteValue(const NMib::NStr::CStr &_SubKey, const NMib::NStr::CStr &_KeyValue);
			void f_DeleteKey(const NMib::NStr::CStr &_SubKey);

			void f_Write(const NMib::NStr::CStr &_SubKey, const NMib::NStr::CStr &_KeyValue, uint32 _Value);
			void f_Write(const NMib::NStr::CStr &_SubKey, const NMib::NStr::CStr &_KeyValue, const NMib::NStr::CStr &_Value);
			void f_Write(const NMib::NStr::CStr &_SubKey, const NMib::NStr::CStr &_KeyValue, const NMib::NContainer::TCVector<uint8> &_Data);
			void f_Write(const NMib::NStr::CStr &_SubKey, const NMib::NStr::CStr &_KeyValue, const NMib::NContainer::TCVector<NMib::NStr::CStr> &_Multi);
	
			void f_EnumValues(const NMib::NStr::CStr &_SubKey, NMib::NContainer::TCVector<NMib::NStr::CStr> &_Values);

			void f_EnumKeys(const NMib::NStr::CStr &_SubKey, NMib::NContainer::TCVector<NMib::NStr::CStr> &_Keys);


		};

	} // Namespace NRuntimeMSVC

} // Namespace NMib

#endif // DPlatformFamily_Windows
