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
		constexpr CGNOMEKeychainLibrary();

		decltype(&::secret_password_store_sync) m_fSecretPasswordStoreSync = nullptr;
		decltype(&::secret_password_lookup_sync) m_fSecretPasswordLookupSync = nullptr;
		decltype(&::secret_password_clear_sync) m_fSecretPasswordClearSync = nullptr;
		decltype(&::secret_password_free) m_fSecretPasswordFree = nullptr;
		decltype(&::secret_service_get_sync) m_fSecretServiceGetSync = nullptr;
		decltype(&::secret_service_get_collections) m_fSecretServiceGetCollections = nullptr;
		decltype(&::secret_service_ensure_session_sync) m_fSecretServiceEnsureSessionSync = nullptr;
		decltype(&::secret_collection_get_label) m_fSecretCollectionGetLabel = nullptr;
		decltype(&::secret_collection_get_locked) m_fSecretCollectionGetLocked = nullptr;

	protected:
		void fp_ClearSymbols() override;
		void fp_FetchSymbols() override;
	};

	constexpr CGNOMEKeychainLibrary::CGNOMEKeychainLibrary()
		: NMib::CDynamicLibraryUtility(NMib::NStr::gc_Str<"libsecret-1.so.0">, EDLFlag_NoThrow)
	{
	}

	void CGNOMEKeychainLibrary::fp_ClearSymbols()
	{
		m_fSecretPasswordStoreSync = nullptr;
		m_fSecretPasswordLookupSync = nullptr;
		m_fSecretPasswordClearSync = nullptr;
		m_fSecretPasswordFree = nullptr;
		m_fSecretServiceGetSync = nullptr;
		m_fSecretServiceGetCollections = nullptr;
		m_fSecretServiceEnsureSessionSync = nullptr;
		m_fSecretCollectionGetLabel = nullptr;
		m_fSecretCollectionGetLocked = nullptr;
	}

	void CGNOMEKeychainLibrary::fp_FetchSymbols()
	{
		fp_Fetch(m_fSecretPasswordStoreSync, "secret_password_store_sync");
		fp_Fetch(m_fSecretPasswordLookupSync, "secret_password_lookup_sync");
		fp_Fetch(m_fSecretPasswordClearSync, "secret_password_clear_sync");
		fp_Fetch(m_fSecretPasswordFree, "secret_password_free");
		fp_Fetch(m_fSecretServiceGetSync, "secret_service_get_sync");
		fp_Fetch(m_fSecretServiceGetCollections, "secret_service_get_collections");
		fp_Fetch(m_fSecretServiceEnsureSessionSync, "secret_service_ensure_session_sync");
		fp_Fetch(m_fSecretCollectionGetLabel, "secret_collection_get_label");
		fp_Fetch(m_fSecretCollectionGetLocked, "secret_collection_get_locked");
	}

	struct CGnomeLib : public NMib::CDynamicLibraryUtility
	{
		constexpr CGnomeLib();

		decltype(&::g_list_free_full) m_fGListFreeFull = nullptr;
		decltype(&::g_free) m_fGFree = nullptr;

	protected:
		void fp_ClearSymbols() override;
		void fp_FetchSymbols() override;
	};

	constexpr CGnomeLib::CGnomeLib()
		: NMib::CDynamicLibraryUtility(NMib::NStr::gc_Str<"libglib-2.0.so.0">, EDLFlag_NoThrow)
	{
	}

	void CGnomeLib::fp_ClearSymbols()
	{
		m_fGListFreeFull = nullptr;
		m_fGFree = nullptr;
	}

	void CGnomeLib::fp_FetchSymbols()
	{
		fp_Fetch(m_fGListFreeFull, "g_list_free_full");
		fp_Fetch(m_fGFree, "g_free");
	}

	struct CGnomeObjectLib : public NMib::CDynamicLibraryUtility
	{
		constexpr CGnomeObjectLib();

		decltype(&::g_object_unref) m_fGObjectUnref = nullptr;

	protected:
		void fp_ClearSymbols() override;
		void fp_FetchSymbols() override;
	};

	constexpr CGnomeObjectLib::CGnomeObjectLib()
		: NMib::CDynamicLibraryUtility(NMib::NStr::gc_Str<"libgobject-2.0.so.0">, EDLFlag_NoThrow)
	{
	}

	void CGnomeObjectLib::fp_ClearSymbols()
	{
		m_fGObjectUnref = nullptr;
	}

	void CGnomeObjectLib::fp_FetchSymbols()
	{
		fp_Fetch(m_fGObjectUnref, "g_object_unref");
	}

	struct CLibSecretPasswordManager : public CLinuxPasswordManager
	{
		CLibSecretPasswordManager();
		~CLibSecretPasswordManager();

		bool f_OK() const override;

		bool f_SecurePassword_IsLocked() override;
		ESecurePassword f_SecurePassword_SetLocation(NMib::NStr::CStr const &_Location) override;
		ESecurePassword f_SecurePassword_Store(NMib::NStr::CStr const &_Key, NMib::NStr::CStrSecure const &_Password) override;
		ESecurePassword f_SecurePassword_Remove(NMib::NStr::CStr const &_Key) override;
		ESecurePassword f_SecurePassword_Get(NMib::NStr::CStr const &_Key, NMib::NStr::CStrSecure &o_Password) override;
		ESecurePassword f_SecurePassword_Exists(NMib::NStr::CStr const &_Key) override;

	private:
		CGnomeLib mp_GnomeLibrary;
		CGnomeObjectLib mp_GnomeObjectLibrary;
		CGNOMEKeychainLibrary mp_KeychainLibrary;
		NMib::NStr::CStr mp_Location;
		mutable NThread::CMutual mp_Lock;
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

	static constexpr SecretSchema gc_SecretSchema =
		{
			"org.malterlib.Password"
			, SECRET_SCHEMA_NONE
			,
			{
				{"key", SECRET_SCHEMA_ATTRIBUTE_STRING}
				, {"NULL", (SecretSchemaAttributeType)0}
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

	static void fg_SecurePassword_GNOME_Log(char const *_pFunc, bool _bSuccess, GError const *_pResult)
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

	ESecurePassword CLibSecretPasswordManager::f_SecurePassword_SetLocation(NMib::NStr::CStr const &_Location)
	{
		DMibLock(mp_Lock);

		mp_Location = _Location;
		return ESecurePassword_OK;
	}

	bool CLibSecretPasswordManager::f_SecurePassword_IsLocked()
	{
		DMibLock(mp_Lock);

		GError *pResult = nullptr;

		auto *pService = mp_KeychainLibrary.m_fSecretServiceGetSync(SECRET_SERVICE_OPEN_SESSION | SECRET_SERVICE_LOAD_COLLECTIONS, nullptr, &pResult);

		if (!pService)
			return true;

		auto CleanupService = g_OnScopeExit / [&]
			{
				mp_GnomeObjectLibrary.m_fGObjectUnref(pService);
			}
		;

		bool bSuccess = mp_KeychainLibrary.m_fSecretServiceEnsureSessionSync(pService, nullptr, &pResult);

		if (!bSuccess)
			return true;

		auto *pCollections = mp_KeychainLibrary.m_fSecretServiceGetCollections(pService);
		if (!pCollections)
			return true;

		auto CleanupCollections = g_OnScopeExit / [&]
			{
				mp_GnomeLibrary.m_fGListFreeFull(pCollections, mp_GnomeObjectLibrary.m_fGObjectUnref);
			}
		;

		bool bAnyUnlocked = false;

		for (auto pListIter = pCollections; pListIter; pListIter = pListIter->next)
		{
			SecretCollection *pSecetService = fg_AutoStaticCast(pListIter->data);
			auto pLabel = mp_KeychainLibrary.m_fSecretCollectionGetLabel(pSecetService);
			if (!pLabel)
				continue;
			auto Cleanup = g_OnScopeExit / [&]
				{
					mp_GnomeLibrary.m_fGFree(pLabel);
				}
			;
			if (!NMib::NStr::CStr(pLabel))
				continue;
			bool bLocked = mp_KeychainLibrary.m_fSecretCollectionGetLocked(pSecetService);
			if (!bLocked)
				bAnyUnlocked = true;
		}

		return !bAnyUnlocked;
	}

	ESecurePassword CLibSecretPasswordManager::f_SecurePassword_Store(NMib::NStr::CStr const &_Key, NMib::NStr::CStrSecure const &_Password)
	{
		DMibLock(mp_Lock);

		GError *pResult = nullptr;

		bool bSuccess = mp_KeychainLibrary.m_fSecretPasswordStoreSync
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
			)
		;

		fg_SecurePassword_GNOME_Log("secret_password_store_sync", bSuccess, pResult);

		return fg_SecurePassword_Decode_Result(bSuccess, pResult);
	}

	ESecurePassword CLibSecretPasswordManager::f_SecurePassword_Remove(NMib::NStr::CStr const &_Key)
	{
		DMibLock(mp_Lock);

		GError *pResult = nullptr;

		bool bSuccess = mp_KeychainLibrary.m_fSecretPasswordClearSync(&gc_SecretSchema, nullptr, &pResult, "key", _Key.f_GetStr(), nullptr);

		if (!bSuccess && !pResult)
			return ESecurePassword_NotFound;

		fg_SecurePassword_GNOME_Log("secret_password_clear_sync", bSuccess, pResult);

		return fg_SecurePassword_Decode_Result(bSuccess, pResult);
	}

	ESecurePassword CLibSecretPasswordManager::f_SecurePassword_Get(NMib::NStr::CStr const &_Key, NMib::NStr::CStrSecure &o_Password)
	{
		DMibLock(mp_Lock);

		GError *pResult = nullptr;

		gchar *pPassword = mp_KeychainLibrary.m_fSecretPasswordLookupSync(&gc_SecretSchema, nullptr, &pResult, "key", _Key.f_GetStr(), nullptr);

		if (!pPassword && !pResult)
			return ESecurePassword_NotFound;

		if (pPassword)
		{
			o_Password = pPassword;
			mp_KeychainLibrary.m_fSecretPasswordFree(pPassword);
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
