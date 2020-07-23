// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Preprocessor/Preprocessor>
#include <Mib/Core/Platform>
#include <initializer_list>

namespace NMib
{
	enum EAggregateInitialization
	{
		EAggregateInitialization_Force
	};

	struct CVirtualDummy
	{
		virtual void f_Dummy()
		{
		}
	};
}

#ifndef DMalterlib
#define DMalterlib 1
#endif

#ifndef DMibNoAggregateConstexpr
#	define DAggregateInit ::NMib::EAggregateInitialization_Force
#else
#	define DAggregateInit 0
#endif

#ifdef DMibDebug
#define inline_never_debug inline_never
#else
#define inline_never_debug
#endif

#include "../../Core/Source/Malterlib_Core_Operators.h"
#include "../../Core/Source/Malterlib_Core_EnumOperators.h"

#include <Mib/Core/EnableIf>
#include <Mib/Type/Traits>

#include <Mib/Contract/Safe>

// Malterlib namespace... this makes 
#define DMibSafety_IncMalterlib_H

namespace NMib
{
#ifdef DMibPLittleEndian
	static const EEndian gc_MachineEndian = EEndian_Little;
#else
	static const EEndian gc_MachineEndian = EEndian_Big;
#endif

	template <typename t_CType>
	using TCInitializerList = std::initializer_list<t_CType>;
	
	template <typename tf_C1, typename tf_C2>
	inline_small typename NTraits::TCRemoveReference<tf_C1>::CType fg_Min(tf_C1 &&_First, tf_C2 &&_Second);

	template <typename tf_C1, typename tf_C2>
	inline_small typename NTraits::TCRemoveReference<tf_C1>::CType fg_Max(tf_C1 &&_First, tf_C2 &&_Second);

	template <typename tf_C1>
	inline_small typename NTraits::TCRemoveReference<tf_C1>::CType fg_Abs(tf_C1 &&_First);
	
	[[noreturn]] void fg_NoReturn();
}

namespace NMib
{
	template <typename t_CType0, typename t_CType1, typename t_CPrio = NTraits::TCCompileTimeConstant<int, 0>>
	class TCConvert
	{
	public:
		typedef int CDefault;

		static inline_small t_CType0 fs_Convert(t_CType1 const &_From)
		{
			return t_CType0(_From);
		}
	};

	namespace NConvertPrivate
	{
		template <typename tf_CType0>
		NMib::NTraits::CTrueBySize fg_IsDefault(typename tf_CType0::CDefault _Dummy);

		template <typename tf_CType0>
		NMib::NTraits::CFalseBySize fg_IsDefault(int16 _Dummy);

		template <typename t_CType0, typename t_CType1, typename t_CPrio>
		class TCIsDefault : public NTraits::TCCompileTimeConstant<bool, sizeof(fg_IsDefault<TCConvert<t_CType0, t_CType1, t_CPrio>>(0)) == sizeof(NMib::NTraits::CFalseBySize)>
		{
		};

		template <typename tf_CType0, typename tf_CType1>
		inline_small typename TCEnableIf<TCIsDefault<tf_CType0, tf_CType1, NMib::NTraits::TCCompileTimeConstant<int, 2> >::mc_Value, tf_CType0>::CType fg_ConvertPrivate(tf_CType1 const &_From)
		{
			return TCConvert<tf_CType0, tf_CType1, NMib::NTraits::TCCompileTimeConstant<int, 2> >::fs_Convert(_From);
		}
		template <typename tf_CType0, typename tf_CType1>
		inline_small typename TCEnableIf<TCIsDefault<tf_CType0, tf_CType1, NMib::NTraits::TCCompileTimeConstant<int, 1> >::mc_Value && !TCIsDefault<tf_CType0, tf_CType1, NMib::NTraits::TCCompileTimeConstant<int, 2> >::mc_Value, tf_CType0>::CType fg_ConvertPrivate(tf_CType1 const &_From)
		{
			return TCConvert<tf_CType0, tf_CType1, NMib::NTraits::TCCompileTimeConstant<int, 1> >::fs_Convert(_From);
		}
		template <typename tf_CType0, typename tf_CType1>
		inline_small typename TCEnableIf<!TCIsDefault<tf_CType0, tf_CType1, NMib::NTraits::TCCompileTimeConstant<int, 1> >::mc_Value && !TCIsDefault<tf_CType0, tf_CType1, NMib::NTraits::TCCompileTimeConstant<int, 2> >::mc_Value, tf_CType0>::CType fg_ConvertPrivate(tf_CType1 const &_From)
		{
			return TCConvert<tf_CType0, tf_CType1, NMib::NTraits::TCCompileTimeConstant<int, 0> >::fs_Convert(_From);
		}
	}

