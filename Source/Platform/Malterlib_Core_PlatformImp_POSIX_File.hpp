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
#ifdef DPlatformFamily_macOS
#	include <sys/attr.h>
#	include <sys/mount.h>
#endif
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/statvfs.h>
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
#include "Malterlib_Core_Platform_Linux_ProcFS.h"

#include <Mib/Core/PlatformSpecific/PosixErrNo>
#include <Mib/Core/PlatformSpecific/PosixUser>

// *************************************************************************************************************************
// POSIX File Implementation
// *************************************************************************************************************************

namespace
{
	template <typename tf_CStr>
	auto fg_ConvertToPOSIXPath(tf_CStr const &_Path, bool _bAddCurrentDir = true)
		-> TCEnableIf<sizeof(typename tf_CStr::CChar) == 1, tf_CStr>
		requires (sizeof(typename tf_CStr::CChar) == 1) // Incorrect string type
	{
		return NFile::CFile::fs_GetExpandedPath(_Path, _bAddCurrentDir);
	}
}

template <typename t_CStats>
static NMib::NFile::EFileAttrib fsg_StatsToAttribs(t_CStats &_Stats);

namespace NMib
{
	namespace NSys
	{
		namespace NFile
		{
			template <typename tf_CStr>
			bool fg_FileExistsGeneral(const tf_CStr &_FileName, uint32 _AttribMask)
			{
				tf_CStr File = fg_ConvertToPOSIXPath(_FileName);
				struct stat Stats;

				if (lstat(File.f_GetStr(), &Stats))
				{
					int Error = errno;
					if
						(
							Error == ENOENT				// A component of _Filename does not exist (or _Filename is empty)
							|| Error == ENOTDIR			// A component of the path prefix is not a dir.
							|| Error == EACCES			// No access (permissions)
							|| Error == ENAMETOOLONG	// Path is too long
							|| Error == ENOMEM			// Ran out of kernel memory
							|| Error == EINVAL			// Seen this happen os MacOS if you delete a symlink in the parent path while checking stats
							|| Error == ESRCH			// Can happen on Linux when checking procfs
						)
					{
						return false;
					}
					else
					{
						// This will most likely be EFAULT (&Stats is invalid)
						DMibErrorFile(NPlatform::fg_FormatErrno<tf_CStr>(typename tf_CStr::CFormat("stat('{}') when checking if file exists") << _FileName, Error));
					}
				}

				NMib::NFile::EFileAttrib LinkAttribs = fsg_StatsToAttribs(Stats);

				if (!(LinkAttribs & NMib::NFile::EFileAttrib_Link))
					return (LinkAttribs & _AttribMask) != NMib::NFile::EFileAttrib_None;

				NMib::NFile::EFileAttrib Attribs = NMib::NFile::EFileAttrib_None;
				if (stat(File.f_GetStr(), &Stats))
				{
					int Error = errno;
					if (Error == ELOOP) // Too many Symbolic links encountered.
					{
						if
							(
								(_AttribMask & NMib::NFile::EFileAttrib_Link)
								|| (_AttribMask & (NMib::NFile::EFileAttrib_File|NMib::NFile::EFileAttrib_Directory)) == (NMib::NFile::EFileAttrib_File|NMib::NFile::EFileAttrib_Directory)
							)
						{
							return true;
						}
						else
							return false;
					}

					// If this is a link that we don't know what it's pointing on assume file.
					if (LinkAttribs & NMib::NFile::EFileAttrib_Link)
						LinkAttribs |= NMib::NFile::EFileAttrib_File;
					Attribs |= LinkAttribs;
				}
				else
					Attribs = fsg_StatsToAttribs(Stats) | NMib::NFile::EFileAttrib_Link;

				return (Attribs & _AttribMask) != NMib::NFile::EFileAttrib_None;
			}
		}
	}
}

bool NSys::NFile::fg_FileExists(const NMib::NStr::CStr &_FileName, NMib::NFile::EFileAttrib _AttribMask)
{
	return fg_FileExistsGeneral(CStr(_FileName), _AttribMask);
}

bool NSys::NFile::fg_FileExists(const NMib::NStr::CStrNonTracked &_FileName, NMib::NFile::EFileAttrib _AttribMask)
{
	return fg_FileExistsGeneral(_FileName, _AttribMask);
}

NMib::NStream::CFilePos NSys::NFile::fg_GetFreeSpace(const NMib::NStr::CStr &_Path)
{
	CStr Path = _Path;
	struct statvfs Stats;
	if (statvfs(Path, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("statvfs('{}')") << _Path, errno));

	return NMib::NStream::CFilePos(Stats.f_frsize) * NMib::NStream::CFilePos(Stats.f_bavail);
}

NMib::NStream::CFilePos NSys::NFile::fg_GetUsedSpace(const NMib::NStr::CStr &_Path)
{
	CStr Path = _Path;
	struct statvfs Stats;
	if (statvfs(Path, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("statvfs('{}')") << _Path, errno));

	return NMib::NStream::CFilePos(Stats.f_frsize) * NMib::NStream::CFilePos(Stats.f_blocks - Stats.f_bfree);
}

NMib::NStream::CFilePos NSys::NFile::fg_GetTotalSpace(const NMib::NStr::CStr &_Path)
{
	CStr Path = _Path;
	struct statvfs Stats;
	if (statvfs(Path, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("statvfs('{}')") << _Path, errno));

	return NMib::NStream::CFilePos(Stats.f_frsize) * NMib::NStream::CFilePos(Stats.f_blocks);
}

namespace
{
#ifdef DPlatformFamily_Linux
	NStr::CStr fg_ParseOctalCoded(NStr::CStr const &_String)
	{
		NStr::CStr Return;

		for (ch8 const *pParse = _String.f_GetStr(); *pParse;)
		{
			if (pParse[0] == '\\' && fg_CharIsNumber(pParse[1]))
			{
				++pParse;
				CStr OctalNumber;
				OctalNumber.f_AddChar(*pParse);
				++pParse;
				if (fg_CharIsNumber(*pParse))
				{
					OctalNumber.f_AddChar(*pParse);
					++pParse;
				}
				if (fg_CharIsNumber(*pParse))
				{
					OctalNumber.f_AddChar(*pParse);
					++pParse;
				}

				ch8 const *pParseOctal = OctalNumber.f_GetStr();
				Return.f_AddChar(fg_StrToIntParse(pParseOctal, 32, (ch8 const *)nullptr, false, EStrToIntParseMode_Octal));
				continue;
			}

			Return.f_AddChar(*pParse);
			++pParse;
		}

		return Return;
	}
#endif
}

NContainer::TCVector<NStr::CStr> NSys::NFile::fg_GetMounts(NMib::NFile::EFileMountType _Types)
{
#ifdef DPlatformFamily_Linux
	NContainer::TCVector<NMib::NStr::CStr> Return;

	NContainer::TCVector<ch8> FileData = NPlatform::fg_ReadProcFS("/proc/self/mountinfo");

	auto pParse = FileData.f_GetArray();

	NContainer::TCSet<CStr> KnownDeviceIDs;

	while (*pParse)
	{
		auto *pStart = pParse;
		fg_ParseToEndOfLine(pParse);
		CStr Str(pStart, pParse - pStart);
		fg_ParseEndOfLine(pParse);
		auto Components = Str.f_Split(" ");

		if (Components.f_GetLen() < 10)
			continue;

		CStr DeviceID = fg_ParseOctalCoded(Components[2]);
		CStr MountPath = fg_ParseOctalCoded(Components[4]);
		CStr SourcePath = fg_ParseOctalCoded(Components[9]);

		CStr Remote = "Remote";
		if (SourcePath.f_FindChar(':') >= 0)
		{
			if (!(_Types & NMib::NFile::EFileMountType_Remote))
				continue;
		}
		else
		{
			if (!(_Types & NMib::NFile::EFileMountType_Local))
				continue;
			Remote = "Local";
		}

		uint64 UsedSpace = 0;
		try
		{
			UsedSpace = fg_GetUsedSpace(MountPath);
		}
		catch (NMib::NFile::CExceptionFile const &)
		{
		}

		CStr Block = "Block";
		if (UsedSpace > 0 && SourcePath != "tmpfs" && (!DeviceID || !KnownDeviceIDs.f_FindEqual(DeviceID)))
		{
			if (!(_Types & NMib::NFile::EFileMountType_Block))
				continue;
		}
		else
		{
			if (!(_Types & NMib::NFile::EFileMountType_Special))
				continue;;
			Block = "Special";
		}

		if (DeviceID)
			KnownDeviceIDs[DeviceID];

		Return.f_Insert(fg_Move(MountPath));
	}

	return Return;
#else
	int nMounts = getfsstat(nullptr, 0, MNT_NOWAIT);

	if (nMounts < 0)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("getfsstat(get count)"), errno));

	NContainer::TCVector<struct statfs> SourceMounts;
	SourceMounts.f_SetLen(nMounts);
	nMounts = getfsstat(SourceMounts.f_GetArray(), nMounts * sizeof(struct statfs), MNT_NOWAIT);
	if (nMounts < 0)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("getfsstat(get)"), errno));

	SourceMounts.f_SetLen(nMounts);

	NMib::NContainer::TCVector<NMib::NStr::CStr> Return;
	for (auto &Mount : SourceMounts)
	{
		CStr MountLocation = Mount.f_mntonname;
		CStr MountSource = Mount.f_mntfromname;

		if (Mount.f_flags & MNT_LOCAL)
		{
			if (!(_Types & NMib::NFile::EFileMountType_Local))
				continue;
		}
		else
		{
			if (!(_Types & NMib::NFile::EFileMountType_Remote))
				continue;
		}

		CStr FsType = Mount.f_fstypename;

		if (Mount.f_blocks && FsType != "devfs" && FsType != "autofs")
		{
			if (!(_Types & NMib::NFile::EFileMountType_Block))
				continue;
		}
		else
		{
			if (!(_Types & NMib::NFile::EFileMountType_Special))
				continue;;
		}

		Return.f_Insert(fg_Move(MountLocation));
	}

	return Return;
#endif
}

NMib::NFile::EFileSystemFeature NSys::NFile::fg_GetFileSystemFeatures()
{
	return NMib::NFile::EFileSystemFeature_HasExecuteAttrib;
}

namespace NMib
{
	namespace NSys
	{
		namespace NFile
		{
			template <typename tf_CStr>
			tf_CStr fg_ResolveSymbolicLink(tf_CStr const &_FileFrom)
				requires (tf_CStr::mc_Type == EStrType_UTF && sizeof(typename tf_CStr::CChar) == 1)
			{
				tf_CStr FileFrom = fg_ConvertToPOSIXPath(_FileFrom);
				tf_CStr NewString;
				auto nChars = readlink(FileFrom, NewString.f_GetStr(PATH_MAX + 1), PATH_MAX);
				if (nChars < 0)
				{
					int ErrNo = errno;
					DMibErrorFile(NPlatform::fg_FormatErrno<tf_CStr>(typename tf_CStr::CFormat("readlink('{}') when resolving symbolic link") << _FileFrom, ErrNo));
				}
				NewString.f_SetAt(nChars, 0);
				NewString.f_TrimSize();
				return NewString;
			}
		}
	}
}
NMib::NStr::CStr NSys::NFile::fg_ResolveSymbolicLink(const NMib::NStr::CStr &_FileFrom)
{
	return fg_ResolveSymbolicLink<CStr>(_FileFrom);
}

bool NSys::NFile::fg_CanCreateSymbolicLink(NMib::NFile::EFileAttrib _Type, NMib::NFile::ESymbolicLinkFlag _Flags)
{
	return true;
}

