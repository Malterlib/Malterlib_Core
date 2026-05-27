// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

using namespace NMib;

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mman.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#include <uuid/uuid.h>
#include <poll.h>

#include <sys/param.h>

#ifdef DPlatformFamily_macOS
	#include <mach/mach.h>
	#include <mach/mach_vm.h>
#endif

#include "Malterlib_Core_PlatformImp_POSIX.h"

#include <Mib/Core/PlatformSpecific/PosixErrNo>

// *************************************************************************************************************************
// POSIX virtual memory allocation
// *************************************************************************************************************************

#define DMibMacOS_UseMadvise

namespace NMib::NSys::NPrivate
{
	extern umint g_PageSize;
}

void *fg_AllocVirtualMemory(umint &_Size, ENumaNode _NumaNode, umint _Alignment, EAllocationFlag _Flags)
{
	int Protection = PROT_READ | PROT_WRITE;
#ifdef DPlatformFamily_macOS
#ifdef DMibMacOS_UseMadvise
	if ((_Flags & EAllocationFlag_NoCommit) && CSystem::ms_PlatformVersion < 10'06'00)
		Protection = PROT_NONE;
#else
	if (_Flags & EAllocationFlag_NoCommit)
		Protection = PROT_NONE;
#endif
#endif

	auto fl_ReportError
		= [&](int ErrNo)
		{
			if (ErrNo == ENOMEM)
				DMibErrorMemory("The OS returned an error from mmap: Out of memory (ENOMEM)");

			DMibErrorMemory(NMib::NPlatform::fg_FormatErrno("mmap (alloc virtual memory)", ErrNo));
		}
	;

	auto fSetUncommited = [&](void *_pMemory)
		{
#ifdef DPlatformFamily_macOS
#	ifdef DMibMacOS_UseMadvise
/*			if ((_Flags & EAllocationFlag_NoCommit) && CSystem::ms_PlatformVersion >= 10'06'00)
			{
				if (madvise(_pMemory, _Size, MADV_FREE_REUSABLE))
				{
					int ErrNo = errno;
					DMibErrorMemory(NMib::NPlatform::fg_FormatErrno("madvise (virtual decommit)", ErrNo));
				}
			}*/
#	endif
#elif defined(DPlatformFamily_Linux)
#	if DMibConfig_MemoryManager_HugePage_Enable
			madvise(_pMemory, _Size, MADV_HUGEPAGE);
#	else
			madvise(_pMemory, _Size, MADV_NOHUGEPAGE);
#	endif
			if ((_Flags & EAllocationFlag_NoCommit))
				madvise(_pMemory, _Size, MADV_DONTNEED);
#endif
			g_ForceMmapSequence.f_FetchAdd(1);
			return _pMemory;
		}
	;

#ifdef DPlatformFamily_macOS
	int Tag = 244;

	if (_Flags & EAllocationFlag_MainHeap)
		Tag = 240;
	if (_Flags & EAllocationFlag_NonTrackedMainHeap)
		Tag = 250;
#	define DVmTag(d_Tag) VM_MAKE_TAG(d_Tag)
#else
#	define DVmTag(d_Tag) 0
#endif

	int MapOptions = MAP_ANON | MAP_PRIVATE | MAP_NORESERVE;

	_Size = fg_AlignUp(_Size, NMib::NSys::NPrivate::g_PageSize);
	if (_Alignment > NMib::NSys::NPrivate::g_PageSize)
	{
		// First try anon with exact size
		_Size = fg_AlignUp(_Size, _Alignment);

		uint8 *pAddress = (uint8 *)mmap(nullptr, _Size, Protection, MapOptions, DVmTag(Tag), 0);

		if (!pAddress || pAddress == MAP_FAILED)
			fl_ReportError(errno);

		if (!((umint)pAddress & (_Alignment - 1)))
		{
			if (!(_Flags & EAllocationFlag_WillFreeWithSize))
			{
				DMibFastCheck(g_bCreatingSystemDone);
				DMibLock(g_VirtualMapLock);
				(*g_VirtualMap)[((umint)pAddress)>>12] = _Size;
			}
			return fSetUncommited(pAddress);
		}
		else
		{
			if (munmap(pAddress, _Size))
			{
				DMibTraceSafe("munmap failed: {}" DMibNewLine, NMib::NPlatform::fg_FormatErrno("munmap", errno));
				DMibPDebugBreak; // Should not fail
			}
			umint AllocSize = _Size + _Alignment - NMib::NSys::NPrivate::g_PageSize;
			pAddress = (uint8 *)mmap(nullptr, AllocSize, Protection, MapOptions, DVmTag(Tag + 1), 0);

			if (!pAddress || pAddress == MAP_FAILED)
				fl_ReportError(errno);

			uint8 *pEndAlignment = pAddress + AllocSize;

			uint8 *pStartAddress = fg_AlignUp(pAddress, _Alignment);
			uint8 *pEndAddress = pStartAddress + _Size;
			umint ToFreeStart = pStartAddress - pAddress;
			if (ToFreeStart)
			{
				if (munmap(pAddress, ToFreeStart))
				{
					DMibTraceSafe("munmap failed: {}" DMibNewLine, NMib::NPlatform::fg_FormatErrno("munmap", errno));
					DMibPDebugBreak; // Should not fail
				}
			}
			umint ToFreeEnd = pEndAlignment - pEndAddress;
			if (ToFreeEnd)
			{
				if (munmap(pEndAddress, ToFreeEnd))
				{
					DMibTraceSafe("munmap failed: {}" DMibNewLine, NMib::NPlatform::fg_FormatErrno("munmap", errno));
					DMibPDebugBreak; // Should not fail
				}
			}
			if (!(_Flags & EAllocationFlag_WillFreeWithSize))
			{
				DMibFastCheck(g_bCreatingSystemDone);
				DMibLock(g_VirtualMapLock);
				(*g_VirtualMap)[((umint)pStartAddress)>>12] = _Size;
			}
			return fSetUncommited(pStartAddress);
		}
	}
	else
	{
		void *pAddress = mmap(nullptr, _Size, Protection, MapOptions, DVmTag(Tag + 2), 0);

		if (!pAddress || pAddress == MAP_FAILED)
			fl_ReportError(errno);

		if (!(_Flags & EAllocationFlag_WillFreeWithSize))
		{
			DMibFastCheck(g_bCreatingSystemDone);
			DMibLock(g_VirtualMapLock);
			(*g_VirtualMap)[((umint)pAddress)>>12] = _Size;
		}

		return fSetUncommited(pAddress);
	}
}

