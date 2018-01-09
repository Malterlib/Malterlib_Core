// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_PlatformImp_Linux_FileNotificationContext.h"

using namespace NMib::NFile;

CFileChangeNotificationContext::CNotification::CNotification(CFileChangeNotificationContext* _pNotificationContext, CStr const &_BasePath):
	m_pContext(_pNotificationContext)
	, m_BasePath(_BasePath)
{
	m_pReportTo = nullptr;
	m_Flags = NMib::NFile::EFileChange_None;
}

CFileChangeNotificationContext::CNotification::~CNotification()
{
	f_Clear();
}

void CFileChangeNotificationContext::CNotification::f_Clear()
{
	f_Cancel();
	m_Changes.f_Clear();
}

void CFileChangeNotificationContext::CNotification::f_Cancel()
{
	while (!m_Watches.f_IsEmpty())
		m_pContext->f_UnlinkWatch(*m_Watches.f_FindSmallest(), this, false);
}

auto CFileChangeNotificationContext::CNotification::f_WatchPath(CWatch *_pParentWatch, CStr const &_Path, bool _bThrow) -> CWatch *
{
	CWatch *pWatch;
	try
	{
		int WatchDescriptor = m_pContext->f_Inotify_AddWatch(_Path);
		pWatch = &m_pContext->f_LinkWatch(WatchDescriptor, _Path, this, _pParentWatch);
	}
	catch (NException::CException const &_Exception)
	{
		if (_bThrow)
			throw;
		DMibTrace("Error add watch on sub path {} ({})\n", _Path << _Exception.f_GetErrorStr());
		return nullptr;
	}

	for (auto &bDirectory : pWatch->m_ChildFiles)
	{
		if (bDirectory && (m_Flags & NFile::EFileChange_Recursive))
			f_WatchPath(pWatch, CFile::fs_AppendPath(pWatch->f_GetPath(), pWatch->m_ChildFiles.fs_GetKey(bDirectory)), false);
	}
	return pWatch;
}

void CFileChangeNotificationContext::CNotification::f_RegisterChange
	(
		CFindChangesContext &o_Context
		, CStr const &_Path
		, NMib::NFile::EFileChangeNotification _Notification
		, CStr const &_RenameFrom
	)
{
	CChange Change;
	Change.m_Notification = _Notification;
	Change.m_Path = _Path;
	Change.m_PathFrom = _RenameFrom;
	
	if (o_Context.m_ChangesSet(Change).f_WasCreated())
	{
		if (_Notification == EFileChangeNotification_Removed || _Notification == EFileChangeNotification_Added || _Notification == EFileChangeNotification_Renamed)
			o_Context.m_ChangesFileName.f_Insert(fg_Move(Change));
		else
			o_Context.m_Changes.f_Insert(fg_Move(Change));
	}
}

void CFileChangeNotificationContext::CNotification::f_OnRemovedFromRename(CFindChangesContext &o_Context, CPendingRename const &_PendingRename)
{
	if (((m_Flags & NFile::EFileChange_DirectoryName) && _PendingRename.m_bIsDir) || ((m_Flags & NFile::EFileChange_FileName) && !_PendingRename.m_bIsDir))
		f_RegisterChange(o_Context, _PendingRename.m_RelativePath, NMib::NFile::EFileChangeNotification_Removed);
	if (m_Flags & NFile::EFileChange_DirectoryName)
		f_RegisterChange(o_Context, NFile::CFile::fs_GetPath(_PendingRename.m_RelativePath), NMib::NFile::EFileChangeNotification_Modified);
	
	if (_PendingRename.m_bIsDir && (m_Flags & NFile::EFileChange_Recursive) && _PendingRename.m_pWatch)
	{
		_PendingRename.m_pWatch->f_ForEachChildFile
			(
				[&](CStr const &_Path, bool _bIsDir)
				{
					CStr ChildRelativePath = CFile::fs_AppendPath(_PendingRename.m_RelativePath, _Path);
					if (((m_Flags & NFile::EFileChange_DirectoryName) && _bIsDir) || ((m_Flags & NFile::EFileChange_FileName) && !_bIsDir))
						f_RegisterChange(o_Context, ChildRelativePath, NMib::NFile::EFileChangeNotification_Removed);
				}
				, true  
			)
		;
	}
	
	if (_PendingRename.m_pWatch)
		m_pContext->f_UnlinkWatch(_PendingRename.m_pWatch, this, true);
}

void CFileChangeNotificationContext::CNotification::f_OnAdded(CFindChangesContext &o_Context, CStr const &_Path, bool _bIsDir, CWatch* _pWatch)
{
	CWatch *pWatch = nullptr;
	if ((m_Flags & NFile::EFileChange_Recursive) && _bIsDir)
		pWatch = f_WatchPath(_pWatch, CFile::fs_AppendPath(m_BasePath, _Path), false);
	
	if (((m_Flags & NFile::EFileChange_DirectoryName) && _bIsDir) || ((m_Flags & NFile::EFileChange_FileName) && !_bIsDir))
		f_RegisterChange(o_Context, _Path, NMib::NFile::EFileChangeNotification_Added);
	if (m_Flags & NFile::EFileChange_DirectoryName)
		f_RegisterChange(o_Context, NFile::CFile::fs_GetPath(_Path), NMib::NFile::EFileChangeNotification_Modified);
	
	if (!pWatch || !(m_Flags & (NFile::EFileChange_DirectoryName | NFile::EFileChange_FileName)))
		return;
	
	pWatch->f_ForEachChildFile
		(
			[&](CStr const &_ChildPath, bool _bIsDir)
			{
				CStr RelativePath = CFile::fs_AppendPath(_Path, _ChildPath);
				if (((m_Flags & NFile::EFileChange_DirectoryName) && _bIsDir) || ((m_Flags & NFile::EFileChange_FileName) && !_bIsDir))
					f_RegisterChange(o_Context, RelativePath, NMib::NFile::EFileChangeNotification_Added);
			}
			, true  
		)
	;
}

