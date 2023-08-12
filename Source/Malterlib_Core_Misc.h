// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

#define DMibInterfaceSingle(_InterfaceName, _Implementation) class _Implementation; typedef _Implementation CIImp ## _InterfaceName;
#define DMibInterfaceVirtual(_InterfaceName) typedef NMib::CEmpty CIImp ## _InterfaceName;
#define DMibInterfaceImp(_InterfaceName) typedef NMib::TCChooseType<NMib::NTraits::TCIsSame<CIImp ## _InterfaceName, NMib::CEmpty>::mc_Value, IImp ## _InterfaceName, CIImp ## _InterfaceName>::CType CImp;\
	typedef NMib::TCChooseType<NMib::NTraits::TCIsSame<CIImp ## _InterfaceName, NMib::CEmpty>::mc_Value, IImp ## _InterfaceName, NMib::CEmpty>::CType CBase;
#ifndef DMibPNoShortCuts

#define DInterfaceSingle DMibInterfaceSingle
#define DInterfaceVirtual DMibInterfaceVirtual
#define DInterfaceImp DMibInterfaceImp

#endif

namespace NMib
{
	namespace NMisc
	{
		NStr::CStr fg_FormatTime(NTime::CTime &_Time);
		NStr::CStr fg_FormatTimeFileName(NTime::CTime &_Time);
		bool fg_CheckAccessRights(NStr::CStr const& _Path, bool _bRandom = true);
		bool fg_CheckAccessRights(NStr::CStrNonTracked const& _Path, bool _bRandom = true);
		bool fg_CheckFileAccessRights(NStr::CStr _Path);
		
 
		template <typename tf_CContainer, typename tf_CType>
		auto fg_FindEqual(tf_CContainer &&_Container, const tf_CType &_ToFind) -> decltype(fg_Forward<tf_CContainer>(_Container).f_FindEqual(_ToFind))
		{
			return _Container.f_FindEqual(_ToFind);
		}

		template <typename tf_CContainer, typename tf_CType, typename tf_CDefault>
		auto fg_FindEqual(tf_CContainer &&_Container, const tf_CType &_ToFind, const tf_CDefault &_Default)
			-> typename NTraits::TCRemoveReference<decltype(*fg_Forward<tf_CContainer>(_Container).f_FindEqual(_ToFind))>::CType
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

		template <typename t_CType>
		class TCTypeID
		{
			public:		
		};

		template <int t_TypeID>
		class TCTypeIDReverse
		{
			public:		
		};
		
#		define DMibTypeID(_Class, _ID) namespace NMib{namespace NMisc{template <> class TCTypeID<_Class > {public: enum {ETypeID = _ID};}; template <> class TCTypeIDReverse<_ID> {public: typedef _Class CType;};}}
		
		
		class CClassContainer
		{
			private:
			public:

			CClassContainer() {}
			CClassContainer(CClassContainer const&) {} // Ignore the link.
			
			virtual ~CClassContainer()
			{
			}
			
			DMibListLinkD_Link(CClassContainer, m_Link);
			
			void *f_GetVoidPtr() const
			{
				return (void *)this;
			}
			
			template <typename t_CCast>
			t_CCast *f_Get() const
			{
				return (t_CCast *)fp_GetPointer();
			}
			/*
			// Todo add dynamic_cast system
			template <typename t_CCast, typename t_CBase>
			t_CCast *f_GetDynamic() const
			{
				return dynamic_cast<t_CCast *>((t_CBase *)fp_GetPointer());
			}
			*/

			inline_small aint f_GetIdentifier() const
			{
				return fp_GetIdentifier();
			}

			inline_small mint f_GetContext() const
			{
				return fp_GetContext();
			}
		protected:
			virtual void *fp_GetPointer() const = 0;
			virtual aint fp_GetIdentifier() const = 0;
			virtual mint fp_GetContext() const = 0;
									
		};
		
