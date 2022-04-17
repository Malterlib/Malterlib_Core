// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <sys/utsname.h>

#include "Malterlib_Core_PlatformImp_MacOSX_ObjCPP.h"
#include "Malterlib_Core_Platform_OSX_ObjC.h"

#import <Cocoa/Cocoa.h>
#import <AppKit/NSApplication.h>
//#include <CoreFoundation/CFString.h>
//#include <TargetConditionals.h>
#import <CoreFoundation/CoreFoundation.h>
#import <ApplicationServices/ApplicationServices.h>
#import <CoreServices/CoreServices.h>
#import <Security/Security.h>
//#include <HIServices/Processes.h>

#import <objc/runtime.h>
#import <sys/param.h>
#include <sys/sysctl.h>

#include "Malterlib_Core_PlatformImp_POSIX.h"
#include <Mib/Cryptography/UUID>

#if DPlatformVersion >= 1080
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

using namespace NMib;
using namespace NStr;
using namespace NContainer;

#include <Mib/Core/PlatformSpecific/PosixErrNo>

extern NAtomic::TCAtomicAggregate<mint> g_ForceMmapSequence;

namespace NMib
{
	namespace NSys
	{
		namespace
		{
			bool fg_OverrideHome()
			{
				static bool bOverrideHome = fg_GetSys()->f_GetEnvironmentVariable("MalterlibOverrideHome") == "true";
				return bOverrideHome;
			}
		}

		CStr fg_MacOSX_GetApplicationSupportDirectory()
		{
			if (fg_OverrideHome())
				return fg_GetSys()->f_GetEnvironmentVariable("HOME") / "Library/Application Support";

			CAutoReleasePool ARPool;

			NSArray *pPaths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
			if (!pPaths)
				DMibErrorFile(NPlatform::fg_FormatErrno("NSSearchPathForDirectoriesInDomains (get application support diretory)", errno));

			NSString *pAppSupport = [pPaths objectAtIndex:0];

			CFDataRef pData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, (CFStringRef)pAppSupport, kCFStringEncodingUTF8, '?');

			if (!pData)
				DMibErrorFile(NPlatform::fg_FormatErrno("CFStringCreateExternalRepresentation (get application support diretory)", errno));

			auto Cleanup = g_OnScopeExit / [&]
				{
					CFRelease(pData);
				}
			;

			CStr Out;
			Out.f_AddStr(CFDataGetBytePtr(pData), CFDataGetLength(pData));

			return Out;
		}

		CStrNonTracked fg_MacOSX_GetApplicationSupportDirectoryNonTracked()
		{
			if (fg_OverrideHome())
				return NSys::fg_Process_GetEnvironmentVariable_NonProtected(CStrNonTracked("HOME")) / "Library/Application Support";

			CAutoReleasePool ARPool;

			NSArray *pPaths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
			if (!pPaths)
				DMibErrorFile(NPlatform::fg_FormatErrno("NSSearchPathForDirectoriesInDomains (get application support diretory)", errno));
			NSString *pAppSupport = [pPaths objectAtIndex:0];

			CFDataRef pData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, (CFStringRef)pAppSupport, kCFStringEncodingUTF8, '?');

			if (!pData)
				DMibErrorFile(NPlatform::fg_FormatErrno("CFStringCreateExternalRepresentation (get application support diretory)", errno));

			auto Cleanup = g_OnScopeExit / [&]
				{
					CFRelease(pData);
				}
			;

			CWStrNonTracked Out;
			Out.f_AddStr(CFDataGetBytePtr(pData), CFDataGetLength(pData));

			return Out;
		}

		CStr fg_MacOSX_GetCachesDirectory()
		{
			if (fg_OverrideHome())
				return fg_GetSys()->f_GetEnvironmentVariable("HOME") / "Library/Caches";

			CAutoReleasePool ARPool;

			NSArray *pPaths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
			if (!pPaths)
				DMibErrorFile(NPlatform::fg_FormatErrno("NSSearchPathForDirectoriesInDomains (get caches diretory)", errno));
			NSString *pCaches = [pPaths objectAtIndex:0];

			CFDataRef pData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, (CFStringRef)pCaches, kCFStringEncodingUTF8, '?');

			if (!pData)
				DMibErrorFile(NPlatform::fg_FormatErrno("CFStringCreateExternalRepresentation (get caches diretory)", errno));

			auto Cleanup = g_OnScopeExit / [&]
				{
					CFRelease(pData);
				}
			;

			CStr Out;
			Out.f_AddStr(CFDataGetBytePtr(pData), CFDataGetLength(pData));

			return Out;
		}

		CStrNonTracked fg_MacOSX_GetCachesDirectoryNonTracked()
		{
			if (fg_OverrideHome())
				return NSys::fg_Process_GetEnvironmentVariable_NonProtected(CStrNonTracked("HOME")) / "Library/Caches";

			CAutoReleasePool ARPool;

			NSArray *pPaths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
			if (!pPaths)
				DMibErrorFile(NPlatform::fg_FormatErrno("NSSearchPathForDirectoriesInDomains (get caches diretory)", errno));

			NSString *pCaches = [pPaths objectAtIndex:0];

			CFDataRef pData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, (CFStringRef)pCaches, kCFStringEncodingUTF8, '?');

			if (!pData)
				DMibErrorFile(NPlatform::fg_FormatErrno("CFStringCreateExternalRepresentation (get caches diretory)", errno));

			auto Cleanup = g_OnScopeExit / [&]
				{
					CFRelease(pData);
				}
			;

			CWStrNonTracked Out;
			Out.f_AddStr(CFDataGetBytePtr(pData), CFDataGetLength(pData));

			return Out;
		}

		CStr fg_MacOSX_GetLogDirectory()
		{
			if (fg_OverrideHome())
				return fg_GetSys()->f_GetEnvironmentVariable("HOME") / "Library/Logs";

			CAutoReleasePool ARPool;

			NSArray *pPaths = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES);
			if (!pPaths)
				DMibErrorFile(NPlatform::fg_FormatErrno("NSSearchPathForDirectoriesInDomains (get log diretory)", errno));
			NSString *pCaches = [pPaths objectAtIndex:0];

			CFDataRef pData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, (CFStringRef)pCaches, kCFStringEncodingUTF8, '?');

			if (!pData)
				DMibErrorFile(NPlatform::fg_FormatErrno("CFStringCreateExternalRepresentation (get log diretory)", errno));

			auto Cleanup = g_OnScopeExit / [&]
				{
					CFRelease(pData);
				}
			;

			CStr Out;
			Out.f_AddStr(CFDataGetBytePtr(pData), CFDataGetLength(pData));
			Out += "/Logs";

			return Out;
		}

		CStrNonTracked fg_MacOSX_GetLogDirectoryNonTracked()
		{
			if (fg_OverrideHome())
				return NSys::fg_Process_GetEnvironmentVariable_NonProtected(CStrNonTracked("HOME")) / "Library/Logs";

			CAutoReleasePool ARPool;

			NSArray *pPaths = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES);
			if (!pPaths)
				DMibErrorFile(NPlatform::fg_FormatErrno("NSSearchPathForDirectoriesInDomains (get log diretory)", errno));
			NSString *pCaches = [pPaths objectAtIndex:0];

			CFDataRef pData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, (CFStringRef)pCaches, kCFStringEncodingUTF8, '?');

			if (!pData)
				DMibErrorFile(NPlatform::fg_FormatErrno("CFStringCreateExternalRepresentation (get log diretory)", errno));

			auto Cleanup = g_OnScopeExit / [&]
				{
					CFRelease(pData);
				}
			;

			CWStrNonTracked Out;
			Out.f_AddStr(CFDataGetBytePtr(pData), CFDataGetLength(pData));
			Out += "/Logs";

			return Out;
		}


        CStr fg_MacOSX_GetUserHomeDirectory()
		{
			if (fg_OverrideHome())
				return fg_GetSys()->f_GetEnvironmentVariable("HOME");

			CAutoReleasePool ARPool;

            NSString *pPath = NSHomeDirectory();
			if (!pPath)
				DMibErrorFile(NPlatform::fg_FormatErrno("NSHomeDirectory (get home diretory)", errno));
			CFDataRef pData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, (CFStringRef)pPath, kCFStringEncodingUTF8, '?');

			if (!pData)
				DMibErrorFile(NPlatform::fg_FormatErrno("CFStringCreateExternalRepresentation (get home diretory)", errno));

			auto Cleanup = g_OnScopeExit / [&]
				{
					CFRelease(pData);
				}
			;

			CStr Out;
			Out.f_AddStr(CFDataGetBytePtr(pData), CFDataGetLength(pData));

			return Out;
		}

		CStrNonTracked fg_MacOSX_GetUserHomeDirectoryNonTracked()
		{
			if (fg_OverrideHome())
				return NSys::fg_Process_GetEnvironmentVariable_NonProtected(CStrNonTracked("HOME"));

			CAutoReleasePool ARPool;

            NSString *pPath = NSHomeDirectory();
			if (!pPath)
				DMibErrorFile(NPlatform::fg_FormatErrno("NSHomeDirectory (get home diretory)", errno));
			CFDataRef pData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, (CFStringRef)pPath, kCFStringEncodingUTF8, '?');

			if (!pData)
				DMibErrorFile(NPlatform::fg_FormatErrno("CFStringCreateExternalRepresentation (get home diretory)", errno));

			auto Cleanup = g_OnScopeExit / [&]
				{
					CFRelease(pData);
				}
			;

			CWStrNonTracked Out;
			Out.f_AddStr(CFDataGetBytePtr(pData), CFDataGetLength(pData));

			return Out;
		}

		NMib::NStr::CStr fg_MacOSX_GetSystemLanguage()
		{
			CFArrayRef lLanguages = CFLocaleCopyPreferredLanguages();

			if ( CFArrayGetCount(lLanguages) == 0 )
				return "en";

			CFStringRef pCode = (CFStringRef)CFArrayGetValueAtIndex(lLanguages, 0);

			CFDataRef pData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, pCode, kCFStringEncodingUTF8, '?');

			int ErrNo = errno;

			CFRelease(lLanguages);

			if (!pData)
				DMibError(NPlatform::fg_FormatErrno("CFStringCreateExternalRepresentation (get system language)", ErrNo));

			auto Cleanup = g_OnScopeExit / [&]
				{
					CFRelease(pData);
				}
			;

			CStr Language(CFDataGetBytePtr(pData), CFDataGetLength(pData));

			return Language;
		}
	} // Namespace NSys

	namespace NRuntime
	{
		void fg_MacOSX_NativeHideMainWindow(void * _pNativeWindowHandle)
		{
			[NSApp hide:(__bridge NSView*)_pNativeWindowHandle];
		}

		void fg_MacOSX_SetBadgeLabel(NStr::CStr const& _Label)
		{
			CAutoReleasePool ARPool;

			NSString* pLabel = NPlatform::fg_MaxOSX_GetString(_Label);
			[[NSApp dockTile] setBadgeLabel:pLabel];
		}

		void fg_MacOSX_ClearBadgeLabel()
		{
			[[NSApp dockTile] setBadgeLabel:nil];
		}

		bool fg_MacOSX_PlaySound(uint8 const* _pWaveform, mint _nBytes)
		{
			CAutoReleasePool ARPool;

			NSData* pNSWaveform = [NSData dataWithBytes:_pWaveform length:_nBytes];
			if (!pNSWaveform)
				return false;

			NSSound* pSound = [[NSSound alloc ]initWithData:pNSWaveform];
			if (!pSound)
				return false;

			return [pSound play];
		}

	} // Namespace NRuntime
}

