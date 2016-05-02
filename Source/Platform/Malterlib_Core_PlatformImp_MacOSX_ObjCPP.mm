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
					
		void fg_RunServiceStatusApp(NFunction::TCFunction<void ()> const& _fPause, NFunction::TCFunction<void ()> const& _fResume, NStr::CStr const &_ServiceName, NContainer::TCVector<uint8> const& _IconData)
		{
			CAutoReleasePool ARPool;
			
			CMenuContext MenuContext;
			MenuContext.m_fPause = _fPause;
			MenuContext.m_fResume = _fResume;
			
			NStr::CStr ClassName
			= "MenuContext_" + NDataProcessing::CUniversallyUniqueIdentifier(NDataProcessing::EUniversallyUniqueIdentifierGenerate_Random)
			.f_GetAsString(NDataProcessing::EUniversallyUniqueIdentifierFormat_AlphaNum)
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
			
			[NSApplication sharedApplication];
			
			NSMenu* pMenu = [[NSMenu allocWithZone:[NSMenu menuZone]] initWithTitle:@""];
			
			NSMenuItem* pNameItem = [[NSMenuItem allocWithZone:[NSMenu menuZone]] initWithTitle:NPlatform::fg_MaxOSX_GetString(_ServiceName) action:NULL keyEquivalent:@""];
			[pNameItem setEnabled:false];
			[pMenu addItem:pNameItem];
			
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
				pNotification->f_RefCountDecrease();
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
		
		CFileChangeNoticationContext::CNotification::CFileSnapshot::CFileSnapshot(CFileSnapshot const &_Other, CFileSnapshot *_pParent)
			: m_pParent(_pParent)
			, m_FileName(_Other.m_FileName)
			, m_Stats(_Other.m_Stats)
		{
			for (auto iChild = _Other.m_Children.f_GetIterator(); iChild; ++iChild)
			{
				auto Mapped = m_Children(iChild.f_GetKey(), *iChild, this);
				if (iChild->m_Link.f_IsInTree())
					m_ChildrenByName.f_Insert(*Mapped);
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
			
			m_Changes.f_DeleteAll();
			
			if (m_Link.f_IsInList())
			{
				DMibLock(m_pContext->m_Lock);
				m_pContext->m_OpenNotifications.f_Remove(this);
			}
		}
		
		void fgr_UpdateFileSnapshot(CFileChangeNoticationContext::CNotification::CFileSnapshot &_Snapshot, CStr const &_Path, CStr const &_RootPath, bool _bRecursive, bool _bRecursiveInfoNeeded)
		{
			CStr Path = CFile::fs_AppendPath(_RootPath, _Path);
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
					
					auto Mapped = _Snapshot.m_Children(CFileChangeNoticationContext::CNotification::CFileKey(Stats), &_Snapshot);
					auto &Child = *Mapped;
					
					// Remove ourselves
					if (Child.m_Link.f_IsInTree())
						_Snapshot.m_ChildrenByName.f_Remove(Child);
					
					Child.m_bDelete = false;
					
					Child.m_FileName = FileName;
					Child.m_Stats = Stats;
					
					// Remove other child
					if (auto pChildByName = _Snapshot.m_ChildrenByName.f_FindEqual(FileName))
						_Snapshot.m_ChildrenByName.f_Remove(pChildByName);
					
					_Snapshot.m_ChildrenByName.f_Insert(Child);
					
					if 
						(
							(Stats.st_mode & S_IFDIR)
							&&
							!(Stats.st_mode & S_IFLNK)
							&& 
							(
								_bRecursive
								|| (_bRecursiveInfoNeeded && Mapped.f_WasCreated())
							)
						)
					{
						for (auto iSnap = Child.m_Children.f_GetIterator(); iSnap; ++iSnap)
							iSnap->m_bDelete = true;
						
						fgr_UpdateFileSnapshot(Child, CFile::fs_AppendPath(_Path, FileName), _RootPath, _bRecursive, _bRecursiveInfoNeeded);

						for (auto iSnap = Child.m_Children.f_GetIterator(); iSnap;)
						{
							if (iSnap->m_bDelete)
							{
								if (iSnap->m_Link.f_IsInTree())
									Child.m_ChildrenByName.f_Remove(*iSnap);
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

		void CFileChangeNoticationContext::CNotification::f_AddNotification(EFileChangeNotification _Type, CStr const &_RelativePath)
		{
			auto pChange = DMibNew CChange;
			pChange->m_Notification = _Type;
			pChange->m_Path = _RelativePath;

			{
				DMibLock(m_ChangesLock);
				m_Changes.f_Insert(pChange);
				if (m_pReportTo)
					m_pReportTo->f_Signal();
			}
		}

		void CFileChangeNoticationContext::CNotification::fr_FindChanges(CFileSnapshot const &_OldSnapshot, CFileSnapshot const &_NewSnapshot, bool _bRecursive, CStr const &_Path)
		{
			for (auto iChild = _OldSnapshot.m_Children.f_GetIterator(); iChild; ++iChild)
			{
				auto pNew = _NewSnapshot.m_Children.f_FindEqual(iChild.f_GetKey());
				auto pOld = &*iChild;
				if (!pNew)
				{
					bool bIsDir = (pOld->m_Stats.st_mode & S_IFDIR) && !(pOld->m_Stats.st_mode & S_IFLNK);
					if
						(
						 (bIsDir && (m_Flags & EFileChange_DirectoryName))
						 || (!bIsDir && (m_Flags & EFileChange_FileName))
						 )
					{
						f_AddNotification(EFileChangeNotification_Removed, CFile::fs_AppendPath(_Path, pOld->m_FileName));
					}
				}
			}
			for (auto iChild = _NewSnapshot.m_Children.f_GetIterator(); iChild; ++iChild)
			{
				auto pNew = &*iChild;
				auto pOld = _OldSnapshot.m_Children.f_FindEqual(iChild.f_GetKey());
				bool bIsDir = (pNew->m_Stats.st_mode & S_IFDIR) && !(pNew->m_Stats.st_mode & S_IFLNK);
				if (pOld)
				{
					// Compare
					
					if 
						(
							(bIsDir && (m_Flags & EFileChange_DirectoryName))
							|| (!bIsDir && (m_Flags & EFileChange_FileName))
						)
					{
						if (pNew->m_FileName != pOld->m_FileName)
						{
							f_AddNotification(EFileChangeNotification_RenamedFrom, CFile::fs_AppendPath(_Path, pOld->m_FileName));
							f_AddNotification(EFileChangeNotification_RenamedTo, CFile::fs_AppendPath(_Path, pNew->m_FileName));
						}
					}
					
					bool bModified = false;
					
					if (m_Flags & EFileChange_Attributes)
					{
						if (pNew->m_Stats.st_flags != pOld->m_Stats.st_flags)
							bModified = true;
					}
					
					if (m_Flags & EFileChange_FileSize)
					{
						if (pNew->m_Stats.st_size != pOld->m_Stats.st_size)
							bModified = true;						
					}
					
					if (m_Flags & EFileChange_Write)
					{
						if 
							(
								pNew->m_Stats.st_mtimespec.tv_sec != pOld->m_Stats.st_mtimespec.tv_sec
								|| pNew->m_Stats.st_mtimespec.tv_nsec != pOld->m_Stats.st_mtimespec.tv_nsec
							)
						{
							bModified = true;
						}
					}
					
					if (m_Flags & EFileChange_Security)
					{
						if 
							(
								pNew->m_Stats.st_mode != pOld->m_Stats.st_mode
								|| pNew->m_Stats.st_uid != pOld->m_Stats.st_uid
								|| pNew->m_Stats.st_gid != pOld->m_Stats.st_gid
							)
						{
							bModified = true;
						}
						
					}

					if (bModified)
						f_AddNotification(EFileChangeNotification_Modified, CFile::fs_AppendPath(_Path, pNew->m_FileName));
					
					if (_bRecursive && (!pNew->m_Children.f_IsEmpty() || !pOld->m_Children.f_IsEmpty()))
						fr_FindChanges(*pOld, *pNew, _bRecursive, CFile::fs_AppendPath(_Path, pNew->m_FileName));
				}
				else
				{
					if
						(
							(bIsDir && (m_Flags & EFileChange_DirectoryName))
							|| (!bIsDir && (m_Flags & EFileChange_FileName))
						)
					{
						f_AddNotification(EFileChangeNotification_Added, CFile::fs_AppendPath(_Path, pNew->m_FileName));
					}
				}
			}
		}

		void CFileChangeNoticationContext::CNotification::f_ScanDir(CStr const &_Path, bool _bInitial, bool _bNeedSubDirs)
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
				CFileSnapshot NewSnapshot{m_RootSnapshot, nullptr};
				
				CFileSnapshot *pFindSnapshot = &NewSnapshot;
				CFileSnapshot *pFindSnapshotOld = &m_RootSnapshot;
				{
					CStr FindPath = RelativePath;
					while (pFindSnapshot && !FindPath.f_IsEmpty())
					{
						CStr Path = fg_GetStrSep(FindPath, "/");
						pFindSnapshot = pFindSnapshot->m_ChildrenByName.f_FindEqual(Path);
						pFindSnapshotOld = pFindSnapshotOld->m_ChildrenByName.f_FindEqual(Path);
					}
				}
				
				if (pFindSnapshot)
				{
					// We can do a partial update
					if (bRecursive)
					{
						pFindSnapshot->m_ChildrenByName.f_Clear();
						pFindSnapshot->m_Children.f_Clear();
					}
					else
					{
						for (auto iSnap = pFindSnapshot->m_Children.f_GetIterator(); iSnap; ++iSnap)
							iSnap->m_bDelete = true;
					}
					fgr_UpdateFileSnapshot(*pFindSnapshot, RelativePath, m_NotificationPath, bRecursive, (m_Flags & EFileChange_Recursive) != 0);
					
					for (auto iSnap = pFindSnapshot->m_Children.f_GetIterator(); iSnap;)
					{
						if (iSnap->m_bDelete)
						{
							if (iSnap->m_Link.f_IsInTree())
								pFindSnapshot->m_ChildrenByName.f_Remove(*iSnap);
							iSnap.f_Remove();
							continue;
						}
						++iSnap;
					}
					fr_FindChanges(*pFindSnapshotOld, *pFindSnapshot, bRecursive, RelativePath);
				}
				else
				{
					// We have to do a full update
					fgr_UpdateFileSnapshot(NewSnapshot, CStr(), m_NotificationPath, bRecursive, (m_Flags & EFileChange_Recursive) != 0);
					fr_FindChanges(m_RootSnapshot, NewSnapshot, bRecursive, CStr());
				}
				
				//fgr_TraceSnapshot(NewSnapshot, 0);
				m_RootSnapshot.m_ChildrenByName = fg_Move(NewSnapshot.m_ChildrenByName);
				m_RootSnapshot.m_Children = fg_Move(NewSnapshot.m_Children);
			}
			else
			{
				CFileSnapshot NewSnapshot{nullptr};
				fgr_UpdateFileSnapshot(NewSnapshot, CStr(), m_NotificationPath, bRecursive, (m_Flags & EFileChange_Recursive) != 0);
				if (!_bInitial)
					fr_FindChanges(m_RootSnapshot, NewSnapshot, bRecursive, CStr());
				//fgr_TraceSnapshot(NewSnapshot, 0);
				m_RootSnapshot.m_ChildrenByName = fg_Move(NewSnapshot.m_ChildrenByName);
				m_RootSnapshot.m_Children = fg_Move(NewSnapshot.m_Children);
			}
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
			CNotification *pNotification = (CNotification *)clientCallBackInfo;
			ch8 const **pPaths = (ch8 const **)eventPaths;
			for (mint i = 0; i < numEvents; ++i)
			{
				CStr Path(pPaths[i]);
				if (Path.f_GetAt(Path.f_GetLen() - 1) == '/')
					Path = Path.f_Left(Path.f_GetLen() - 1);
				
				FSEventStreamEventFlags Flags = eventFlags[i];
				//FSEventStreamEventId EventID = eventIDs[i];
				pNotification->f_ScanDir(Path, false, (Flags & kFSEventStreamEventFlagMustScanSubDirs) != 0);
			}
		}
		
		void CFileChangeNoticationContext::f_DispatchOnThread(NMib::NFunction::TCFunction<void (NFunction::CThisTag &)> const &_Dispatch)
		{
			auto &Internal = *m_pInternal;
			m_DispatchQueue.f_Push(_Dispatch);
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
				)
			;
			if (!pStream)
				DMibErrorFile("Open file notification: FSEventStreamCreate failed");
			
			pNotification->m_pEventStream = pStream;
			{
				DMibLock(m_Lock);
				m_OpenNotifications.f_Insert(pNotification.f_Get());
			}
			pNotification->f_RefCountIncrease();

			pNotification->m_Flags = _OpenFlags;
			pNotification->m_pReportTo = _pReportTo;

			// Scan full dir in this call so we don't miss any notifications
			pNotification->f_ScanDir(pNotification->m_NotificationPath, true, true);
			
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
						pNotification->f_ScanDir(pNotification->m_NotificationPath, false, true);
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
			
			pNotification->f_RefCountDecrease();
			{
				DMibLock(pNotification->m_ChangesLock);
				pNotification->m_pReportTo = nullptr;
			}
			
			f_DispatchOnThread
				(
					[this, pNotification]()
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
				pNotification->m_Changes.f_DeleteAll();
			}
			return bChanged;
		}
		bint CFileChangeNoticationContext::f_GetNotification(void *_pNotification, CStr &_Path, NFile::EFileChangeNotification &_Notification)
		{
			DMibLock(m_Lock);
			CNotification *pNotification = (CNotification *)_pNotification;
			bint bChanged = false;
			{
				DMibLock(pNotification->m_ChangesLock);
				CNotification::CChange *pChange = pNotification->m_Changes.f_Pop();
				
				if (pChange)
				{
					_Path = pChange->m_Path;
					_Notification = pChange->m_Notification;
					bChanged = true;
					delete pChange;
				}
				else
					bChanged = false;
			}
			return bChanged;
		}
		
	}
	
	
} // Namespace NMib