void NSys::NFile::fg_CreateSymbolicLink(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo, NMib::NFile::EFileAttrib _Type, NMib::NFile::ESymbolicLinkFlag _Flags)
{
	CStr FileFrom = _FileFrom;

	CStr FileTo = fg_ConvertToPOSIXPath(_FileTo);

	int ErrNo = symlink(FileFrom.f_GetStr(), FileTo.f_GetStr());

	if (ErrNo)
	{
		ErrNo = errno;
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("symlink('{}', '{}') when creating symbolic link") << _FileFrom << _FileTo, ErrNo));
	}
}

void NSys::NFile::fg_CreateHardLink(const NMib::NStr::CStr &_FileFrom, const NMib::NStr::CStr &_FileTo)
{
	CStr FileFrom = fg_ConvertToPOSIXPath(_FileFrom);
	CStr FileTo = fg_ConvertToPOSIXPath(_FileTo);

	int ErrNo = link(FileFrom.f_GetStr(), FileTo.f_GetStr());

	if (ErrNo)
	{
		ErrNo = errno;
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr(CStr::CFormat("link('{}', '{}') when creating hard link") << _FileFrom << _FileTo), ErrNo));
	}
}

void NSys::NFile::fg_DeleteDirectory(const NMib::NStr::CStr &_File)
{
	if (fg_FileExists(_File, NMib::NFile::EFileAttrib_File))
		DMibErrorFile("Cannot delete directory, it's a file");

	CStr File = fg_ConvertToPOSIXPath(_File);

	int ErrNo = rmdir(File.f_GetStr());

	if (ErrNo)
	{
		ErrNo = errno;
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("rmdir('{}') when deleting directory") << _File, ErrNo));
	}
}

void NSys::NFile::fg_DeleteDirectory(const NMib::NStr::CStrNonTracked &_File)
{
	if (fg_FileExists(_File, NMib::NFile::EFileAttrib_File))
		DMibErrorFile("Cannot delete directory, it's a file");

	CStrNonTracked File = fg_ConvertToPOSIXPath(_File);

	int ErrNo = rmdir(File.f_GetStr());

	if (ErrNo)
	{
		ErrNo = errno;
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("rmdir('{}') when deleting directory") << _File, ErrNo));
	}
}

void NSys::NFile::fg_Delete(const NMib::NStr::CStr &_File)
{
//	if (fg_FileExists(_File, NMib::NFile::EFileAttrib_Directory))
//		DMibErrorFile("Cannot delete directory as file");

	CStr File = fg_ConvertToPOSIXPath(_File);

	int ErrNo = unlink(File.f_GetStr());

	if (ErrNo)
	{
		ErrNo = errno;
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("unlink('{}') when deleting file") << _File, ErrNo));
	}
}

void NSys::NFile::fg_Delete(const NMib::NStr::CStrNonTracked &_File)
{
	if (fg_FileExists(_File, NMib::NFile::EFileAttrib_Directory))
		DMibErrorFile("Cannot delete directory as file");

	CStrNonTracked File = fg_ConvertToPOSIXPath(_File);

	int ErrNo = unlink(File.f_GetStr());

	if (ErrNo)
	{
		ErrNo = errno;
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("unlink('{}') when deleting file") << _File, ErrNo));
	}
}


CStr NSys::NFile::fg_GetCurrentDirectory()
{
	char *pRet = getcwd(nullptr, 0);

	if (!pRet)
		DMibErrorFile(NPlatform::fg_FormatErrno("getcwd when getting current directory", errno));

	auto Cleanup
		= fg_OnScopeExit
		(
			[&]()
			{
				free(pRet);
			}
		)
	;

	CStr Ret = pRet;

	return Ret;
}

NStr::CStrNonTracked NSys::NFile::fg_GetCurrentDirectoryNonTracked()
{
	char *pRet = getcwd(nullptr, 0);

	if (!pRet)
		DMibErrorFile(NPlatform::fg_FormatErrno("getcwd when getting current directory", errno));

	auto Cleanup
		= fg_OnScopeExit
		(
			[&]()
			{
				free(pRet);
			}
		)
	;

	CStrNonTracked Ret = pRet;

	return Ret;
}

void NSys::NFile::fg_SetCurrentDirectory(const NMib::NStr::CStr &_Directory)
{
	CStr Dir = fg_ConvertToPOSIXPath(_Directory);

	if (chdir(Dir))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("chdir('{}') when setting current directory") << _Directory, errno));
}

template <typename tf_CStr>
void fg_CreateDirectoryHelper(const tf_CStr &_FileDirectory)
{
	tf_CStr FileDirectory = fg_ConvertToPOSIXPath(_FileDirectory);

	if (NSys::NFile::fg_FileExistsGeneral(FileDirectory, NMib::NFile::EFileAttrib_Directory))
		return;

	tf_CStr NewPath = NMib::NStr::fg_StrReplaceChar(FileDirectory, '\\', '/');
	tf_CStr CurrentPath;
	if (!NewPath.f_IsEmpty() && NewPath[0] == '/')
	{
		CurrentPath = "/";
		NewPath = NewPath.f_Extract(1);
	}
	while (!NewPath.f_IsEmpty())
	{
		tf_CStr Current;
		Current = NMib::NStr::fg_GetStrSepNoTrim(NewPath, "/");
		CurrentPath += Current;

		if (!NSys::NFile::fg_FileExistsGeneral(CurrentPath, NMib::NFile::EFileAttrib_Directory))
		{
			tf_CStr Canon = CurrentPath;
			if (mkdir(Canon.f_GetStr(), S_IWUSR|S_IXUSR|S_IRUSR | S_IRGRP|S_IWGRP|S_IXGRP | S_IXOTH|S_IROTH))
			{
				int ErrNo = errno;
				if (ErrNo != EEXIST || !NSys::NFile::fg_FileExists(CStr(CurrentPath), NMib::NFile::EFileAttrib_Directory)) // Someone else got inbetween and created the same directory
					DMibErrorFile(NMib::NPlatform::fg_FormatErrno<tf_CStr>(typename tf_CStr::CFormat("mkdir('{}') when creating directory '{}'") << CurrentPath << _FileDirectory,ErrNo));
			}
		}
		CurrentPath += "/";
	}
}

void NSys::NFile::fg_CreateDirectory(const NMib::NStr::CStr &_FileDirectory)
{
	fg_CreateDirectoryHelper(CStr(_FileDirectory));
}

void NSys::NFile::fg_CreateDirectory(const NMib::NStr::CStrNonTracked &_FileDirectory)
{
	fg_CreateDirectoryHelper(_FileDirectory);
}

// *************************************************************************************************************************
// POSIX File Operations Implementation
// *************************************************************************************************************************

class CPOSIXFile
{
public:
	virtual ~CPOSIXFile()
	{
	}
	int m_BSDFile;
	virtual ch8 const *f_GetFileName() = 0;
	virtual void f_Delete() = 0;
};

template <typename t_CStr, typename t_CAllocator>
class TCPOSIXFileImp : public CPOSIXFile
{
	t_CStr m_Str;
public:
	TCPOSIXFileImp(t_CStr const &_Str)
		: m_Str(_Str)
	{
	}
	virtual ch8 const *f_GetFileName() override
	{
		return m_Str;
	}
	virtual void f_Delete() override
	{
		NStorage::TCUniquePointer<TCPOSIXFileImp, t_CAllocator> pPtr = fg_Explicit(this);
	}
};

template <typename t_CStats>
static NMib::NFile::EFileAttrib fsg_StatsToAttribs(t_CStats &_Stats)
{
	NMib::NFile::EFileAttrib MalterlibAttr = NMib::NFile::EFileAttrib_None;
	switch(_Stats.st_mode & S_IFMT)
	{
	case S_IFDIR: // [XSI] directory
		MalterlibAttr |= NMib::NFile::EFileAttrib_Directory;
		break;
	case S_IFLNK: // [XSI] symbolic link
		MalterlibAttr |= NMib::NFile::EFileAttrib_Link;
		break;
	case S_IFBLK: // [XSI] block special
	case S_IFIFO: // [XSI] named pipe (fifo)
	case S_IFCHR: // [XSI] character special
	case S_IFREG: // [XSI] regular
	case S_IFSOCK: // [XSI] socket
		MalterlibAttr |= NMib::NFile::EFileAttrib_File;
		break;
	};

	if (_Stats.st_mode & S_IXUSR)
		MalterlibAttr |= NMib::NFile::EFileAttrib_Executable;

#ifdef DMibPMachKernel
	if (_Stats.st_flags & UF_HIDDEN)
		MalterlibAttr |= NMib::NFile::EFileAttrib_Hidden;
	if (_Stats.st_flags & UF_IMMUTABLE)
		MalterlibAttr |= NMib::NFile::EFileAttrib_ReadOnly;
#endif
#ifdef DMibPLinuxKernel
	// Use user write flag for read only on linux
	if (!(_Stats.st_mode & S_IWUSR))
		MalterlibAttr |= NMib::NFile::EFileAttrib_ReadOnly;
#endif

	if (_Stats.st_mode & S_IRUSR)
		MalterlibAttr |= NMib::NFile::EFileAttrib_UserRead;
	if (_Stats.st_mode & S_IWUSR)
		MalterlibAttr |= NMib::NFile::EFileAttrib_UserWrite;
	if (_Stats.st_mode & S_IXUSR)
		MalterlibAttr |= NMib::NFile::EFileAttrib_UserExecute;

	if (_Stats.st_mode & S_IRGRP)
		MalterlibAttr |= NMib::NFile::EFileAttrib_GroupRead;
	if (_Stats.st_mode & S_IWGRP)
		MalterlibAttr |= NMib::NFile::EFileAttrib_GroupWrite;
	if (_Stats.st_mode & S_IXGRP)
		MalterlibAttr |= NMib::NFile::EFileAttrib_GroupExecute;


	if (_Stats.st_mode & S_IROTH)
		MalterlibAttr |= NMib::NFile::EFileAttrib_EveryoneRead;
	if (_Stats.st_mode & S_IWOTH)
		MalterlibAttr |= NMib::NFile::EFileAttrib_EveryoneWrite;
	if (_Stats.st_mode & S_IXOTH)
		MalterlibAttr |= NMib::NFile::EFileAttrib_EveryoneExecute;

	return MalterlibAttr;
}

NMib::NFile::EFileAttrib NSys::NFile::fg_GetSupportedAttributes()
{
	using namespace NMib::NFile;
	return EFileAttrib_Directory
		| EFileAttrib_Link
#ifdef DMibPMachKernel
		| EFileAttrib_Hidden
#endif
		| EFileAttrib_ReadOnly
		| EFileAttrib_File
		| EFileAttrib_Executable
		| EFileAttrib_UserExecute
		| EFileAttrib_UserRead
		| EFileAttrib_UserWrite
		| EFileAttrib_GroupExecute
		| EFileAttrib_GroupRead
		| EFileAttrib_GroupWrite
		| EFileAttrib_EveryoneExecute
		| EFileAttrib_EveryoneRead
		| EFileAttrib_EveryoneWrite
	;
}

NMib::NFile::EFileAttrib NSys::NFile::fg_GetValidAttributes()
{
	return NMib::NFile::EFileAttrib_UnixAttributesValid;
}

umint NSys::NFile::fg_MaximumPathLength()
{
	return PATH_MAX;
}

