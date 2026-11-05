// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>

namespace NMib
{
	template <typename tf_CType, size_t tf_ArrayElements>
	consteval size_t fg_ArraySize(tf_CType (&_Array)[tf_ArrayElements]) noexcept
	{
		return tf_ArrayElements;
	}

	template <typename tf_CType>
	consteval size_t fg_ArraySize(tf_CType const &_Array) noexcept
		requires (sizeof(tf_CType) == 0)
	{
		return 0;
	}

	template <typename tf_CType, typename tf_CTypeToCopyTo>
	mark_artificial mark_nodebug constexpr inline_always_debug decltype(auto) fg_ForwardAs(tf_CTypeToCopyTo &&_ToForward) noexcept
	{
		return static_cast<typename NPrivate::TCForwardCopyEvalHelper<tf_CType &&, tf_CTypeToCopyTo>::CType>(_ToForward);
	}

	template <typename tf_CType>
	mark_artificial mark_nodebug constexpr inline_always_debug NTraits::TCRemoveReference<tf_CType> &&fg_Move(tf_CType &&_ToMove) noexcept
		requires (!NTraits::cIsConst<NTraits::TCRemoveReference<tf_CType>>) // Trying to move const value
	{
		return ((NTraits::TCRemoveReference<tf_CType> &&)_ToMove);
	}

	template <typename tf_CType>
	mark_artificial mark_nodebug constexpr inline_always_debug NTraits::TCRemoveReference<tf_CType> fg_ExchangeMove(tf_CType &&_ToMove) noexcept
		requires (!NTraits::cIsConst<NTraits::TCRemoveReference<tf_CType>>) // Trying to move const value
	{
		return ((NTraits::TCRemoveReference<tf_CType> &&)_ToMove);
	}

	template <typename tf_CType>
	mark_artificial mark_nodebug constexpr inline_always_debug NTraits::TCRemoveReference<tf_CType> &&fg_MoveAllowConst(tf_CType &&_ToMove) noexcept
	{
		return ((NTraits::TCRemoveReference<tf_CType> &&)_ToMove);
	}

	template <typename tf_CType>
	mark_artificial mark_nodebug constexpr inline_always_debug tf_CType fg_TempCopy(tf_CType const &_Value)
	{
		return _Value;
	}

	template <typename tf_CToType, typename tf_CType>
	mark_artificial mark_nodebug constexpr inline_always_debug decltype(auto) fg_CopyOrMove(tf_CType &&_Value)
	{
		using CToType = NTraits::TCRemoveReferenceAndQualifiers<tf_CToType>;

		if constexpr (NTraits::cIsSame<CToType, NTraits::TCRemoveReferenceAndQualifiers<tf_CType>>)
		{
			if constexpr (NTraits::cIsRValueReference<tf_CType &&>)
				return (fg_Forward<tf_CType>(_Value));
			else
				return CToType(fg_Forward<tf_CType>(_Value));
		}
		else
			return CToType(fg_Forward<tf_CType>(_Value));
	}

	template <typename tf_CType>
	mark_artificial mark_nodebug constexpr inline_always_debug tf_CType volatile &fg_Volatile(tf_CType &_In) noexcept
	{
		return (tf_CType volatile &)_In;
	}

	template <typename tf_CType>
	mark_artificial mark_nodebug constexpr inline_always_debug tf_CType const &fg_Const(tf_CType const &_In) noexcept
	{
		return (tf_CType const &)_In;
	}

	template <typename tf_CType, TCEnableIf<NTraits::cIsRValueReference<tf_CType> || !NTraits::cIsReference<tf_CType>> *>
	mark_artificial mark_nodebug constexpr inline_always_debug decltype(auto) fg_ConstOrMove(tf_CType &&_In) noexcept
	{
		return fg_Move(_In);
	}

	template <typename tf_CType, TCEnableIf<!NTraits::cIsRValueReference<tf_CType> && NTraits::cIsReference<tf_CType>> *>
	mark_artificial mark_nodebug constexpr inline_always_debug auto fg_ConstOrMove(tf_CType &&_In) noexcept -> NTraits::TCRemoveReference<tf_CType> const &
	{
		return static_cast<NTraits::TCRemoveReference<tf_CType> const &>(_In);
	}

	template <typename t_CValue>
	mark_nodebug TCMoveValueFunctor<t_CValue>::TCMoveValueFunctor(t_CValue &&_Value)
		: mp_Value(fg_Move(_Value))
	{
	}

	template <typename t_CValue>
	mark_artificial mark_nodebug inline_always t_CValue TCMoveValueFunctor<t_CValue>::operator () ()
	{
		return fg_Move(mp_Value);
	}

	template <typename t_CType>
	mark_nodebug TCCopy<t_CType>::TCCopy(t_CType const &_Data)
		: mp_Data(_Data)
	{
	}

	template <typename t_CType>
	mark_nodebug TCCopy<t_CType>::TCCopy(TCCopy const &_Other)
		: mp_Data(_Other.mp_Data)
	{
	}

	template <typename t_CType>
	mark_nodebug TCCopy<t_CType>::TCCopy(TCCopy &&_Other)
		: mp_Data(_Other.mp_Data)
	{
	}

	template <typename t_CType>
	mark_nodebug t_CType const &TCCopy<t_CType>::operator * () const
	{
		return mp_Data;
	}

	template <typename tf_CType>
	mark_nodebug inline_always_debug TCCopy<tf_CType> fg_Copy(tf_CType const &_In)
	{
		return TCCopy<tf_CType>(_In);
	}

	template <typename t_CType>
	mark_nodebug TCByValue<t_CType>::TCByValue(t_CType const &_Data)
		: mp_Data(_Data)
	{
	}

	template <typename t_CType>
	mark_nodebug TCByValue<t_CType>::TCByValue(TCByValue const &_Other)
		: mp_Data(_Other.mp_Data)
	{
	}

	template <typename t_CType>
	mark_nodebug TCByValue<t_CType>::TCByValue(TCByValue &&_Other)
		: mp_Data(_Other.mp_Data)
	{
	}

	template <typename t_CType>
	mark_nodebug t_CType const &TCByValue<t_CType>::operator * () const
	{
		return mp_Data;
	}

	template <typename tf_CType>
	mark_nodebug inline_always_debug TCByValue<tf_CType> fg_ByValue(tf_CType const &_In)
	{
		return TCByValue<tf_CType>(_In);
	}

	template <typename t_CType>
	template <typename tf_CType>
	mark_nodebug TCExplicit<t_CType>::TCExplicit(tf_CType &&_Data)
		: mp_Data(fg_Forward<tf_CType>(_Data))
	{
	}

	template <typename t_CType>
	mark_nodebug t_CType TCExplicit<t_CType>::operator * ()
	{
		return fg_Forward<t_CType>(mp_Data);
	}

	template <typename tf_CType, TCEnableIf<NTraits::cIsReference<tf_CType>> *>
	mark_nodebug inline_always_debug TCExplicit<tf_CType> fg_Explicit(tf_CType &&_In)
	{
		return TCExplicit<tf_CType>(_In);
	}

