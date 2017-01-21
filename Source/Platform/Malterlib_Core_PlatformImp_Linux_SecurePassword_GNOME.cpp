// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include "Malterlib_Core_PlatformImp_Linux_SecurePassword.h"
#include <Mib/Core/DynamicLibrary>

#include <gnome-keyring-1/gnome-keyring.h>

namespace NMib
{
	namespace NSys
	{	

		//
		// GNOME Secure Passwords
		//

		DMibDefineDynamicLibraryClass(CGNOMEKeychainLibrary, EDLFlag_NoThrow | EDLFlag_NoAutoLoad, "libgnome-keyring.so.0"
										,	gnome_keyring_store_password_sync
										,	gnome_keyring_find_password_sync
										,	gnome_keyring_delete_password_sync
										,	gnome_keyring_free_password
									);


		struct CGNOMEPasswordManager : public CLinuxPasswordManager
		{
		private:

			CGNOMEKeychainLibrary mp_KeychainLibrary;
			NMib::NStr::CStr mp_Location;

		public:

			CGNOMEPasswordManager();
			~CGNOMEPasswordManager();

			bool f_OK() const;

			ESecurePassword f_SecurePassword_SetLocation(NMib::NStr::CStr const& _Location);
			ESecurePassword f_SecurePassword_Store(NMib::NStr::CStr const& _Key, NMib::NStr::CStrSecure const& _Password);
			ESecurePassword f_SecurePassword_Remove(NMib::NStr::CStr const& _Key);
			ESecurePassword f_SecurePassword_Get(NMib::NStr::CStr const& _Key, NMib::NStr::CStrSecure& _oPassword);
			ESecurePassword f_SecurePassword_Exists(NMib::NStr::CStr const& _Key);
		};

		NPtr::TCUniquePointer<CLinuxPasswordManager> fg_CreateGNOMEPasswordManager()
		{
			NPtr::TCUniquePointer<CGNOMEPasswordManager> pManager = fg_Construct();

			if (!pManager->f_OK())
				return nullptr;

			return fg_Move(pManager);
		}

		//
		// GNOME Secure Password Implementation
		//