void *NSys::fg_Mem_VirtualAlloc(umint &_Size, EAllocationFlag _AllocFlags, ENumaNode _NumaNode, umint _Alignment)
{
	return fg_AllocVirtualMemory(_Size, _NumaNode, _Alignment, _AllocFlags);
}

void NSys::fg_Mem_VirtualProtect(void *_pMem, umint _Size, uaint _Protect)
{
	int Protection =
		( (_Protect & EProtect_Read) ? PROT_READ : 0 )
		| ( (_Protect & EProtect_Write) ? PROT_WRITE : 0 )
		| ( (_Protect & EProtect_Exec) ? PROT_EXEC : 0 )
	;

	auto pMemStart = fg_AlignDown((uint8 *)_pMem, NMib::NSys::NPrivate::g_PageSize);
	auto pMemEnd = fg_AlignUp((uint8 *)_pMem + _Size, NMib::NSys::NPrivate::g_PageSize);

	if (mprotect(pMemStart, pMemEnd - pMemStart, Protection))
	{
		int ErrNo = errno;
		DMibErrorMemory(NPlatform::fg_FormatErrno("mprotect (virtual protect)", ErrNo));
	}
}

uaint NSys::fg_Mem_VirtualGetProtect(void const *_pMem)
{
#ifdef DPlatformFamily_macOS
	mach_vm_address_t QueryAddress = mach_vm_address_t(_pMem);
	mach_vm_address_t Address = QueryAddress;
	mach_vm_size_t Size = 0;
	vm_region_basic_info_data_64_t Info;
	mach_msg_type_number_t Count = VM_REGION_BASIC_INFO_COUNT_64;
	mach_port_t ObjectName = MACH_PORT_NULL;
	kern_return_t Result = mach_vm_region
		(
			mach_task_self()
			, &Address
			, &Size
			, VM_REGION_BASIC_INFO_64
			, reinterpret_cast<vm_region_info_t>(&Info)
			, &Count
			, &ObjectName
		)
	;
	if (ObjectName != MACH_PORT_NULL)
		mach_port_deallocate(mach_task_self(), ObjectName);

	if (Result != KERN_SUCCESS)
		return 0;
	if (QueryAddress < Address || QueryAddress - Address >= Size)
		return 0;

	return
		((Info.protection & VM_PROT_READ) ? EProtect_Read : 0)
		| ((Info.protection & VM_PROT_WRITE) ? EProtect_Write : 0)
		| ((Info.protection & VM_PROT_EXECUTE) ? EProtect_Exec : 0)
	;
#else
	NContainer::TCVector<ch8, NMemory::CAllocator_NonTrackedHeap> FileData;
	try
	{
		FileData = NPlatform::fg_ReadProcFSNonTracked("/proc/self/maps");
	}
	catch (NMib::NFile::CExceptionFile const &)
	{
		return EProtect_All;
	}

	umint Target = umint(_pMem);
	ch8 const *pIterator = FileData.f_GetArray();
	ch8 const *pEnd = pIterator + FileData.f_GetLen();
	while (pIterator < pEnd)
	{
		ch8 const *pLineEnd = pIterator;
		while (pLineEnd < pEnd && *pLineEnd != '\n' && *pLineEnd != 0)
			++pLineEnd;

		ch8 const *pLineIterator = pIterator;
		umint Start = NStr::fg_StrToIntParseHexNoSign(pLineIterator, pLineEnd - pLineIterator, umint(0));
		if (pLineIterator == pIterator || pLineIterator >= pLineEnd || *pLineIterator != '-')
		{
			pIterator = pLineEnd + 1;
			continue;
		}
		++pLineIterator;
		ch8 const *pEndStart = pLineIterator;
		umint End = NStr::fg_StrToIntParseHexNoSign(pLineIterator, pLineEnd - pLineIterator, umint(0));
		if (pLineIterator == pEndStart || pLineIterator >= pLineEnd || *pLineIterator != ' ')
		{
			pIterator = pLineEnd + 1;
			continue;
		}
		++pLineIterator;
		if (pLineIterator + 2 >= pLineEnd)
		{
			pIterator = pLineEnd + 1;
			continue;
		}

		if (Target < Start || Target >= End)
		{
			pIterator = pLineEnd + 1;
			continue;
		}

		return
			((pLineIterator[0] == 'r') ? EProtect_Read : 0)
			| ((pLineIterator[1] == 'w') ? EProtect_Write : 0)
			| ((pLineIterator[2] == 'x') ? EProtect_Exec : 0)
		;

		pIterator = pLineEnd + 1;
	}

	return 0;
#endif
}