	template <typename tf_CType, TCEnableIf<!NTraits::cIsReference<tf_CType> && !NTraits::cIsConst<NTraits::TCRemoveReference<tf_CType>>> *>
	mark_nodebug inline_always_debug TCExplicit<tf_CType> fg_Explicit(tf_CType &&_In)
	{
		return TCExplicit<tf_CType>(fg_Move(_In));
	}

	template <typename tf_CType, TCEnableIf<!NTraits::cIsReference<tf_CType> && NTraits::cIsConst<NTraits::TCRemoveReference<tf_CType>>> *>
	mark_nodebug inline_always_debug TCExplicit<tf_CType> fg_Explicit(tf_CType &&_In)
	{
		return TCExplicit<tf_CType>(_In);
	}

	mark_nodebug inline_always_debug TCExplicit<void> fg_Explicit()
	{
		return TCExplicit<void>();
	}

	template <typename tf_CType, TCEnableIf<NTraits::cIsReference<tf_CType>> *>
	mark_nodebug inline_always_debug TCExplicit<tf_CType> CExplicitHelper::operator = (tf_CType &&_In) const
	{
		return TCExplicit<tf_CType>(_In);
	}

	template <typename tf_CType, TCEnableIf<!NTraits::cIsReference<tf_CType>> *>
	mark_nodebug inline_always_debug TCExplicit<tf_CType> CExplicitHelper::operator = (tf_CType &&_In) const
	{
		return TCExplicit<tf_CType>(fg_Move(_In));
	}

	template <typename t_CType>
	template <typename tf_CType>
	mark_nodebug TCAttach<t_CType>::TCAttach(tf_CType &&_Data)
		: mp_Data(fg_Forward<tf_CType>(_Data))
	{
	}

	template <typename t_CType>
	mark_nodebug t_CType TCAttach<t_CType>::operator * ()
	{
		return fg_Forward<t_CType>(mp_Data);
	}

	template <typename tf_CType, TCEnableIf<NTraits::cIsReference<tf_CType>> *>
	mark_nodebug inline_always_debug TCAttach<tf_CType> fg_Attach(tf_CType &&_In)
	{
		return TCAttach<tf_CType>(_In);
	}

	template <typename tf_CType, TCEnableIf<!NTraits::cIsReference<tf_CType> && !NTraits::cIsConst<NTraits::TCRemoveReference<tf_CType>>> *>
	mark_nodebug inline_always_debug TCAttach<tf_CType> fg_Attach(tf_CType &&_In)
	{
		return TCAttach<tf_CType>(fg_Move(_In));
	}

	template <typename tf_CType, TCEnableIf<!NTraits::cIsReference<tf_CType> && NTraits::cIsConst<NTraits::TCRemoveReference<tf_CType>>> *>
	mark_nodebug inline_always_debug TCAttach<tf_CType> fg_Attach(tf_CType &&_In)
	{
		return TCAttach<tf_CType>(_In);
	}

	mark_nodebug inline_always_debug TCAttach<void> fg_Attach()
	{
		return TCAttach<void>();
	}

	namespace NInternal
	{
		template <typename tf_CReturn>
		mark_nodebug CDefault::operator tf_CReturn ()
		{
			return tf_CReturn();
		}

		template <typename tf_CType>
		mark_nodebug inline_always_debug CIgnore &CIgnore::operator = (tf_CType &&_Value)
		{
			return *this;
		}
	}

	mark_nodebug inline_always_debug NInternal::CIgnore fg_Ignore()
	{
		return NInternal::CIgnore();
	}

	mark_nodebug inline_always_debug NInternal::CDefault fg_Default()
	{
		return NInternal::CDefault();
	}

	namespace NInternal
	{
		template <typename t_CFrom>
		template <typename tf_CFrom>
		mark_nodebug TCAutoStaticCast<t_CFrom>::TCAutoStaticCast(tf_CFrom &&_From)
			: m_From(fg_Forward<tf_CFrom>(_From))
		{
		}

		template <typename t_CFrom>
		template <typename tf_CTo>
		mark_nodebug TCAutoStaticCast<t_CFrom>::operator tf_CTo ()
		{
			return static_cast<tf_CTo>(m_From);
		}
	}

	template <typename tf_CFrom>
	mark_nodebug NInternal::TCAutoStaticCast<tf_CFrom> fg_AutoStaticCast(tf_CFrom &&_From)
	{
		return NInternal::TCAutoStaticCast<tf_CFrom>(fg_Forward<tf_CFrom>(_From));
	}

	namespace NInternal
	{
		template <typename t_CFrom>
		template <typename tf_CFrom>
		mark_nodebug TCAutoConstCast<t_CFrom>::TCAutoConstCast(tf_CFrom &&_From)
			: m_From(fg_Forward<tf_CFrom>(_From))
		{
		}

		template <typename t_CFrom>
		template <typename tf_CTo>
		mark_nodebug TCAutoConstCast<t_CFrom>::operator tf_CTo ()
		{
			return const_cast<tf_CTo>(m_From);
		}
	}

	template <typename tf_CFrom>
	mark_nodebug NInternal::TCAutoConstCast<tf_CFrom> fg_AutoConstCast(tf_CFrom &&_From)
	{
		return NInternal::TCAutoConstCast<tf_CFrom>(fg_Forward<tf_CFrom>(_From));
	}

	namespace NInternal
	{
		template <typename t_CFrom>
		template <typename tf_CFrom>
		mark_nodebug TCAutoReinterpretCast<t_CFrom>::TCAutoReinterpretCast(tf_CFrom &&_From)
			: m_From(fg_Forward<tf_CFrom>(_From))
		{
		}

		template <typename t_CFrom>
		template <typename tf_CTo>
		mark_nodebug TCAutoReinterpretCast<t_CFrom>::operator tf_CTo ()
		{
			return reinterpret_cast<tf_CTo>(m_From);
		}
	}

	template <typename tf_CFrom>
	mark_nodebug NInternal::TCAutoReinterpretCast<tf_CFrom> fg_AutoReinterpretCast(tf_CFrom &&_From)
	{
		return NInternal::TCAutoReinterpretCast<tf_CFrom>(fg_Forward<tf_CFrom>(_From));
	}

	namespace NInternal
	{
		template <typename t_CFrom>
		template <typename tf_CFrom>
		mark_nodebug TCAutoCCast<t_CFrom>::TCAutoCCast(tf_CFrom &&_From)
			: m_From(fg_Forward<tf_CFrom>(_From))
		{
		}

		template <typename t_CFrom>
		template <typename tf_CTo>
		mark_nodebug TCAutoCCast<t_CFrom>::operator tf_CTo ()
		{
			return (tf_CTo)(m_From);
		}
	}

	template <typename tf_CFrom>
	mark_nodebug NInternal::TCAutoCCast<tf_CFrom> fg_AutoCCast(tf_CFrom &&_From)
	{
		return NInternal::TCAutoCCast<tf_CFrom>(fg_Forward<tf_CFrom>(_From));
	}

	template <typename tf_CType>
	mark_nodebug inline_always_debug tf_CType &fg_RemoveQualifiers(tf_CType &_In)
	{
		return _In;
	}

