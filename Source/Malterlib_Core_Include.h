// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#ifdef DMibSafety_IncMalterlib_H
//#	error "You have to include this file through <Mib/Core/Core>"

namespace NMib
{
	namespace NMemory
	{
		class CAllocator_Heap;
		class CAllocator_Virtual;

		template
		<
			typename t_CBaseAllocator
			, bool t_bStatic
		>
		class TCAllocator_Secure;

		struct CAllocator_HeapSecure;

		using CDefaultAllocator = CAllocator_Heap;
	}

	namespace NThread
	{
		class CNoLock;
		template <typename t_CEvent, bool t_bAllowRecursive>
		class TCMutual;
		struct CEventAutoResetAggregate;
	}

	namespace NFile
	{
#if DMibPPtrBits > 32
		static constexpr mint gc_IdealNetworkQueueSize = 128 * 1024 * 1024;
#else
		static constexpr mint gc_IdealNetworkQueueSize = 16 * 1024 * 1024;
#endif
		static constexpr mint gc_IdealIoSize = 1024 * 1024;
	}

	namespace NContainer
	{
		template <mint t_MinSize = 16, bool t_bShrink = true, bool t_bCheckBounds = true>
		struct TCVectorOptions;

		struct CVectorOptionsDefault;

		template <typename t_CData, typename t_CAllocator = NMib::NMemory::CAllocator_Heap, typename t_COptions = CVectorOptionsDefault>
		struct TCVector;

		struct CByteVector;
		struct CSecureByteVector;

//#define DMibSecureClearIOBuffers_Enable
#ifdef DMibSecureClearIOBuffers_Enable
		using CIOByteVector = NContainer::CSecureByteVector;
#else
		using CIOByteVector = NContainer::CByteVector;
#endif
	}

	namespace NFile
	{
	}

	struct CVoidTag;

	template <typename t_CReturn>
	struct TCVoidFunctor
	{
		template <typename ...tfp_CParam>
		t_CReturn operator ()(tfp_CParam && ...) const volatile
		{
			return t_CReturn();
		}
	};

	template <>
	struct TCVoidFunctor<void>
	{
		template <typename ...tfp_CParam>
		void operator ()(tfp_CParam && ...) const volatile
		{
		}
	};

	using CVoidFunctor = TCVoidFunctor<void>;

	struct CEmpty
	{
	};

	namespace NStorage
	{
		using CSharedPointerOptionUnderlying = int32;
		enum ESharedPointerOption : int32
		{
			ESharedPointerOption_None = 0
			, ESharedPointerOption_SupportWeakPointer = DMibBit(0)
		};

		template <CSharedPointerOptionUnderlying t_Options = ESharedPointerOption_None, typename t_CountType = smint>
		struct TCIntrusiveRefCount;

		template <typename t_CountType>
		struct TCIntrusiveRefCount<ESharedPointerOption_None, t_CountType>;

		template <typename t_CountType>
		struct TCIntrusiveRefCount<ESharedPointerOption_SupportWeakPointer, t_CountType>;
	}

	template <typename t_CIntType>
	class TCLimitsInt;

	namespace NFunction
	{
		template
		<
			typename t_CFunction // The function definition to contain
			, typename... tp_COptions // Arguments, Can be function definition, option (CFunctionSupportEqualityCompareTag, CFunctionSupportOrderedCompareTag) or allocator
		>
		class TCFunctionNoAlloc;

		template <typename t_CSignature>
		struct TCFunctionMovable;
	}
}


#include "Malterlib_Core_General.h"
#include "Malterlib_Core_Literals.h"

#include "../../Memory/Source/Malterlib_Memory_Allocator_New.h"

#ifdef DMalterlibUseStaticLibCxx
#include <__compare/ordering.h>
#include <__compare/common_comparison_category.h>
#else
#include <compare>
#endif

namespace NMib
{
	template <typename ...tp_CType>
	using TCCommonOrderingType = std::common_comparison_category_t<tp_CType...>;

