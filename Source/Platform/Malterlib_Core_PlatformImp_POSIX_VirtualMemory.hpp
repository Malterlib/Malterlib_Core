// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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

#include "Malterlib_Core_PlatformImp_POSIX.h"

#include <Mib/Core/PlatformSpecific/PosixErrNo>

// *************************************************************************************************************************
// POSIX virtual memory allocation
// *************************************************************************************************************************

#define DMibOSX_UseMadvise

namespace NMib::NSys::NPrivate
{
	extern mint g_PageSize;
	constinit NAtomic::TCAtomic<mint> g_ForceMmapSequence = 0;
}

void *fg_AllocVirtualMemory(mint &_Size, ENumaNode _NumaNode, mint _Alignment, EAllocationFlag _Flags)
{
	int Protection = PROT_READ | PROT_WRITE;
#ifdef DPlatformFamily_OSX
#ifdef DMibOSX_UseMadvise
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
#ifdef DPlatformFamily_OSX
#	ifdef DMibOSX_UseMadvise
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
			NSys::NPrivate::g_ForceMmapSequence.f_FetchAdd(1);
			return _pMemory;
		}
	;

	int Tag = 244;

	if (_Flags & EAllocationFlag_MainHeap)
		Tag = 240;
	if (_Flags & EAllocationFlag_NonTrackedMainHeap)
		Tag = 250;

#ifdef DPlatformFamily_OSX
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

		if (!((mint)pAddress & (_Alignment - 1)))
		{
			if (!(_Flags & EAllocationFlag_WillFreeWithSize))
			{
				DMibFastCheck(g_bCreatingSystemDone);
				DMibLock(g_VirtualMapLock);
				(*g_VirtualMap)[((mint)pAddress)>>12] = _Size;
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
			mint AllocSize = _Size + _Alignment - NMib::NSys::NPrivate::g_PageSize;
			pAddress = (uint8 *)mmap(nullptr, AllocSize, Protection, MapOptions, DVmTag(Tag + 1), 0);

			if (!pAddress || pAddress == MAP_FAILED)
				fl_ReportError(errno);

			uint8 *pEndAlignment = pAddress + AllocSize;

			uint8 *pStartAddress = fg_AlignUp(pAddress, _Alignment);
			uint8 *pEndAddress = pStartAddress + _Size;
			mint ToFreeStart = pStartAddress - pAddress;
			if (ToFreeStart)
			{
				if (munmap(pAddress, ToFreeStart))
				{
					DMibTraceSafe("munmap failed: {}" DMibNewLine, NMib::NPlatform::fg_FormatErrno("munmap", errno));
					DMibPDebugBreak; // Should not fail
				}
			}
			mint ToFreeEnd = pEndAlignment - pEndAddress;
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
				(*g_VirtualMap)[((mint)pStartAddress)>>12] = _Size;
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
			(*g_VirtualMap)[((mint)pAddress)>>12] = _Size;
		}

		return fSetUncommited(pAddress);
	}
}

void *NSys::fg_Mem_VirtualAlloc(mint &_Size, EAllocationFlag _AllocFlags, ENumaNode _NumaNode, mint _Alignment)
{
	return fg_AllocVirtualMemory(_Size, _NumaNode, _Alignment, _AllocFlags);
}

void NSys::fg_Mem_VirtualProtect(void *_pMem, mint _Size, uaint _Protect)
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

