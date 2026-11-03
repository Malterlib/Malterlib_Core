// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>

namespace NMib
{
	namespace NMisc
	{
		template <typename tf_CContainer, typename tf_CType>
		auto fg_FindEqual(tf_CContainer &&_Container, tf_CType const &_ToFind) -> decltype(fg_Forward<tf_CContainer>(_Container).f_FindEqual(_ToFind))
		{
			return _Container.f_FindEqual(_ToFind);
		}

		template <typename tf_CContainer, typename tf_CType, typename tf_CDefault>
		auto fg_FindEqual(tf_CContainer &&_Container, tf_CType const &_ToFind, tf_CDefault const &_Default)
			-> NTraits::TCRemoveReference<decltype(*fg_Forward<tf_CContainer>(_Container).f_FindEqual(_ToFind))>
		{
			auto *pFind = _Container.f_FindEqual(_ToFind);
			if (pFind)
				return *pFind;
			return _Default;
		}

		template <typename tf_CFunction>
		void *fg_FunctionPtrToVoidPtr(tf_CFunction _Function)
		{
			void *pRet = (void * &)_Function;
			return pRet;
		}

		template <typename tf_CContainer, typename tf_CFunctor>
		void fg_ForEachAbortable(tf_CContainer &&_Container, tf_CFunctor &&_Functor)
		{
			auto Iter = _Container.f_GetIterator();
			while (Iter)
			{
				if (!fg_Forward<tf_CFunctor>(_Functor)(*Iter))
					break;
				++Iter;
			}
		}

		template <typename tf_CContainer, typename tf_CFunctor>
		void fg_ForEach(tf_CContainer &&_Container, tf_CFunctor &&_Functor)
		{
			auto Iter = _Container.f_GetIterator();
			while (Iter)
			{
				fg_Forward<tf_CFunctor>(_Functor)(*Iter);
				++Iter;
			}
		}

		template <typename tf_CInteger, typename tf_FOnCheck>
		void fg_ChunkRange(tf_CInteger _Start, tf_CInteger _End, tf_CInteger _ChunkSize, tf_FOnCheck &&_fOnChunk)
		{
			for (tf_CInteger iOutput = _Start; iOutput < _End;)
			{
				auto ThisTime = fg_Min(_ChunkSize, _End - iOutput);
				_fOnChunk(iOutput, ThisTime);
				iOutput += ThisTime;
			}
		}

		template <typename tf_CCast>
		tf_CCast *CClassContainer::f_Get() const
		{
			return (tf_CCast *)fp_GetPointer();
		}

		inline aint CClassContainer::f_GetIdentifier() const
		{
			return fp_GetIdentifier();
		}

		inline umint CClassContainer::f_GetContext() const
		{
			return fp_GetContext();
		}

		template <typename t_CType>
		TCClassContainer<t_CType>::TCClassContainer(t_CType const &_Containee, aint _Identifier)
		{
			m_pContainee = &_Containee;
			m_Identifier = _Identifier;
			m_Context = 0;
		}

		template <typename t_CType>
		TCClassContainer<t_CType>::TCClassContainer(t_CType const &_Containee, aint _Identifier, umint _Context)
		{
			m_pContainee = &_Containee;
			m_Identifier = _Identifier;
			m_Context = _Context;
		}

		template <typename t_CType>
		aint TCClassContainer<t_CType>::fp_GetIdentifier() const
		{
			return m_Identifier;
		}

		template <typename t_CType>
		umint TCClassContainer<t_CType>::fp_GetContext() const
		{
			return m_Context;
		}

		template <typename t_CType>
		void *TCClassContainer<t_CType>::fp_GetPointer() const
		{
			return (void *)m_pContainee;
		}

		template <typename tf_CType>
		TCClassContainer<tf_CType> fg_GetClassContainer(tf_CType const &_Object)
		{
			return fg_Move(TCClassContainer<tf_CType>(_Object, TCTypeID<tf_CType>::mc_TypeID));
		}

