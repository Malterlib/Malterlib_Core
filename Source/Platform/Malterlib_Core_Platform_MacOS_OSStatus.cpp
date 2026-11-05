// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_MacOS_OSStatus.h"

namespace NMib
{
	namespace NPlatform
	{
#include "Malterlib_Core_Platform_MacOS_OSStatuses.hpp"

		struct CSubSystem_Core_MacOS_ErrorStore : public CSubSystem
		{
			struct CError : public CMacOSError
			{
				struct CCompare
				{
					inline_small int operator () (CError const &_Node) const;
				};

				NIntrusive::TCAVLLink<> m_TreeLink;
			};

			CSubSystem_Core_MacOS_ErrorStore();

			CError *f_GetError(int _Code);

			CError m_Errors[sizeof(gs_MacOSErrors) / sizeof(CMacOSError)];
			NIntrusive::TCAVLTree<&CError::m_TreeLink, CError::CCompare> m_Tree;
		};

		int CSubSystem_Core_MacOS_ErrorStore::CError::CCompare::operator () (CError const &_Node) const
		{
			return _Node.m_Code;
		}

		CSubSystem_Core_MacOS_ErrorStore::CSubSystem_Core_MacOS_ErrorStore()
		{
			int nErrors = sizeof(gs_MacOSErrors) / sizeof(CMacOSError);
			for (int iError = 0; iError < nErrors; ++iError)
			{
				CError &Err = m_Errors[iError];
				auto &OSErr = gs_MacOSErrors[iError];

				CError *pErr = m_Tree.f_FindEqual(OSErr.m_Code);

				if (pErr)
				{
					[[maybe_unused]] NStr::CFStr256 Short = NStr::CFStr256((OSErr.m_pShort ? OSErr.m_pShort : "")).f_TrimLeft().f_TrimRight();
					[[maybe_unused]] NStr::CFStr256 Long = NStr::CFStr256((OSErr.m_pLong ? OSErr.m_pLong : "")).f_TrimLeft().f_TrimRight();

					DMibTrace
						(
							"MacOSErrorStor: Error {} ({}, {}) already in strore as ({}, {})\n"
							, OSErr.m_Code
							, Short
							, Long
							, (pErr->m_pShort ? pErr->m_pShort : "")
							, (pErr->m_pLong ? pErr->m_pLong : "")
						)
					;
				}
				else
				{
					Err.m_Code = OSErr.m_Code;
					Err.m_pShort = OSErr.m_pShort;
					Err.m_pLong = OSErr.m_pLong;
					m_Tree.f_Insert(Err);
				}
			}
		}

		auto CSubSystem_Core_MacOS_ErrorStore::f_GetError(int _Code) -> CError *
		{
			return m_Tree.f_FindEqual(_Code);
		}

		constinit TCSubSystem<CSubSystem_Core_MacOS_ErrorStore, ESubSystemDestruction_Last> g_SubSystem_Core_Platform_MacOS_ErrorStore = {DAggregateInit};

		CMacOSError const *fg_GetOSStatusError(int _Status)
		{
			return g_SubSystem_Core_Platform_MacOS_ErrorStore->f_GetError(_Status);
		}

		NStr::CFStr256 fg_FormatOSStatus(ch8 const *_pDesc, int _Status)
		{
			auto const *pError = fg_GetOSStatusError(_Status);

			if (!pError)
			{
				if (_pDesc[0])
					return NStr::CFStr256::CFormat("The OS returned an error from {}: Unknown error ({})") << _pDesc << _Status;
				else
					return NStr::CFStr256::CFormat("Unknown error ({})") << _Status;
			}

			if (_pDesc[0])
			{
				return NStr::CFStr256::CFormat("The OS returned an error from {}: {} ({} {})")
					<< _pDesc
					<< (pError->m_pLong ? pError->m_pLong : "Unknown")
					<< _Status
					<< (pError->m_pShort ? pError->m_pShort : "Unknown")
				;
			}
			else
				return NStr::CFStr256::CFormat("{} ({} {})") << (pError->m_pLong ? pError->m_pLong : "Unknown") << _Status << (pError->m_pShort ? pError->m_pShort : "Unknown");
		}
	}
}

