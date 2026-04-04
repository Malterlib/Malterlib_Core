// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

/*
	At the end of this file: The KWallet DBus Interface
*/
#include "Malterlib_Core_PlatformImp_Linux_SecurePassword.h"
#include <Mib/Core/DynamicLibrary>
#include <Mib/Desktop/DBus>

namespace NMib
{
	namespace NSys
	{

		//
		// KDE Secure Password Implementation
		//

		struct CKWalletPasswordManager : public CLinuxPasswordManager
		{
		private:
			NMib::NStr::CStr mp_Location;
			NMib::NStr::CStr mp_Folder;

			NThread::CMutual mp_DBusLock;
			NDBus::CSystem& mp_DBus;
			NDBus::CConnection mp_Connection;
			NDBus::CError mp_Error;

			NMib::NStr::CStr mp_WalletName;

			bool mp_bOK;

			static char const* mc_KWalletService;
			static char const* mc_KWalletPath;
			static char const* mc_KWalletInterface;

			bool fp_OpenConnection();

			template<typename... Args>
			bool fp_MethodCall(NDBus::CMessage& _oReply, char const* _pMethodName, Args const&... _Args)
			{
				NDBus::CMessage Call(NDBus::EMessageType_Method, mc_KWalletService, mc_KWalletPath, mc_KWalletInterface, _pMethodName, mp_DBus);

				{
					NDBus::CMessageWriter Writer(Call);

					if (!Writer.f_AppendArgs(_Args...))
						return false;
				}

				return mp_Connection.f_BlockingSendWithReply(Call, _oReply);
			}

			template<typename... Args>
			bool fp_ParseSimpleMessage(NDBus::CMessage const& _Msg, Args&... _Args)
			{
				NDBus::CMessageReader Reader(_Msg);

				if (!Reader.f_ArgAvailable())
					return false;

				if (!Reader.f_PopArgs(_Args...))
					return false;

				return true;
			}

			int fp_GetWalletHandle();
			bool fp_LaunchKWallet();
			void fp_LogConnectionError(char const* _pMsg);

		public:

			CKWalletPasswordManager(NDBus::CSystem& _DBus);
			~CKWalletPasswordManager();

			bool f_OK() const;

			bool f_SecurePassword_IsLocked();
			ESecurePassword f_SecurePassword_SetLocation(NMib::NStr::CStr const& _Location);
			ESecurePassword f_SecurePassword_Store(NMib::NStr::CStr const& _Key, NMib::NStr::CStrSecure const& _Password);
			ESecurePassword f_SecurePassword_Remove(NMib::NStr::CStr const& _Key);
			ESecurePassword f_SecurePassword_Get(NMib::NStr::CStr const& _Key, NMib::NStr::CStrSecure& _oPassword);
			ESecurePassword f_SecurePassword_Exists(NMib::NStr::CStr const& _Key);
		};

		char const* CKWalletPasswordManager::mc_KWalletService = "org.kde.kwalletd";
		char const* CKWalletPasswordManager::mc_KWalletPath = "/modules/kwalletd";
		char const* CKWalletPasswordManager::mc_KWalletInterface = "org.kde.KWallet";

		NStorage::TCUniquePointer<CLinuxPasswordManager> fg_CreateKWalletPasswordManager(NDBus::CSystem* _pDBus)
		{
			if (!_pDBus)
				return nullptr;

			if (!_pDBus->f_ReadyForUse())
				return nullptr;

			NStorage::TCUniquePointer<CKWalletPasswordManager> pManager = fg_Construct(*_pDBus);

			if (!pManager->f_OK())
				return nullptr;

			return fg_Move(pManager);
		}

		//
		// KDE Secure Password Implementation
		//

