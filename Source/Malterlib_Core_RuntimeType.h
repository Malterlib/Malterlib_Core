// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib
{
	struct CRunTimeObjectInfo;

	struct CRunTimeObjectInfoLinks : public CVirtualDummy
	{
		NIntrusive::TCAVLLink<> m_TreeLinkNamespace;
	};

	struct CRunTimeObjectInfo : public CRunTimeObjectInfoLinks
	{		
		struct CCompare
		{
			ch8 const * operator () (CRunTimeObjectInfo const &_Info)
			{
				return _Info.m_pName;
			}

			auto operator () (ch8 const *_pLeft, ch8 const *_pRight)
			{
				return NStr::fg_StrCmp(_pLeft, _pRight) <=> 0;
			}
		};

		NStr::CStr f_GetName();
		NStr::CStr f_GetNamespaceName();
		NStr::CStr f_GetFullName();

		static CRunTimeObjectInfo *f_GetObject(const ch8 *_pName, mint _NameLen);

		void f_ForEachLeafChild(NFunction::TCFunction<void (CRunTimeObjectInfo const &_RuntimeObjectInfo)> const &_fOnChild) const;
		void f_Construct(const ch8 *_pName, const ch8 *_pParent, bool _bIsStatic = true);
		void f_Destruct();

		virtual ~CRunTimeObjectInfo();

		virtual void *f_CreateObject() const = 0;

		DMibListLinkDS_Link(CRunTimeObjectInfo, m_TreeLinkChild); // 8 bytes
		DMibListLinkDS_List(CRunTimeObjectInfo, m_TreeLinkChild) m_Children; // 4 bytes
		NIntrusive::TCAVLTree<&CRunTimeObjectInfo::m_TreeLinkNamespace, CCompare, NMib::NMemory::CDefaultAllocator, CRunTimeObjectInfo> m_Namespace;
		CRunTimeObjectInfo *m_pParent = nullptr; // 4 bytes
		CRunTimeObjectInfo *m_pNamespace = nullptr; // 4 bytes
		const ch8 *m_pName = nullptr;
		bool m_bIsStatic = true;

		
	};

	struct CRunTimeObjectInfoContainer : public CRunTimeObjectInfo
	{
		~CRunTimeObjectInfoContainer()
		{
			f_Destruct();
		}

		void f_Construct(const ch8 *_pName)
		{
			m_Name = _pName;
			CRunTimeObjectInfo::f_Construct(m_Name, nullptr, false);
		}

		virtual void *f_CreateObject() const
		{
			DMibError("Trying to create an run time object with an unregistered class");
		}

		NStr::CStr m_Name;
	};

	template <typename t_CObject, typename t_CCastClass = void>
	struct TCRuntimeClassNamed : public CRunTimeObjectInfo
	{
		TCRuntimeClassNamed(const ch8 *_pName, const ch8 *_pParent)
		{
			f_Construct(_pName, _pParent);
		}

		virtual void *f_CreateObject() const
		{
			if constexpr (NTraits::TCIsAbstract<t_CObject>::mc_Value)
				DMibError("Cannot construct an abstract class");
			else
				return fg_ConstructObject<t_CObject>(NMemory::CDefaultAllocator());
		}
	};

	CRunTimeObjectInfo *fg_GetRuntimeTypeInfo(const ch8 *_pObjectName);
	void *fg_CreateRuntimeTypeRawPtr(const ch8 *_pObjectName);
	template <typename tf_CType>
	NStorage::TCUniquePointer<tf_CType> fg_CreateRuntimeType(const ch8 *_pObjectName)
	{
		return fg_Explicit(static_cast<tf_CType *>(fg_CreateRuntimeTypeRawPtr(_pObjectName)));
	}

#	define DMibRuntimeClassNamedCastedBase(_Class, _Name, _Cast) ::NMib::TCRuntimeClassNamed<_Class, _Cast> g_RuntimeClassNamed_##_Class(#_Name, nullptr);
#	define DMibRuntimeClassNamedCasted(_Parent, _Class, _Name, _Cast) ::NMib::TCRuntimeClassNamed<_Class, _Cast> g_RuntimeClassNamed_##_Class(#_Name, #_Parent);
#	define DMibRuntimeClassMakeActive(_Name) NMib::NSys::fg_Compiler_MakeActive(0, &g_RuntimeClassNamed_##_Name)

#	define DMibRuntimeClassNamed(_Parent, _Class, _Name) DMibRuntimeClassNamedCasted(_Parent, _Class, _Name, void)
#	define DMibRuntimeClass(_Parent, _Class) DMibRuntimeClassNamed(_Parent, _Class, _Class)
#	define DMibRuntimeClassCasted(_Parent, _Class, _Cast) DMibRuntimeClassNamedCasted(_Parent, _Class, _Class, _Cast)

#	define DMibRuntimeClassBaseNamed(_Class, _Name) DMibRuntimeClassNamedCastedBase(_Class, _Name, void)
#	define DMibRuntimeClassBase(_Class) DMibRuntimeClassBaseNamed(_Class, _Class)
#	define DMibRuntimeClassBaseCasted(_Class, _Cast) DMibRuntimeClassNamedCastedBase(_Class, _Class, _Cast)

#	ifndef DMibPNoShortCuts
#		define DRuntimeClassNamedCastedBase(_Class, _Name, _Cast) DMibRuntimeClassNamedCastedBase(_Class, _Name, _Cast)
#		define DRuntimeClassNamedCasted(_Parent, _Class, _Name, _Cast) DMibRuntimeClassNamedCasted(_Parent, _Class, _Name, _Cast)

#		define DRuntimeClassNamed(_Parent, _Class, _Name) DMibRuntimeClassNamed(_Parent, _Class, _Name)
#		define DRuntimeClass(_Parent, _Class) DMibRuntimeClass(_Parent, _Class)
#		define DRuntimeClassCasted(_Parent, _Class, _Cast) DMibRuntimeClassCasted(_Parent, _Class, _Cast)

#		define DRuntimeClassBaseNamed(_Class, _Name) DMibRuntimeClassBaseNamed(_Class, _Name)
#		define DRuntimeClassBase(_Class) DMibRuntimeClassBase(_Class)
#		define DRuntimeClassBaseCasted(_Class, _Cast) DMibRuntimeClassBaseCasted(_Class, _Cast)
#		define DRuntimeClassMakeActive DMibRuntimeClassMakeActive
#	endif

}
