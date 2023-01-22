// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Core/SubSystem>
#include "Malterlib_Core_RuntimeType.h"

namespace NMib
{

	namespace
	{
		struct CSubSystem_Core_RunTimeObject : public CSubSystem
		{
			CRunTimeObjectInfoContainer m_GlobalNamespace;
		};
		
		constinit TCSubSystem<CSubSystem_Core_RunTimeObject, ESubSystemDestruction_BeforeMemoryManager> g_DynamicObjectsSystem = {DAggregateInit};
	}

	
	NStr::CStr CRunTimeObjectInfo::f_GetName()
	{
		NStr::CStr ToReturn = m_pName;
		return ToReturn;
	}

	NStr::CStr CRunTimeObjectInfo::f_GetNamespaceName()
	{
		NStr::CStr ToReturn;

		CRunTimeObjectInfo *pNamespace = m_pNamespace;
		while (pNamespace && pNamespace->m_pName && *pNamespace->m_pName)
		{
			if (ToReturn.f_IsEmpty())
				ToReturn = NStr::CStr(pNamespace->m_pName);
			else
				ToReturn = NStr::CStr(pNamespace->m_pName) + "::" + ToReturn;
			pNamespace = pNamespace->m_pNamespace;
		}

		return ToReturn;
	}

	NStr::CStr CRunTimeObjectInfo::f_GetFullName()
	{
		NStr::CStr ToReturn = m_pName;

		CRunTimeObjectInfo *pNamespace = m_pNamespace;
		while (pNamespace && pNamespace->m_pName && *pNamespace->m_pName)
		{
			ToReturn = NStr::CStr(pNamespace->m_pName) + "::" + ToReturn;
			pNamespace = pNamespace->m_pNamespace;
		}

		return ToReturn;
	}

	CRunTimeObjectInfo *CRunTimeObjectInfo::f_GetObject(const ch8 *_pName, mint _NameLen)
	{
		CRunTimeObjectInfo *pNamespace = &g_DynamicObjectsSystem->m_GlobalNamespace;
		NStr::CStr Namespace;
		const ch8 *pNamespaceStr = _pName;
		while (1)
		{
			DMibFastCheck(mint(pNamespaceStr - _pName) < _NameLen);
			NStr::CStr String;
			aint iSub = NStr::fg_StrFind(pNamespaceStr, "::");
			if (iSub >= 0)
			{
				String.f_AddStr(pNamespaceStr, iSub);
				pNamespaceStr += iSub + 2;
			}
			else
				String.f_AddStr(pNamespaceStr);

			CRunTimeObjectInfo *pInfo = pNamespace->m_Namespace.f_FindEqual(String);

			if (Namespace.f_GetLen())
				Namespace.f_AddStr("::");
			Namespace.f_AddStr(String);

			if (!pInfo)
			{
				NStorage::TCUniquePointer<CRunTimeObjectInfoContainer, NMemory::CAllocator_NonTrackedHeap> pNewInfo = fg_Construct();
				
				pNewInfo->m_bIsStatic = false;
				((CRunTimeObjectInfoContainer *)pNewInfo.f_Get())->f_Construct(Namespace);
				pInfo = pNewInfo.f_Detach();
			}

			pNamespace = pInfo;

			if (iSub < 0)
				break;
		}

		return pNamespace;
	}

	void CRunTimeObjectInfo::f_ForEachLeafChild(NFunction::TCFunction<void (CRunTimeObjectInfo const &_RuntimeObjectInfo)> const &_fOnChild) const
	{
		for (auto &Child : m_Children)
		{
			if (Child.m_Children.f_IsEmpty())
				_fOnChild(Child);
			else
				Child.f_ForEachLeafChild(_fOnChild);
		}
	}

