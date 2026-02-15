// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_Platform_MacOS_OSStatus.h"

namespace NMib
{
	namespace NPlatform
	{
		#include "Malterlib_Core_Platform_MacOS_OSStatuses.hpp"

		class CSubSystem_Core_MacOS_ErrorStore : public CSubSystem
		{
			public:

			struct CError : public CMacOSError
			{
				class CCompare
				{
				public:
					inline_small int operator () (CError const &_Node) const
					{
						return _Node.m_Code;
					}
				};

				NIntrusive::TCAVLLink<> m_TreeLink;
			};

			CError m_Errors[sizeof(gs_MacOSErrors)/sizeof(CMacOSError)];
			NIntrusive::TCAVLTree<&CError::m_TreeLink, CError::CCompare> m_Tree;

			CSubSystem_Core_MacOS_ErrorStore()
			{
				int nErrors = sizeof(gs_MacOSErrors)/sizeof(CMacOSError);
				for	(int i = 0; i < nErrors; ++i)
				{
					CError &Err = m_Errors[i];
					auto &OSErr = gs_MacOSErrors[i];

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

			CError *f_GetError(int _Code)
			{
				return m_Tree.f_FindEqual(_Code);
			}
		};

		constinit TCSubSystem<CSubSystem_Core_MacOS_ErrorStore, ESubSystemDestruction_Last> g_SubSystem_Core_Platform_MacOS_ErrorStore = {DAggregateInit};

		CMacOSError const *fg_GetOSStatusError(int _Status)
		{
			return g_SubSystem_Core_Platform_MacOS_ErrorStore->f_GetError(_Status);
		}

		NStr::CFStr256 fg_FormatOSStatus(const ch8 *_pDesc, int _Status)
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
				return NStr::CFStr256::CFormat("The OS returned an error from {}: {} ({} {})") << _pDesc << (pError->m_pLong ? pError->m_pLong : "Unknown") << _Status << (pError->m_pShort ? pError->m_pShort : "Unknown");
			else
				return NStr::CFStr256::CFormat("{} ({} {})") << (pError->m_pLong ? pError->m_pLong : "Unknown") << _Status << (pError->m_pShort ? pError->m_pShort : "Unknown");
		}
	}
}