void CFileChangeNotificationContext::CNotification::f_OnEvent(CFindChangesContext &o_Context, inotify_event *_pEvent, TCSharedPointer<CWatch> const &_pWatch)
{
	if (!_pEvent || !_pWatch)
		return;

	CStr EventPath = _pWatch->f_GetPath();
	
	CStr EventFileName(_pEvent->name, fg_StrLen(_pEvent->name, _pEvent->len));

	if (!EventFileName.f_IsEmpty())
		EventPath += CStr::CFormat("/{}") << EventFileName;
	
	CStr RelativePath = EventPath.f_Delete(0, m_BasePath.f_GetLen()+1);
	//DMibConOut2("f_OnEvent: {} = 0x{nfh,sf0,sj8} 0x{nfh,sf0,sj8}\n", CStr(_pEvent->name, fg_StrLen(_pEvent->name, _pEvent->len)), _pEvent->mask, _pEvent->cookie);

	bool bIsDir = (_pEvent->mask & IN_ISDIR) != 0;

	if (_pEvent->mask & IN_DELETE_SELF)
	{
		if (RelativePath.f_IsEmpty() && (((m_Flags & NFile::EFileChange_DirectoryName) && bIsDir) || ((m_Flags & NFile::EFileChange_FileName) && !bIsDir)))
			f_RegisterChange(o_Context, RelativePath, NMib::NFile::EFileChangeNotification_Removed);
		m_pContext->f_UnlinkWatch(_pWatch, this, true);
	}
	else if (_pEvent->mask & IN_CREATE)
		f_OnAdded(o_Context, RelativePath, bIsDir, _pWatch.f_Get());
	else if (_pEvent->mask & IN_DELETE)
	{
		if (((m_Flags & NFile::EFileChange_DirectoryName) && bIsDir) || ((m_Flags & NFile::EFileChange_FileName) && !bIsDir))
			f_RegisterChange(o_Context, RelativePath, NMib::NFile::EFileChangeNotification_Removed);
		if (m_Flags & NFile::EFileChange_DirectoryName)
			f_RegisterChange(o_Context, NFile::CFile::fs_GetPath(RelativePath), NMib::NFile::EFileChangeNotification_Modified);
	}
	else if (_pEvent->mask & IN_MOVED_FROM)
	{
		CPendingRename &PendingRename = m_PendingRenames[_pEvent->cookie];
		PendingRename.m_RelativePath = RelativePath;
		PendingRename.m_bIsDir = bIsDir;
		PendingRename.m_pWatch = fg_Explicit(_pWatch->f_GetChild(EventFileName));
		return;
	}
	else if (_pEvent->mask & IN_MOVED_TO)
	{
		auto pRename = m_PendingRenames.f_FindEqual(_pEvent->cookie);
		if (pRename)
		{
			auto &Rename = *pRename;
			
			if (((m_Flags & NFile::EFileChange_DirectoryName) && bIsDir) || ((m_Flags & NFile::EFileChange_FileName) && !bIsDir))
				f_RegisterChange(o_Context, RelativePath, NMib::NFile::EFileChangeNotification_Renamed, Rename.m_RelativePath);
			if (m_Flags & NFile::EFileChange_DirectoryName)
				f_RegisterChange(o_Context, NFile::CFile::fs_GetPath(RelativePath), NMib::NFile::EFileChangeNotification_Modified);
			if (m_Flags & NFile::EFileChange_DirectoryName)
				f_RegisterChange(o_Context, NFile::CFile::fs_GetPath(Rename.m_RelativePath), NMib::NFile::EFileChangeNotification_Modified);
			if (bIsDir && (m_Flags & NFile::EFileChange_Recursive) && Rename.m_pWatch)
			{
				Rename.m_pWatch->f_ForEachChildFile
					(
						[&](CStr const &_ChildPath, bool _bIsDir)
						{
							CStr ChildRelativePath = CFile::fs_AppendPath(RelativePath, _ChildPath);
							CStr FromPath = CFile::fs_AppendPath(Rename.m_RelativePath, _ChildPath);
							if (((m_Flags & NFile::EFileChange_DirectoryName) && _bIsDir) || ((m_Flags & NFile::EFileChange_FileName) && !_bIsDir))
								f_RegisterChange(o_Context, ChildRelativePath, NMib::NFile::EFileChangeNotification_Renamed, FromPath);
						}
						, true  
					)
				;
			}
			m_PendingRenames.f_Remove(pRename);
		}
		else
			f_OnAdded(o_Context, RelativePath, bIsDir, _pWatch.f_Get());
	}

	if (((_pEvent->mask & IN_MODIFY) && (m_Flags & NFile::EFileChange_Write)) || ((_pEvent->mask & IN_ATTRIB) && (m_Flags & NFile::EFileChange_Attributes)))
		f_RegisterChange(o_Context, RelativePath, NMib::NFile::EFileChangeNotification_Modified);
}