namespace NMib
{
	namespace NDaemon
	{
		class CMenuContext
		{
		public:

			NFunction::TCFunction<void ()> m_fPause;
			NFunction::TCFunction<void ()> m_fResume;

		};

		static CMenuContext *fsg_GetContext(id self)
		{
			auto Class = object_getClass(self);
			auto pContextIvar = class_getInstanceVariable(Class, "m_pContext");
			void *(*fGetIvarVoidStar)(id, Ivar) = (void *(*)(id, Ivar))object_getIvar;
			return (CMenuContext *)fGetIvarVoidStar(self, pContextIvar);
		}

		static void fsg_PauseDaemon(id self, SEL cmd, id obj)
		{
			CMenuContext *pContext = fsg_GetContext(self);
			if (pContext && pContext->m_fPause)
				pContext->m_fPause();
		}

		static void fsg_ResumeDaemon(id self, SEL cmd, id obj)
		{
			CMenuContext *pContext = fsg_GetContext(self);
			if (pContext && pContext->m_fResume)
				pContext->m_fResume();
		}

		static void fsg_QuitDaemon(id self, SEL cmd, id obj)
		{
			[NSApp stop:self];
		}

		static void fsg_CancelDaemon(id self, SEL cmd, id obj)
		{
			// Do nothing.
		}

		static NThread::CMutualSpin gs_QuitLock;
		static NSApplication *gs_pSerivceApplication = nullptr;
		static bool gs_bPendingQuit = false;

		inline_never void fg_CancelRunDaemonStatusAppDoCancel(void *)
		{
			[NSApp stop:NSApp];
			[NSApp abortModal];
		}

		void fg_CancelRunDaemonStatusApp()
		{
			{
				DMibLock(gs_QuitLock);
				gs_bPendingQuit = true;
				if (!gs_pSerivceApplication)
					return;
			}

			dispatch_async_f
				(
					dispatch_get_main_queue()
					, nullptr
					, &fg_CancelRunDaemonStatusAppDoCancel
				)
			;
		}

		void fg_RunDaemonStatusApp
			(
				NFunction::TCFunction<void ()> const &_fPause
				, NFunction::TCFunction<void ()> const &_fResume
				, NStr::CStr const &_DaemonName
				, NContainer::CByteVector const& _IconData
				, bool _bRunningAsDaemon
			)
		{
			bool bPendingQuit;
			{
				DMibLock(gs_QuitLock);
				gs_pSerivceApplication = [NSApplication sharedApplication];
				bPendingQuit = gs_bPendingQuit;
			}
			auto Cleanup = g_OnScopeExit / [&]
				{
					DMibLock(gs_QuitLock);
					gs_pSerivceApplication = nullptr;
				}
			;

			if (bPendingQuit)
				return;

			CAutoReleasePool ARPool;

			CMenuContext MenuContext;
			MenuContext.m_fPause = _fPause;
			MenuContext.m_fResume = _fResume;

			bool bCanPause = _fPause && _fResume;

			NStr::CStr ClassName
				= "MenuContext_"
				+ NCryptography::CUniversallyUniqueIdentifier(NCryptography::EUniversallyUniqueIdentifierGenerate_Random).f_GetAsString
				(
					NCryptography::EUniversallyUniqueIdentifierFormat_AlphaNum
				)
			;

			Class pMenuContextClass = objc_allocateClassPair([NSObject class], ClassName.f_GetStr(), 0);

			auto Cleanup2 = g_OnScopeExit / [&]
				{
					objc_disposeClassPair(pMenuContextClass);
				}
			;

			CStr Types = CStr::CFormat("{}{}{}{}") << @encode(id) << @encode(id) << @encode(SEL) << @encode(id);

			if (bCanPause)
			{
				class_addMethod(pMenuContextClass, @selector(doPauseDaemon:), (IMP)fsg_PauseDaemon, Types.f_GetStr());
				class_addMethod(pMenuContextClass, @selector(doResumeDaemon:), (IMP)fsg_ResumeDaemon, Types.f_GetStr());
			}
			class_addMethod(pMenuContextClass, @selector(doQuitDaemon:), (IMP)fsg_QuitDaemon, Types.f_GetStr());
			class_addMethod(pMenuContextClass, @selector(doCancelDaemon:), (IMP)fsg_CancelDaemon, Types.f_GetStr());

			class_addIvar(pMenuContextClass, "m_pContext", sizeof(void *), rint(log2(sizeof(void *))), @encode(void *));
			auto pContextIvar = class_getInstanceVariable(pMenuContextClass, "m_pContext");

			objc_registerClassPair(pMenuContextClass);

			id pMenuContextObject = [[pMenuContextClass alloc] init];

			void (*fSetIvarVoidStar)(id, Ivar, void *) = (void (*)(id, Ivar, void *))object_setIvar;
			fSetIvarVoidStar(pMenuContextObject, pContextIvar, &MenuContext);

			NSMenu* pMenu = [[NSMenu alloc] initWithTitle:@""];

			{
				NSMenuItem* pNameItem = [[NSMenuItem alloc] initWithTitle:NPlatform::fg_MaxOSX_GetString(_DaemonName) action:NULL keyEquivalent:@""];
				pNameItem.enabled = false;
				[pMenu addItem:pNameItem];
			}

			if (!_bRunningAsDaemon)
			{
				NStr::CStr ProgramPath = NFile::CFile::fs_GetProgramPath();
				NSMenuItem* pNameItem = [[NSMenuItem alloc] initWithTitle:NPlatform::fg_MaxOSX_GetString(ProgramPath) action:NULL keyEquivalent:@""];
				pNameItem.enabled = false;
				[pMenu addItem:pNameItem];
			}

			[pMenu addItem:[NSMenuItem separatorItem]];

			if (bCanPause)
			{
				NSMenuItem* pPauseItem = [[NSMenuItem alloc] initWithTitle:@"Pause" action:NULL keyEquivalent:@""];
				pPauseItem.target = (id)pMenuContextObject;
				pPauseItem.action = @selector(doPauseDaemon:);
				[pMenu addItem:pPauseItem];

				NSMenuItem* pResumeItem = [[NSMenuItem alloc] initWithTitle:@"Resume" action:NULL keyEquivalent:@""];
				pResumeItem.target = (id)pMenuContextObject;
				pResumeItem.action = @selector(doResumeDaemon:);
				[pMenu addItem:pResumeItem];
			}

			NSMenuItem* pQuitItem = [[NSMenuItem alloc] initWithTitle: (_bRunningAsDaemon ? @"Restart" : @"Quit") action:NULL keyEquivalent:@""];
			pQuitItem.target = (id)pMenuContextObject;
			pQuitItem.action = @selector(doQuitDaemon:);

			[pMenu addItem:pQuitItem];
			[pMenu addItem:[NSMenuItem separatorItem]];

			NSMenuItem* pCancelItem = [[NSMenuItem alloc] initWithTitle:@"Cancel" action:NULL keyEquivalent:@""];
			pCancelItem.target = (id)pMenuContextObject;
			pCancelItem.action = @selector(doCancelDaemon:);
			[pMenu addItem:pCancelItem];

			auto *pSystemStatusBar = [NSStatusBar systemStatusBar];

			NSStatusItem* pStatusItem = [pSystemStatusBar statusItemWithLength:NSVariableStatusItemLength];

			if (!_IconData.f_IsEmpty())
			{
				NSData* pImageData = [NSData dataWithBytes:_IconData.f_GetArray() length:_IconData.f_GetLen()];
				NSImage* pImage = [[NSImage alloc] initWithData:pImageData];
				double ExpectedHeight = pSystemStatusBar.thickness - 2.0;
				double Scaling = ExpectedHeight / pImage.size.height;
				pImage.size = {pImage.size.width * Scaling, pImage.size.height * Scaling};
				[pImage setTemplate: YES];

				pStatusItem.button.image = pImage;
			}

			if (!pStatusItem.button.image)
				pStatusItem.button.title = @"MD";

			pStatusItem.menu = pMenu;

			{
				DMibLock(gs_QuitLock);
				if (gs_bPendingQuit)
					return;
			}
			[NSApp run];
		}
	}
}

