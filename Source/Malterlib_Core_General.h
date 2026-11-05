// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>

#include <bit>

// New override

namespace NMib
{
	struct CLineNumber
	{
		/*
		CLineNumber(const CLineNumber& _Src)
		{
			m_nLine = _Src.m_nLine;
		}*/

		CLineNumber(aint _nLine);

		operator aint () const;

		aint m_nLine;
	};
}

#if DMibConfig_MalterlibMemoryManager_Debug
#	define DMibNew new(DMibPFile, NMib::CLineNumber(DMibPLine))
#else
#	define DMibNew new
#endif

namespace NMib
{
	struct CHideNull
	{
		using CNullPtr = decltype(nullptr);
	};

	using CNullPtr = CHideNull::CNullPtr;

	struct CCompareConstructTag
	{
	};

	struct CAllocatorConstructTag
	{
	};

#if defined(DCompiler_MSVC) && defined(DCompiler_MSVC_EDG)

	namespace NPrivate
	{
		template <typename t_CType>
		struct TCForwardHelper
		{
			using CType = t_CType;

			inline_small t_CType const &operator () (t_CType const &_Left) const
			{
				return (_Left);
			}
		};
	}

	template <typename tf_CType>
	constexpr inline_always_debug tf_CType &&fg_Forward(typename NPrivate::TCForwardHelper<tf_CType>::CType &_ToForward)
	{
		return (tf_CType &&)_ToForward;
	}
#else

	// NOLINTBEGIN(malterlib-header-declarations-only): used in constant expressions of templates instantiated before Malterlib_Core_General.hpp is included, such as TCInt
	template <typename tf_CType>
	mark_artificial mark_nodebug constexpr inline_always_debug tf_CType &&fg_Forward(NTraits::TCRemoveReference<tf_CType> &_ToForward) noexcept
	{
		return static_cast<tf_CType &&>(_ToForward);
	}

	template <typename tf_CType>
	mark_artificial mark_nodebug constexpr inline_always_debug tf_CType &&fg_Forward(NTraits::TCRemoveReference<tf_CType> &&_ToForward) noexcept
	{
		static_assert(!NTraits::cIsLValueReference<tf_CType>, "Cannot be a lvalue reference here");
		return static_cast<tf_CType &&>(_ToForward);
	}

	// NOLINTEND(malterlib-header-declarations-only)
#endif

	template <typename tf_CType, size_t tf_ArrayElements>
	consteval size_t fg_ArraySize(tf_CType (&_Array)[tf_ArrayElements]) noexcept;

	template <typename tf_CType>
	consteval size_t fg_ArraySize(tf_CType const &_Array) noexcept
		requires (sizeof(tf_CType) == 0)
	;

	namespace NPrivate
	{
		template <typename t_CType, typename t_CTypeToCopyTo>
		struct TCForwardCopyEvalHelper
		{
			using CTypeQualifiers = NMib::NTraits::TCCopyQualifiers<NMib::NTraits::TCRemoveReference<t_CType>, NMib::NTraits::TCRemoveReference<t_CTypeToCopyTo>>;

			using CType = TCConditional
				<
					NMib::NTraits::cIsRValueReference<t_CType>
					, NMib::NTraits::TCAddRValueReference<CTypeQualifiers>
					, TCConditional<NMib::NTraits::cIsLValueReference<t_CType>, NMib::NTraits::TCAddLValueReference<CTypeQualifiers>, NMib::NTraits::TCRemoveReference<CTypeQualifiers>>
				>
			;
		};
	}

	template <typename tf_CType, typename tf_CTypeToCopyTo>
	mark_artificial mark_nodebug constexpr inline_always_debug decltype(auto) fg_ForwardAs(tf_CTypeToCopyTo &&_ToForward) noexcept;

	template <typename tf_CType>
	mark_artificial mark_nodebug constexpr inline_always_debug NTraits::TCRemoveReference<tf_CType> &&fg_Move(tf_CType &&_ToMove) noexcept
		requires (!NTraits::cIsConst<NTraits::TCRemoveReference<tf_CType>>) // Trying to move const value
	;

	template <typename tf_CType>
	mark_artificial mark_nodebug constexpr inline_always_debug NTraits::TCRemoveReference<tf_CType> fg_ExchangeMove(tf_CType &&_ToMove) noexcept
		requires (!NTraits::cIsConst<NTraits::TCRemoveReference<tf_CType>>) // Trying to move const value
	;

	template <typename tf_CType>
	mark_artificial mark_nodebug constexpr inline_always_debug NTraits::TCRemoveReference<tf_CType> &&fg_MoveAllowConst(tf_CType &&_ToMove) noexcept;

	// NOLINTBEGIN(malterlib-header-declarations-only): deduced return type, used by Core's string container .hpp files, which are included before this .hpp
	template <typename tf_CDestination, typename tf_CSetTo>
	mark_artificial mark_nodebug constexpr inline_always_debug auto fg_Exchange(tf_CDestination &_Destination, tf_CSetTo &&_SetTo)
	{
		auto Temp = fg_Move(_Destination);
		_Destination = fg_Forward<tf_CSetTo>(_SetTo);
		return Temp;
	}

	// NOLINTEND(malterlib-header-declarations-only)

	template <typename tf_CType>
	mark_artificial mark_nodebug constexpr inline_always_debug tf_CType fg_TempCopy(tf_CType const &_Value);

