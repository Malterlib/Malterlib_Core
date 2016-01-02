// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_PlatformImp_Linux_SecurePassword.h"

#include <Mib/Core/DynamicLibrary>
#include <Mib/Desktop/DBus>


namespace NMib
{
	namespace NSys
	{	

		NPtr::TCUniquePointer<CLinuxPasswordManager> fg_CreateGNOMEPasswordManager();
		NPtr::TCUniquePointer<CLinuxPasswordManager> fg_CreateKWalletPasswordManager(NDBus::CSystem* _pDBus);

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

		NPtr::TCUniquePointer<CLinuxPasswordManager> fg_CreateLinuxPasswordManager(NDBus::CSystem* _pDBus)
		{
			NMib::NSys::EDesktopEnvironment Desktop = NMib::NSys::fg_DesktopEnvironment_Get();

			NPtr::TCUniquePointer<CLinuxPasswordManager> pManager;

			switch(Desktop)
			{
				case EDesktopEnvironment_KDE3:
					// OK with KWallet?
				case EDesktopEnvironment_KDE4:
					pManager = fg_CreateKWalletPasswordManager(_pDBus);
					break;

				case EDesktopEnvironment_GNOME:
				case EDesktopEnvironment_Unity:
				case EDesktopEnvironment_XCFE:
					pManager = fg_CreateGNOMEPasswordManager();
					break;

				default:
					{
						pManager = fg_CreateGNOMEPasswordManager();
						if (!pManager)
							pManager = fg_CreateKWalletPasswordManager(_pDBus);
					}
			}

			if (!pManager)
				pManager = fg_Construct<CNullPasswordManager>();
			
			return pManager;
		}

	} // Namespace NSys

} // Namespace NMib