		template <typename tf_CType>
		TCClassContainer<tf_CType> fg_GetClassContainer(tf_CType const &_Object, umint _Context)
		{
			return fg_Move(TCClassContainer<tf_CType>(_Object, TCTypeID<tf_CType>::mc_TypeID, _Context));
		}

		constexpr CRandomShiftRNG::CRandomShiftRNG(uint32 _Seed0, uint32 _Seed1, uint32 _Seed2)
		{
			if (_Seed0 != 0)
				mp_StateX = _Seed0;
			else
				mp_StateX = 123456789;
			if (_Seed1 != 0)
				mp_StateY = _Seed1;
			else
				mp_StateY = 362436069;
			if (_Seed2 != 0)
				mp_StateZ = _Seed2;
			else
				mp_StateZ = 2521288629;
			// Propagate seed
			f_GetValue<uint32>();
			f_GetValue<uint32>();
			f_GetValue<uint32>();
		}

		template <typename tf_CInt>
		constexpr inline_small tf_CInt CRandomShiftRNG::f_GetValue()
		{
			uint32 Temp;
			mp_StateX ^= mp_StateX << 16;
			mp_StateX ^= mp_StateX >> 5;
			mp_StateX ^= mp_StateX << 1;

			Temp = mp_StateX;
			mp_StateX = mp_StateY;
			mp_StateY = mp_StateZ;
			mp_StateZ = Temp ^ mp_StateX ^ mp_StateY;

			if constexpr (sizeof(tf_CInt) > 4)
			{
				tf_CInt Return = mp_StateZ;
				for (umint iWord = 1; iWord < (sizeof(tf_CInt) + 3) / 4; ++iWord)
				{
					Return <<= 32;
					Return |= f_GetValue<uint32>();
				}

				return Return;
			}
			else
				return (tf_CInt)mp_StateZ;
		}

		template <typename tf_CInt>
		constexpr inline_small tf_CInt CRandomShiftRNG::f_GetValue(tf_CInt _Max)
		{
			return f_GetValue<tf_CInt>() % _Max;
		}

		template <typename tf_CInt>
		constexpr inline_small tf_CInt CRandomShiftRNG::f_GetValue(tf_CInt _Min, tf_CInt _Max)
		{
			return _Min + f_GetValue<tf_CInt>() % (_Max - _Min);
		}

		constexpr inline_small uint32 CRandomShiftRNG::fs_Max()
		{
			return TCLimitsInt<uint32>::mc_Max;
		}

		static inline_small int32 fg_GetRandom()
		{
			return fg_RandomThreadLocal().f_GetValue<int32>() & 0x7fffffff;
		}

		static inline_small uint32 fg_GetRandomUnsigned()
		{
			return fg_RandomThreadLocal().f_GetValue<uint32>();
		}

		static inline_small fp64 fg_GetRandomFloat()
		{
			return fp64(fg_RandomThreadLocal().f_GetValue<uint32>()) / fp64(fg_RandomThreadLocal().fs_Max());
		}

		static inline_small fp64 fg_GetRandomFloatFullPrecision()
		{
			struct
			{
				uint32 m_Random[2];
			} Random;

			auto &ThreadLocal = fg_RandomThreadLocal();

			Random.m_Random[0] = ThreadLocal.f_GetValue<uint32>();
			Random.m_Random[1] = ThreadLocal.f_GetValue<uint32>();

			uint64 RandomInt64 = fg_BitCast<uint64>(Random);

			return fp64(pfp64(RandomInt64)) / fp64(pfp64(TCLimitsInt<uint64>::mc_Max));
		}

		static inline_small void fg_SetRanmomSeed(aint _Seed)
		{
			fg_RandomThreadLocal() = CRandomShiftRNG(_Seed);
		}

