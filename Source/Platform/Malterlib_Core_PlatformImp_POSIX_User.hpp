// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

using namespace NMib;

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mman.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#include <uuid/uuid.h>
#include <poll.h>

#include <sys/param.h>

#include "Malterlib_Core_PlatformImp_POSIX.h"

#include <Mib/Core/PlatformSpecific/PosixErrNo>
#include <Mib/Core/PlatformSpecific/PosixUser>

namespace
{
	CStr fg_Helper_GetGroupName(gid_t _GroupID)
	{
		NMib::NPlatform::CGetGrGidState State;
		group *pGroup = fg_Helper_GetGrGid(_GroupID, State);

		if (pGroup)
			return CStr(pGroup->gr_name);
		else
		{
			if (State.m_Error == 0) // Does not exist
				return CStr::fs_ToStr(_GroupID);
			DMibErrorFile(NMib::NPlatform::fg_FormatErrno(CStr::CFormat("getgrgid_r({}) when getting group name") << _GroupID, State.m_Error));
		}
	}

	CStr fg_Helper_GetUserName(uid_t _UserID)
	{
		NMib::NPlatform::CGetPwUidState State;

		auto *pUser = fg_Helper_GetPwUid(_UserID, State);
		if (pUser)
			return CStr(pUser->pw_name);
		else
		{
			if (State.m_Error == 0) // Does not exist
				return CStr::fs_ToStr(_UserID);
			else
				DMibErrorFile(NMib::NPlatform::fg_FormatErrno(CStr::CFormat("getpwuid_r({}) when getting user name") << _UserID, State.m_Error));
		}
	}
}

NMib::NStr::CStr NSys::fg_UserManagement_GetProcessRealUser()
{
	return NMib::NStr::CStr::fs_ToStr(getuid());
}

NMib::NStr::CStr NSys::fg_UserManagement_GetProcessEffectiveUser()
{
	return NMib::NStr::CStr::fs_ToStr(geteuid());
}

NMib::NStr::CStr NSys::fg_UserManagement_GetProcessRealGroup()
{
	return NMib::NStr::CStr::fs_ToStr(getgid());
}

NMib::NStr::CStr NSys::fg_UserManagement_GetProcessEffectiveGroup()
{
	return NMib::NStr::CStr::fs_ToStr(getegid());
}

NMib::NStr::CStr NSys::fg_UserManagement_GetProcessRealUserName()
{
	return fg_Helper_GetUserName(getuid());
}

NMib::NStr::CStr NSys::fg_UserManagement_GetProcessEffectiveUserName()
{
	return fg_Helper_GetUserName(geteuid());
}

NMib::NStr::CStr NSys::fg_UserManagement_GetProcessRealGroupName()
{
	return fg_Helper_GetGroupName(getgid());
}

NMib::NStr::CStr NSys::fg_UserManagement_GetProcessEffectiveGroupName()
{
	return fg_Helper_GetGroupName(getegid());
}


bool NSys::fg_UserManagement_GroupExists(NMib::NStr::CStr const &_GroupName, NMib::NStr::CStr &_ReturnGID)
{
	errno = 0;
	
	NMib::NPlatform::CGetGrGidState State;
	group *pGroup = NMib::NPlatform::fg_Helper_GetGrNam(_GroupName.f_GetStr(), State);

	if (pGroup)
		_ReturnGID = NMib::NStr::CStr::CFormat("{}") << pGroup->gr_gid;
	else if (errno != 0)
		DMibError(NPlatform::fg_FormatErrno(CStr::CFormat("getgrnam_r('{}') when checking if group exists") << _GroupName, State.m_Error));
	

	return pGroup != nullptr;
}