		CKWalletPasswordManager::CKWalletPasswordManager(NDBus::CSystem& _DBus)
			: mp_DBus(_DBus)
			, mp_bOK(false)
			, mp_Connection(_DBus)
			, mp_Error(_DBus)
			, mp_Location("Ids") // This needs to be named like this to be compatibly with old version of library (When Malterlib was named Ids)
			, mp_Folder("Ids Secure Storage")
		{
			if (!mp_Connection.f_Open(NDBus::EDBusBus_Session))
				return;

			auto Cleanup = fg_OnScopeExit(
					[&]()
					{
						if (!mp_bOK)
						{
							mp_Connection.f_Close();
						}
					}
				);

			auto fl_CheckEnabled =
				[&](bool _bLog) -> bool
				{ // Check KWallet is enabled
					NDBus::CMessage IsEnabledReply(mp_DBus);
					if (!fp_MethodCall(IsEnabledReply, "isEnabled"))
					{
						if (_bLog)
							fp_LogConnectionError("Call to isEnabled failed.");
						return false;
					}

					bool bEnabled;
					if (!fp_ParseSimpleMessage(IsEnabledReply, bEnabled))
					{
						if (_bLog)
							DMibLog(Error, "KWallet: isEnabled returned an unexpected reply.", 0);
						return false;
					}

					if (!bEnabled)
					{
						if (_bLog)
							DMibLog(Error, "KWallet: KWallet is not enabled.", 0);
						return false;
					}

					return true;
				};

			if (!fl_CheckEnabled(false))
			{
				if (fp_LaunchKWallet())
				{
					if (!fl_CheckEnabled(true))
						return;
				}
				else
					return;
			}

			{ // Get network wallet name
				NDBus::CMessage WalletNameReply(mp_DBus);
				if (!fp_MethodCall(WalletNameReply, "networkWallet"))
				{
					fp_LogConnectionError("KWallet: Call to networkWallet failed.");
					return;
				}

				if (!fp_ParseSimpleMessage(WalletNameReply, mp_WalletName))
				{
					DMibLog(Error, "KWallet: networkWallet returned an unexpected reply.", 0);
					return;
				}
			}

			mp_bOK = true;
		}

		CKWalletPasswordManager::~CKWalletPasswordManager()
		{
			mp_Connection.f_Close();
		}

		bool CKWalletPasswordManager::f_OK() const
		{
			return mp_bOK;
		}

		bool CKWalletPasswordManager::f_SecurePassword_IsLocked()
		{
			return false;
		}

		ESecurePassword CKWalletPasswordManager::f_SecurePassword_SetLocation(NMib::NStr::CStr const& _Location)
		{
			mp_Location = _Location;
			mp_Folder = _Location + " Secure Storage";
			return ESecurePassword_OK;
		}

		ESecurePassword CKWalletPasswordManager::f_SecurePassword_Store(NMib::NStr::CStr const& _Key, NMib::NStr::CStrSecure const& _Password)
		{
			DMibLock(mp_DBusLock);

			//	method int org.kde.KWallet.writePassword(int handle, QString folder, QString key, QString value, QString appid)

			int32 Handle = fp_GetWalletHandle();
			if (Handle == -1)
				return ESecurePassword_Failure;

			NDBus::CMessage WriteReply(mp_DBus);
			if (!fp_MethodCall(
							WriteReply
						,	"writePassword"
						,	Handle
						,	mp_Folder
						,	_Key
						,	_Password.f_GetStr()
						,	mp_Location))
			{
				fp_LogConnectionError("KWallet: writePassword failed.");
				return ESecurePassword_Failure;
			}

			int32 Result;

			if (!fp_ParseSimpleMessage(WriteReply, Result))
			{
				DMibLog(Error, "KWallet: writePassword returned an unexpected reply.", 0);
				return ESecurePassword_Failure;
			}

			return (Result == 0) ? ESecurePassword_OK : ESecurePassword_Failure;
		}

		ESecurePassword CKWalletPasswordManager::f_SecurePassword_Remove(NMib::NStr::CStr const& _Key)
		{
			DMibLock(mp_DBusLock);

			// method int org.kde.KWallet.removeEntry(int handle, QString folder, QString key, QString appid)


			int32 Handle = fp_GetWalletHandle();
			if (Handle == -1)
				return ESecurePassword_Failure;

			NDBus::CMessage RemoveReply(mp_DBus);
			if (!fp_MethodCall(
							RemoveReply
						,	"removeEntry"
						,	Handle
						,	mp_Folder
						,	_Key
						,	mp_Location))
			{
				fp_LogConnectionError("removeEntry failed.");
				return ESecurePassword_Failure;
			}

			int32 Result;

			if (!fp_ParseSimpleMessage(RemoveReply, Result))
			{
				DMibLog(Error, "KWallet: removeEntry returned an unexpected reply.", 0);
				return ESecurePassword_Failure;
			}

			// OK. So the Result error codes are not actually defined other than 0 == Success.
			// From greping the source code for kwalletd I have come up with the following rules.

			if (Result == -3)
				return ESecurePassword_NotFound;
			else if (Result < 0)
				return ESecurePassword_Failure;
			else
				return ESecurePassword_OK;
		}

