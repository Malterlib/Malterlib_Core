// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

#define DMibInterfaceSingle(_InterfaceName, _Implementation) class _Implementation; typedef _Implementation CIImp ## _InterfaceName;
#define DMibInterfaceVirtual(_InterfaceName) typedef NMib::CEmpty CIImp ## _InterfaceName;
#define DMibInterfaceImp(_InterfaceName) 	typedef NMib::TCChooseType<NMib::NTraits::TCIsSame<CIImp ## _InterfaceName, NMib::CEmpty>::mc_Value, IImp ## _InterfaceName, CIImp ## _InterfaceName>::CType CImp;\
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
		bint fg_CheckAccessRights(NStr::CStr const& _Path, bool _bRandom = true);
		bint fg_CheckAccessRights(NStr::CStrNonTracked const& _Path, bool _bRandom = true);
		bint fg_CheckFileAccessRights(NStr::CStr _Path);
		
 
		template <typename t_CContainer, typename t_CType>
		auto fg_FindEqual(t_CContainer &&_Container, const t_CType &_ToFind) -> decltype(fg_Forward<t_CContainer>(_Container).f_FindEqual(_ToFind))
		{
			return _Container.f_FindEqual(_ToFind);
		}

		template <typename t_CContainer, typename t_CType, typename t_CDefault>
		auto fg_FindEqual(t_CContainer &&_Container, const t_CType &_ToFind, const t_CDefault &_Default) -> typename NTraits::TCRemoveReference<decltype(*fg_Forward<t_CContainer>(_Container).f_FindEqual(_ToFind))>::CType
		{
			auto *pFind = _Container.f_FindEqual(_ToFind);
			if (pFind)
				return *pFind;
			return _Default;
		}

		template <typename t_CFunction>
		void *fg_FunctionPtrToVoidPtr(t_CFunction _Function)
		{
			void *pRet = (void * &)_Function;
			return pRet;
		}

		template <typename t_CContainer, typename t_CFunctor>
		void fg_ForEachAbortable(t_CContainer &&_Container, t_CFunctor &&_Functor) 
/*			-> typename TCEnableIf
			<
				NTraits::TCIsCallableWith
				<
					typename NTraits::TCRemoveReference<t_CFunctor>::CType
					, bint (void)
				>::mc_Value
			>::CType*/
		{
			auto Iter = _Container.f_GetIterator();
			while (Iter)
			{
				if (!fg_Forward<t_CFunctor>(_Functor)(*Iter))
					break;
				++Iter;
			}
		}

		template <typename t_CContainer, typename t_CFunctor>
		void fg_ForEach(t_CContainer &&_Container, t_CFunctor &&_Functor) 
