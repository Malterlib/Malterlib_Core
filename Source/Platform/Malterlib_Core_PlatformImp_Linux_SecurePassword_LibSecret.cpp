// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include "Malterlib_Core_PlatformImp_Linux_SecurePassword.h"
#include <Mib/Core/DynamicLibrary>

#include <libsecret/secret.h>

namespace NMib::NSys
{
	//
	// GNOME Secure Passwords
	//

	struct CGNOMEKeychainLibrary : public NMib::CDynamicLibraryUtility
	{
		constexpr CGNOMEKeychainLibrary()
			: NMib::CDynamicLibraryUtility(NMib::NStr::gc_Str<"libsecret-1.so.0">, EDLFlag_NoThrow)
		{
		}

		decltype(&::secret_password_store_sync) secret_password_store_sync = nullptr;
		decltype(&::secret_password_lookup_sync) secret_password_lookup_sync = nullptr;
		decltype(&::secret_password_clear_sync) secret_password_clear_sync = nullptr;
		decltype(&::secret_password_free) secret_password_free = nullptr;
		decltype(&::secret_service_get_sync) secret_service_get_sync = nullptr;
		decltype(&::secret_service_get_collections) secret_service_get_collections = nullptr;
		decltype(&::secret_service_ensure_session_sync) secret_service_ensure_session_sync = nullptr;
		decltype(&::secret_collection_get_label) secret_collection_get_label = nullptr;
		decltype(&::secret_collection_get_locked) secret_collection_get_locked = nullptr;

	protected:
		void fp_ClearSymbols() override
		{
			secret_password_store_sync = nullptr;
			secret_password_lookup_sync = nullptr;
			secret_password_clear_sync = nullptr;
			secret_password_free = nullptr;
			secret_service_get_sync = nullptr;
			secret_service_get_collections = nullptr;
			secret_service_ensure_session_sync = nullptr;
			secret_collection_get_label = nullptr;
			secret_collection_get_locked = nullptr;
		}

		void fp_FetchSymbols() override
		{
			fp_Fetch(secret_password_store_sync, "secret_password_store_sync");
			fp_Fetch(secret_password_lookup_sync, "secret_password_lookup_sync");
			fp_Fetch(secret_password_clear_sync, "secret_password_clear_sync");
			fp_Fetch(secret_password_free, "secret_password_free");
			fp_Fetch(secret_service_get_sync, "secret_service_get_sync");
			fp_Fetch(secret_service_get_collections, "secret_service_get_collections");
			fp_Fetch(secret_service_ensure_session_sync, "secret_service_ensure_session_sync");
			fp_Fetch(secret_collection_get_label, "secret_collection_get_label");
			fp_Fetch(secret_collection_get_locked, "secret_collection_get_locked");
		}
	};

	struct CGnomeLib : public NMib::CDynamicLibraryUtility
	{
		constexpr CGnomeLib()
			: NMib::CDynamicLibraryUtility(NMib::NStr::gc_Str<"libglib-2.0.so.0">, EDLFlag_NoThrow)
		{
		}

		decltype(&::g_list_free_full) g_list_free_full = nullptr;
		decltype(&::g_free) g_free = nullptr;

	protected:
		void fp_ClearSymbols() override
		{
			g_list_free_full = nullptr;
			g_free = nullptr;
		}

		void fp_FetchSymbols() override
		{
			fp_Fetch(g_list_free_full, "g_list_free_full");
			fp_Fetch(g_free, "g_free");
		}
	};

	struct CGnomeObjectLib : public NMib::CDynamicLibraryUtility
	{
		constexpr CGnomeObjectLib()
			: NMib::CDynamicLibraryUtility(NMib::NStr::gc_Str<"libgobject-2.0.so.0">, EDLFlag_NoThrow)
		{
		}

		decltype(&::g_object_unref) g_object_unref = nullptr;

	protected:
		void fp_ClearSymbols() override
		{
			g_object_unref = nullptr;
		}