void NSys::fg_Mem_VirtualCommit(void *_pMem, umint _Size)
{
#ifdef DPlatformFamily_Emscripten
	// Nop
#else

	auto pMemStart = fg_AlignDown((uint8 *)_pMem, NMib::NSys::NPrivate::g_PageSize);
	auto pMemEnd = fg_AlignUp((uint8 *)_pMem + _Size, NMib::NSys::NPrivate::g_PageSize);

#if defined(DPlatformFamily_macOS)
#ifdef DMibMacOS_UseMadvise
	if (CSystem::ms_PlatformVersion >= 10'06'00)
	{
		if (madvise(pMemStart, pMemEnd - pMemStart, MADV_FREE_REUSE))
		{
			int ErrNo = errno;
			DMibErrorMemory(NPlatform::fg_FormatErrno("madvise (virtual decommit)", ErrNo));
		}
		return;
	}
#endif
	if (mprotect(pMemStart, pMemEnd - pMemStart, PROT_READ | PROT_WRITE))
	{
		int ErrNo = errno;
		{
			DMibErrorMemory(NPlatform::fg_FormatErrno("mprotect (virtual commit)", ErrNo));
		}
	}
#else

	if (madvise(pMemStart, pMemEnd - pMemStart, MADV_WILLNEED))
	{
		int ErrNo = errno;

#ifdef DPlatformFamily_macOS
		if (CSystem::ms_PlatformVersion >= 10'06'00 || ErrNo != EINVAL) // This is a bug in macOS...
#endif
#ifdef DPlatformFamily_Linux
		if (ErrNo != EBADF) // Seems that this does not work on Linux because it only works on memory mapped files?
#endif
		{
			DMibErrorMemory(NPlatform::fg_FormatErrno("madvise (virtual commit)", ErrNo));
		}
	}
#endif
#endif
}

