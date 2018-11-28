// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib
{
//	template <aint t_Dummy = 0>
	class CRunTimeObjectInfo
	{		
	public:
		CRunTimeObjectInfo()
		{
			m_pParent = nullptr;
			m_pNamespace = nullptr;
			m_pName = nullptr;
			m_bIsStatic = true;
		}
//		DMibListLinkD_Link(CRunTimeObjectInfo, m_Link);

		operator const ch8 *() const
		{
			return m_pName;
		}
		typedef NIntrusive::TCTreeCompare_String< CRunTimeObjectInfo, ch8 const *> CCompare;
		DMibIntrusiveLink(CRunTimeObjectInfo, NIntrusive::TCAVLLink<>, m_TreeLinkNamespace);
		DMibListLinkDS_Link(CRunTimeObjectInfo, m_TreeLinkChild); // 8 bytes
		DMibListLinkDS_List(CRunTimeObjectInfo, m_TreeLinkChild) m_Children; // 4 bytes
		NIntrusive::TCAVLTree<CRunTimeObjectInfo::CLinkTraits_m_TreeLinkNamespace, CRunTimeObjectInfo::CCompare> m_Namespace;
		typedef NIntrusive::TCAVLTree<CRunTimeObjectInfo::CLinkTraits_m_TreeLinkNamespace, CRunTimeObjectInfo::CCompare>::CIterator CNamespaceIterator;
		CRunTimeObjectInfo *m_pParent; // 4 bytes
		CRunTimeObjectInfo *m_pNamespace; // 4 bytes
		const ch8 *m_pName;
		bint m_bIsStatic;
		// Total of 42 bytes + 4 bytes for virtual table = 46 bytes
//		const ch8 *m_pNamespace;
//		const ch8 *m_pParentName;

		NStr::CStr f_GetName();
		NStr::CStr f_GetNamespaceName();
		NStr::CStr f_GetFullName();

		static CRunTimeObjectInfo *f_GetObject(const ch8*_pName);

		void f_ForEachLeafChild(NFunction::TCFunction<void (CRunTimeObjectInfo const &_RuntimeObjectInfo)> const &_fOnChild) const;
		void f_Construct(const ch8 *_pName, const ch8 *_pParent, bint _bIsStatic = true);
		void f_Destruct();

		virtual ~CRunTimeObjectInfo();

		virtual void *f_CreateObject() const = 0;
	};

	class CRunTimeObjectInfoContainer : public CRunTimeObjectInfo
	{
	public:
		NStr::CStr m_Name;

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
	};

	template <typename t_CObject, typename t_CCastClass = void>
	class TCRuntimeClassNamed : public CRunTimeObjectInfo
	{
	public:
		TCRuntimeClassNamed(const ch8 *_pName, const ch8 *_pParent)
		{
			f_Construct(_pName, _pParent);
		}

		virtual void *f_CreateObject() const
		{
			if constexpr (NTraits::TCIsAbstract<t_CObject>::mc_Value)
				DMibError("Cannot construct an abstract class");
			else
				return (t_CCastClass *)DMibNew t_CObject();
		}
	};

	CRunTimeObjectInfo *fg_GetRuntimeTypeInfo(const ch8 *_pObjectName);
	void *fg_CreateRuntimeType(const ch8 *_pObjectName);
	template <typename tf_CType>
	NPtr::TCUniquePointer<tf_CType> fg_CreateRuntimeType(const ch8 *_pObjectName)
	{
		return fg_Explicit(static_cast<tf_CType *>(fg_CreateRuntimeType(_pObjectName)));
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
