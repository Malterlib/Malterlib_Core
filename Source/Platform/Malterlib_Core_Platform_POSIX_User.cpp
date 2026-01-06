// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_Platform_POSIX_User.h"

#include <unistd.h>

namespace NMib
{
	namespace NPlatform
	{
		passwd *fg_Helper_GetPwUid(uid_t _UserID, CGetPwUidState &o_State)
		{
			mint Size = 1024;

			auto InitialLength = sysconf(_SC_GETPW_R_SIZE_MAX);
			if (InitialLength > 0)
				Size = InitialLength;

			while (true)
			{
				o_State.m_Buffer.f_SetLen(Size);

				passwd *pUser = nullptr;
				auto Error = getpwuid_r(_UserID, &o_State.m_User, o_State.m_Buffer.f_GetArray(), Size, &pUser);

				if (!Error && pUser)
					return pUser;
				else
				{
					if (Error != ERANGE)
					{
						o_State.m_Error = Error;
						return nullptr;
					}
				}

				Size *= 2;
			}
		}

		static int fg_TranslateLookupError(int _Error)
		{
			switch (_Error)
			{
			case ENOENT:
			case ESRCH:
			case EBADF:
			case EPERM:
				return 0;
			default:
				return _Error;
			}
		}

		passwd *fg_Helper_GetPwNam(ch8 const *_pName, CGetPwUidState &o_State)
		{
			mint Size = 1024;

			auto InitialLength = sysconf(_SC_GETPW_R_SIZE_MAX);
			if (InitialLength > 0)
				Size = InitialLength;

			while (true)
			{
				o_State.m_Buffer.f_SetLen(Size);

				passwd *pUser = nullptr;
				auto Error = getpwnam_r(_pName, &o_State.m_User, o_State.m_Buffer.f_GetArray(), Size, &pUser);

				if (!Error && pUser)
					return pUser;
				else
				{
					if (Error != ERANGE)
					{
						o_State.m_Error = fg_TranslateLookupError(Error);
						return nullptr;
					}
				}

				Size *= 2;
			}
		}

		group *fg_Helper_GetGrGid(gid_t _GroupID, CGetGrGidState &o_State)
		{
			mint Size = 1024;

			auto InitialLength = sysconf(_SC_GETGR_R_SIZE_MAX);
			if (InitialLength > 0)
				Size = InitialLength;

			while (true)
			{
				o_State.m_Buffer.f_SetLen(Size);

				group *pGroup = nullptr;
				auto Error = getgrgid_r(_GroupID, &o_State.m_Group, o_State.m_Buffer.f_GetArray(), Size, &pGroup);

				if (!Error && pGroup)
					return pGroup;
				else
				{
					if (Error != ERANGE)
					{
						o_State.m_Error = fg_TranslateLookupError(Error);
						return nullptr;
					}
				}

				Size *= 2;
			}
		}

		group *fg_Helper_GetGrNam(ch8 const *_pName, CGetGrGidState &o_State)
		{
			mint Size = 1024;

			auto InitialLength = sysconf(_SC_GETGR_R_SIZE_MAX);
			if (InitialLength > 0)
				Size = InitialLength;

			while (true)
			{
				o_State.m_Buffer.f_SetLen(Size);

				group *pGroup = nullptr;
				auto Error = getgrnam_r(_pName, &o_State.m_Group, o_State.m_Buffer.f_GetArray(), Size, &pGroup);

				if (!Error && pGroup)
					return pGroup;
				else
				{
					if (Error != ERANGE)
					{
						o_State.m_Error = fg_TranslateLookupError(Error);
						return nullptr;
					}
				}

				Size *= 2;
			}
		}
	}
}