	template <typename tf_CToType, typename tf_CType>
	mark_artificial mark_nodebug constexpr inline_always_debug decltype(auto) fg_CopyOrMove(tf_CType &&_Value);

	template <typename tf_CType>
	mark_artificial mark_nodebug constexpr inline_always_debug tf_CType volatile &fg_Volatile(tf_CType &_In) noexcept;

	template <typename tf_CType>
	mark_artificial mark_nodebug constexpr inline_always_debug tf_CType const &fg_Const(tf_CType const &_In) noexcept;

	template <typename tf_CType, TCEnableIf<NTraits::cIsRValueReference<tf_CType> || !NTraits::cIsReference<tf_CType>> * = nullptr>
	mark_artificial mark_nodebug constexpr inline_always_debug decltype(auto) fg_ConstOrMove(tf_CType &&_In) noexcept;

	template <typename tf_CType, TCEnableIf<!NTraits::cIsRValueReference<tf_CType> && NTraits::cIsReference<tf_CType>> * = nullptr>
	mark_artificial mark_nodebug constexpr inline_always_debug auto fg_ConstOrMove(tf_CType &&_In) noexcept -> NTraits::TCRemoveReference<tf_CType> const &;

	template <typename t_CValue>
	struct TCMoveValueFunctor
	{
		mark_nodebug TCMoveValueFunctor(t_CValue &&_Value);

		mark_artificial mark_nodebug inline_always t_CValue operator () ();

	private:
		t_CValue mp_Value;
	};

	template <typename t_CType>
	struct TCCopy
	{
		mark_nodebug TCCopy(t_CType const &_Data);

		mark_nodebug TCCopy(TCCopy const &_Other);

		mark_nodebug TCCopy(TCCopy &&_Other);

		mark_nodebug t_CType const &operator * () const;

	private:
		t_CType const &mp_Data;
	};

	template <typename tf_CType>
	mark_nodebug inline_always_debug TCCopy<tf_CType> fg_Copy(tf_CType const &_In);

	template <typename t_CType>
	struct TCByValue
	{
		mark_nodebug TCByValue(t_CType const &_Data);

		mark_nodebug TCByValue(TCByValue const &_Other);

		mark_nodebug TCByValue(TCByValue &&_Other);

		mark_nodebug t_CType const &operator * () const;

	private:
		t_CType const &mp_Data;
	};

	template <typename tf_CType>
	mark_nodebug inline_always_debug TCByValue<tf_CType> fg_ByValue(tf_CType const &_In);

	template <typename t_CType>
	struct TCExplicit
	{
		template <typename tf_CType>
		mark_nodebug TCExplicit(tf_CType &&_Data);

		mark_nodebug t_CType operator * ();

	private:
		t_CType mp_Data;
	};

	template <>
	struct TCExplicit<void>
	{
	};

	template <typename tf_CType, TCEnableIf<NTraits::cIsReference<tf_CType>> * = nullptr>
	mark_nodebug inline_always_debug TCExplicit<tf_CType> fg_Explicit(tf_CType &&_In);

	template <typename tf_CType, TCEnableIf<!NTraits::cIsReference<tf_CType> && !NTraits::cIsConst<NTraits::TCRemoveReference<tf_CType>>> * = nullptr>
	mark_nodebug inline_always_debug TCExplicit<tf_CType> fg_Explicit(tf_CType &&_In);

	template <typename tf_CType, TCEnableIf<!NTraits::cIsReference<tf_CType> && NTraits::cIsConst<NTraits::TCRemoveReference<tf_CType>>> * = nullptr>
	mark_nodebug inline_always_debug TCExplicit<tf_CType> fg_Explicit(tf_CType &&_In);

	mark_nodebug inline_always_debug TCExplicit<void> fg_Explicit();

	struct CExplicitHelper
	{
		template <typename tf_CType, TCEnableIf<NTraits::cIsReference<tf_CType>> * = nullptr>
		mark_nodebug inline_always_debug TCExplicit<tf_CType> operator = (tf_CType &&_In) const;

		template <typename tf_CType, TCEnableIf<!NTraits::cIsReference<tf_CType>> * = nullptr>
		mark_nodebug inline_always_debug TCExplicit<tf_CType> operator = (tf_CType &&_In) const;
	};

	extern CExplicitHelper const &g_Explicit;

	template <typename t_CType>
	struct TCAttach
	{
		template <typename tf_CType>
		mark_nodebug TCAttach(tf_CType &&_Data);

		mark_nodebug t_CType operator * ();

	private:
		t_CType mp_Data;
	};

	template <>
	struct TCAttach<void>
	{
	};

	template <typename tf_CType, TCEnableIf<NTraits::cIsReference<tf_CType>> * = nullptr>
	mark_nodebug inline_always_debug TCAttach<tf_CType> fg_Attach(tf_CType &&_In);

	template <typename tf_CType, TCEnableIf<!NTraits::cIsReference<tf_CType> && !NTraits::cIsConst<NTraits::TCRemoveReference<tf_CType>>> * = nullptr>
	mark_nodebug inline_always_debug TCAttach<tf_CType> fg_Attach(tf_CType &&_In);

	template <typename tf_CType, TCEnableIf<!NTraits::cIsReference<tf_CType> && NTraits::cIsConst<NTraits::TCRemoveReference<tf_CType>>> * = nullptr>
	mark_nodebug inline_always_debug TCAttach<tf_CType> fg_Attach(tf_CType &&_In);