		static inline_small CRandomShiftRNG &fg_Random()
		{
			return fg_RandomThreadLocal();
		}

		template <typename tf_CIntType>
		tf_CIntType fg_GetHighEntropyRandomInteger()
		{
			static_assert(sizeof(tf_CIntType) <= 16);
			tf_CIntType Return;
			NSys::fg_Security_GenerateHighEntropyData((uint8 *)&Return, sizeof(Return));
			return Return;
		}

		template <typename tf_CLeft, typename tf_CRight>
		inline_small ECompareResult fg_Compare(tf_CLeft const &_Left, tf_CRight const &_Right)
		{
			if (_Left < _Right)
				return ECompareResult_LessThan;
			if (_Right < _Left)
				return ECompareResult_GreaterThan;
			return ECompareResult_Equal;
		}

		template <typename tf_CLeft, typename tf_CRight>
		inline_small ECompareResult fg_Compare(ECompareResult _LastResult, tf_CLeft const &_Left, tf_CRight const &_Right)
		{
			if (!(_LastResult == ECompareResult_Equal))
				return _LastResult;
			if (_Left < _Right)
				return ECompareResult_LessThan;
			if (_Right < _Left)
				return ECompareResult_GreaterThan;
			return ECompareResult_Equal;
		}

		// Don't quite know where to put this.
		// Looks for the pattern "$(Identifier)" and replaces it with the result from the lookup func.
		// If the lookup func returns false no substitution is made.
		template <typename tf_CLookupFunc, typename tf_CString> // Of the form: bool Func(tf_CString const &_VarName, tf_CString &_Value)
		tf_CString fg_EvaluateStringWithVariables(tf_CString const &_Str, tf_CLookupFunc &&_LookupFunc)
		{
			int iDollarPos = _Str.f_FindChar('$');
			if (iDollarPos == -1)
				return _Str;
			else
			{
				tf_CString Result, OptionName, Value;
				typename tf_CString::CChar const *pRawValue = _Str.f_GetStr();
				typename tf_CString::CChar const *pStartValue = _Str.f_GetStr();
				aint nParsed;

				while (iDollarPos != -1)
				{
					if (iDollarPos > 0)
						Result.f_AddStr(pRawValue, iDollarPos);

					pRawValue += iDollarPos;
					pStartValue = pRawValue;
					pRawValue += (typename tf_CString::CParse("$({})") >> OptionName).f_Parse(pRawValue, nParsed);

					if (nParsed)
					{
						OptionName = OptionName.f_Trim();

						if (fg_Forward<tf_CLookupFunc>(_LookupFunc)(OptionName, Value))
							Result += Value;
						else
							pRawValue = pStartValue;
					}
					else
						pRawValue = pStartValue;

					if (pRawValue == pStartValue)
					{
						Result.f_AddStr("$");
						++pRawValue;
					}

					iDollarPos = NStr::fg_StrFindChar(pRawValue, '$');
					if (iDollarPos == -1)
						Result.f_AddStr(pRawValue, NStr::fg_StrLen(pRawValue));
				}
				return Result;
			}
		}

		template <typename tf_CType, typename tf_FCompare, typename tf_CFind>
		constexpr aint fg_BinarySearch(tf_CType const *_pArray, umint _Len, tf_CFind const &_ToFind, tf_FCompare &&_fCompare)
		{
			umint Low = 0;
			umint High = _Len;

			tf_CType const *pArray = _pArray;

			while (Low < High)
			{
				umint Mid = (Low + High) >> 1;
				if (fg_CheckOrdering(_fCompare(pArray[Mid], _ToFind)) < 0)
					Low = Mid + 1;
				else
					High = Mid;
			}

			if (Low < _Len && fg_CheckOrdering(_fCompare(pArray[Low], _ToFind)) == 0)
				return Low;
			else
				return -1;
		}