	template <typename tf_CType>
	mark_nodebug inline_always_debug tf_CType &fg_RemoveQualifiers(tf_CType const &_In)
	{
		return const_cast<tf_CType &>(_In);
	}

	template <typename tf_CType>
	mark_nodebug inline_always_debug tf_CType &fg_RemoveQualifiers(tf_CType volatile &_In)
	{
		return const_cast<tf_CType &>(_In);
	}

	template <typename tf_CType>
	mark_nodebug inline_always_debug tf_CType &fg_RemoveQualifiers(tf_CType const volatile &_In)
	{
		return const_cast<tf_CType &>(_In);
	}

	template <typename tf_CType>
	mark_nodebug inline_always_debug tf_CType &&fg_RemoveQualifiers(tf_CType &&_In)
	{
		return fg_Move(_In);
	}

	template <typename tf_CType>
	mark_nodebug inline_always_debug tf_CType &&fg_RemoveQualifiers(tf_CType const &&_In)
	{
		return fg_Move(const_cast<tf_CType &>(_In));
	}

	template <typename tf_CType>
	mark_nodebug inline_always_debug tf_CType &&fg_RemoveQualifiers(tf_CType volatile &&_In)
	{
		return fg_Move(const_cast<tf_CType &>(_In));
	}

	template <typename tf_CType>
	mark_nodebug inline_always_debug tf_CType &&fg_RemoveQualifiers(tf_CType const volatile &&_In)
	{
		return fg_Move(const_cast<tf_CType &>(_In));
	}

	template <typename t_CType>
	mark_nodebug TCLambdaMover<t_CType>::TCLambdaMover(t_CType &&_Other)
		: mp_Type(fg_Move(_Other))
	{
	}

	template <typename t_CType>
	mark_nodebug TCLambdaMover<t_CType>::TCLambdaMover(TCCopy<t_CType> &&_Other)
		: mp_Type(*_Other)
	{
	}

	template <typename t_CType>
	mark_nodebug TCLambdaMover<t_CType>::TCLambdaMover(TCLambdaMover const &_Other)
		: mp_Type(fg_Move(fg_RemoveQualifiers(_Other.mp_Type)))
	{
	}

	template <typename t_CType>
	mark_nodebug TCLambdaMover<t_CType>::TCLambdaMover(TCLambdaMover &_Other)
		: mp_Type(fg_Move(fg_RemoveQualifiers(_Other.mp_Type)))
	{
	}

	template <typename t_CType>
	mark_nodebug TCLambdaMover<t_CType>::TCLambdaMover(TCLambdaMover &&_Other)
		: mp_Type(fg_Move(_Other.mp_Type))
	{
	}

	template <typename t_CType>
	mark_nodebug t_CType &TCLambdaMover<t_CType>::operator * () const
	{
		return fg_RemoveQualifiers(mp_Type);
	}

	template <typename tf_CType>
	mark_nodebug TCLambdaMover<NTraits::TCRemoveReferenceAndQualifiers<tf_CType>> fg_LambdaMove(tf_CType &&_Type)
	{
		return TCLambdaMover<NTraits::TCRemoveReferenceAndQualifiers<tf_CType>>(fg_Move(_Type));
	}

	template <typename tf_CType>
	mark_nodebug TCLambdaMover<NTraits::TCRemoveReferenceAndQualifiers<tf_CType>> fg_LambdaMove(TCCopy<tf_CType> &&_Type)
	{
		return TCLambdaMover<NTraits::TCRemoveReferenceAndQualifiers<tf_CType>>(fg_Move(_Type));
	}

	template <typename tf_CType>
	mark_nodebug inline_always_debug tf_CType volatile const &fg_ConstVolatile(tf_CType &_In)
	{
		return (tf_CType volatile const &)_In;
	}

	template <typename tf_CType>
	mark_nodebug static constexpr inline_small tf_CType fg_PowerOfTwoMinusOne(uaint _Power)
	{
		return ((tf_CType(1)) << (_Power - 1)) + (((tf_CType(1)) << (_Power - 1)) - tf_CType(1));
	}

	template <typename tf_CType0, typename tf_CType1>
	static inline_small void fg_Swap(tf_CType0 &_Left, tf_CType1 &_Right)
	{
		tf_CType0 Temp = fg_Move(_Left);
		_Left = fg_Move(_Right);
		_Right = fg_Move(Temp);
	}

	template <typename tf_CTo, typename tf_CFrom>
	static constexpr inline_small tf_CTo fg_BitCast(tf_CFrom const &_From)
	{
		return std::bit_cast<tf_CTo>(_From);
	}

	template <typename tf_CType>
	mark_nodebug inline_always_debug tf_CType *fg_NullPtr()
	{
		return (tf_CType *)nullptr;
	}

	template <typename t_CType>
	t_CType TCHelper_ByteSwap<t_CType>::fs_Swap(t_CType const &_Data)
	{
		umint Size = sizeof(_Data);
		int iEnd = Size - 1;
		int iStart = 0;
		t_CType Return = _Data;
		uint8 *pArray = (uint8 *)&Return;
		while (iEnd > iStart)
		{
			fg_Swap(pArray[iStart], pArray[iEnd]);
			++iStart;
			--iEnd;
		}
		return Return;
	}

#ifdef DMibPCanDo_uint8
	inline uint8 TCHelper_ByteSwap<uint8>::fs_Swap(uint8 _Data)
	{
		return _Data;
	}
#endif

#ifdef DMibPCanDo_uint16
	inline uint16 TCHelper_ByteSwap<uint16>::fs_Swap(uint16 _Data)
	{
#	ifdef DMibPByteSwap16
		return DMibPByteSwap16(_Data);
#	else
		return DMibPDefaultSwap_uint16(_Data);
#	endif
	}
#endif

#ifdef DMibPCanDo_uint32
	inline uint32 TCHelper_ByteSwap<uint32>::fs_Swap(uint32 _Data)
	{
#	ifdef DMibPByteSwap32
		return DMibPByteSwap32(_Data);
#	else
		return DMibPDefaultSwap_uint32(_Data);
#	endif
	}
#endif

#ifdef DMibPCanDo_uint64
	inline uint64 TCHelper_ByteSwap<uint64>::fs_Swap(uint64 _Data)
	{
#	ifdef DMibPByteSwap64
		return DMibPByteSwap64(_Data);
#	else
		return DMibPDefaultSwap_uint64(_Data);
#	endif
	}
#endif

	template <typename tf_CInt>
	inline_small tf_CInt fg_ByteSwap(tf_CInt _In)
	{
		// Static branch
		if constexpr (TCHelper_ByteSwap<tf_CInt>::mc_DefaultImplementation)
		{
			using CSwapper = typename TCHelper_ByteSwapChooseBySize<sizeof(tf_CInt), tf_CInt>::CSwapper;
			return CSwapper::fs_Swap((typename CSwapper::CType &)_In);
		}
		else
			return TCHelper_ByteSwap<tf_CInt>::fs_Swap(_In);
	}

	template <typename tf_CInt>
	inline_small void fg_ByteSwap(tf_CInt *_pIn, umint _Len)
	{
		for (umint iValue = 0; iValue < _Len; ++iValue)
			_pIn[iValue] = fg_ByteSwap(_pIn[iValue]);
	}