namespace NMib
{
	namespace NOSXRuntime
	{
		struct CFileChangeNoticationContext::CInternal
		{
			CInternal()
			{
			}
			CFRunLoopRef m_RunLoopRef = nullptr;
		};

		using namespace NFile;
		CFileChangeNoticationContext::CFileChangeNoticationContext()
			: m_pInternal(fg_Construct())
		{
			try
			{
				if (CFile::fs_FileExists(CStr("/etc/synthetic.conf")))
				{
					CStr Contents = CFile::fs_ReadStringFromFile(CStr("/etc/synthetic.conf"), true);
					for (auto &Line : Contents.f_SplitLine())
					{
						if (Line.f_StartsWith("#"))
							continue;
						auto Mapping = Line.f_Split("\t");
						if (Mapping.f_GetLen() != 2)
							continue;
						CStr To = "/" + Mapping[0];
						CStr From = "/" + Mapping[1];
						m_SyntheticPaths[To] = From;
					}
				}
			}
			catch (NFile::CExceptionFile const &)
			{
			}
		}

		CFileChangeNoticationContext::~CFileChangeNoticationContext()
		{
			auto &Internal = *m_pInternal;
			if (m_pProcessThread)
			{
				m_pProcessThread->f_Stop(false);
				{
					DMibLock(m_RunLoopLock);
					if (Internal.m_RunLoopRef)
						CFRunLoopStop(Internal.m_RunLoopRef);
				}
				m_pProcessThread.f_Clear();
			}

			m_bDestroying = true;
			// Clear dispatch queue
			while (auto ToDispatch = m_DispatchQueue.f_Pop())
				(*ToDispatch)();

			DMibFastCheck(m_OpenNotifications.f_IsEmpty()); // File notifications were not closed

			// Clean up any non-closed notifications
			while (auto pPop = m_OpenNotifications.f_Pop())
			{
				NStorage::TCSharedPointer<CNotification> pNotification = fg_Explicit((CNotification *)pPop);
				pNotification->f_RefCountDecrease(DMibRefcountDebuggingOnly(pNotification->m_DebugSelfRef));
			}
		}

		CFileChangeNoticationContext::CNotification::CFileSnapshot::CFileSnapshot(CFileSnapshot *_pParent)
			: m_pParent(_pParent)
		{
		}

		CFileChangeNoticationContext::CNotification::CFileSnapshot::CFileSnapshot(CFileSnapshot const &_Other, CFileSnapshot *_pParent, CSnapshotsByNode &_SnapshotsByNode)
			: m_pParent(_pParent)
			, m_FileName(_Other.m_FileName)
			, m_FullFileName(_Other.m_FullFileName)
			, m_Stats(_Other.m_Stats)
		{
			for (auto iChild = _Other.m_Children.f_GetIterator(); iChild; ++iChild)
			{
				auto &NewChild = m_Children.f_Insert(fg_Construct(*iChild, this, _SnapshotsByNode));

				if (iChild->m_Link.f_IsInTree())
					m_ChildrenByName.f_Insert(NewChild);
				if (iChild->m_LinkNode.f_IsInList())
				{
					auto &Snapshots = _SnapshotsByNode[NewChild.f_GetKey()];
					Snapshots.f_Insert(NewChild);
				}
			}
		}

		CFileChangeNoticationContext::CNotification::CNotification(CFileChangeNoticationContext *_pContext)
			: m_pContext(_pContext)
			, m_RootSnapshot(nullptr)
		{
			m_pEventStream = nullptr;
			m_pReportTo = nullptr;
			m_Flags = EFileChange_None;
			m_bAddedToRunLoop = false;
			m_bStreamStarted = false;
		}
		CFileChangeNoticationContext::CNotification::~CNotification()
		{
			f_Clear();
		}
		void CFileChangeNoticationContext::CNotification::f_Clear()
		{
			if (m_bStreamStarted)
			{
				FSEventStreamStop(m_pEventStream);
				m_bStreamStarted = false;
			}
			if (m_bAddedToRunLoop)
			{
				FSEventStreamInvalidate(m_pEventStream);
				m_bAddedToRunLoop = false;
			}

			if (m_pEventStream)
			{
				FSEventStreamRelease(m_pEventStream);
				m_pEventStream = nullptr;
			}

			m_Changes.f_Clear();

			{
				DMibLock(m_pContext->m_Lock);
				if (m_Link.f_IsInList())
					m_pContext->m_OpenNotifications.f_Remove(this);
			}

			m_RootSnapshot.f_Clear(m_SnapshotsByNode);
		}

		using CUpdateSnapshotContext = CFileChangeNoticationContext::CNotification::CUpdateSnapshotContext;

		void fg_LinkFileSnapshot(CUpdateSnapshotContext &_Context, CFileChangeNoticationContext::CNotification::CFileSnapshot &_Snapshot)
		{
			auto FileKey = _Snapshot.f_GetKey();

			if (_Snapshot.m_LinkNode.f_IsInList())
				_Snapshot.f_RemoveFromNodeMap(_Context.m_SnapshotsByNode);

			_Snapshot.m_bDelete = false;
			_Snapshot.m_UpdateSequence = _Context.m_UpdateSequence;

			auto &Snapshots = _Context.m_SnapshotsByNode[FileKey];
			for (auto &Snapshot : Snapshots)
			{
				if (Snapshot.m_FullFileName.f_IsEmpty())
					continue;
				CStr Directory = CFile::fs_GetPath(_Context.m_NotificationPath.m_UserPath / Snapshot.m_FullFileName);
				if (_Context.m_DirsToUpdate(Directory).f_WasCreated())
				{
					DMibFileChangeNotificationsDebugOut("ADDED DIRECTORY '{}' from '{}' finding '{}'", Directory, _Snapshot.m_FullFileName, Snapshot.m_FullFileName);
				}
			}

			if (auto *pOldSnapshots = _Context.m_OldSnapshotsByNode.f_FindEqual(FileKey))
			{
				for (auto &Snapshot : *pOldSnapshots)
				{
					if (Snapshot.m_FullFileName.f_IsEmpty())
						continue;
					CStr Directory = CFile::fs_GetPath(_Context.m_NotificationPath.m_UserPath / Snapshot.m_FullFileName);
					if (_Context.m_DirsToUpdate(Directory).f_WasCreated())
					{
						DMibFileChangeNotificationsDebugOut("ADDED OLD DIRECTORY '{}' from '{}' finding '{}'", Directory, _Snapshot.m_FullFileName, Snapshot.m_FullFileName);
					}
				}
			}

			Snapshots.f_Insert(_Snapshot);
		}