		template <typename tf_CType, typename tf_CFind>
		constexpr aint fg_BinarySearch(tf_CType const *_pArray, umint _Len, tf_CFind const &_ToFind)
		{
			return fg_BinarySearch(_pArray, _Len, _ToFind, CSort_Default());
		}

		template <typename tf_CType, typename tf_FCompare>
		constexpr bool fg_IsSorted(tf_CType const *_pArray, umint _Len, tf_FCompare &&_fCompare)
		{
			tf_CType const *pArray = _pArray;
			for (umint iPrevious = 0, iCurrent = 1; iCurrent != _Len; ++iPrevious, ++iCurrent)
			{
				if (fg_CheckOrdering(_fCompare(pArray[iCurrent], pArray[iPrevious])) < 0)
					return false;
			}

			return true;
		}

		template <typename tf_CType>
		constexpr bool fg_IsSorted(tf_CType const *_pArray, umint _Len)
		{
			return fg_IsSorted(_pArray, _Len, CSort_Default());
		}

		template <typename tf_CType, typename tf_FCompare, typename tf_CFind>
		constexpr aint fg_BinarySearchLowerBound(tf_CType const *_pArray, umint _Len, tf_CFind const &_ToFind, tf_FCompare &&_fCompare)
		{
			umint Low = 0;
			umint High = _Len;

			if (High == 0)
				return -1;

			tf_CType const *pArray = _pArray;

			while (Low < High)
			{
				umint Mid = (Low + High) >> 1;
				if (fg_CheckOrdering(_fCompare(pArray[Mid], _ToFind)) < 0)
					Low = Mid + 1;
				else
					High = Mid;
			}

			return Low;
		}

		template <typename tf_CType, typename tf_CFind>
		constexpr aint fg_BinarySearchLowerBound(tf_CType const *_pArray, umint _Len, tf_CFind const &_ToFind)
		{
			return fg_BinarySearchLowerBound(_pArray, _Len, _ToFind, CSort_Default());
		}
	} // Namespace NMisc

	namespace NContainer
	{
		template <typename tf_CContainer>
		constexpr inline_always bool operator == (tf_CContainer &_Container, CIteratorEndSentinel const &_EndSentinel) noexcept
		{
			return !_Container;
		}

		template <typename tf_CContainer>
		constexpr inline_always auto begin(tf_CContainer &&_Container) -> decltype(_Container.f_GetIterator())
		{
			return _Container.f_GetIterator();
		}

		template <typename tf_CContainer>
		constexpr inline_always CIteratorEndSentinel end(tf_CContainer &&_Container, TCEnableIf<!NTraits::cIsVoid<decltype(_Container.f_GetIterator())>, bool> _bHasIterator)
		{
			return CIteratorEndSentinel();
		}

		template <typename tf_CType>
		constexpr inline_always tf_CType *begin(TCVector<tf_CType> &_Container)
		{
			return _Container.f_GetArray();
		}

		template <typename tf_CType>
		constexpr inline_always tf_CType const *begin(TCVector<tf_CType> const &_Container)
		{
			return _Container.f_GetArray();
		}

		template <typename tf_CType>
		constexpr inline_always tf_CType *end(TCVector<tf_CType> &_Container)
		{
			return _Container.f_GetArray() + _Container.f_GetLen();
		}

		template <typename tf_CType>
		constexpr inline_always tf_CType const *end(TCVector<tf_CType> const &_Container)
		{
			return _Container.f_GetArray() + _Container.f_GetLen();
		}
	}

	namespace NStr
	{
		template <typename tf_CType>
		constexpr inline_always typename TCStr<tf_CType>::CChar const *begin(TCStr<tf_CType> const &_String)
		{
			return _String.f_GetStr();
		}

		template <typename tf_CType>
		constexpr inline_always typename TCStr<tf_CType>::CChar const *end(TCStr<tf_CType> const &_String)
		{
			return _String.f_GetStr() + _String.f_GetLen();
		}
	}
} // Namespace NMib
