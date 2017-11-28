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

namespace NMib
{
	namespace NSys
	{
		CStr fg_MacOSX_GetApplicationSupportDirectory()
		{
			CAutoReleasePool ARPool;

			NSArray *pPaths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
			if (!pPaths)
				DMibErrorFile(NPlatform::fg_FormatErrno("NSSearchPathForDirectoriesInDomains (get application support diretory)", errno));
			
			NSString *pAppSupport = [pPaths objectAtIndex:0];

			CFDataRef pData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, (CFStringRef)pAppSupport, kCFStringEncodingUTF8, '?');

			if (!pData)
				DMibErrorFile(NPlatform::fg_FormatErrno("CFStringCreateExternalRepresentation (get application support diretory)", errno));

			auto Cleanup = g_OnScopeExit > [&]
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
			CAutoReleasePool ARPool;

			NSArray *pPaths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
			if (!pPaths)
				DMibErrorFile(NPlatform::fg_FormatErrno("NSSearchPathForDirectoriesInDomains (get application support diretory)", errno));
			NSString *pAppSupport = [pPaths objectAtIndex:0];

			CFDataRef pData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, (CFStringRef)pAppSupport, kCFStringEncodingUTF8, '?');

			if (!pData)
				DMibErrorFile(NPlatform::fg_FormatErrno("CFStringCreateExternalRepresentation (get application support diretory)", errno));

			auto Cleanup = g_OnScopeExit > [&]
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
			CAutoReleasePool ARPool;

			NSArray *pPaths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
			if (!pPaths)
				DMibErrorFile(NPlatform::fg_FormatErrno("NSSearchPathForDirectoriesInDomains (get caches diretory)", errno));
			NSString *pCaches = [pPaths objectAtIndex:0];

