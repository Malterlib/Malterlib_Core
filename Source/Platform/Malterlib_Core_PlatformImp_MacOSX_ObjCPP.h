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
			
			class CNotification : public NStorage::TCSharedPointerIntrusiveBase<>
			{
			public:
				CNotification(CFileChangeNoticationContext *_pContext);
				~CNotification();
				void f_Clear();
				
				
				class CChange
				{
				public:
					NFile::EFileChangeNotification m_Notification = NFile::EFileChangeNotification_Undefined;
					NMib::NStr::CStr m_Path;
					NMib::NStr::CStr m_PathFrom;
					
					bool operator < (CChange const &_Right) const
					{
						return NStorage::fg_TupleReferences(m_Notification, m_Path, m_PathFrom) < NStorage::fg_TupleReferences(_Right.m_Notification, _Right.m_Path, _Right.m_PathFrom);
					}
				};
				
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
						
				struct CFindChangesContext
				{
					NContainer::TCLinkedList<CChange> m_ChangesFileNameRename;
					NContainer::TCLinkedList<CChange> m_ChangesFileNameRemove;
					NContainer::TCLinkedList<CChange> m_ChangesFileNameAdd;
					NContainer::TCLinkedList<CChange> m_Changes;
					NContainer::TCSet<CChange> m_ChangesSet;
					NContainer::TCSet<CFileKey> m_UsedOld;
					NContainer::TCSet<CFileKey> m_PotentialOld;
				};

				struct CFileSnapshot
				{
					CFileSnapshot(CFileSnapshot *_pParent);
					
					class CCompare
					{
					public:
						inline_small NMib::NStr::CStr const &operator () (CFileSnapshot const &_Node) const
						{
							return _Node.m_FileName;
						}
					};
					
					CFileKey f_GetKey() const
					{
						return CFileKey(m_Stats);
					}
					
					// 
					struct stat m_Stats;
					
					NMib::NStr::CStr m_FileName;
					NMib::NStr::CStr m_FullFileName;
					
					CFileSnapshot *m_pParent;
					uint64 m_UpdateSequence = 0;
					bool m_bDelete = false;
					
					NMib::NIntrusive::TCAVLLink<> m_Link;
					DMibListLinkDS_Link(CFileSnapshot, m_LinkNode);
					
					NContainer::TCLinkedList<CFileSnapshot> m_Children;
					NMib::NIntrusive::TCAVLTree<&CFileSnapshot::m_Link, CCompare> m_ChildrenByName;

					CFileSnapshot
						(
							CFileSnapshot const &_Other
							, CFileSnapshot *_pParent
							, NContainer::TCMap<CFileKey, DMibListLinkDS_List(CFileSnapshot, m_LinkNode)> &_SnapshotsByNode
						)
					;
					void f_Clear(NContainer::TCMap<CFileKey, DMibListLinkDS_List(CFileSnapshot, m_LinkNode)> &_SnapshotsByNode);
					void f_RemoveFromNodeMap(NContainer::TCMap<CFileKey, DMibListLinkDS_List(CFileSnapshot, m_LinkNode)> &_Map);
					void f_PotentiallyRemoved(CFindChangesContext &o_Context) const;
				};

				using CSnapshotsByNode = NContainer::TCMap<CFileKey, DMibListLinkDS_List(CFileSnapshot, m_LinkNode)>;

				struct CUpdateSnapshotContext
				{
					CUpdateSnapshotContext
						(
							CSnapshotsByNode &_SnapshotsByNode
							, CSnapshotsByNode &_OldSnapshotsByNode
							, uint64 _UpdateSequence
						 	, NStr::CStr const &_NotificationPath
						)
						: m_SnapshotsByNode(_SnapshotsByNode)
						, m_OldSnapshotsByNode(_OldSnapshotsByNode)
						, m_UpdateSequence(_UpdateSequence)
						, m_NotificationPath(_NotificationPath)
					{
					}

					CSnapshotsByNode &m_SnapshotsByNode;
					CSnapshotsByNode &m_OldSnapshotsByNode;
					uint64 m_UpdateSequence;
					NStr::CStr m_NotificationPath;

					NContainer::TCMap<NStr::CStr, zbool> m_DirsToUpdate;
					NContainer::TCMap<NStr::CStr, zbool> m_ChangedPaths;
				};

				CFileSnapshot m_RootSnapshot;
				CSnapshotsByNode m_SnapshotsByNode;
				
				void f_AddNotification(CFindChangesContext &o_Context, EFileChangeNotification _Type, NStr::CStr const &_RelativePath, NStr::CStr const &_RenameFrom = {});
				void fr_FindChanges
					(
						CFindChangesContext &o_Context
						, CFileSnapshot const &_NewSnapshot
						, bool _bRecursive
						, bool _bPotentianllyRecursive
						, bool _bFirstRecursive
					)
				;
				void f_ScanDir
					(
						NStr::CStr const &_Path
						, bool _bInitial
						, bool _bNeedSubDirs
						, CUpdateSnapshotContext &_UpdateContext
						, CFileSnapshot &o_NewSnapshot
					)
				;
				void f_InitialScan();
				void f_FullRescan();
				void f_ProcessChanges
					(
						mint _nEvents
						, ch8 const **_pPaths
						, FSEventStreamEventFlags const _Flags[]
						, FSEventStreamEventId const _IDs[]
						, bool _bInitialScan
					)
				;
				void f_ProcessChangesPerFile
					(
						mint _nEvents
						, ch8 const **_pPaths
						, FSEventStreamEventFlags const _Flags[]
						, FSEventStreamEventId const _IDs[]
					)
				;
				
				DMibListLinkDS_Link(CNotification, m_Link);

				FSEventStreamRef m_pEventStream;
				CFileChangeNoticationContext *m_pContext;
				NMib::NFile::EFileChange m_Flags;
				NMib::NStr::CStr m_NotificationPath;
				NContainer::TCLinkedList<CChange> m_Changes;
				NThread::CMutual m_ChangesLock;
				
				NContainer::TCLinkedList<NStorage::TCTuple<NStr::CStr, bool>> m_RenamedFromQueue;

				DMibRefcountDebuggingOnly(NStorage::CRefCountDebugReference m_DebugSelfRef);

				NMib::NThread::CSemaphoreReportableAggregate *m_pReportTo;
				uint64 m_UpdateSequence = 0;
				bool m_bAddedToRunLoop = false;
				bool m_bStreamStarted = false;
				bool m_bPerFileEvents = false;
			};
			typedef DMibListLinkDS_Iter(CNotification, m_Link)  CNotificationIter;
			
			NThread::CMutual m_Lock;

			DMibListLinkDS_List(CNotification, m_Link) m_OpenNotifications;
			
			NMib::NStorage::TCUniquePointer<NMib::NThread::CThreadObject> m_pProcessThread;
			
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
			
			NStorage::TCUniquePointer<CInternal> m_pInternal;
			
			NMib::NThread::CMutual m_RunLoopLock;
			zbool m_bDestroying;
			
			NMib::NContainer::TCThreadSafeQueue<NMib::NFunction::TCFunctionMovable<void ()>> m_DispatchQueue;

			void f_DispatchOnThread(NMib::NFunction::TCFunctionMovable<void ()> &&_Dispatch);
			void f_StartThread();
			void *f_Open(const NMib::NStr::CStr &_FileName, NMib::NFile::EFileChange _OpenFlags, NMib::NThread::CSemaphoreReportableAggregate *_pReportTo);
			
			void f_Close(void *_pNotification);
			bint f_Changed(void *_pNotification);
			bint f_GetNotification(void *_pNotification, NMib::NStr::CStr &_Path, NFile::EFileChangeNotification &_Notification, NMib::NStr::CStr &_PathFrom);
			
		};
	}


} // Namespace NMib