		template <typename t_CType>
		class TCClassContainer : public CClassContainer
		{
			public:
			
			const t_CType *m_pContainee;
			aint m_Identifier;
			mint m_Context;
			
			TCClassContainer(const t_CType &_Containee, aint _Identifier)
			{
				m_pContainee = &_Containee;
				m_Identifier = _Identifier;
				m_Context = 0;
			}
			TCClassContainer(const t_CType &_Containee, aint _Identifier, mint _Context)
			{
				m_pContainee = &_Containee;
				m_Identifier = _Identifier;
				m_Context = _Context;
			}
			
		protected:
			virtual aint fp_GetIdentifier() const
			{
				return m_Identifier;
			}

			virtual mint fp_GetContext() const
			{
				return m_Context;
			}
			
			virtual void *fp_GetPointer() const
			{
				return (void *)m_pContainee;
			}
		};
		
		template <typename t_CType>
		TCClassContainer<t_CType> fg_GetClassContainer(const t_CType &_Object)
		{
			return fg_Move(TCClassContainer<t_CType>(_Object, TCTypeID<t_CType>::ETypeID));
		}

		template <typename t_CType>
		TCClassContainer<t_CType> fg_GetClassContainer(const t_CType &_Object, mint _Context)
		{
			return fg_Move(TCClassContainer<t_CType>(_Object, TCTypeID<t_CType>::ETypeID, _Context));
		}
		
#		define DGetCC(_Object)	NMib::NMisc::fg_GetClassContainer(_Object).f_GetVoidPtr()
#		define DGetCCContext(_Object, _Context)	NMib::NMisc::fg_GetClassContainer(_Object, _Context).f_GetVoidPtr()
		
		class CClassContainerList
		{
			public:
			
			DMibListLinkD_List(CClassContainer, m_Link) m_List;
			typedef DMibListLinkD_Iter(CClassContainer, m_Link) CIter;
			
			~CClassContainerList()
			{
			}					
		};	

		CClassContainerList *fg_GetClassContainerListArgList(CClassContainerList &_List, CMibArgList &_Args);
		CClassContainerList *fg_GetClassContainerList(CClassContainerList *_pList, ...);

		// Marsaglias xorshf generator, period 2^96-1
		class CRandomShiftRNG
		{
		public:

			constexpr CRandomShiftRNG(uint32 _Seed0 = 123456789, uint32 _Seed1 = 362436069, uint32 _Seed2 = 2521288629)
			{
				if (_Seed0 != 0)
					mp_X = _Seed0;
				else
					mp_X = 123456789;
				if (_Seed1 != 0)
					mp_Y = _Seed1;
				else
					mp_Y = 362436069;
				if (_Seed2 != 0)
					mp_Z = _Seed2;
				else
					mp_Z = 2521288629;
				// Propagate seed
				f_GetValue<uint32>();
				f_GetValue<uint32>();
				f_GetValue<uint32>();
			}

			template <typename tf_CInt>
			constexpr inline_small tf_CInt f_GetValue()
			{
				uint32 t;
				mp_X ^= mp_X << 16;
				mp_X ^= mp_X >> 5;
				mp_X ^= mp_X << 1;

				t = mp_X;
				mp_X = mp_Y;
				mp_Y = mp_Z;
				mp_Z = t ^ mp_X ^ mp_Y;

				if constexpr (sizeof(tf_CInt) > 4)
				{
					tf_CInt Return = mp_Z;
					for (mint i = 1; i < (sizeof(tf_CInt) + 3) / 4; ++i)
					{
						Return <<= 32;
						Return |= f_GetValue<uint32>();
					}

					return Return;
				}
				else
					return (tf_CInt)mp_Z;
			}

			template <typename tf_CInt>
			constexpr inline_small tf_CInt f_GetValue(tf_CInt _Max)
			{
				return f_GetValue<tf_CInt>() % _Max;
			}

