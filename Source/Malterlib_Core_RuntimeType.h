// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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
			ch8 const *operator () (CRunTimeObjectInfo const &_Info);
			auto operator () (ch8 const *_pLeft, ch8 const *_pRight);
		};

		virtual ~CRunTimeObjectInfo();

		NStr::CStr f_GetName();
		NStr::CStr f_GetNamespaceName();
		NStr::CStr f_GetFullName();

		static CRunTimeObjectInfo *fs_GetObject(ch8 const *_pName, umint _NameLen);

		void f_ForEachLeafChild(NFunction::TCFunction<void (CRunTimeObjectInfo const &_RuntimeObjectInfo)> const &_fOnChild) const;
		void f_Construct(ch8 const *_pName, ch8 const *_pParent, bool _bIsStatic = true);
		void f_Destruct();

		virtual void *f_CreateObject() const = 0;

		DMibListLinkDS_Link(CRunTimeObjectInfo, m_TreeLinkChild); // 8 bytes
		DMibListLinkDS_List(CRunTimeObjectInfo, m_TreeLinkChild) m_Children; // 4 bytes
		NIntrusive::TCAVLTree<&CRunTimeObjectInfo::m_TreeLinkNamespace, CCompare, NMib::NMemory::CDefaultAllocator, CRunTimeObjectInfo> m_Namespace;
		CRunTimeObjectInfo *m_pParent = nullptr;    // 4 bytes
		CRunTimeObjectInfo *m_pNamespace = nullptr; // 4 bytes
		ch8 const *m_pName = nullptr;
		bool m_bIsStatic = true;
	};

	struct CRunTimeObjectInfoContainer : public CRunTimeObjectInfo
	{
		~CRunTimeObjectInfoContainer();

		void f_Construct(ch8 const *_pName);

		virtual void *f_CreateObject() const;

		NStr::CStr m_Name;
	};

	template <typename t_CObject, typename t_CCastClass = void>
	struct TCRuntimeClassNamed : public CRunTimeObjectInfo
	{
		TCRuntimeClassNamed(ch8 const *_pName, ch8 const *_pParent);

		virtual void *f_CreateObject() const;
	};

	CRunTimeObjectInfo *fg_GetRuntimeTypeInfo(ch8 const *_pObjectName);
	void *fg_CreateRuntimeTypeRawPtr(ch8 const *_pObjectName);
	template <typename tf_CType>
	NStorage::TCUniquePointer<tf_CType> fg_CreateRuntimeType(ch8 const *_pObjectName);

#define DMibRuntimeClassNamedCastedBase(d_Class, d_Name, d_Cast) ::NMib::TCRuntimeClassNamed<d_Class, d_Cast> g_RuntimeClassNamed_##d_Class(#d_Name, nullptr);
#define DMibRuntimeClassNamedCasted(d_Parent, d_Class, d_Name, d_Cast) ::NMib::TCRuntimeClassNamed<d_Class, d_Cast> g_RuntimeClassNamed_##d_Class(#d_Name, #d_Parent);
#define DMibRuntimeClassMakeActive(d_Name) NMib::NSys::fg_Compiler_MakeActive(0, &g_RuntimeClassNamed_##d_Name)

#define DMibRuntimeClassNamed(d_Parent, d_Class, d_Name) DMibRuntimeClassNamedCasted(d_Parent, d_Class, d_Name, void)
#define DMibRuntimeClass(d_Parent, d_Class) DMibRuntimeClassNamed(d_Parent, d_Class, d_Class)
#define DMibRuntimeClassCasted(d_Parent, d_Class, d_Cast) DMibRuntimeClassNamedCasted(d_Parent, d_Class, d_Class, d_Cast)

#define DMibRuntimeClassBaseNamed(d_Class, d_Name) DMibRuntimeClassNamedCastedBase(d_Class, d_Name, void)
#define DMibRuntimeClassBase(d_Class) DMibRuntimeClassBaseNamed(d_Class, d_Class)
#define DMibRuntimeClassBaseCasted(d_Class, d_Cast) DMibRuntimeClassNamedCastedBase(d_Class, d_Class, d_Cast)

#ifndef DMibPNoShortCuts
#	define DRuntimeClassNamedCastedBase(d_Class, d_Name, d_Cast) DMibRuntimeClassNamedCastedBase(d_Class, d_Name, d_Cast)
#	define DRuntimeClassNamedCasted(d_Parent, d_Class, d_Name, d_Cast) DMibRuntimeClassNamedCasted(d_Parent, d_Class, d_Name, d_Cast)

#	define DRuntimeClassNamed(d_Parent, d_Class, d_Name) DMibRuntimeClassNamed(d_Parent, d_Class, d_Name)
#	define DRuntimeClass(d_Parent, d_Class) DMibRuntimeClass(d_Parent, d_Class)
#	define DRuntimeClassCasted(d_Parent, d_Class, d_Cast) DMibRuntimeClassCasted(d_Parent, d_Class, d_Cast)

#	define DRuntimeClassBaseNamed(d_Class, d_Name) DMibRuntimeClassBaseNamed(d_Class, d_Name)
#	define DRuntimeClassBase(d_Class) DMibRuntimeClassBase(d_Class)
#	define DRuntimeClassBaseCasted(d_Class, d_Cast) DMibRuntimeClassBaseCasted(d_Class, d_Cast)
#	define DRuntimeClassMakeActive DMibRuntimeClassMakeActive
#endif
}

#include "Malterlib_Core_RuntimeType.hpp"
