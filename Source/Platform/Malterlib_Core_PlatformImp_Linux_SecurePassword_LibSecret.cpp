// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include "Malterlib_Core_PlatformImp_Linux_SecurePassword.h"
#include <Mib/Core/DynamicLibrary>

#include <libsecret/secret.h>
 
namespace NMib::NSys
{
	//
	// GNOME Secure Passwords
	//

	DMibDefineDynamicLibraryClass
		(
			CGNOMEKeychainLibrary
			, EDLFlag_NoThrow | EDLFlag_NoAutoLoad
			, "libsecret-1.so.0"
			, secret_password_store_sync
			, secret_password_lookup_sync
			, secret_password_clear_sync
			, secret_password_free
		)
	;

	struct CLibSecretPasswordManager : public CLinuxPasswordManager
	{
	private:
		CGNOMEKeychainLibrary mp_KeychainLibrary;
		NMib::NStr::CStr mp_Location;
		mutable NThread::CMutual mp_Lock;

	public:

		CLibSecretPasswordManager();
		~CLibSecretPasswordManager();

		bool f_OK() const;

		ESecurePassword f_SecurePassword_SetLocation(NMib::NStr::CStr const& _Location);
		ESecurePassword f_SecurePassword_Store(NMib::NStr::CStr const& _Key, NMib::NStr::CStrSecure const& _Password);
		ESecurePassword f_SecurePassword_Remove(NMib::NStr::CStr const& _Key);
		ESecurePassword f_SecurePassword_Get(NMib::NStr::CStr const& _Key, NMib::NStr::CStrSecure& _oPassword);
		ESecurePassword f_SecurePassword_Exists(NMib::NStr::CStr const& _Key);
	};

	NStorage::TCUniquePointer<CLinuxPasswordManager> fg_CreateLibSecretPasswordManager()
	{
		NStorage::TCUniquePointer<CLibSecretPasswordManager> pManager = fg_Construct();

		if (!pManager->f_OK())
			return nullptr;

		return fg_Move(pManager);
	}

	//
	// GNOME Secure Password Implementation
	//

	static SecretSchema const gc_SecretSchema =
		{
			"org.malterlib.Password"
			, SECRET_SCHEMA_NONE
			,
			{
				{ "key", SECRET_SCHEMA_ATTRIBUTE_STRING }
				, { "NULL", (SecretSchemaAttributeType)0 }
			}
		}
	;

	CLibSecretPasswordManager::CLibSecretPasswordManager()
	{
		mp_KeychainLibrary.f_Reload();
	}

	CLibSecretPasswordManager::~CLibSecretPasswordManager()
	{
	}

	bool CLibSecretPasswordManager::f_OK() const
	{
#ifdef DMibSanitizerEnabled_Thread
		return false;
#endif
		DMibLock(mp_Lock);

		if (!mp_KeychainLibrary.f_OK())
			return false;

		if (fg_GetSys()->f_GetEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS").f_IsEmpty())
			return false;

		return true;
	}

	static ESecurePassword fg_SecurePassword_Decode_Result(bool _bSuccess, GError const *_pResult)
	{
		if (_bSuccess)
			return ESecurePassword_OK;

		return ESecurePassword_Failure;
	}

	[[maybe_unused]] static char const *fg_SecurePassword_Decode_Result_ToString(GError const *_pResult)
	{
		if (!_pResult)
			return "Unknown";

		return _pResult->message;
	}

	static void fg_SecurePassword_GNOME_Log(char const* _pFunc, bool _bSuccess, GError const *_pResult)
	{
		if (_bSuccess)
			return;

		DMibLog(Error, "GNOMEKeychain: {} = {}", _pFunc, fg_SecurePassword_Decode_Result_ToString(_pResult));
	}

	ESecurePassword CLibSecretPasswordManager::f_SecurePassword_SetLocation(NMib::NStr::CStr const& _Location)
	{
		DMibLock(mp_Lock);

		mp_Location = _Location;
		return ESecurePassword_OK;
	}

	ESecurePassword CLibSecretPasswordManager::f_SecurePassword_Store(NMib::NStr::CStr const &_Key, NMib::NStr::CStrSecure const& _Password)
	{
		DMibLock(mp_Lock);

		GError *pResult = nullptr;

		bool bSuccess = mp_KeychainLibrary.secret_password_store_sync
			(
				&gc_SecretSchema
				, SECRET_COLLECTION_DEFAULT
				, mp_Location.f_GetStr()
				, _Password.f_GetStr()
				, nullptr
				, &pResult
				, "key"
				, _Key.f_GetStr()
				, nullptr
			);

		fg_SecurePassword_GNOME_Log("secret_password_store_sync", bSuccess, pResult);

		return fg_SecurePassword_Decode_Result(bSuccess, pResult);
	}

	ESecurePassword CLibSecretPasswordManager::f_SecurePassword_Remove(NMib::NStr::CStr const& _Key)
	{
		DMibLock(mp_Lock);

		GError *pResult = nullptr;

		bool bSuccess = mp_KeychainLibrary.secret_password_clear_sync
			(
				&gc_SecretSchema
				, nullptr
				, &pResult
				, "key"
				, _Key.f_GetStr()
				, nullptr
			)
		;

		if (!bSuccess && !pResult)
			return ESecurePassword_NotFound;

		fg_SecurePassword_GNOME_Log("secret_password_clear_sync", bSuccess, pResult);

		return fg_SecurePassword_Decode_Result(bSuccess, pResult);
	}

	ESecurePassword CLibSecretPasswordManager::f_SecurePassword_Get(NMib::NStr::CStr const& _Key, NMib::NStr::CStrSecure &o_Password)
	{
		DMibLock(mp_Lock);

		GError *pResult = nullptr;

		gchar *pPassword = mp_KeychainLibrary.secret_password_lookup_sync
			(
				&gc_SecretSchema
				, nullptr
				, &pResult
				, "key"
				, _Key.f_GetStr()
				, nullptr
			)
		;

		if (!pPassword && !pResult)
			return ESecurePassword_NotFound;

		if (pPassword)
		{
			o_Password = pPassword;
			mp_KeychainLibrary.secret_password_free(pPassword);
		}

		fg_SecurePassword_GNOME_Log("secret_password_lookup_sync", !!pPassword, pResult);

		return fg_SecurePassword_Decode_Result(!!pPassword, pResult);
	}

	ESecurePassword CLibSecretPasswordManager::f_SecurePassword_Exists(NMib::NStr::CStr const &_Key)
	{
		NMib::NStr::CStrSecure Dummy;

		return f_SecurePassword_Get(_Key, Dummy);
	}
}