			template <typename tf_CInt>
			constexpr inline_small tf_CInt f_GetValue(tf_CInt _Min, tf_CInt _Max)
			{ 
				return _Min + f_GetValue<tf_CInt>() % (_Max - _Min); 
			}

			constexpr inline_small static uint32 fs_Max()
			{
				return TCLimitsInt<uint32>::mc_Max;
			}

		private:
			uint32 mp_X;
			uint32 mp_Y;
			uint32 mp_Z;
		};

		class CAutoRandom : public CRandomShiftRNG
		{
		public:
			CAutoRandom();
		};

		CRandomShiftRNG &fg_RandomThreadLocal();

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
			union
			{
				uint64 m_RandomInt64;
				uint32 m_Random[2];
			} Random;

			auto &ThreadLocal = fg_RandomThreadLocal();

			Random.m_Random[0] = ThreadLocal.f_GetValue<uint32>();
			Random.m_Random[1] = ThreadLocal.f_GetValue<uint32>();

			return fp64(pfp64(Random.m_RandomInt64)) / fp64(pfp64(TCLimitsInt<uint64>::mc_Max));
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
		tf_CIntType fg_GetHighEntropyRandomInteger();

		class CDeadlockDetocterPause
		{
		public:
			CDeadlockDetocterPause()
			{
				NSys::fg_Debug_PauseDeadlockDetector();
			}
			~CDeadlockDetocterPause()
			{
				NSys::fg_Debug_ResumeDeadlockDetector();
			}
		};

#define DMibDeadlockDetectorPause NMib::NMisc::CDeadlockDetocterPause MalterlibDeadlockDetectorPauseScope;
#ifndef DMibPNoShortCuts
#define DDeadlockDetectorPause DMibDeadlockDetectorPause
#endif

		enum ECompareResult
		{
			ECompareResult_LessThan = -1,
			ECompareResult_Equal = 0,
			ECompareResult_GreaterThan = 1,
		};

		template <typename t_CLeft, typename t_CRight>
		inline_small ECompareResult fg_Compare(const t_CLeft &_Left, const t_CRight &_Right)
		{
			if (_Left < _Right)
				return ECompareResult_LessThan;
			if (_Right < _Left)
				return ECompareResult_GreaterThan;
			return ECompareResult_Equal;
		}

		template <typename t_CLeft, typename t_CRight>
		inline_small ECompareResult fg_Compare(ECompareResult _LastResult, const t_CLeft &_Left, const t_CRight &_Right)
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
			{
				return _Str;
			}
			else
			{
				tf_CString Result, OptionName, Value;
				typename tf_CString::CChar const* pRawValue = _Str.f_GetStr();
				typename tf_CString::CChar const* pStartValue = _Str.f_GetStr();
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
						{
							Result += Value;
						}
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
					{
						Result.f_AddStr(pRawValue, NStr::fg_StrLen(pRawValue));
					}
				}
				return Result;
			}
		}
	}

	namespace NContainer
	{
		// STL iterator adaptors used in new for syntax for (auto &iItem : Container)

		struct CIteratorEndSentinel
		{
			template <typename tf_CContainer>
			friend inline_always bool operator == (tf_CContainer &_Container, CIteratorEndSentinel const &_EndSentinel)
			{
				return !_Container;
			}
		};
		
		template <typename tf_CContainer>
		auto begin(tf_CContainer &&_Container) -> decltype(_Container.f_GetIterator())
		{
			return _Container.f_GetIterator();
		}
		
		template <typename tf_CContainer>
		CIteratorEndSentinel end(tf_CContainer &&_Container, TCEnableIfType<!NTraits::TCIsVoid<decltype(_Container.f_GetIterator())>::mc_Value, bool> = true)
		{
			return CIteratorEndSentinel();
		}
	}
	namespace NIntrusive
	{
		using NContainer::CIteratorEndSentinel;
		using NContainer::begin;
		using NContainer::end;
	}

}
