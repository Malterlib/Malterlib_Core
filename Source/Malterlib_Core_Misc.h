// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>

namespace NMib::NCryptography
{
	struct CSecureRandom;
}

namespace NMib
{
	namespace NMisc
	{
		NStr::CStr fg_FormatTime(NTime::CTime &_Time);
		NStr::CStr fg_FormatTimeFileName(NTime::CTime &_Time);
		bool fg_CheckAccessRights(NStr::CStr const &_Path, bool _bRandom = true);
		bool fg_CheckAccessRights(NStr::CStrNonTracked const &_Path, bool _bRandom = true);
		bool fg_CheckFileAccessRights(NStr::CStr const &_Path);

		template <typename tf_CContainer, typename tf_CType>
		auto fg_FindEqual(tf_CContainer &&_Container, tf_CType const &_ToFind) -> decltype(fg_Forward<tf_CContainer>(_Container).f_FindEqual(_ToFind));

		template <typename tf_CContainer, typename tf_CType, typename tf_CDefault>
		auto fg_FindEqual(tf_CContainer &&_Container, tf_CType const &_ToFind, tf_CDefault const &_Default)
			-> NTraits::TCRemoveReference<decltype(*fg_Forward<tf_CContainer>(_Container).f_FindEqual(_ToFind))>
		;

		template <typename tf_CFunction>
		void *fg_FunctionPtrToVoidPtr(tf_CFunction _Function);

		template <typename tf_CContainer, typename tf_CFunctor>
		void fg_ForEachAbortable(tf_CContainer &&_Container, tf_CFunctor &&_Functor);

		template <typename tf_CContainer, typename tf_CFunctor>
		void fg_ForEach(tf_CContainer &&_Container, tf_CFunctor &&_Functor);

		template <typename tf_CInteger, typename tf_FOnCheck>
		void fg_ChunkRange(tf_CInteger _Start, tf_CInteger _End, tf_CInteger _ChunkSize, tf_FOnCheck &&_fOnChunk);

		template <typename t_CType>
		struct TCTypeID
		{
		};

		template <int t_TypeID>
		struct TCTypeIDReverse
		{
		};

#define DMibTypeID(d_Class, d_ID) \
	namespace NMib{namespace NMisc{template <> struct TCTypeID<d_Class > {static constexpr aint mc_TypeID = d_ID;}; template <> struct TCTypeIDReverse<d_ID> {using CType = d_Class;};}}

		struct CClassContainer
		{
			CClassContainer();

			CClassContainer(CClassContainer const &_Other); // Ignore the link.

			virtual ~CClassContainer();

			DMibListLinkD_Link(CClassContainer, m_Link);

			void *f_GetVoidPtr() const;

			template <typename tf_CCast>
			tf_CCast *f_Get() const;

			/*
			// Todo add dynamic_cast system
			template <typename t_CCast, typename t_CBase>
			t_CCast *f_GetDynamic() const
			{
				return dynamic_cast<t_CCast *>((t_CBase *)fp_GetPointer());
			}
			*/

			inline_small aint f_GetIdentifier() const;

			inline_small umint f_GetContext() const;

		protected:
			virtual void *fp_GetPointer() const = 0;
			virtual aint fp_GetIdentifier() const = 0;
			virtual umint fp_GetContext() const = 0;

		private:
		};

		template <typename t_CType>
		struct TCClassContainer : public CClassContainer
		{
			t_CType const *m_pContainee;
			aint m_Identifier;
			umint m_Context;

			TCClassContainer(t_CType const &_Containee, aint _Identifier);

			TCClassContainer(t_CType const &_Containee, aint _Identifier, umint _Context);

		protected:
			virtual aint fp_GetIdentifier() const;

			virtual umint fp_GetContext() const;

			virtual void *fp_GetPointer() const;
		};

		template <typename tf_CType>
		TCClassContainer<tf_CType> fg_GetClassContainer(tf_CType const &_Object);

		template <typename tf_CType>
		TCClassContainer<tf_CType> fg_GetClassContainer(tf_CType const &_Object, umint _Context);

#define DGetCC(d_Object)	NMib::NMisc::fg_GetClassContainer(d_Object).f_GetVoidPtr()
#define DGetCCContext(d_Object, d_Context)	NMib::NMisc::fg_GetClassContainer(d_Object, d_Context).f_GetVoidPtr()

		struct CClassContainerList
		{
			DMibListLinkD_List(CClassContainer, m_Link) m_List;
			using CIter = DMibListLinkD_Iter(CClassContainer, m_Link);

			~CClassContainerList();
		};

		// NOLINTNEXTLINE(malterlib-conceptual-prefix): CMibArgList is va_list, a pointer on macOS and Windows and an array on Linux
		CClassContainerList *fg_GetClassContainerListArgList(CClassContainerList &_List, CMibArgList &_Args);
		CClassContainerList *fg_GetClassContainerList(CClassContainerList *_pList, ...);

		// Marsaglias xorshf generator, period 2^96-1
		struct CRandomShiftRNG
		{
			constexpr CRandomShiftRNG(uint32 _Seed0 = 123456789, uint32 _Seed1 = 362436069, uint32 _Seed2 = 2521288629);

			template <typename tf_CInt>
			constexpr inline_small tf_CInt f_GetValue();

			template <typename tf_CInt>
			constexpr inline_small tf_CInt f_GetValue(tf_CInt _Max);