namespace
{
	void fg_SetBSDFileAttributes(int _iFile, mode_t DefaultMode, NMib::NFile::EFileAttrib _Attributes, ch8 const *_pFileName);
}
uint32 fg_MalterlibAttributesToMode(NMib::NFile::EFileAttrib _Attributes, uint32 _Mode = 0644);

int fg_GetUnixOpenFlags()
{
	int OpenFlags = 0;
#ifdef DPlatformFamily_macOS
	if (NMib::CSystem::ms_PlatformVersion >= 10'07'00)
		OpenFlags |= O_CLOEXEC;
#elif defined(DPlatformFamily_Linux)
	OpenFlags |= O_CLOEXEC;
#endif
	return OpenFlags;
}

void fg_SetUnixHandleOptions(int _File)
{
#ifdef DPlatformFamily_macOS
	if (NMib::CSystem::ms_PlatformVersion >= 10'07'00)
		return;
#elif defined(DPlatformFamily_Linux)
	return;
#endif

	// Set CloseOnExec so that child processes do not get our open files.
	int FDFlags = fcntl(_File, F_GETFD);
	if (FDFlags != -1)
	{
		FDFlags |= FD_CLOEXEC;

		if (fcntl(_File, F_SETFD, FDFlags) == -1)
		{
			// We let this go deliberately. Nothing overly bad can happen.
		}
	}
	else
	{
		// We let this go deliberately. Nothing overly bad can happen.
	}
}

template <typename tf_CFileStr, bool tf_bException, typename tf_CStr>
int fg_OpenHelperBSDFile(const tf_CStr &_FileName, NMib::NFile::EFileOpen _OpenFlags, tf_CStr &_oPosixFileName, NMib::NFile::EFileAttrib _Attributes)
{
	using namespace NMib::NFile;
	if ((_OpenFlags & (EFileOpen_Read | EFileOpen_Write | EFileOpen_ReadAttribs | EFileOpen_WriteAttribs)) == 0)
	{
		if constexpr (tf_bException)
			DMibErrorFile("Open flags contain neither read or write flags, one of them must be specified");
		else
			return -1;
	}

	if ((_OpenFlags & (EFileOpen_DontCreate | EFileOpen_DontOpenExisting)) == (EFileOpen_DontCreate | EFileOpen_DontOpenExisting))
	{
		if constexpr (tf_bException)
			DMibErrorFile("Conflicting open flags (both don't open existing and don't create)");
		else
			return -1;
	}

	if ((_OpenFlags & (EFileOpen_DontOpenExisting | EFileOpen_Read | EFileOpen_Write)) == (EFileOpen_DontOpenExisting | EFileOpen_Read))
	{
		if constexpr (tf_bException)
			DMibErrorFile("You are trying to open a file that does not exist for read access only, this makes no sense)");
		else
			return -1;
	}

	_oPosixFileName = fg_ConvertToPOSIXPath(_FileName);

	auto & FileName = _oPosixFileName;

	uint32 CreateDisposition = 0;
	enum
	{
		EDisp_CreateNew,
		EDisp_OpenExisting,
		EDisp_TruncateExisting,
		EDisp_OpenAlways,
		EDisp_CreateAlways
	};
	if (_OpenFlags & EFileOpen_DontOpenExisting)
	{
		CreateDisposition = EDisp_CreateNew;
	}
	else if (_OpenFlags & EFileOpen_DontCreate)
	{
		if (_OpenFlags & EFileOpen_DontTruncate)
			CreateDisposition = EDisp_OpenExisting;
		else
			CreateDisposition = EDisp_TruncateExisting;
	}
	else
	{
		if (_OpenFlags & EFileOpen_Write)
		{
			if (_OpenFlags & EFileOpen_DontTruncate)
				CreateDisposition = EDisp_OpenAlways;
			else
				CreateDisposition = EDisp_CreateAlways;
		}
		else
			CreateDisposition = EDisp_OpenExisting;
	}

	bool bRead = (_OpenFlags & EFileOpen_Read) != 0;
	bool bWrite = (_OpenFlags & EFileOpen_Write) != 0;

	uint32 OpenFlags = fg_GetUnixOpenFlags();
	if (bRead && bWrite)
		OpenFlags |= O_RDWR;
	else if (bRead)
		OpenFlags |= O_RDONLY;
	else if (bWrite)
		OpenFlags |= O_WRONLY;

	switch (CreateDisposition)
	{
		case EDisp_CreateNew:
			{
				OpenFlags |= O_CREAT | O_EXCL;
			}
			break;
		case EDisp_OpenExisting:
			{
			}
			break;
		case EDisp_TruncateExisting:
			{
				OpenFlags |= O_TRUNC;
			}
			break;
		case EDisp_OpenAlways:
			{
				OpenFlags |= O_CREAT;
			}
			break;
		case EDisp_CreateAlways:
			{
				OpenFlags |= O_CREAT | O_TRUNC;
			}
			break;
	}

#ifdef DPlatformFamily_Linux
	if (_OpenFlags & NMib::NFile::EFileOpen_NoCache)
		OpenFlags |= O_DIRECT;
	if (_OpenFlags & NMib::NFile::EFileOpen_WriteThrough)
		OpenFlags |= O_SYNC;
#endif

	bool bExists = NSys::NFile::fg_FileExistsGeneral(_FileName, EFileAttrib_File | EFileAttrib_Directory);
	if (bExists)
	{
		struct stat Stats;
		if (stat(FileName, &Stats))
		{
			if constexpr (tf_bException)
				DMibErrorFile(NMib::NPlatform::fg_FormatErrno<tf_CFileStr>(typename tf_CFileStr::CFormat("stat('{}') when opening file") << FileName,errno));
			else
				return -1;
		}

		EFileAttrib Attribs = fsg_StatsToAttribs(Stats);

		if (Attribs & EFileAttrib_Directory)
		{
			if (!(_OpenFlags & EFileOpen_Directory))
			{
				if constexpr (tf_bException)
					DMibErrorFile(tf_CFileStr(typename tf_CFileStr::CFormat("Directory '{}' cannot be openened as file") << FileName));
				else
					return -1;
			}
		}
		else
		{
			if (_OpenFlags & EFileOpen_Directory)
			{
				if constexpr (tf_bException)
					DMibErrorFile(tf_CFileStr(typename tf_CFileStr::CFormat("File '{}' cannot be openened as directory") << FileName));
				else
					return -1;
			}
		}
	}

//	umask(0);

	int iFile = open(FileName, OpenFlags, fg_MalterlibAttributesToMode(_Attributes));
	if (iFile < 0)
	{
		if constexpr (tf_bException)
			DMibErrorFile(NMib::NPlatform::fg_FormatErrno<tf_CFileStr>(typename tf_CFileStr::CFormat("open('{}') when opening file") << FileName, errno));
		else
			return -1;
	}

	auto Cleanup = g_OnScopeExit / [&]
		{
			close(iFile);
		}
	;

	fg_SetUnixHandleOptions(iFile);

#ifdef DPlatformFamily_macOS
	if (_OpenFlags & NMib::NFile::EFileOpen_NoCache)
	{
		if (fcntl(iFile, F_NOCACHE, 1))
		{
			if constexpr (tf_bException)
				DMibErrorFile(NMib::NPlatform::fg_FormatErrno<tf_CFileStr>(typename tf_CFileStr::CFormat("fcntl('{}', F_NOCACHE, 1) when opening file") << FileName, errno));
			else
				return -1;
		}
	}
#endif

	if ((_OpenFlags & (EFileOpen_Read | EFileOpen_Write)) && !(_OpenFlags & EFileOpen_ShareBypass))
	{
		int LockFlags = 0;
		if ((_OpenFlags & (NMib::NFile::EFileOpen_ShareRead | NMib::NFile::EFileOpen_ShareWrite)))
			LockFlags = LOCK_SH|LOCK_NB;
		else
			LockFlags = LOCK_EX|LOCK_NB;

		if (flock(iFile, LockFlags))
		{
			int FlockErr = errno;

			if constexpr (tf_bException)
			{
				if (FlockErr == EWOULDBLOCK)
				{
					DMibErrorFile
						(
							NMib::NPlatform::fg_FormatErrno<tf_CFileStr>
							(
								typename tf_CFileStr::CFormat("flock('{}', {nfh}) when opening file. The file is probably locked by another program") << FileName << LockFlags
								, errno
							)
						)
					;
				}
				else
					DMibErrorFile(NMib::NPlatform::fg_FormatErrno<tf_CFileStr>(typename tf_CFileStr::CFormat("flock('{}', {nfh}) when opening file") << FileName << LockFlags, errno));
			}
			else
				return -1;
		}
	}

	if (OpenFlags & (O_WRONLY | O_RDWR))
		fg_SetBSDFileAttributes(iFile, 0644, _Attributes, _FileName);
	else if (_Attributes != EFileAttrib_None)
	{
		if constexpr (tf_bException)
			DMibErrorFile("You cannot specify attributes without opening the file for write");
		else
			return -1;
	}

	Cleanup.f_Clear();
	return iFile;
}

template <typename tf_CFileStr, bool tf_bException, typename tf_CStr>
void *fg_OpenHelper(const tf_CStr &_FileName, NMib::NFile::EFileOpen _OpenFlags, NMib::NFile::EFileAttrib _Attributes)
{
	tf_CStr PosixFileName;
	int iFile = fg_OpenHelperBSDFile<tf_CFileStr, tf_bException, tf_CStr>(_FileName, _OpenFlags, PosixFileName, _Attributes);

	if constexpr (!tf_bException)
	{
		if (iFile < 0)
			return nullptr;
	}

	NStorage::TCUniquePointer<TCPOSIXFileImp<tf_CFileStr, typename tf_CFileStr::CAllocator>, typename tf_CFileStr::CAllocator> pNewFile = fg_Construct(PosixFileName);
	pNewFile->m_BSDFile = iFile;
	return pNewFile.f_Detach();
}

namespace
{
	template <typename tf_CStr>
	TCVector<ch8, typename tf_CStr::CAllocator> fg_ReadProcFS(NMib::NStr::CFStr256 const &_Path)
	{
		using namespace NMib::NFile;
		auto Flags = EFileOpen_Read | EFileOpen_ShareAll | EFileOpen_NoLocalCache | EFileOpen_ShareBypass;

		void *pFileHandle = fg_OpenHelper<NMib::NStr::CFStr256, true>(_Path, Flags, EFileAttrib_None);
		auto Cleanup
			= fg_OnScopeExit
			(
				[&]()
				{
					NSys::NFile::fg_Close(pFileHandle);
				}
			)
		;

		TCVector<ch8, typename tf_CStr::CAllocator> FileData;

		CPOSIXFile *pFile = (CPOSIXFile *)pFileHandle;

		ch8 Temp[256];
		while (1)
		{

			ssize_t ReadBytes = read(pFile->m_BSDFile, Temp, 256);

			if (ReadBytes > 0)
				FileData.f_Insert(Temp, ReadBytes);
			if (ReadBytes < 256)
				break;
		}

		FileData.f_Insert(ch8(0)); // Insert null terminator

		return FileData;
	}