	struct CVoidTag
	{
		auto operator <=> (CVoidTag const &_Right) const noexcept = default;

		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const
		{
			o_Str += "void";
		}
	};
	extern CVoidTag const g_Void;

}

#include <Mib/Numeric/Integer>

using zbool = NMib::TCAutoClear<bool>;
using zmint = NMib::TCAutoClear<mint>;
using zsmint = NMib::TCAutoClear<smint>;
using zamint = NMib::TCAutoClear<aint>;
using zuamint = NMib::TCAutoClear<uaint>;
using zint8 = NMib::TCAutoClear<int8>;
using zuint8 = NMib::TCAutoClear<uint8>;
using zint16 = NMib::TCAutoClear<int16>;
using zuint16 = NMib::TCAutoClear<uint16>;
using zint32 = NMib::TCAutoClear<int32>;
using zuint32 = NMib::TCAutoClear<uint32>;
using zint64 = NMib::TCAutoClear<int64>;
using zuint64 = NMib::TCAutoClear<uint64>;
using zint80 = NMib::TCAutoClear<int80>;
using zuint80 = NMib::TCAutoClear<uint80>;
using zint96 = NMib::TCAutoClear<int96>;
using zuint96 = NMib::TCAutoClear<uint96>;
using zint128 = NMib::TCAutoClear<int128>;
using zuint128 = NMib::TCAutoClear<uint128>;
using zint160 = NMib::TCAutoClear<int160>;
using zuint160 = NMib::TCAutoClear<uint160>;
using zint192 = NMib::TCAutoClear<int192>;
using zuint192 = NMib::TCAutoClear<uint192>;
using zint256 = NMib::TCAutoClear<int256>;
using zuint256 = NMib::TCAutoClear<uint256>;
using zint320 = NMib::TCAutoClear<int320>;
using zuint320 = NMib::TCAutoClear<uint320>;
using zint384 = NMib::TCAutoClear<int384>;
using zuint384 = NMib::TCAutoClear<uint384>;
using zint512 = NMib::TCAutoClear<int512>;
using zuint512 = NMib::TCAutoClear<uint512>;
using zint1024 = NMib::TCAutoClear<int1024>;
using zuint1024 = NMib::TCAutoClear<uint1024>;
using zint2048 = NMib::TCAutoClear<int2048>;
using zuint2048 = NMib::TCAutoClear<uint2048>;
using zint4096 = NMib::TCAutoClear<int4096>;
using zuint4096 = NMib::TCAutoClear<uint4096>;
using zint8192 = NMib::TCAutoClear<int8192>;
using zuint8192 = NMib::TCAutoClear<uint8192>;
using zint16384 = NMib::TCAutoClear<int16384>;
using zuint16384 = NMib::TCAutoClear<uint16384>;
using zch8 = NMib::TCAutoClear<ch8>;
using zch16 = NMib::TCAutoClear<ch16>;
using zch32 = NMib::TCAutoClear<ch32>;

#include <Mib/Numeric/Float>

#include <Mib/Memory/Construct>

#include <Mib/String/Algorithm>

#include "Malterlib_Core_PlatformInterface.h"

#include <Mib/Debug/Debug>

#if defined(DMibContract_AnyEnabled) || DMibEnableSafeCheck > 0
	#define DMibNeedDebugException
#endif

#include <Mib/CommandLine/Console>
#include <Mib/Memory/Memory>

#include <Mib/Memory/Allocators/Virtual>
#include <Mib/Memory/Allocators/Default>
#include <Mib/Memory/Allocators/Secure>

#include "../../Numeric/Source/Malterlib_Numeric_Float_StdLib.hpp"

namespace NMib
{
	namespace NStorage
	{
		template <typename t_CType, typename... tp_COptions>
		class TCSharedPointer;

		template <typename t_CType, typename... tp_COptions>
		class TCWeakPointer;

	}
}

#include "../../String/Source/Malterlib_String_Types.h"