		void fp_FetchSymbols() override
		{
			fp_Fetch(g_object_unref, "g_object_unref");
		}
	};

	struct CLibSecretPasswordManager : public CLinuxPasswordManager
	{
	private:
		CGnomeLib mp_GnomeLibrary;
		CGnomeObjectLib mp_GnomeObjectLibrary;
		CGNOMEKeychainLibrary mp_KeychainLibrary;
		NMib::NStr::CStr mp_Location;
		mutable NThread::CMutual mp_Lock;

	public:

		CLibSecretPasswordManager();
		~CLibSecretPasswordManager();

		bool f_OK() const override;

		bool f_SecurePassword_IsLocked() override;
		ESecurePassword f_SecurePassword_SetLocation(NMib::NStr::CStr const& _Location) override;
		ESecurePassword f_SecurePassword_Store(NMib::NStr::CStr const& _Key, NMib::NStr::CStrSecure const& _Password) override;
		ESecurePassword f_SecurePassword_Remove(NMib::NStr::CStr const& _Key) override;
		ESecurePassword f_SecurePassword_Get(NMib::NStr::CStr const& _Key, NMib::NStr::CStrSecure& _oPassword) override;
		ESecurePassword f_SecurePassword_Exists(NMib::NStr::CStr const& _Key) override;
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
		mp_GnomeLibrary.f_Reload();
		mp_GnomeObjectLibrary.f_Reload();
	}

	CLibSecretPasswordManager::~CLibSecretPasswordManager()
	{
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

	bool CLibSecretPasswordManager::f_OK() const
	{
#ifdef DMibSanitizerEnabled_Thread
		return false;
#endif
		DMibLock(mp_Lock);

		if (!mp_KeychainLibrary.f_OK())
			return false;

		if (!mp_GnomeLibrary.f_OK())
			return false;

		if (!mp_GnomeObjectLibrary.f_OK())
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

	ESecurePassword CLibSecretPasswordManager::f_SecurePassword_SetLocation(NMib::NStr::CStr const& _Location)
	{
		DMibLock(mp_Lock);

		mp_Location = _Location;
		return ESecurePassword_OK;
	}

	bool CLibSecretPasswordManager::f_SecurePassword_IsLocked()
	{
		DMibLock(mp_Lock);

		GError *pResult = nullptr;

		auto *pService = mp_KeychainLibrary.secret_service_get_sync
			(
				SECRET_SERVICE_OPEN_SESSION | SECRET_SERVICE_LOAD_COLLECTIONS
				, nullptr
				, &pResult
			)
		;

		if (!pService)
			return true;

		auto CleanupService = g_OnScopeExit / [&]
			{
				mp_GnomeObjectLibrary.g_object_unref(pService);
			}
		;

		bool bSuccess = mp_KeychainLibrary.secret_service_ensure_session_sync(pService, nullptr, &pResult);

		if (!bSuccess)
			return true;

		auto *pCollections = mp_KeychainLibrary.secret_service_get_collections(pService);
		if (!pCollections)
			return true;

		auto CleanupCollections = g_OnScopeExit / [&]
			{
				mp_GnomeLibrary.g_list_free_full(pCollections, mp_GnomeObjectLibrary.g_object_unref);
			}
		;

		bool bAnyUnlocked = false;

		for(auto pListIter = pCollections; pListIter; pListIter = pListIter->next)
		{
			SecretCollection *pSecetService = fg_AutoStaticCast(pListIter->data);
			auto pLabel = mp_KeychainLibrary.secret_collection_get_label(pSecetService);
			if (!pLabel)
				continue;
			auto Cleanup = g_OnScopeExit / [&]
				{
					mp_GnomeLibrary.g_free(pLabel);
				}
			;
			if (!NMib::NStr::CStr(pLabel))
				continue;
			bool bLocked = mp_KeychainLibrary.secret_collection_get_locked(pSecetService);
			if (!bLocked)
				bAnyUnlocked = true;
		}

		return !bAnyUnlocked;
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
