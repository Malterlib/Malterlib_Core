// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_PlatformImp_Linux_SecurePassword.h"

#include <Mib/Core/DynamicLibrary>
#include <Mib/Desktop/DBus>


namespace NMib
{
	namespace NSys
	{

		NStorage::TCUniquePointer<CLinuxPasswordManager> fg_CreateLibSecretPasswordManager();
		NStorage::TCUniquePointer<CLinuxPasswordManager> fg_CreateKWalletPasswordManager(NDBus::CSystem* _pDBus);

		struct CNullPasswordManager : public CLinuxPasswordManager
		{
			CNullPasswordManager()
			{}
			~CNullPasswordManager()
			{}

			bool f_OK() const override
			{
				return true;
			}

			bool f_SecurePassword_IsLocked() override
			{
				return true;
			}

			ESecurePassword f_SecurePassword_SetLocation(NMib::NStr::CStr const& _Location) override
			{
				return ESecurePassword_Failure;
			}

			ESecurePassword f_SecurePassword_Store(NMib::NStr::CStr const& _Key, NMib::NStr::CStrSecure const& _Password) override
			{
				return ESecurePassword_Failure;
			}

			ESecurePassword f_SecurePassword_Remove(NMib::NStr::CStr const& _Key) override
			{
				return ESecurePassword_Failure;
			}

			ESecurePassword f_SecurePassword_Get(NMib::NStr::CStr const& _Key, NMib::NStr::CStrSecure& _oPassword) override
			{
				return ESecurePassword_Failure;
			}

			ESecurePassword f_SecurePassword_Exists(NMib::NStr::CStr const& _Key) override
			{
				return ESecurePassword_Failure;
			}

			bool f_SecurePassword_Supported() override
			{
				return false;
			}

		};

		NStorage::TCUniquePointer<CLinuxPasswordManager> fg_CreateLinuxPasswordManager(NDBus::CSystem* _pDBus)
		{
			NStorage::TCUniquePointer<CLinuxPasswordManager> pManager;

			pManager = fg_CreateLibSecretPasswordManager();

			if (!pManager)
				pManager = fg_CreateKWalletPasswordManager(_pDBus);

			if (!pManager)
				pManager = fg_Construct<CNullPasswordManager>();

			return pManager;
		}

	} // Namespace NSys

} // Namespace NMib

