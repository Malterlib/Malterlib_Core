// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

#include <bit>

// New override

namespace NMib
{
	class CLineNumber
	{
	public:
	/*	CLineNumber(const CLineNumber& _Src)
		{
			m_nLine = _Src.m_nLine;
		}*/

		CLineNumber(aint _nLine)
			:m_nLine(_nLine)
		{
		}
		aint m_nLine;

		operator aint () const
		{
			return m_nLine;
		}
	};
}

#if DMibConfig_MalterlibMemoryManager_Debug
#define DMibNew new(DMibPFile, NMib::CLineNumber(DMibPLine))
#else
#define DMibNew new
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

			inline_small const t_CType& operator()(const t_CType& _Left) const
			{
				return (_Left);
			}
		};
	}

	template <typename t_CType>
	constexpr inline_always_debug t_CType&& fg_Forward(typename NPrivate::TCForwardHelper<t_CType>::CType &_ToForward)
	{
		return (t_CType&& )_ToForward;
	}
#else

	template <typename t_CType>
	mark_artificial mark_nodebug constexpr inline_always_debug t_CType&& fg_Forward(NTraits::TCRemoveReference<t_CType> &_ToForward) noexcept
	{
		return static_cast<t_CType&&>(_ToForward);
	}

	template <typename t_CType>
	mark_artificial mark_nodebug constexpr inline_always_debug t_CType&& fg_Forward(NTraits::TCRemoveReference<t_CType> &&_ToForward) noexcept
	{
		static_assert(!NTraits::cIsLValueReference<t_CType>, "Cannot be a lvalue reference here");
		return static_cast<t_CType&&>(_ToForward);
	}
