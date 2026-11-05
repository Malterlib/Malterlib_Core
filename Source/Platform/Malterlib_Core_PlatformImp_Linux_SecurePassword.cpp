// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_PlatformImp_Linux_SecurePassword.h"

#include <Mib/Core/DynamicLibrary>
#include <Mib/Desktop/DBus>

namespace NMib
{
	namespace NSys
	{
		CLinuxPasswordManager::~CLinuxPasswordManager()
		{
		}

		bool CLinuxPasswordManager::f_SecurePassword_Supported()
		{
			return true;
		}

		NStorage::TCUniquePointer<CLinuxPasswordManager> fg_CreateLibSecretPasswordManager();
		NStorage::TCUniquePointer<CLinuxPasswordManager> fg_CreateKWalletPasswordManager(NDBus::CSystem *_pDBus);

		struct CNullPasswordManager : public CLinuxPasswordManager
		{
			CNullPasswordManager();
			~CNullPasswordManager();

			bool f_OK() const override;
			bool f_SecurePassword_IsLocked() override;
			ESecurePassword f_SecurePassword_SetLocation(NMib::NStr::CStr const &_Location) override;
			ESecurePassword f_SecurePassword_Store(NMib::NStr::CStr const &_Key, NMib::NStr::CStrSecure const &_Password) override;
			ESecurePassword f_SecurePassword_Remove(NMib::NStr::CStr const &_Key) override;
			ESecurePassword f_SecurePassword_Get(NMib::NStr::CStr const &_Key, NMib::NStr::CStrSecure &o_Password) override;
			ESecurePassword f_SecurePassword_Exists(NMib::NStr::CStr const &_Key) override;
			bool f_SecurePassword_Supported() override;
		};

		CNullPasswordManager::CNullPasswordManager()
		{
		}

		CNullPasswordManager::~CNullPasswordManager()
		{
		}

		bool CNullPasswordManager::f_OK() const
		{
			return true;
		}

		bool CNullPasswordManager::f_SecurePassword_IsLocked()
		{
			return true;
		}

		ESecurePassword CNullPasswordManager::f_SecurePassword_SetLocation(NMib::NStr::CStr const &_Location)
		{
			return ESecurePassword_Failure;
		}

		ESecurePassword CNullPasswordManager::f_SecurePassword_Store(NMib::NStr::CStr const &_Key, NMib::NStr::CStrSecure const &_Password)
		{
			return ESecurePassword_Failure;
		}

		ESecurePassword CNullPasswordManager::f_SecurePassword_Remove(NMib::NStr::CStr const &_Key)
		{
			return ESecurePassword_Failure;
		}

		ESecurePassword CNullPasswordManager::f_SecurePassword_Get(NMib::NStr::CStr const &_Key, NMib::NStr::CStrSecure &o_Password)
		{
			return ESecurePassword_Failure;
		}

		ESecurePassword CNullPasswordManager::f_SecurePassword_Exists(NMib::NStr::CStr const &_Key)
		{
			return ESecurePassword_Failure;
		}

		bool CNullPasswordManager::f_SecurePassword_Supported()
		{
			return false;
		}

		NStorage::TCUniquePointer<CLinuxPasswordManager> fg_CreateLinuxPasswordManager(NDBus::CSystem *_pDBus)
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