#include "../../String/Source/Container/Malterlib_String_Container_Types.h"

#include <Mib/Intrusive/SinglyLinkedList>
#include <Mib/Intrusive/DoublyLinkedList>
#include <Mib/Core/CoroutineHandler>

#include <Mib/Memory/MemoryReporter>

#include <Mib/Storage/UniquePointer>

#include <Mib/Concurrency/RuntimeTypeRegistry>

#include <Mib/Exception/Exception>
#include "../../Core/Source/Malterlib_Core_OnScopeExit.h"


#include <Mib/Storage/Pointer>

#include <Mib/Thread/Thread>

#include "../../Storage/Source/Malterlib_Storage_LazyInit.hpp"

#include <Mib/Stream/Binary>

#include <Mib/Container/Map>
#include <Mib/Container/Set>
#include <Mib/Container/Vector>

#include <Mib/Storage/Aggregate>

#include <Mib/Thread/Local>

#include <Mib/Memory/Pool>

#include "../../String/Source/Malterlib_String.h"


#include <Mib/Time/Time>

#include "../../Core/Source/Malterlib_Core_Misc.h"

#include "../../Core/Source/Malterlib_Core_System.h"

#include <Mib/Container/LinkedList>
#include <Mib/Stream/Streams/LinkedList>

#include <Mib/Stream/Streams/Vector>

#include <Mib/Log/Log>

#include <Mib/Contract/Contract>

#include "Malterlib_Core_PlatformInterfaceTemp.h"

#include <Mib/Type/TypeID>

#include "../../Core/Source/Malterlib_Core_OnScopeExitShared.h"

#include "../../Memory/Source/Malterlib_Memory_Reporter_CategoriesInterface.h"


// Template implementations
#include "Malterlib_Core_General.hpp"
#include "../../Memory/Source/Malterlib_Memory_Pool_Implementation.h"
#include "../../Exception/Source/Malterlib_Exception.hpp"
#include "../../Exception/Source/Malterlib_Exception_FastExceptions.hpp"
#include "../../Thread/Source/Malterlib_Thread.hpp"
#include "../../Storage/Source/Malterlib_Storage_Aggregate.hpp"

#include "../../Memory/Source/Malterlib_Memory_Allocator_Virtual.hpp"
#include "../../Memory/Source/Malterlib_Memory_Allocator_Heap.hpp"
#include "../../Memory/Source/Malterlib_Memory_Allocator_Secure.hpp"
#include "../../Container/Source/Vector/Malterlib_Container_Vector_Format.hpp"
#include "../../Container/Source/Map/Malterlib_Container_Map_Format.hpp"
#include "../../Container/Source/Map/Malterlib_Container_Map_IsContainer.hpp"
#include "../../Container/Source/Set/Malterlib_Container_Set_Format.hpp"
#include "../../Container/Source/Set/Malterlib_Container_Set_IsContainer.hpp"

#include "../../Concurrency/Source/DistributedActor/Malterlib_Concurrency_RuntimeTypeRegistry.hpp"

#include "Platform/Malterlib_Core_PlatformImp.imp.h"
#include "../../Debug/Source/Malterlib_Debug.hpp"

namespace NMib
{
	namespace NDebug
	{
#		if DMibEnableSafeCheck > 0
			NStr::CStrNonTracked fg_GetLastAssertMessage();
#		endif
	}
}


#ifndef DMibPNoShortCuts
	using namespace NMib;
	using namespace NMib::NMemory;
	using namespace NMib::NThread;
	using namespace NMib::NTime;
	using namespace NMib::NStr;
	using namespace NMib::NContainer;
	using namespace NMib::NIntrusive;
	using namespace NMib::NException;
	using namespace NMib::NMisc;
	using namespace NMib::NStream;
	using namespace NMib::NContainer;
	using namespace NMib::NAtomic;
	using namespace NMib::NSystem;
	#define DNewLine DMibNewLine
#endif

#endif