void NSys::fg_Mem_VirtualCommit(void *_pMem, mint _Size)
{
#ifdef DPlatformFamily_Emscripten
	// Nop
#else

	auto pMemStart = fg_AlignDown((uint8 *)_pMem, NMib::NSys::NPrivate::g_PageSize);
	auto pMemEnd = fg_AlignUp((uint8 *)_pMem + _Size, NMib::NSys::NPrivate::g_PageSize);

#if defined(DPlatformFamily_OSX)
#ifdef DMibOSX_UseMadvise
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

#ifdef DPlatformFamily_OSX
		if (CSystem::ms_PlatformVersion >= 10'06'00 || ErrNo != EINVAL) // This is a bug in OSX...
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

void NSys::fg_Mem_VirtualDecommit(void *_pMem, mint _Size)
{
#ifdef DPlatformFamily_Emscripten
	// Nop
#else
	auto pMemStart = fg_AlignDown((uint8 *)_pMem, NMib::NSys::NPrivate::g_PageSize);
	auto pMemEnd = fg_AlignUp((uint8 *)_pMem + _Size, NMib::NSys::NPrivate::g_PageSize);

#if defined(DPlatformFamily_OSX)
#ifdef DMibOSX_UseMadvise
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

#ifdef MADV_FREE
	int Advice = MADV_FREE;
#else
	int Advice = MADV_DONTNEED;
#endif

	if (madvise(pMemStart, pMemEnd - pMemStart, Advice))
	{
		int ErrNo = errno;
#ifdef DPlatformFamily_OSX
		if (CSystem::ms_PlatformVersion >= 10'06'00 || ErrNo != EINVAL) // This is a bug in OSX...
#endif
		{
			DMibErrorMemory(NPlatform::fg_FormatErrno("madvise (virtual decommit)", ErrNo));
		}
	}
#endif
#endif
}

extern bool g_bSysDeleted;
void NSys::fg_Mem_VirtualFree(void *_pMem, mint _Size)
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
			auto pSize = (*g_VirtualMap).f_FindEqual(((mint)_pMem)>>12);
			if (pSize)
			{
				//DMibDTraceSafe("pSize found\n", 0);
				_Size = *pSize;
				NSys::NPrivate::g_ForceMmapSequence.f_FetchAdd(1);
				if (munmap(pMemStart, _Size))
				{
					int ErrNo = errno;
					DMibErrorMemory(NPlatform::fg_FormatErrno(CFStr256::CFormat("munmap({}, {}) when freeing virtual memory") << _pMem << _Size, ErrNo));
				}
				else if (!g_bSysDeleted)
				{
					DMibFastCheck(g_bCreatingSystemDone);
					(*g_VirtualMap).f_Remove(((mint)_pMem)>>12);
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
	NSys::NPrivate::g_ForceMmapSequence.f_FetchAdd(1);
	if (munmap(pMemStart, _Size))
	{
		int ErrNo = errno;
		DMibErrorMemory(NPlatform::fg_FormatErrno(CFStr256::CFormat("munmap({}, {}) when freeing virtual memory") << _pMem << _Size, ErrNo));
	}
}

void *NSys::fg_Mem_VirtualRealloc(void *_pMem, mint &_Size, mint _OldSize, EAllocationFlag _AllocFlags, ENumaNode _NumaNode)
{
    fg_Mem_VirtualFree(_pMem, _OldSize);
	return fg_Mem_VirtualAlloc(_Size, _AllocFlags, _NumaNode);
}

void *NSys::fg_Mem_VirtualResize(void *_pMem, mint &_Size, mint _OldSize, EAllocationFlag _AllocFlags, ENumaNode _NumaNode)
{
	void *pNewMem = fg_Mem_VirtualAlloc(_Size, _AllocFlags, _NumaNode);
	fg_MemCopy(pNewMem, _pMem, fg_Min(_Size, _OldSize));
    fg_Mem_VirtualFree(_pMem, _OldSize);
	return pNewMem;
}

mint NSys::fg_Mem_VirtualSize(const void *_pMem)
{
	auto *pSys = fg_GetSys_POSIX();
	if (pSys)
	{
		{
			DMibLock(g_VirtualMapLock);
			mint *pFind = (*g_VirtualMap).f_FindEqual(((mint)_pMem)>>12);
			if (pFind)
				return *pFind;
		}
	}
	return 0;
}

mint NSys::fg_Mem_VirtualTrySize(const void *_pMem)
{
	auto *pSys = fg_GetSys_POSIX();
	if (pSys)
	{
		{
			DMibLock(g_VirtualMapLock);
			mint *pFind = (*g_VirtualMap).f_FindEqual(((mint)_pMem)>>12);
			if (pFind)
				return *pFind;
		}
	}
	return 0;
}

mint NSys::fg_Mem_PageSize()
{
	return sysconf(_SC_PAGE_SIZE);
}