		ESecurePassword CKWalletPasswordManager::f_SecurePassword_Get(NMib::NStr::CStr const& _Key, NMib::NStr::CStrSecure& _oPassword)
		{
			DMibLock(mp_DBusLock);

			int32 Handle = fp_GetWalletHandle();
			if (Handle == -1)
				return ESecurePassword_Failure;

			{ // Check it exists first (to differentiate between no password and an empty password)
				// method bool org.kde.KWallet.hasEntry(int handle, QString folder, QString key, QString appid)

				NDBus::CMessage HasEntryReply(mp_DBus);
				if (!fp_MethodCall(
								HasEntryReply
							,	"hasEntry"
							,	Handle
							,	mp_Folder
							,	_Key
							,	mp_Location))
				{
					fp_LogConnectionError("hasEntry failed (for SecurePassword_Get).");
					return ESecurePassword_Failure;
				}

				bool bHasEntry = false;

				if (!fp_ParseSimpleMessage(HasEntryReply, bHasEntry))
				{
					DMibLog(Error, "KWallet: hasEntry returned an unexpected reply (for SecurePassword_Get).", 0);
					return ESecurePassword_Failure;
				}

				if (!bHasEntry)
					return ESecurePassword_NotFound;
			}

			{
				// method QString org.kde.KWallet.readPassword(int handle, QString folder, QString key, QString appid)

				NDBus::CMessage ReadReply(mp_DBus);
				if (!fp_MethodCall(
								ReadReply
							,	"readPassword"
							,	Handle
							,	mp_Folder
							,	_Key
							,	mp_Location))
				{
					fp_LogConnectionError("readPassword failed.");
					return ESecurePassword_Failure;
				}

				if (!fp_ParseSimpleMessage(ReadReply, _oPassword))
				{
					DMibLog(Error, "KWallet: readPassword returned an unexpected reply.", 0);
					return ESecurePassword_Failure;
				}

//				DMibLog(Info, "KWaller: readPassword returned \"{}\" for \"{}\"", _oPassword.f_GetStr(), _Key );
			}

			return ESecurePassword_OK;
		}

		ESecurePassword CKWalletPasswordManager::f_SecurePassword_Exists(NMib::NStr::CStr const& _Key)
		{
			DMibLock(mp_DBusLock);

			// method bool org.kde.KWallet.hasEntry(int handle, QString folder, QString key, QString appid)

			int32 Handle = fp_GetWalletHandle();
			if (Handle == -1)
				return ESecurePassword_Failure;

			NDBus::CMessage HasEntryReply(mp_DBus);
			if (!fp_MethodCall(
							HasEntryReply
						,	"hasEntry"
						,	Handle
						,	mp_Folder
						,	_Key
						,	mp_Location))
			{
				fp_LogConnectionError("hasEntry failed.");
				return ESecurePassword_Failure;
			}

			bool bHasEntry = false;

			if (!fp_ParseSimpleMessage(HasEntryReply, bHasEntry))
			{
				DMibLog(Error, "KWallet: hasEntry returned an unexpected reply.", 0);
				return ESecurePassword_Failure;
			}

			return bHasEntry ? ESecurePassword_OK : ESecurePassword_NotFound;
		}

