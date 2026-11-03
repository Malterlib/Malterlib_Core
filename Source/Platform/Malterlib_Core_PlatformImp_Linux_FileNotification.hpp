// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

inline auto &CFileChangeNotificationContext::CWatch::f_GetChildren()
{
	return mp_Children;
}

template <typename tf_FOnFile>
void CFileChangeNotificationContext::CWatch::f_ForEachChildFileRecursive(CStr const &_BasePath, tf_FOnFile &&_fOnFile, bool _bRecursive)
{
	for (auto &bIsDir : mp_ChildFiles)
		_fOnFile(CFile::fs_AppendPath(_BasePath, mp_ChildFiles.fs_GetKey(bIsDir)), bIsDir);

	if (!_bRecursive)
		return;

	for (auto &Child : mp_Children)
		Child.f_ForEachChildFileRecursive(CFile::fs_AppendPath(_BasePath, Child.mp_FileName), _fOnFile, true);
}

template <typename tf_FOnFile>
void CFileChangeNotificationContext::CWatch::f_ForEachChildFile(tf_FOnFile &&_fOnFile, bool _bRecursive)
{
	f_ForEachChildFileRecursive("", _fOnFile, _bRecursive);
}