	template <typename tf_CStr>
	bool fg_ReadProcFS(NMib::NStr::CFStr256 const &_Path, TCVector<ch8, typename tf_CStr::CAllocator> &o_Data)
	{
		using namespace NMib::NFile;
		auto Flags = EFileOpen_Read | EFileOpen_ShareAll | EFileOpen_NoLocalCache | EFileOpen_ShareBypass;

		void *pFileHandle = fg_OpenHelper<NMib::NStr::CFStr256, false>(_Path, Flags, EFileAttrib_None);
		if (!pFileHandle)
			return false;

		auto Cleanup
			= fg_OnScopeExit
			(
				[&]()
				{
					NSys::NFile::fg_Close(pFileHandle);
				}
			)
		;

		TCVector<ch8, typename tf_CStr::CAllocator> FileData;

		CPOSIXFile *pFile = (CPOSIXFile *)pFileHandle;

		try
		{
			ch8 Temp[256];
			while (1)
			{

				ssize_t ReadBytes = read(pFile->m_BSDFile, Temp, 256);

				if (ReadBytes > 0)
					FileData.f_Insert(Temp, ReadBytes);
				if (ReadBytes < 256)
					break;
			}

			FileData.f_Insert(ch8(0)); // Insert null terminator
		}
		catch (NMib::NFile::CExceptionFile const &)
		{
			return false;
		}

		o_Data = fg_Move(FileData);

		return true;
	}
}

umint NMib::NPlatform::fg_ReadProcFS(NMib::NStr::CFStr256 const &_Path, uint8 *_pData, umint _nBytes)
{
	using namespace NMib::NFile;
	auto Flags = EFileOpen_Read | EFileOpen_ShareBypass | EFileOpen_NoLocalCache | EFileOpen_ShareBypass;

	CFStr256 PosixFileName;
	int BSDFile = fg_OpenHelperBSDFile<NMib::NStr::CFStr256, true>(_Path, Flags, PosixFileName, EFileAttrib_None);
	auto Cleanup
		= fg_OnScopeExit
		(
			[&]()
			{
				close(BSDFile);
			}
		)
	;

	ssize_t ReadBytes = read(BSDFile, _pData, _nBytes);

	if (ReadBytes < 0)
		DMibPDebugBreak;

	return ReadBytes;
}

TCVector<ch8> NMib::NPlatform::fg_ReadProcFS(NMib::NStr::CFStr256 const &_Path)
{
	return ::fg_ReadProcFS<NMib::NStr::CStr>(_Path);
}

bool NMib::NPlatform::fg_ReadProcFS(NMib::NStr::CFStr256 const &_Path, NContainer::TCVector<ch8> &o_Output)
{
	return ::fg_ReadProcFS<NMib::NStr::CStr>(_Path, o_Output);
}

TCVector<ch8, NMemory::CAllocator_NonTrackedHeap> NMib::NPlatform::fg_ReadProcFSNonTracked(NMib::NStr::CFStr256 const &_Path)
{
	return ::fg_ReadProcFS<NMib::NStr::CStrNonTracked>(_Path);
}

void *NSys::NFile::fg_Open(const NMib::NStr::CStr &_FileName, NMib::NFile::EFileOpen _OpenFlags, NMib::NFile::EFileAttrib _Attributes)
{
	return fg_OpenHelper<NMib::NStr::CStr, true>(CStr(_FileName), _OpenFlags, _Attributes);
}

void *NSys::NFile::fg_Open(const NMib::NStr::CStrNonTracked &_FileName, NMib::NFile::EFileOpen _OpenFlags, NMib::NFile::EFileAttrib _Attributes)
{
	return fg_OpenHelper<NMib::NStr::CStrNonTracked, true>(_FileName, _OpenFlags, _Attributes);
}

void *NSys::NFile::fg_GetOSFile(void *_pFile)
{
	static_assert(sizeof(void *) >= sizeof(CPOSIXFile::m_BSDFile), "Cannot fit");
	return (void *)(umint)((CPOSIXFile *)_pFile)->m_BSDFile;
}

void NSys::NFile::fg_Close(void *_pFile)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;

	int ErrNo = close(pFile->m_BSDFile);
	if (ErrNo)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("close('{}') when closing file") << pFile->f_GetFileName(), errno));

	pFile->f_Delete();
}

umint NSys::NFile::fg_Read(void *_pFile, void *_pData, const CMibFilePos &_Offset, umint _NumBytes)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;

	if (_NumBytes > umint(1) * 1024 * 1024 * 1024)
	{
		umint ReturnBytes = 0;
		umint nBytesLeft = _NumBytes;
		CMibFilePos Offset = _Offset;
		uint8 *pData = (uint8 *)_pData;

		while (nBytesLeft)
		{
			umint ThisTime = fg_Min(nBytesLeft, umint(1) * 1024 * 1024 * 1024);
			ssize_t ReadBytes = pread(pFile->m_BSDFile, pData, ThisTime, Offset);
			if (ReadBytes < 0)
				DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("pread('{}') when reading file") << pFile->f_GetFileName(), errno));
			ReturnBytes += ReadBytes;
			pData += ReadBytes;
			Offset += ReadBytes;
			nBytesLeft -= ReadBytes;

			if (ReadBytes != ThisTime)
				break; //
		}

		return ReturnBytes;
	}
	else
	{
		ssize_t ReadBytes = pread(pFile->m_BSDFile, _pData, _NumBytes, _Offset);

		if (ReadBytes < 0)
			DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("pread('{}') when reading file") << pFile->f_GetFileName(), errno));

		return ReadBytes;
	}
}

umint NSys::NFile::fg_Write(void *_pFile, const void *_pData, const CMibFilePos &_Offset, umint _NumBytes)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;

	if (_NumBytes > umint(1) * 1024 * 1024 * 1024)
	{
		umint ReturnBytes = 0;
		umint nBytesLeft = _NumBytes;
		CMibFilePos Offset = _Offset;
		uint8 const *pData = (uint8 const *)_pData;

		while (nBytesLeft)
		{
			umint ThisTime = fg_Min(nBytesLeft, umint(1) * 1024 * 1024 * 1024);
			ssize_t WrittenBytes = pwrite(pFile->m_BSDFile, pData, ThisTime, Offset);
			if (WrittenBytes < 0)
				DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("pwrite('{}') when writing file") << pFile->f_GetFileName(), errno));
			ReturnBytes += WrittenBytes;
			pData += WrittenBytes;
			Offset += WrittenBytes;
			nBytesLeft -= WrittenBytes;

			if (WrittenBytes != ThisTime)
				break; //
		}

		return ReturnBytes;
	}
	else
	{
		ssize_t WrittenBytes = pwrite(pFile->m_BSDFile, _pData, _NumBytes, _Offset);

		if (WrittenBytes < 0)
			DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("pwrite('{}') when writing file") << pFile->f_GetFileName(), errno));
		return WrittenBytes;
	}
}


void NSys::NFile::fg_SetSize(void *_pFile, const CMibFilePos &_Size)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;

	int iRet = ftruncate(pFile->m_BSDFile, _Size);

	if (iRet < 0)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("ftruncate('{}') when setting file length") << pFile->f_GetFileName(), errno));
}

CMibFilePos NSys::NFile::fg_GetSize(const NMib::NStr::CStr &_FileName)
{
	CStr FileName = fg_ConvertToPOSIXPath(_FileName);

	struct stat Stats;
	int iRet = stat(FileName.f_GetStr(), &Stats);
	if (iRet < 0)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("stat('{}') when getting file length") << FileName, errno));
	return Stats.st_size;
}

CMibFilePos NSys::NFile::fg_GetSize(void *_pFile)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;

	struct stat Stats;
	int iRet = fstat(pFile->m_BSDFile, &Stats);
	if (iRet < 0)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("fstat('{}') when getting file length") << pFile->f_GetFileName(), errno));
	return Stats.st_size;
}

void NSys::NFile::fg_Flush(void *_pFile)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;

	int iRet = fsync(pFile->m_BSDFile);
	if (iRet < 0)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("fsync('{}') when flushing file") << pFile->f_GetFileName(), errno));
}

void NSys::NFile::fg_LockRange(void *_pFile, const CMibFilePos &_Offset, const CMibFilePos &_NumBytes, NMib::NFile::EFileLock _Flags)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;

	struct flock ToLock;
	ToLock.l_start = _Offset;
	ToLock.l_len = _NumBytes;
	ToLock.l_pid = 0;
	ToLock.l_type = 0;
	ToLock.l_whence = SEEK_SET;

	int Opr = F_SETLK;
	if (_Flags & NMib::NFile::EFileLock_PreventRead)
		ToLock.l_type = F_RDLCK;
	else
		ToLock.l_type = F_WRLCK;

	if (_Flags & NMib::NFile::EFileLock_Block)
		Opr = F_SETLKW;

	int iRet = fcntl(pFile->m_BSDFile, Opr, &ToLock);
	if (iRet < 0)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("fcntl('{}', {}, {}) when locking file range") << pFile->f_GetFileName() << _Offset << _NumBytes, errno));
}

void NSys::NFile::fg_UnlockRange(void *_pFile, const CMibFilePos &_Offset, const CMibFilePos &_NumBytes)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;

	struct flock ToLock;
	ToLock.l_start = _Offset;
	ToLock.l_len = _NumBytes;
	ToLock.l_pid = 0;
	ToLock.l_type = 0;
	ToLock.l_whence = SEEK_SET;
	int Opr = F_SETLK;
	ToLock.l_type = F_UNLCK;

	int iRet = fcntl(pFile->m_BSDFile, Opr, &ToLock);
	if (iRet < 0)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("fcntl('{}', {}, {}) when unlocking file range") << pFile->f_GetFileName() << _Offset << _NumBytes, errno));
}


NMib::NFile::EFileAttrib NSys::NFile::fg_GetAttributes(void *_pFile)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;
	struct stat Stats;
	if (fstat(pFile->m_BSDFile, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("fstat('{}') when getting file attributes") << pFile->f_GetFileName(), errno));

	NMib::NFile::EFileAttrib Attribs = fsg_StatsToAttribs(Stats);
	return Attribs;
}

uint32 fg_MalterlibAttributesToMode(NMib::NFile::EFileAttrib _Attributes, uint32 _Mode)
{
	uint32 Mode = _Mode;

	if (_Attributes & NMib::NFile::EFileAttrib_UnixAttributesValid)
	{
		Mode &= ~uint32(S_IRWXU | S_IRWXG | S_IRWXO);

		if (_Attributes & NMib::NFile::EFileAttrib_UserRead)
			Mode |= S_IRUSR;
		if (_Attributes & NMib::NFile::EFileAttrib_UserWrite)
			Mode |= S_IWUSR;
		if (_Attributes & NMib::NFile::EFileAttrib_UserExecute)
			Mode |= S_IXUSR;

		if (_Attributes & NMib::NFile::EFileAttrib_GroupRead)
			Mode |= S_IRGRP;
		if (_Attributes & NMib::NFile::EFileAttrib_GroupWrite)
			Mode |= S_IWGRP;
		if (_Attributes & NMib::NFile::EFileAttrib_GroupExecute)
			Mode |= S_IXGRP;

		if (_Attributes & NMib::NFile::EFileAttrib_EveryoneRead)
			Mode |= S_IROTH;
		if (_Attributes & NMib::NFile::EFileAttrib_EveryoneWrite)
			Mode |= S_IWOTH;
		if (_Attributes & NMib::NFile::EFileAttrib_EveryoneExecute)
			Mode |= S_IXOTH;
	}
	else
	{
#ifdef DMibPLinuxKernel
		Mode &= ~uint32(S_IXUSR | S_IXGRP | S_IXOTH | S_IWUSR);
#else
		Mode &= ~uint32(S_IXUSR | S_IXGRP | S_IXOTH);
#endif

		if (_Attributes & NMib::NFile::EFileAttrib_Executable)
			Mode |= S_IXUSR | S_IXGRP | S_IXOTH;
#ifdef DMibPLinuxKernel
		// Use user write flag for read only on linux
		if (!(_Attributes & NMib::NFile::EFileAttrib_ReadOnly))
			Mode |= S_IWUSR;
#endif
	}


	return Mode;
}

