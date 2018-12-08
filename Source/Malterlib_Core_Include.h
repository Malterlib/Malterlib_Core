// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#ifdef DMibSafety_IncMalterlib_H
//#	error "You have to include this file through <Mib/Core/Core>"

namespace NMib
{
	class CSharedPointerHolder;

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

		using CAllocator_HeapSecure = TCAllocator_Secure<CAllocator_Heap, true>;
	}
	
	namespace NThread
	{
		class CNoLock;
		template <typename t_CEvent, bool t_bAllowRecursive>
		class TMutual;
		class CEventAutoResetAggregate;
	}

	namespace NContainer
	{
		template <mint t_MinSize = 16, bool t_bShrink = true, bool t_bCheckBounds = true>
		struct TCVectorOptions;

		struct CVectorOptionsDefault;

		template <typename t_CData, typename t_CAllocator = NMib::NMemory::CAllocator_Heap, typename t_COptions = CVectorOptionsDefault>
		class TCVector;

		using CByteVector = TCVector<uint8>;
		using CSecureByteVector = TCVector<uint8, NMemory::CAllocator_HeapSecure>;
	}

	struct CVoidTag 
	{
		bool operator == (CVoidTag const &_Right) const
		{
			return true;
		}

		bool operator < (CVoidTag const &_Right) const
		{
			return false;
		}
	};


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

	namespace NStorage
	{
		using CSharedPointerOptionUnderlaying = int32;
		enum ESharedPointerOption : int32
		{
			ESharedPointerOption_None = 0
			, ESharedPointerOption_SupportWeakPointer = DMibBit(0)
		};

		template <CSharedPointerOptionUnderlaying t_Options = ESharedPointerOption_None> 
		class TCSharedPointerIntrusiveBase;

		template <>
		class TCSharedPointerIntrusiveBase<ESharedPointerOption_None>;

		template <>
		class TCSharedPointerIntrusiveBase<ESharedPointerOption_SupportWeakPointer>;

	}
	
	namespace NStream
	{
		template <typename t_CKeyStr, typename t_CToStream>
		class TCNamedStreamInfo;

		template <typename t_CKeyStr, typename t_CToStream, typename t_CToStreamDefault>
		TCNamedStreamInfo<t_CKeyStr, t_CToStream> fg_Named(t_CKeyStr const &_Key, t_CToStream &_ToStream, t_CToStreamDefault const &_Default);

		template <typename t_CKeyStr, typename t_CToStream>
		TCNamedStreamInfo<t_CKeyStr, t_CToStream const> fg_Named(t_CKeyStr const &_Key, t_CToStream const &_ToStream);
		
	}
	
	template <typename t_CIntType>
	class TCLimitsInt;
	template <typename t_CIntType>
	class TCLimitsIntDyn;

	namespace NFunction
	{
		template 
		<
			typename t_CFunction // The function definition to contain
			, typename... tp_COptions // Arguments, Can be function definition, option (CFunctionSupportCompareTag) or allocator 
		>
		class TCFunctionNoAlloc;
	}
}


#include "Malterlib_Core_General.h"

#include <Mib/Memory/Construct>

#include <Mib/String/Algorithm>

#include "Malterlib_Core_PlatformInterface.h"

#include <Mib/Debug/Debug>
#include <Mib/CommandLine/Console>
#include <Mib/Memory/Memory>

#include <Mib/Memory/Allocators/Virtual>
#include <Mib/Memory/Allocators/Default>
#include <Mib/Memory/Allocators/Secure>

namespace NMib
{
	namespace NMemory
	{
		typedef NMib::NMemory::CAllocator_Heap CDefaultAllocator;
	}
}

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

#include <Mib/Storage/UniquePointer>

#ifdef DMibEnableDistributedActors
#	include <Mib/Concurrency/RuntimeTypeRegistry>
#endif

#include <Mib/Exception/Exception>
#include "../../Core/Source/Malterlib_Core_OnScopeExit.h"


#include <Mib/Storage/Pointer>

#include <Mib/Thread/Thread>

#include <Mib/Stream/Binary>

#include <Mib/Container/Vector>

#include <Mib/Storage/Aggregate>

#include <Mib/Thread/Local>

#include <Mib/Memory/Pool>

#include "../../String/Source/Malterlib_String.h"

#include <Mib/Container/Map>

#include <Mib/Time/Time>

#include "../../Core/Source/Malterlib_Core_Misc.h"

#include "../../Core/Source/Malterlib_Core_System.h"

#include <Mib/Container/LinkedList>
#include <Mib/Stream/Streams/LinkedList>

#include <Mib/Container/Registry>

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
#include "../../Thread/Source/Malterlib_Thread.hpp"
#include "../../Storage/Source/Malterlib_Storage_Aggregate.hpp"

#include "../../Memory/Source/Malterlib_Memory_Allocator_Virtual.hpp"
#include "../../Memory/Source/Malterlib_Memory_Allocator_Heap.hpp"
#include "../../Memory/Source/Malterlib_Memory_Allocator_Secure.hpp"
#include "../../Container/Source/Vector/Malterlib_Container_Vector_Format.hpp"
#include "../../Container/Source/Malterlib_Container_Map.hpp"

#ifdef DMibEnableDistributedActors
#include "../../Concurrency/Source/DistributedActor/Malterlib_Concurrency_RuntimeTypeRegistry.hpp"
#endif

#include "Platform/Malterlib_Core_PlatformImp.imp.h"

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