#endif

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

	namespace NPrivate
	{
		template <typename t_CType, typename t_CTypeToCopyTo>
		struct TCForwardCopyEvalHelper
		{
			using CTypeQualifiers = NMib::NTraits::TCCopyQualifiers
				<
					NMib::NTraits::TCRemoveReference<t_CType>
					, NMib::NTraits::TCRemoveReference<t_CTypeToCopyTo>
				>
			;

		    using CType = TCConditional
				<
					NMib::NTraits::cIsRValueReference<t_CType>
					, NMib::NTraits::TCAddRValueReference<CTypeQualifiers>
					, TCConditional
					<
						NMib::NTraits::cIsLValueReference<t_CType>
						, NMib::NTraits::TCAddLValueReference<CTypeQualifiers>
						, NMib::NTraits::TCRemoveReference<CTypeQualifiers>
					>
				>
			;

		};
	}

	template <typename t_CType, typename t_CTypeToCopyTo>
	mark_artificial mark_nodebug constexpr inline_always_debug decltype(auto) fg_ForwardAs(t_CTypeToCopyTo &&_ToForward) noexcept
	{
		return static_cast<typename NPrivate::TCForwardCopyEvalHelper<t_CType &&, t_CTypeToCopyTo>::CType>(_ToForward);
	}

	template <typename t_CType>
	mark_artificial mark_nodebug constexpr inline_always_debug NTraits::TCRemoveReference<t_CType> &&fg_Move(t_CType &&_ToMove) noexcept
		requires (!NTraits::cIsConst<NTraits::TCRemoveReference<t_CType>>) // Trying to move const value
	{
		return ((NTraits::TCRemoveReference<t_CType> &&)_ToMove);
	}

	template <typename t_CType>
	mark_artificial mark_nodebug constexpr inline_always_debug NTraits::TCRemoveReference<t_CType> fg_ExchangeMove(t_CType &&_ToMove) noexcept
		requires (!NTraits::cIsConst<NTraits::TCRemoveReference<t_CType>>) // Trying to move const value
	{
		return ((NTraits::TCRemoveReference<t_CType> &&)_ToMove);
	}

	template <typename t_CType>
	mark_artificial mark_nodebug constexpr inline_always_debug NTraits::TCRemoveReference<t_CType> &&fg_MoveAllowConst(t_CType &&_ToMove) noexcept
	{
		return ((NTraits::TCRemoveReference<t_CType> &&)_ToMove);
	}

	template <typename tf_CDestination, typename t_CSetTo>
	mark_artificial mark_nodebug constexpr inline_always_debug auto fg_Exchange(tf_CDestination &_Destination, t_CSetTo &&_SetTo)
	{
		auto Temp = fg_Move(_Destination);
		_Destination = fg_Forward<t_CSetTo>(_SetTo);
		return Temp;
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

	template <typename t_CType>
	mark_artificial mark_nodebug constexpr inline_always_debug t_CType volatile &fg_Volatile(t_CType &_In) noexcept
	{
		return (t_CType volatile &)_In;
	}

	template <typename t_CType>
	mark_artificial mark_nodebug constexpr inline_always_debug t_CType const &fg_Const(t_CType const&_In) noexcept
	{
		return (t_CType const &)_In;
	}

	template <typename t_CType, TCEnableIf<NTraits::cIsRValueReference<t_CType> || !NTraits::cIsReference<t_CType>> * = nullptr>
	mark_artificial mark_nodebug constexpr inline_always_debug decltype(auto) fg_ConstOrMove(t_CType &&_In) noexcept
	{
		return fg_Move(_In);
	}

	template <typename t_CType, TCEnableIf<!NTraits::cIsRValueReference<t_CType> && NTraits::cIsReference<t_CType>> * = nullptr>
	mark_artificial mark_nodebug constexpr inline_always_debug auto fg_ConstOrMove(t_CType &&_In) noexcept -> NTraits::TCRemoveReference<t_CType> const &
	{
		return static_cast<NTraits::TCRemoveReference<t_CType> const &>(_In);
	}

	template <typename t_CValue>
	struct TCMoveValueFunctor
	{
		mark_nodebug TCMoveValueFunctor(t_CValue &&_Value)
			: mp_Value(fg_Move(_Value))
		{
		}

		mark_artificial mark_nodebug inline_always t_CValue operator()()
		{
			return fg_Move(mp_Value);
		}

	private:
		t_CValue mp_Value;
	};

	template <typename t_CType>
	class TCCopy
	{
		t_CType const &m_Data;
	public:

		mark_nodebug TCCopy(t_CType const &_Data)
			: m_Data(_Data)
		{
		}

		mark_nodebug TCCopy(TCCopy const &_Other)
			: m_Data(_Other.m_Data)
		{
		}

		mark_nodebug TCCopy(TCCopy &&_Other)
			: m_Data(_Other.m_Data)
		{
		}

		mark_nodebug t_CType const &operator *() const
		{
			return m_Data;
		}
	};
	template <typename t_CType>
	mark_nodebug inline_always_debug TCCopy<t_CType> fg_Copy(t_CType const&_In)
	{
		return TCCopy<t_CType>(_In);
	}

	template <typename t_CType>
	class TCByValue
	{
		t_CType const &m_Data;
	public:

		mark_nodebug TCByValue(t_CType const &_Data)
			: m_Data(_Data)
		{
		}

		mark_nodebug TCByValue(TCByValue const &_Other)
			: m_Data(_Other.m_Data)
		{
		}

		mark_nodebug TCByValue(TCByValue &&_Other)
			: m_Data(_Other.m_Data)
		{
		}

		mark_nodebug t_CType const &operator *() const
		{
			return m_Data;
		}
	};
	template <typename t_CType>
	mark_nodebug inline_always_debug TCByValue<t_CType> fg_ByValue(t_CType const&_In)
	{
		return TCByValue<t_CType>(_In);
	}

	template <typename t_CType>
	class TCExplicit
	{
		t_CType m_Data;
	public:

		template <typename tf_CType>
		mark_nodebug TCExplicit(tf_CType &&_Data)
			: m_Data(fg_Forward<tf_CType>(_Data))
		{
		}

		mark_nodebug t_CType operator *()
		{
			return fg_Forward<t_CType>(m_Data);
		}
	};

	template <>
	class TCExplicit<void>
	{
	public:
	};

	template <typename tf_CType, TCEnableIf<NTraits::cIsReference<tf_CType>> * = nullptr>
	mark_nodebug inline_always_debug TCExplicit<tf_CType> fg_Explicit(tf_CType &&_In)
	{
		return TCExplicit<tf_CType>(_In);
	}

	template <typename tf_CType, TCEnableIf<!NTraits::cIsReference<tf_CType> && !NTraits::cIsConst<NTraits::TCRemoveReference<tf_CType>>> * = nullptr>
	mark_nodebug inline_always_debug TCExplicit<tf_CType> fg_Explicit(tf_CType &&_In)
	{
		return TCExplicit<tf_CType>(fg_Move(_In));
	}

	template <typename tf_CType, TCEnableIf<!NTraits::cIsReference<tf_CType> && NTraits::cIsConst<NTraits::TCRemoveReference<tf_CType>>> * = nullptr>
	mark_nodebug inline_always_debug TCExplicit<tf_CType> fg_Explicit(tf_CType &&_In)
	{
		return TCExplicit<tf_CType>(_In);
	}

	mark_nodebug inline_always_debug TCExplicit<void> fg_Explicit()
	{
		return TCExplicit<void>();
	}

	struct CExplicitHelper
	{
		template <typename tf_CType, TCEnableIf<NTraits::cIsReference<tf_CType>> * = nullptr>
		mark_nodebug inline_always_debug TCExplicit<tf_CType> operator = (tf_CType &&_In) const
		{
			return TCExplicit<tf_CType>(_In);
		}

		template <typename tf_CType, TCEnableIf<!NTraits::cIsReference<tf_CType>> * = nullptr>
		mark_nodebug inline_always_debug TCExplicit<tf_CType> operator = (tf_CType &&_In) const
		{
			return TCExplicit<tf_CType>(fg_Move(_In));
		}
	};

	extern CExplicitHelper const &g_Explicit;

	template <typename t_CType>
	class TCAttach
	{
		t_CType m_Data;
	public:

		template <typename tf_CType>
		mark_nodebug TCAttach(tf_CType &&_Data)
			: m_Data(fg_Forward<tf_CType>(_Data))
		{
		}

		mark_nodebug t_CType operator *()
		{
			return fg_Forward<t_CType>(m_Data);
		}
	};

	template <>
	class TCAttach<void>
	{
	public:
	};

	template <typename tf_CType, TCEnableIf<NTraits::cIsReference<tf_CType>> * = nullptr>
	mark_nodebug inline_always_debug TCAttach<tf_CType> fg_Attach(tf_CType &&_In)
	{
		return TCAttach<tf_CType>(_In);
	}

	template <typename tf_CType, TCEnableIf<!NTraits::cIsReference<tf_CType> && !NTraits::cIsConst<NTraits::TCRemoveReference<tf_CType>>> * = nullptr>
	mark_nodebug inline_always_debug TCAttach<tf_CType> fg_Attach(tf_CType &&_In)
	{
		return TCAttach<tf_CType>(fg_Move(_In));
	}

	template <typename tf_CType, TCEnableIf<!NTraits::cIsReference<tf_CType> && NTraits::cIsConst<NTraits::TCRemoveReference<tf_CType>>> * = nullptr>
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
		struct CDefault
		{
			template <typename tf_CReturn>
			mark_nodebug operator tf_CReturn()
			{
				return tf_CReturn();
			}
		};

		struct CIgnore
		{
			template <typename tf_CType>
			mark_nodebug inline_always_debug CIgnore &operator =(tf_CType &&_Value)
			{
				return *this;
			}
		};
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
		struct TCAutoStaticCast
		{
			t_CFrom m_From;

			template<typename ft_CFrom>
			mark_nodebug TCAutoStaticCast(ft_CFrom &&_From)
				: m_From(fg_Forward<ft_CFrom>(_From))
			{
			}

			template <typename ft_CTo>
			mark_nodebug operator ft_CTo()
			{
				return static_cast<ft_CTo>(m_From);
			}
		};

	}
	template <typename ft_CFrom>
	mark_nodebug NInternal::TCAutoStaticCast<ft_CFrom> fg_AutoStaticCast(ft_CFrom &&_From)
	{
		return NInternal::TCAutoStaticCast<ft_CFrom>(fg_Forward<ft_CFrom>(_From));
	}

	namespace NInternal
	{
		template <typename t_CFrom>
		struct TCAutoConstCast
		{
			t_CFrom m_From;

			template<typename ft_CFrom>
			mark_nodebug TCAutoConstCast(ft_CFrom &&_From)
				: m_From(fg_Forward<ft_CFrom>(_From))
			{
			}

			template <typename ft_CTo>
			mark_nodebug operator ft_CTo()
			{
				return const_cast<ft_CTo>(m_From);
			}
		};

	}
	template <typename ft_CFrom>
	mark_nodebug NInternal::TCAutoConstCast<ft_CFrom> fg_AutoConstCast(ft_CFrom &&_From)
	{
		return NInternal::TCAutoConstCast<ft_CFrom>(fg_Forward<ft_CFrom>(_From));
	}


	namespace NInternal
	{
		template <typename t_CFrom>
		struct TCAutoReinterpretCast
		{
			t_CFrom m_From;

			template<typename ft_CFrom>
			mark_nodebug TCAutoReinterpretCast(ft_CFrom &&_From)
				: m_From(fg_Forward<ft_CFrom>(_From))
			{
			}

			template <typename ft_CTo>
			mark_nodebug operator ft_CTo()
			{
				return reinterpret_cast<ft_CTo>(m_From);
			}
		};

	}
	template <typename ft_CFrom>
	mark_nodebug NInternal::TCAutoReinterpretCast<ft_CFrom> fg_AutoReinterpretCast(ft_CFrom &&_From)
	{
		return NInternal::TCAutoReinterpretCast<ft_CFrom>(fg_Forward<ft_CFrom>(_From));
	}

	namespace NInternal
	{
		template <typename t_CFrom>
		struct TCAutoCCast
		{
			t_CFrom m_From;

			template<typename ft_CFrom>
			mark_nodebug TCAutoCCast(ft_CFrom &&_From)
				: m_From(fg_Forward<ft_CFrom>(_From))
			{
			}

			template <typename ft_CTo>
			mark_nodebug operator ft_CTo()
			{
				return (ft_CTo)(m_From);
			}
		};

	}
	template <typename ft_CFrom>
	mark_nodebug NInternal::TCAutoCCast<ft_CFrom> fg_AutoCCast(ft_CFrom &&_From)
	{
		return NInternal::TCAutoCCast<ft_CFrom>(fg_Forward<ft_CFrom>(_From));
	}

	template <bool t_bCopy, bool t_bMove>
	struct TCSupportCopyMove
	{
	};

	template <>
	struct TCSupportCopyMove<true, false>
	{
		TCSupportCopyMove() = default;
		TCSupportCopyMove(TCSupportCopyMove const &) = default;
		TCSupportCopyMove(TCSupportCopyMove &&) = delete;

		TCSupportCopyMove &operator =(TCSupportCopyMove const &) = default;
		TCSupportCopyMove &operator =(TCSupportCopyMove &&) = delete;
	};

	template <>
	struct TCSupportCopyMove<false, true>
	{
		TCSupportCopyMove() = default;
		TCSupportCopyMove(TCSupportCopyMove const &) = delete;
		TCSupportCopyMove(TCSupportCopyMove &&) = default;
		TCSupportCopyMove &operator =(TCSupportCopyMove const &) = delete;
		TCSupportCopyMove &operator =(TCSupportCopyMove &&) = default;
	};

	template <>
	struct TCSupportCopyMove<false, false>
	{
		TCSupportCopyMove() = default;
		TCSupportCopyMove(TCSupportCopyMove const &) = delete;
		TCSupportCopyMove(TCSupportCopyMove &&) = delete;
		TCSupportCopyMove &operator =(TCSupportCopyMove const &) = delete;
		TCSupportCopyMove &operator =(TCSupportCopyMove &&) = delete;
	};

	template <typename t_CType>
	mark_nodebug inline_always_debug t_CType &fg_RemoveQualifiers(t_CType &_In)
	{
		return _In;
	}

	template <typename t_CType>
	mark_nodebug inline_always_debug t_CType &fg_RemoveQualifiers(t_CType const &_In)
	{
		return const_cast<t_CType &>(_In);
	}

	template <typename t_CType>
	mark_nodebug inline_always_debug t_CType &fg_RemoveQualifiers(t_CType volatile &_In)
	{
		return const_cast<t_CType &>(_In);
	}

	template <typename t_CType>
	mark_nodebug inline_always_debug t_CType &fg_RemoveQualifiers(t_CType const volatile &_In)
	{
		return const_cast<t_CType &>(_In);
	}

	template <typename t_CType>
	mark_nodebug inline_always_debug t_CType &&fg_RemoveQualifiers(t_CType &&_In)
	{
		return fg_Move(_In);
	}

	template <typename t_CType>
	mark_nodebug inline_always_debug t_CType &&fg_RemoveQualifiers(t_CType const &&_In)
	{
		return fg_Move(const_cast<t_CType &>(_In));
	}

	template <typename t_CType>
	mark_nodebug inline_always_debug t_CType &&fg_RemoveQualifiers(t_CType volatile &&_In)
	{
		return fg_Move(const_cast<t_CType &>(_In));
	}

	template <typename t_CType>
	mark_nodebug inline_always_debug t_CType &&fg_RemoveQualifiers(t_CType const volatile &&_In)
	{
		return fg_Move(const_cast<t_CType &>(_In));
	}


	template <typename t_CType>
	class TCLambdaMover
	{
		t_CType m_Type;
	public:

		mark_nodebug TCLambdaMover(t_CType &&_Other)
			: m_Type(fg_Move(_Other))
		{
		}

		mark_nodebug TCLambdaMover(TCCopy<t_CType> &&_Other)
			: m_Type(*_Other)
		{
		}

		mark_nodebug TCLambdaMover(TCLambdaMover const &_Other)
			: m_Type(fg_Move(fg_RemoveQualifiers(_Other.m_Type)))
		{
		}

		mark_nodebug TCLambdaMover(TCLambdaMover &_Other)
			: m_Type(fg_Move(fg_RemoveQualifiers(_Other.m_Type)))
		{
		}

		mark_nodebug TCLambdaMover(TCLambdaMover &&_Other)
			: m_Type(fg_Move(_Other.m_Type))
		{
		}

		mark_nodebug t_CType &operator *() const
		{
			return fg_RemoveQualifiers(m_Type);
		}
	};

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

	template <typename t_CType>
	mark_nodebug inline_always_debug t_CType volatile const &fg_ConstVolatile(t_CType &_In)
	{
		return (t_CType volatile const &)_In;
	}

	template <typename t_CType>
	mark_nodebug static constexpr inline_small t_CType fg_PowerOfTwoMinusOne(uaint _Power)
	{
		return ((t_CType(1)) << (_Power - 1)) + (((t_CType(1)) << (_Power - 1)) - t_CType(1));
	}


	template <typename t_CType0, typename t_CType1>
	static inline_small void fg_Swap(t_CType0 &_Left, t_CType1 &_Right)
	{
		t_CType0 Temp = fg_Move(_Left);
		_Left = fg_Move(_Right);
		_Right = fg_Move(Temp);
	}


	template <typename t_CTo, typename t_CFrom>
	constexpr static inline_small t_CTo fg_BitCast(t_CFrom const &_From)
	{
		return std::bit_cast<t_CTo>(_From);
	}

	template <typename t_CType>
	mark_nodebug inline_always_debug t_CType *fg_NullPtr()
	{
		return (t_CType *)nullptr;
	}

	template <typename t_CType>
	class TCHelper_ByteSwap
	{
	public:
		using CType = t_CType;
		enum
		{
			EDefaultImplementation = 1
		};
		static t_CType fs_Swap(t_CType const& _Data)
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
	};

	#ifdef DMibPCanDo_uint8
	template <>
	class TCHelper_ByteSwap<uint8>
	{
	public:
		using CType = uint8;
		enum
		{
			EDefaultImplementation = 0
		};
		static inline_small uint8 fs_Swap(uint8 _Data)
		{
			return _Data;
		}
	};
	#endif