		int CKWalletPasswordManager::fp_GetWalletHandle()
		{
			int32 Handle = -1;
			bool bHasFolder = false;

			{
				NDBus::CMessage OpenReply(mp_DBus);

				if (!fp_MethodCall(OpenReply, "open", mp_WalletName, (int64)0, mp_Location))
				{
					fp_LogConnectionError("open method failed.");
					return -1;
				}

				if (!fp_ParseSimpleMessage(OpenReply, Handle))
				{
					DMibLog(Error, "KWallet: open method returned unexpected reply.", 0);
					return -1;
				}
			}

			{ // Check for our folder
				NDBus::CMessage HasFolderReply(mp_DBus);

				if (!fp_MethodCall(HasFolderReply, "hasFolder", Handle, mp_Folder, mp_Location))
				{
					fp_LogConnectionError("Failed to check for SecureStorage folder.");
					return -1;
				}

				if (!fp_ParseSimpleMessage(HasFolderReply, bHasFolder))
				{
					DMibLog(Error, "KWallet: hasFolder returned an unexpected reply.", 0);
					return -1;
				}
			}

			// Create the folder if required.
			if (!bHasFolder)
			{
				NDBus::CMessage CreateFolderReply(mp_DBus);

				if (!fp_MethodCall(CreateFolderReply, "createFolder", Handle, mp_Folder, mp_Location))
				{
					fp_LogConnectionError("Call to createFolder failed for SecureStorage folder.");
					return -1;
				}

				if (!fp_ParseSimpleMessage(CreateFolderReply, bHasFolder))
				{
					DMibLog(Error, "KWallet: createFolder returned an unexpected reply.", 0);
					return -1;
				}

				if (!bHasFolder)
				{
					DMibLog(Error, "KWallet: createFolder failed to create SecureStorage folder.", 0);
					return -1;
				}
			}

			return Handle;
		}

		bool CKWalletPasswordManager::fp_LaunchKWallet()
		{
			// method int org.kde.KLauncher.start_service_by_desktop_name(QString serviceName, QStringList urls, QStringList envs, QString startup_id, bool blind, QString& dbusServiceName, QString& error, int& pid)

			NDBus::CMessage LaunchReply(mp_DBus);

			NDBus::CMessage Call
				(
					NDBus::EMessageType_Method
					, "org.kde.klauncher"
					, "/KLauncher"
					, "org.kde.KLauncher"
					, "start_service_by_desktop_name"
					, mp_DBus
				)
			;

			{
				NDBus::CMessageWriter Writer(Call);

				NMib::NContainer::TCVector<NMib::NStr::CStr> lEmptyStrList;

				static char const* pKWalletService = "kwalletd";

				if
					(
						!Writer.f_AppendArgs
						(
							pKWalletService
							, lEmptyStrList
							, lEmptyStrList
							, NMib::NStr::CStr()
							, false
						)
					)
				{
					return false;
				}
			}

			if (!mp_Connection.f_BlockingSendWithReply(Call, LaunchReply))
			{
				fp_LogConnectionError("Failed to launch kwalletd.");
				return false;
			}

			int32 Result;
			NMib::NStr::CStr DubsName;
			NMib::NStr::CStr ErrorString;
			int32 PID;

			if
				(
					!fp_ParseSimpleMessage
					(
						LaunchReply
						, Result
						, DubsName
						, ErrorString
						, PID
					)
				)
			{
				DMibLog(Error, "KWallet: start_service_by_desktop_name returned an unexpected reply.", 0);
				return false;
			}

			if (Result != 0)
			{
				DMibLog(Error, "KWallet: start_service_by_desktop_name for kwalletd failed with \"{}\".", ErrorString);
				return false;
			}

			return true;
		}

		void CKWalletPasswordManager::fp_LogConnectionError(char const* _pMsg)
		{
			NDBus::CError const& Error = mp_Connection.f_GetLastError();
			(void)Error;
			DMibLog(Error, "KWallet: {}", _pMsg);
			DMibLog(Error, "KWallet: ErrorName: \"{}\", Error: \"{}\"", Error.f_GetName(), Error.f_GetMessage());
		}


	} // Namespace NSys

} // Namespace NMib