		void fgr_UpdateFileSnapshot
			(
				CUpdateSnapshotContext &_Context
			 	, CFileChangeNoticationContext::CNotification::CFileSnapshot &_Snapshot
				, bool _bRecursive
				, bool _bRecursiveInfoNeeded
			)
		{
			CStr Path = _Context.m_NotificationPath.m_UserPath / _Snapshot.m_FullFileName;

			if (_Snapshot.m_Stats.st_dev == 0)
			{
				_Context.m_ChangedPaths[_Snapshot.m_FullFileName] = true;
				_Snapshot.f_Clear(_Context.m_SnapshotsByNode);
				return;
			}

			for (auto iSnap = _Snapshot.m_Children.f_GetIterator(); iSnap; ++iSnap)
				iSnap->m_bDelete = true;

			DIR *pDir = opendir(Path);
			if (pDir)
			{
				auto Cleanup
					= fg_OnScopeExit
					(
						[&]()
						{
							closedir(pDir);
						}
					)
				;

				while (1)
				{
					dirent *pEntry = readdir(pDir);

					if (!pEntry)
						break;

					CStr FileName(pEntry->d_name);

					if (FileName == "." || FileName == "..")
						continue;

					CStr FullFileName = CFile::fs_AppendPath(Path, FileName);

					struct stat Stats;
					if (lstat(FullFileName, &Stats))
						continue;

					bool bWasCreated = false;
					auto pExistingChild = _Snapshot.m_ChildrenByName.f_FindEqual(FileName);
					if (!pExistingChild)
					{
						pExistingChild = &_Snapshot.m_Children.f_Insert(fg_Construct(&_Snapshot));
						bWasCreated = true;
					}
					auto &Child = *pExistingChild;

					// Remove ourselves
					if (Child.m_Link.f_IsInTree())
						_Snapshot.m_ChildrenByName.f_Remove(Child);

					if (Child.m_LinkNode.f_IsInList())
						Child.f_RemoveFromNodeMap(_Context.m_SnapshotsByNode);

					Child.m_FileName = FileName;
					Child.m_FullFileName = CFile::fs_AppendPath(_Snapshot.m_FullFileName, FileName);
					Child.m_Stats = Stats;

					// Remove other child
					if (auto pChildByName = _Snapshot.m_ChildrenByName.f_FindEqual(FileName))
						_Snapshot.m_ChildrenByName.f_Remove(pChildByName);

					_Snapshot.m_ChildrenByName.f_Insert(Child);

					fg_LinkFileSnapshot(_Context, Child);

					if
						(
							(Stats.st_mode & S_IFDIR)
							&& !(Stats.st_mode & S_IFLNK)
							&& Stats.st_dev == _Snapshot.f_GetKey().st_dev
							&&
							(
								_bRecursive
								|| (_bRecursiveInfoNeeded && bWasCreated)
							)
						)
					{
						fgr_UpdateFileSnapshot(_Context, Child, _bRecursive, _bRecursiveInfoNeeded);
					}
				}
			}

			for (auto iSnap = _Snapshot.m_Children.f_GetIterator(); iSnap;)
			{
				if (iSnap->m_bDelete)
				{
					_Context.m_ChangedPaths[iSnap->m_FullFileName] = true;
					iSnap->f_Clear(_Context.m_SnapshotsByNode);

					if (iSnap->m_Link.f_IsInTree())
						_Snapshot.m_ChildrenByName.f_Remove(*iSnap);
					if (iSnap->m_LinkNode.f_IsInList())
						iSnap->f_RemoveFromNodeMap(_Context.m_SnapshotsByNode);
					iSnap.f_Remove();
					continue;
				}
				++iSnap;
			}
		}

		void fgr_TraceSnapshot(CFileChangeNoticationContext::CNotification::CFileSnapshot const &_Snapshot, mint _Depth)
		{
			DMibTrace("{sj*}{}\n", "" << (_Depth * 4) << _Snapshot.m_FileName);
			for (auto iChild = _Snapshot.m_Children.f_GetIterator(); iChild; ++iChild)
			{
				fgr_TraceSnapshot(*iChild, _Depth + 1);
			}
		}

		void CFileChangeNoticationContext::CNotification::f_AddNotification
			(
				CFindChangesContext &o_Context
				, EFileChangeNotification _Type
				, CStr const &_RelativePath
				, NStr::CStr const &_RenameFrom
			)
		{
			CChange Change;
			Change.m_Notification = _Type;
			Change.m_Path = _RelativePath;
			Change.m_PathFrom = _RenameFrom;

			if (o_Context.m_ChangesSet(Change).f_WasCreated())
			{
				if (_Type == EFileChangeNotification_Renamed)
					o_Context.m_ChangesFileNameRename.f_Insert(fg_Move(Change));
				else if (_Type == EFileChangeNotification_Removed)
					o_Context.m_ChangesFileNameRemove.f_Insert(fg_Move(Change));
				else if (_Type == EFileChangeNotification_Added)
					o_Context.m_ChangesFileNameAdd.f_Insert(fg_Move(Change));
				else
					o_Context.m_Changes.f_Insert(fg_Move(Change));
			}
		}

		void CFileChangeNoticationContext::CNotification::CFileSnapshot::f_PotentiallyRemoved(CFindChangesContext &o_Context) const
		{
			o_Context.m_PotentialOld[f_GetKey()];

			for (auto &Child : m_Children)
				Child.f_PotentiallyRemoved(o_Context);
		}

		void CFileChangeNoticationContext::CNotification::fr_FindChanges
			(
				CFindChangesContext &o_Context
				, CFileSnapshot const &_NewSnapshot
				, bool _bRecursive
				, bool _bPotentianllyRecursive
				, bool _bFirstRecursive
			)
		{
			CFileSnapshot const *pOldSnapshot = nullptr;

			if (auto const *pOldSnapshots = m_SnapshotsByNode.f_FindEqual(_NewSnapshot.f_GetKey()))
			{
				{
					uint64 Best = 0;
					for (auto &OldSnapshot : *pOldSnapshots)
					{
						DMibFastCheck(!OldSnapshot.m_bDelete);

						if (OldSnapshot.m_FullFileName == _NewSnapshot.m_FullFileName && (OldSnapshot.m_UpdateSequence > Best || !pOldSnapshot))
							pOldSnapshot = &OldSnapshot;
					}
				}
				if (!pOldSnapshot)
				{
					uint64 Best = 0;
					for (auto &OldSnapshot : *pOldSnapshots)
					{
						DMibFastCheck(!OldSnapshot.m_bDelete);
						if (OldSnapshot.m_FileName == _NewSnapshot.m_FileName && (OldSnapshot.m_UpdateSequence > Best || !pOldSnapshot))
						{
							pOldSnapshot = &OldSnapshot;
							break;
						}
					}
				}
				if (!pOldSnapshot)
				{
					uint64 Best = 0;
					for (auto &OldSnapshot : *pOldSnapshots)
					{
						DMibFastCheck(!OldSnapshot.m_bDelete);
						if (OldSnapshot.m_UpdateSequence > Best || !pOldSnapshot)
						{
							Best = OldSnapshot.m_UpdateSequence;
							pOldSnapshot = &OldSnapshot;
						}
					}
				}
			}

			bool bIsDir = (_NewSnapshot.m_Stats.st_mode & S_IFDIR) && !(_NewSnapshot.m_Stats.st_mode & S_IFLNK);

			if (!pOldSnapshot)
			{
				if (((bIsDir && (m_Flags & EFileChange_DirectoryName)) || (!bIsDir && (m_Flags & EFileChange_FileName))) && !_NewSnapshot.m_FullFileName.f_IsEmpty())
					f_AddNotification(o_Context, EFileChangeNotification_Added, _NewSnapshot.m_FullFileName);
			}
			else
			{
				o_Context.m_UsedOld[_NewSnapshot.f_GetKey()];

				bool bModified = false;

				if (m_Flags & EFileChange_Attributes)
				{
					if (_NewSnapshot.m_Stats.st_flags != pOldSnapshot->m_Stats.st_flags)
						bModified = true;
				}

				if (m_Flags & EFileChange_FileSize)
				{
					if (_NewSnapshot.m_Stats.st_size != pOldSnapshot->m_Stats.st_size)
						bModified = true;
					if (bIsDir && _NewSnapshot.m_Stats.st_nlink != pOldSnapshot->m_Stats.st_nlink)
						bModified = true;
				}

				if (m_Flags & EFileChange_Write)
				{
					if
						(
							_NewSnapshot.m_Stats.st_mtimespec.tv_sec != pOldSnapshot->m_Stats.st_mtimespec.tv_sec
							|| _NewSnapshot.m_Stats.st_mtimespec.tv_nsec != pOldSnapshot->m_Stats.st_mtimespec.tv_nsec
							|| _NewSnapshot.m_Stats.st_ctimespec.tv_sec != pOldSnapshot->m_Stats.st_ctimespec.tv_sec
							|| _NewSnapshot.m_Stats.st_ctimespec.tv_nsec != pOldSnapshot->m_Stats.st_ctimespec.tv_nsec
						)
					{
						bModified = true;
					}
				}

				if (m_Flags & EFileChange_Security)
				{
					if
						(
							_NewSnapshot.m_Stats.st_mode != pOldSnapshot->m_Stats.st_mode
							|| _NewSnapshot.m_Stats.st_uid != pOldSnapshot->m_Stats.st_uid
							|| _NewSnapshot.m_Stats.st_gid != pOldSnapshot->m_Stats.st_gid
						)
					{
						bModified = true;
					}

				}

				if (bModified)
					f_AddNotification(o_Context, EFileChangeNotification_Modified, _NewSnapshot.m_FullFileName);

				if (bIsDir && _bPotentianllyRecursive)
				{
					if
						(
							_NewSnapshot.m_Stats.st_size != pOldSnapshot->m_Stats.st_size
							|| _NewSnapshot.m_Stats.st_nlink != pOldSnapshot->m_Stats.st_nlink
							|| _NewSnapshot.m_Stats.st_mtimespec.tv_sec != pOldSnapshot->m_Stats.st_mtimespec.tv_sec
							|| _NewSnapshot.m_Stats.st_mtimespec.tv_nsec != pOldSnapshot->m_Stats.st_mtimespec.tv_nsec
							|| _NewSnapshot.m_Stats.st_ctimespec.tv_sec != pOldSnapshot->m_Stats.st_ctimespec.tv_sec
							|| _NewSnapshot.m_Stats.st_ctimespec.tv_nsec != pOldSnapshot->m_Stats.st_ctimespec.tv_nsec
						)
					{
						_bRecursive = true;
					}
				}
			}

			if (_bRecursive || _bFirstRecursive)
			{
				if (pOldSnapshot)
				{
					if (_bRecursive)
						pOldSnapshot->f_PotentiallyRemoved(o_Context);
					else
					{
						for (auto &Child : pOldSnapshot->m_Children)
							o_Context.m_PotentialOld[Child.f_GetKey()];
					}
				}
				for (auto &Child : _NewSnapshot.m_Children)
					fr_FindChanges(o_Context, Child, _bRecursive, _bPotentianllyRecursive, false);
			}
		}