#define DMibPDefaultSwap_uint16(x) ((((uint16)(x) & 0xff00) >> 8) | \
                             (((uint16)(x) & 0x00ff) << 8))

#define DMibPDefaultSwap_uint32(x) ((((uint32)(x) & 0xff000000) >> 24) | \
                             (((uint32)(x) & 0x00ff0000) >>  8) | \
                             (((uint32)(x) & 0x0000ff00) <<  8) | \
                             (((uint32)(x) & 0x000000ff) << 24))

#define DMibPDefaultSwap_uint64(x) ((((uint64)(x) & 0xff00000000000000ULL) >> 56) | \
                             (((uint64)(x) & 0x00ff000000000000ULL) >> 40) | \
                             (((uint64)(x) & 0x0000ff0000000000ULL) >> 24) | \
                             (((uint64)(x) & 0x000000ff00000000ULL) >>  8) | \
                             (((uint64)(x) & 0x00000000ff000000ULL) <<  8) | \
                             (((uint64)(x) & 0x0000000000ff0000ULL) << 24) | \
                             (((uint64)(x) & 0x000000000000ff00ULL) << 40) | \
                             (((uint64)(x) & 0x00000000000000ffULL) << 56))

	#ifdef DMibPCanDo_uint16
	template <>
	class TCHelper_ByteSwap<uint16>
	{
	public:
		using CType = uint16;
		enum
		{
			EDefaultImplementation = 0
		};
		static inline_small uint16 fs_Swap(uint16 _Data)
		{
			#ifdef DMibPByteSwap16
			return DMibPByteSwap16(_Data);
			#else
			return DMibPDefaultSwap_uint16(_Data);
			#endif
		}
	};
	#endif
	#ifdef DMibPCanDo_uint32
	template <>
	class TCHelper_ByteSwap<uint32>
	{
	public:
		using CType = uint32;
		enum
		{
			EDefaultImplementation = 0
		};
		static inline_small uint32 fs_Swap(uint32 _Data)
		{
			#ifdef DMibPByteSwap32
			return DMibPByteSwap32(_Data);
			#else
			return DMibPDefaultSwap_uint32(_Data);
			#endif
		}
	};
	#endif
	#ifdef DMibPCanDo_uint64
	template <>
	class TCHelper_ByteSwap<uint64>
	{
	public:
		using CType = uint64;
		enum
		{
			EDefaultImplementation = 0
		};
		static inline_small uint64 fs_Swap(uint64 _Data)
		{
			#ifdef DMibPByteSwap64
			return DMibPByteSwap64(_Data);
			#else
			return DMibPDefaultSwap_uint64(_Data);
			#endif
		}
	};
	#endif

	template <umint t_nBytes, typename t_CType>
	class TCHelper_ByteSwapChooseBySize
	{
	public:
		using CSwapper = TCHelper_ByteSwap<t_CType>;
	};

	template <typename t_CType>
	class TCHelper_ByteSwapChooseBySize<1, t_CType>
	{
	public:
		using CSwapper = TCHelper_ByteSwap<uint8>;
	};

	template <typename t_CType>
	class TCHelper_ByteSwapChooseBySize<2, t_CType>
	{
	public:
		using CSwapper = TCHelper_ByteSwap<uint16>;
	};

	template <typename t_CType>
	class TCHelper_ByteSwapChooseBySize<4, t_CType>
	{
	public:
		using CSwapper = TCHelper_ByteSwap<uint32>;
	};

	template <typename t_CType>
	class TCHelper_ByteSwapChooseBySize<8, t_CType>
	{
	public:
		using CSwapper = TCHelper_ByteSwap<uint64>;
	};

	template <typename t_CInt>
	inline_small t_CInt fg_ByteSwap(t_CInt _In)
	{
		// Static branch
		if constexpr (TCHelper_ByteSwap<t_CInt>::EDefaultImplementation)
		{
			using CSwapper = typename TCHelper_ByteSwapChooseBySize<sizeof(t_CInt), t_CInt>::CSwapper;
			return CSwapper::fs_Swap((typename CSwapper::CType &)_In);
		}
		else
			return TCHelper_ByteSwap<t_CInt>::fs_Swap(_In);
	}

	template <typename t_CInt>
	inline_small void fg_ByteSwap(t_CInt *_pIn, umint _Len)
	{
		for (umint i = 0; i < _Len; ++i)
		{
			_pIn[i] = fg_ByteSwap(_pIn[i]);
		}
	}

	template <typename t_CInt>
	inline_small t_CInt fg_ByteSwapLE(t_CInt _In)
	{
		#ifdef DMibPLittleEndian
		return _In;
		#else
		return fg_ByteSwap(_In);
		#endif
	}

	template <typename t_CInt>
	inline_small t_CInt fg_ByteSwapBE(t_CInt _In)
	{
		#ifdef DMibPLittleEndian
		return fg_ByteSwap(_In);
		#else
		return _In;
		#endif
	}


	template <typename tf_CFirst, typename tf_CSecond>
	constexpr inline_small NTraits::TCRemoveReference<tf_CFirst> fg_Min(tf_CFirst &&_First, tf_CSecond &&_Second)
	{
		if (fg_Forward<tf_CFirst>(_First) < fg_Forward<tf_CSecond>(_Second))
			return fg_Forward<tf_CFirst>(_First);
		else
			return fg_Forward<tf_CSecond>(_Second);
	}

	template <typename tf_CFirst, typename tf_CSecond, typename ...tfp_CRest>
	constexpr inline_small NTraits::TCRemoveReference<tf_CFirst> fg_Min(tf_CFirst &&_First, tf_CSecond &&_Second, tfp_CRest &&...p_Rest)
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
	constexpr inline_small NTraits::TCRemoveReference<tf_CFirst> fg_Max(tf_CFirst &&_First, tf_CSecond &&_Second, tfp_CRest &&...p_Rest)
	{
		if (fg_Forward<tf_CSecond>(_Second) < fg_Forward<tf_CFirst>(_First))
			return fg_Max(fg_Forward<tf_CFirst>(_First), fg_Forward<tfp_CRest>(p_Rest)...);
		else
			return fg_Max(fg_Forward<tf_CSecond>(_Second), fg_Forward<tfp_CRest>(p_Rest)...);
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
	constexpr inline_small NTraits::TCRemoveReference<tf_CFirst> fg_MaxValidFloat(tf_CFirst &&_First, tf_CSecond &&_Second, tfp_CRest &&...p_Rest)
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
	constexpr inline_small NTraits::TCRemoveReference<tf_CFirst> fg_MinValidFloat(tf_CFirst &&_First, tf_CSecond &&_Second, tfp_CRest &&...p_Rest)
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
	constexpr inline_small tf_CFirst fg_MinConstexpr(tf_CFirst _First, tf_CSecond _Second, tfp_CRest &&...p_Rest)
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
	constexpr inline_small tf_CFirst fg_MaxConstexpr(tf_CFirst _First, tf_CSecond _Second, tfp_CRest &&...p_Rest)
	{
		return fg_MaxConstexpr((_Second < _First) ? _First : _Second, p_Rest...);
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


	template <typename t_ToAlign>
	constexpr inline_small t_ToAlign fg_AlignUpConstExpr(t_ToAlign _pMem, umint _Alignment)
	{
		using CIntegerType = NTraits::TCUnsigned<NTraits::TCIntFromSizeLarger<sizeof(t_ToAlign)>>;
		return (t_ToAlign)((((CIntegerType)_pMem) + CIntegerType(_Alignment - 1)) & (~(CIntegerType(_Alignment) - CIntegerType(1))));
	}

	template <typename t_ToAlign>
	constexpr inline_small t_ToAlign fg_AlignDownConstExpr(t_ToAlign _pMem, umint _Alignment)
	{
		using CIntegerType = NTraits::TCUnsigned<NTraits::TCIntFromSizeLarger<sizeof(t_ToAlign)>>;
		return (t_ToAlign)(((CIntegerType)_pMem) & (~(CIntegerType(_Alignment) - CIntegerType(1))));
	}

	template <typename t_ToAlign>
	inline_small t_ToAlign fg_AlignUp(t_ToAlign _pMem, umint _Alignment)
	{
		using CIntegerType = NTraits::TCUnsigned<NTraits::TCIntFromSizeLarger<sizeof(t_ToAlign)>>;
		DMibFastCheck(_Alignment > 0 || ((CIntegerType)_pMem) == 0);
		return (t_ToAlign)((((CIntegerType)_pMem) + CIntegerType(_Alignment - 1)) & (~(CIntegerType(_Alignment) - CIntegerType(1))));
	}

	template <typename t_ToAlign>
	inline_small t_ToAlign fg_AlignDown(t_ToAlign _pMem, umint _Alignment)
	{
		using CIntegerType = NTraits::TCUnsigned<NTraits::TCIntFromSizeLarger<sizeof(t_ToAlign)>>;
		DMibFastCheck(_Alignment > 0 || ((CIntegerType)_pMem) == 0);
		return (t_ToAlign)(((CIntegerType)_pMem) & (~(CIntegerType(_Alignment) - CIntegerType(1))));
	}

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

	template <typename t_CType>
	inline_small t_CType fg_HighPart(t_CType _Data)
	{
		return _Data >> (sizeof(_Data)*4);
	}

	template <typename t_CType>
	inline_small t_CType fg_LowPart(t_CType _Data)
	{
		return _Data & ((1 << sizeof(_Data)*4)-1);
	}

	template <typename tf_CType>
	TCEnableIf
	<
		!NTraits::cIsVoid<tf_CType>
		&& !NTraits::cIsArray<tf_CType>
		&& !NTraits::cIsPointer<tf_CType>
		, void
	>
	fg_CallDestructor(tf_CType &_Type)
	{
		_Type.~tf_CType();
	}

	template <typename tf_CType>
	TCEnableIf
	<
		NTraits::cIsVoid<tf_CType>
		|| NTraits::cIsPointer<tf_CType>
		, void
	>
	fg_CallDestructor(tf_CType &_Type)
	{
	}

	template <typename tf_CType>
	TCEnableIf
	<
		NTraits::cIsArray<tf_CType>
		, void
	>
	fg_CallDestructor(tf_CType &_Type)
	{
		aint iElement = NTraits::gc_ArrayExtent<tf_CType> - 1;
		for (; iElement >= 0; --iElement)
		{
			fg_CallDestructor(_Type[iElement]);
		}
	}

	struct CSort_Default
	{
		template <typename t_CKey0, typename t_CKey1>
		constexpr inline_small static auto operator() (t_CKey0 &&_Left, t_CKey1 &&_Right) noexcept(noexcept(_Left <=> _Right))
		{
			return _Left <=> _Right;
		}
	};

	namespace NPrivate
	{
#ifdef DCompiler_MSVC
	#pragma warning(push)
	#pragma warning(disable:4307)
	#pragma warning(disable:4309)
#endif

		template <typename t_CIntType, bool t_bSigned>
		struct TCLimitsIntHelper
		{
			constexpr static t_CIntType mc_Min = 0;
			constexpr static t_CIntType mc_Max = t_CIntType(0) - 1;
			constexpr static t_CIntType mc_AllBits = mc_Max;
		};


		template <typename t_CIntType>
		struct TCLimitsIntHelper<t_CIntType, 1>
		{
			using CUnsigned = NMib::NTraits::TCUnsigned<t_CIntType>;

			constexpr static t_CIntType mc_Min = (t_CIntType(1) << ((sizeof(t_CIntType)*8)-1));
			constexpr static t_CIntType mc_Max = t_CIntType((CUnsigned(1) << ((sizeof(t_CIntType)*8)-1)) - CUnsigned(1));
			constexpr static t_CIntType mc_AllBits = t_CIntType(CUnsigned(0) - 1);
		};

#ifdef DCompiler_MSVC
	#pragma warning(pop)
#endif
	}

	template <typename t_CIntType>
	class TCLimitsInt : public NMib::NPrivate::TCLimitsIntHelper<t_CIntType, NTraits::cIsSigned<t_CIntType>>
	{
	public:
	};

	struct CAutoLimitMin
	{
		template <typename tf_CType>
		operator tf_CType ()
		{
			return TCLimitsInt<tf_CType>::mc_Min;
		}
	};

	struct CAutoLimitMax
	{
		template <typename tf_CType>
		operator tf_CType ()
		{
			return TCLimitsInt<tf_CType>::mc_Max;
		}
	};

	inline_always static CAutoLimitMin fg_LimitsMin()
	{
		return CAutoLimitMin();
	}

	inline_always static CAutoLimitMax fg_LimitsMax()
	{
		return CAutoLimitMax();
	}

#if defined(DCompiler_clang) || defined(DCompiler_gcc)
	template <typename t_CInt>
	constexpr inline_small bool fg_MultiplyOverflow(t_CInt _Left, t_CInt _Right, t_CInt &o_Result) noexcept
		requires (NTraits::cIsFundamental<t_CInt> && NTraits::cIsInteger<t_CInt> && !NTraits::cIsSame<NTraits::TCRemoveQualifiers<t_CInt>, bool>)
	{
		return __builtin_mul_overflow(_Left, _Right, &o_Result);
	}
#endif

	template <typename t_CInt>
	constexpr inline_small bool fg_MultiplyOverflow(t_CInt _Left, t_CInt _Right, t_CInt &o_Result) noexcept
		requires
		(
			NTraits::cIsInteger<t_CInt>
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
			&& !NTraits::cIsFundamental<t_CInt>
#endif
			&& !NTraits::cIsSame<NTraits::TCRemoveQualifiers<t_CInt>, bool>
		)
	{
		if constexpr (NTraits::cIsSigned<t_CInt>)
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
					if (_Left > TCLimitsInt<t_CInt>::mc_Max / _Right)
						return true;
				}
				else if (_Right < TCLimitsInt<t_CInt>::mc_Min / _Left)
					return true;
			}
			else
			{
				if (_Right > 0)
				{
					if (_Left < TCLimitsInt<t_CInt>::mc_Min / _Right)
						return true;
				}
				else if (_Right < TCLimitsInt<t_CInt>::mc_Max / _Left)
					return true;
			}
		}
		else
		{
			if (_Right != 0 && _Left > TCLimitsInt<t_CInt>::mc_Max / _Right)
				return true;
		}

		o_Result = _Left * _Right;
		return false;
	}

	template <typename t_CInt0, typename t_CInt1>
	bool fg_SafeLargerThan(t_CInt0 const &_Left, t_CInt1 const &_Right)
	{
		if constexpr (sizeof(t_CInt0) > sizeof(t_CInt1))
		{
			if constexpr (NTraits::cIsSigned<t_CInt0>)
			{
				if constexpr (NTraits::cIsSigned<t_CInt1>)
					return _Left > t_CInt0(_Right);
				else
				{
					t_CInt0 Zero(0);
					if (_Left < Zero)
						return false;
					return _Left > t_CInt0(_Right);
				}
			}
			else
			{
				if constexpr (NTraits::cIsSigned<t_CInt1>)
				{
					t_CInt1 Zero(0);
					if (_Right < Zero)
						return true;
					return _Left > t_CInt0(_Right);
				}
				else
					return _Left > t_CInt0(_Right);
			}
		}
		else if constexpr (sizeof(t_CInt0) < sizeof(t_CInt1))
		{
			if constexpr (NTraits::cIsSigned<t_CInt0>)
			{
				if constexpr (NTraits::cIsSigned<t_CInt1>)
					return t_CInt1(_Left) > _Right;
				else
				{
					t_CInt0 Zero(0);
					if (_Left < Zero)
						return false;
					return t_CInt1(_Left) > _Right;
				}
			}
			else
			{
				if constexpr (NTraits::cIsSigned<t_CInt1>)
				{
					t_CInt1 Zero(0);
					if (_Right < Zero)
						return true;
					return t_CInt1(_Left) > _Right;
				}
				else
					return t_CInt1(_Left) > _Right;
			}
		}
		else
		{
			if constexpr (NTraits::cIsSigned<t_CInt0>)
			{
				if constexpr (NTraits::cIsSigned<t_CInt1>)
					return _Left > t_CInt0(_Right);
				else
				{
					t_CInt0 Zero(0);
					if (_Left < Zero)
						return false;
					return t_CInt1(_Left) > _Right;
				}
			}
			else
			{
				if constexpr (NTraits::cIsSigned<t_CInt1>)
				{
					t_CInt1 Zero(0);
					if (_Right < Zero)
						return true;
					return _Left > t_CInt0(_Right);
				}
				else
					return _Left > t_CInt0(_Right);
			}
		}
	}


	namespace NStr
	{
		namespace NPrivate
		{
			template <typename t_CFormatter, typename t_CData>
			struct TCDetermineStringFormatterReturnType;
		}

		template <typename t_CFormatter, typename t_CData>
		inline_small typename NPrivate::TCDetermineStringFormatterReturnType<t_CFormatter, t_CData>::CType fg_CreateStringFormatter(t_CFormatter &_Formatter, t_CData const &_Data);

	}
	// Auto clear int

	template <typename t_CInt, t_CInt t_ClearVal = 0>
	class TCAutoClearInt
	{
		static t_CInt const &fsp_GetType();
	public:
		TCAutoClearInt()
		{
			m_Int = t_ClearVal;
		}
		TCAutoClearInt(const t_CInt &_Int)
		{
			m_Int = _Int;
		}
		t_CInt m_Int;

		operator t_CInt & ()
		{
			return m_Int;
		}

		operator const t_CInt & () const
		{
			return m_Int;
		}

		t_CInt & f_Get()
		{
			return m_Int;
		}

		const t_CInt &f_Get() const
		{
			return m_Int;
		}

		TCAutoClearInt &operator = (const TCAutoClearInt &_Other)
		{
			m_Int = _Other.m_Int;
			return *this;
		}

		TCAutoClearInt &operator = (const t_CInt &_Other)
		{
			m_Int = _Other;
			return *this;
		}

		template <typename t_CFormatter>
		int f_GetStringFormatType(t_CFormatter &_Formatter);

		template <typename t_CFormatter>
		auto f_CreateStringFormatter(t_CFormatter &_Formatter) const -> decltype(NStr::fg_CreateStringFormatter(_Formatter, m_Int))
		{
			return NStr::fg_CreateStringFormatter(_Formatter, m_Int);
		}

		template <typename t_CStream>
		void f_Feed(t_CStream &_Stream) const
		{
			_Stream << m_Int;
		}

		template <typename t_CStream>
		void f_Consume(t_CStream &_Stream)
		{
			_Stream >> m_Int;
		}

		auto operator <=> (TCAutoClearInt const &_Right) const noexcept(noexcept(f_Get() <=> _Right.f_Get()))
		{
			return f_Get() <=> _Right.f_Get();
		}

		bool operator == (TCAutoClearInt const &_Right) const noexcept(noexcept(f_Get() == _Right.f_Get()))
		{
			return f_Get() == _Right.f_Get();
		}

		template <typename tf_CRight>
		auto operator <=> (tf_CRight const &_Right) const noexcept(noexcept(f_Get() <=> _Right))
		{
			return f_Get() <=> _Right;
		}

		template <typename tf_CRight, tf_CRight tf_RightValue>
		auto operator <=> (TCAutoClearInt<tf_CRight, tf_RightValue> const &_Right) const noexcept(noexcept(f_Get() <=> _Right.f_Get()))
		{
			return f_Get() <=> _Right.f_Get();
		}

		template <typename tf_CRight>
		bool operator == (tf_CRight const &_Right) const noexcept(noexcept(f_Get() == _Right))
		{
			return f_Get() == _Right;
		}

		template <typename tf_CRight, tf_CRight tf_RightValue>
		bool operator == (TCAutoClearInt<tf_CRight, tf_RightValue> const &_Right) const noexcept(noexcept(f_Get() == _Right.f_Get()))
		{
			return f_Get() == _Right.f_Get();
		}
	};

	template <typename t_CType>
	class TCAutoClear
	{
	public:
		TCAutoClear() = default;
		TCAutoClear(TCAutoClear const &_Other) = default;
		TCAutoClear(TCAutoClear &&_Other) = default;

		TCAutoClear(t_CType const& _Value)
		{
			m_Value = _Value;
		}

		t_CType m_Value = 0;

		operator t_CType & ()
		{
			return m_Value;
		}

		operator const t_CType & () const
		{
			return m_Value;
		}

		t_CType &f_Get()
		{
			return m_Value;
		}

		const t_CType &f_Get() const
		{
			return m_Value;
		}

		TCAutoClear &operator = (TCAutoClear const &_Other) = default;
		TCAutoClear &operator = (TCAutoClear &&_Other) = default;

		TCAutoClear &operator = (t_CType const& _Other)
		{
			m_Value = _Other;
			return *this;
		}

		template <typename t_CFormatter>
		int f_GetStringFormatType(t_CFormatter &_Formatter);

		template <typename t_CFormatter>
		auto f_CreateStringFormatter(t_CFormatter &_Formatter) const -> decltype(NStr::fg_CreateStringFormatter(_Formatter, m_Value))
		{
			return NStr::fg_CreateStringFormatter(_Formatter, m_Value);
		}

		template <typename t_CStream>
		void f_Feed(t_CStream &_Stream) const
		{
			_Stream << m_Value;
		}

		template <typename t_CStream>
		void f_Consume(t_CStream &_Stream)
		{
			_Stream >> m_Value;
		}

		auto operator <=> (TCAutoClear const &_Right) const noexcept(noexcept(f_Get() <=> _Right.f_Get()))
		{
			return f_Get() <=> _Right.f_Get();
		}

		bool operator == (TCAutoClear const &_Right) const noexcept(noexcept(f_Get() == _Right.f_Get()))
		{
			return f_Get() == _Right.f_Get();
		}

		template <typename tf_CRight>
		auto operator <=> (tf_CRight const &_Right) const noexcept(noexcept(f_Get() <=> _Right))
		{
			return f_Get() <=> _Right;
		}

		template <typename tf_CRight>
		auto operator <=> (TCAutoClear<tf_CRight> const &_Right) const noexcept(noexcept(f_Get() <=> _Right.f_Get()))
		{
			return f_Get() <=> _Right.f_Get();
		}

		template <typename tf_CRight>
		bool operator == (tf_CRight const &_Right) const noexcept(noexcept(f_Get() == _Right))
		{
			return f_Get() == _Right;
		}

		template <typename tf_CRight>
		bool operator == (TCAutoClear<tf_CRight> const &_Right) const noexcept(noexcept(f_Get() == _Right.f_Get()))
		{
			return f_Get() == _Right.f_Get();
		}

		template <typename tf_CRight, tf_CRight tf_RightValue>
		auto operator <=> (TCAutoClearInt<tf_CRight, tf_RightValue> const &_Right) const noexcept(noexcept(f_Get() <=> _Right.f_Get()))
		{
			return f_Get() <=> _Right.f_Get();
		}

		template <typename tf_CRight, tf_CRight tf_RightValue>
		bool operator == (TCAutoClearInt<tf_CRight, tf_RightValue> const &_Right) const noexcept(noexcept(f_Get() == _Right.f_Get()))
		{
			return f_Get() == _Right.f_Get();
		}
	};

	template <typename t_CType, t_CType t_Argument0, t_CType t_Argument1>
	constexpr inline t_CType gc_ConstantMax = (t_Argument0 > t_Argument1 ? t_Argument0 : t_Argument1);

	template <typename t_CType, t_CType t_Argument0, t_CType t_Argument1>
	constexpr inline t_CType gc_ConstantMin = (t_Argument0 < t_Argument1 ? t_Argument0 : t_Argument1);

	template <typename t_CType, t_CType t_Argument0>
	constexpr inline t_CType gc_ConstantAbs = (t_Argument0 < t_CType(0) ? (t_CType(0) - t_Argument0) : t_Argument0);

	template <typename t_CAny>
	static t_CAny fg_MakeSymbolActive(t_CAny &&_Other)
	{
		return _Other;
	}

	template <typename tf_CType>
	ch8 const *fg_GetTypeName();

	struct CConstExprSubStr
	{
		constexpr CConstExprSubStr(char const *_pString, umint _Len)
			: m_pString(_pString)
			, m_Len(_Len)
		{
		}

		char const *m_pString;
		umint m_Len;
	};

	template <typename tf_CType>
	static consteval CConstExprSubStr fg_GetTypeNameConstExpr();

	template <umint t_nCharacters>
	struct TCConstExprSubStr : public CConstExprSubStr
	{
		constexpr TCConstExprSubStr(char const *_pString)
			: CConstExprSubStr(m_String, t_nCharacters)
		{
			for (umint i = 0; i < t_nCharacters; ++i)
				m_String[i] = _pString[i];
			m_String[t_nCharacters] = 0;
		}

		char m_String[t_nCharacters + 1];
	};

	template <typename tf_CType>
	static constexpr auto fg_GetTypeNameConstExprArray()
	{
		constexpr auto String = fg_GetTypeNameConstExpr<tf_CType>();
		return TCConstExprSubStr<String.m_Len>{String.m_pString};
	}

	static constexpr uint32 fg_JenkinsHash(const char * const _pString);
	static constexpr uint32 fg_JenkinsHash(const char * const _pString, umint _Len, char _ExtraChar);

	static consteval void fg_ParseTypeIdentifierConstexpr(char const *&_pParse);
	static consteval void fg_ParseUntilCallingConvention(char const *&_pParse);

	template <auto tf_pMemberPointer>
	static consteval CConstExprSubStr fg_GetMemberPointerNameConstExpr();