	mark_nodebug inline_always_debug TCAttach<void> fg_Attach();

	namespace NInternal
	{
		struct CDefault
		{
			template <typename tf_CReturn>
			mark_nodebug operator tf_CReturn ();
		};

		struct CIgnore
		{
			template <typename tf_CType>
			mark_nodebug inline_always_debug CIgnore &operator = (tf_CType &&_Value);
		};
	}

	mark_nodebug inline_always_debug NInternal::CIgnore fg_Ignore();

	mark_nodebug inline_always_debug NInternal::CDefault fg_Default();

	namespace NInternal
	{
		template <typename t_CFrom>
		struct TCAutoStaticCast
		{
			template <typename tf_CFrom>
			mark_nodebug TCAutoStaticCast(tf_CFrom &&_From);

			template <typename tf_CTo>
			mark_nodebug operator tf_CTo ();

			t_CFrom m_From;
		};
	}
	template <typename tf_CFrom>
	mark_nodebug NInternal::TCAutoStaticCast<tf_CFrom> fg_AutoStaticCast(tf_CFrom &&_From);

	namespace NInternal
	{
		template <typename t_CFrom>
		struct TCAutoConstCast
		{
			template <typename tf_CFrom>
			mark_nodebug TCAutoConstCast(tf_CFrom &&_From);

			template <typename tf_CTo>
			mark_nodebug operator tf_CTo ();

			t_CFrom m_From;
		};
	}
	template <typename tf_CFrom>
	mark_nodebug NInternal::TCAutoConstCast<tf_CFrom> fg_AutoConstCast(tf_CFrom &&_From);

	namespace NInternal
	{
		template <typename t_CFrom>
		struct TCAutoReinterpretCast
		{
			template <typename tf_CFrom>
			mark_nodebug TCAutoReinterpretCast(tf_CFrom &&_From);

			template <typename tf_CTo>
			mark_nodebug operator tf_CTo ();

			t_CFrom m_From;
		};
	}
	template <typename tf_CFrom>
	mark_nodebug NInternal::TCAutoReinterpretCast<tf_CFrom> fg_AutoReinterpretCast(tf_CFrom &&_From);

	namespace NInternal
	{
		template <typename t_CFrom>
		struct TCAutoCCast
		{
			template <typename tf_CFrom>
			mark_nodebug TCAutoCCast(tf_CFrom &&_From);

			template <typename tf_CTo>
			mark_nodebug operator tf_CTo ();

			t_CFrom m_From;
		};
	}
	template <typename tf_CFrom>
	mark_nodebug NInternal::TCAutoCCast<tf_CFrom> fg_AutoCCast(tf_CFrom &&_From);

	template <bool t_bCopy, bool t_bMove>
	struct TCSupportCopyMove
	{
	};

	template <>
	struct TCSupportCopyMove<true, false>
	{
		TCSupportCopyMove() = default;
		TCSupportCopyMove(TCSupportCopyMove const &_Other) = default;
		TCSupportCopyMove(TCSupportCopyMove &&_Other) = delete;

		TCSupportCopyMove &operator = (TCSupportCopyMove const &_Other) = default;
		TCSupportCopyMove &operator = (TCSupportCopyMove &&_Other) = delete;
	};

	template <>
	struct TCSupportCopyMove<false, true>
	{
		TCSupportCopyMove() = default;
		TCSupportCopyMove(TCSupportCopyMove const &_Other) = delete;
		TCSupportCopyMove(TCSupportCopyMove &&_Other) = default;
		TCSupportCopyMove &operator = (TCSupportCopyMove const &_Other) = delete;
		TCSupportCopyMove &operator = (TCSupportCopyMove &&_Other) = default;
	};

	template <>
	struct TCSupportCopyMove<false, false>
	{
		TCSupportCopyMove() = default;
		TCSupportCopyMove(TCSupportCopyMove const &_Other) = delete;
		TCSupportCopyMove(TCSupportCopyMove &&_Other) = delete;
		TCSupportCopyMove &operator = (TCSupportCopyMove const &_Other) = delete;
		TCSupportCopyMove &operator = (TCSupportCopyMove &&_Other) = delete;
	};

	template <typename tf_CType>
	mark_nodebug inline_always_debug tf_CType &fg_RemoveQualifiers(tf_CType &_In);

	template <typename tf_CType>
	mark_nodebug inline_always_debug tf_CType &fg_RemoveQualifiers(tf_CType const &_In);

	template <typename tf_CType>
	mark_nodebug inline_always_debug tf_CType &fg_RemoveQualifiers(tf_CType volatile &_In);

	template <typename tf_CType>
	mark_nodebug inline_always_debug tf_CType &fg_RemoveQualifiers(tf_CType const volatile &_In);

	template <typename tf_CType>
	mark_nodebug inline_always_debug tf_CType &&fg_RemoveQualifiers(tf_CType &&_In);

	template <typename tf_CType>
	mark_nodebug inline_always_debug tf_CType &&fg_RemoveQualifiers(tf_CType const &&_In);

	template <typename tf_CType>
	mark_nodebug inline_always_debug tf_CType &&fg_RemoveQualifiers(tf_CType volatile &&_In);

	template <typename tf_CType>
	mark_nodebug inline_always_debug tf_CType &&fg_RemoveQualifiers(tf_CType const volatile &&_In);