	template <typename tf_CInt>
	inline_small tf_CInt fg_ByteSwapLE(tf_CInt _In)
	{
#ifdef DMibPLittleEndian
		return _In;
#else
		return fg_ByteSwap(_In);
#endif
	}

	template <typename tf_CInt>
	inline_small tf_CInt fg_ByteSwapBE(tf_CInt _In)
	{
#ifdef DMibPLittleEndian
		return fg_ByteSwap(_In);
#else
		return _In;
#endif
	}

	template <typename tf_CFirst, typename tf_CMin, typename tf_CMax>
	constexpr inline_small NTraits::TCRemoveReference<tf_CFirst> fg_Clamp(tf_CFirst &&_First, tf_CMin &&_Min, tf_CMax &&_Max)
	{
		return fg_Max(fg_Min(fg_Forward<tf_CFirst>(_First), fg_Forward<tf_CMax>(_Max)), fg_Forward<tf_CMin>(_Min));
	}

	template <typename tf_CLeft, typename tf_CRight>
	constexpr inline_small NTraits::TCRemoveReference<tf_CLeft> fg_MaxValidFloat(tf_CLeft &&_Left, tf_CRight &&_Right)
	{
		if (_Left.f_IsNan())
		{
			if (_Right.f_IsNan())
				return _Left;
			else
				return _Right;
		}
		else
		{
			if (_Right.f_IsNan())
				return _Left;

			if (_Left < _Right)
				return fg_Forward<tf_CRight>(_Right);
			else
				return fg_Forward<tf_CLeft>(_Left);
		}
	}

	template <typename tf_CFirst, typename tf_CSecond, typename ...tfp_CRest>
	constexpr inline_small NTraits::TCRemoveReference<tf_CFirst> fg_MaxValidFloat(tf_CFirst &&_First, tf_CSecond &&_Second, tfp_CRest && ...p_Rest)
	{
		return fg_MaxValidFloat(fg_MaxValidFloat(fg_Forward<tf_CFirst>(_First), fg_Forward<tf_CSecond>(_Second)), fg_Forward<tfp_CRest>(p_Rest)...);
	}

	template <typename tf_CLeft, typename tf_CRight>
	constexpr inline_small NTraits::TCRemoveReference<tf_CLeft> fg_MinValidFloat(tf_CLeft &&_Left, tf_CRight &&_Right)
	{
		if (_Left.f_IsNan())
		{
			if (_Right.f_IsNan())
				return _Left;
			else
				return _Right;
		}
		else
		{
			if (_Right.f_IsNan())
				return _Left;

			if (_Left < _Right)
				return fg_Forward<tf_CLeft>(_Left);
			else
				return fg_Forward<tf_CRight>(_Right);
		}
	}

	template <typename tf_CFirst, typename tf_CSecond, typename ...tfp_CRest>
	constexpr inline_small NTraits::TCRemoveReference<tf_CFirst> fg_MinValidFloat(tf_CFirst &&_First, tf_CSecond &&_Second, tfp_CRest && ...p_Rest)
	{
		return fg_MinValidFloat(fg_MinValidFloat(fg_Forward<tf_CFirst>(_First), fg_Forward<tf_CSecond>(_Second)), fg_Forward<tfp_CRest>(p_Rest)...);
	}

	template <typename tf_CFirst>
	constexpr inline_small NTraits::TCRemoveReference<tf_CFirst> fg_Abs(tf_CFirst &&_First)
	{
		if (fg_Forward<tf_CFirst>(_First) >= NTraits::TCRemoveReference<tf_CFirst>(0))
			return fg_Forward<tf_CFirst>(_First);
		else
			return -fg_Forward<tf_CFirst>(_First);
	}

	template <typename tf_CFirst, typename tf_CMin, typename tf_CMax>
	constexpr inline_small tf_CFirst fg_ClampConstexpr(tf_CFirst _First, tf_CMin _Min, tf_CMax _Max)
	{
		return fg_MaxConstexpr(fg_MinConstExpr(_First, _Max), _Min);
	}

	template <typename tf_CFirst>
	constexpr inline_small tf_CFirst fg_AbsConstexpr(tf_CFirst _First)
	{
		return (_First >= tf_CFirst(0)) ? _First : -_First;
	}

	template <typename tf_CToAlign>
	constexpr inline_small tf_CToAlign fg_AlignDownConstExpr(tf_CToAlign _Mem, umint _Alignment)
	{
		using CIntegerType = NTraits::TCUnsigned<NTraits::TCIntFromSizeLarger<sizeof(tf_CToAlign)>>;
		return (tf_CToAlign)(((CIntegerType)_Mem) & (~(CIntegerType(_Alignment) - CIntegerType(1))));
	}

	template <typename tf_CToAlign>
	inline_small tf_CToAlign fg_AlignUp(tf_CToAlign _Mem, umint _Alignment)
	{
		using CIntegerType = NTraits::TCUnsigned<NTraits::TCIntFromSizeLarger<sizeof(tf_CToAlign)>>;
		DMibFastCheck(_Alignment > 0 || ((CIntegerType)_Mem) == 0);
		return (tf_CToAlign)((((CIntegerType)_Mem) + CIntegerType(_Alignment - 1)) & (~(CIntegerType(_Alignment) - CIntegerType(1))));
	}

	template <typename tf_CToAlign>
	inline_small tf_CToAlign fg_AlignDown(tf_CToAlign _Mem, umint _Alignment)
	{
		using CIntegerType = NTraits::TCUnsigned<NTraits::TCIntFromSizeLarger<sizeof(tf_CToAlign)>>;
		DMibFastCheck(_Alignment > 0 || ((CIntegerType)_Mem) == 0);
		return (tf_CToAlign)(((CIntegerType)_Mem) & (~(CIntegerType(_Alignment) - CIntegerType(1))));
	}

	template <typename tf_CType>
	inline_small tf_CType fg_HighPart(tf_CType _Data)
	{
		return _Data >> (sizeof(_Data) * 4);
	}

	template <typename tf_CType>
	inline_small tf_CType fg_LowPart(tf_CType _Data)
	{
		return _Data & ((1 << sizeof(_Data) * 4) - 1);
	}

	template <typename tf_CType>
	TCEnableIf<!NTraits::cIsVoid<tf_CType> && !NTraits::cIsArray<tf_CType> && !NTraits::cIsPointer<tf_CType>, void> fg_CallDestructor(tf_CType &_Type)
	{
		_Type.~tf_CType();
	}

	template <typename tf_CType>
	TCEnableIf<NTraits::cIsVoid<tf_CType> || NTraits::cIsPointer<tf_CType>, void> fg_CallDestructor(tf_CType &_Type)
	{
	}

	template <typename tf_CType>
	TCEnableIf<NTraits::cIsArray<tf_CType>, void> fg_CallDestructor(tf_CType &_Type)
	{
		aint iElement = NTraits::gc_ArrayExtent<tf_CType> - 1;
		for (; iElement >= 0; --iElement)
			fg_CallDestructor(_Type[iElement]);
	}

