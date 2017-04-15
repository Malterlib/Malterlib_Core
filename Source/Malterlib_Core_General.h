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
#define DMibNewAligned(d_Type) new(CNewAligned(NMib::NTraits::TCAlignmentOf<d_Type>::mc_Value), DMibPFile, NMib::CLineNumber(DMibPLine)) d_Type
#else
#define DMibNew new
#define DMibNewAligned(d_Type) new(CNewAligned(NMib::NTraits::TCAlignmentOf<d_Type>::mc_Value)) d_Type
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

#if defined(DCompiler_MSVC) && DMibCompilerVersion < 1700 || defined(DCompiler_MSVC_EDG)

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
	constexpr inline_always_debug t_CType&& fg_Forward(typename NTraits::TCRemoveReference<t_CType>::CType &_ToForward) 
	{
    	return static_cast<t_CType&&>(_ToForward);
	}

	template <typename t_CType> 
	constexpr inline_always_debug t_CType&& fg_Forward(typename NTraits::TCRemoveReference<t_CType>::CType &&_ToForward) noexcept
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
	inline_always_debug typename NPrivate::TCForwardCopyEvalHelper<t_CType, t_CTypeToCopyTo>::CType fg_ForwardAs(t_CTypeToCopyTo &&_ToForward) 
	{
    	return static_cast<typename NPrivate::TCForwardCopyEvalHelper<t_CType, t_CTypeToCopyTo>::CType>(_ToForward);
	}
	
	template <typename t_CType> 
	inline_always_debug typename NTraits::TCRemoveReference<t_CType>::CType &&fg_Move(t_CType &&_ToMove) noexcept
	{
		static_assert(!NTraits::TCIsConst<typename NTraits::TCRemoveReference<t_CType>::CType>::mc_Value, "Trying to move const value");
    	return ((typename NTraits::TCRemoveReference<t_CType>::CType &&)_ToMove);
	}
	
	template <typename t_CType> 
	inline_always_debug typename NTraits::TCRemoveReference<t_CType>::CType &&fg_MoveAllowConst(t_CType &&_ToMove) noexcept
	{
    	return ((typename NTraits::TCRemoveReference<t_CType>::CType &&)_ToMove);
	}
	
	template <typename tf_CType>
	tf_CType fg_TempCopy(tf_CType const &_Value)
	{
		return _Value;
	}
	
	template <typename t_CType>
	inline_always_debug t_CType volatile &fg_Volatile(t_CType &_In)
	{
		return (t_CType volatile &)_In;
	}

	template <typename t_CType>
	inline_always_debug t_CType const &fg_Const(t_CType const&_In)
	{
		return (t_CType const &)_In;
	}

	template <typename t_CType, TCEnableIfType<NTraits::TCIsRValueReference<t_CType>::mc_Value || !NTraits::TCIsReference<t_CType>::mc_Value> * = nullptr>
	constexpr inline_always_debug decltype(auto) fg_ConstOrMove(t_CType &&_In)
	{
		return fg_Move(_In);
	}

#ifndef DDocumentation_Doxygen
	template <typename t_CType, TCEnableIfType<!NTraits::TCIsRValueReference<t_CType>::mc_Value && NTraits::TCIsReference<t_CType>::mc_Value> * = nullptr>
	constexpr inline_always_debug auto fg_ConstOrMove(t_CType &&_In) -> typename NTraits::TCRemoveReference<t_CType>::CType const &
	{
		return static_cast<typename NTraits::TCRemoveReference<t_CType>::CType const &>(_In);
	}
#endif

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