	template <typename t_CType>
	struct TCLambdaMover
	{
		mark_nodebug TCLambdaMover(t_CType &&_Other);
		mark_nodebug TCLambdaMover(TCCopy<t_CType> &&_Other);
		mark_nodebug TCLambdaMover(TCLambdaMover const &_Other);
		mark_nodebug TCLambdaMover(TCLambdaMover &_Other);
		mark_nodebug TCLambdaMover(TCLambdaMover &&_Other);

		mark_nodebug t_CType &operator * () const;

	private:
		t_CType mp_Type;
	};

	template <typename tf_CType>
	mark_nodebug TCLambdaMover<NTraits::TCRemoveReferenceAndQualifiers<tf_CType>> fg_LambdaMove(tf_CType &&_Type);

	template <typename tf_CType>
	mark_nodebug TCLambdaMover<NTraits::TCRemoveReferenceAndQualifiers<tf_CType>> fg_LambdaMove(TCCopy<tf_CType> &&_Type);

	template <typename tf_CType>
	mark_nodebug inline_always_debug tf_CType volatile const &fg_ConstVolatile(tf_CType &_In);

	template <typename tf_CType>
	mark_nodebug static constexpr inline_small tf_CType fg_PowerOfTwoMinusOne(uaint _Power);

	template <typename tf_CType0, typename tf_CType1>
	static inline_small void fg_Swap(tf_CType0 &_Left, tf_CType1 &_Right);

	template <typename tf_CTo, typename tf_CFrom>
	static constexpr inline_small tf_CTo fg_BitCast(tf_CFrom const &_From);

	template <typename tf_CType>
	mark_nodebug inline_always_debug tf_CType *fg_NullPtr();

	template <typename t_CType>
	struct TCHelper_ByteSwap
	{
		using CType = t_CType;

		static t_CType fs_Swap(t_CType const &_Data);

		static constexpr aint mc_DefaultImplementation = 1;
	};

#ifdef DMibPCanDo_uint8
	template <>
	struct TCHelper_ByteSwap<uint8>
	{
		using CType = uint8;

		static inline_small uint8 fs_Swap(uint8 _Data);

		static constexpr aint mc_DefaultImplementation = 0;
	};
#endif

#define DMibPDefaultSwap_uint16(d_Value) ((((uint16)(d_Value) & 0xff00) >> 8) | \
	(((uint16)(d_Value) & 0x00ff) << 8))

#define DMibPDefaultSwap_uint32(d_Value) ((((uint32)(d_Value) & 0xff000000) >> 24) | \
	(((uint32)(d_Value) & 0x00ff0000) >>  8) | \
	(((uint32)(d_Value) & 0x0000ff00) <<  8) | \
	(((uint32)(d_Value) & 0x000000ff) << 24))

#define DMibPDefaultSwap_uint64(d_Value) ((((uint64)(d_Value) & 0xff00000000000000ULL) >> 56) | \
	(((uint64)(d_Value) & 0x00ff000000000000ULL) >> 40) | \
	(((uint64)(d_Value) & 0x0000ff0000000000ULL) >> 24) | \
	(((uint64)(d_Value) & 0x000000ff00000000ULL) >>  8) | \
	(((uint64)(d_Value) & 0x00000000ff000000ULL) <<  8) | \
	(((uint64)(d_Value) & 0x0000000000ff0000ULL) << 24) | \
	(((uint64)(d_Value) & 0x000000000000ff00ULL) << 40) | \
	(((uint64)(d_Value) & 0x00000000000000ffULL) << 56))

#ifdef DMibPCanDo_uint16
	template <>
	struct TCHelper_ByteSwap<uint16>
	{
		using CType = uint16;

		static inline_small uint16 fs_Swap(uint16 _Data);

		static constexpr aint mc_DefaultImplementation = 0;
	};
#endif
#ifdef DMibPCanDo_uint32
	template <>
	struct TCHelper_ByteSwap<uint32>
	{
		using CType = uint32;

		static inline_small uint32 fs_Swap(uint32 _Data);

		static constexpr aint mc_DefaultImplementation = 0;
	};
#endif
#ifdef DMibPCanDo_uint64
	template <>
	struct TCHelper_ByteSwap<uint64>
	{
		using CType = uint64;

		static inline_small uint64 fs_Swap(uint64 _Data);

		static constexpr aint mc_DefaultImplementation = 0;
	};