			CFDataRef pData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, (CFStringRef)pCaches, kCFStringEncodingUTF8, '?');

			if (!pData)
				DMibErrorFile(NPlatform::fg_FormatErrno("CFStringCreateExternalRepresentation (get caches diretory)", errno));
			
			auto Cleanup = g_OnScopeExit > [&]
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
			CAutoReleasePool ARPool;

			NSArray *pPaths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
			if (!pPaths)
				DMibErrorFile(NPlatform::fg_FormatErrno("NSSearchPathForDirectoriesInDomains (get caches diretory)", errno));
			
			NSString *pCaches = [pPaths objectAtIndex:0];

			CFDataRef pData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, (CFStringRef)pCaches, kCFStringEncodingUTF8, '?');

			if (!pData)
				DMibErrorFile(NPlatform::fg_FormatErrno("CFStringCreateExternalRepresentation (get caches diretory)", errno));

			auto Cleanup = g_OnScopeExit > [&]
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
			CAutoReleasePool ARPool;
			
			NSArray *pPaths = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES);
			if (!pPaths)
				DMibErrorFile(NPlatform::fg_FormatErrno("NSSearchPathForDirectoriesInDomains (get log diretory)", errno));
			NSString *pCaches = [pPaths objectAtIndex:0];
			
			CFDataRef pData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, (CFStringRef)pCaches, kCFStringEncodingUTF8, '?');
			
			if (!pData)
				DMibErrorFile(NPlatform::fg_FormatErrno("CFStringCreateExternalRepresentation (get log diretory)", errno));
			
			auto Cleanup = g_OnScopeExit > [&]
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
			CAutoReleasePool ARPool;
			
			NSArray *pPaths = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES);
			if (!pPaths)
				DMibErrorFile(NPlatform::fg_FormatErrno("NSSearchPathForDirectoriesInDomains (get log diretory)", errno));
			NSString *pCaches = [pPaths objectAtIndex:0];
			
			CFDataRef pData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, (CFStringRef)pCaches, kCFStringEncodingUTF8, '?');
			
			if (!pData)
				DMibErrorFile(NPlatform::fg_FormatErrno("CFStringCreateExternalRepresentation (get log diretory)", errno));
			
			auto Cleanup = g_OnScopeExit > [&]
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
			CAutoReleasePool ARPool;

            NSString *pPath = NSHomeDirectory();
			if (!pPath)
				DMibErrorFile(NPlatform::fg_FormatErrno("NSHomeDirectory (get home diretory)", errno));
			CFDataRef pData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, (CFStringRef)pPath, kCFStringEncodingUTF8, '?');
            
			if (!pData)
				DMibErrorFile(NPlatform::fg_FormatErrno("CFStringCreateExternalRepresentation (get home diretory)", errno));
            
			auto Cleanup = g_OnScopeExit > [&]
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
			CAutoReleasePool ARPool;

            NSString *pPath = NSHomeDirectory();
			if (!pPath)
				DMibErrorFile(NPlatform::fg_FormatErrno("NSHomeDirectory (get home diretory)", errno));
			CFDataRef pData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, (CFStringRef)pPath, kCFStringEncodingUTF8, '?');
            
			if (!pData)
				DMibErrorFile(NPlatform::fg_FormatErrno("CFStringCreateExternalRepresentation (get home diretory)", errno));
            
			auto Cleanup = g_OnScopeExit > [&]
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

			auto Cleanup = g_OnScopeExit > [&]
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
			[NSApp hide:(NSView*)_pNativeWindowHandle];
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
		
		bint fg_MacOSX_PlaySound(uint8 const* _pWaveform, mint _nBytes)
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
	namespace NService
	{
		class CMenuContext
		{
		public:
			
			NFunction::TCFunction<void ()> m_fPause;
			NFunction::TCFunction<void ()> m_fResume;
			
		};
		
		static void fgs_PauseService(id self, SEL cmd, id obj)
		{
			CMenuContext *pContext;
			object_getInstanceVariable(self, "m_pContext", (void **)&pContext);
			
			if (pContext && pContext->m_fPause)
				pContext->m_fPause();
		}
		
		static void fgs_ResumeService(id self, SEL cmd, id obj)
		{
			CMenuContext *pContext;
			object_getInstanceVariable(self, "m_pContext", (void **)&pContext);
			
			if (pContext && pContext->m_fResume)
				pContext->m_fResume();
		}
		
		static void fgs_QuitService(id self, SEL cmd, id obj)
		{
			[NSApp stop:self];
		}
		
		static void fgs_CancelService(id self, SEL cmd, id obj)
		{
			// Do nothing.
		}

		static NSApplication *gs_pSerivceApplication = nullptr; 
		void fg_CancelRunServiceStatusApp()
		{
			if (!gs_pSerivceApplication)
				return;
			dispatch_async
				(
					dispatch_get_main_queue()
					, ^
					{
						[NSApp stop:NSApp];
						NSEvent* event = [NSEvent otherEventWithType: NSApplicationDefined
															location: NSMakePoint(0,0)
													  modifierFlags: 0
														  timestamp: 0.0
														windowNumber: 0
															context: nil
															subtype: 0
															  data1: 0
															  data2: 0];
						[NSApp postEvent: event atStart: true];
					}
				)
			;
		}
		
		void fg_RunServiceStatusApp(NFunction::TCFunction<void ()> const& _fPause, NFunction::TCFunction<void ()> const& _fResume, NStr::CStr const &_ServiceName, NContainer::TCVector<uint8> const& _IconData)
		{
			CAutoReleasePool ARPool;
			
			CMenuContext MenuContext;
			MenuContext.m_fPause = _fPause;
			MenuContext.m_fResume = _fResume;
			
			NStr::CStr ClassName
				= "MenuContext_" 
				+ NDataProcessing::CUniversallyUniqueIdentifier(NDataProcessing::EUniversallyUniqueIdentifierGenerate_Random).f_GetAsString
				(
					NDataProcessing::EUniversallyUniqueIdentifierFormat_AlphaNum
				)
			;
			
			Class pMenuContextClass = objc_allocateClassPair([NSObject class], ClassName.f_GetStr(), 0);
			
			CStr Types = CStr::CFormat("{}{}{}{}") << @encode(id) << @encode(id) << @encode(SEL) << @encode(id);
			
			class_addMethod(pMenuContextClass, @selector(doPauseService:), (IMP)fgs_PauseService, Types.f_GetStr());
			class_addMethod(pMenuContextClass, @selector(doResumeService:), (IMP)fgs_ResumeService, Types.f_GetStr());
			class_addMethod(pMenuContextClass, @selector(doQuitService:), (IMP)fgs_QuitService, Types.f_GetStr());
			class_addMethod(pMenuContextClass, @selector(doCancelService:), (IMP)fgs_CancelService, Types.f_GetStr());
			
			class_addIvar(pMenuContextClass, "m_pContext", sizeof(void *), rint(log2(sizeof(void *))), @encode(void *));
			
			objc_registerClassPair(pMenuContextClass);
			
			void* pMenuContextObject = [[pMenuContextClass alloc] init];
			object_setInstanceVariable((id)pMenuContextObject, "m_pContext", &MenuContext);
			
			gs_pSerivceApplication = [NSApplication sharedApplication];
			auto Cleanup = g_OnScopeExit > [&]
				{
					gs_pSerivceApplication = nullptr;
				}
			;
			
			NSMenu* pMenu = [[NSMenu allocWithZone:[NSMenu menuZone]] initWithTitle:@""];

			{
				NSMenuItem* pNameItem = [[NSMenuItem allocWithZone:[NSMenu menuZone]] initWithTitle:NPlatform::fg_MaxOSX_GetString(_ServiceName) action:NULL keyEquivalent:@""];
				[pNameItem setEnabled:false];
				[pMenu addItem:pNameItem];
			}
			{
				NStr::CStr ProgramPath = NFile::CFile::fs_GetProgramPath();
				NSMenuItem* pNameItem = [[NSMenuItem allocWithZone:[NSMenu menuZone]] initWithTitle:NPlatform::fg_MaxOSX_GetString(ProgramPath) action:NULL keyEquivalent:@""];
				[pNameItem setEnabled:false];
				[pMenu addItem:pNameItem];
			}
			
			[pMenu addItem:[NSMenuItem separatorItem]];
			
			NSMenuItem* pPauseItem = [[NSMenuItem allocWithZone:[NSMenu menuZone]] initWithTitle:@"Pause" action:NULL keyEquivalent:@""];
			[pPauseItem setTarget:(id)pMenuContextObject];
			[pPauseItem setAction:@selector(doPauseService:)];
			[pMenu addItem:pPauseItem];
			
			NSMenuItem* pResumeItem = [[NSMenuItem allocWithZone:[NSMenu menuZone]] initWithTitle:@"Resume" action:NULL keyEquivalent:@""];
			[pResumeItem setTarget:(id)pMenuContextObject];
			[pResumeItem setAction:@selector(doResumeService:)];
			[pMenu addItem:pResumeItem];
			
			NSMenuItem* pQuitItem = [[NSMenuItem allocWithZone:[NSMenu menuZone]] initWithTitle:@"Quit" action:NULL keyEquivalent:@""];
			[pQuitItem setTarget:(id)pMenuContextObject];
			[pQuitItem setAction:@selector(doQuitService:)];
			[pMenu addItem:pQuitItem];
			
			[pMenu addItem:[NSMenuItem separatorItem]];
			
			NSMenuItem* pCancelItem = [[NSMenuItem allocWithZone:[NSMenu menuZone]] initWithTitle:@"Cancel" action:NULL keyEquivalent:@""];
			[pCancelItem setTarget:(id)pMenuContextObject];
			[pCancelItem setAction:@selector(doCancelService:)];
			[pMenu addItem:pCancelItem];
			
			NSStatusItem* pStatusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
			
			if (!_IconData.f_IsEmpty())
			{
				NSData* pImageData = [NSData dataWithBytes:_IconData.f_GetArray() length:_IconData.f_GetLen()];
				NSImage* pImage = [[NSImage alloc] initWithData:pImageData];
				
				[pStatusItem setImage: pImage];
			}
			
			if (![pStatusItem image])
				[pStatusItem setTitle:@"MD"];
			
			[pStatusItem setHighlightMode:YES];
			[pStatusItem setMenu:pMenu];
			
			[NSApp run];
			
			objc_disposeClassPair(pMenuContextClass);
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
				: m_RunLoop(nullptr)
				, m_Thread(nullptr)
				, m_pDispatchObject(nullptr)
				, m_pDispatchObjectClass(nullptr)
			{
			}
			NSRunLoop *m_RunLoop;
			NSThread *m_Thread;
			id m_pDispatchObject;
			Class m_pDispatchObjectClass;
		};
		
		using namespace NFile;
		CFileChangeNoticationContext::CFileChangeNoticationContext()
			: m_DispatchObjectClassName()
			, m_pInternal(fg_Construct()) 
		{
			m_DispatchObjectClassName
				= "Dispatcher_" + NDataProcessing::CUniversallyUniqueIdentifier(NDataProcessing::EUniversallyUniqueIdentifierGenerate_Random)
				.f_GetAsString(NDataProcessing::EUniversallyUniqueIdentifierFormat_AlphaNum)
			;
		}
		
		static id fg_DoDispatch(id self, SEL _cmd, id obj)
		{
			CFileChangeNoticationContext *pContext;
			object_getInstanceVariable(self, "m_pContext", (void **)&pContext);
			
			while (auto ToDispatch = pContext->m_DispatchQueue.f_Pop())
				(*ToDispatch)();
			return nil;
		}
		
		CFileChangeNoticationContext::~CFileChangeNoticationContext()
		{
			auto &Internal = *m_pInternal;
			if (m_pProcessThread)
			{
				m_pProcessThread->f_Stop(false);
				{
					DMibLock(m_RunLoopLock);
					if (Internal.m_RunLoop)
						CFRunLoopStop([Internal.m_RunLoop getCFRunLoop]);
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
				NPtr::TCSharedPointer<CNotification> pNotification = fg_Explicit((CNotification *)pPop);
				pNotification->f_RefCountDecrease(DMibRefcountDebuggingOnly(pNotification->m_DebugSelfRef));
			}
			
			if (Internal.m_pDispatchObject)
				 [Internal.m_pDispatchObject release];
			
			if (Internal.m_pDispatchObjectClass)
				objc_disposeClassPair(Internal.m_pDispatchObjectClass);
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
					_SnapshotsByNode[NewChild.f_GetKey()].f_Insert(NewChild);
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
			
			if (m_Link.f_IsInList())
			{
				DMibLock(m_pContext->m_Lock);
				m_pContext->m_OpenNotifications.f_Remove(this);
			}
			
			m_RootSnapshot.f_Clear(m_SnapshotsByNode);
		}
		
		void fg_LinkFileSnapshot
			(
				CFileChangeNoticationContext::CNotification::CSnapshotsByNode &_SnapshotsByNode
				, CFileChangeNoticationContext::CNotification::CFileSnapshot &_Snapshot
			)
		{
			auto FileKey = _Snapshot.f_GetKey();

			if (_Snapshot.m_LinkNode.f_IsInList())
				_Snapshot.f_RemoveFromNodeMap(_SnapshotsByNode);
			
			_Snapshot.m_bDelete = false;
			
			_SnapshotsByNode[FileKey].f_Insert(_Snapshot);
		}
		
		void fgr_UpdateFileSnapshot
			(
				CFileChangeNoticationContext::CNotification::CSnapshotsByNode &_SnapshotsByNode
				, CFileChangeNoticationContext::CNotification::CFileSnapshot &_Snapshot
				, CStr const &_RootPath
				, bool _bRecursive
				, bool _bRecursiveInfoNeeded
			)
		{
			CStr Path = CFile::fs_AppendPath(_RootPath, _Snapshot.m_FullFileName);
			
			if (_Snapshot.m_Stats.st_dev == 0)
			{
				_Snapshot.f_Clear(_SnapshotsByNode);
				return;
			}

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
						Child.f_RemoveFromNodeMap(_SnapshotsByNode);
					
					Child.m_bDelete = false;
					
					Child.m_FileName = FileName;
					Child.m_FullFileName = CFile::fs_AppendPath(_Snapshot.m_FullFileName, FileName);
					Child.m_Stats = Stats;
					
					// Remove other child
					if (auto pChildByName = _Snapshot.m_ChildrenByName.f_FindEqual(FileName))
						_Snapshot.m_ChildrenByName.f_Remove(pChildByName);

					_Snapshot.m_ChildrenByName.f_Insert(Child);
					
					fg_LinkFileSnapshot(_SnapshotsByNode, Child);
					
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
						for (auto iSnap = Child.m_Children.f_GetIterator(); iSnap; ++iSnap)
							iSnap->m_bDelete = true;
						
						fgr_UpdateFileSnapshot(_SnapshotsByNode, Child, _RootPath, _bRecursive, _bRecursiveInfoNeeded);

						for (auto iSnap = Child.m_Children.f_GetIterator(); iSnap;)
						{
							if (iSnap->m_bDelete)
							{
								if (iSnap->m_Link.f_IsInTree())
									Child.m_ChildrenByName.f_Remove(*iSnap);
								if (iSnap->m_LinkNode.f_IsInList())
									iSnap->f_RemoveFromNodeMap(_SnapshotsByNode);
								iSnap.f_Remove();
								continue;
							}
							++iSnap;
						}
					}
				}
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
				if (_Type == EFileChangeNotification_Removed || _Type == EFileChangeNotification_Added || _Type == EFileChangeNotification_Renamed)
					o_Context.m_ChangesFileName.f_Insert(fg_Move(Change));
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
			if (_NewSnapshot.m_Stats.st_dev == 0)
			{
				m_RootSnapshot.f_PotentiallyRemoved(o_Context);
				return;
			}
			
			CFileSnapshot const *pOldSnapshot = nullptr;

			if (auto const *pOldSnapshots = m_SnapshotsByNode.f_FindEqual(_NewSnapshot.f_GetKey()))
			{
				for (auto &OldSnapshot : *pOldSnapshots)
				{
					if (OldSnapshot.m_FullFileName == _NewSnapshot.m_FullFileName)
					{
						pOldSnapshot = &OldSnapshot;
						break;
					}
				}
				if (!pOldSnapshot)
				{
					for (auto &OldSnapshot : *pOldSnapshots)
					{
						if (OldSnapshot.m_FileName == _NewSnapshot.m_FileName)
						{
							pOldSnapshot = &OldSnapshot;
							break;
						}
					}
				}
				if (!pOldSnapshot)
					pOldSnapshot = pOldSnapshots->f_GetFirst();
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
				, CFileChangeNoticationContext::CNotification::CSnapshotsByNode &o_NewSnapshotsByNode
				, CFileSnapshot &o_NewSnapshot
				, TCMap<CStr, zbool> &o_ChangedPaths
			)
		{
			bool bRecursive = (m_Flags & EFileChange_Recursive) != 0;
			
			if (!_Path.f_StartsWith(m_NotificationPath))
			{
				DMibTrace("'{}' does not start with '{}'\n", _Path << m_NotificationPath);
				DMibSafeCheck(false, "File path not correct!!");
				return; // Error
			}
			
			if (!bRecursive && _Path != m_NotificationPath)
				return; // Drop this notification as we are not interested in it
			
			if (!_bNeedSubDirs && bRecursive)
				bRecursive = false;
			
			CStr RelativePath = NMib::NFile::CFile::fs_MakePathRelative(_Path, m_NotificationPath);
			
			if (RelativePath == _Path)
			{
				DMibSafeCheck(false, "Failed to make relative path!!");
				return; // Error
			}
			
			if (!_bInitial && (_Path != m_NotificationPath || (bRecursive != (m_Flags & EFileChange_Recursive) != 0)))
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
					auto &bChangedPathRecursive = o_ChangedPaths[pFindSnapshot->m_FullFileName];
					if (bRecursive)
						bChangedPathRecursive = true;
					// We can do a partial update
					if (bRecursive)
						pFindSnapshot->f_Clear(o_NewSnapshotsByNode);
					else
					{
						for (auto iSnap = pFindSnapshot->m_Children.f_GetIterator(); iSnap; ++iSnap)
							iSnap->m_bDelete = true;
					}
					pFindSnapshot->f_RemoveFromNodeMap(o_NewSnapshotsByNode);
					if (lstat(CFile::fs_AppendPath(m_NotificationPath, pFindSnapshot->m_FullFileName).f_GetStr(), &pFindSnapshot->m_Stats))
						NMem::fg_MemClear(pFindSnapshot->m_Stats);
					fg_LinkFileSnapshot(o_NewSnapshotsByNode, *pFindSnapshot);
					fgr_UpdateFileSnapshot(o_NewSnapshotsByNode, *pFindSnapshot, m_NotificationPath, bRecursive, (m_Flags & EFileChange_Recursive) != 0);
					
					for (auto iSnap = pFindSnapshot->m_Children.f_GetIterator(); iSnap;)
					{
						if (iSnap->m_bDelete)
						{
							if (iSnap->m_Link.f_IsInTree())
								pFindSnapshot->m_ChildrenByName.f_Remove(*iSnap);
							if (iSnap->m_LinkNode.f_IsInList())
								iSnap->f_RemoveFromNodeMap(o_NewSnapshotsByNode);
							iSnap.f_Remove();
							continue;
						}
						++iSnap;
					}
				}
				else
				{
					// We have to do a full update
					o_NewSnapshot.f_RemoveFromNodeMap(o_NewSnapshotsByNode);
					if (lstat(m_NotificationPath.f_GetStr(), &o_NewSnapshot.m_Stats))
						NMem::fg_MemClear(o_NewSnapshot.m_Stats);
					fg_LinkFileSnapshot(o_NewSnapshotsByNode, o_NewSnapshot);
					fgr_UpdateFileSnapshot(o_NewSnapshotsByNode, o_NewSnapshot, m_NotificationPath, bRecursive, (m_Flags & EFileChange_Recursive) != 0);
					auto &bChangedPathRecursive = o_ChangedPaths[""];
					if (bRecursive)
						bChangedPathRecursive = true;
				}
				
				//fgr_TraceSnapshot(NewSnapshot, 0);
			}
			else
			{
				o_NewSnapshot.f_RemoveFromNodeMap(o_NewSnapshotsByNode);
				if (lstat(m_NotificationPath.f_GetStr(), &o_NewSnapshot.m_Stats))
					NMem::fg_MemClear(o_NewSnapshot.m_Stats);
				fg_LinkFileSnapshot(o_NewSnapshotsByNode, o_NewSnapshot);
				fgr_UpdateFileSnapshot(o_NewSnapshotsByNode, o_NewSnapshot, m_NotificationPath, bRecursive, (m_Flags & EFileChange_Recursive) != 0);
				auto &bChangedPathRecursive = o_ChangedPaths[""];
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
			TCMap<CStr, zbool> ChangedPaths;
			
			for (mint i = 0; i < _nEvents; ++i)
			{
				CStr Path(_pPaths[i]);
				if (Path.f_GetAt(Path.f_GetLen() - 1) == '/')
					Path = Path.f_Left(Path.f_GetLen() - 1);
				
				FSEventStreamEventFlags Flags = _Flags[i];
				
				f_ScanDir(Path, _bInitialScan, (Flags & kFSEventStreamEventFlagMustScanSubDirs) != 0, NewSnapshotsByNode, NewSnapshot, ChangedPaths);
			}
			
			fg_LinkFileSnapshot(NewSnapshotsByNode, NewSnapshot);

			if (!_bInitialScan)
			{
				if (ChangedPaths.f_IsEmpty())
				{
					NewSnapshotsByNode.f_Clear();
					return;
				}

				CFindChangesContext FindChangesContext;
				
				for (auto &bChangedPathRecursive : ChangedPaths)
				{
					CStr const &ChangedPath = ChangedPaths.fs_GetKey(bChangedPathRecursive);
					CFileSnapshot *pFindSnapshot = &NewSnapshot;
					bool bRecursiveSetting = (m_Flags & EFileChange_Recursive) != 0;
					{
						CStr FindPath = ChangedPath;
						while (pFindSnapshot && !FindPath.f_IsEmpty())
						{
							CStr Path = fg_GetStrSep(FindPath, "/");
							pFindSnapshot = pFindSnapshot->m_ChildrenByName.f_FindEqual(Path);
						}
					}
					if (pFindSnapshot)
						fr_FindChanges(FindChangesContext, *pFindSnapshot, bChangedPathRecursive, bRecursiveSetting, true);
					else
						fr_FindChanges(FindChangesContext, NewSnapshot, bRecursiveSetting, bRecursiveSetting, true);
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
							if (!pSnapshots)
								continue;
							for (auto &Snapshot : *pSnapshots)
								f_AddNotification(FindChangesContext, EFileChangeNotification_Removed, Snapshot.m_FullFileName);
						}
					}
				}
				
				if (!FindChangesContext.m_ChangesFileName.f_IsEmpty() || !FindChangesContext.m_Changes.f_IsEmpty())
				{
					DMibLock(m_ChangesLock);
					m_Changes.f_Insert(fg_Move(FindChangesContext.m_ChangesFileName));
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
			fg_LinkFileSnapshot(m_SnapshotsByNode, m_RootSnapshot);
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
					//DMibConOut2("IGNORE Change: {} = {nfh} - {}\n", EventPath, Flags, _IDs[i]);
					continue;
				}

				CStr RelativePath = NMib::NFile::CFile::fs_MakePathRelative(EventPath, m_NotificationPath);

				//DMibConOut2("Change: {} = {nfh} - {}\n", EventPath, Flags, _IDs[i]);
				
				bool bIsDir = (Flags & kFSEventStreamEventFlagItemIsDir) && !(Flags & kFSEventStreamEventFlagItemIsSymlink);
				
				if (m_Flags & NFile::EFileChange_Recursive)
				{
					if ((Flags & kFSEventStreamEventFlagItemCreated) && bIsDir)
					{
						CStr ToFind = fg_Format("{}/*", EventPath);
						for (auto &File : NFile::CFile::fs_FindFilesEx(ToFind, NFile::EFileAttrib_File | NFile::EFileAttrib_Directory, true, false))
						{
							CStr RelativePath = NMib::NFile::CFile::fs_MakePathRelative(File.m_Path, m_NotificationPath);
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
			
			//DMibConOut2("m_RenamedFromQueue {}\n", m_RenamedFromQueue.f_GetLen());
			
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
			
			//DMibConOut2("GENERATED {}\n", FindChangesContext.m_ChangesFileName.f_GetLen() + FindChangesContext.m_Changes.f_GetLen());
			
			if (!FindChangesContext.m_ChangesFileName.f_IsEmpty() || !FindChangesContext.m_Changes.f_IsEmpty())
			{
				DMibLock(m_ChangesLock);
				m_Changes.f_Insert(fg_Move(FindChangesContext.m_ChangesFileName));
				m_Changes.f_Insert(fg_Move(FindChangesContext.m_Changes));
				if (m_pReportTo)
					m_pReportTo->f_Signal();
			}
		}

		void CFileChangeNoticationContext::CNotification::f_InitialScan()
		{
			ch8 const *Paths[1] = {m_NotificationPath.f_GetStr()};
			FSEventStreamEventFlags const Flags[1] = {kFSEventStreamEventFlagMustScanSubDirs};
			FSEventStreamEventId const IDs[1] = {1};
			
			f_ProcessChanges(1, Paths, Flags, IDs, true);
		}
		
		void CFileChangeNoticationContext::CNotification::f_FullRescan()
		{
			ch8 const *Paths[1] = {m_NotificationPath.f_GetStr()};
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
				if (Internal.m_RunLoop)
					[Internal.m_pDispatchObject performSelector:@selector(doDispatch:) onThread:Internal.m_Thread withObject:nil waitUntilDone:false];
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
			
			auto &Internal = *m_pInternal;
			
			CAutoReleasePool ARPool;
			
			Class pDispatcherClass = objc_allocateClassPair([NSObject class], m_DispatchObjectClassName.f_GetStr(), 0);
			
			Internal.m_pDispatchObjectClass = pDispatcherClass;

			CStr Types = CStr::CFormat("{}{}{}{}") << @encode(id) << @encode(id) << @encode(SEL) << @encode(id);
			
			class_addMethod(pDispatcherClass, @selector(doDispatch:), (IMP)fg_DoDispatch, Types.f_GetStr());
			
			class_addIvar(pDispatcherClass, "m_pContext", sizeof(void *), rint(log2(sizeof(void *))), @encode(void *));
			
			objc_registerClassPair(pDispatcherClass);
			
			Internal.m_pDispatchObject = [[pDispatcherClass alloc] init];
			
			object_setInstanceVariable((id)Internal.m_pDispatchObject, "m_pContext", this);
			
			m_pProcessThread
				= NThread::CThreadObject::fs_StartThread
				(
					[this](NThread::CThreadObject *_pThread) -> aint
					{
						auto &Internal = *m_pInternal;
						CAutoReleasePool ARPool;
						NSPort *pPort = [NSMachPort port];
						{
							DMibLock(m_RunLoopLock);
							Internal.m_RunLoop = [NSRunLoop currentRunLoop];
							Internal.m_Thread = [NSThread currentThread];
							[Internal.m_RunLoop addPort:pPort forMode:NSDefaultRunLoopMode]; // Dummy port so run loop does not quit at once
							[Internal.m_pDispatchObject performSelector:@selector(doDispatch:) onThread:Internal.m_Thread withObject:nil waitUntilDone:false];
						}

						
						while (_pThread->f_GetState() != NThread::EThreadState_EventWantQuit)
						{
							CFRunLoopRun();
						}
						{
							DMibLock(m_RunLoopLock);
							[Internal.m_RunLoop removePort:pPort forMode:NSDefaultRunLoopMode];
							
							Internal.m_RunLoop = nullptr;
							Internal.m_Thread = nullptr;
						}
						
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
		
		void *CFileChangeNoticationContext::f_Open(const CStr &_FileName, NMib::NFile::EFileChange _OpenFlags, NMib::NThread::CSemaphoreReportableAggregate *_pReportTo)
		{
			CStr NotificationPath = _FileName;
			if (NMib::NFile::CFile::fs_FileExists(NotificationPath, EFileAttrib_File))
				DMibErrorFile("Open file notification: '{}' is not a directory");
			
			if (!NMib::NFile::CFile::fs_FileExists(NotificationPath, EFileAttrib_Directory))
				DMibErrorFile("Open file notification: Directory '{}' does not exist");
			
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
			
			NPtr::TCSharedPointer<CNotification> pNotification = fg_Construct(this);
			pNotification->m_NotificationPath = NotificationPath;

			FSEventStreamContext CallbackContext;
			CallbackContext.version = 0;
			CallbackContext.info = pNotification.f_Get();
			CallbackContext.retain = nullptr;
			CallbackContext.release = nullptr;
			CallbackContext.copyDescription = nullptr;
			
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
					, kFSEventStreamCreateFlagNoDefer | kFSEventStreamCreateFlagWatchRoot | (pNotification->m_bPerFileEvents ? kFSEventStreamCreateFlagFileEvents : 0)
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
						FSEventStreamScheduleWithRunLoop(pNotification->m_pEventStream, [Internal.m_RunLoop getCFRunLoop], kCFRunLoopDefaultMode);
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
			NPtr::TCSharedPointer<CNotification> pNotification = fg_Explicit((CNotification *)_pNotification);
			
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
		
		bint CFileChangeNoticationContext::f_Changed(void *_pNotification)
		{
			DMibLock(m_Lock);
			CNotification *pNotification = (CNotification *)_pNotification;
			bint bChanged = false;
			{
				DMibLock(pNotification->m_ChangesLock);
				bChanged = !pNotification->m_Changes.f_IsEmpty();
				pNotification->m_Changes.f_Clear();
			}
			return bChanged;
		}

		bint CFileChangeNoticationContext::f_GetNotification(void *_pNotification, CStr &_Path, NFile::EFileChangeNotification &_Notification, CStr &_PathFrom)
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