bool NSys::fg_UserManagement_UserExists(NMib::NStr::CStr const &_UserName, NMib::NStr::CStr &_ReturnUID)
{
	errno = 0;
	
	NMib::NPlatform::CGetPwUidState State;
	passwd *pPassword = NMib::NPlatform::fg_Helper_GetPwNam(_UserName.f_GetStr(), State);

	if (pPassword != nullptr)
		_ReturnUID = NMib::NStr::CStr::CFormat("{}") << pPassword->pw_uid;
	else if (State.m_Error != 0)
		DMibError(NPlatform::fg_FormatErrno(CStr::CFormat("getpwnam_r('{}') when checking if user exists") << _UserName, State.m_Error));
	
	return pPassword != nullptr;
}

NMib::NContainer::TCVector<NMib::NStr::CStr> NSys::fg_UserManagement_UserGetMemberOfGroups(NMib::NStr::CStr const &_UserName)
{
	errno = 0;
	
	CStr UserName = _UserName;
	
	NMib::NPlatform::CGetPwUidState State;
	passwd *pPassword = NMib::NPlatform::fg_Helper_GetPwNam(_UserName.f_GetStr(), State);

	if (pPassword == nullptr)
	{
		if (State.m_Error == 0)
			DMibError(CStr::CFormat("User does not exists: {}") << _UserName);
		else
			DMibError(NPlatform::fg_FormatErrno(CStr::CFormat("getpwnam_r('{}') when getting user group members") << _UserName, State.m_Error));
	}
	
#ifdef DPlatformFamily_macOS
	TCVector<int> Groups;
#else
	TCVector<gid_t> Groups;
#endif
	int nGroups = 1;
	while (true)
	{
		int nGroupsOrig = nGroups;
		Groups.f_SetLen(nGroups);
		if (getgrouplist(UserName.f_GetStr(), pPassword->pw_gid, Groups.f_GetArray(), &nGroups) == -1)
		{
			if (nGroups <= nGroupsOrig)
				DMibError(NPlatform::fg_FormatErrno(CStr::CFormat("getgrouplist('{}') when getting user group members") << _UserName, errno));
		}
		else
		{
			Groups.f_SetLen(nGroups);
			break;
		}
	}

	NMib::NContainer::TCVector<NMib::NStr::CStr> Ret;
	
	for (auto &Group : Groups)
		Ret.f_Insert(CStr::fs_ToStr(Group));
	
	return Ret;
}

bool NSys::fg_UserManagement_UserIsMemberOfGroup(NMib::NStr::CStr const &_GroupName, NMib::NStr::CStr const &_UserName)
{
	errno = 0;
	
	NMib::NPlatform::CGetPwUidState State;
	passwd *pPassword = NMib::NPlatform::fg_Helper_GetPwNam(_UserName.f_GetStr(), State);

	if (pPassword == nullptr)
	{
		if (State.m_Error == 0)
			DMibError(CStr::CFormat("User does not exists: {}") << _UserName);
		else
			DMibError(NPlatform::fg_FormatErrno(CStr::CFormat("getpwnam_r('{}') when checking if user is member of group") << _UserName, State.m_Error));
	}
	
	short int lp;
	struct group grp;
	struct group * grpptr=&grp;
	struct group * tempGrpPtr;
	char grpbuffer[200];
	int  grplinelen = sizeof(grpbuffer);
	
	if ((getgrnam_r(_GroupName.f_GetStr(), grpptr, grpbuffer, grplinelen, &tempGrpPtr)) != 0)
		DMibError(NPlatform::fg_FormatErrno(CStr::CFormat("getgrnam_r('{}') when checking if user is member of group") << _GroupName, errno));
	
	if (tempGrpPtr == nullptr)
		DMibError(CStr::CFormat("Group does not exists: {}") << _GroupName);

	if (grp.gr_gid == pPassword->pw_gid)
		return true;

	for (lp = 1; NULL != *(grp.gr_mem); lp++, (grp.gr_mem)++)
	{
		CStrPtr Ptr;
		Ptr.f_SetConstPtr(*(grp.gr_mem), fg_StrLen(*(grp.gr_mem)));
		if (_UserName.f_Cmp(Ptr) == 0)
			return true;
	}
	
	return false;
}