namespace
{
#ifdef DMibPMachKernel
	uint32 fg_MalterlibAttributesToFlags(uint32 _Flags, NMib::NFile::EFileAttrib _Attributes)
	{
		uint32 Flags = _Flags;
		if (_Attributes & NMib::NFile::EFileAttrib_ReadOnly)
			Flags |= UF_IMMUTABLE;
		else
			Flags &= (~uint32(UF_IMMUTABLE));
		if (_Attributes & NMib::NFile::EFileAttrib_Hidden)
			Flags |= UF_HIDDEN;
		else
			Flags &= (~uint32(UF_HIDDEN));
		return Flags;
	}
#endif
}

namespace
{
	void fg_SetBSDFileAttributes(int _iFile, mode_t _DefaultMode, NMib::NFile::EFileAttrib _Attributes, ch8 const *_pFileName)
	{
		struct stat Stats;
		if (fstat(_iFile, &Stats))
			DMibErrorFile(NMib::NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("fstat('{}') when setting file attributes") << _pFileName, errno));

		if (!_DefaultMode)
			_DefaultMode = Stats.st_mode;

	#ifdef DMibPMachKernel
		auto fl_MakeWritable
			= [&]()
			{
				if (Stats.st_flags & UF_IMMUTABLE)
				{
					Stats.st_flags &= ~UF_IMMUTABLE,
					fchflags(_iFile, Stats.st_flags);
				}
			}
		;
	#else
		auto fl_MakeWritable
			= [&]()
			{
			}
		;
	#endif

		// Set the mode...
		{
			uint32 Mode = fg_MalterlibAttributesToMode(_Attributes, _DefaultMode);

			if ((Mode & ~S_IFMT) != (Stats.st_mode & ~S_IFMT))
			{
				fl_MakeWritable();
				if (fchmod(_iFile, Mode))
					DMibErrorFile(NMib::NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("chmod('{}') when setting file attributes") << _pFileName, errno));
			}
		}


	#ifdef DMibPMachKernel
		{
			uint32 Flags = fg_MalterlibAttributesToFlags(Stats.st_flags, _Attributes);

			if (Flags != Stats.st_flags)
			{
				if (fchflags(_iFile, Flags))
					DMibErrorFile(NMib::NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("fchflags('{}') when setting file attributes") << _pFileName, errno));
			}
		}
	#endif
	}
}

void NSys::NFile::fg_SetAttributes(void *_pFile, NMib::NFile::EFileAttrib _Attributes)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;
	fg_SetBSDFileAttributes(pFile->m_BSDFile, 0, _Attributes, pFile->f_GetFileName());
}

NMib::NFile::EFileAttrib NSys::NFile::fg_GetAttributes(NMib::NStr::CStr const& _Filename)
{
	CStr Canonical = fg_ConvertToPOSIXPath(_Filename);
	struct stat Stats;
	if (lstat(Canonical, &Stats))
	{
		auto ErrNo = errno;
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("lstat('{}') when getting file attributes") << Canonical, ErrNo));
	}

	NMib::NFile::EFileAttrib LinkAttribs = fsg_StatsToAttribs(Stats);

	if (!(LinkAttribs & NMib::NFile::EFileAttrib_Link))
		return LinkAttribs;

	if (stat(Canonical, &Stats))
		return LinkAttribs;

	return fsg_StatsToAttribs(Stats) | NMib::NFile::EFileAttrib_Link;
}

NMib::NFile::EFileAttrib NSys::NFile::fg_GetAttributesOnLink(NMib::NStr::CStr const& _Filename)
{
	CStr Canonical = fg_ConvertToPOSIXPath(_Filename);
	struct stat Stats;
	if (lstat(Canonical, &Stats))
	{
		auto ErrNo = errno;
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("lstat('{}') when getting file attributes") << Canonical, ErrNo));
	}
	NMib::NFile::EFileAttrib Attribs = fsg_StatsToAttribs(Stats);
	return Attribs;
}

NMib::NFile::ECheckFileRights NSys::NFile::fg_CheckFileRights(CStr const &_File, NMib::NFile::EFileRight _Rights)
{
	if (!NMib::NFile::CFile::fs_FileExists(_File))
		return NMib::NFile::ECheckFileRights_DoesNotExist;

	bool bRead = (_Rights & NMib::NFile::EFileRight_Read) != 0;
	bool bWrite = (_Rights & NMib::NFile::EFileRight_Write) != 0;

	uint32 OpenFlags = fg_GetUnixOpenFlags();
	if (bRead && bWrite)
		OpenFlags |= O_RDWR;
	else if (bRead)
		OpenFlags |= O_RDONLY;
	else if (bWrite)
		OpenFlags |= O_WRONLY;

	int iFile = open(_File, OpenFlags, 0644);
	if (iFile < 0)
	{
		switch (errno)
		{
		case EACCES:
			return NMib::NFile::ECheckFileRights_NoAccess;
		}

		return NMib::NFile::ECheckFileRights_Access;
	}

	close(iFile);

	return NMib::NFile::ECheckFileRights_Access;
}

NMib::NFile::CUniqueFileIdentifier NSys::NFile::fg_GetUniqueIdentifier(void *_pFile)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;

	struct stat Stats;
	if (fstat(pFile->m_BSDFile, &Stats))
	{
		auto ErrNo = errno;
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("fstat('{}') when getting unique file ID") << pFile->f_GetFileName(), ErrNo));
	}

	return {static_cast<uint64>(Stats.st_dev), Stats.st_ino};
}

NMib::NFile::CUniqueFileIdentifier NSys::NFile::fg_GetUniqueIdentifier(NMib::NStr::CStr const& _FileName)
{
	CStr Canonical = fg_ConvertToPOSIXPath(_FileName);
	struct stat Stats;
	if (stat(Canonical, &Stats))
	{
		auto ErrNo = errno;
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("stat('{}') when getting unique file ID") << Canonical, ErrNo));
	}

	return {static_cast<uint64>(Stats.st_dev), Stats.st_ino};
}

NMib::NFile::CUniqueFileIdentifier NSys::NFile::fg_GetUniqueIdentifierOnLink(NMib::NStr::CStr const& _FileName)
{
	CStr Canonical = fg_ConvertToPOSIXPath(_FileName);
	struct stat Stats;
	if (lstat(Canonical, &Stats))
	{
		auto ErrNo = errno;
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("lstat('{}') when getting unique file ID on link") << Canonical, ErrNo));
	}

	return {static_cast<uint64>(Stats.st_dev), Stats.st_ino};
}

void NSys::NFile::fg_SetAttributes(NMib::NStr::CStr const& _Filename, NMib::NFile::EFileAttrib _Attributes)
{
	struct stat Stats;
	CStr Filename = fg_ConvertToPOSIXPath(_Filename);
	if (stat(Filename, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("stat('{}') when setting file attributes") << Filename,errno));

	// Set the mode...
	{
		uint32 Mode = fg_MalterlibAttributesToMode(_Attributes, Stats.st_mode);

		if (Mode != Stats.st_mode)
		{
			if (chmod(Filename, Mode))
				DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("chmod('{}') when setting file attributes") << Filename,errno));
		}
	}

	// Set the flags...
#ifdef DMibPMachKernel
	{
		uint32 Flags = fg_MalterlibAttributesToFlags(Stats.st_flags, _Attributes);
		if (Flags != Stats.st_flags)
		{
			if (chflags(Filename, Flags))
				DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("chflags('{}') when setting file attributes") << Filename,errno));
		}
	}
#endif
}

void NSys::NFile::fg_SetAttributesOnLink(NMib::NStr::CStr const& _Filename, NMib::NFile::EFileAttrib _Attributes)
{
	struct stat Stats;
	CStr Filename = fg_ConvertToPOSIXPath(_Filename);
	if (lstat(Filename, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("lstat('{}') when setting file attributes") << Filename,errno));

	// Set the mode...
	{
		uint32 Mode = fg_MalterlibAttributesToMode(_Attributes, Stats.st_mode);

		if (Mode != Stats.st_mode)
		{
			if (lchmod(Filename, Mode))
				DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("chmod('{}') when setting file attributes") << Filename,errno));
		}
	}

	// Set the flags...
#ifdef DMibPMachKernel
	{
		uint32 Flags = fg_MalterlibAttributesToFlags(Stats.st_flags, _Attributes);
		if (Flags != Stats.st_flags)
		{
			if (lchflags(Filename, Flags))
				DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("chflags('{}') when setting file attributes") << Filename,errno));
		}
	}
#endif
}

[[maybe_unused]] static timespec fsg_CTimeToTimespec(NTime::CTime const &_Time)
{
	static CTime EpochStart = NTime::CTimeConvert::fs_CreateTime(1970, 1, 1);
	auto Span = _Time - EpochStart;
	timespec TimeSpec;
	TimeSpec.tv_sec = Span.f_GetSeconds();

	// Convert fraction to nanoseconds using integer arithmetic with rounding
	// to avoid precision loss from floating-point truncation
	uint64 FractionInt = Span.f_GetFractionInt();
	constexpr uint64 Billion = 1'000'000'000;
	constexpr uint64 Divisor = NTime::NPrivate::CConst::mc_FractionDividend / Billion;

	// Integer division with rounding: (a + b/2) / b rounds to nearest
	uint64 Nanoseconds = (FractionInt + Divisor / 2) / Divisor;

	// Handle overflow if nanoseconds rounds to 1 second
	if (Nanoseconds >= Billion)
	{
		++TimeSpec.tv_sec;
		TimeSpec.tv_nsec = 0;
	}
	else
	{
		TimeSpec.tv_nsec = static_cast<long>(Nanoseconds);
	}

	return TimeSpec;
}

static NTime::CTime fsg_TimespecToCTime(timespec const &_DateTime)
{
	static CTime EpochStart = NTime::CTimeConvert::fs_CreateTime(1970, 1, 1);

	// Convert nanoseconds to internal fraction using integer arithmetic
	// FractionInt = Nanoseconds * mc_FractionDividend / 1e9
	//             = Nanoseconds * (Divisor * 1e9 + Remainder) / 1e9
	//             = Nanoseconds * Divisor + Nanoseconds * Remainder / 1e9
	constexpr uint64 Billion = 1'000'000'000;
	constexpr uint64 Divisor = NTime::NPrivate::CConst::mc_FractionDividend / Billion;
	constexpr uint64 Remainder = NTime::NPrivate::CConst::mc_FractionDividend % Billion;

	uint64 Nanoseconds = _DateTime.tv_nsec;
	uint64 FractionInt = Nanoseconds * Divisor + Nanoseconds * Remainder / Billion;

	CTimeSpan Span;
	Span.f_SetSecondsNoFraction(_DateTime.tv_sec);
	Span.f_SetFractionInt(FractionInt);

	return EpochStart + Span;
}

NTime::CTime NSys::NFile::fg_GetCreationTime(void *_pFile)
{
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;
	struct stat Stats;
	if (fstat(pFile->m_BSDFile, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("fstat('{}') when getting file creation time") << pFile->f_GetFileName(), errno));
	return fsg_TimespecToCTime(Stats.st_mtim);
#else
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;
#if DPlatformVersion < 1060
	struct stat64 Stats;
	if (fstat64(pFile->m_BSDFile, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("fstat('{}') when getting file creation time") << pFile->f_GetFileName(), errno));
#else
	struct stat Stats;
	if (fstat(pFile->m_BSDFile, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("fstat('{}') when getting file creation time") << pFile->f_GetFileName(), errno));
#endif
	return fsg_TimespecToCTime(Stats.st_birthtimespec);
#endif
}