		void CFileChangeNoticationContext::CNotification::CFileSnapshot::f_RemoveFromNodeMap(NContainer::TCMap<CFileKey, DMibListLinkDS_List(CFileSnapshot, m_LinkNode)> &_Map)
		{
			auto Key = f_GetKey();
			auto pList = _Map.f_FindEqual(Key);

			m_LinkNode.f_Unlink();

			if (pList && pList->f_IsEmpty())
				_Map.f_Remove(Key);
		}

		void CFileChangeNoticationContext::CNotification::CFileSnapshot::f_Clear(CSnapshotsByNode &_SnapshotsByNode)
		{
			for (auto &Child : m_Children)
				Child.f_Clear(_SnapshotsByNode);
			if (m_LinkNode.f_IsInList())
				f_RemoveFromNodeMap(_SnapshotsByNode);
			m_ChildrenByName.f_Clear();
			m_Children.f_Clear();
		}

		void CFileChangeNoticationContext::CNotification::f_ScanDir
			(
				CStr const &_Path
				, bool _bInitial
				, bool _bNeedSubDirs
			 	, CUpdateSnapshotContext &_UpdateContext
				, CFileSnapshot &o_NewSnapshot
			)
		{
			bool bRecursive = (m_Flags & EFileChange_Recursive) != 0;

			if (!_Path.f_StartsWith(m_NotificationPath.m_UserPath))
			{
				DMibTrace("'{}' does not start with '{}'\n", _Path << m_NotificationPath.m_UserPath);
				DMibSafeCheck(false, "File path not correct!!");
				return; // Error
			}

			if (!bRecursive && _Path != m_NotificationPath.m_UserPath)
				return; // Drop this notification as we are not interested in it

			if (!_bNeedSubDirs && bRecursive)
				bRecursive = false;

			CStr RelativePath = NMib::NFile::CFile::fs_MakePathRelative(_Path, m_NotificationPath.m_UserPath);

			if (RelativePath == _Path)
			{
				DMibSafeCheck(false, "Failed to make relative path!!");
				return; // Error
			}

			if (!_bInitial && (_Path != m_NotificationPath.m_UserPath || (bRecursive != (m_Flags & EFileChange_Recursive) != 0)))
			{
				CFileSnapshot *pFindSnapshot = &o_NewSnapshot;
				{
					CStr FindPath = RelativePath;
					while (pFindSnapshot && !FindPath.f_IsEmpty())
					{
						CStr Path = fg_GetStrSep(FindPath, "/");
						pFindSnapshot = pFindSnapshot->m_ChildrenByName.f_FindEqual(Path);
					}
				}

				if (pFindSnapshot)
				{
					auto &bChangedPathRecursive = _UpdateContext.m_ChangedPaths[pFindSnapshot->m_FullFileName];
					if (bRecursive)
						bChangedPathRecursive = true;
					// We can do a partial update
					if (bRecursive)
						pFindSnapshot->f_Clear(_UpdateContext.m_SnapshotsByNode);
					else
					{
						for (auto iSnap = pFindSnapshot->m_Children.f_GetIterator(); iSnap; ++iSnap)
							iSnap->m_bDelete = true;
					}
					pFindSnapshot->f_RemoveFromNodeMap(_UpdateContext.m_SnapshotsByNode);
					if (lstat(CFile::fs_AppendPath(m_NotificationPath.m_UserPath, pFindSnapshot->m_FullFileName).f_GetStr(), &pFindSnapshot->m_Stats))
						NMemory::fg_MemClear(pFindSnapshot->m_Stats);
					else
						fg_LinkFileSnapshot(_UpdateContext, *pFindSnapshot);
					fgr_UpdateFileSnapshot(_UpdateContext, *pFindSnapshot, bRecursive, (m_Flags & EFileChange_Recursive) != 0);

					for (auto iSnap = pFindSnapshot->m_Children.f_GetIterator(); iSnap;)
					{
						if (iSnap->m_bDelete)
						{
							_UpdateContext.m_ChangedPaths[iSnap->m_FullFileName] = true;
							iSnap->f_Clear(_UpdateContext.m_SnapshotsByNode);
							if (iSnap->m_Link.f_IsInTree())
								pFindSnapshot->m_ChildrenByName.f_Remove(*iSnap);
							if (iSnap->m_LinkNode.f_IsInList())
								iSnap->f_RemoveFromNodeMap(_UpdateContext.m_SnapshotsByNode);
							iSnap.f_Remove();
							continue;
						}
						++iSnap;
					}
				}
				else
				{
					// We have to do a full update
					o_NewSnapshot.f_RemoveFromNodeMap(_UpdateContext.m_SnapshotsByNode);
					if (lstat(m_NotificationPath.m_UserPath.f_GetStr(), &o_NewSnapshot.m_Stats))
						NMemory::fg_MemClear(o_NewSnapshot.m_Stats);
					else
						fg_LinkFileSnapshot(_UpdateContext, o_NewSnapshot);
					fgr_UpdateFileSnapshot(_UpdateContext, o_NewSnapshot, bRecursive, (m_Flags & EFileChange_Recursive) != 0);
					auto &bChangedPathRecursive = _UpdateContext.m_ChangedPaths[""];
					if (bRecursive)
						bChangedPathRecursive = true;
				}

				//fgr_TraceSnapshot(NewSnapshot, 0);
			}
			else
			{
				o_NewSnapshot.f_RemoveFromNodeMap(_UpdateContext.m_SnapshotsByNode);
				if (lstat(m_NotificationPath.m_UserPath.f_GetStr(), &o_NewSnapshot.m_Stats))
					NMemory::fg_MemClear(o_NewSnapshot.m_Stats);
				else
					fg_LinkFileSnapshot(_UpdateContext, o_NewSnapshot);
				fgr_UpdateFileSnapshot(_UpdateContext, o_NewSnapshot, bRecursive, (m_Flags & EFileChange_Recursive) != 0);
				auto &bChangedPathRecursive = _UpdateContext.m_ChangedPaths[""];
				if (bRecursive)
					bChangedPathRecursive = true;
			}
		}