	template <typename tf_CKey0, typename tf_CKey1>
	constexpr inline_small auto CSort_Default::operator () (tf_CKey0 &&_Left, tf_CKey1 &&_Right) noexcept(noexcept(_Left <=> _Right))
	{
		return _Left <=> _Right;
	}

	template <typename tf_CType>
	CAutoLimitMin::operator tf_CType ()
	{
		return TCLimitsInt<tf_CType>::mc_Min;
	}

	template <typename tf_CType>
	CAutoLimitMax::operator tf_CType ()
	{
		return TCLimitsInt<tf_CType>::mc_Max;
	}

	inline_always static CAutoLimitMin fg_LimitsMin()
	{
		return CAutoLimitMin();
	}

	inline_always static CAutoLimitMax fg_LimitsMax()
	{
		return CAutoLimitMax();
	}

#if defined(DCompiler_clang) || defined(DCompiler_gcc)
	template <typename tf_CInt>
	constexpr inline_small bool fg_MultiplyOverflow(tf_CInt _Left, tf_CInt _Right, tf_CInt &o_Result) noexcept
		requires (NTraits::cIsFundamental<tf_CInt> && NTraits::cIsInteger<tf_CInt> && !NTraits::cIsSame<NTraits::TCRemoveQualifiers<tf_CInt>, bool>)
	{
		return __builtin_mul_overflow(_Left, _Right, &o_Result);
	}
#endif

	template <typename tf_CInt>
	constexpr inline_small bool fg_MultiplyOverflow(tf_CInt _Left, tf_CInt _Right, tf_CInt &o_Result) noexcept
		requires
		(
			NTraits::cIsInteger<tf_CInt>
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
			&& !NTraits::cIsFundamental<tf_CInt>
#endif
			&& !NTraits::cIsSame<NTraits::TCRemoveQualifiers<tf_CInt>, bool>
		)
	{
		if constexpr (NTraits::cIsSigned<tf_CInt>)
		{
			if (_Left == 0 || _Right == 0)
			{
				o_Result = 0;
				return false;
			}

			if (_Left > 0)
			{
				if (_Right > 0)
				{
					if (_Left > TCLimitsInt<tf_CInt>::mc_Max / _Right)
						return true;
				}
				else if (_Right < TCLimitsInt<tf_CInt>::mc_Min / _Left)
					return true;
			}
			else
			{
				if (_Right > 0)
				{
					if (_Left < TCLimitsInt<tf_CInt>::mc_Min / _Right)
						return true;
				}
				else if (_Right < TCLimitsInt<tf_CInt>::mc_Max / _Left)
					return true;
			}
		}
		else
		{
			if (_Right != 0 && _Left > TCLimitsInt<tf_CInt>::mc_Max / _Right)
				return true;
		}

		o_Result = _Left * _Right;
		return false;
	}

	template <typename tf_CInt0, typename tf_CInt1>
	bool fg_SafeLargerThan(tf_CInt0 const &_Left, tf_CInt1 const &_Right)
	{
		if constexpr (sizeof(tf_CInt0) > sizeof(tf_CInt1))
		{
			if constexpr (NTraits::cIsSigned<tf_CInt0>)
			{
				if constexpr (NTraits::cIsSigned<tf_CInt1>)
					return _Left > tf_CInt0(_Right);
				else
				{
					tf_CInt0 Zero(0);
					if (_Left < Zero)
						return false;
					return _Left > tf_CInt0(_Right);
				}
			}
			else
			{
				if constexpr (NTraits::cIsSigned<tf_CInt1>)
				{
					tf_CInt1 Zero(0);
					if (_Right < Zero)
						return true;
					return _Left > tf_CInt0(_Right);
				}
				else
					return _Left > tf_CInt0(_Right);
			}
		}
		else if constexpr (sizeof(tf_CInt0) < sizeof(tf_CInt1))
		{
			if constexpr (NTraits::cIsSigned<tf_CInt0>)
			{
				if constexpr (NTraits::cIsSigned<tf_CInt1>)
					return tf_CInt1(_Left) > _Right;
				else
				{
					tf_CInt0 Zero(0);
					if (_Left < Zero)
						return false;
					return tf_CInt1(_Left) > _Right;
				}
			}
			else
			{
				if constexpr (NTraits::cIsSigned<tf_CInt1>)
				{
					tf_CInt1 Zero(0);
					if (_Right < Zero)
						return true;
					return tf_CInt1(_Left) > _Right;
				}
				else
					return tf_CInt1(_Left) > _Right;
			}
		}
		else
		{
			if constexpr (NTraits::cIsSigned<tf_CInt0>)
			{
				if constexpr (NTraits::cIsSigned<tf_CInt1>)
					return _Left > tf_CInt0(_Right);
				else
				{
					tf_CInt0 Zero(0);
					if (_Left < Zero)
						return false;
					return tf_CInt1(_Left) > _Right;
				}
			}
			else
			{
				if constexpr (NTraits::cIsSigned<tf_CInt1>)
				{
					tf_CInt1 Zero(0);
					if (_Right < Zero)
						return true;
					return _Left > tf_CInt0(_Right);
				}
				else
					return _Left > tf_CInt0(_Right);
			}
		}
	}

	template <typename t_CInt, t_CInt t_ClearVal>
	TCAutoClearInt<t_CInt, t_ClearVal>::TCAutoClearInt()
	{
		m_Int = t_ClearVal;
	}

	template <typename t_CInt, t_CInt t_ClearVal>
	TCAutoClearInt<t_CInt, t_ClearVal>::TCAutoClearInt(t_CInt const &_Int)
	{
		m_Int = _Int;
	}

	template <typename t_CInt, t_CInt t_ClearVal>
	TCAutoClearInt<t_CInt, t_ClearVal>::operator t_CInt & ()
	{
		return m_Int;
	}

	template <typename t_CInt, t_CInt t_ClearVal>
	TCAutoClearInt<t_CInt, t_ClearVal>::operator t_CInt const & () const
	{
		return m_Int;
	}

	template <typename t_CInt, t_CInt t_ClearVal>
	t_CInt &TCAutoClearInt<t_CInt, t_ClearVal>::f_Get()
	{
		return m_Int;
	}

	template <typename t_CInt, t_CInt t_ClearVal>
	t_CInt const &TCAutoClearInt<t_CInt, t_ClearVal>::f_Get() const
	{
		return m_Int;
	}

	template <typename t_CInt, t_CInt t_ClearVal>
	auto TCAutoClearInt<t_CInt, t_ClearVal>::operator = (TCAutoClearInt const &_Other) -> TCAutoClearInt &
	{
		m_Int = _Other.m_Int;
		return *this;
	}

	template <typename t_CInt, t_CInt t_ClearVal>
	auto TCAutoClearInt<t_CInt, t_ClearVal>::operator = (t_CInt const &_Other) -> TCAutoClearInt &
	{
		m_Int = _Other;
		return *this;
	}

	template <typename t_CInt, t_CInt t_ClearVal>
	template <typename tf_CFormatter>
	auto TCAutoClearInt<t_CInt, t_ClearVal>::f_CreateStringFormatter(tf_CFormatter &_Formatter) const -> decltype(NStr::fg_CreateStringFormatter(_Formatter, fg_GetType<t_CInt const &>()))
	{
		return NStr::fg_CreateStringFormatter(_Formatter, m_Int);
	}