	template <typename tf_CType0, typename tf_CType1>
	inline_small typename TCDisableIf<NTraits::TCIsSame<tf_CType0, tf_CType1>::mc_Value, tf_CType0>::CType fg_Convert(tf_CType1 const &_From)
	{
		return NConvertPrivate::fg_ConvertPrivate<tf_CType0, tf_CType1>(_From);
	}

	template <typename tf_CType0, typename tf_CType1>
	inline_small typename TCEnableIf<NTraits::TCIsSame<tf_CType0, tf_CType1>::mc_Value, tf_CType0>::CType fg_Convert(tf_CType1 const &_From)
	{
		return _From;
	}

}

#include <Mib/Bit/Static>

#include <Mib/Numeric/Integer>
#include <Mib/Numeric/Float>
#	include "../../Core/Source/Platform/Malterlib_Core_PlatformImp.h"

#	include "../../Core/Source/Malterlib_Core_Include.h"
#include <Mib/Numeric/FloatImp>
namespace NMib
{
	namespace NFile
	{
		enum EFileSystemFeature
		{
			EFileSystemFeature_None			= 0
			, EFileSystemFeature_HasDrives	= DMibBit(0)
			, EFileSystemFeature_HasExecuteAttrib	= DMibBit(1)
		};
		
		enum EFileOpen
		{
			EFileOpen_None = 0
			, EFileOpen_ReadAttribs = DMibBit(0)			// The file is opened for attrib read access, if the file does not exist the open call fails
			, EFileOpen_WriteAttribs = DMibBit(1)		// The file is opened for attrib write access, if the file does not exist the open call fails
			, EFileOpen_Read = DMibBit(2)				// The file is opened for read access, if the file does not exist the open call fails
			, EFileOpen_Write = DMibBit(3)				// The file is opened for write access, if the file does not exist a new file is created
			, EFileOpen_DontCreate = DMibBit(4)			// The file is not created if it does not exist
			, EFileOpen_DontTruncate = DMibBit(5)		// The file is not truncated if it exist
			, EFileOpen_DontOpenExisting = DMibBit(6)	// The file fail opening if it already exists
			, EFileOpen_ShareRead = DMibBit(7)			// The file can be shared if read access is requested in subsequently opened files
			, EFileOpen_ShareWrite = DMibBit(8)			// The file can be shared if write access is requested in subsequently opened files
			, EFileOpen_WriteThrough = DMibBit(9)		// The file must be directly written to disk when flush is called, and no lazy writes
			, EFileOpen_NoCache = DMibBit(10)			// The file should not be cached by the operating system
			, EFileOpen_NoLocalCache = DMibBit(11)		// The file should cached locally in memory
			, EFileOpen_ShareDelete = DMibBit(12)		// The file can be shared if delete access is requested in subsequently opened files
			, EFileOpen_Temporary = DMibBit(13)			// The file is temporary and should be deleted automatically when closed
			, EFileOpen_RawFileName = DMibBit(14)		// The current directory should not be added to file name before opening
			, EFileOpen_NoFileLength = DMibBit(15)		// Don't expect the file to have a file length and don't cache it
			, EFileOpen_Directory = DMibBit(16)			// The target is a directory that is assumed to exist. Implies DontCreate and DontTruncate
			, EFileOpen_Link = DMibBit(17)				// Open the actual link instead of what the link is pointing to
			, EFileOpen_ShareBypass = DMibBit(18)		// Bypass file locking if possible
			, EFileOpen_ShareAll = EFileOpen_ShareRead | EFileOpen_ShareWrite | EFileOpen_ShareDelete
		};