//		TCExplicit(TCExplicit const &_Other);
//		TCExplicit(TCExplicit &&_Other);
//		static_assert(NTraits::TCIsReference<t_CType>::mc_Value, "Should always be a reference here");

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
	
	template <typename tf_CType, TCEnableIfType<!NTraits::TCIsReference<tf_CType>::mc_Value> * = nullptr>
	inline_always_debug TCExplicit<tf_CType> fg_Explicit(tf_CType &&_In)
	{
		return TCExplicit<tf_CType>(fg_Move(_In));
	}
	
	inline_always_debug TCExplicit<void> fg_Explicit()
	{
		return TCExplicit<void>();
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
	static inline_small t_CType fg_PowerOfTwoMinusOne(uaint _Power)
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
		if (TCHelper_ByteSwap<t_CInt>::EDefaultImplementation)
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


	template <typename tf_C1, typename tf_C2>
	inline_small typename NTraits::TCRemoveReference<tf_C1>::CType fg_Min(tf_C1 &&_First, tf_C2 &&_Second)
	{
		if (fg_Forward<tf_C1>(_First) < fg_Forward<tf_C2>(_Second))
			return fg_Forward<tf_C1>(_First);
		else
			return fg_Forward<tf_C2>(_Second);
	}

	template <typename tf_C1, typename tf_C2>
	inline_small typename NTraits::TCRemoveReference<tf_C1>::CType fg_Max(tf_C1 &&_First, tf_C2 &&_Second)
	{
		if (fg_Forward<tf_C2>(_Second) < fg_Forward<tf_C1>(_First))
			return fg_Forward<tf_C1>(_First);
		else
			return fg_Forward<tf_C2>(_Second);
	}

	template <typename tf_C1, typename tf_C2, typename tf_C3>
	inline_small typename NTraits::TCRemoveReference<tf_C1>::CType fg_Clamp(tf_C1 &&_First, tf_C2 &&_Min, tf_C3 &&_Max)
	{
		return fg_Max(fg_Min(fg_Forward<tf_C1>(_First), fg_Forward<tf_C3>(_Max)), fg_Forward<tf_C2>(_Min));
	}

	template <typename tf_C1>
	inline_small typename NTraits::TCRemoveReference<tf_C1>::CType fg_Abs(tf_C1 &&_First)
	{
		if (fg_Forward<tf_C1>(_First) >= typename NTraits::TCRemoveReference<tf_C1>::CType(0))
			return fg_Forward<tf_C1>(_First);
		else
			return -fg_Forward<tf_C1>(_First);
	}

	template <typename tf_C1, typename tf_C2>
	constexpr inline_small tf_C1 fg_MinConstexpr(tf_C1 _First, tf_C2 _Second)
	{
		return (_First < _Second) ? _First : _Second;
	}

	template <typename tf_C1, typename tf_C2>
	constexpr inline_small tf_C1 fg_MaxConstexpr(tf_C1 _First, tf_C2 _Second)
	{
		return (_Second < _First) ? _First : _Second;
	}

	template <typename tf_C1, typename tf_C2, typename tf_C3>
	constexpr inline_small tf_C1 fg_ClampConstexpr(tf_C1 _First, tf_C2 _Min, tf_C3 _Max)
	{
		return fg_MaxConstexpr(fg_MinConstExpr(_First, _Max), _Min);
	}

	template <typename tf_C1>
	constexpr inline_small tf_C1 fg_AbsConstexpr(tf_C1 _First)
	{
		return (_First >= tf_C1(0)) ? _First : -_First;
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

	class CDefaultPointerHolder
	{
	public:
#ifndef DMibNoAggregateConstexpr
		constexpr CDefaultPointerHolder(EAggregateInitialization _Init)
			: m_pData(nullptr)
		{
		}
		inline_always CDefaultPointerHolder()
		{
		}
#endif
		void *m_pData;

		inline_small void * f_Get() const
		{
			return m_pData;
		}

		inline_small void f_Set(void *_pAddress)
		{
			m_pData = _pAddress;
		}

		inline_small void f_Set(CDefaultPointerHolder &_Address)
		{
			m_pData = _Address.m_pData;
		}        
	};

	template <typename t_CPointer, typename t_CTyped>
	class TCDynamicPtr
	{
	public:
#ifndef DMibNoAggregateConstexpr
		constexpr TCDynamicPtr(EAggregateInitialization _Init)
			: m_PtrData(_Init)
		{
		}
		inline_always TCDynamicPtr()
		{
		}
#endif
        t_CPointer m_PtrData;

		inline_small TCDynamicPtr &operator = (t_CTyped *_pSetTo)
		{
			m_PtrData.f_Set((void *)_pSetTo);
			return *this;
		}

		inline_small bint operator == (t_CTyped *_pCompareTo) const
		{
			return (t_CTyped *)m_PtrData.f_Get() == _pCompareTo;
		}

		inline_small operator t_CTyped * () const
		{			
			return (t_CTyped *)m_PtrData.f_Get();
		}

		inline_small t_CTyped * operator -> () const
		{
			return (t_CTyped *)m_PtrData.f_Get();
		}

		inline_small operator t_CTyped * ()
		{			
			return (t_CTyped *)m_PtrData.f_Get();
		}

		inline_small t_CTyped * operator -> ()
		{
			return (t_CTyped *)m_PtrData.f_Get();
		}

		inline_small operator t_CTyped * () const volatile
		{			
			return (t_CTyped *)m_PtrData.f_Get();
		}

		inline_small t_CTyped * operator -> () const volatile
		{
			return (t_CTyped *)m_PtrData.f_Get();
		}

		inline_small operator t_CTyped * () volatile
		{			
			return (t_CTyped *)m_PtrData.f_Get();
		}

		inline_small t_CTyped * operator -> () volatile
		{
			return (t_CTyped *)m_PtrData.f_Get();
		}
        
	};

	template <typename t_CTyped>
		class TCDynamicPtr<CDefaultPointerHolder, t_CTyped>
	{
	public:
#ifndef DMibNoAggregateConstexpr
		constexpr TCDynamicPtr(EAggregateInitialization _Init)
			: m_pPtr(nullptr)
		{
		}
		inline_always TCDynamicPtr()
		{
		}
#endif
        t_CTyped *m_pPtr;

		inline_small TCDynamicPtr &operator = (t_CTyped *_pSetTo)
		{
			m_pPtr = _pSetTo;
			return *this;
		}

		inline_small bint operator == (t_CTyped *_pCompareTo) const
		{
			return m_pPtr == _pCompareTo;
		}

		inline_small operator t_CTyped * () const
		{			
			return m_pPtr;
		}

		inline_small t_CTyped * operator -> () const
		{
			return m_pPtr;
		}
        
		inline_small operator t_CTyped * ()
		{			
			return m_pPtr;
		}

		inline_small t_CTyped * operator -> ()
		{
			return m_pPtr;
		}

		inline_small operator t_CTyped * () volatile
		{			
			return m_pPtr;
		}

		inline_small t_CTyped * operator -> () volatile
		{
			return m_pPtr;
		}
        
		inline_small operator t_CTyped * () const volatile
		{			
			return m_pPtr;
		}

		inline_small t_CTyped * operator -> () const volatile
		{
			return m_pPtr;
		}
        


        
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
	
	class CCompare_Default
	{
	public:
		typedef aint CRet;

		template <typename t_CContext, typename t_CKey0, typename t_CKey1>
		static inline_small CRet fs_Compare(t_CContext && _Context, t_CKey0 *const _pLeft, t_CKey1 *const _pRight)
		{
			if (*_pLeft < *_pRight)
				return -1;
			if (*_pLeft > *_pRight)
				return 1;
			return 0;
		}

	};

	class CSort_Default
	{
	public:
		template <typename t_CKey0, typename t_CKey1>
		inline_small bint operator()(t_CKey0 &&_Left, t_CKey1 &&_Right) const
		{
			return fg_Forward<t_CKey0>(_Left) < fg_Forward<t_CKey1>(_Right);
		}
	};

	class CEmpty
	{
	public:
	};

	namespace NPrivate
	{
#ifdef DCompiler_MSVC
	#pragma warning(push)
	#pragma warning(disable:4307)
	#pragma warning(disable:4309)
#endif
		
		template <typename t_CIntType, bint t_bSigned, bint t_bFundamental>
		class TCLimitsIntHelper
		{
		public:
			const static t_CIntType mc_Min;
			const static t_CIntType mc_Max;
		};


		template <typename t_CIntType, bint t_bFundamental>
		class TCLimitsIntHelper<t_CIntType, 1, t_bFundamental>
		{
		public:
			typedef typename NMib::NTraits::TCUnsigned<t_CIntType>::CType CUnsigned;
			const static t_CIntType mc_Min;
			const static t_CIntType mc_Max;
		};

		
		template <typename t_CIntType, bint t_bSigned, bint t_bFundamental>
		const t_CIntType TCLimitsIntHelper<t_CIntType, t_bSigned, t_bFundamental>::mc_Min = t_CIntType(0);

		template <typename t_CIntType, bint t_bSigned, bint t_bFundamental>
		const t_CIntType TCLimitsIntHelper<t_CIntType, t_bSigned, t_bFundamental>::mc_Max = (t_CIntType(0) - 1);


		template <typename t_CIntType, bint t_bFundamental>
		const t_CIntType TCLimitsIntHelper<t_CIntType, 1, t_bFundamental>::mc_Min = (t_CIntType(1) << ((sizeof(t_CIntType)*8)-1));

		template <typename t_CIntType, bint t_bFundamental>
		const t_CIntType TCLimitsIntHelper<t_CIntType, 1, t_bFundamental>::mc_Max = t_CIntType((CUnsigned(1) << ((sizeof(t_CIntType)*8)-1)) - CUnsigned(1));
		

		template <typename t_CIntType>
		class TCLimitsIntHelper<t_CIntType, 0, 1>
		{
		public:
			const static t_CIntType mc_Min = t_CIntType(0);
			const static t_CIntType mc_Max = (t_CIntType(0) - 1);
		};


		template <typename t_CIntType>
		const t_CIntType TCLimitsIntHelper<t_CIntType, 0, 1>::mc_Min;
		template <typename t_CIntType>
		const t_CIntType TCLimitsIntHelper<t_CIntType, 0, 1>::mc_Max;

		template <typename t_CIntType>
		class TCLimitsIntHelper<t_CIntType, 1, 1>
		{
			typedef typename NMib::NTraits::TCUnsigned<t_CIntType>::CType CUnsigned;
		public:
			const static t_CIntType mc_Min = (t_CIntType(1) << ((sizeof(t_CIntType)*8)-1));
			const static t_CIntType mc_Max = t_CIntType((CUnsigned(1) << ((sizeof(t_CIntType)*8)-1)) - CUnsigned(1));
		};

    
		template <typename t_CIntType>
		const t_CIntType TCLimitsIntHelper<t_CIntType, 1, 1>::mc_Min;
		template <typename t_CIntType>
		const t_CIntType TCLimitsIntHelper<t_CIntType, 1, 1>::mc_Max;
     
#ifdef DCompiler_MSVC
	#pragma warning(pop)
#endif
	}

	template <typename t_CIntType>
	class TCLimitsInt : public NMib::NPrivate::TCLimitsIntHelper<t_CIntType, NTraits::TCIsSigned<t_CIntType>::mc_Value, NTraits::TCIsFundamental<t_CIntType>::mc_Value>
	{
	public:
	};

	template <typename t_CIntType>
	class TCLimitsIntDyn : public NMib::NPrivate::TCLimitsIntHelper<t_CIntType, NTraits::TCIsSigned<t_CIntType>::mc_Value, false>
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
	bint fg_SafeLargerThan(t_CInt0 const &_Left, t_CInt1 const &_Right)
	{
		if (sizeof(t_CInt0) > sizeof(t_CInt1))
		{
			if (NTraits::TCIsSigned<t_CInt0>::mc_Value)
			{
				if (NTraits::TCIsSigned<t_CInt1>::mc_Value)
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
				if (NTraits::TCIsSigned<t_CInt1>::mc_Value)
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
		else if (sizeof(t_CInt0) < sizeof(t_CInt1))
		{
			if (NTraits::TCIsSigned<t_CInt0>::mc_Value)
			{
				if (NTraits::TCIsSigned<t_CInt1>::mc_Value)
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
				if (NTraits::TCIsSigned<t_CInt1>::mc_Value)
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
			if (NTraits::TCIsSigned<t_CInt0>::mc_Value)
			{
				if (NTraits::TCIsSigned<t_CInt1>::mc_Value)
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
				if (NTraits::TCIsSigned<t_CInt1>::mc_Value)
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
	};

	template <typename tf_CLeft, typename tf_CRight>
	bool operator < (TCAutoClear<tf_CLeft> const &_Left, tf_CRight const &_Right)
	{
		return _Left.f_Get() < _Right;
	}

	template <typename tf_CLeft, typename tf_CRight>
	bool operator < (tf_CLeft const &_Left, TCAutoClear<tf_CRight> const &_Right)
	{
		return _Left < _Right.f_Get();
	}

	template <typename tf_CLeft, typename tf_CRight>
	bool operator < (TCAutoClear<tf_CLeft> const &_Left, TCAutoClear<tf_CRight> const &_Right)
	{
		return _Left.f_Get() < _Right.f_Get();
	}

	template <typename tf_CLeft, typename tf_CRight>
	bool operator == (TCAutoClear<tf_CLeft> const &_Left, tf_CRight const &_Right)
	{
		return _Left.f_Get() == _Right;
	}

	template <typename tf_CLeft, typename tf_CRight>
	bool operator == (tf_CLeft const &_Left, TCAutoClear<tf_CRight> const &_Right)
	{
		return _Left == _Right.f_Get();
	}

	template <typename tf_CLeft, typename tf_CRight>
	bool operator == (TCAutoClear<tf_CLeft> const &_Left, TCAutoClear<tf_CRight> const &_Right)
	{
		return _Left.f_Get() == _Right.f_Get();
	}


	template <typename tf_CLeft, tf_CLeft tf_LeftValue, typename tf_CRight>
	bool operator < (TCAutoClearInt<tf_CLeft, tf_LeftValue> const &_Left, tf_CRight const &_Right)
	{
		return _Left.f_Get() < _Right;
	}

	template <typename tf_CLeft, typename tf_CRight, tf_CRight tf_RightValue>
	bool operator < (tf_CLeft const &_Left, TCAutoClearInt<tf_CRight, tf_RightValue> const &_Right)
	{
		return _Left < _Right.f_Get();
	}

	template <typename tf_CLeft, tf_CLeft tf_LeftValue, typename tf_CRight, tf_CRight tf_RightValue>
	bool operator < (TCAutoClearInt<tf_CLeft, tf_LeftValue> const &_Left, TCAutoClearInt<tf_CRight, tf_RightValue> const &_Right)
	{
		return _Left.f_Get() < _Right.f_Get();
	}

	template <typename tf_CLeft, tf_CLeft tf_LeftValue, typename tf_CRight>
	bool operator == (TCAutoClearInt<tf_CLeft, tf_LeftValue> const &_Left, tf_CRight const &_Right)
	{
		return _Left.f_Get() == _Right;
	}

	template <typename tf_CLeft, typename tf_CRight, tf_CRight tf_RightValue>
	bool operator == (tf_CLeft const &_Left, TCAutoClearInt<tf_CRight, tf_RightValue> const &_Right)
	{
		return _Left == _Right.f_Get();
	}

	template <typename tf_CLeft, tf_CLeft tf_LeftValue, typename tf_CRight, tf_CRight tf_RightValue>
	bool operator == (TCAutoClearInt<tf_CLeft, tf_LeftValue> const &_Left, TCAutoClearInt<tf_CRight, tf_RightValue> const &_Right)
	{
		return _Left.f_Get() == _Right.f_Get();
	}



	template <typename tf_CLeft, tf_CLeft tf_LeftValue, typename tf_CRight>
	bool operator < (TCAutoClearInt<tf_CLeft, tf_LeftValue> const &_Left, TCAutoClear<tf_CRight> const &_Right)
	{
		return _Left.f_Get() < _Right.f_Get();
	}
	template <typename tf_CLeft, typename tf_CRight, tf_CRight tf_RightValue>
	bool operator < (TCAutoClear<tf_CLeft> const &_Left, TCAutoClearInt<tf_CRight, tf_RightValue> const &_Right)
	{
		return _Left.f_Get() < _Right.f_Get();
	}

	template <typename tf_CLeft, tf_CLeft tf_LeftValue, typename tf_CRight>
	bool operator == (TCAutoClearInt<tf_CLeft, tf_LeftValue> const &_Left, TCAutoClear<tf_CRight> const &_Right)
	{
		return _Left.f_Get() == _Right.f_Get();
	}
	template <typename tf_CLeft, typename tf_CRight, tf_CRight tf_RightValue>
	bool operator == (TCAutoClear<tf_CLeft> const &_Left, TCAutoClearInt<tf_CRight, tf_RightValue> const &_Right)
	{
		return _Left.f_Get() == _Right.f_Get();
	}

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
		constexpr CConstExprSubStr(ch8 const *_pString, mint _Len)
			: m_pString(_pString)
			, m_Len(_Len)
		{
		}

		ch8 const *m_pString;
		mint m_Len;
	};

	template <typename tf_CType>
	constexpr CConstExprSubStr fg_GetTypeNameConstExpr()
	{
#if defined(DCompiler_MSVC)
		ch8 const *pParseStart = DMibPFunctionSignature;
		ch8 const *pParse = pParseStart;
		while (*pParse && *pParse != '<')
			++pParse;
		if (*pParse == '<')
			++pParse;
		if (pParse[0] == 'c' && pParse[1] == 'l' && pParse[2] == 'a' && pParse[3] == 's' && pParse[4] == 's' && pParse[5] == ' ')
			pParse += 6;
		else if (pParse[0] == 's' && pParse[1] == 't' && pParse[2] == 'r' && pParse[3] == 'u' && pParse[4] == 'c' && pParse[5] == 't' && pParse[5] == ' ')
			pParse += 7;
		ch8 const *pStartType = pParse;
		mint nStart = 0;
		mint nStartParen = 0;

		while (*pParse)
		{
			if (*pParse == '(')
			{
				++nStartParen;
			}
			else if (*pParse == ')')
			{
				--nStartParen;
			}
			else if (!nStartParen)
			{
				if (*pParse == '<')
				{
					++nStart;
				}
				else if (*pParse == '>')
				{
					if (nStart == 0)
						break;
					--nStart;
				}
			}
			++pParse;
		}
		return CConstExprSubStr(pStartType, (pParse - pStartType));
#else
		ch8 const *pParseStart = DMibPFunctionSignature;
		ch8 const *pParse = pParseStart;
		while (*pParse && *pParse != '=')
			++pParse;
		if (*pParse == '=')
			++pParse;
		if (*pParse == ' ')
			++pParse;
		ch8 const *pStartType = pParse;
		mint nStart = 1;

		while (*pParse)
		{
			if (*pParse == '[')
			{
				++nStart;
			}
			else if (*pParse == ']')
			{
				if (--nStart == 0)
					break;
			}
			++pParse;
		}
		return CConstExprSubStr(pStartType, (pParse - pStartType));
#endif
	}

	constexpr uint32 fg_JenkinsHash(const char * const _pString)
	{
		uint32 Hash = 0;
		for (ch8 const *pStr = _pString; *pStr; ++pStr)
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

	constexpr uint32 fg_JenkinsHash(const char * const _pString, mint _Len, ch8 _ExtraChar)
	{
		uint32 Hash = 0;
		ch8 const *pEnd = _pString + _Len;
		for (ch8 const *pStr = _pString; pStr < pEnd; ++pStr)
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
	
	template <typename tf_CMemberFunction>
	constexpr uint32 fg_GetMemberFunctionHash(const char * const _pFunctionName)
	{
		ch8 const *pStartName = nullptr;

		ch8 const *pEnd = _pFunctionName;
		for (; *pEnd; ++pEnd)
			;
		
		mint nOpen = 0;
		for (ch8 const *pParse = pEnd - 1; pParse > _pFunctionName; --pParse)
		{
			if (*pParse == '>' || *pParse == ')' || *pParse == ']')
			{
				++nOpen;
			}
			else if (*pParse == '<' || *pParse == '(' || *pParse == '[')
				--nOpen;
			
			if (nOpen == 0 && *pParse == ':')
			{
				pStartName = pParse + 1;
				break;
			}
		}
			
		auto ClassTypeName = fg_GetTypeNameConstExpr<typename NTraits::TCMemberFunctionPointerTraits<tf_CMemberFunction>::CClass>();
		return fg_JenkinsHash(pStartName) ^ fg_JenkinsHash(ClassTypeName.m_pString, ClassTypeName.m_Len, ']');
	}

	template <typename tf_CType>
	constexpr uint32 fg_GetTypeHash()
	{
		auto ClassTypeName = fg_GetTypeNameConstExpr<tf_CType>();
		return fg_JenkinsHash(ClassTypeName.m_pString, ClassTypeName.m_Len, ']');
	}
}

typedef NMib::TCAutoClear<bint> zbint;

typedef NMib::TCAutoClear<bool> zbool;

typedef NMib::TCAutoClear<mint> zmint;
typedef NMib::TCAutoClear<smint> zsmint;

typedef NMib::TCAutoClear<aint> zamint;
typedef NMib::TCAutoClear<uaint> zuamint;

typedef NMib::TCAutoClear<int8> zint8;
typedef NMib::TCAutoClear<uint8> zuint8;
typedef NMib::TCAutoClear<int16> zint16;
typedef NMib::TCAutoClear<uint16> zuint16;
typedef NMib::TCAutoClear<int32> zint32;
typedef NMib::TCAutoClear<uint32> zuint32;
typedef NMib::TCAutoClear<int64> zint64;
typedef NMib::TCAutoClear<uint64> zuint64;
typedef NMib::TCAutoClear<int80> zint80;
typedef NMib::TCAutoClear<uint80> zuint80;
typedef NMib::TCAutoClear<int128> zint128;
typedef NMib::TCAutoClear<uint128> zuint128;
typedef NMib::TCAutoClear<int160> zint160;
typedef NMib::TCAutoClear<uint160> zuint160;
typedef NMib::TCAutoClear<int256> zint256;
typedef NMib::TCAutoClear<uint256> zuint256;
typedef NMib::TCAutoClear<int320> zint320;
typedef NMib::TCAutoClear<uint320> zuint320;
typedef NMib::TCAutoClear<int512> zint512;
typedef NMib::TCAutoClear<uint512> zuint512;
typedef NMib::TCAutoClear<int1024> zint1024;
typedef NMib::TCAutoClear<uint1024> zuint1024;
typedef NMib::TCAutoClear<int2048> zint2048;
typedef NMib::TCAutoClear<uint2048> zuint2048;
typedef NMib::TCAutoClear<int4096> zint4096;
typedef NMib::TCAutoClear<uint4096> zuint4096;
typedef NMib::TCAutoClear<int8192> zint8192;
typedef NMib::TCAutoClear<uint8192> zuint8192;
typedef NMib::TCAutoClear<ch8> zch8;
typedef NMib::TCAutoClear<ch16> zch16;
typedef NMib::TCAutoClear<ch32> zch32;

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
#	define DNewAligned DMibNewAligned
#endif