#endif

	template <umint t_nBytes, typename t_CType>
	struct TCHelper_ByteSwapChooseBySize
	{
		using CSwapper = TCHelper_ByteSwap<t_CType>;
	};

	template <typename t_CType>
	struct TCHelper_ByteSwapChooseBySize<1, t_CType>
	{
		using CSwapper = TCHelper_ByteSwap<uint8>;
	};

	template <typename t_CType>
	struct TCHelper_ByteSwapChooseBySize<2, t_CType>
	{
		using CSwapper = TCHelper_ByteSwap<uint16>;
	};

	template <typename t_CType>
	struct TCHelper_ByteSwapChooseBySize<4, t_CType>
	{
		using CSwapper = TCHelper_ByteSwap<uint32>;
	};

	template <typename t_CType>
	struct TCHelper_ByteSwapChooseBySize<8, t_CType>
	{
		using CSwapper = TCHelper_ByteSwap<uint64>;
	};

	template <typename tf_CInt>
	inline_small tf_CInt fg_ByteSwap(tf_CInt _In);

	template <typename tf_CInt>
	inline_small void fg_ByteSwap(tf_CInt *_pIn, umint _Len);

	template <typename tf_CInt>
	inline_small tf_CInt fg_ByteSwapLE(tf_CInt _In);

	template <typename tf_CInt>
	inline_small tf_CInt fg_ByteSwapBE(tf_CInt _In);

	// NOLINTBEGIN(malterlib-header-declarations-only): used in constant expressions of templates instantiated before Malterlib_Core_General.hpp is included, such as TCInt
	template <typename tf_CFirst, typename tf_CSecond>
	constexpr inline_small NTraits::TCRemoveReference<tf_CFirst> fg_Min(tf_CFirst &&_First, tf_CSecond &&_Second)
	{
		if (fg_Forward<tf_CFirst>(_First) < fg_Forward<tf_CSecond>(_Second))
			return fg_Forward<tf_CFirst>(_First);
		else
			return fg_Forward<tf_CSecond>(_Second);
	}

	template <typename tf_CFirst, typename tf_CSecond, typename ...tfp_CRest>
	constexpr inline_small NTraits::TCRemoveReference<tf_CFirst> fg_Min(tf_CFirst &&_First, tf_CSecond &&_Second, tfp_CRest && ...p_Rest)
	{
		if (fg_Forward<tf_CFirst>(_First) < fg_Forward<tf_CSecond>(_Second))
			return fg_Min(fg_Forward<tf_CFirst>(_First), fg_Forward<tfp_CRest>(p_Rest)...);
		else
			return fg_Min(fg_Forward<tf_CSecond>(_Second), fg_Forward<tfp_CRest>(p_Rest)...);
	}

	template <typename tf_CFirst, typename tf_CSecond>
	constexpr inline_small NTraits::TCRemoveReference<tf_CFirst> fg_Max(tf_CFirst &&_First, tf_CSecond &&_Second)
	{
		if (fg_Forward<tf_CSecond>(_Second) < fg_Forward<tf_CFirst>(_First))
			return fg_Forward<tf_CFirst>(_First);
		else
			return fg_Forward<tf_CSecond>(_Second);
	}

	template <typename tf_CFirst, typename tf_CSecond, typename ...tfp_CRest>
	constexpr inline_small NTraits::TCRemoveReference<tf_CFirst> fg_Max(tf_CFirst &&_First, tf_CSecond &&_Second, tfp_CRest && ...p_Rest)
	{
		if (fg_Forward<tf_CSecond>(_Second) < fg_Forward<tf_CFirst>(_First))
			return fg_Max(fg_Forward<tf_CFirst>(_First), fg_Forward<tfp_CRest>(p_Rest)...);
		else
			return fg_Max(fg_Forward<tf_CSecond>(_Second), fg_Forward<tfp_CRest>(p_Rest)...);
	}

	// NOLINTEND(malterlib-header-declarations-only)

	template <typename tf_CFirst, typename tf_CMin, typename tf_CMax>
	constexpr inline_small NTraits::TCRemoveReference<tf_CFirst> fg_Clamp(tf_CFirst &&_First, tf_CMin &&_Min, tf_CMax &&_Max);

	template <typename tf_CLeft, typename tf_CRight>
	constexpr inline_small NTraits::TCRemoveReference<tf_CLeft> fg_MaxValidFloat(tf_CLeft &&_Left, tf_CRight &&_Right);

	template <typename tf_CFirst, typename tf_CSecond, typename ...tfp_CRest>
	constexpr inline_small NTraits::TCRemoveReference<tf_CFirst> fg_MaxValidFloat(tf_CFirst &&_First, tf_CSecond &&_Second, tfp_CRest && ...p_Rest);

	template <typename tf_CLeft, typename tf_CRight>
	constexpr inline_small NTraits::TCRemoveReference<tf_CLeft> fg_MinValidFloat(tf_CLeft &&_Left, tf_CRight &&_Right);

	template <typename tf_CFirst, typename tf_CSecond, typename ...tfp_CRest>
	constexpr inline_small NTraits::TCRemoveReference<tf_CFirst> fg_MinValidFloat(tf_CFirst &&_First, tf_CSecond &&_Second, tfp_CRest && ...p_Rest);

	template <typename tf_CFirst>
	constexpr inline_small NTraits::TCRemoveReference<tf_CFirst> fg_Abs(tf_CFirst &&_First);

	// NOLINTBEGIN(malterlib-header-declarations-only): used in constant expressions before the .hpp is included, in the alignas of TCFloat
	// in Malterlib_Numeric_Float.h, the static constexpr members of TCVariantCommon in Malterlib_Storage_Variant.h and TCVector in
	// Malterlib_Container_Vector.h, and a static_assert in Malterlib_Concurrency_Promise.h
	template <typename tf_CFirst>
	constexpr inline_small tf_CFirst fg_MinConstexpr(tf_CFirst _First)
	{
		return _First;
	}

	template <typename tf_CFirst, typename tf_CSecond>
	constexpr inline_small tf_CFirst fg_MinConstexpr(tf_CFirst _First, tf_CSecond _Second)
	{
		return (_First < _Second) ? _First : _Second;
	}

	template <typename tf_CFirst, typename tf_CSecond, typename ...tfp_CRest>
	constexpr inline_small tf_CFirst fg_MinConstexpr(tf_CFirst _First, tf_CSecond _Second, tfp_CRest && ...p_Rest)
	{
		return fg_MinConstexpr((_First < _Second) ? _First : _Second, p_Rest...);
	}

	template <typename tf_CFirst>
	constexpr inline_small tf_CFirst fg_MaxConstexpr(tf_CFirst _First)
	{
		return _First;
	}

	template <typename tf_CFirst, typename tf_CSecond>
	constexpr inline_small tf_CFirst fg_MaxConstexpr(tf_CFirst _First, tf_CSecond _Second)
	{
		return (_Second < _First) ? _First : _Second;
	}

	template <typename tf_CFirst, typename tf_CSecond, typename ...tfp_CRest>
	constexpr inline_small tf_CFirst fg_MaxConstexpr(tf_CFirst _First, tf_CSecond _Second, tfp_CRest && ...p_Rest)
	{
		return fg_MaxConstexpr((_Second < _First) ? _First : _Second, p_Rest...);
	}

	// NOLINTEND(malterlib-header-declarations-only)

	template <typename tf_CFirst, typename tf_CMin, typename tf_CMax>
	constexpr inline_small tf_CFirst fg_ClampConstexpr(tf_CFirst _First, tf_CMin _Min, tf_CMax _Max);

	template <typename tf_CFirst>
	constexpr inline_small tf_CFirst fg_AbsConstexpr(tf_CFirst _First);

	// NOLINTBEGIN(malterlib-header-declarations-only): used in constant expressions before the .hpp is included, in
	// gc_ActorQueueDispatchFunctionMemory in Malterlib_Concurrency_RunQueue.h and array bounds in Malterlib_Concurrency_ActorHolder.h
	template <typename tf_CToAlign>
	constexpr inline_small tf_CToAlign fg_AlignUpConstExpr(tf_CToAlign _Mem, umint _Alignment)
	{
		using CIntegerType = NTraits::TCUnsigned<NTraits::TCIntFromSizeLarger<sizeof(tf_CToAlign)>>;
		return (tf_CToAlign)((((CIntegerType)_Mem) + CIntegerType(_Alignment - 1)) & (~(CIntegerType(_Alignment) - CIntegerType(1))));
	}

	// NOLINTEND(malterlib-header-declarations-only)

	template <typename tf_CToAlign>
	constexpr inline_small tf_CToAlign fg_AlignDownConstExpr(tf_CToAlign _Mem, umint _Alignment);

	template <typename tf_CToAlign>
	inline_small tf_CToAlign fg_AlignUp(tf_CToAlign _Mem, umint _Alignment);

	template <typename tf_CToAlign>
	inline_small tf_CToAlign fg_AlignDown(tf_CToAlign _Mem, umint _Alignment);

	template <typename t_CToAlign, t_CToAlign t_Value, t_CToAlign t_Align>
	struct TCAlignUp
	{
		static constexpr t_CToAlign mc_Value = (t_Value + (t_Align - 1)) & (~(t_Align - 1));
	};

	template <typename t_CToAlign, t_CToAlign t_Value, t_CToAlign t_Align>
	struct TCAlignDown
	{
		static constexpr t_CToAlign mc_Value = t_Value & (~(t_Align - 1));
	};

	template <typename tf_CType>
	inline_small tf_CType fg_HighPart(tf_CType _Data);

	template <typename tf_CType>
	inline_small tf_CType fg_LowPart(tf_CType _Data);

	template <typename tf_CType>
	TCEnableIf<!NTraits::cIsVoid<tf_CType> && !NTraits::cIsArray<tf_CType> && !NTraits::cIsPointer<tf_CType>, void> fg_CallDestructor(tf_CType &_Type);

	template <typename tf_CType>
	TCEnableIf<NTraits::cIsVoid<tf_CType> || NTraits::cIsPointer<tf_CType>, void> fg_CallDestructor(tf_CType &_Type);

	template <typename tf_CType>
	TCEnableIf<NTraits::cIsArray<tf_CType>, void> fg_CallDestructor(tf_CType &_Type);

	struct CSort_Default
	{
		template <typename tf_CKey0, typename tf_CKey1>
		static constexpr inline_small auto operator () (tf_CKey0 &&_Left, tf_CKey1 &&_Right) noexcept(noexcept(_Left <=> _Right));
	};

	namespace NPrivate
	{
#ifdef DCompiler_MSVC
#	pragma warning(push)
#	pragma warning(disable:4307)
#	pragma warning(disable:4309)
#endif

		template <typename t_CIntType, bool t_bSigned>
		struct TCLimitsIntHelper
		{
			static constexpr t_CIntType mc_Min = 0;
			static constexpr t_CIntType mc_Max = t_CIntType(0) - 1;
			static constexpr t_CIntType mc_AllBits = mc_Max;
		};

		template <typename t_CIntType>
		struct TCLimitsIntHelper<t_CIntType, 1>
		{
			using CUnsigned = NMib::NTraits::TCUnsigned<t_CIntType>;

			static constexpr t_CIntType mc_Min = (t_CIntType(1) << ((sizeof(t_CIntType) * 8) - 1));
			static constexpr t_CIntType mc_Max = t_CIntType((CUnsigned(1) << ((sizeof(t_CIntType) * 8) - 1)) - CUnsigned(1));
			static constexpr t_CIntType mc_AllBits = t_CIntType(CUnsigned(0) - 1);
		};

#ifdef DCompiler_MSVC
#	pragma warning(pop)
#endif
	}

	template <typename t_CIntType>
	struct TCLimitsInt : public NMib::NPrivate::TCLimitsIntHelper<t_CIntType, NTraits::cIsSigned<t_CIntType>>
	{
	};

	struct CAutoLimitMin
	{
		template <typename tf_CType>
		operator tf_CType ();
	};

	struct CAutoLimitMax
	{
		template <typename tf_CType>
		operator tf_CType ();
	};

	inline_always static CAutoLimitMin fg_LimitsMin();

	inline_always static CAutoLimitMax fg_LimitsMax();