NTime::CTime NSys::NFile::fg_GetAccessTime(void *_pFile)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;
	struct stat Stats;
	if (fstat(pFile->m_BSDFile, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("fstat('{}') when getting file access time") << pFile->f_GetFileName(), errno));
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	return fsg_TimespecToCTime(Stats.st_atim);
#else
	return fsg_TimespecToCTime(Stats.st_atimespec);
#endif
}

NTime::CTime NSys::NFile::fg_GetWriteTime(void *_pFile)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;
	struct stat Stats;
	if (fstat(pFile->m_BSDFile, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("fstat('{}') when getting file write time") << pFile->f_GetFileName(), errno));
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	return fsg_TimespecToCTime(Stats.st_mtim);
#else
	return fsg_TimespecToCTime(Stats.st_mtimespec);
#endif
}

NTime::CTime NSys::NFile::fg_GetCreationTime(NMib::NStr::CStr const& _FileName)
{
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	struct stat Stats;
	if (stat(_FileName.f_GetStr(), &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("stat('{}') when getting file creation time") << _FileName, errno));
	return fsg_TimespecToCTime(Stats.st_mtim);
#else
#if DPlatformVersion < 1060
	struct stat64 Stats;
	if (stat64(_FileName.f_GetStr(), &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("stat64('{}') when getting file creation time") << _FileName, errno));
#else
	struct stat Stats;
	if (stat(_FileName.f_GetStr(), &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("stat('{}') when getting file creation time") << _FileName, errno));
#endif
	return fsg_TimespecToCTime(Stats.st_birthtimespec);
#endif
}

NTime::CTime NSys::NFile::fg_GetAccessTime(NMib::NStr::CStr const& _FileName)
{
	struct stat Stats;
	if (stat(_FileName.f_GetStr(), &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("fstat('{}') when getting file access time") << _FileName, errno));
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	return fsg_TimespecToCTime(Stats.st_atim);
#else
	return fsg_TimespecToCTime(Stats.st_atimespec);
#endif
}

NTime::CTime NSys::NFile::fg_GetWriteTime(NMib::NStr::CStr const& _FileName)
{
	struct stat Stats;
	if (stat(_FileName.f_GetStr(), &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("stat('{}') when getting file write time") << _FileName, errno));
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	return fsg_TimespecToCTime(Stats.st_mtim);
#else
	return fsg_TimespecToCTime(Stats.st_mtimespec);
#endif
}

NTime::CTime NSys::NFile::fg_GetCreationTimeOnLink(NMib::NStr::CStr const& _FileName)
{
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	struct stat Stats;
	if (lstat(_FileName.f_GetStr(), &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("lstat('{}') when getting file creation time") << _FileName, errno));
	return fsg_TimespecToCTime(Stats.st_mtim);
#else
#if DPlatformVersion < 1060
	struct stat64 Stats;
	if (lstat64(_FileName.f_GetStr(), &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("lstat64('{}') when getting file creation time") << _FileName, errno));
#else
	struct stat Stats;
	if (lstat(_FileName.f_GetStr(), &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("lstat('{}') when getting file creation time") << _FileName, errno));
#endif
	return fsg_TimespecToCTime(Stats.st_birthtimespec);
#endif
}

NTime::CTime NSys::NFile::fg_GetAccessTimeOnLink(NMib::NStr::CStr const& _FileName)
{
	struct stat Stats;
	if (lstat(_FileName.f_GetStr(), &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("lstat('{}') when getting file access time") << _FileName, errno));
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	return fsg_TimespecToCTime(Stats.st_atim);
#else
	return fsg_TimespecToCTime(Stats.st_atimespec);
#endif
}

NTime::CTime NSys::NFile::fg_GetWriteTimeOnLink(NMib::NStr::CStr const& _FileName)
{
	struct stat Stats;
	if (lstat(_FileName.f_GetStr(), &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("lstat('{}') when getting file write time") << _FileName, errno));
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	return fsg_TimespecToCTime(Stats.st_mtim);
#else
	return fsg_TimespecToCTime(Stats.st_mtimespec);
#endif
}

#ifdef DPlatformFamily_macOS

void NSys::NFile::fg_SetCreationTime(void *_pFile, const NTime::CTime &_Time)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;

	struct attrlist AttributeList = { 0 };
	AttributeList.bitmapcount = ATTR_BIT_MAP_COUNT;
	AttributeList.commonattr = ATTR_CMN_CRTIME;
	struct timespec Time = fsg_CTimeToTimespec(_Time);

	if (fsetattrlist(pFile->m_BSDFile, &AttributeList, &Time, sizeof(Time), 0))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("fsetattrlist('{}') when setting file write time") << pFile->f_GetFileName(), errno));
}

void NSys::NFile::fg_SetAccessTime(void *_pFile, const NTime::CTime &_Time)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;

	struct attrlist AttributeList = { 0 };
	AttributeList.bitmapcount = ATTR_BIT_MAP_COUNT;
	AttributeList.commonattr = ATTR_CMN_ACCTIME;
	struct timespec Time = fsg_CTimeToTimespec(_Time);

	if (fsetattrlist(pFile->m_BSDFile, &AttributeList, &Time, sizeof(Time), 0))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("fsetattrlist('{}') when setting file write time") << pFile->f_GetFileName(), errno));
}

void NSys::NFile::fg_SetWriteTime(void *_pFile, const NTime::CTime &_Time)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;

	struct attrlist AttributeList = { 0 };
	AttributeList.bitmapcount = ATTR_BIT_MAP_COUNT;
	AttributeList.commonattr = ATTR_CMN_MODTIME;
	struct timespec Time = fsg_CTimeToTimespec(_Time);

	if (fsetattrlist(pFile->m_BSDFile, &AttributeList, &Time, sizeof(Time), 0))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("fsetattrlist('{}') when setting file write time") << pFile->f_GetFileName(), errno));
}

void NSys::NFile::fg_SetCreationTime(NMib::NStr::CStr const &_FileName, const NTime::CTime &_Time)
{
	struct attrlist AttributeList = { 0 };
	AttributeList.bitmapcount = ATTR_BIT_MAP_COUNT;
	AttributeList.commonattr = ATTR_CMN_CRTIME;
	struct timespec Time = fsg_CTimeToTimespec(_Time);

	if (setattrlist(_FileName.f_GetStr(), &AttributeList, &Time, sizeof(Time), 0))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("setattrlist('{}') when setting file write time") << _FileName, errno));
}

void NSys::NFile::fg_SetAccessTime(NMib::NStr::CStr const &_FileName, const NTime::CTime &_Time)
{
	struct attrlist AttributeList = { 0 };
	AttributeList.bitmapcount = ATTR_BIT_MAP_COUNT;
	AttributeList.commonattr = ATTR_CMN_ACCTIME;
	struct timespec Time = fsg_CTimeToTimespec(_Time);

	if (setattrlist(_FileName.f_GetStr(), &AttributeList, &Time, sizeof(Time), 0))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("setattrlist('{}') when setting file write time") << _FileName, errno));
}

void NSys::NFile::fg_SetWriteTime(NMib::NStr::CStr const &_FileName, const NTime::CTime &_Time)
{
	struct attrlist AttributeList = { 0 };
	AttributeList.bitmapcount = ATTR_BIT_MAP_COUNT;
	AttributeList.commonattr = ATTR_CMN_MODTIME;
	struct timespec Time = fsg_CTimeToTimespec(_Time);

	if (setattrlist(_FileName.f_GetStr(), &AttributeList, &Time, sizeof(Time), 0))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("setattrlist('{}') when setting file write time") << _FileName, errno));
}

void NSys::NFile::fg_SetCreationTimeOnLink(NMib::NStr::CStr const &_FileName, const NTime::CTime &_Time)
{
	struct attrlist AttributeList = { 0 };
	AttributeList.bitmapcount = ATTR_BIT_MAP_COUNT;
	AttributeList.commonattr = ATTR_CMN_CRTIME;
	struct timespec Time = fsg_CTimeToTimespec(_Time);

	if (setattrlist(_FileName.f_GetStr(), &AttributeList, &Time, sizeof(Time), FSOPT_NOFOLLOW))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("setattrlist('{}') when setting file write time") << _FileName, errno));
}

void NSys::NFile::fg_SetAccessTimeOnLink(NMib::NStr::CStr const &_FileName, const NTime::CTime &_Time)
{
	struct attrlist AttributeList = { 0 };
	AttributeList.bitmapcount = ATTR_BIT_MAP_COUNT;
	AttributeList.commonattr = ATTR_CMN_ACCTIME;
	struct timespec Time = fsg_CTimeToTimespec(_Time);

	if (setattrlist(_FileName.f_GetStr(), &AttributeList, &Time, sizeof(Time), FSOPT_NOFOLLOW))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("setattrlist('{}') when setting file write time") << _FileName, errno));
}

void NSys::NFile::fg_SetWriteTimeOnLink(NMib::NStr::CStr const &_FileName, const NTime::CTime &_Time)
{
	struct attrlist AttributeList = { 0 };
	AttributeList.bitmapcount = ATTR_BIT_MAP_COUNT;
	AttributeList.commonattr = ATTR_CMN_MODTIME;
	struct timespec Time = fsg_CTimeToTimespec(_Time);

	if (setattrlist(_FileName.f_GetStr(), &AttributeList, &Time, sizeof(Time), FSOPT_NOFOLLOW))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("setattrlist('{}') when setting file write time") << _FileName, errno));
}

#else

static timeval fsg_CTimeToTimeVal(const NTime::CTime &_DateTime)
{
	static CTime EpochStart = NTime::CTimeConvert::fs_CreateTime(1970, 1, 1);
	CTimeSpan Span = _DateTime - EpochStart;
	timeval Ret;
	Ret.tv_usec = (Span.f_GetFraction() * fp64(1000000)).f_ToInt();
	Ret.tv_sec = Span.f_GetSeconds();
	return Ret;
}

#ifndef TIMEVAL_TO_TIMESPEC
#define	TIMEVAL_TO_TIMESPEC(tv, ts) { \
	(ts)->tv_sec = (tv)->tv_sec; \
	(ts)->tv_nsec = (tv)->tv_usec * 1000; \
}
#endif

#ifndef TIMESPEC_TO_TIMEVAL
#define	TIMESPEC_TO_TIMEVAL(tv, ts) { \
	(tv)->tv_sec = (ts)->tv_sec; \
	(tv)->tv_usec = (ts)->tv_nsec / 1000; \
}
#endif

void NSys::NFile::fg_SetCreationTime(void *_pFile, const NTime::CTime &_Time)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;

#if defined(DPlatformFamily_Linux)
	if (NLocal::g_f_futimens)
	{
		struct timespec TimeSpecs[2] = {0};
		TimeSpecs[0].tv_nsec = UTIME_OMIT;
		TimeSpecs[1] = fsg_CTimeToTimespec(_Time);
		if (NLocal::g_f_futimens(pFile->m_BSDFile, TimeSpecs))
			DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("futimens('{}') when setting file creation time") << pFile->f_GetFileName(), errno));
		return;
	}
#endif

	struct stat Stats;
	if (fstat(pFile->m_BSDFile, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("fstat('{}') when setting file creation time") << pFile->f_GetFileName(), errno));
	timeval Vals[2];
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)

	TIMESPEC_TO_TIMEVAL(Vals+1, &Stats.st_mtim);
#else
	TIMESPEC_TO_TIMEVAL(Vals+1, &Stats.st_mtimespec);