/*			-> typename TCEnableIf
			<
				!NTraits::TCIsCallableWith
				<
					typename NTraits::TCRemoveReference<t_CFunctor>::CType
					, bint (decltype(*(*((typename t_CContainer::CIterator *)nullptr))))
				>::mc_Value
			>::CType*/
		{
			auto Iter = _Container.f_GetIterator();
			while (Iter)
			{
				fg_Forward<t_CFunctor>(_Functor)(*Iter);
				++Iter;
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

		class CRandom
		{
		public:
			CRandom()
			{
				m_Seed = 12164;
			}
			CRandom(aint _Seed)
			{
				m_Seed = _Seed;
			}
			aint m_Seed;
			void f_SetSeed(aint _Seed)
			{
				m_Seed = _Seed;
			}

			aint f_Get()
			{
				return (((m_Seed = m_Seed * 214013L + 2531011L) >> 16) & 0x7fff);
			}

		};

		class CRandom31
		{
		public:

			CRandom31()
			{
				m_Seed = 12164;
			}

			CRandom31(int32 _Seed)
			{
				m_Seed = _Seed;
			}

			void f_SetSeed(int32 _Seed)
			{
				m_Seed = _Seed;
			}
			int32 f_GetSeed()
			{
				return m_Seed;
			}
			/* 
			Park and Miller's psuedo-random number generator.
			*/
			int32 m_Seed;
			int32 f_Get()
			{
				static const int32 A = 16807;
				static const int32 M = 2147483647;   // 2^31 - 1
				static const int32 q = M / A;       // M / A
				static const int32 r = M % A;         // M % A
				m_Seed = A * (m_Seed % q) - r * (m_Seed / q);
				if (m_Seed < 0) 
					m_Seed += M;
				return m_Seed;
			}

			static int32 fs_Max()
			{
				return 2147483646;
			}
		};

		class CRandom30
		{
		public:
			CRandom30()
			{
				m_Seed = 12164;
			}

			CRandom30(int32 _Seed)
			{
				m_Seed = _Seed;
			}

			void f_SetSeed(int32 _Seed)
			{
				m_Seed = _Seed;
			}
			int32 f_GetSeed()
			{
				return m_Seed;
			}
			/* 
			Park and Miller's psuedo-random number generator.
			*/
			int32 m_Seed;
			int32 f_Get()
			{
				static const int32 A = 16807;
				static const int32 M = 2147483647;   // 2^31 - 1
				static const int32 q = M / A;       // M / A
				static const int32 r = M % A;         // M % A
				m_Seed = A * (m_Seed % q) - r * (m_Seed / q);
				if (m_Seed < 0) 
					m_Seed += M;
				return m_Seed >> 1;
			}

			static int32 fs_Max()
			{
				return 2147483646 >> 1;
			}
		};

		// Marsaglias xorshf generator, period 2^96-1
		class CRandomShiftRNG
		{
		public:

			CRandomShiftRNG(uint32 _Seed0 = 123456789, uint32 _Seed1 = 362436069, uint32 _Seed2 = 2521288629)
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
			inline_small tf_CInt f_GetValue()
			{
				uint32 t;
				mp_X ^= mp_X << 16;
				mp_X ^= mp_X >> 5;
				mp_X ^= mp_X << 1;

				t = mp_X;
				mp_X = mp_Y;
				mp_Y = mp_Z;
				mp_Z = t ^ mp_X ^ mp_Y;

				return (tf_CInt)mp_Z;
			}

			template <typename tf_CInt>
			inline_small tf_CInt f_GetValue(tf_CInt _Max)
			{ 
				return f_GetValue<tf_CInt>() % _Max; 
			}

			template <typename tf_CInt>
			inline_small tf_CInt f_GetValue(tf_CInt _Min, tf_CInt _Max)
			{ 
				return _Min + f_GetValue<tf_CInt>() % (_Max - _Min); 
			}

			inline_small static uint32 fs_Max()
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

		extern NAggregate::TCAggregate<NThread::TCThreadLocal<CAutoRandom, NMem::CAllocator_NonTrackedHeap>> g_Random;

		static inline_small int32 fg_GetRandom()
		{
			return (*g_Random)->f_GetValue<int32>() & 0x7fffffff;
		}

		static inline_small uint32 fg_GetRandomUnsigned()
		{
			return (*g_Random)->f_GetValue<uint32>();
		}

		static inline_small fp64 fg_GetRandomFloat()
		{
			return fp64((*g_Random)->f_GetValue<uint32>()) / fp64((*g_Random)->fs_Max());
		}

		static inline_small void fg_SetRanmomSeed(aint _Seed)
		{
			((CRandomShiftRNG &)*(*g_Random)) = CRandomShiftRNG(_Seed);
		}
		
		static inline_small CRandomShiftRNG &fg_Random()
		{
			return **g_Random;
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
		template <typename tf_CLookupFunc, typename tf_CString> // Of the form: bint Func(tf_CString const &_VarName, tf_CString &_Value)
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
		};
		
		template <typename tf_CContainer>
		inline_always bool operator ==(tf_CContainer &_Container, CIteratorEndSentinel const &_EndSentinel)
		{
			return !_Container;
		}
		
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