		void CFileChangeNoticationContext::CNotification::f_ProcessChanges
			(
				mint _nEvents
				, ch8 const **_pPaths
				, FSEventStreamEventFlags const _Flags[]
				, FSEventStreamEventId const _IDs[]
				, bool _bInitialScan
			)
		{
			CFileChangeNoticationContext::CNotification::CSnapshotsByNode NewSnapshotsByNode;
			CFileSnapshot NewSnapshot{m_RootSnapshot, nullptr, NewSnapshotsByNode};

			++m_UpdateSequence;

			CUpdateSnapshotContext UpdateContext{NewSnapshotsByNode, m_SnapshotsByNode, m_UpdateSequence, m_NotificationPath};

			for (mint i = 0; i < _nEvents; ++i)
			{
				CStr Path(_pPaths[i]);
				if (Path.f_GetAt(Path.f_GetLen() - 1) == '/')
					Path = Path.f_Left(Path.f_GetLen() - 1);

				FSEventStreamEventFlags Flags = _Flags[i];

				if (NMib::CSystem::ms_PlatformVersion >= 10'15'00)
				{
					if
						(
							(!(Flags & kFSEventStreamEventFlagItemIsDir) && !(Flags & kFSEventStreamEventFlagMustScanSubDirs))
							|| (Flags & (kFSEventStreamEventFlagItemRenamed | kFSEventStreamEventFlagItemCreated | kFSEventStreamEventFlagItemRemoved))
						)
					{
						if (Path.f_StartsWith(m_NotificationPathCompare.m_ResolvedPath) || Path.f_StartsWith(m_NotificationPathCompare.m_SyntheticPath))
							Path = CFile::fs_GetPath(Path);
					}
				}

				if (Path == m_NotificationPath.m_ResolvedPath || Path == m_NotificationPath.m_SyntheticPath)
					Path = m_NotificationPath.m_UserPath;
				else if (Path.f_StartsWith(m_NotificationPathCompare.m_ResolvedPath))
					Path = m_NotificationPathCompare.m_UserPath + Path.f_Extract(m_NotificationPathCompare.m_ResolvedPath.f_GetLen());
				else if (Path.f_StartsWith(m_NotificationPathCompare.m_SyntheticPath))
					Path = m_NotificationPathCompare.m_UserPath + Path.f_Extract(m_NotificationPathCompare.m_SyntheticPath.f_GetLen());

				UpdateContext.m_DirsToUpdate[Path] = (Flags & (kFSEventStreamEventFlagMustScanSubDirs | kFSEventStreamEventFlagRootChanged)) != 0;
			}

			TCSet<NStr::CStr> UpdatedDirectories;
			TCVector<NStr::CStr> RecursiveUpdatedDirectories;

			for (bool bDoneSomething = true; bDoneSomething;)
			{
				bDoneSomething = false;

				auto UpdateThisTime = UpdateContext.m_DirsToUpdate;

				for (auto &bScanSubdirs : UpdateThisTime)
				{
					auto &Path = UpdateThisTime.fs_GetKey(bScanSubdirs);
					if (!UpdatedDirectories(Path).f_WasCreated())
						continue;

					auto WholePath = Path + "/";
					bool bAlreadyScanned = false;
					for (auto &Dir : RecursiveUpdatedDirectories)
					{
						if (WholePath.f_StartsWith(Dir))
						{
							bAlreadyScanned = true;
							break;
						}
					}

					if (bAlreadyScanned)
						continue;

					if (bScanSubdirs)
						RecursiveUpdatedDirectories.f_Insert(Path + "/");

					bDoneSomething = true;
					f_ScanDir(Path, _bInitialScan, bScanSubdirs, UpdateContext, NewSnapshot);
				}
			}

			fg_LinkFileSnapshot(UpdateContext, NewSnapshot);

			if (!_bInitialScan)
			{
				if (UpdateContext.m_ChangedPaths.f_IsEmpty())
				{
					NewSnapshotsByNode.f_Clear();
					return;
				}

				CFindChangesContext FindChangesContext;

				for (auto &bChangedPathRecursive : UpdateContext.m_ChangedPaths)
				{
					CStr const &ChangedPath = UpdateContext.m_ChangedPaths.fs_GetKey(bChangedPathRecursive);
					CFileSnapshot *pFindSnapshot = &NewSnapshot;
					bool bRecursiveSetting = (m_Flags & EFileChange_Recursive) != 0;

					auto ChangedPathComponents = ChangedPath.f_Split<true>("/");
					{
						for (auto &File : ChangedPathComponents)
						{
							pFindSnapshot = pFindSnapshot->m_ChildrenByName.f_FindEqual(File);
							if (!pFindSnapshot)
								break;
						}
					}

					if (pFindSnapshot && pFindSnapshot->m_Stats.st_dev != 0)
						fr_FindChanges(FindChangesContext, *pFindSnapshot, bChangedPathRecursive, bRecursiveSetting, true);
					else
					{
						if (ChangedPathComponents.f_IsEmpty())
							m_RootSnapshot.f_PotentiallyRemoved(FindChangesContext);
						else
						{
							auto *pSnapshot = &m_RootSnapshot;
							for (auto &File : ChangedPathComponents)
							{
								pSnapshot = pSnapshot->m_ChildrenByName.f_FindEqual(File);
								if (!pSnapshot)
									break;
							}
							if (pSnapshot)
								pSnapshot->f_PotentiallyRemoved(FindChangesContext);
						}
					}
				}

				if (m_Flags & (EFileChange_DirectoryName | EFileChange_FileName))
				{
					for (auto &Used : FindChangesContext.m_UsedOld)
					{
						auto *pOldSnapshots = m_SnapshotsByNode.f_FindEqual(Used);
						auto *pNewSnapshots = NewSnapshotsByNode.f_FindEqual(Used);

						DMibCheck(pOldSnapshots);
						DMibCheck(pNewSnapshots);
						if (!pOldSnapshots || !pNewSnapshots)
							continue;

						TCSet<CStr> OldFileNames;
						TCSet<CStr> NewFileNames;

						bool bIsDir = false;

						for (auto &Snapshot : *pOldSnapshots)
						{
							OldFileNames[Snapshot.m_FullFileName];
							bIsDir = (Snapshot.m_Stats.st_mode & S_IFDIR) && !(Snapshot.m_Stats.st_mode & S_IFLNK);
						}

						for (auto &Snapshot : *pNewSnapshots)
						{
							NewFileNames[Snapshot.m_FullFileName];
							bIsDir = (Snapshot.m_Stats.st_mode & S_IFDIR) && !(Snapshot.m_Stats.st_mode & S_IFLNK);
						}

						if (OldFileNames != NewFileNames && ((bIsDir && (m_Flags & EFileChange_DirectoryName)) || (!bIsDir && (m_Flags & EFileChange_FileName))))
						{
							// Remove files that are the same
							for (auto iOld = OldFileNames.f_GetIterator(); iOld;)
							{
								if (NewFileNames.f_Remove(iOld.f_GetKey()))
								{
									iOld.f_Remove();
									continue;
								}
								++iOld;
							}

							// Assume renames
							{
								auto iOld = OldFileNames.f_GetIterator();
								auto iNew = NewFileNames.f_GetIterator();
								for (; iOld && iNew;)
								{
									f_AddNotification(FindChangesContext, EFileChangeNotification_Renamed, *iNew, *iOld);
									iNew.f_Remove();
									iOld.f_Remove();
								}
							}

							for (auto &Removed : OldFileNames)
								f_AddNotification(FindChangesContext, EFileChangeNotification_Removed, Removed);
							for (auto &Added : NewFileNames)
								f_AddNotification(FindChangesContext, EFileChangeNotification_Added, Added);
						}
					}
					for (auto &PotentiallyRemoved : FindChangesContext.m_PotentialOld)
					{
						if (!FindChangesContext.m_UsedOld.f_FindEqual(PotentiallyRemoved))
						{
							auto *pSnapshots = m_SnapshotsByNode.f_FindEqual(PotentiallyRemoved);
							if (!pSnapshots || NewSnapshotsByNode.f_FindEqual(PotentiallyRemoved))
								continue;
							for (auto &Snapshot : *pSnapshots)
								f_AddNotification(FindChangesContext, EFileChangeNotification_Removed, Snapshot.m_FullFileName);
						}
					}
				}

				if
					(
					 	!FindChangesContext.m_ChangesFileNameRename.f_IsEmpty()
					 	|| !FindChangesContext.m_ChangesFileNameRemove.f_IsEmpty()
					 	|| !FindChangesContext.m_ChangesFileNameAdd.f_IsEmpty()
					 	|| !FindChangesContext.m_Changes.f_IsEmpty()
					)
				{
					DMibLock(m_ChangesLock);
					m_Changes.f_Insert(fg_Move(FindChangesContext.m_ChangesFileNameRename));
					m_Changes.f_Insert(fg_Move(FindChangesContext.m_ChangesFileNameRemove));
					m_Changes.f_Insert(fg_Move(FindChangesContext.m_ChangesFileNameAdd));
					m_Changes.f_Insert(fg_Move(FindChangesContext.m_Changes));
					if (m_pReportTo)
						m_pReportTo->f_Signal();
				}
			}

			if (NewSnapshot.m_LinkNode.f_IsInList())
				NewSnapshot.f_RemoveFromNodeMap(NewSnapshotsByNode);

			m_RootSnapshot.f_Clear(m_SnapshotsByNode);
			m_SnapshotsByNode.f_Clear();
			m_SnapshotsByNode = fg_Move(NewSnapshotsByNode);
			m_RootSnapshot.m_Stats = NewSnapshot.m_Stats;
			m_RootSnapshot.m_ChildrenByName = fg_Move(NewSnapshot.m_ChildrenByName);
			m_RootSnapshot.m_Children = fg_Move(NewSnapshot.m_Children);
			{
				CUpdateSnapshotContext UpdateContext{m_SnapshotsByNode, m_SnapshotsByNode, m_UpdateSequence, m_NotificationPath};
				fg_LinkFileSnapshot(UpdateContext, m_RootSnapshot);
			}
		}

