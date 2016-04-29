// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <CoreServices/CoreServices.h>
#include <Mib/Concurrency/ThreadSafeQueue>

#include <sys/stat.h>

namespace NMib
{
	namespace NOSXRuntime
	{
		using namespace NFile;
		class CFileChangeNoticationContext
		{
		public:
			
			CFileChangeNoticationContext();
			~CFileChangeNoticationContext();
			
			class CNotification : public NPtr::TCSharedPointerIntrusiveBase<>
			{
			public:
				CNotification(CFileChangeNoticationContext *_pContext);
				~CNotification();
				void f_Clear();
				
				DMibListLinkDS_Link(CNotification, m_Link);

				zbint m_bAddedToRunLoop;
				zbint m_bStreamStarted;
				FSEventStreamRef m_pEventStream;
				CFileChangeNoticationContext *m_pContext;
				NMib::NFile::EFileChange m_Flags;
				NMib::NStr::CStr m_NotificationPath;
				
				class CChange
				{
				public:
					CChange()
					{
						m_Notification = NFile::EFileChangeNotification_Undefined;
					}
					NFile::EFileChangeNotification m_Notification;
					NMib::NStr::CStr m_Path;
					DMibListLinkDS_Link(CChange, m_Link);
				};
				DMibListLinkDS_List(CChange, m_Link) m_Changes;
				NThread::CMutual m_ChangesLock;
				
				NMib::NThread::CSemaphoreReportableAggregate *m_pReportTo;
				
				struct CFileKey
				{
					decltype(stat::st_dev) st_dev;
					decltype(stat::st_ino) st_ino;
					bool operator < (CFileKey const &_Right) const
					{
						if (st_dev < _Right.st_dev)
							return true;
						else if (st_dev > _Right.st_dev)
							return false;
						return st_ino < _Right.st_ino;
					}
						
					CFileKey(struct stat const &_Init)
						: st_dev(_Init.st_dev)
						, st_ino(_Init.st_ino)
					{
					}
				};
						
				struct CFileSnapshot
				{
					CFileSnapshot(CFileSnapshot *_pParent);
					CFileSnapshot(CFileSnapshot const &_Other, CFileSnapshot *_pParent);
					
					class CCompare
					{
					public:
						inline_small NMib::NStr::CStr const &operator () (CFileSnapshot const &_Node) const
						{
							return _Node.m_FileName;
						}
					};
					
					// 
					struct stat m_Stats;
					
					NMib::NStr::CStr m_FileName;
					
					CFileSnapshot *m_pParent;
					zbool m_bDelete;
					
					DMibIntrusiveLink(CFileSnapshot, NMib::NIntrusive::TCAVLLink<>, m_Link);
					
					
					NContainer::TCMap<CFileKey, CFileSnapshot> m_Children;
					NMib::NIntrusive::TCAVLTree<CLinkTraits_m_Link, CCompare> m_ChildrenByName;
					
				};
				
				CFileSnapshot m_RootSnapshot;
				
				void f_AddNotification(EFileChangeNotification _Type, NStr::CStr const &_RelativePath);
				void fr_FindChanges(CFileSnapshot const &_OldSnapshot, CFileSnapshot const &_NewSnapshot, bool _bRecursive, NStr::CStr const &_Path);
				void f_ScanDir(NStr::CStr const &_Path, bool _bInitial, bool _bNeedSubDirs);

			};
			typedef DMibListLinkDS_Iter(CNotification, m_Link)  CNotificationIter;
			
			NThread::CMutual m_Lock;

			DMibListLinkDS_List(CNotification, m_Link) m_OpenNotifications;
			
			NMib::NPtr::TCUniquePointer<NMib::NThread::CThreadObject> m_pProcessThread;
			
			static void fs_EventCallback
				(
					ConstFSEventStreamRef streamRef
					, void *clientCallBackInfo
					, size_t numEvents
					, void *eventPaths
					, const FSEventStreamEventFlags eventFlags[]
					, const FSEventStreamEventId eventIDs[]
				)
			;
			
			struct CInternal;
			
			NPtr::TCUniquePointer<CInternal> m_pInternal;
			
			NMib::NThread::CMutual m_RunLoopLock;
			NStr::CStr m_DispatchObjectClassName;
			zbool m_bDestroying;
			
			NMib::NContainer::TCThreadSafeQueue<NMib::NFunction::TCFunction<void (NFunction::CThisTag &)>> m_DispatchQueue;
			
			void f_DispatchOnThread(NMib::NFunction::TCFunction<void (NFunction::CThisTag &)> const &_Dispatch);
			void f_StartThread();
			void *f_Open(const NMib::NStr::CStr &_FileName, NMib::NFile::EFileChange _OpenFlags, NMib::NThread::CSemaphoreReportableAggregate *_pReportTo);
			
			void f_Close(void *_pNotification);
			bint f_Changed(void *_pNotification);
			bint f_GetNotification(void *_pNotification, NMib::NStr::CStr &_Path, NFile::EFileChangeNotification &_Notification);
			
		};
	}


} // Namespace NMib