#ifdef DCompiler_MSVC_Workaround
#	define DMibSupportMemberNameFromMemberPointer 0
#else
#	define DMibSupportMemberNameFromMemberPointer 1
#endif

#if DMibSupportMemberNameFromMemberPointer

	template <auto tf_pMemberFunction>
	static consteval uint32 fg_GetMemberFunctionHash();

#define DMibPointerToMemberFunctionForHash(d_FunctionName) d_FunctionName
#define DMibIfNotSupportMemberNameFromMemberPointer(...)

#else

	static consteval uint32 fg_GetMemberFunctionNameHash(const char * const _pFunctionName);

	template <auto tf_pMemberFunction>
	static consteval uint32 fg_GetMemberFunctionHash(uint32 _NameHash);

#define DMibPointerToMemberFunctionForHash(d_FunctionName) d_FunctionName, fg_GetMemberFunctionNameHash(DMibStringize(d_FunctionName))
#define DMibIfNotSupportMemberNameFromMemberPointer(...) __VA_ARGS__

#endif

	template <typename tf_CClass>
	static consteval uint32 fg_GetMemberFunctionHash(const char * const _pFunctionName);

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
#define DMibGetParent(_Class, _Member, _Ptr) ((_Class *)(((uint8 *)_Ptr) + ( ((umint)((_Class *)((void*)_Ptr))) - ((umint)(&((_Class *)((void *)_Ptr))->_Member)) )))

#ifndef DMibPNoShortCuts
#	define DGetParent(_Class, _Member, _Ptr) DMibGetParent(_Class, _Member, _Ptr)
#	define DNew DMibNew
#endif