		void CFileChangeNoticationContext::CNotification::f_ProcessChangesPerFile
			(
				mint _nEvents
				, ch8 const **_pPaths
				, FSEventStreamEventFlags const _Flags[]
				, FSEventStreamEventId const _IDs[]
			)
		{
			CFindChangesContext FindChangesContext;

			TCSet<CStr> ProtectedDirs;
			for (mint i = 0; i < _nEvents; ++i)
			{
				CStr EventPath(_pPaths[i]);
				if (EventPath.f_GetAt(EventPath.f_GetLen() - 1) == '/')
					EventPath = EventPath.f_Left(EventPath.f_GetLen() - 1);

				FSEventStreamEventFlags Flags = _Flags[i];

				if (ProtectedDirs.f_FindEqual(EventPath))
				{
					//DMibConErrOut2("IGNORE Change: {} = {nfh} - {}\n", EventPath, Flags, _IDs[i]);
					continue;
				}

				CStr RelativePath = NMib::NFile::CFile::fs_MakePathRelative(EventPath, m_NotificationPath.m_UserPath);

				//DMibConErrOut2("Change: {} = {nfh} - {}\n", EventPath, Flags, _IDs[i]);

				bool bIsDir = (Flags & kFSEventStreamEventFlagItemIsDir) && !(Flags & kFSEventStreamEventFlagItemIsSymlink);

				if (m_Flags & NFile::EFileChange_Recursive)
				{
					if ((Flags & kFSEventStreamEventFlagItemCreated) && bIsDir)
					{
						CStr ToFind = fg_Format("{}/*", EventPath);
						for (auto &File : NFile::CFile::fs_FindFilesEx(ToFind, NFile::EFileAttrib_File | NFile::EFileAttrib_Directory, true, false))
						{
							CStr RelativePath = NMib::NFile::CFile::fs_MakePathRelative(File.m_Path, m_NotificationPath.m_UserPath);
							bool bIsDir = (File.m_Attribs & NFile::EFileAttrib_Directory) && !(File.m_Attribs & NFile::EFileAttrib_Link);
							if (((m_Flags & NFile::EFileChange_DirectoryName) && bIsDir) || ((m_Flags & NFile::EFileChange_FileName) && !bIsDir))
								f_AddNotification(FindChangesContext, EFileChangeNotification_Added, RelativePath);
						}
					}
				}

				if
					(
						((Flags & kFSEventStreamEventFlagItemModified) && (m_Flags & NFile::EFileChange_Write))
						|| ((Flags & (kFSEventStreamEventFlagItemInodeMetaMod | kFSEventStreamEventFlagItemChangeOwner | kFSEventStreamEventFlagItemXattrMod)) && (m_Flags & NFile::EFileChange_Attributes))
					)
				{
					f_AddNotification(FindChangesContext, EFileChangeNotification_Modified, RelativePath);
				}
				else if ((Flags & (kFSEventStreamEventFlagItemCreated | kFSEventStreamEventFlagItemRenamed)) == (kFSEventStreamEventFlagItemCreated | kFSEventStreamEventFlagItemRenamed))
				{
					m_RenamedFromQueue.f_Insert({RelativePath, bIsDir});
				}
				else if ((Flags & (kFSEventStreamEventFlagItemCreated | kFSEventStreamEventFlagItemRenamed)) == kFSEventStreamEventFlagItemRenamed)
				{
					if (!m_RenamedFromQueue.f_IsEmpty())
					{
						CStr RenamedFrom = fg_Get<0>(m_RenamedFromQueue.f_Pop());
						CStr RenamedTo = RelativePath;
						if (((m_Flags & NFile::EFileChange_DirectoryName) && bIsDir) || ((m_Flags & NFile::EFileChange_FileName) && !bIsDir))
							f_AddNotification(FindChangesContext, EFileChangeNotification_Renamed, RenamedTo, RenamedFrom);
						CStr RenamedToDirectory = NFile::CFile::fs_GetPath(RenamedTo);
						CStr RenamedFromDirectory = NFile::CFile::fs_GetPath(RenamedFrom);
						if (m_Flags & (NFile::EFileChange_FileSize | NFile::EFileChange_Attributes))
							f_AddNotification(FindChangesContext, EFileChangeNotification_Modified, RenamedToDirectory);
						if (RenamedToDirectory != RenamedFromDirectory)
						{
							if (m_Flags & (NFile::EFileChange_FileSize | NFile::EFileChange_Attributes))
								f_AddNotification(FindChangesContext, EFileChangeNotification_Modified, RenamedFromDirectory);
						}
					}
					else
					{
						if (((m_Flags & NFile::EFileChange_DirectoryName) && bIsDir) || ((m_Flags & NFile::EFileChange_FileName) && !bIsDir))
							f_AddNotification(FindChangesContext, EFileChangeNotification_Added, RelativePath);
						if (m_Flags & (NFile::EFileChange_FileSize | NFile::EFileChange_Attributes))
							f_AddNotification(FindChangesContext, EFileChangeNotification_Modified, CFile::fs_GetPath(RelativePath));
					}
				}
				else if (Flags & kFSEventStreamEventFlagItemCreated)
				{
					ProtectedDirs[CFile::fs_GetPath(EventPath)];
					if (((m_Flags & NFile::EFileChange_DirectoryName) && bIsDir) || ((m_Flags & NFile::EFileChange_FileName) && !bIsDir))
						f_AddNotification(FindChangesContext, EFileChangeNotification_Added, RelativePath);
					if (m_Flags & (NFile::EFileChange_FileSize | NFile::EFileChange_Attributes))
						f_AddNotification(FindChangesContext, EFileChangeNotification_Modified, CFile::fs_GetPath(RelativePath));
				}
				else if (Flags & kFSEventStreamEventFlagItemRemoved)
				{
					if (((m_Flags & NFile::EFileChange_DirectoryName) && bIsDir) || ((m_Flags & NFile::EFileChange_FileName) && !bIsDir))
						f_AddNotification(FindChangesContext, EFileChangeNotification_Removed, RelativePath);
					if (m_Flags & (NFile::EFileChange_FileSize | NFile::EFileChange_Attributes))
						f_AddNotification(FindChangesContext, EFileChangeNotification_Modified, CFile::fs_GetPath(RelativePath));
				}

			}

			//DMibConErrOut2("m_RenamedFromQueue {}\n", m_RenamedFromQueue.f_GetLen());

			for (auto &RenameTo : m_RenamedFromQueue)
			{
				CStr const &FileName = fg_Get<0>(RenameTo);
				bool bIsDir = fg_Get<1>(RenameTo);
				if (((m_Flags & NFile::EFileChange_DirectoryName) && bIsDir) || ((m_Flags & NFile::EFileChange_FileName) && !bIsDir))
					f_AddNotification(FindChangesContext, EFileChangeNotification_Removed, FileName);
				if (m_Flags & (NFile::EFileChange_FileSize | NFile::EFileChange_Attributes))
					f_AddNotification(FindChangesContext, EFileChangeNotification_Modified, CFile::fs_GetPath(FileName));
			}

			m_RenamedFromQueue.f_Clear();

			//DMibConErrOut2("GENERATED {}\n", FindChangesContext.m_ChangesFileName.f_GetLen() + FindChangesContext.m_Changes.f_GetLen());

			if
				(
					!FindChangesContext.m_ChangesFileNameRename.f_IsEmpty()
					|| !FindChangesContext.m_ChangesFileNameRemove.f_IsEmpty()
					|| !FindChangesContext.m_ChangesFileNameAdd.f_IsEmpty()
					|| !FindChangesContext.m_Changes.f_IsEmpty()
				)
			{
				DMibLock(m_ChangesLock);
				m_Changes.f_Insert(fg_Move(FindChangesContext.m_ChangesFileNameRename));
				m_Changes.f_Insert(fg_Move(FindChangesContext.m_ChangesFileNameRemove));
				m_Changes.f_Insert(fg_Move(FindChangesContext.m_ChangesFileNameAdd));
				m_Changes.f_Insert(fg_Move(FindChangesContext.m_Changes));
				if (m_pReportTo)
					m_pReportTo->f_Signal();
			}
		}

		void CFileChangeNoticationContext::CNotification::f_InitialScan()
		{
			ch8 const *Paths[1] = {m_NotificationPath.m_UserPath.f_GetStr()};
			FSEventStreamEventFlags const Flags[1] = {kFSEventStreamEventFlagMustScanSubDirs};
			FSEventStreamEventId const IDs[1] = {1};

			f_ProcessChanges(1, Paths, Flags, IDs, true);
		}

		void CFileChangeNoticationContext::CNotification::f_FullRescan()
		{
			ch8 const *Paths[1] = {m_NotificationPath.m_UserPath.f_GetStr()};
			FSEventStreamEventFlags const Flags[1] = {kFSEventStreamEventFlagMustScanSubDirs};
			FSEventStreamEventId const IDs[1] = {1};

			f_ProcessChanges(1, Paths, Flags, IDs, false);
		}

		void CFileChangeNoticationContext::fs_EventCallback
			(
				ConstFSEventStreamRef streamRef
				, void *clientCallBackInfo
				, size_t numEvents
				, void *eventPaths
				, const FSEventStreamEventFlags eventFlags[]
				, const FSEventStreamEventId eventIDs[]
			)
		{
			if (!numEvents)
				return;

			// Syncronize with previously returned virtual memory
			g_ForceMmapSequence.f_FetchAdd(1);

			CNotification *pNotification = (CNotification *)clientCallBackInfo;

			if (pNotification->m_bPerFileEvents)
				pNotification->f_ProcessChangesPerFile(numEvents, (ch8 const **)eventPaths, eventFlags, eventIDs);
			else
				pNotification->f_ProcessChanges(numEvents, (ch8 const **)eventPaths, eventFlags, eventIDs, false);
		}
		void CFileChangeNoticationContext::f_DispatchOnThread(NMib::NFunction::TCFunctionMovable<void ()> &&_Dispatch)
		{
			auto &Internal = *m_pInternal;
			m_DispatchQueue.f_Push(fg_Move(_Dispatch));
			{
				DMibLock(m_RunLoopLock);
				if (Internal.m_RunLoopRef)
				{
					auto pThis = this;
					CFRunLoopPerformBlock
						(
						 	Internal.m_RunLoopRef
						 	, kCFRunLoopDefaultMode
						 	, ^()
						 	{
								while (auto ToDispatch = pThis->m_DispatchQueue.f_Pop())
									(*ToDispatch)();
							}
						)
					;
					CFRunLoopWakeUp(Internal.m_RunLoopRef);
				}
			}

			f_StartThread();
		}

