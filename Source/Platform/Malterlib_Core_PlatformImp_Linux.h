// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

using namespace NMib;
using namespace NMib::NStr;
using namespace NMib::NTime;
using namespace NMib::NMemory;
using namespace NMib::NContainer;

#include "Malterlib_Core_PlatformImp_POSIX.h"
#include "Malterlib_Core_PlatformImp_POSIX_Net.h"
#include "Malterlib_Core_PlatformImp_Linux_FileNotification.h"

namespace NMib::NSys
{
	struct CLinuxPasswordManager;
}

namespace NMib::NDBus
{
	class CSystem;
}

class CSystemLinux : public NMib::CSystem
{
public:
	static void fs_ForkPrepare();
	static void fs_ForkParentOrChild();
	static void fs_ForkParent();
	static void fs_ForkChild();

	CSystemLinux();
	NMib::NSys::CLinuxPasswordManager* f_GetPasswordManager();

	void f_InitModule();
	void f_DestroyThreadSpecific();

	void f_Destruct();
	static void fs_ThreadDestructionHook(void* _ThreadID);

	void f_RegisterDestructionHookForThread();
	void f_LoadLibraries();

	CSystem_POSIX m_Posix;

	pthread_key_t m_ThreadDestructionHook;

	NMib::NStorage::TCAggregate<CPOSIXSocketContext> m_SocketContext = { DAggregateInit };

	NMib::NStorage::TCUniquePointer<NMib::NDBus::CSystem> m_pDBus; // May be nullptr

	NMib::NAtomic::TCAtomic<umint> m_PasswordManagerCreated;
	NMib::NStorage::TCUniquePointer<NMib::NSys::CLinuxPasswordManager> m_pPasswordManager;

	NMib::NSys::EDesktopEnvironment m_DesktopEnvironment;

	bool m_bForkedChild = false;

	NMib::NStorage::TCAggregate<CFileChangeNotificationContext> m_FileChangeNotificationContext = { DAggregateInit };
};