	template <typename t_CInt, t_CInt t_ClearVal>
	template <typename tf_CStream>
	void TCAutoClearInt<t_CInt, t_ClearVal>::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << m_Int;
	}

	template <typename t_CInt, t_CInt t_ClearVal>
	template <typename tf_CStream>
	void TCAutoClearInt<t_CInt, t_ClearVal>::f_Consume(tf_CStream &_Stream)
	{
		_Stream >> m_Int;
	}

	template <typename t_CInt, t_CInt t_ClearVal>
	auto TCAutoClearInt<t_CInt, t_ClearVal>::operator <=> (TCAutoClearInt const &_Right) const noexcept(noexcept(f_Get() <=> _Right.f_Get()))
	{
		return f_Get() <=> _Right.f_Get();
	}

	template <typename t_CInt, t_CInt t_ClearVal>
	bool TCAutoClearInt<t_CInt, t_ClearVal>::operator == (TCAutoClearInt const &_Right) const noexcept(noexcept(f_Get() == _Right.f_Get()))
	{
		return f_Get() == _Right.f_Get();
	}

	template <typename t_CInt, t_CInt t_ClearVal>
	template <typename tf_CRight>
	auto TCAutoClearInt<t_CInt, t_ClearVal>::operator <=> (tf_CRight const &_Right) const noexcept(noexcept(f_Get() <=> _Right))
	{
		return f_Get() <=> _Right;
	}

	template <typename t_CInt, t_CInt t_ClearVal>
	template <typename tf_CRight, tf_CRight tf_RightValue>
	auto TCAutoClearInt<t_CInt, t_ClearVal>::operator <=> (TCAutoClearInt<tf_CRight, tf_RightValue> const &_Right) const noexcept(noexcept(f_Get() <=> _Right.f_Get()))
	{
		return f_Get() <=> _Right.f_Get();
	}

	template <typename t_CInt, t_CInt t_ClearVal>
	template <typename tf_CRight>
	bool TCAutoClearInt<t_CInt, t_ClearVal>::operator == (tf_CRight const &_Right) const noexcept(noexcept(f_Get() == _Right))
	{
		return f_Get() == _Right;
	}

	template <typename t_CInt, t_CInt t_ClearVal>
	template <typename tf_CRight, tf_CRight tf_RightValue>
	bool TCAutoClearInt<t_CInt, t_ClearVal>::operator == (TCAutoClearInt<tf_CRight, tf_RightValue> const &_Right) const noexcept(noexcept(f_Get() == _Right.f_Get()))
	{
		return f_Get() == _Right.f_Get();
	}

	template <typename t_CType>
	TCAutoClear<t_CType>::TCAutoClear(t_CType const &_Value)
	{
		m_Value = _Value;
	}

	template <typename t_CType>
	TCAutoClear<t_CType>::operator t_CType & ()
	{
		return m_Value;
	}

	template <typename t_CType>
	TCAutoClear<t_CType>::operator t_CType const & () const
	{
		return m_Value;
	}

	template <typename t_CType>
	t_CType &TCAutoClear<t_CType>::f_Get()
	{
		return m_Value;
	}

	template <typename t_CType>
	t_CType const &TCAutoClear<t_CType>::f_Get() const
	{
		return m_Value;
	}

	template <typename t_CType>
	auto TCAutoClear<t_CType>::operator = (t_CType const &_Other) -> TCAutoClear &
	{
		m_Value = _Other;
		return *this;
	}

	template <typename t_CType>
	template <typename tf_CFormatter>
	auto TCAutoClear<t_CType>::f_CreateStringFormatter(tf_CFormatter &_Formatter) const -> decltype(NStr::fg_CreateStringFormatter(_Formatter, fg_GetType<t_CType const &>()))
	{
		return NStr::fg_CreateStringFormatter(_Formatter, m_Value);
	}

	template <typename t_CType>
	template <typename tf_CStream>
	void TCAutoClear<t_CType>::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << m_Value;
	}

	template <typename t_CType>
	template <typename tf_CStream>
	void TCAutoClear<t_CType>::f_Consume(tf_CStream &_Stream)
	{
		_Stream >> m_Value;
	}

	template <typename t_CType>
	auto TCAutoClear<t_CType>::operator <=> (TCAutoClear const &_Right) const noexcept(noexcept(f_Get() <=> _Right.f_Get()))
	{
		return f_Get() <=> _Right.f_Get();
	}

	template <typename t_CType>
	bool TCAutoClear<t_CType>::operator == (TCAutoClear const &_Right) const noexcept(noexcept(f_Get() == _Right.f_Get()))
	{
		return f_Get() == _Right.f_Get();
	}

	template <typename t_CType>
	template <typename tf_CRight>
	auto TCAutoClear<t_CType>::operator <=> (tf_CRight const &_Right) const noexcept(noexcept(f_Get() <=> _Right))
	{
		return f_Get() <=> _Right;
	}

	template <typename t_CType>
	template <typename tf_CRight>
	auto TCAutoClear<t_CType>::operator <=> (TCAutoClear<tf_CRight> const &_Right) const noexcept(noexcept(f_Get() <=> _Right.f_Get()))
	{
		return f_Get() <=> _Right.f_Get();
	}

	template <typename t_CType>
	template <typename tf_CRight>
	bool TCAutoClear<t_CType>::operator == (tf_CRight const &_Right) const noexcept(noexcept(f_Get() == _Right))
	{
		return f_Get() == _Right;
	}

	template <typename t_CType>
	template <typename tf_CRight>
	bool TCAutoClear<t_CType>::operator == (TCAutoClear<tf_CRight> const &_Right) const noexcept(noexcept(f_Get() == _Right.f_Get()))
	{
		return f_Get() == _Right.f_Get();
	}

	template <typename t_CType>
	template <typename tf_CRight, tf_CRight tf_RightValue>
	auto TCAutoClear<t_CType>::operator <=> (TCAutoClearInt<tf_CRight, tf_RightValue> const &_Right) const noexcept(noexcept(f_Get() <=> _Right.f_Get()))
	{
		return f_Get() <=> _Right.f_Get();
	}

	template <typename t_CType>
	template <typename tf_CRight, tf_CRight tf_RightValue>
	bool TCAutoClear<t_CType>::operator == (TCAutoClearInt<tf_CRight, tf_RightValue> const &_Right) const noexcept(noexcept(f_Get() == _Right.f_Get()))
	{
		return f_Get() == _Right.f_Get();
	}

	template <typename tf_CAny>
	static tf_CAny fg_MakeSymbolActive(tf_CAny &&_Other)
	{
		return _Other;
	}

	constexpr CConstExprSubStr::CConstExprSubStr(ch8 const *_pString, umint _Len)
		: m_pString(_pString)
		, m_Len(_Len)
	{
	}

	constexpr CConstExprSubStr::CConstExprSubStr(ch8 const *_pString)
		: m_pString(_pString)
		, m_Len(_pString ? NStr::fg_StrLen(_pString) : 0)
	{
	}

	consteval ch8 const *CConstExprSubStr::data() const
	{
		return m_pString;
	}

	consteval umint CConstExprSubStr::size() const
	{
		return m_Len;
	}

	template <umint t_nCharacters>
	constexpr TCConstExprSubStr<t_nCharacters>::TCConstExprSubStr(char const *_pString)
		: CConstExprSubStr(m_String, t_nCharacters)
	{
		for (umint iChar = 0; iChar < t_nCharacters; ++iChar)
			m_String[iChar] = _pString[iChar];
		m_String[t_nCharacters] = 0;
	}

	template <typename tf_CType>
	static constexpr auto fg_GetTypeNameConstExprArray()
	{
		constexpr auto c_String = fg_GetTypeNameConstExpr<tf_CType>();
		return TCConstExprSubStr<c_String.m_Len>{c_String.m_pString};
	}

	static constexpr uint32 fg_JenkinsHash(char const * const _pString)
	{
		uint32 Hash = 0;
		for (char const *pStr = _pString; *pStr; ++pStr)
		{
			Hash = (uint64(Hash) + *pStr) & uint64(0xffffffff);
			Hash = (uint64(Hash) + (Hash << 10)) & uint64(0xffffffff);
			Hash ^= (Hash >> 6);
		}
		Hash = (uint64(Hash) + (Hash << 3)) & uint64(0xffffffff);
		Hash ^= (Hash >> 11);
		Hash = (uint64(Hash) + (Hash << 15)) & uint64(0xffffffff);
		return Hash;
	}

	static constexpr uint32 fg_JenkinsHash(char const * const _pString, umint _Len, char _ExtraChar)
	{
		uint32 Hash = 0;
		for (char const *pStr = _pString; pStr < _pString + _Len; ++pStr)
		{
			Hash = (uint64(Hash) + *pStr) & uint64(0xffffffff);
			Hash = (uint64(Hash) + (Hash << 10)) & uint64(0xffffffff);
			Hash ^= (Hash >> 6);
		}
		if (_ExtraChar)
		{
			Hash = (uint64(Hash) + _ExtraChar) & uint64(0xffffffff);
			Hash = (uint64(Hash) + (Hash << 10)) & uint64(0xffffffff);
			Hash ^= (Hash >> 6);
		}
		Hash = (uint64(Hash) + (Hash << 3)) & uint64(0xffffffff);
		Hash ^= (Hash >> 11);
		Hash = (uint64(Hash) + (Hash << 15)) & uint64(0xffffffff);
		return Hash;
	}

	static consteval void fg_ParseUntilCallingConvention(char const * &_pParse)
	{
		umint nStart = 0;

		constexpr char const *c_CallingConventions[] =
			{
				"__thiscall "
				, "__cdecl "
				, "__clrcall "
				, "__stdcall "
				, "__fastcall "
				, "__thiscall "
				, "__vectorcall "
			}
		;

		auto pStart = _pParse;
		while (*_pParse)
		{
			if (*_pParse == '<')
				++nStart;
			else if (*_pParse == '>')
				--nStart;
			else if (*_pParse == '`')
			{
				while (*_pParse && *_pParse != '\'')
					++_pParse;
				if (*_pParse == '\'' && _pParse[1] == '{')
				{
					_pParse += 2;
					while (*_pParse && *_pParse != '\'')
						++_pParse;
					if (*_pParse == '\'')
					{
						++_pParse;
						continue;
					}
				}
			}
			else if (nStart == 0)
			{
				if (NStr::fg_CharIsAlphabetical(*_pParse) || NStr::fg_CharIsNumber(*_pParse) || *_pParse == ':' || *_pParse == '_')
					;
				else
				{
					if (*_pParse == ' ')
					{
						++_pParse;
						bool bFoundCallingConvention = false;
						for (auto &pCallingConvention : c_CallingConventions)
						{
							if (NStr::fg_StrStartsWith(pStart, pCallingConvention))
							{
								bFoundCallingConvention = true;
								break;
							}
						}
						if (bFoundCallingConvention)
							break;
						pStart = _pParse;
					}
				}
			}
			++_pParse;
		}
	}

	static consteval void fg_ParseTypeIdentifierConstexpr(char const * &_pParse)
	{
		umint nStart = 0;

		while (*_pParse)
		{
			if (*_pParse == '<')
				++nStart;
			else if (*_pParse == '>')
				--nStart;
			else if (*_pParse == '`')
			{
				while (*_pParse && *_pParse != '\'')
					++_pParse;
				if (*_pParse == '\'' && _pParse[1] == '{')
				{
					_pParse += 2;
					while (*_pParse && *_pParse != '\'')
						++_pParse;
					if (*_pParse == '\'')
					{
						++_pParse;
						continue;
					}
				}
			}
			else if (nStart == 0)
			{
				if (NStr::fg_CharIsAlphabetical(*_pParse) || NStr::fg_CharIsNumber(*_pParse) || *_pParse == ':' || *_pParse == '_')
					;
				else
					break;
			}
			++_pParse;
		}
	}

	template <typename tf_CType>
	static consteval CConstExprSubStr fg_GetTypeNameConstExpr()
	{
#if defined(DCompiler_MSVC)
		char const *pParseStart = DMibPFunctionSignature;
		char const *pParse = pParseStart;
		while (*pParse && *pParse != '<')
			++pParse;
		if (*pParse == '<')
			++pParse;
		if (NStr::fg_StrStartsWith(pParse, "class "))
			pParse += 6;
		else if (NStr::fg_StrStartsWith(pParse, "struct "))
			pParse += 7;
		char const *pStartType = pParse;
		umint nStart = 0;
		umint nStartParen = 0;

		while (*pParse)
		{
			if (*pParse == '(')
				++nStartParen;
			else if (*pParse == ')')
				--nStartParen;
			else if (!nStartParen)
			{
				if (*pParse == '<')
					++nStart;
				else if (*pParse == '>')
				{
					if (nStart == 0)
						break;
					--nStart;
				}
			}
			++pParse;
		}

		if (pStartType[0] == '`' && pStartType[1] == 'a')
			throw "Not portable"; // `anonymous-namespace'::

		return CConstExprSubStr(pStartType, (pParse - pStartType));
#else
		char const *pParseStart = DMibPFunctionSignature;
		char const *pParse = pParseStart;
		while (*pParse && *pParse != '=')
			++pParse;
		if (*pParse == '=')
			++pParse;
		if (*pParse == ' ')
			++pParse;
		char const *pStartType = pParse;
		umint nStart = 1;

		while (*pParse)
		{
			if (*pParse == '[')
				++nStart;
			else if (*pParse == ']')
			{
				if (--nStart == 0)
					break;
			}
			++pParse;
		}

		if (pStartType[0] == '(' && pStartType[1] == 'a')
			throw "Not portable"; // (anonymous namespace)::

		return CConstExprSubStr(pStartType, (pParse - pStartType));
#endif
	}

	template <typename tf_CType>
	ch8 const *fg_GetTypeName()
	{
#ifdef DCompiler_MSVC
		static ch8 s_ReturnName[sizeof(DMibPFunctionSignature)];
		static bool s_bInit = false;
		if (s_bInit)
			return s_ReturnName;
		ch8 const *pParseStart = DMibPFunctionSignature;
		ch8 const *pParse = pParseStart;
		while (*pParse && *pParse != '<')
			++pParse;
		if (*pParse == '<')
			++pParse;
		ch8 const *pStartType = pParse;
		umint nStart = 0;
		umint nStartParen = 0;

		while (*pParse)
		{
			if (nStartParen)
			{
				if (*pParse == '(')
					++nStartParen;
				else if (*pParse == ')')
					--nStartParen;
			}
			else
			{
				if (*pParse == '<')
					++nStart;
				else if (*pParse == '>')
				{
					if (nStart == 0)
						break;
					--nStart;
				}
			}
			++pParse;
		}
		NStr::fg_StrCopy(s_ReturnName, pStartType, (pParse - pStartType) + 1);
		NMib::NAtomic::fg_MemoryFence();
		s_bInit = true;
		NMib::NAtomic::fg_MemoryFence();
		return s_ReturnName;
#else
		static ch8 s_ReturnName[sizeof(DMibPFunctionSignature)];
		static constinit NAtomic::TCAtomic<uint32> s_bInit = 0;
		if (s_bInit.f_Load(NAtomic::gc_MemoryOrder_Acquire) == 2)
			return s_ReturnName;

		uint32 Expected = 0;
		if (s_bInit.f_CompareExchangeStrong(Expected, 1))
		{
			ch8 const *pParseStart = DMibPFunctionSignature;
			ch8 const *pParse = pParseStart;
			while (*pParse && *pParse != '=')
				++pParse;
			if (*pParse == '=')
				++pParse;
			if (*pParse == ' ')
				++pParse;
			ch8 const *pStartType = pParse;
			umint nStart = 1;

			while (*pParse)
			{
				if (*pParse == '[')
					++nStart;
				else if (*pParse == ']')
				{
					if (--nStart == 0)
						break;
				}
				++pParse;
			}
			NStr::fg_StrCopy(s_ReturnName, pStartType, (pParse - pStartType) + 1);
			s_bInit.f_Store(2, NAtomic::gc_MemoryOrder_Release);
		}
		else
		{
			while (s_bInit.f_Load(NAtomic::gc_MemoryOrder_Acquire) < 2)
				NSys::fg_Thread_SmallestSleep();
		}

		return s_ReturnName;

#endif
	}

	template <auto tf_MemberPointer>
	static consteval CConstExprSubStr fg_GetMemberPointerNameConstExpr()
	{
#if defined(DCompiler_MSVC)
		char const *pParse = DMibPFunctionSignature;
		while (*pParse && *pParse != '<')
			++pParse;
		if (*pParse == '<')
			++pParse;

		// Return type
		fg_ParseUntilCallingConvention(pParse);

		char const *pStartType = pParse;

		// Function name
		fg_ParseTypeIdentifierConstexpr(pParse);

		return CConstExprSubStr(pStartType, (pParse - pStartType));
#else
		char const *pParseStart = DMibPFunctionSignature;
		char const *pParse = pParseStart;
		while (*pParse && *pParse != '=')
			++pParse;
		if (*pParse == '=')
			++pParse;
		if (*pParse == ' ')
			++pParse;
		char const *pStartType = pParse;
		umint nStart = 1;

		while (*pParse)
		{
			if (*pParse == '[')
				++nStart;
			else if (*pParse == ']')
			{
				if (--nStart == 0)
					break;
			}
			++pParse;
		}
		if (*pStartType == '&')
			++pStartType;
		return CConstExprSubStr(pStartType, (pParse - pStartType));
#endif
	}