/*
The KWallet DBus Interface
Service: org.kde.kwalletd
Path: /modules/kwalletd

	signal void org.kde.KWallet.allWalletsClosed()
	signal void org.kde.KWallet.applicationDisconnected(QString wallet, QString application)
	method void org.kde.KWallet.changePassword(QString wallet, qlonglong wId, QString appid)
	method int org.kde.KWallet.close(QString wallet, bool force)
	method int org.kde.KWallet.close(int handle, bool force, QString appid)
	method void org.kde.KWallet.closeAllWallets()
	method bool org.kde.KWallet.createFolder(int handle, QString folder, QString appid)
	method int org.kde.KWallet.deleteWallet(QString wallet)
	method bool org.kde.KWallet.disconnectApplication(QString wallet, QString application)
	method QStringList org.kde.KWallet.entryList(int handle, QString folder, QString appid)
	method int org.kde.KWallet.entryType(int handle, QString folder, QString key, QString appid)
	method bool org.kde.KWallet.folderDoesNotExist(QString wallet, QString folder)
	method QStringList org.kde.KWallet.folderList(int handle, QString appid)
	signal void org.kde.KWallet.folderListUpdated(QString wallet)
	signal void org.kde.KWallet.folderUpdated(QString, QString)
	method bool org.kde.KWallet.hasEntry(int handle, QString folder, QString key, QString appid)
	method bool org.kde.KWallet.hasFolder(int handle, QString folder, QString appid)
	method bool org.kde.KWallet.isEnabled()
	method bool org.kde.KWallet.isOpen(QString wallet)
	method bool org.kde.KWallet.isOpen(int handle)
	method bool org.kde.KWallet.keyDoesNotExist(QString wallet, QString folder, QString key)
	method QString org.kde.KWallet.localWallet()
	method QString org.kde.KWallet.networkWallet()
	method int org.kde.KWallet.open(QString wallet, qlonglong wId, QString appid)
	method int org.kde.KWallet.openAsync(QString wallet, qlonglong wId, QString appid, bool handleSession)
	method int org.kde.KWallet.openPath(QString path, qlonglong wId, QString appid)
	method int org.kde.KWallet.openPathAsync(QString path, qlonglong wId, QString appid, bool handleSession)
	method Q_NOREPLY void org.kde.KWallet.pamOpen(QString wallet, QByteArray passwordHash, int sessionTimeout)
	method QByteArray org.kde.KWallet.readEntry(int handle, QString folder, QString key, QString appid)
	method QVariantMap org.kde.KWallet.readEntryList(int handle, QString folder, QString key, QString appid)
	method QByteArray org.kde.KWallet.readMap(int handle, QString folder, QString key, QString appid)
	method QVariantMap org.kde.KWallet.readMapList(int handle, QString folder, QString key, QString appid)
	method QString org.kde.KWallet.readPassword(int handle, QString folder, QString key, QString appid)
	method QVariantMap org.kde.KWallet.readPasswordList(int handle, QString folder, QString key, QString appid)
	method void org.kde.KWallet.reconfigure()
	method int org.kde.KWallet.removeEntry(int handle, QString folder, QString key, QString appid)
	method bool org.kde.KWallet.removeFolder(int handle, QString folder, QString appid)
	method int org.kde.KWallet.renameEntry(int handle, QString folder, QString oldName, QString newName, QString appid)
	method Q_NOREPLY void org.kde.KWallet.sync(int handle, QString appid)
	method QStringList org.kde.KWallet.users(QString wallet)
	signal void org.kde.KWallet.walletAsyncOpened(int tId, int handle)
	signal void org.kde.KWallet.walletClosed(QString wallet)
	signal void org.kde.KWallet.walletClosed(int handle)
	signal void org.kde.KWallet.walletCreated(QString wallet)
	signal void org.kde.KWallet.walletDeleted(QString wallet)
	signal void org.kde.KWallet.walletListDirty()
	signal void org.kde.KWallet.walletOpened(QString wallet)
	method QStringList org.kde.KWallet.wallets()
	method int org.kde.KWallet.writeEntry(int handle, QString folder, QString key, QByteArray value, QString appid)
	method int org.kde.KWallet.writeEntry(int handle, QString folder, QString key, QByteArray value, int entryType, QString appid)
	method int org.kde.KWallet.writeMap(int handle, QString folder, QString key, QByteArray value, QString appid)
	method int org.kde.KWallet.writePassword(int handle, QString folder, QString key, QString value, QString appid)
	method QDBusVariant org.freedesktop.DBus.Properties.Get(QString interface_name, QString property_name)
	method QVariantMap org.freedesktop.DBus.Properties.GetAll(QString interface_name)
	method void org.freedesktop.DBus.Properties.Set(QString interface_name, QString property_name, QDBusVariant value)
	method QString org.freedesktop.DBus.Introspectable.Introspect()
*/