#if defined(DCompiler_clang) || defined(DCompiler_gcc)
	template <typename tf_CInt>
	constexpr inline_small bool fg_MultiplyOverflow(tf_CInt _Left, tf_CInt _Right, tf_CInt &o_Result) noexcept
		requires (NTraits::cIsFundamental<tf_CInt> && NTraits::cIsInteger<tf_CInt> && !NTraits::cIsSame<NTraits::TCRemoveQualifiers<tf_CInt>, bool>)
	;
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
	;

	template <typename tf_CInt0, typename tf_CInt1>
	bool fg_SafeLargerThan(tf_CInt0 const &_Left, tf_CInt1 const &_Right);

	namespace NStr
	{
		namespace NPrivate
		{
			template <typename t_CFormatter, typename t_CData>
			struct TCDetermineStringFormatterReturnType;
		}

		template <typename tf_CFormatter, typename tf_CData>
		inline_small typename NPrivate::TCDetermineStringFormatterReturnType<tf_CFormatter, tf_CData>::CType fg_CreateStringFormatter(tf_CFormatter &_Formatter, tf_CData const &_Data);
	}
	// Auto clear int

	template <typename t_CInt, t_CInt t_ClearVal = 0>
	struct TCAutoClearInt
	{
		TCAutoClearInt();

		TCAutoClearInt(t_CInt const &_Int);

		operator t_CInt & ();

		operator t_CInt const & () const;

		t_CInt &f_Get();

		t_CInt const &f_Get() const;

		TCAutoClearInt &operator = (TCAutoClearInt const &_Other);

		TCAutoClearInt &operator = (t_CInt const &_Other);

		template <typename tf_CFormatter>
		int f_GetStringFormatType(tf_CFormatter &_Formatter);

		template <typename tf_CFormatter>
		auto f_CreateStringFormatter(tf_CFormatter &_Formatter) const -> decltype(NStr::fg_CreateStringFormatter(_Formatter, fg_GetType<t_CInt const &>()));

		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;

		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		auto operator <=> (TCAutoClearInt const &_Right) const noexcept(noexcept(f_Get() <=> _Right.f_Get()));

		bool operator == (TCAutoClearInt const &_Right) const noexcept(noexcept(f_Get() == _Right.f_Get()));

		template <typename tf_CRight>
		auto operator <=> (tf_CRight const &_Right) const noexcept(noexcept(f_Get() <=> _Right));

		template <typename tf_CRight, tf_CRight tf_RightValue>
		auto operator <=> (TCAutoClearInt<tf_CRight, tf_RightValue> const &_Right) const noexcept(noexcept(f_Get() <=> _Right.f_Get()));

		template <typename tf_CRight>
		bool operator == (tf_CRight const &_Right) const noexcept(noexcept(f_Get() == _Right));

		template <typename tf_CRight, tf_CRight tf_RightValue>
		bool operator == (TCAutoClearInt<tf_CRight, tf_RightValue> const &_Right) const noexcept(noexcept(f_Get() == _Right.f_Get()));

		t_CInt m_Int;

	private:
		static t_CInt const &fsp_GetType();
	};

	template <typename t_CType>
	struct TCAutoClear
	{
		TCAutoClear() = default;
		TCAutoClear(TCAutoClear const &_Other) = default;
		TCAutoClear(TCAutoClear &&_Other) = default;

		TCAutoClear(t_CType const &_Value);

		operator t_CType & ();

		operator t_CType const & () const;

		t_CType &f_Get();

		t_CType const &f_Get() const;

		TCAutoClear &operator = (TCAutoClear const &_Other) = default;
		TCAutoClear &operator = (TCAutoClear &&_Other) = default;

		TCAutoClear &operator = (t_CType const &_Other);

		template <typename tf_CFormatter>
		int f_GetStringFormatType(tf_CFormatter &_Formatter);

		template <typename tf_CFormatter>
		auto f_CreateStringFormatter(tf_CFormatter &_Formatter) const -> decltype(NStr::fg_CreateStringFormatter(_Formatter, fg_GetType<t_CType const &>()));

		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;

		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		auto operator <=> (TCAutoClear const &_Right) const noexcept(noexcept(f_Get() <=> _Right.f_Get()));

		bool operator == (TCAutoClear const &_Right) const noexcept(noexcept(f_Get() == _Right.f_Get()));

		template <typename tf_CRight>
		auto operator <=> (tf_CRight const &_Right) const noexcept(noexcept(f_Get() <=> _Right));

		template <typename tf_CRight>
		auto operator <=> (TCAutoClear<tf_CRight> const &_Right) const noexcept(noexcept(f_Get() <=> _Right.f_Get()));

		template <typename tf_CRight>
		bool operator == (tf_CRight const &_Right) const noexcept(noexcept(f_Get() == _Right));

		template <typename tf_CRight>
		bool operator == (TCAutoClear<tf_CRight> const &_Right) const noexcept(noexcept(f_Get() == _Right.f_Get()));

		template <typename tf_CRight, tf_CRight tf_RightValue>
		auto operator <=> (TCAutoClearInt<tf_CRight, tf_RightValue> const &_Right) const noexcept(noexcept(f_Get() <=> _Right.f_Get()));

		template <typename tf_CRight, tf_CRight tf_RightValue>
		bool operator == (TCAutoClearInt<tf_CRight, tf_RightValue> const &_Right) const noexcept(noexcept(f_Get() == _Right.f_Get()));

		t_CType m_Value = 0;
	};

	template <typename t_CType, t_CType t_Argument0, t_CType t_Argument1>
	constexpr inline t_CType gc_ConstantMax = (t_Argument0 > t_Argument1 ? t_Argument0 : t_Argument1);

	template <typename t_CType, t_CType t_Argument0, t_CType t_Argument1>
	constexpr inline t_CType gc_ConstantMin = (t_Argument0 < t_Argument1 ? t_Argument0 : t_Argument1);

	template <typename t_CType, t_CType t_Argument0>
	constexpr inline t_CType gc_ConstantAbs = (t_Argument0 < t_CType(0) ? (t_CType(0) - t_Argument0) : t_Argument0);

	template <typename tf_CAny>
	static tf_CAny fg_MakeSymbolActive(tf_CAny &&_Other);

	template <typename tf_CType>
	ch8 const *fg_GetTypeName();

	namespace NStr
	{
		template <typename tf_CData>
		constexpr umint fg_StrLen(tf_CData const *_pStr);
	}

	struct CConstExprSubStr
	{
		constexpr CConstExprSubStr(ch8 const *_pString, umint _Len);

		constexpr CConstExprSubStr(ch8 const *_pString);

		consteval ch8 const *data() const;

		consteval umint size() const;

		ch8 const *m_pString = nullptr;
		umint m_Len = 0;
	};

	template <typename tf_CType>
	static consteval CConstExprSubStr fg_GetTypeNameConstExpr();

	template <umint t_nCharacters>
	struct TCConstExprSubStr : public CConstExprSubStr
	{
		constexpr TCConstExprSubStr(char const *_pString);

		char m_String[t_nCharacters + 1];
	};

	template <typename tf_CType>
	static constexpr auto fg_GetTypeNameConstExprArray();

	static constexpr uint32 fg_JenkinsHash(char const * const _pString);
	static constexpr uint32 fg_JenkinsHash(char const * const _pString, umint _Len, char _ExtraChar);

	static consteval void fg_ParseTypeIdentifierConstexpr(char const * &_pParse);
	static consteval void fg_ParseUntilCallingConvention(char const * &_pParse);

	template <auto tf_MemberPointer>
	static consteval CConstExprSubStr fg_GetMemberPointerNameConstExpr();

