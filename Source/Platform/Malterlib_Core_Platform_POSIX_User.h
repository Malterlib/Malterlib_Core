// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <pwd.h>
#include <grp.h>

namespace NMib
{
	namespace NPlatform
	{
		struct CGetPwUidState
		{
			passwd m_User;
			NContainer::TCVector<char> m_Buffer;
			int m_Error = 0;
		};

		passwd *fg_Helper_GetPwUid(uid_t _UserID, CGetPwUidState &o_State);
		passwd *fg_Helper_GetPwNam(ch8 const *_pName, CGetPwUidState &o_State);

		struct CGetGrGidState
		{
			group m_Group;
			NContainer::TCVector<char> m_Buffer;
			int m_Error = 0;
		};

		group *fg_Helper_GetGrGid(gid_t _UserID, CGetGrGidState &o_State);
		group *fg_Helper_GetGrNam(ch8 const *_pName, CGetGrGidState &o_State);
	}
}