	void CRunTimeObjectInfo::f_Construct(const ch8 *_pName, const ch8 *_pParent, bool _bIsStatic)
	{
//		NSys::fg_DebugOutput((NStr::CFStr256::CFormat("Constructing 0x{nfh,sj16,sf0} {} parent {}" DMibNewLine) << (mint)this << _pName << _pParent).f_GetStr());

		const ch8 *pNamespace = NStr::fg_StrAdd(_pName, NStr::fg_StrFindReverse(_pName, "::"));
		const ch8 *pName = _pName;

		if (pNamespace)
		{
			NStr::CStr Namespace;
			Namespace.f_AddStr(_pName, pNamespace - _pName);
			pName = pNamespace + 2;
			m_pNamespace = f_GetObject(Namespace.f_GetStr(), Namespace.f_GetLen());
		}
		else
		{
			m_pNamespace = &g_DynamicObjectsSystem->m_GlobalNamespace;
		}
		
		m_pName = pName;

		m_bIsStatic = _bIsStatic;
		
		CRunTimeObjectInfo *pInfo = m_pNamespace->m_Namespace.f_FindEqual(pName);
		if (_pParent)
			m_pParent = f_GetObject(_pParent, NStr::fg_StrLen(_pParent));
		else
			m_pParent = nullptr;

		if (pInfo)
		{
			if (pInfo->m_bIsStatic || !_bIsStatic)
			{
				DMibError("This runtimeclass is already registered");
			}
			else
			{
				// Transfer children
				CRunTimeObjectInfo *pRoot = pInfo->m_Children.f_GetFirst();

				while (pRoot)
				{
					pInfo->m_Children.f_Remove(pRoot);
					pRoot->m_pParent = this;
					m_Children.f_Insert(pRoot);

					pRoot = pInfo->m_Children.f_GetFirst();
				}

				pRoot = pInfo->m_Namespace.f_GetRoot();

				while (pRoot)
				{
					pInfo->m_Namespace.f_Remove(pRoot);
					pRoot->m_pNamespace = this;
					m_Namespace.f_Insert(pRoot);

					pRoot = pInfo->m_Namespace.f_GetRoot();
				}

				if (pInfo->m_pParent)
				{
					DMibSafeCheck(pInfo->m_pParent == m_pParent, "We should have the same parent");
					pInfo->m_pParent->m_Children.f_Remove(pInfo);
					pInfo->m_pParent = nullptr;
				}

				DMibSafeCheck(pInfo->m_pNamespace == m_pNamespace, "Should have same namespace");
				
				pInfo->m_pNamespace->m_Namespace.f_Remove(pInfo);
				pInfo->m_pNamespace = nullptr;
				NStorage::TCUniquePointer<CRunTimeObjectInfo, NMemory::CAllocator_NonTrackedHeap> pInfoPtr = fg_Explicit(pInfo);
				pInfoPtr.f_Clear();
				m_pNamespace->m_Namespace.f_Insert(this);
			}
		}
		else
		{
			m_pNamespace->m_Namespace.f_Insert(this);
		}

		if (m_pParent)
			m_pParent->m_Children.f_Insert(this);
	}

	CRunTimeObjectInfo::~CRunTimeObjectInfo()
	{
		if (m_bIsStatic)
			f_Destruct();
	}

	void CRunTimeObjectInfo::f_Destruct()
	{
//		NSys::fg_DebugOutput((NStr::CFStr256::CFormat("Destroying 0x{nfh,sj16,sf0} {}" DMibNewLine) << (mint)m_pName<< m_pName ).f_GetStr());

		if (!m_Children.f_IsEmpty() || !m_Namespace.f_IsEmpty())
		{
			DMibSafeCheck(m_bIsStatic,"If we have reached the destructor and have children left something has gone terribly wrong when we aren't static");
			// Remove from pool

			NStorage::TCUniquePointer<CRunTimeObjectInfoContainer, NMemory::CAllocator_NonTrackedHeap> pNewTempConstruct = fg_Construct();
			auto *pNewTemp = pNewTempConstruct.f_Detach();
			pNewTemp->m_pName = m_pName;
			pNewTemp->m_pParent = m_pParent;
			pNewTemp->m_pNamespace = m_pNamespace;
			pNewTemp->m_bIsStatic = false;

			CRunTimeObjectInfo *pRoot = m_Children.f_GetFirst();

			while (pRoot)
			{
				m_Children.f_Remove(pRoot);
				pRoot->m_pParent = pNewTemp;
				pNewTemp->m_Children.f_Insert(pRoot);
				pRoot = m_Children.f_GetFirst();
			}

			pRoot = m_Namespace.f_GetRoot();

			while (pRoot)
			{
				m_Namespace.f_Remove(pRoot);
				pRoot->m_pNamespace = pNewTemp;
				pNewTemp->m_Namespace.f_Insert(pRoot);
				pRoot = m_Namespace.f_GetRoot();
			}

			if (m_pParent)
			{
				m_pParent->m_Children.f_Remove(this);
				m_pParent->m_Children.f_Insert(pNewTemp);
			}

			if (m_pNamespace)
			{
				m_pNamespace->m_Namespace.f_Remove(this);
				m_pNamespace->m_Namespace.f_Insert(pNewTemp);
			}

		}
		else
		{
			if (m_pParent)
			{
				m_pParent->m_Children.f_Remove(this);
				if (!m_pParent->m_bIsStatic && m_pParent->m_Children.f_IsEmpty() && m_pParent->m_Namespace.f_IsEmpty())
				{
					NStorage::TCUniquePointer<CRunTimeObjectInfo, NMemory::CAllocator_NonTrackedHeap> pTempDestruct = fg_Explicit(m_pParent);
					pTempDestruct.f_Clear();
				}
			}

			if (m_pNamespace)
			{
				m_pNamespace->m_Namespace.f_Remove(this);
				if (!m_pNamespace->m_bIsStatic && m_pNamespace->m_Children.f_IsEmpty() && m_pNamespace->m_Namespace.f_IsEmpty())
				{
					NStorage::TCUniquePointer<CRunTimeObjectInfo, NMemory::CAllocator_NonTrackedHeap> pTempDestruct = fg_Explicit(m_pNamespace);
					pTempDestruct.f_Clear();
				}
			}
		}
	}