		void CFileChangeNoticationContext::f_StartThread()
		{
			if (m_pProcessThread)
				return;
			DMibLock(m_RunLoopLock);
			if (m_pProcessThread)
				return;

			m_pProcessThread
				= NThread::CThreadObject::fs_StartThread
				(
					[this](NThread::CThreadObject *_pThread) -> aint
					{
						auto &Internal = *m_pInternal;

						CFRunLoopSourceContext RunLoopSourceContext
							{
								0
								, nullptr
								, nullptr
								, nullptr
								, nullptr
								, nullptr
								, nullptr
								, nullptr
								, nullptr
								, [](void *info)
								{
								}
							}
						;

						CFRunLoopSourceRef pDummyRunLoopSource = CFRunLoopSourceCreate
							(
							 	nullptr
							 	, 0
							 	, &RunLoopSourceContext
							)
						;

						{
							DMibLock(m_RunLoopLock);
							Internal.m_RunLoopRef = CFRunLoopGetCurrent();
							CFRunLoopAddSource(Internal.m_RunLoopRef, pDummyRunLoopSource, kCFRunLoopDefaultMode);
						}
						auto Cleanup = g_OnScopeExit / [&]
							{
								CFRelease(pDummyRunLoopSource);
								CFRunLoopRemoveSource(Internal.m_RunLoopRef, pDummyRunLoopSource, kCFRunLoopDefaultMode);
								{
									DMibLock(m_RunLoopLock);
									Internal.m_RunLoopRef = nullptr;
								}
							}
						;


						while (auto ToDispatch = m_DispatchQueue.f_Pop())
							(*ToDispatch)();

						while (_pThread->f_GetState() != NThread::EThreadState_EventWantQuit)
							CFRunLoopRun();

						return 0;
					}
					, "File change notifications"
				)
			;
		}


		CStr fg_GetRealPathName(int _FD, CStr const &_Fallback)
		{
			char RealPath[MAXPATHLEN];
			if (_FD && fcntl(_FD, F_GETPATH, RealPath) != -1)
			{
				//		DMibTrace("{} = {}\n", RealPath << _Fallback);
				return CStr(RealPath);
			}
			else
				return _Fallback;
		}

		CStr fg_GetRealPathName(CStr const &_FileName)
		{
			DIR *pDir = opendir(_FileName);
			if (!pDir)
				return _FileName;

			CStr Ret = fg_GetRealPathName(dirfd(pDir), _FileName);

			closedir(pDir);

			return Ret;
		}

		void *CFileChangeNoticationContext::f_Open(const CStr &_FileName, NMib::NFile::EFileChange _OpenFlags, NMib::NThread::CSemaphoreAggregate *_pReportTo)
		{
			CStr NotificationPath = _FileName;
			if (NMib::NFile::CFile::fs_FileExists(NotificationPath, EFileAttrib_File))
				DMibErrorFile("Open file notification: '{}' is not a directory");

			if (!NMib::NFile::CFile::fs_FileExists(NotificationPath, EFileAttrib_Directory))
				DMibErrorFile("Open file notification: Directory '{}' does not exist");

			CStr UserPath = NotificationPath;
			NotificationPath = fg_GetRealPathName(NotificationPath); // Account for lower/upper case

			CFStringRef WatchPath = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)NotificationPath.f_GetStr(), NotificationPath.f_GetLen(), kCFStringEncodingUTF8, false);
			if (!WatchPath)
				DMibErrorFile("Open file notification: CFStringCreateWithBytes failed");

			auto Cleanup
				= fg_OnScopeExit
				(
					[&]()
					{
						CFRelease(WatchPath);
					}
				)
			;

			CFArrayRef PathsToWatch = CFArrayCreate(NULL, (const void **)&WatchPath, 1, NULL);

			if (!WatchPath)
				DMibErrorFile("Open file notification: CFArrayCreate failed");

			auto Cleanup2
				= fg_OnScopeExit
				(
					[&]()
					{
						CFRelease(PathsToWatch);
					}
				)
			;

			CStr SyntheticPath = NotificationPath;

			for (auto &Source : m_SyntheticPaths)
			{
				auto &Destination = m_SyntheticPaths.fs_GetKey(Source);
				if (NotificationPath == Source)
				{
					SyntheticPath = Destination;
					break;
				}
				else if (NotificationPath.f_StartsWith(Source + "/"))
				{
					SyntheticPath = Destination / NotificationPath.f_Extract(Source.f_GetLen() + 1);
					break;
				}
			}

			NStorage::TCSharedPointer<CNotification> pNotification = fg_Construct(this);
			pNotification->m_NotificationPath.m_UserPath = UserPath;
			pNotification->m_NotificationPath.m_ResolvedPath = NotificationPath;
			pNotification->m_NotificationPath.m_SyntheticPath = SyntheticPath;
			pNotification->m_NotificationPathCompare.m_UserPath = UserPath + "/";
			pNotification->m_NotificationPathCompare.m_ResolvedPath = NotificationPath + "/";
			pNotification->m_NotificationPathCompare.m_SyntheticPath = SyntheticPath + "/";

			FSEventStreamContext CallbackContext;
			CallbackContext.version = 0;
			CallbackContext.info = pNotification.f_Get();
			CallbackContext.retain = nullptr;
			CallbackContext.release = nullptr;
			CallbackContext.copyDescription = nullptr;

			// This doesn't work because all events are accumelated, so if one file get's a flag set, that flag will be set forever in subsequent events
			pNotification->m_bPerFileEvents = false; //= CSystem::ms_PlatformVersion >= 10'07'00;

			FSEventStreamRef pStream;

			/* Create the stream, passing in a callback */
			pStream = FSEventStreamCreate
				(
					NULL
					, &fs_EventCallback
					, &CallbackContext
					, PathsToWatch
					, kFSEventStreamEventIdSinceNow
					, CFAbsoluteTime(0.0)
					, kFSEventStreamCreateFlagNoDefer
					| kFSEventStreamCreateFlagWatchRoot
					| ((pNotification->m_bPerFileEvents || NMib::CSystem::ms_PlatformVersion >= 10'15'00) ? kFSEventStreamCreateFlagFileEvents : 0)
				)
			;

			if (!pStream)
				DMibErrorFile("Open file notification: FSEventStreamCreate failed");

			pNotification->m_pEventStream = pStream;
			{
				DMibLock(m_Lock);
				m_OpenNotifications.f_Insert(pNotification.f_Get());
			}
			pNotification->f_RefCountIncrease(DMibRefcountDebuggingOnly(pNotification->m_DebugSelfRef));

			pNotification->m_Flags = _OpenFlags;
			pNotification->m_pReportTo = _pReportTo;

			// Scan full dir in this call so we don't miss any notifications
			if (!pNotification->m_bPerFileEvents)
				pNotification->f_InitialScan();

			NThread::CEvent SetupDone;
			SetupDone.f_ResetSignaled();

			f_DispatchOnThread
				(
					[this, pNotification, &SetupDone]()
					{
						if (m_bDestroying)
							return;
						auto &Internal = *m_pInternal;
						FSEventStreamScheduleWithRunLoop(pNotification->m_pEventStream, Internal.m_RunLoopRef, kCFRunLoopDefaultMode);
						pNotification->m_bAddedToRunLoop = true;
						FSEventStreamStart(pNotification->m_pEventStream);
						pNotification->m_bStreamStarted = true;
						// Scan full dir and note any changes that have happened since f_Open was called, the rest will be handled by the notifications from the OS
						if (!pNotification->m_bPerFileEvents)
							pNotification->f_FullRescan();
						SetupDone.f_SetSignaled();
					}
				)
			;

			// We need to wait for notification to be fully setup or the the user might miss some file changes
			SetupDone.f_Wait();

			return pNotification.f_Get();
		}

		void CFileChangeNoticationContext::f_Close(void *_pNotification)
		{
			DMibLock(m_Lock);
			NStorage::TCSharedPointer<CNotification> pNotification = fg_Explicit((CNotification *)_pNotification);

			pNotification->f_RefCountDecrease(DMibRefcountDebuggingOnly(pNotification->m_DebugSelfRef));
			{
				DMibLock(pNotification->m_ChangesLock);
				pNotification->m_pReportTo = nullptr;
			}

			f_DispatchOnThread
				(
					[pNotification]()
					{
						pNotification->f_Clear();
					}
				)
			;
		}

		bool CFileChangeNoticationContext::f_Changed(void *_pNotification)
		{
			DMibLock(m_Lock);
			CNotification *pNotification = (CNotification *)_pNotification;
			bool bChanged = false;
			{
				DMibLock(pNotification->m_ChangesLock);
				bChanged = !pNotification->m_Changes.f_IsEmpty();
				pNotification->m_Changes.f_Clear();
			}
			return bChanged;
		}

		bool CFileChangeNoticationContext::f_GetNotification(void *_pNotification, CStr &_Path, NFile::EFileChangeNotification &_Notification, CStr &_PathFrom)
		{
			DMibLock(m_Lock);
			CNotification *pNotification = (CNotification *)_pNotification;
			{
				DMibLock(pNotification->m_ChangesLock);
				if (pNotification->m_Changes.f_IsEmpty())
					return false;

				CNotification::CChange &Change = pNotification->m_Changes.f_GetFirst();

				_Path = Change.m_Path;
				_Notification = Change.m_Notification;
				_PathFrom = Change.m_PathFrom;
				pNotification->m_Changes.f_Remove(Change);
			}
			return true;
		}
	}
} // Namespace NMib
