// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Desktop/DBus>

namespace NMib
{
	namespace NSys
	{	

		struct CLinuxPasswordManager
		{
			virtual ~CLinuxPasswordManager() {}

			virtual bool f_OK() const = 0;

			virtual ESecurePassword f_SecurePassword_SetLocation(NMib::NStr::CStr const& _Location) = 0;
			virtual ESecurePassword f_SecurePassword_Store(NMib::NStr::CStr const& _Key, NMib::NStr::CStrSecure const& _Password) = 0;
			virtual ESecurePassword f_SecurePassword_Remove(NMib::NStr::CStr const& _Key) = 0;
			virtual ESecurePassword f_SecurePassword_Get(NMib::NStr::CStr const& _Key, NMib::NStr::CStrSecure& _oPassword) = 0;
			virtual ESecurePassword f_SecurePassword_Exists(NMib::NStr::CStr const& _Key) = 0;
			
			virtual bool f_SecurePassword_Supported()
			{
				return true;
			}
		};

		NStorage::TCUniquePointer<CLinuxPasswordManager> fg_CreateLinuxPasswordManager(NDBus::CSystem* _pDBus);

	} // Namespace NSys

} // Namespace NMib