#endif
	Vals[0] = fsg_CTimeToTimeVal(_Time);
	if (futimes(pFile->m_BSDFile, Vals))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("futimes('{}') when setting file creation time") << pFile->f_GetFileName(), errno));
}

void NSys::NFile::fg_SetAccessTime(void *_pFile, const NTime::CTime &_Time)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;

#if defined(DPlatformFamily_Linux)
	if (NLocal::g_f_futimens)
	{
		struct timespec TimeSpecs[2] = {0};
		TimeSpecs[1].tv_nsec = UTIME_OMIT;
		TimeSpecs[0] = fsg_CTimeToTimespec(_Time);
		if (NLocal::g_f_futimens(pFile->m_BSDFile, TimeSpecs))
			DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("futimens('{}') when setting file creation time") << pFile->f_GetFileName(), errno));
		return;
	}
#endif

	struct stat Stats;
	if (fstat(pFile->m_BSDFile, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("fstat('{}') when setting file access time") << pFile->f_GetFileName(), errno));
	timeval Vals[2];
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	TIMESPEC_TO_TIMEVAL(Vals+1, &Stats.st_mtim);
#else
	TIMESPEC_TO_TIMEVAL(Vals+1, &Stats.st_mtimespec);
#endif
	Vals[0] = fsg_CTimeToTimeVal(_Time);
	if (futimes(pFile->m_BSDFile, Vals))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("futimes('{}') when setting file access time") << pFile->f_GetFileName(), errno));
}

void NSys::NFile::fg_SetWriteTime(void *_pFile, const NTime::CTime &_Time)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;

#if defined(DPlatformFamily_Linux)
	if (NLocal::g_f_futimens)
	{
		struct timespec TimeSpecs[2] = {0};
		TimeSpecs[0].tv_nsec = UTIME_OMIT;
		TimeSpecs[1] = fsg_CTimeToTimespec(_Time);
		if (NLocal::g_f_futimens(pFile->m_BSDFile, TimeSpecs))
			DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("futimens('{}') when setting file creation time") << pFile->f_GetFileName(), errno));
		return;
	}
#endif

	struct stat Stats;
	if (fstat(pFile->m_BSDFile, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("fstat('{}') when setting file write time") << pFile->f_GetFileName(), errno));
	timeval Vals[2];
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	TIMESPEC_TO_TIMEVAL(Vals, &Stats.st_atim);
#else
	TIMESPEC_TO_TIMEVAL(Vals, &Stats.st_atimespec);
#endif
	Vals[1] = fsg_CTimeToTimeVal(_Time);
	if (futimes(pFile->m_BSDFile, Vals))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("futimes('{}') when setting file write time") << pFile->f_GetFileName(), errno));
}

void NSys::NFile::fg_SetCreationTime(NMib::NStr::CStr const &_FileName, const NTime::CTime &_Time)
{
#if defined(DPlatformFamily_Linux)
	if (NLocal::g_f_utimensat)
	{
		struct timespec TimeSpecs[2] = {0};
		TimeSpecs[0].tv_nsec = UTIME_OMIT;
		TimeSpecs[1] = fsg_CTimeToTimespec(_Time);
		if (NLocal::g_f_utimensat(AT_FDCWD, _FileName.f_GetStr(), TimeSpecs, 0))
			DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("utimensat('{}') when setting file creation time") << _FileName, errno));
		return;
	}
#endif

	struct stat Stats;
	if (stat(_FileName.f_GetStr(), &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("stat('{}') when setting file creation time") << _FileName, errno));
	timeval Vals[2];
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	TIMESPEC_TO_TIMEVAL(Vals+1, &Stats.st_mtim);
#else
	TIMESPEC_TO_TIMEVAL(Vals+1, &Stats.st_mtimespec);
#endif
	Vals[0] = fsg_CTimeToTimeVal(_Time);
	if (utimes(_FileName.f_GetStr(), Vals))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("utimes('{}') when setting file creation time") << _FileName, errno));
}

void NSys::NFile::fg_SetAccessTime(NMib::NStr::CStr const &_FileName, const NTime::CTime &_Time)
{
#if defined(DPlatformFamily_Linux)
	if (NLocal::g_f_utimensat)
	{
		struct timespec TimeSpecs[2] = {0};
		TimeSpecs[1].tv_nsec = UTIME_OMIT;
		TimeSpecs[0] = fsg_CTimeToTimespec(_Time);
		if (NLocal::g_f_utimensat(AT_FDCWD, _FileName.f_GetStr(), TimeSpecs, 0))
			DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("utimensat('{}') when setting file creation time") << _FileName, errno));
		return;
	}
#endif

	struct stat Stats;
	if (stat(_FileName.f_GetStr(), &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("stat('{}') when setting file access time") << _FileName, errno));
	timeval Vals[2];
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	TIMESPEC_TO_TIMEVAL(Vals+1, &Stats.st_mtim);
#else
	TIMESPEC_TO_TIMEVAL(Vals+1, &Stats.st_mtimespec);
#endif
	Vals[0] = fsg_CTimeToTimeVal(_Time);
	if (utimes(_FileName.f_GetStr(), Vals))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("utimes('{}') when setting file access time") << _FileName, errno));
}

void NSys::NFile::fg_SetWriteTime(NMib::NStr::CStr const &_FileName, const NTime::CTime &_Time)
{
#if defined(DPlatformFamily_Linux)
	if (NLocal::g_f_utimensat)
	{
		struct timespec TimeSpecs[2] = {0};
		TimeSpecs[0].tv_nsec = UTIME_OMIT;
		TimeSpecs[1] = fsg_CTimeToTimespec(_Time);
		if (NLocal::g_f_utimensat(AT_FDCWD, _FileName.f_GetStr(), TimeSpecs, 0))
			DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("utimensat('{}') when setting file creation time") << _FileName, errno));
		return;
	}
#endif

	struct stat Stats;
	if (stat(_FileName.f_GetStr(), &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("stat('{}') when setting file write time") << _FileName, errno));
	timeval Vals[2];
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	TIMESPEC_TO_TIMEVAL(Vals, &Stats.st_atim);
#else
	TIMESPEC_TO_TIMEVAL(Vals, &Stats.st_atimespec);
#endif
	Vals[1] = fsg_CTimeToTimeVal(_Time);
	if (utimes(_FileName.f_GetStr(), Vals))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("utimes('{}') when setting file write time") << _FileName, errno));
}

void NSys::NFile::fg_SetCreationTimeOnLink(NMib::NStr::CStr const &_FileName, const NTime::CTime &_Time)
{
#if defined(DPlatformFamily_Linux)
	if (NLocal::g_f_utimensat)
	{
		struct timespec TimeSpecs[2] = {0};
		TimeSpecs[0].tv_nsec = UTIME_OMIT;
		TimeSpecs[1] = fsg_CTimeToTimespec(_Time);
		if (NLocal::g_f_utimensat(AT_FDCWD, _FileName.f_GetStr(), TimeSpecs, AT_SYMLINK_NOFOLLOW))
			DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("utimensat('{}') when setting file creation time") << _FileName, errno));
		return;
	}
#endif

	struct stat Stats;
	if (lstat(_FileName.f_GetStr(), &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("lstat('{}') when setting file creation time") << _FileName, errno));
	timeval Vals[2];
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	TIMESPEC_TO_TIMEVAL(Vals+1, &Stats.st_mtim);
#else
	TIMESPEC_TO_TIMEVAL(Vals+1, &Stats.st_mtimespec);
#endif
	Vals[0] = fsg_CTimeToTimeVal(_Time);
	if (lutimes(_FileName.f_GetStr(), Vals))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("lutimes('{}') when setting file creation time") << _FileName, errno));
}

void NSys::NFile::fg_SetAccessTimeOnLink(NMib::NStr::CStr const &_FileName, const NTime::CTime &_Time)
{
#if defined(DPlatformFamily_Linux)
	if (NLocal::g_f_utimensat)
	{
		struct timespec TimeSpecs[2] = {0};
		TimeSpecs[1].tv_nsec = UTIME_OMIT;
		TimeSpecs[0] = fsg_CTimeToTimespec(_Time);
		if (NLocal::g_f_utimensat(AT_FDCWD, _FileName.f_GetStr(), TimeSpecs, AT_SYMLINK_NOFOLLOW))
			DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("utimensat('{}') when setting file creation time") << _FileName, errno));
		return;
	}
#endif

	struct stat Stats;
	if (lstat(_FileName.f_GetStr(), &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("lstat('{}') when setting file access time") << _FileName, errno));
	timeval Vals[2];
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	TIMESPEC_TO_TIMEVAL(Vals+1, &Stats.st_mtim);
#else
	TIMESPEC_TO_TIMEVAL(Vals+1, &Stats.st_mtimespec);
#endif
	Vals[0] = fsg_CTimeToTimeVal(_Time);
	if (lutimes(_FileName.f_GetStr(), Vals))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("lutimes('{}') when setting file access time") << _FileName, errno));
}

void NSys::NFile::fg_SetWriteTimeOnLink(NMib::NStr::CStr const &_FileName, const NTime::CTime &_Time)
{
#if defined(DPlatformFamily_Linux)
	if (NLocal::g_f_utimensat)
	{
		struct timespec TimeSpecs[2] = {0};
		TimeSpecs[0].tv_nsec = UTIME_OMIT;
		TimeSpecs[1] = fsg_CTimeToTimespec(_Time);
		if (NLocal::g_f_utimensat(AT_FDCWD, _FileName.f_GetStr(), TimeSpecs, AT_SYMLINK_NOFOLLOW))
			DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("utimensat('{}') when setting file creation time") << _FileName, errno));
		return;
	}
#endif

	struct stat Stats;
	if (lstat(_FileName.f_GetStr(), &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("lstat('{}') when setting file write time") << _FileName, errno));
	timeval Vals[2];
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	TIMESPEC_TO_TIMEVAL(Vals, &Stats.st_atim);
#else
	TIMESPEC_TO_TIMEVAL(Vals, &Stats.st_atimespec);
#endif
	Vals[1] = fsg_CTimeToTimeVal(_Time);
	if (lutimes(_FileName.f_GetStr(), Vals))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("lutimes('{}') when setting file write time") << _FileName, errno));
}

#endif

NMib::NStr::CStr NSys::NFile::fg_GetOwnerOnLink(const NMib::NStr::CStr &_Path)
{
	CStr UserName;

	CStr Canonical = fg_ConvertToPOSIXPath(_Path);
	struct stat Stats;
	if (lstat(Canonical, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("lstat('{}') when getting link owner") << Canonical, errno));

	NMib::NPlatform::CGetPwUidState State;
	auto *pPasswd = fg_Helper_GetPwUid(Stats.st_uid, State);

	if (pPasswd)
		UserName = CStr(pPasswd->pw_name);
	else
	{
		if (State.m_Error == 0) // Does not exist
			return CStr::fs_ToStr(Stats.st_uid);
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("getpwuid_r({}) when getting link owner for '{}'") << Stats.st_uid << Canonical, State.m_Error));
	}

	return UserName;
}
NMib::NStr::CStr NSys::NFile::fg_GetGroupOnLink(const NMib::NStr::CStr &_Path)
{
	CStr GroupName;

	CStr Canonical = fg_ConvertToPOSIXPath(_Path);
	struct stat Stats;
	if (lstat(Canonical, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("lstat('{}') when getting link group") << Canonical, errno));

	NMib::NPlatform::CGetGrGidState State;
	group *pGroup = fg_Helper_GetGrGid(Stats.st_gid, State);

	if (pGroup)
		GroupName = CStr(pGroup->gr_name);
	else
	{
		if (State.m_Error == 0) // Does not exist
			return CStr::fs_ToStr(Stats.st_gid);
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("getgrgid_r({}) when getting link group for '{}'") << Stats.st_gid << Canonical, State.m_Error));
	}

	return GroupName;
}

