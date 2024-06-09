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

	namespace NContainer
	{
		template <mint t_MinSize = 16, bool t_bShrink = true, bool t_bCheckBounds = true>
		struct TCVectorOptions;

		struct CVectorOptionsDefault;

		template <typename t_CData, typename t_CAllocator = NMib::NMemory::CAllocator_Heap, typename t_COptions = CVectorOptionsDefault>
		class TCVector;

		struct CByteVector;
		struct CSecureByteVector;
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

	typedef TCVoidFunctor<void> CVoidFunctor;

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

		template <CSharedPointerOptionUnderlying t_Options = ESharedPointerOption_None>
		struct TCIntrusiveRefCount;

		template <>
		struct TCIntrusiveRefCount<ESharedPointerOption_None>;

		template <>
		struct TCIntrusiveRefCount<ESharedPointerOption_SupportWeakPointer>;
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
		auto operator <=> (CVoidTag const &_Right) const = default;

		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const
		{
			o_Str += "void";
		}
	};
	extern CVoidTag const g_Void;

}

#include <Mib/Numeric/Integer>

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

#include "../../String/Source/Malterlib_String_Container_Types.h"

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