#ifdef DCompiler_MSVC_Workaround
#	define DMibSupportMemberNameFromMemberPointer 0
#else
#	define DMibSupportMemberNameFromMemberPointer 1
#endif

#if DMibSupportMemberNameFromMemberPointer

	template <auto tf_MemberFunction>
	static consteval uint32 fg_GetMemberFunctionHash();

#	define DMibPointerToMemberFunctionForHash(d_FunctionName) d_FunctionName
#	define DMibIfNotSupportMemberNameFromMemberPointer(...)

#else

	static consteval uint32 fg_GetMemberFunctionNameHash(char const * const _pFunctionName);

	template <auto tf_MemberFunction>
	static consteval uint32 fg_GetMemberFunctionHash(uint32 _NameHash);

#	define DMibPointerToMemberFunctionForHash(d_FunctionName) d_FunctionName, fg_GetMemberFunctionNameHash(DMibStringize(d_FunctionName))
#	define DMibIfNotSupportMemberNameFromMemberPointer(...) __VA_ARGS__

#endif

	template <typename tf_CClass>
	static consteval uint32 fg_GetMemberFunctionHash(char const * const _pFunctionName);

	template <typename tf_CType>
	static consteval uint32 fg_GetTypeHash();

#ifdef DCompiler_MSVC_Workaround
#	define DMibConstantTypeHash(d_Type) NMib::fg_GetTypeHash<d_Type>()
#else
	template <typename t_CType>
	struct TCGetTypeHash
	{
		static constexpr uint32 mc_Value = fg_GetTypeHash<t_CType>();
	};

#	define DMibConstantTypeHash(d_Type) NMib::TCGetTypeHash<d_Type>::mc_Value
#endif
}

#include <Mib/Bit/Bit>

// Gets a pointer to a class wich member is contained in from a pointer to that member
#define DMibGetParent(d_Class, d_Member, d_Ptr) ((d_Class *)(((uint8 *)d_Ptr) + ( ((umint)((d_Class *)((void*)d_Ptr))) - ((umint)(&((d_Class *)((void *)d_Ptr))->d_Member)) )))

#ifndef DMibPNoShortCuts
#	define DGetParent(d_Class, d_Member, d_Ptr) DMibGetParent(d_Class, d_Member, d_Ptr)
#	define DNew DMibNew
#endif