void NSys::fg_Mem_VirtualDecommit(void *_pMem, umint _Size)
{
#ifdef DPlatformFamily_Emscripten
	// Nop
#else
	auto pMemStart = fg_AlignDown((uint8 *)_pMem, NMib::NSys::NPrivate::g_PageSize);
	auto pMemEnd = fg_AlignUp((uint8 *)_pMem + _Size, NMib::NSys::NPrivate::g_PageSize);

#if defined(DPlatformFamily_macOS)
#ifdef DMibMacOS_UseMadvise
	if (CSystem::ms_PlatformVersion >= 10'06'00)
	{
		if (madvise(pMemStart, pMemEnd - pMemStart, MADV_FREE_REUSABLE))
		{
			int ErrNo = errno;
			DMibErrorMemory(NPlatform::fg_FormatErrno("madvise (virtual decommit)", ErrNo));
		}
		return;
	}
#endif
	if (mprotect(pMemStart, pMemEnd - pMemStart, PROT_NONE))
	{
		int ErrNo = errno;
		{
			DMibErrorMemory(NPlatform::fg_FormatErrno("mprotect (virtual decommit)", ErrNo));
		}
	}
#else

	int Advice = MADV_FREE;
#ifdef DPlatformFamily_Linux
	if (NMib::CSystem::ms_PlatformVersion < 4'005'000)
		Advice = MADV_DONTNEED;
#endif

	if (madvise(pMemStart, pMemEnd - pMemStart, Advice))
	{
		int ErrNo = errno;
#ifdef DPlatformFamily_macOS
		if (CSystem::ms_PlatformVersion >= 10'06'00 || ErrNo != EINVAL) // This is a bug in macOS...
#endif
		{
			DMibErrorMemory(NPlatform::fg_FormatErrno("madvise (virtual decommit)", ErrNo));
		}
	}
#endif
#endif
}

extern bool g_bSysDeleted;
inline_never void NSys::fg_Mem_VirtualFree(void *_pMem, umint _Size)
{
	auto pMemStart = fg_AlignDown((uint8 *)_pMem, NMib::NSys::NPrivate::g_PageSize);
	if (_Size == 0)
	{
		//DMibDTraceSafe("Size is 0\n", 0);
		auto *pSys = fg_GetSys_POSIX();
		if (pSys)
		{
			//DMibDTraceSafe("Sys exists\n", 0);
			DMibLock(g_VirtualMapLock);
			auto pSize = (*g_VirtualMap).f_FindEqual(((umint)_pMem)>>12);
			if (pSize)
			{
				//DMibDTraceSafe("pSize found\n", 0);
				_Size = *pSize;
				g_ForceMmapSequence.f_FetchAdd(1);
				if (munmap(pMemStart, _Size))
				{
					int ErrNo = errno;
					DMibErrorMemory(NPlatform::fg_FormatErrno(CFStr256::CFormat("munmap({}, {}) when freeing virtual memory") << _pMem << _Size, ErrNo));
				}
				else if (!g_bSysDeleted)
				{
					DMibFastCheck(g_bCreatingSystemDone);
					(*g_VirtualMap).f_Remove(((umint)_pMem)>>12);
				}
				return;
			}
			else
			{
				//DMibDTraceSafe("pSize not found\n", 0);
			}
		}
	}
	auto pMemEnd = fg_AlignUp((uint8 *)_pMem + _Size, NMib::NSys::NPrivate::g_PageSize);
	_Size = pMemEnd - pMemStart;
	g_ForceMmapSequence.f_FetchAdd(1);
	if (munmap(pMemStart, _Size))
	{
		int ErrNo = errno;
		DMibErrorMemory(NPlatform::fg_FormatErrno(CFStr256::CFormat("munmap({}, {}) when freeing virtual memory") << _pMem << _Size, ErrNo));
	}
}

void *NSys::fg_Mem_VirtualRealloc(void *_pMem, umint &_Size, umint _OldSize, EAllocationFlag _AllocFlags, ENumaNode _NumaNode)
{
    fg_Mem_VirtualFree(_pMem, _OldSize);
	return fg_Mem_VirtualAlloc(_Size, _AllocFlags, _NumaNode);
}

void *NSys::fg_Mem_VirtualResize(void *_pMem, umint &_Size, umint _OldSize, EAllocationFlag _AllocFlags, ENumaNode _NumaNode)
{
	void *pNewMem = fg_Mem_VirtualAlloc(_Size, _AllocFlags, _NumaNode);
	fg_MemCopy(pNewMem, _pMem, fg_Min(_Size, _OldSize));
    fg_Mem_VirtualFree(_pMem, _OldSize);
	return pNewMem;
}

umint NSys::fg_Mem_VirtualSize(const void *_pMem)
{
	auto *pSys = fg_GetSys_POSIX();
	if (pSys)
	{
		{
			DMibLock(g_VirtualMapLock);
			umint *pFind = (*g_VirtualMap).f_FindEqual(((umint)_pMem)>>12);
			if (pFind)
				return *pFind;
		}
	}
	return 0;
}

umint NSys::fg_Mem_VirtualTrySize(const void *_pMem)
{
	auto *pSys = fg_GetSys_POSIX();
	if (pSys)
	{
		{
			DMibLock(g_VirtualMapLock);
			umint *pFind = (*g_VirtualMap).f_FindEqual(((umint)_pMem)>>12);
			if (pFind)
				return *pFind;
		}
	}
	return 0;
}

umint NSys::fg_Mem_PageSize()
{
	return sysconf(_SC_PAGE_SIZE);
}