CStr NSys::NFile::fg_GetOwner(CStr const &_Path)
{
	CStr UserName;

	CStr Canonical = fg_ConvertToPOSIXPath(_Path);
	struct stat Stats;
	if (stat(Canonical, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("stat('{}') when getting file owner") << Canonical, errno));

	NMib::NPlatform::CGetPwUidState State;
	auto *pPasswd = fg_Helper_GetPwUid(Stats.st_uid, State);

	if (pPasswd)
		UserName = CStr(pPasswd->pw_name);
	else
	{
		if (State.m_Error == 0) // Does not exist
			return CStr::fs_ToStr(Stats.st_uid);
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("getpwuid_r({}) when getting file owner for '{}'") << Stats.st_uid << Canonical, State.m_Error));
	}

	return UserName;
}

CStr NSys::NFile::fg_GetGroup(CStr const &_Path)
{
	CStr GroupName;

	CStr Canonical = fg_ConvertToPOSIXPath(_Path);
	struct stat Stats;
	if (stat(Canonical, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("stat('{}') when getting file group") << Canonical, errno));

	NMib::NPlatform::CGetGrGidState State;
	group *pGroup = fg_Helper_GetGrGid(Stats.st_gid, State);

	if (pGroup)
		GroupName = CStr(pGroup->gr_name);
	else
	{
		if (State.m_Error == 0) // Does not exist
			return CStr::fs_ToStr(Stats.st_gid);
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("getgrgid_r({}) when getting file group for '{}'") << Stats.st_gid << Canonical, State.m_Error));
	}

	return GroupName;
}

void NSys::NFile::fg_SetOwner(CStr const &_Path, CStr const &_Owner)
{
	CStr Canonical = fg_ConvertToPOSIXPath(_Path);
	CStr Name = _Owner;

	NMib::NPlatform::CGetPwUidState State;
	passwd *pPasswd = NMib::NPlatform::fg_Helper_GetPwNam(Name.f_GetStr(), State);

	if (!pPasswd)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("getpwnam_r('{}') when setting owner on file '{}'") << Name << Canonical, State.m_Error));

	if (chown(Canonical.f_GetStr(), pPasswd->pw_uid, -1) != 0)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("chown('{}', {}) when setting owner on file") << Canonical << pPasswd->pw_uid, errno));
}

void NSys::NFile::fg_SetGroup(CStr const &_Path, CStr const &_Group)
{
	CStr Canonical = fg_ConvertToPOSIXPath(_Path);
	CStr Name = _Group;

	NMib::NPlatform::CGetGrGidState State;
	group *pGroup = NMib::NPlatform::fg_Helper_GetGrNam(Name.f_GetStr(), State);

	if (!pGroup)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("getgrnam_r('{}') when setting group on file '{}'") << Name << Canonical, State.m_Error));

	if (chown(Canonical.f_GetStr(), -1, pGroup->gr_gid) != 0)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("chown('{}', {}) when setting group on file") << Canonical << pGroup->gr_gid, errno));
}

void NSys::NFile::fg_SetOwner(void *_pFile, const NMib::NStr::CStr &_Owner)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;
	CStr Name = _Owner;

	NMib::NPlatform::CGetPwUidState State;
	passwd *pPasswd = NMib::NPlatform::fg_Helper_GetPwNam(Name.f_GetStr(), State);

	if (!pPasswd)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("getpwnam_r('{}') when setting owner on file '{}'") << Name << pFile->f_GetFileName(), State.m_Error));

	if (fchown(pFile->m_BSDFile, pPasswd->pw_uid, -1) != 0)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("fchown('{}', {}) when setting owner on file") << pFile->f_GetFileName() << pPasswd->pw_uid, errno));
}

void NSys::NFile::fg_SetGroup(void *_pFile, const NMib::NStr::CStr &_Group)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;
	CStr Name = _Group;

	NMib::NPlatform::CGetGrGidState State;
	group *pGroup = NMib::NPlatform::fg_Helper_GetGrNam(Name.f_GetStr(), State);

	if (!pGroup)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("getgrnam_r('{}') when setting group on file '{}'") << Name << pFile->f_GetFileName(), State.m_Error));

	if (fchown(pFile->m_BSDFile, -1, pGroup->gr_gid) != 0)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("fchown('{}', {}) when setting group on file") << pFile->f_GetFileName() << pGroup->gr_gid, errno));
}

void NSys::NFile::fg_SetOwnerOnLink(CStr const &_Path, CStr const &_Owner)
{
	CStr Canonical = fg_ConvertToPOSIXPath(_Path);
	CStr Name = _Owner;

	struct stat Stats;
	if (lstat(Canonical.f_GetStr(), &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("lstat('{}') when setting owner on link") << Canonical, errno));

	if ((Stats.st_mode & S_IFLNK) == 0)
		DMibErrorFile(CStr::CFormat("Cannot set owner on link when file is not a link ({})") << Canonical);

	NMib::NPlatform::CGetPwUidState State;
	passwd *pPasswd = NMib::NPlatform::fg_Helper_GetPwNam(Name.f_GetStr(), State);

	if (!pPasswd)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("getpwnam_r('{}') when setting owner on link '{}'") << Name << Canonical, State.m_Error));

	if (lchown(Canonical.f_GetStr(), pPasswd->pw_uid, -1) != 0)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("lchown('{}', {}) when setting owner on link") << Canonical << pPasswd->pw_uid, errno));
}

void NSys::NFile::fg_SetGroupOnLink(CStr const &_Path, CStr const &_Group)
{
	CStr Canonical = fg_ConvertToPOSIXPath(_Path);
	CStr Name = _Group;

	struct stat Stats;
	if (lstat(Canonical.f_GetStr(), &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("lstat('{}') when setting group on link") << Canonical, errno));

	if ((Stats.st_mode & S_IFLNK) == 0)
		DMibErrorFile(CStr::CFormat("Cannot set group on link when file is not a link ({})") << Canonical);

	NMib::NPlatform::CGetGrGidState State;
	group *pGroup = NMib::NPlatform::fg_Helper_GetGrNam(Name.f_GetStr(), State);

	if (!pGroup)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("getgrnam_r('{}') when setting group on link '{}'") << Name << Canonical, State.m_Error));

	if (lchown(Canonical.f_GetStr(), -1, pGroup->gr_gid) != 0)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("lchown('{}', {}) when setting group on link") << Canonical << pGroup->gr_gid, errno));
}

// *************************************************************************************************************************
// POSIX Find File Implementation
// *************************************************************************************************************************

CPOSIXFileFind::CPOSIXFileFind()
{
	m_pDir = nullptr;
}

CPOSIXFileFind::~CPOSIXFileFind()
{
	if (m_pDir)
	{
		closedir(m_pDir);
	}
}

uint64 CPOSIXFileFind::f_ParseAttrib()
{
	struct stat Stats;
	CStr FileName = m_LastFullName;
	NMib::NFile::EFileAttrib LinkAttribs = NMib::NFile::EFileAttrib_None;
	if (!lstat(FileName, &Stats))
	{
		LinkAttribs = fsg_StatsToAttribs(Stats);
		if (!(LinkAttribs & NMib::NFile::EFileAttrib_Link))
			return LinkAttribs; // lstat is same as stat in this case
	}

	if (stat(FileName, &Stats))
		return LinkAttribs;

	return fsg_StatsToAttribs(Stats) | (LinkAttribs & NMib::NFile::EFileAttrib_Link);
}

void *NSys::NFile::fg_FindOpen(const NMib::NStr::CStr &_FindPattern)
{
//	DMibTrace("fg_FindOpen({})\n", _FindPattern);
	CStr FileName = fg_ConvertToPOSIXPath(_FindPattern);
	CStr Path = NMib::NFile::CFile::fs_GetPath(FileName);
	CStr MatchStr = NMib::NFile::CFile::fs_GetFile(_FindPattern);

	if (Path.f_IsEmpty())
		Path = "/";

	CStr DirFileName = Path;

	DIR *pDir = opendir(DirFileName);
	if (!pDir)
	{
		int ErrNo = errno;
		if (ErrNo != ENOENT) // No such file just means that the find returns empty result set
			DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("opendir('{}') when finding files") << Path, ErrNo));
	}

	CPOSIXFileFind *pFind = fg_ConstructObject<CPOSIXFileFind>(NMemory::CDefaultAllocator());

	pFind->m_FullPath = Path;
	pFind->m_SearchPattern = MatchStr;
	pFind->m_pDir = pDir;
	return pFind;
}

static bool fsg_MatchPattern(const ch8 *_pStr, const ch8 *_pPattern)
{
	NStr::CStr Temp0 = NStr::CStr(_pStr).f_UpperCase();
	NStr::CStr Temp1 = NStr::CStr(_pPattern).f_UpperCase();
	const char *pParse = Temp0;
	const char *pPattern = Temp1;
	umint nWildCardAttempt = 0; // This ensures that e.g. *.hcl matches 0x0409.Estonian.hcl
	bool bWildCardSearch = false;

	while (true)
	{
		while (*pParse && *pPattern)
		{
			if (*pPattern == '*')
			{
				bWildCardSearch = true;
				++pPattern;
				umint iWildCardAttempt = 0;
				while (*pParse)
				{
					if (*pParse != *pPattern)
						++pParse;
					else if (iWildCardAttempt < nWildCardAttempt)
					{
						++pParse;
						++iWildCardAttempt;
					}
					else
					{
						break;
					}
				}
			}
			else if (*pPattern == '?')
			{
				++pPattern;
				++pParse;
			}
			else
			{
				if (*pPattern != *pParse)
					break;
				++pPattern;
				++pParse;
			}
		}

		if (*pParse == *pPattern)
		{
			return true;
		}
		else if (bWildCardSearch && *pParse)
		{
			++nWildCardAttempt;
			pParse = Temp0;
			pPattern = Temp1;
		}
		else
		{
			break;
		}
	}

	return false;
}

const NMib::NStr::CStr *NSys::NFile::fg_FindNext(void *_pFindContext, NMib::NFile::EFileAttrib &_FileAttribs)
{
	if (!_pFindContext)
		return nullptr;

	CPOSIXFileFind *pFind = (CPOSIXFileFind*)_pFindContext;

	if (!pFind->m_pDir)
		return nullptr;

	while (1)
	{
		errno = 0;
		dirent *pEntry = readdir(pFind->m_pDir);

		if (!pEntry)
		{
			if (errno)
				DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("readdir('{}') when finding files") << pFind->m_FullPath, errno));
			else
				return nullptr;
		}

		CStr FileName = pEntry->d_name;

		if (FileName != "." && FileName != "..")
		{
			bool bAccept = fsg_MatchPattern(FileName, pFind->m_SearchPattern);

			if (bAccept)
			{
				pFind->m_LastFullName = NMib::NFile::CFile::fs_AppendPath(pFind->m_FullPath, FileName);
				_FileAttribs = (NMib::NFile::EFileAttrib)pFind->f_ParseAttrib();
				return &pFind->m_LastFullName;
			}
		}
	}

	return nullptr;
}

void NSys::NFile::fg_FindClose(void *_pFindContext)
{
	if (!_pFindContext)
		return;
	CPOSIXFileFind *pFind = (CPOSIXFileFind*)_pFindContext;
	fg_DeleteObject(NMemory::CDefaultAllocator(), pFind);
}

