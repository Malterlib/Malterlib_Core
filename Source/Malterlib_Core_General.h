// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

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
		typedef decltype(nullptr) CNullPtr;
	};

	typedef CHideNull::CNullPtr CNullPtr;


#	define DMibStaticCheck( _Expression ) static_assert(_Expression, "Static assert failed: " DMibStringize(_Expression))
		
#	ifndef DMibPNoShortCuts
#		define DStaticCheck DMibStaticCheck
#	endif

#if defined(DCompiler_MSVC) && defined(DCompiler_MSVC_EDG)

	namespace NPrivate
	{
		template <typename t_CType> 
		struct TCForwardHelper
		{
		    typedef t_CType CType;

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
	mark_artificial constexpr inline_always_debug t_CType&& fg_Forward(typename NTraits::TCRemoveReference<t_CType>::CType &_ToForward)
	{
		return static_cast<t_CType&&>(_ToForward);
	}

	template <typename t_CType> 
	mark_artificial constexpr inline_always_debug t_CType&& fg_Forward(typename NTraits::TCRemoveReference<t_CType>::CType &&_ToForward) noexcept
	{
		static_assert(!NTraits::TCIsLValueReference<t_CType>::mc_Value, "Cannot be a lvalue reference here");
		return static_cast<t_CType&&>(_ToForward);
	}
#endif

	namespace NPrivate
	{
		template <typename t_CType, typename t_CTypeToCopyTo>
		struct TCForwardCopyEvalHelper
		{
			typedef typename NMib::NTraits::TCCopyQualifiers
				<
					typename NMib::NTraits::TCRemoveReference<t_CType>::CType
					, typename NMib::NTraits::TCRemoveReference<t_CTypeToCopyTo>::CType
				>::CType CTypeQualifiers
			;
			
		    typedef typename TCChooseType
				<
					NMib::NTraits::TCIsRValueReference<t_CType>::mc_Value
					, typename NMib::NTraits::TCAddRValueReference<CTypeQualifiers>::CType
					, typename TCChooseType
					<
						NMib::NTraits::TCIsLValueReference<t_CType>::mc_Value
						, typename NMib::NTraits::TCAddLValueReference<CTypeQualifiers>::CType
						, typename NMib::NTraits::TCRemoveReference<CTypeQualifiers>::CType
					>::CType
				>
				::CType CType;
			;
			
		};
	}
	
	template <typename t_CType, typename t_CTypeToCopyTo> 
	mark_artificial inline_always_debug decltype(auto) fg_ForwardAs(t_CTypeToCopyTo &&_ToForward)
	{
		return static_cast<typename NPrivate::TCForwardCopyEvalHelper<t_CType &&, t_CTypeToCopyTo>::CType>(_ToForward);
	}
	
	template <typename t_CType>
	mark_artificial inline_always_debug typename NTraits::TCRemoveReference<t_CType>::CType &&fg_Move(t_CType &&_ToMove) noexcept
		requires (!NTraits::TCIsConst<typename NTraits::TCRemoveReference<t_CType>::CType>::mc_Value) // Trying to move const value
	{
		return ((typename NTraits::TCRemoveReference<t_CType>::CType &&)_ToMove);
	}

	template <typename t_CType>
	mark_artificial inline_always_debug typename NTraits::TCRemoveReference<t_CType>::CType fg_ExchangeMove(t_CType &&_ToMove) noexcept
		requires (!NTraits::TCIsConst<typename NTraits::TCRemoveReference<t_CType>::CType>::mc_Value) // Trying to move const value
	{
		return ((typename NTraits::TCRemoveReference<t_CType>::CType &&)_ToMove);
	}

	template <typename t_CType>
	mark_artificial inline_always_debug typename NTraits::TCRemoveReference<t_CType>::CType &&fg_MoveAllowConst(t_CType &&_ToMove) noexcept
	{
		return ((typename NTraits::TCRemoveReference<t_CType>::CType &&)_ToMove);
	}

	template <typename tf_CDestination, typename t_CSetTo>
	mark_artificial constexpr inline_always_debug auto fg_Exchange(tf_CDestination &_Destination, t_CSetTo &&_SetTo)
	{
		auto Temp = fg_Move(_Destination);
		_Destination = fg_Forward<t_CSetTo>(_SetTo);
		return Temp;
	}
	
	template <typename tf_CType>
	mark_artificial constexpr inline_always_debug tf_CType fg_TempCopy(tf_CType const &_Value)
	{
		return _Value;
	}

	template <typename tf_CToType, typename tf_CType>
	mark_artificial constexpr inline_always_debug decltype(auto) fg_CopyOrMove(tf_CType &&_Value)
	{
		using CToType = typename NTraits::TCRemoveReferenceAndQualifiers<tf_CToType>::CType;

		if constexpr (NTraits::TCIsSame<CToType, typename NTraits::TCRemoveReferenceAndQualifiers<tf_CType>::CType>::mc_Value)
		{
			if constexpr (NTraits::TCIsRValueReference<tf_CType &&>::mc_Value)
				return (fg_Forward<tf_CType>(_Value));
			else
				return CToType(fg_Forward<tf_CType>(_Value));
		}
		else
			return CToType(fg_Forward<tf_CType>(_Value));
	}

	template <typename t_CType>
	mark_artificial constexpr inline_always_debug t_CType volatile &fg_Volatile(t_CType &_In)
	{
		return (t_CType volatile &)_In;
	}

	template <typename t_CType>
	mark_artificial constexpr inline_always_debug t_CType const &fg_Const(t_CType const&_In)
	{
		return (t_CType const &)_In;
	}

	template <typename t_CType, TCEnableIfType<NTraits::TCIsRValueReference<t_CType>::mc_Value || !NTraits::TCIsReference<t_CType>::mc_Value> * = nullptr>
	mark_artificial constexpr inline_always_debug decltype(auto) fg_ConstOrMove(t_CType &&_In)
	{
		return fg_Move(_In);
	}

	template <typename t_CType, TCEnableIfType<!NTraits::TCIsRValueReference<t_CType>::mc_Value && NTraits::TCIsReference<t_CType>::mc_Value> * = nullptr>
	mark_artificial constexpr inline_always_debug auto fg_ConstOrMove(t_CType &&_In) -> typename NTraits::TCRemoveReference<t_CType>::CType const &
	{
		return static_cast<typename NTraits::TCRemoveReference<t_CType>::CType const &>(_In);
	}

	template <typename t_CValue>
	struct TCMoveValueFunctor
	{
		TCMoveValueFunctor(t_CValue &&_Value)
			: mp_Value(fg_Move(_Value))
		{
		}

		mark_artificial inline_always t_CValue operator()()
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

		TCCopy(t_CType const &_Data)
			: m_Data(_Data)
		{
		}

		TCCopy(TCCopy const &_Other)
			: m_Data(_Other.m_Data)
		{
		}

		TCCopy(TCCopy &&_Other)
			: m_Data(_Other.m_Data)
		{
		}

		t_CType const &operator *() const
		{
			return m_Data;
		}
	};
	template <typename t_CType>
	inline_always_debug TCCopy<t_CType> fg_Copy(t_CType const&_In)
	{
		return TCCopy<t_CType>(_In);
	}

	template <typename t_CType>
	class TCByValue
	{
		t_CType const &m_Data;
	public:

		TCByValue(t_CType const &_Data)
			: m_Data(_Data)
		{
		}

		TCByValue(TCByValue const &_Other)
			: m_Data(_Other.m_Data)
		{
		}

		TCByValue(TCByValue &&_Other)
			: m_Data(_Other.m_Data)
		{
		}

		t_CType const &operator *() const
		{
			return m_Data;
		}
	};
	template <typename t_CType>
	inline_always_debug TCByValue<t_CType> fg_ByValue(t_CType const&_In)
	{
		return TCByValue<t_CType>(_In);
	}

	template <typename t_CType>
	class TCExplicit
	{
		t_CType m_Data;
	public:

		template <typename tf_CType>
		TCExplicit(tf_CType &&_Data)
			: m_Data(fg_Forward<tf_CType>(_Data))
		{
		}

		t_CType operator *()
		{
			return fg_Forward<t_CType>(m_Data);
		}
	};

	template <>
	class TCExplicit<void>
	{
	public:
	};
	
	template <typename tf_CType, TCEnableIfType<NTraits::TCIsReference<tf_CType>::mc_Value> * = nullptr>
	inline_always_debug TCExplicit<tf_CType> fg_Explicit(tf_CType &&_In)
	{
		return TCExplicit<tf_CType>(_In);
	}
	
	template <typename tf_CType, TCEnableIfType<!NTraits::TCIsReference<tf_CType>::mc_Value && !NTraits::TCIsConst<typename NTraits::TCRemoveReference<tf_CType>::CType>::mc_Value> * = nullptr>
	inline_always_debug TCExplicit<tf_CType> fg_Explicit(tf_CType &&_In)
	{
		return TCExplicit<tf_CType>(fg_Move(_In));
	}
	
	template <typename tf_CType, TCEnableIfType<!NTraits::TCIsReference<tf_CType>::mc_Value && NTraits::TCIsConst<typename NTraits::TCRemoveReference<tf_CType>::CType>::mc_Value> * = nullptr>
	inline_always_debug TCExplicit<tf_CType> fg_Explicit(tf_CType &&_In)
	{
		return TCExplicit<tf_CType>(_In);
	}
	
	inline_always_debug TCExplicit<void> fg_Explicit()
	{
		return TCExplicit<void>();
	}
	
	struct CExplicitHelper
	{
		template <typename tf_CType, TCEnableIfType<NTraits::TCIsReference<tf_CType>::mc_Value> * = nullptr>
		inline_always_debug TCExplicit<tf_CType> operator = (tf_CType &&_In) const
		{
			return TCExplicit<tf_CType>(_In);
		}
		
		template <typename tf_CType, TCEnableIfType<!NTraits::TCIsReference<tf_CType>::mc_Value> * = nullptr>
		inline_always_debug TCExplicit<tf_CType> operator = (tf_CType &&_In) const
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
		TCAttach(tf_CType &&_Data)
			: m_Data(fg_Forward<tf_CType>(_Data))
		{
		}

		t_CType operator *()
		{
			return fg_Forward<t_CType>(m_Data);
		}
	};

	template <>
	class TCAttach<void>
	{
	public:
	};

	template <typename tf_CType, TCEnableIfType<NTraits::TCIsReference<tf_CType>::mc_Value> * = nullptr>
	inline_always_debug TCAttach<tf_CType> fg_Attach(tf_CType &&_In)
	{
		return TCAttach<tf_CType>(_In);
	}

	template <typename tf_CType, TCEnableIfType<!NTraits::TCIsReference<tf_CType>::mc_Value && !NTraits::TCIsConst<typename NTraits::TCRemoveReference<tf_CType>::CType>::mc_Value> * = nullptr>
	inline_always_debug TCAttach<tf_CType> fg_Attach(tf_CType &&_In)
	{
		return TCAttach<tf_CType>(fg_Move(_In));
	}

	template <typename tf_CType, TCEnableIfType<!NTraits::TCIsReference<tf_CType>::mc_Value && NTraits::TCIsConst<typename NTraits::TCRemoveReference<tf_CType>::CType>::mc_Value> * = nullptr>
	inline_always_debug TCAttach<tf_CType> fg_Attach(tf_CType &&_In)
	{
		return TCAttach<tf_CType>(_In);
	}

	inline_always_debug TCAttach<void> fg_Attach()
	{
		return TCAttach<void>();
	}

	namespace NInternal
	{
		struct CDefault
		{
			template <typename tf_CReturn>
			operator tf_CReturn()
			{
				return tf_CReturn();
			}
		};

		struct CIgnore
		{
			template <typename tf_CType>
			inline_always_debug CIgnore &operator =(tf_CType &&_Value)
			{
				return *this;
			}
		};
	}

	inline_always_debug NInternal::CIgnore fg_Ignore()
	{
		return NInternal::CIgnore();
	}

	inline_always_debug NInternal::CDefault fg_Default()
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
			TCAutoStaticCast(ft_CFrom &&_From)
				: m_From(fg_Forward<ft_CFrom>(_From))
			{
			}

			template <typename ft_CTo>
			operator ft_CTo()
			{
				return static_cast<ft_CTo>(m_From);
			}
		};

	}
	template <typename ft_CFrom>
	NInternal::TCAutoStaticCast<ft_CFrom> fg_AutoStaticCast(ft_CFrom &&_From)
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
			TCAutoConstCast(ft_CFrom &&_From)
				: m_From(fg_Forward<ft_CFrom>(_From))
			{
			}

			template <typename ft_CTo>
			operator ft_CTo()
			{
				return const_cast<ft_CTo>(m_From);
			}
		};

	}
	template <typename ft_CFrom>
	NInternal::TCAutoConstCast<ft_CFrom> fg_AutoConstCast(ft_CFrom &&_From)
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
			TCAutoReinterpretCast(ft_CFrom &&_From)
				: m_From(fg_Forward<ft_CFrom>(_From))
			{
			}

			template <typename ft_CTo>
			operator ft_CTo()
			{
				return reinterpret_cast<ft_CTo>(m_From);
			}
		};

	}
	template <typename ft_CFrom>
	NInternal::TCAutoReinterpretCast<ft_CFrom> fg_AutoReinterpretCast(ft_CFrom &&_From)
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
			TCAutoCCast(ft_CFrom &&_From)
				: m_From(fg_Forward<ft_CFrom>(_From))
			{
			}

			template <typename ft_CTo>
			operator ft_CTo()
			{
				return (ft_CTo)(m_From);
			}
		};

	}
	template <typename ft_CFrom>
	NInternal::TCAutoCCast<ft_CFrom> fg_AutoCCast(ft_CFrom &&_From)
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
	inline_always_debug t_CType &fg_RemoveQualifiers(t_CType &_In)
	{
		return _In;
	}

	template <typename t_CType>
	inline_always_debug t_CType &fg_RemoveQualifiers(t_CType const &_In)
	{
		return const_cast<t_CType &>(_In);
	}

	template <typename t_CType>
	inline_always_debug t_CType &fg_RemoveQualifiers(t_CType volatile &_In)
	{
		return const_cast<t_CType &>(_In);
	}

	template <typename t_CType>
	inline_always_debug t_CType &fg_RemoveQualifiers(t_CType const volatile &_In)
	{
		return const_cast<t_CType &>(_In);
	}

	template <typename t_CType>
	inline_always_debug t_CType &&fg_RemoveQualifiers(t_CType &&_In)
	{
		return fg_Move(_In);
	}

	template <typename t_CType>
	inline_always_debug t_CType &&fg_RemoveQualifiers(t_CType const &&_In)
	{
		return fg_Move(const_cast<t_CType &>(_In));
	}

	template <typename t_CType>
	inline_always_debug t_CType &&fg_RemoveQualifiers(t_CType volatile &&_In)
	{
		return fg_Move(const_cast<t_CType &>(_In));
	}

	template <typename t_CType>
	inline_always_debug t_CType &&fg_RemoveQualifiers(t_CType const volatile &&_In)
	{
		return fg_Move(const_cast<t_CType &>(_In));
	}


	template <typename t_CType>
	class TCLambdaMover
	{
		t_CType m_Type;
	public:

		TCLambdaMover(t_CType &&_Other)
			: m_Type(fg_Move(_Other))
		{
		}

		TCLambdaMover(TCCopy<t_CType> &&_Other)
			: m_Type(*_Other)
		{
		}

		TCLambdaMover(TCLambdaMover const &_Other)
			: m_Type(fg_Move(fg_RemoveQualifiers(_Other.m_Type)))
		{
		}

		TCLambdaMover(TCLambdaMover &_Other)
			: m_Type(fg_Move(fg_RemoveQualifiers(_Other.m_Type)))
		{
		}

		TCLambdaMover(TCLambdaMover &&_Other)
			: m_Type(fg_Move(_Other.m_Type))
		{
		}

		t_CType &operator *() const
		{
			return fg_RemoveQualifiers(m_Type);
		}
	};

	template <typename tf_CType>
	TCLambdaMover<typename NTraits::TCRemoveQualifiers<typename NTraits::TCRemoveReference<tf_CType>::CType>::CType> fg_LambdaMove(tf_CType &&_Type)
	{
		return TCLambdaMover<typename NTraits::TCRemoveQualifiers<typename NTraits::TCRemoveReference<tf_CType>::CType>::CType>(fg_Move(_Type));
	}

	template <typename tf_CType>
	TCLambdaMover<typename NTraits::TCRemoveQualifiers<typename NTraits::TCRemoveReference<tf_CType>::CType>::CType> fg_LambdaMove(TCCopy<tf_CType> &&_Type)
	{
		return TCLambdaMover<typename NTraits::TCRemoveQualifiers<typename NTraits::TCRemoveReference<tf_CType>::CType>::CType>(fg_Move(_Type));
	}

	template <typename t_CType>
	inline_always_debug t_CType volatile const &fg_ConstVolatile(t_CType &_In)
	{
		return (t_CType volatile const &)_In;
	}

	template <typename t_CType>
	static constexpr inline_small t_CType fg_PowerOfTwoMinusOne(uaint _Power)
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


	template <typename t_CType>
	inline_always_debug t_CType *fg_NullPtr()
	{
		return (t_CType *)nullptr;
	}

	template <typename t_CType>
	class TCHelper_ByteSwap
	{
	public:
		typedef t_CType CType;
		enum
		{
			EDefaultImplementation = 1
		};
		static t_CType fs_Swap(t_CType const& _Data)
		{
			mint Size = sizeof(_Data);
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
		typedef uint8 CType;
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
		typedef uint16 CType;
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
		typedef uint32 CType;
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
		typedef uint64 CType;
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

	template <mint t_nBytes, typename t_CType>
	class TCHelper_ByteSwapChooseBySize
	{
	public:
		typedef TCHelper_ByteSwap<t_CType> CSwapper;
	};

	template <typename t_CType>
	class TCHelper_ByteSwapChooseBySize<1, t_CType>
	{
	public:
		typedef TCHelper_ByteSwap<uint8> CSwapper;
	};

	template <typename t_CType>
	class TCHelper_ByteSwapChooseBySize<2, t_CType>
	{
	public:
		typedef TCHelper_ByteSwap<uint16> CSwapper;
	};

	template <typename t_CType>
	class TCHelper_ByteSwapChooseBySize<4, t_CType>
	{
	public:
		typedef TCHelper_ByteSwap<uint32> CSwapper;
	};

	template <typename t_CType>
	class TCHelper_ByteSwapChooseBySize<8, t_CType>
	{
	public:
		typedef TCHelper_ByteSwap<uint64> CSwapper;
	};
	
	template <typename t_CInt>
	inline_small t_CInt fg_ByteSwap(t_CInt _In)
	{
		// Static branch
		if constexpr (TCHelper_ByteSwap<t_CInt>::EDefaultImplementation)
		{
			typedef typename TCHelper_ByteSwapChooseBySize<sizeof(t_CInt), t_CInt>::CSwapper CSwapper;
			return CSwapper::fs_Swap((typename CSwapper::CType &)_In);
		}
		else
			return TCHelper_ByteSwap<t_CInt>::fs_Swap(_In);
	}
		
	template <typename t_CInt>
	inline_small void fg_ByteSwap(t_CInt *_pIn, mint _Len)
	{
		for (mint i = 0; i < _Len; ++i)
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
	constexpr inline_small typename NTraits::TCRemoveReference<tf_CFirst>::CType fg_Min(tf_CFirst &&_First, tf_CSecond &&_Second)
	{
		if (fg_Forward<tf_CFirst>(_First) < fg_Forward<tf_CSecond>(_Second))
			return fg_Forward<tf_CFirst>(_First);
		else
			return fg_Forward<tf_CSecond>(_Second);
	}

	template <typename tf_CFirst, typename tf_CSecond, typename ...tfp_CRest>
	constexpr inline_small typename NTraits::TCRemoveReference<tf_CFirst>::CType fg_Min(tf_CFirst &&_First, tf_CSecond &&_Second, tfp_CRest &&...p_Rest)
	{
		if (fg_Forward<tf_CFirst>(_First) < fg_Forward<tf_CSecond>(_Second))
			return fg_Min(fg_Forward<tf_CFirst>(_First), fg_Forward<tfp_CRest>(p_Rest)...);
		else
			return fg_Min(fg_Forward<tf_CSecond>(_Second), fg_Forward<tfp_CRest>(p_Rest)...);
	}

	template <typename tf_CFirst, typename tf_CSecond>
	constexpr inline_small typename NTraits::TCRemoveReference<tf_CFirst>::CType fg_Max(tf_CFirst &&_First, tf_CSecond &&_Second)
	{
		if (fg_Forward<tf_CSecond>(_Second) < fg_Forward<tf_CFirst>(_First))
			return fg_Forward<tf_CFirst>(_First);
		else
			return fg_Forward<tf_CSecond>(_Second);
	}

	template <typename tf_CFirst, typename tf_CSecond, typename ...tfp_CRest>
	constexpr inline_small typename NTraits::TCRemoveReference<tf_CFirst>::CType fg_Max(tf_CFirst &&_First, tf_CSecond &&_Second, tfp_CRest &&...p_Rest)
	{
		if (fg_Forward<tf_CSecond>(_Second) < fg_Forward<tf_CFirst>(_First))
			return fg_Max(fg_Forward<tf_CFirst>(_First), fg_Forward<tfp_CRest>(p_Rest)...);
		else
			return fg_Max(fg_Forward<tf_CSecond>(_Second), fg_Forward<tfp_CRest>(p_Rest)...);
	}

	template <typename tf_CFirst, typename tf_CMin, typename tf_CMax>
	constexpr inline_small typename NTraits::TCRemoveReference<tf_CFirst>::CType fg_Clamp(tf_CFirst &&_First, tf_CMin &&_Min, tf_CMax &&_Max)
	{
		return fg_Max(fg_Min(fg_Forward<tf_CFirst>(_First), fg_Forward<tf_CMax>(_Max)), fg_Forward<tf_CMin>(_Min));
	}

	template <typename tf_CFirst>
	constexpr inline_small typename NTraits::TCRemoveReference<tf_CFirst>::CType fg_Abs(tf_CFirst &&_First)
	{
		if (fg_Forward<tf_CFirst>(_First) >= typename NTraits::TCRemoveReference<tf_CFirst>::CType(0))
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
	constexpr inline_small t_ToAlign fg_AlignUpConstExpr(t_ToAlign _pMem, mint _Alignment)
	{
		typedef typename NTraits::TCUnsigned<typename NTraits::TCIntFromSizeLarger<sizeof(t_ToAlign)>::CType>::CType CIntegerType;
		return (t_ToAlign)((((CIntegerType)_pMem) + CIntegerType(_Alignment - 1)) & (~(CIntegerType(_Alignment) - CIntegerType(1))));
	}

	template <typename t_ToAlign>
	constexpr inline_small t_ToAlign fg_AlignDownConstExpr(t_ToAlign _pMem, mint _Alignment)
	{
		typedef typename NTraits::TCUnsigned<typename NTraits::TCIntFromSizeLarger<sizeof(t_ToAlign)>::CType>::CType CIntegerType;
		return (t_ToAlign)(((CIntegerType)_pMem) & (~(CIntegerType(_Alignment) - CIntegerType(1))));
	}

	template <typename t_ToAlign>
	inline_small t_ToAlign fg_AlignUp(t_ToAlign _pMem, mint _Alignment)
	{
		DMibFastCheck(_Alignment > 0);
		typedef typename NTraits::TCUnsigned<typename NTraits::TCIntFromSizeLarger<sizeof(t_ToAlign)>::CType>::CType CIntegerType;
		return (t_ToAlign)((((CIntegerType)_pMem) + CIntegerType(_Alignment - 1)) & (~(CIntegerType(_Alignment) - CIntegerType(1))));
	}

	template <typename t_ToAlign>
	inline_small t_ToAlign fg_AlignDown(t_ToAlign _pMem, mint _Alignment)
	{
		DMibFastCheck(_Alignment > 0);
		typedef typename NTraits::TCUnsigned<typename NTraits::TCIntFromSizeLarger<sizeof(t_ToAlign)>::CType>::CType CIntegerType;
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
	typename TCEnableIf
	<
		!NTraits::TCIsVoid<tf_CType>::mc_Value
		&& !NTraits::TCIsArray<tf_CType>::mc_Value
		&& !NTraits::TCIsPointer<tf_CType>::mc_Value
		, void
	>::CType
	fg_CallDestructor(tf_CType &_Type)
	{
		_Type.~tf_CType();
	}

	template <typename tf_CType>
	typename TCEnableIf
	<
		NTraits::TCIsVoid<tf_CType>::mc_Value
		|| NTraits::TCIsPointer<tf_CType>::mc_Value
		, void
	>::CType
	fg_CallDestructor(tf_CType &_Type)
	{
	}
	
	template <typename tf_CType>
	typename TCEnableIf
	<
		NTraits::TCIsArray<tf_CType>::mc_Value
		, void
	>::CType
	fg_CallDestructor(tf_CType &_Type)
	{
		aint iElement = NTraits::TCExtent<tf_CType>::mc_Value - 1;
		for (; iElement >= 0; --iElement)
		{
			fg_CallDestructor(_Type[iElement]);
		}
	}
	
	class CSort_Default
	{
	public:
		template <typename t_CKey0, typename t_CKey1>
		inline_small auto operator() (t_CKey0 &&_Left, t_CKey1 &&_Right) const
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
			typedef typename NMib::NTraits::TCUnsigned<t_CIntType>::CType CUnsigned;

			constexpr static t_CIntType mc_Min = (t_CIntType(1) << ((sizeof(t_CIntType)*8)-1));
			constexpr static t_CIntType mc_Max = t_CIntType((CUnsigned(1) << ((sizeof(t_CIntType)*8)-1)) - CUnsigned(1));
			constexpr static t_CIntType mc_AllBits = t_CIntType(CUnsigned(0) - 1);
		};

#ifdef DCompiler_MSVC
	#pragma warning(pop)
#endif
	}

	template <typename t_CIntType>
	class TCLimitsInt : public NMib::NPrivate::TCLimitsIntHelper<t_CIntType, NTraits::TCIsSigned<t_CIntType>::mc_Value>
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
	
	
	template <typename t_CInt0, typename t_CInt1>
	bool fg_SafeLargerThan(t_CInt0 const &_Left, t_CInt1 const &_Right)
	{
		if constexpr (sizeof(t_CInt0) > sizeof(t_CInt1))
		{
			if constexpr (NTraits::TCIsSigned<t_CInt0>::mc_Value)
			{
				if constexpr (NTraits::TCIsSigned<t_CInt1>::mc_Value)
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
				if constexpr (NTraits::TCIsSigned<t_CInt1>::mc_Value)
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
			if constexpr (NTraits::TCIsSigned<t_CInt0>::mc_Value)
			{
				if constexpr (NTraits::TCIsSigned<t_CInt1>::mc_Value)
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
				if constexpr (NTraits::TCIsSigned<t_CInt1>::mc_Value)
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
			if constexpr (NTraits::TCIsSigned<t_CInt0>::mc_Value)
			{
				if constexpr (NTraits::TCIsSigned<t_CInt1>::mc_Value)
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
				if constexpr (NTraits::TCIsSigned<t_CInt1>::mc_Value)
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

		auto operator <=> (TCAutoClearInt const &_Right) const
		{
			return f_Get() <=> _Right.f_Get();
		}

		bool operator == (TCAutoClearInt const &_Right) const
		{
			return f_Get() == _Right.f_Get();
		}

		template <typename tf_CRight>
		auto operator <=> (tf_CRight const &_Right) const
		{
			return f_Get() <=> _Right;
		}

		template <typename tf_CRight, tf_CRight tf_RightValue>
		auto operator <=> (TCAutoClearInt<tf_CRight, tf_RightValue> const &_Right) const
		{
			return f_Get() <=> _Right.f_Get();
		}

		template <typename tf_CRight>
		bool operator == (tf_CRight const &_Right) const
		{
			return f_Get() == _Right;
		}

		template <typename tf_CRight, tf_CRight tf_RightValue>
		bool operator == (TCAutoClearInt<tf_CRight, tf_RightValue> const &_Right) const
		{
			return f_Get() == _Right.f_Get();
		}
	};

	template <typename t_CType>
	class TCAutoClear
	{
	public:
		TCAutoClear()
		{
			m_Value = 0;
		}
		TCAutoClear(t_CType const& _Value)
		{
			m_Value = _Value;
		}
		t_CType m_Value;

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



		TCAutoClear &operator = (TCAutoClear const& _Other)
		{
			m_Value = _Other.m_Value;
			return *this;
		}

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

		auto operator <=> (TCAutoClear const &_Right) const
		{
			return f_Get() <=> _Right.f_Get();
		}

		bool operator == (TCAutoClear const &_Right) const
		{
			return f_Get() == _Right.f_Get();
		}

		template <typename tf_CRight>
		auto operator <=> (tf_CRight const &_Right) const
		{
			return f_Get() <=> _Right;
		}

		template <typename tf_CRight>
		auto operator <=> (TCAutoClear<tf_CRight> const &_Right) const
		{
			return f_Get() <=> _Right.f_Get();
		}

		template <typename tf_CRight>
		bool operator == (tf_CRight const &_Right) const
		{
			return f_Get() == _Right;
		}

		template <typename tf_CRight>
		bool operator == (TCAutoClear<tf_CRight> const &_Right) const
		{
			return f_Get() == _Right.f_Get();
		}

		template <typename tf_CRight, tf_CRight tf_RightValue>
		auto operator <=> (TCAutoClearInt<tf_CRight, tf_RightValue> const &_Right) const
		{
			return f_Get() <=> _Right.f_Get();
		}

		template <typename tf_CRight, tf_CRight tf_RightValue>
		bool operator == (TCAutoClearInt<tf_CRight, tf_RightValue> const &_Right) const
		{
			return f_Get() == _Right.f_Get();
		}
	};

	template <typename t_CType, t_CType _Argument0, t_CType _Argument1>
	struct TCConstantMax : public NTraits::TCCompileTimeConstant<t_CType, (_Argument0 > _Argument1 ? _Argument0 : _Argument1)>
	{
	};


	template <typename t_CType, t_CType _Argument0, t_CType _Argument1>
	struct TCConstantMin : public NTraits::TCCompileTimeConstant<t_CType, (_Argument0 < _Argument1 ? _Argument0 : _Argument1)>
	{
	};

	template <typename t_CType, t_CType _Argument0>
	struct TCConstantAbs: public NTraits::TCCompileTimeConstant<t_CType, (_Argument0 < t_CType(0) ? (t_CType(0)-_Argument0) : _Argument0)>
	{
	};

	template <typename t_CAny>
	static t_CAny fg_MakeSymbolActive(t_CAny &&_Other)
	{
		return _Other;
	}

	template <typename tf_CType>
	ch8 const *fg_GetTypeName();

	struct CConstExprSubStr
	{
		constexpr CConstExprSubStr(char const *_pString, mint _Len)
			: m_pString(_pString)
			, m_Len(_Len)
		{
		}

		char const *m_pString;
		mint m_Len;
	};

	template <typename tf_CType>
	static consteval CConstExprSubStr fg_GetTypeNameConstExpr();

	template <mint t_nCharacters>
	struct TCConstExprSubStr : public CConstExprSubStr
	{
		constexpr TCConstExprSubStr(char const *_pString)
			: CConstExprSubStr(m_String, t_nCharacters)
		{
			for (mint i = 0; i < t_nCharacters; ++i)
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
	static constexpr uint32 fg_JenkinsHash(const char * const _pString, mint _Len, char _ExtraChar);

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
#define DMibGetParent(_Class, _Member, _Ptr) ((_Class *)(((uint8 *)_Ptr) + ( ((mint)((_Class *)((void*)_Ptr))) - ((mint)(&((_Class *)((void *)_Ptr))->_Member)) )))
#define DMibGetHighestBitSet(_Number) (NMib::TCHighestBitSet<mint, _Number>::mc_Value)

#ifndef DMibPNoShortCuts
#	define DGetParent(_Class, _Member, _Ptr) DMibGetParent(_Class, _Member, _Ptr)
#	define DBit(_Bit) DMibBit(_Bit)
#	define DBitTyped(_Bit, _Type) DMibBitTyped(_Bit, _Type)
#	define DBitRange(_BitStart, _BitEnd) DMibBitRange(_BitStart, _BitEnd)
#	define DGetHighestBitSet(_Number) DMibGetHighestBitSet(_Number)
#	define DNew DMibNew
#endif