	CRunTimeObjectInfo *fg_GetRuntimeTypeInfo(const ch8 *_pObjectName)
	{
		aint iNamespace = NStr::fg_StrFind(_pObjectName, "::");
		if (iNamespace >= 0)
		{
			const ch8 *pNamespace = _pObjectName + iNamespace;				
			const ch8 *pNamespaceStart = _pObjectName;

			CRunTimeObjectInfo *pRuntimeObject = &g_DynamicObjectsSystem->m_GlobalNamespace;

			while (1)
			{
				NStr::CStr Str;
				Str.f_AddStr(pNamespaceStart, (pNamespace - pNamespaceStart));

				const ch8 *pFind = Str;
				pRuntimeObject = pRuntimeObject->m_Namespace.f_FindEqual(pFind);

				if (!pRuntimeObject)
					return nullptr;

				pNamespaceStart = pNamespace + 2;
		
				iNamespace = NStr::fg_StrFind(pNamespaceStart, "::");

				if (iNamespace < 0)
				{
					pRuntimeObject = pRuntimeObject->m_Namespace.f_FindEqual(pNamespaceStart);
					if (pRuntimeObject)
						return pRuntimeObject;
					else
						return nullptr;
				}
				else
					pNamespace = pNamespaceStart + iNamespace;
			}
		}
		else
		{
			CRunTimeObjectInfo *pRuntimeObject = g_DynamicObjectsSystem->m_GlobalNamespace.m_Namespace.f_FindEqual(_pObjectName);
			if (pRuntimeObject)
				return pRuntimeObject;
			else
				return nullptr;
		}
	}

	void *fg_CreateRuntimeType(const ch8 *_pObjectName)
	{
		aint iNamespace = NStr::fg_StrFind(_pObjectName, "::");
		if (iNamespace >= 0)
		{
			const ch8 *pNamespace = _pObjectName + iNamespace;				
			const ch8 *pNamespaceStart = _pObjectName;

			CRunTimeObjectInfo *pRuntimeObject = &g_DynamicObjectsSystem->m_GlobalNamespace;

			while (1)
			{
				NStr::CStr Str;
				Str.f_AddStr(pNamespaceStart, (pNamespace - pNamespaceStart));

				const ch8 *pFind = Str;
				pRuntimeObject = pRuntimeObject->m_Namespace.f_FindEqual(pFind);

				if (!pRuntimeObject)
					return nullptr;

				pNamespaceStart = pNamespace + 2;
				iNamespace = NStr::fg_StrFind(pNamespaceStart, "::");

				if (iNamespace < 0)
				{
					pRuntimeObject = pRuntimeObject->m_Namespace.f_FindEqual(pNamespaceStart);
					if (pRuntimeObject)
						return pRuntimeObject->f_CreateObject();
					else
						return nullptr;
				}
				else
					pNamespace = pNamespaceStart + iNamespace;
			}
		}
		else
		{
			CRunTimeObjectInfo *pRuntimeObject = g_DynamicObjectsSystem->m_GlobalNamespace.m_Namespace.f_FindEqual(_pObjectName);
			if (pRuntimeObject)
				return pRuntimeObject->f_CreateObject();
			else
				return nullptr;
		}
	}
}