			template <typename tf_CInt>
			constexpr inline_small tf_CInt f_GetValue(tf_CInt _Min, tf_CInt _Max);

			static constexpr inline_small uint32 fs_Max();

		private:
			uint32 mp_StateX;
			uint32 mp_StateY;
			uint32 mp_StateZ;
		};

		struct CAutoRandom : public CRandomShiftRNG
		{
			CAutoRandom();
		};

		CRandomShiftRNG &fg_RandomThreadLocal();

		NCryptography::CSecureRandom &fg_SecureRandomThreadLocal();

		static inline_small int32 fg_GetRandom();

		static inline_small uint32 fg_GetRandomUnsigned();

		static inline_small fp64 fg_GetRandomFloat();

		static inline_small fp64 fg_GetRandomFloatFullPrecision();

		// Secure random functions (ChaCha20-based CSPRNG)
		// Use these for security-sensitive operations (tokens, keys, nonces, etc.)
		int32 fg_GetSecureRandom();
		uint32 fg_GetSecureRandomUnsigned();
		fp64 fg_GetSecureRandomFloat();

		static inline_small void fg_SetRanmomSeed(aint _Seed);

		static inline_small CRandomShiftRNG &fg_Random();

		template <typename tf_CIntType>
		tf_CIntType fg_GetHighEntropyRandomInteger();

		struct CDeadlockDetocterPause
		{
			CDeadlockDetocterPause();

			~CDeadlockDetocterPause();
		};

#define DMibDeadlockDetectorPause NMib::NMisc::CDeadlockDetocterPause MalterlibDeadlockDetectorPauseScope;
#ifndef DMibPNoShortCuts
#	define DDeadlockDetectorPause DMibDeadlockDetectorPause
#endif

		enum ECompareResult
		{
			ECompareResult_LessThan      = -1
			, ECompareResult_Equal       = 0
			, ECompareResult_GreaterThan = 1
		};

		template <typename tf_CLeft, typename tf_CRight>
		inline_small ECompareResult fg_Compare(tf_CLeft const &_Left, tf_CRight const &_Right);

		template <typename tf_CLeft, typename tf_CRight>
		inline_small ECompareResult fg_Compare(ECompareResult _LastResult, tf_CLeft const &_Left, tf_CRight const &_Right);

		template <typename tf_CLookupFunc, typename tf_CString>
		tf_CString fg_EvaluateStringWithVariables(tf_CString const &_Str, tf_CLookupFunc &&_LookupFunc);

		template <typename tf_CType, typename tf_FCompare, typename tf_CFind>
		constexpr aint fg_BinarySearch(tf_CType const *_pArray, umint _Len, tf_CFind const &_ToFind, tf_FCompare &&_fCompare);

		template <typename tf_CType, typename tf_CFind>
		constexpr aint fg_BinarySearch(tf_CType const *_pArray, umint _Len, tf_CFind const &_ToFind);

		template <typename tf_CType, typename tf_FCompare>
		constexpr bool fg_IsSorted(tf_CType const *_pArray, umint _Len, tf_FCompare &&_fCompare);

		template <typename tf_CType>
		constexpr bool fg_IsSorted(tf_CType const *_pArray, umint _Len);

		template <typename tf_CType, typename tf_FCompare, typename tf_CFind>
		constexpr aint fg_BinarySearchLowerBound(tf_CType const *_pArray, umint _Len, tf_CFind const &_ToFind, tf_FCompare &&_fCompare);

		template <typename tf_CType, typename tf_CFind>
		constexpr aint fg_BinarySearchLowerBound(tf_CType const *_pArray, umint _Len, tf_CFind const &_ToFind);
	}

	namespace NContainer
	{
		// STL iterator adaptors used in new for syntax for (auto &iItem : Container)

		struct CIteratorEndSentinel
		{
			template <typename tf_CContainer>
			constexpr friend bool operator == (tf_CContainer &_Container, CIteratorEndSentinel const &_EndSentinel) noexcept;
		};

		template <typename tf_CContainer>
		constexpr inline_always auto begin(tf_CContainer &&_Container) -> decltype(_Container.f_GetIterator());

		template <typename tf_CContainer>
		constexpr inline_always CIteratorEndSentinel end(tf_CContainer &&_Container, TCEnableIf<!NTraits::cIsVoid<decltype(_Container.f_GetIterator())>, bool> _bHasIterator = true);

		template <typename tf_CType>
		constexpr inline_always tf_CType *begin(TCVector<tf_CType> &_Container);

		template <typename tf_CType>
		constexpr inline_always tf_CType const *begin(TCVector<tf_CType> const &_Container);

		template <typename tf_CType>
		constexpr inline_always tf_CType *end(TCVector<tf_CType> &_Container);

		template <typename tf_CType>
		constexpr inline_always tf_CType const *end(TCVector<tf_CType> const &_Container);
	}

	namespace NStr
	{
		template <typename tf_CType>
		constexpr inline_always typename TCStr<tf_CType>::CChar const *begin(TCStr<tf_CType> const &_String);

		template <typename tf_CType>
		constexpr inline_always typename TCStr<tf_CType>::CChar const *end(TCStr<tf_CType> const &_String);
	}

	namespace NIntrusive
	{
		using NContainer::CIteratorEndSentinel;
		using NContainer::begin;
		using NContainer::end;
	}
	namespace NStr
	{
		using NContainer::CIteratorEndSentinel;
		using NContainer::begin;
		using NContainer::end;
	}
}

#include "Malterlib_Core_Misc.hpp"