		enum EFileAttrib
		{
			EFileAttrib_None		= 0
			, EFileAttrib_Directory	= DMibBit(0)
			, EFileAttrib_Link		= DMibBit(1)
			, EFileAttrib_Hidden		= DMibBit(2)
			, EFileAttrib_ReadOnly	= DMibBit(3)		// Available on Windows and OSX. On Linux this maps to EFileAttrib_UserWrite. If any of them are set the attribute is set on Linux
			, EFileAttrib_System		= DMibBit(4)
			, EFileAttrib_File		= DMibBit(5) // Used for finding files			
			, EFileAttrib_BackedUp	= DMibBit(6) // Bit that is reset when a file is written to (opened with write access). Only supporte by virtual FS.
			, EFileAttrib_FindDirectoryLast	= DMibBit(7)
			, EFileAttrib_Archive	= DMibBit(8)
			, EFileAttrib_Executable = DMibBit(9) // Only on platforms/filesystems that support it.
			, EFileAttrib_EmulatedLink = DMibBit(10) // Used to save emulated links and distinguish emulated from real links
			, EFileAttrib_UserExecute	= DMibBit(11)	// Unix only 
			, EFileAttrib_UserRead	= DMibBit(12)		// Unix only 
			, EFileAttrib_UserWrite	= DMibBit(13)		// Unix only 
			, EFileAttrib_GroupExecute	= DMibBit(14)	// Unix only 
			, EFileAttrib_GroupRead	= DMibBit(15)		// Unix only 
			, EFileAttrib_GroupWrite = DMibBit(16)		// Unix only 
			, EFileAttrib_EveryoneExecute	= DMibBit(17)	// Unix only 
			, EFileAttrib_EveryoneRead	= DMibBit(18)		// Unix only 
			, EFileAttrib_EveryoneWrite = DMibBit(19)		// Unix only 
			, EFileAttrib_UnixAttributesValid = DMibBit(20)		// When setting attributes needs to be specified for unix only attributes to be applied
			, EFileAttrib_AllUnixPermissions
				= EFileAttrib_UserExecute
				| EFileAttrib_UserRead
				| EFileAttrib_UserWrite
				| EFileAttrib_GroupExecute
				| EFileAttrib_GroupRead
				| EFileAttrib_GroupWrite
				| EFileAttrib_EveryoneExecute
				| EFileAttrib_EveryoneRead
				| EFileAttrib_EveryoneWrite
		};

		enum EFileMountType
		{
			EFileMountType_None = 0
			, EFileMountType_Block = DMibBit(0)
			, EFileMountType_Special = DMibBit(1)
			, EFileMountType_Local = DMibBit(2)
			, EFileMountType_Remote = DMibBit(3)
		};

		enum ESymbolicLinkFlag
		{
			ESymbolicLinkFlag_None					= 0
			, ESymbolicLinkFlag_Relative			= DMibBit(0)
			, ESymbolicLinkFlag_ConvertToDevicePath	= DMibBit(1) // Mutually exclusive with relative
			, ESymbolicLinkFlag_AllowEmulation		= DMibBit(2) // Emulates links with so they can be resolved and created without symlink support, but they will not work in the OS
		};

		enum EFileLock
		{
			EFileLock_None			= 0
			, EFileLock_PreventRead	= DMibBit(0)
			, EFileLock_Block		= DMibBit(1)
		};

		enum EFileChange
		{
			EFileChange_None			= 0
			, EFileChange_Recursive		= DMibBit(0)
			, EFileChange_FileName		= DMibBit(1)
			, EFileChange_DirectoryName	= DMibBit(2)
			, EFileChange_Attributes		= DMibBit(3)
			, EFileChange_FileSize		= DMibBit(4)
			, EFileChange_Write			= DMibBit(5)
			, EFileChange_Security		= DMibBit(6)
			, EFileChange_All = DMibBitRange(0, 6)
		};

		enum EFileChangeNotification
		{
			 EFileChangeNotification_Undefined		= 0
			 , EFileChangeNotification_Unknown
			 , EFileChangeNotification_Added		
			 , EFileChangeNotification_Removed
			 , EFileChangeNotification_Modified
			 , EFileChangeNotification_Renamed
		};

		enum EFileRight
		{
			EFileRight_None		= 0
			, EFileRight_Read		= DMibBit(0)
			, EFileRight_Write	= DMibBit(1)
			, EFileRight_Execute = DMibBit(2)
		};

		enum ECheckFileRights
		{
			ECheckFileRights_NoAccess = 0
			, ECheckFileRights_Access
			, ECheckFileRights_DoesNotExist
		};

	}
}

#include <Mib/File/File>
#include <Mib/Network/Address>
#include <Mib/Network/PlatformSocket>

#	include "../../Core/Source/Malterlib_Core_Misc.hpp"

#	undef DMibSafety_IncMalterlib_H