		static GnomeKeyringPasswordSchema const gc_HansoftSchema = 
			{
				GNOME_KEYRING_ITEM_GENERIC_SECRET,
				{
					{ "key", 		GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
					{ NULL, GNOME_KEYRING_ATTRIBUTE_TYPE_STRING }
				}
			};

		CGNOMEPasswordManager::CGNOMEPasswordManager()
		{
			mp_KeychainLibrary.f_Reload();
		}

		CGNOMEPasswordManager::~CGNOMEPasswordManager()
		{

		}

		bool CGNOMEPasswordManager::f_OK() const
		{
			if (!mp_KeychainLibrary.f_OK())
				return false;
			
			if (fg_GetSys()->f_GetEnvironmentVariable("GNOME_KEYRING_CONTROL").f_IsEmpty())
				return false;
			return true;
		}


		static ESecurePassword fg_SecurePassword_Decode_GNOMEResult(GnomeKeyringResult _Result)
		{
			switch(_Result)
			{
				case GNOME_KEYRING_RESULT_OK:
					return ESecurePassword_OK;

				case GNOME_KEYRING_RESULT_DENIED:
					return ESecurePassword_AuthFailed;

				case GNOME_KEYRING_RESULT_NO_KEYRING_DAEMON:
				case GNOME_KEYRING_RESULT_ALREADY_UNLOCKED:
				case GNOME_KEYRING_RESULT_NO_SUCH_KEYRING:
				case GNOME_KEYRING_RESULT_BAD_ARGUMENTS:
				case GNOME_KEYRING_RESULT_IO_ERROR:
				case GNOME_KEYRING_RESULT_CANCELLED:
				case GNOME_KEYRING_RESULT_KEYRING_ALREADY_EXISTS:
					return ESecurePassword_Failure;

				case GNOME_KEYRING_RESULT_NO_MATCH:
					return ESecurePassword_NotFound;

				default:
					return ESecurePassword_Failure;
			}			
		}		


		static char const* fg_SecurePassword_Decode_GNOMEResult_ToString(GnomeKeyringResult _Result)
		{
			switch(_Result)
			{
				case GNOME_KEYRING_RESULT_OK:
					return "GNOME_KEYRING_RESULT_OK";

				case GNOME_KEYRING_RESULT_DENIED:
					return "GNOME_KEYRING_RESULT_DENIED";

				case GNOME_KEYRING_RESULT_NO_KEYRING_DAEMON:
					return "GNOME_KEYRING_RESULT_NO_KEYRING_DAEMON";

				case GNOME_KEYRING_RESULT_ALREADY_UNLOCKED:
					return "GNOME_KEYRING_RESULT_ALREADY_UNLOCKED";

				case GNOME_KEYRING_RESULT_NO_SUCH_KEYRING:
					return "GNOME_KEYRING_RESULT_NO_SUCH_KEYRING";

				case GNOME_KEYRING_RESULT_BAD_ARGUMENTS:
					return "GNOME_KEYRING_RESULT_BAD_ARGUMENTS";

				case GNOME_KEYRING_RESULT_IO_ERROR:
					return "GNOME_KEYRING_RESULT_IO_ERROR";

				case GNOME_KEYRING_RESULT_CANCELLED:
					return "GNOME_KEYRING_RESULT_CANCELLED";

				case GNOME_KEYRING_RESULT_KEYRING_ALREADY_EXISTS:
					return "GNOME_KEYRING_RESULT_KEYRING_ALREADY_EXISTS";

				case GNOME_KEYRING_RESULT_NO_MATCH:
					return "GNOME_KEYRING_RESULT_NO_MATCH";

				default:
					return "Unknown";
			}			
		}	

		static void fg_SecurePassword_GNOME_Log(char const* _pFunc, GnomeKeyringResult _Result)
		{
			(void)fg_SecurePassword_Decode_GNOMEResult_ToString;
/*
			if (_Result == GNOME_KEYRING_RESULT_OK)
				DMibLog(Info, "GNOMEKeychain: {} = {}", _pFunc, fg_SecurePassword_Decode_GNOMEResult_ToString(_Result));
			else
				DMibLog(Error, "GNOMEKeychain: {} = {}", _pFunc, fg_SecurePassword_Decode_GNOMEResult_ToString(_Result));
*/
			if (	_Result != GNOME_KEYRING_RESULT_OK
				&&	_Result != GNOME_KEYRING_RESULT_NO_MATCH)
				DMibLog(Error, "GNOMEKeychain: {} = {}", _pFunc, fg_SecurePassword_Decode_GNOMEResult_ToString(_Result));
		}



		ESecurePassword CGNOMEPasswordManager::f_SecurePassword_SetLocation(NMib::NStr::CStr const& _Location)
		{
			mp_Location = _Location;
			return ESecurePassword_OK;
		}

		ESecurePassword CGNOMEPasswordManager::f_SecurePassword_Store(NMib::NStr::CStr const& _Key, NMib::NStr::CStrSecure const& _Password)
		{
			GnomeKeyringResult Result 
				= mp_KeychainLibrary.gnome_keyring_store_password_sync
				(
					&gc_HansoftSchema
					, nullptr					// Default keychain
					, mp_Location.f_GetStr()
					, _Password.f_GetStr()
					, "key"
					, _Key.f_GetStr()
					, NULL
				);

			fg_SecurePassword_GNOME_Log("gnome_keyring_store_password_sync", Result);

			return fg_SecurePassword_Decode_GNOMEResult(Result);
		}

		ESecurePassword CGNOMEPasswordManager::f_SecurePassword_Remove(NMib::NStr::CStr const& _Key)
		{
			GnomeKeyringResult Result 
				= mp_KeychainLibrary.gnome_keyring_delete_password_sync(
						&gc_HansoftSchema
					,	"key", _Key.f_GetStr()
					, 	NULL
				);

			fg_SecurePassword_GNOME_Log("gnome_keyring_delete_password_sync", Result);

			return fg_SecurePassword_Decode_GNOMEResult(Result);
		}

		ESecurePassword CGNOMEPasswordManager::f_SecurePassword_Get(NMib::NStr::CStr const& _Key, NMib::NStr::CStrSecure& _oPassword)
		{
			gchar* pPassword = nullptr;

			GnomeKeyringResult Result 
				= mp_KeychainLibrary.gnome_keyring_find_password_sync(
						&gc_HansoftSchema
					,	&pPassword
					,	"key", _Key.f_GetStr()
					,	NULL
				);

			if (Result == GNOME_KEYRING_RESULT_OK)
			{
				_oPassword = pPassword;
				mp_KeychainLibrary.gnome_keyring_free_password(pPassword);
			}

			fg_SecurePassword_GNOME_Log("gnome_keyring_find_password_sync", Result);

			return fg_SecurePassword_Decode_GNOMEResult(Result);
		}

		ESecurePassword CGNOMEPasswordManager::f_SecurePassword_Exists(NMib::NStr::CStr const& _Key)
		{
			gchar* pPassword = nullptr;

			GnomeKeyringResult Result 
				= mp_KeychainLibrary.gnome_keyring_find_password_sync(
						&gc_HansoftSchema
					,	&pPassword
					,	"key", _Key.f_GetStr()
					,	NULL
				);

			if (Result == GNOME_KEYRING_RESULT_OK)
				mp_KeychainLibrary.gnome_keyring_free_password(pPassword);

			fg_SecurePassword_GNOME_Log("gnome_keyring_find_password_sync", Result);

			return fg_SecurePassword_Decode_GNOMEResult(Result);
		}


	} // Namespace NSys

} // Namespace NMib