#if DMibSupportMemberNameFromMemberPointer

	template <auto tf_MemberFunction>
	static consteval uint32 fg_GetMemberFunctionHash()
	{
		using CMemberFunction = decltype(tf_MemberFunction);
		auto FunctionName = fg_GetMemberPointerNameConstExpr<tf_MemberFunction>();
		char const *pStartName = nullptr;
		char const *pEnd = FunctionName.m_pString + FunctionName.m_Len - 1;

		umint nOpen = 0;
		for (char const *pParse = pEnd; pParse > FunctionName.m_pString; --pParse)
		{
			if (*pParse == '>' || *pParse == ')' || *pParse == ']')
				++nOpen;
			else if (*pParse == '<' || *pParse == '(' || *pParse == '[')
				--nOpen;

			if (nOpen == 0 && *pParse == ':')
			{
				pStartName = pParse + 1;
				break;
			}
		}

		if (!pStartName)
			pStartName = FunctionName.m_pString;

		auto ClassTypeName = fg_GetTypeNameConstExpr<typename NTraits::TCMemberFunctionPointerTraits<CMemberFunction>::CClass>();
		return fg_JenkinsHash(pStartName, FunctionName.m_Len - (pStartName - FunctionName.m_pString), 0) ^ fg_JenkinsHash(ClassTypeName.m_pString, ClassTypeName.m_Len, ']');
	}

#else

	static consteval uint32 fg_GetMemberFunctionNameHash(char const * const _pFunctionName)
	{
		char const *pStartName = nullptr;
		char const *pEnd = _pFunctionName + NStr::fg_StrLen(_pFunctionName) - 1;

		umint nOpen = 0;
		for (char const *pParse = pEnd; pParse > _pFunctionName; --pParse)
		{
			if (*pParse == '>' || *pParse == ')' || *pParse == ']')
				++nOpen;
			else if (*pParse == '<' || *pParse == '(' || *pParse == '[')
				--nOpen;

			if (nOpen == 0 && *pParse == ':')
			{
				pStartName = pParse + 1;
				break;
			}
		}

		return fg_JenkinsHash(pStartName);
	}

	template <auto tf_MemberFunction>
	static consteval uint32 fg_GetMemberFunctionHash(uint32 _NameHash)
	{
		using CMemberFunction = decltype(tf_MemberFunction);
		auto ClassTypeName = fg_GetTypeNameConstExpr<typename NTraits::TCMemberFunctionPointerTraits<CMemberFunction>::CClass>();
		return _NameHash ^ fg_JenkinsHash(ClassTypeName.m_pString, ClassTypeName.m_Len, ']');
	}

	template <auto tf_MemberFunction, uint32 t_NameHash>
	static consteval uint32 fg_GetMemberFunctionHash()
	{
		return fg_GetMemberFunctionHash<tf_MemberFunction>(t_NameHash);
	}

#endif

	template <typename tf_CClass>
	static consteval uint32 fg_GetMemberFunctionHash(char const * const _pFunctionName)
	{
		auto ClassTypeName = fg_GetTypeNameConstExpr<tf_CClass>();
		return fg_JenkinsHash(_pFunctionName) ^ fg_JenkinsHash(ClassTypeName.m_pString, ClassTypeName.m_Len, ']');
	}

	template <typename tf_CType>
	static consteval uint32 fg_GetTypeHash()
	{
		auto ClassTypeName = fg_GetTypeNameConstExpr<tf_CType>();
		return fg_JenkinsHash(ClassTypeName.m_pString, ClassTypeName.m_Len, ']');
	}
}

