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

// *************************************************************************************************************************
// POSIX File Implementation
// *************************************************************************************************************************

namespace
{
	template <typename tf_CStr>
	auto fg_ConvertToPOSIXPath(tf_CStr const &_Path, bool _bAddCurrentDir = true)
		-> typename TCEnableIf<sizeof(typename tf_CStr::CChar) == 1, tf_CStr>::CType
	{
		static_assert(sizeof(typename tf_CStr::CChar) == 1, "Incorrect string type");
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
			bint fg_FileExistsGeneral(const tf_CStr &_FileName, uint32 _AttribMask)
			{
				tf_CStr File = fg_ConvertToPOSIXPath(_FileName);
				struct stat Stats;
				int RetVal = stat(File.f_GetStr(), &Stats);
				
				NMib::NFile::EFileAttrib Attribs = NMib::NFile::EFileAttrib_None; 
				if (RetVal)
				{
					int Error = errno;
					RetVal = lstat(File.f_GetStr(), &Stats);
					NMib::NFile::EFileAttrib LinkAttribs = fsg_StatsToAttribs(Stats);
					if (RetVal)
					{
						if (Error == ELOOP) // Too many Symbolic links encountered.
						{
							if (_AttribMask & NMib::NFile::EFileAttrib_Link
								|| (_AttribMask & (NMib::NFile::EFileAttrib_File|NMib::NFile::EFileAttrib_Directory)) == (NMib::NFile::EFileAttrib_File|NMib::NFile::EFileAttrib_Directory))
								return true;
							else
								return false;
						}
						else if (	Error == ENOENT 		// A component of _Filename does not exist (or _Filename is empty)
							|| 	Error == ENOTDIR 		// A component of the path prefix is not a dir.
							||	Error == EACCES 		// No access (permissions)
							||	Error == ENAMETOOLONG 	// Path is too long
							||	Error == ENOMEM) 		// Ran out of kernel memory
							return false;
						else
						{
							// This will most likely be EFAULT (&Stats is invalid)
							DMibErrorFile(NPlatform::fg_FormatErrno<tf_CStr>(typename tf_CStr::CFormat("stat('{}') when checking if file exists") << _FileName, Error));
						}
					}
					// If this is a link that we don't know what it's pointing on assume file.
					if (LinkAttribs & NMib::NFile::EFileAttrib_Link)
						LinkAttribs |= NMib::NFile::EFileAttrib_File;
					Attribs |= LinkAttribs;
				}
				else
				{
					Attribs = fsg_StatsToAttribs(Stats);
					RetVal = lstat(File.f_GetStr(), &Stats);
					if (!RetVal)
						Attribs |= fsg_StatsToAttribs(Stats) & NMib::NFile::EFileAttrib_Link;
				}
				
				if (!(_AttribMask & Attribs))
					return false;
				else
					return true;
			}
		}
	}
}

bint NSys::NFile::fg_FileExists(const NMib::NStr::CStr &_FileName, NMib::NFile::EFileAttrib _AttribMask)
{
	return fg_FileExistsGeneral(CStr(_FileName), _AttribMask);
}

bint NSys::NFile::fg_FileExists(const NMib::NStr::CStrNonTracked &_FileName, NMib::NFile::EFileAttrib _AttribMask)
{
	return fg_FileExistsGeneral(_FileName, _AttribMask);
}

NMib::NStream::CFilePos NSys::NFile::fg_GetFreeSpace(const NMib::NStr::CStr &_Path)
{
	CStr Path = _Path;
	struct statvfs Stats;
	if (statvfs(Path, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("statvfs('{}')") << _Path,errno));

	return NMib::NStream::CFilePos(Stats.f_frsize) * NMib::NStream::CFilePos(Stats.f_bavail);
}

NMib::NStream::CFilePos NSys::NFile::fg_GetUsedSpace(const NMib::NStr::CStr &_Path)
{
	CStr Path = _Path;
	struct statvfs Stats;
	if (statvfs(Path, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("statvfs('{}')") << _Path,errno));

	return NMib::NStream::CFilePos(Stats.f_frsize) * NMib::NStream::CFilePos(Stats.f_blocks - Stats.f_bfree);
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
			tf_CStr fg_ResolveSymbolicLink(const tf_CStr &_FileFrom)
			{
				static_assert(tf_CStr::mc_Type == EStrType_UTF && sizeof(typename tf_CStr::CChar) == 1, "");
				
				tf_CStr FileFrom = fg_ConvertToPOSIXPath(_FileFrom);
				tf_CStr NewString;
				auto nChars = readlink(FileFrom, NewString.f_GetStr(2048+1), 2048);
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
	tf_CStr NewPath = 	NMib::NStr::fg_StrReplaceChar(FileDirectory, '\\', '/');
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
		NPtr::TCUniquePointer<TCPOSIXFileImp, t_CAllocator> pPtr = fg_Explicit(this);
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

namespace 
{
	void fg_SetBSDFileAttributes(int _iFile, mode_t DefaultMode, NMib::NFile::EFileAttrib _Attributes, ch8 const *_pFileName);
}
uint32 fg_MalterlibAttributesToMode(NMib::NFile::EFileAttrib _Attributes, uint32 _Mode = 0644);

int fg_GetUnixOpenFlags()
{
	int Openflags = 0;
#ifdef DPlatformFamily_OSX
#if DPlatformVersionMax >= 1070
	if (NMib::CSystem::ms_PlatformVersion >= 10'07'00)
		Openflags |= O_CLOEXEC;
#endif
#endif
	return Openflags;
}

void fg_SetUnixHandleOptions(int _File)
{
#ifdef DPlatformFamily_OSX
#if DPlatformVersionMax >= 1070
	if (NMib::CSystem::ms_PlatformVersion < 10'07'00)
#endif
#endif
	// Set CloseOnExec so that child processes do not get our open files.
	{
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
}

template <typename tf_CFileStr, typename tf_CStr>
int fg_OpenHelperBSDFile(const tf_CStr &_FileName, NMib::NFile::EFileOpen _OpenFlags, tf_CStr &_oPosixFileName, NMib::NFile::EFileAttrib _Attributes)
{
	using namespace NMib::NFile;
	if ((_OpenFlags & (EFileOpen_Read | EFileOpen_Write | EFileOpen_ReadAttribs | EFileOpen_WriteAttribs)) == 0)
		DMibErrorFile("Open flags contain neither read or write flags, one of them must be specified");

	if ((_OpenFlags & (EFileOpen_DontCreate | EFileOpen_DontOpenExisting)) == (EFileOpen_DontCreate | EFileOpen_DontOpenExisting))
		DMibErrorFile("Conflicting open flags (both don't open existing and don't create)");

	if ((_OpenFlags & (EFileOpen_DontOpenExisting | EFileOpen_Read | EFileOpen_Write)) == (EFileOpen_DontOpenExisting | EFileOpen_Read))
		DMibErrorFile("You are trying to open a file that does not exist for read access only, this makes no sence)");

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

	uint32 Openflags = fg_GetUnixOpenFlags();
	if (bRead && bWrite)
		Openflags |= O_RDWR;
	else if (bRead)
		Openflags |= O_RDONLY;
	else if (bWrite)
		Openflags |= O_WRONLY;
	
	switch (CreateDisposition)
	{
		case EDisp_CreateNew:
			{
				Openflags |= O_CREAT | O_EXCL;
			}
			break;
		case EDisp_OpenExisting:
			{
			}
			break;
		case EDisp_TruncateExisting:
			{
				Openflags |= O_TRUNC;
			}
			break;
		case EDisp_OpenAlways:
			{
				Openflags |= O_CREAT;
			}
			break;
		case EDisp_CreateAlways:
			{
				Openflags |= O_CREAT | O_TRUNC;
			}
			break;			
	}
	
	bint bExists = NSys::NFile::fg_FileExistsGeneral(_FileName, EFileAttrib_File|EFileAttrib_Directory);
	if (bExists)
	{
		struct stat Stats;
		if (stat(FileName, &Stats))
			DMibErrorFile(NMib::NPlatform::fg_FormatErrno<tf_CFileStr>(typename tf_CFileStr::CFormat("fstat('{}') when opening file") << FileName,errno));
		
		// For historical reasons EFileAttrib_File is never returned by this.
		EFileAttrib Attribs = fsg_StatsToAttribs(Stats) & (~EFileAttrib_File);
		
		if (!lstat(FileName, &Stats))
		{
			EFileAttrib LinkAttribs = fsg_StatsToAttribs(Stats);
			Attribs |= LinkAttribs & EFileAttrib_Link;
		}
		
		if (Attribs & EFileAttrib_Directory)
		{
			if (!(_OpenFlags & EFileOpen_Directory))
				DMibErrorFile(tf_CFileStr(typename tf_CFileStr::CFormat("Directory '{}' cannot be openened as file") << FileName));
		}
		else
		{
			if (_OpenFlags & EFileOpen_Directory)
				DMibErrorFile(tf_CFileStr(typename tf_CFileStr::CFormat("File '{}' cannot be openened as directory") << FileName));
		}
	}

//	umask(0);
	
	int iFile = open(FileName, Openflags, fg_MalterlibAttributesToMode(_Attributes));
	if (iFile < 0)
		DMibErrorFile(NMib::NPlatform::fg_FormatErrno<tf_CFileStr>(typename tf_CFileStr::CFormat("open('{}') when opening file") << FileName, errno));
	
	auto Cleanup = g_OnScopeExit > [&]
		{
			close(iFile);
		}
	;

	int LockFlags = 0;
	if ((_OpenFlags & (NMib::NFile::EFileOpen_ShareRead | NMib::NFile::EFileOpen_ShareWrite)))
		LockFlags = LOCK_SH|LOCK_NB;
	else
		LockFlags = LOCK_EX|LOCK_NB;

	if (flock(iFile, LockFlags))
	{
		int FlockErr = errno;

		if (FlockErr == EWOULDBLOCK)
			DMibErrorFile(NMib::NPlatform::fg_FormatErrno<tf_CFileStr>(typename tf_CFileStr::CFormat("flock('{}') when opening file. The file is probably locked by another program") << FileName, errno));
		else
			DMibErrorFile(NMib::NPlatform::fg_FormatErrno<tf_CFileStr>(typename tf_CFileStr::CFormat("flock('{}') when opening file") << FileName, errno));
	}
	
	fg_SetUnixHandleOptions(iFile);

	if (Openflags & (O_WRONLY | O_RDWR))
		fg_SetBSDFileAttributes(iFile, 0644, _Attributes, _FileName);
	else if (_Attributes != EFileAttrib_None)
		DMibErrorFile("You cannot specify attributes without opening the file for write");

	Cleanup.f_Clear();
	return iFile;
}

template <typename tf_CFileStr, typename tf_CStr>
void *fg_OpenHelper(const tf_CStr &_FileName, NMib::NFile::EFileOpen _OpenFlags, NMib::NFile::EFileAttrib _Attributes)
{
	
	tf_CStr PosixFileName;
	int iFile = fg_OpenHelperBSDFile<tf_CFileStr, tf_CStr>(_FileName, _OpenFlags, PosixFileName, _Attributes);
	
	NPtr::TCUniquePointer<TCPOSIXFileImp<tf_CFileStr, typename tf_CFileStr::CAllocator>, typename tf_CFileStr::CAllocator> pNewFile = fg_Construct(PosixFileName);
	pNewFile->m_BSDFile = iFile;
	return pNewFile.f_Detach();
}

namespace
{
	template <typename tf_CStr>
	TCVector<ch8, typename tf_CStr::CAllocator> fg_ReadProcFS(NMib::NStr::CFStr256 const &_Path)
	{
		using namespace NMib::NFile;
		auto Flags = EFileOpen_Read | EFileOpen_ShareAll | EFileOpen_NoLocalCache;
		
		void *pFileHandle = fg_OpenHelper<NMib::NStr::CFStr256>(_Path, Flags, EFileAttrib_None);
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
}

mint NMib::NPlatform::fg_ReadProcFS(NMib::NStr::CFStr256 const &_Path, uint8 *_pData, mint _nBytes)
{
	using namespace NMib::NFile;
	auto Flags = EFileOpen_Read | EFileOpen_ShareAll | EFileOpen_NoLocalCache;
	
	CFStr256 PosixFileName;
	int BSDFile = fg_OpenHelperBSDFile<NMib::NStr::CFStr256>(_Path, Flags, PosixFileName, EFileAttrib_None);
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

TCVector<ch8, NMem::CAllocator_NonTrackedHeap> NMib::NPlatform::fg_ReadProcFSNonTracked(NMib::NStr::CFStr256 const &_Path)
{
	return ::fg_ReadProcFS<NMib::NStr::CStrNonTracked>(_Path);
}

void *NSys::NFile::fg_Open(const NMib::NStr::CStr &_FileName, NMib::NFile::EFileOpen _OpenFlags, NMib::NFile::EFileAttrib _Attributes)
{
	return fg_OpenHelper<NMib::NStr::CStr>(CStr(_FileName), _OpenFlags, _Attributes);
}

void *NSys::NFile::fg_Open(const NMib::NStr::CStrNonTracked &_FileName, NMib::NFile::EFileOpen _OpenFlags, NMib::NFile::EFileAttrib _Attributes)
{
	return fg_OpenHelper<NMib::NStr::CStrNonTracked>(_FileName, _OpenFlags, _Attributes);
}

void *NSys::NFile::fg_GetOSFile(void *_pFile)
{
	static_assert(sizeof(void *) >= sizeof(CPOSIXFile::m_BSDFile), "Cannot fit");
	return (void *)(mint)((CPOSIXFile *)_pFile)->m_BSDFile;
}

void NSys::NFile::fg_Close(void *_pFile)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;

	int ErrNo = close(pFile->m_BSDFile);
	if (ErrNo)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("close('{}') when closing file") << pFile->f_GetFileName(), errno));
	
	pFile->f_Delete();
}

mint NSys::NFile::fg_Read(void *_pFile, void *_pData, const CMibFilePos &_Offset, mint _NumBytes)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;
	
	if (_NumBytes > mint(1) * 1024 * 1024 * 1024)
	{
		mint ReturnBytes = 0;
		mint nBytesLeft = _NumBytes;
		CMibFilePos Offset = _Offset;
		uint8 *pData = (uint8 *)_pData;
		
		while (nBytesLeft)
		{
			mint ThisTime = fg_Min(nBytesLeft, mint(1) * 1024 * 1024 * 1024);
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

mint NSys::NFile::fg_Write(void *_pFile, const void *_pData, const CMibFilePos &_Offset, mint _NumBytes)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;

	if (_NumBytes > mint(1) * 1024 * 1024 * 1024)
	{
		mint ReturnBytes = 0;
		mint nBytesLeft = _NumBytes;
		CMibFilePos Offset = _Offset;
		uint8 const *pData = (uint8 const *)_pData;
		
		while (nBytesLeft)
		{
			mint ThisTime = fg_Min(nBytesLeft, mint(1) * 1024 * 1024 * 1024);
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

	// For historical reasons EFileAttrib_File is never returned by this.
	NMib::NFile::EFileAttrib Attribs = fsg_StatsToAttribs(Stats) & (~NMib::NFile::EFileAttrib_File);
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
	if (stat(Canonical, &Stats))
	{
		auto ErrNo = errno;
		if (!lstat(Canonical, &Stats))
		{
			// This is needed to not throw an exception on broken links
			NMib::NFile::EFileAttrib Attribs = fsg_StatsToAttribs(Stats) & (~NMib::NFile::EFileAttrib_File);
			return Attribs;
		}
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("stat('{}') when getting file attributes") << Canonical, ErrNo));
	}

	// For historical reasons EFileAttrib_File is never returned by this.
	NMib::NFile::EFileAttrib Attribs = fsg_StatsToAttribs(Stats) & (~NMib::NFile::EFileAttrib_File);
	
	if (!lstat(Canonical, &Stats))
	{
		NMib::NFile::EFileAttrib LinkAttribs = fsg_StatsToAttribs(Stats);
		Attribs |= LinkAttribs & NMib::NFile::EFileAttrib_Link;
	}

	return Attribs;
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

static NTime::CTime fsg_TimespecToCTime(timespec &_DateTime)
{	
	static CTime EpochStart = NTime::CTimeConvert::fs_CreateTime(1970, 1, 1);
	fp64 Fraction = fp64(_DateTime.tv_nsec) / fp64(1000000000.0);
	return EpochStart + CTimeSpanConvert_BabylonianCommon::fs_CreateSpanFromSeconds(_DateTime.tv_sec, Fraction);
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

struct timespec st_atim;		/* Time of last access.  */
struct timespec st_mtim;		/* Time of last modification.  */
struct timespec st_ctim;		/* Time of last status change.  */

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
	CStr Canonical = fg_ConvertToPOSIXPath(_FileName);
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	struct stat Stats;
	if (stat(Canonical, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("stat('{}') when getting file creation time") << _FileName, errno));
	return fsg_TimespecToCTime(Stats.st_mtim);
#else
#if DPlatformVersion < 1060
	struct stat64 Stats;
	if (stat64(Canonical, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("stat64('{}') when getting file creation time") << _FileName, errno));
#else
	struct stat Stats;
	if (stat(Canonical, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("stat('{}') when getting file creation time") << _FileName, errno));
#endif
	return fsg_TimespecToCTime(Stats.st_birthtimespec);
#endif
}

NTime::CTime NSys::NFile::fg_GetAccessTime(NMib::NStr::CStr const& _FileName)
{
	CStr Canonical = fg_ConvertToPOSIXPath(_FileName);
	struct stat Stats;
	if (stat(Canonical, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("fstat('{}') when getting file access time") << _FileName, errno));
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	return fsg_TimespecToCTime(Stats.st_atim);
#else
	return fsg_TimespecToCTime(Stats.st_atimespec);
#endif	
}

NTime::CTime NSys::NFile::fg_GetWriteTime(NMib::NStr::CStr const& _FileName)
{
	CStr Canonical = fg_ConvertToPOSIXPath(_FileName);
	struct stat Stats;
	if (stat(Canonical, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("stat('{}') when getting file write time") << _FileName, errno));
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	return fsg_TimespecToCTime(Stats.st_mtim);
#else
	return fsg_TimespecToCTime(Stats.st_mtimespec);
#endif
}

NTime::CTime NSys::NFile::fg_GetCreationTimeOnLink(NMib::NStr::CStr const& _FileName)
{
	CStr Canonical = fg_ConvertToPOSIXPath(_FileName);
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	struct stat Stats;
	if (lstat(Canonical, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("lstat('{}') when getting file creation time") << _FileName, errno));
	return fsg_TimespecToCTime(Stats.st_mtim);
#else
#if DPlatformVersion < 1060
	struct stat64 Stats;
	if (lstat64(Canonical, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("lstat64('{}') when getting file creation time") << _FileName, errno));
#else
	struct stat Stats;
	if (lstat(Canonical, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("lstat('{}') when getting file creation time") << _FileName, errno));
#endif
	return fsg_TimespecToCTime(Stats.st_birthtimespec);
#endif
}

NTime::CTime NSys::NFile::fg_GetAccessTimeOnLink(NMib::NStr::CStr const& _FileName)
{
	CStr Canonical = fg_ConvertToPOSIXPath(_FileName);
	struct stat Stats;
	if (lstat(Canonical, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("lstat('{}') when getting file access time") << _FileName, errno));
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	return fsg_TimespecToCTime(Stats.st_atim);
#else
	return fsg_TimespecToCTime(Stats.st_atimespec);
#endif	
}

NTime::CTime NSys::NFile::fg_GetWriteTimeOnLink(NMib::NStr::CStr const& _FileName)
{
	CStr Canonical = fg_ConvertToPOSIXPath(_FileName);
	struct stat Stats;
	if (lstat(Canonical, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("lstat('{}') when getting file write time") << _FileName, errno));
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	return fsg_TimespecToCTime(Stats.st_mtim);
#else
	return fsg_TimespecToCTime(Stats.st_mtimespec);
#endif
}
static timeval fsg_CTime2OSXTime(const NTime::CTime &_DateTime)
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
	struct stat Stats;
	if (fstat(pFile->m_BSDFile, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("fstat('{}') when setting file creation time") << pFile->f_GetFileName(), errno));
	timeval Vals[2];
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	TIMESPEC_TO_TIMEVAL(Vals+1, &Stats.st_mtim);
#else
	TIMESPEC_TO_TIMEVAL(Vals+1, &Stats.st_mtimespec);
#endif
	Vals[0] = fsg_CTime2OSXTime(_Time);
	if (futimes(pFile->m_BSDFile, Vals))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("futimes('{}') when setting file creation time") << pFile->f_GetFileName(), errno));
}

void NSys::NFile::fg_SetAccessTime(void *_pFile, const NTime::CTime &_Time)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;
	struct stat Stats;
	if (fstat(pFile->m_BSDFile, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("fstat('{}') when setting file access time") << pFile->f_GetFileName(), errno));
	timeval Vals[2];
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	TIMESPEC_TO_TIMEVAL(Vals+1, &Stats.st_mtim);
#else
	TIMESPEC_TO_TIMEVAL(Vals+1, &Stats.st_mtimespec);
#endif
	Vals[0] = fsg_CTime2OSXTime(_Time);
	if (futimes(pFile->m_BSDFile, Vals))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("futimes('{}') when setting file access time") << pFile->f_GetFileName(), errno));
}

void NSys::NFile::fg_SetWriteTime(void *_pFile, const NTime::CTime &_Time)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;
	struct stat Stats;
	if (fstat(pFile->m_BSDFile, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("fstat('{}') when setting file write time") << pFile->f_GetFileName(), errno));
	timeval Vals[2];
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	TIMESPEC_TO_TIMEVAL(Vals, &Stats.st_atim);
#else
	TIMESPEC_TO_TIMEVAL(Vals, &Stats.st_atimespec);
#endif
	Vals[1] = fsg_CTime2OSXTime(_Time);
	if (futimes(pFile->m_BSDFile, Vals))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("futimes('{}') when setting file write time") << pFile->f_GetFileName(), errno));
}

void NSys::NFile::fg_SetCreationTime(NMib::NStr::CStr const &_FileName, const NTime::CTime &_Time)
{
	CStr Canonical = fg_ConvertToPOSIXPath(_FileName);
	struct stat Stats;
	if (stat(Canonical.f_GetStr(), &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("stat('{}') when setting file creation time") << _FileName, errno));
	timeval Vals[2];
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	TIMESPEC_TO_TIMEVAL(Vals+1, &Stats.st_mtim);
#else
	TIMESPEC_TO_TIMEVAL(Vals+1, &Stats.st_mtimespec);
#endif
	Vals[0] = fsg_CTime2OSXTime(_Time);
	if (utimes(Canonical.f_GetStr(), Vals))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("utimes('{}') when setting file creation time") << _FileName, errno));
}

void NSys::NFile::fg_SetAccessTime(NMib::NStr::CStr const &_FileName, const NTime::CTime &_Time)
{
	CStr Canonical = fg_ConvertToPOSIXPath(_FileName);
	struct stat Stats;
	if (stat(Canonical.f_GetStr(), &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("stat('{}') when setting file access time") << _FileName, errno));
	timeval Vals[2];
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	TIMESPEC_TO_TIMEVAL(Vals+1, &Stats.st_mtim);
#else
	TIMESPEC_TO_TIMEVAL(Vals+1, &Stats.st_mtimespec);
#endif
	Vals[0] = fsg_CTime2OSXTime(_Time);
	if (utimes(Canonical.f_GetStr(), Vals))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("utimes('{}') when setting file access time") << _FileName, errno));
}

void NSys::NFile::fg_SetWriteTime(NMib::NStr::CStr const &_FileName, const NTime::CTime &_Time)
{
	CStr Canonical = fg_ConvertToPOSIXPath(_FileName);
	struct stat Stats;
	if (stat(Canonical.f_GetStr(), &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("stat('{}') when setting file write time") << _FileName, errno));
	timeval Vals[2];
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	TIMESPEC_TO_TIMEVAL(Vals, &Stats.st_atim);
#else
	TIMESPEC_TO_TIMEVAL(Vals, &Stats.st_atimespec);
#endif
	Vals[1] = fsg_CTime2OSXTime(_Time);
	if (utimes(Canonical.f_GetStr(), Vals))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("utimes('{}') when setting file write time") << _FileName, errno));
}


void NSys::NFile::fg_SetCreationTimeOnLink(NMib::NStr::CStr const &_FileName, const NTime::CTime &_Time)
{
	CStr Canonical = fg_ConvertToPOSIXPath(_FileName);
	struct stat Stats;
	if (lstat(Canonical.f_GetStr(), &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("lstat('{}') when setting file creation time") << _FileName, errno));
	timeval Vals[2];
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	TIMESPEC_TO_TIMEVAL(Vals+1, &Stats.st_mtim);
#else
	TIMESPEC_TO_TIMEVAL(Vals+1, &Stats.st_mtimespec);
#endif
	Vals[0] = fsg_CTime2OSXTime(_Time);
	if (lutimes(Canonical.f_GetStr(), Vals))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("lutimes('{}') when setting file creation time") << _FileName, errno));
}

void NSys::NFile::fg_SetAccessTimeOnLink(NMib::NStr::CStr const &_FileName, const NTime::CTime &_Time)
{
	CStr Canonical = fg_ConvertToPOSIXPath(_FileName);
	struct stat Stats;
	if (lstat(Canonical.f_GetStr(), &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("lstat('{}') when setting file access time") << _FileName, errno));
	timeval Vals[2];
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	TIMESPEC_TO_TIMEVAL(Vals+1, &Stats.st_mtim);
#else
	TIMESPEC_TO_TIMEVAL(Vals+1, &Stats.st_mtimespec);
#endif
	Vals[0] = fsg_CTime2OSXTime(_Time);
	if (lutimes(Canonical.f_GetStr(), Vals))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("lutimes('{}') when setting file access time") << _FileName, errno));
}

void NSys::NFile::fg_SetWriteTimeOnLink(NMib::NStr::CStr const &_FileName, const NTime::CTime &_Time)
{
	CStr Canonical = fg_ConvertToPOSIXPath(_FileName);
	struct stat Stats;
	if (lstat(Canonical.f_GetStr(), &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("lstat('{}') when setting file write time") << _FileName, errno));
	timeval Vals[2];
#if defined(DPlatformFamily_Linux) || defined (DPlatformFamily_Emscripten)
	TIMESPEC_TO_TIMEVAL(Vals, &Stats.st_atim);
#else
	TIMESPEC_TO_TIMEVAL(Vals, &Stats.st_atimespec);
#endif
	Vals[1] = fsg_CTime2OSXTime(_Time);
	if (lutimes(Canonical.f_GetStr(), Vals))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStrNonTracked::CFormat("lutimes('{}') when setting file write time") << _FileName, errno));
}

NMib::NStr::CStr NSys::NFile::fg_GetOwnerOnLink(const NMib::NStr::CStr &_Path)
{
	CStr UserName;
	
	CStr Canonical = fg_ConvertToPOSIXPath(_Path);
	struct stat Stats;
	if (lstat(Canonical, &Stats))
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("lstat('{}') when getting link owner") << Canonical, errno));
	
	errno = 0;
	passwd *pPasswd = getpwuid(Stats.st_uid);
	
	if (pPasswd)
		UserName = CStr(pPasswd->pw_name);
	else
	{
		if (errno == 0) // Does not exist
			return CStr::fs_ToStr(Stats.st_uid);
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("getpwuid({}) when getting link owner for '{}'") << Stats.st_uid << Canonical, errno));
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
	
	errno = 0;
	group *pGroup = getgrgid(Stats.st_gid);
	
	if (pGroup)
		GroupName = CStr(pGroup->gr_name);
	else
	{
		if (errno == 0) // Does not exist
			return CStr::fs_ToStr(Stats.st_gid);
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("getgrgid({}) when getting link group for '{}'") << Stats.st_gid << Canonical, errno));
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

	errno = 0;
	passwd *pPasswd = getpwuid(Stats.st_uid);
	
	if (pPasswd)
		UserName = CStr(pPasswd->pw_name);
	else
	{
		if (errno == 0) // Does not exist
			return CStr::fs_ToStr(Stats.st_uid);
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("getpwuid({}) when getting file owner for '{}'") << Stats.st_uid << Canonical, errno));
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

	errno = 0;
	group *pGroup = getgrgid(Stats.st_gid);
	
	if (pGroup)
		GroupName = CStr(pGroup->gr_name);
	else
	{
		if (errno == 0) // Does not exist
			return CStr::fs_ToStr(Stats.st_gid);
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("getgrgid({}) when getting file group for '{}'") << Stats.st_gid << Canonical, errno));
	}
	
	return GroupName;
}

void NSys::NFile::fg_SetOwner(CStr const &_Path, CStr const &_Owner)
{
	CStr Canonical = fg_ConvertToPOSIXPath(_Path);
	CStr Name = _Owner;

	passwd *pPasswd = getpwnam(Name.f_GetStr());
	
	if (!pPasswd)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("getpwnam('{}') when setting owner on file '{}'") << Name << Canonical, errno));
	
	if (chown(Canonical.f_GetStr(), pPasswd->pw_uid, -1) != 0)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("chown('{}', {}) when setting owner on file") << Canonical << pPasswd->pw_uid, errno));
}

void NSys::NFile::fg_SetGroup(CStr const &_Path, CStr const &_Group)
{
	CStr Canonical = fg_ConvertToPOSIXPath(_Path);
	CStr Name = _Group;
	
	group *pGroup = getgrnam(Name.f_GetStr());
	
	if (!pGroup)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("getgrnam('{}') when setting group on file '{}'") << Name << Canonical, errno));
	
	if (chown(Canonical.f_GetStr(), -1, pGroup->gr_gid) != 0)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("chown('{}', {}) when setting group on file") << Canonical << pGroup->gr_gid, errno));
}

void NSys::NFile::fg_SetOwner(void *_pFile, const NMib::NStr::CStr &_Owner)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;
	CStr Name = _Owner;

	passwd *pPasswd = getpwnam(Name.f_GetStr());
	
	if (!pPasswd)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("getpwnam('{}') when setting owner on file '{}'") << Name << pFile->f_GetFileName(), errno));
	
	if (fchown(pFile->m_BSDFile, pPasswd->pw_uid, -1) != 0)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("fchown('{}', {}) when setting owner on file") << pFile->f_GetFileName() << pPasswd->pw_uid, errno));
}

void NSys::NFile::fg_SetGroup(void *_pFile, const NMib::NStr::CStr &_Group)
{
	CPOSIXFile *pFile = (CPOSIXFile *)_pFile;
	CStr Name = _Group;
	
	group *pGroup = getgrnam(Name.f_GetStr());
	
	if (!pGroup)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("getgrnam('{}') when setting group on file '{}'") << Name << pFile->f_GetFileName(), errno));
	
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

	passwd *pPasswd = getpwnam(Name.f_GetStr());

	if (!pPasswd)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("getpwnam('{}') when setting owner on link '{}'") << Name << Canonical, errno));

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

	group *pGroup = getgrnam(Name.f_GetStr());

	if (!pGroup)
		DMibErrorFile(NPlatform::fg_FormatErrno(CStr::CFormat("getgrnam('{}') when setting group on link '{}'") << Name << Canonical, errno));

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
	auto Attribs = 0;
	if (!lstat(FileName, &Stats))
	{
		NMib::NFile::EFileAttrib LinkAttribs = fsg_StatsToAttribs(Stats);
		Attribs |= LinkAttribs & NMib::NFile::EFileAttrib_Link;
	}
	if (stat(FileName, &Stats))
		return Attribs;
	Attribs |= fsg_StatsToAttribs(Stats);
	return Attribs;
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
	
	CPOSIXFileFind *pFind = DMibNew CPOSIXFileFind;
	
	pFind->m_FullPath = Path;
	pFind->m_SearchPattern = MatchStr;
	pFind->m_pDir = pDir;
	return pFind;
}

static bint fsg_MatchPattern(const ch8 *_pStr, const ch8 *_pPattern)
{
	NStr::CStr Temp0 = NStr::CStr(_pStr).f_UpperCase();
	NStr::CStr Temp1 = NStr::CStr(_pPattern).f_UpperCase();
	const char *pParse = Temp0;
	const char *pPattern = Temp1;
	mint nWildCardAttempt = 0; // This ensures that e.g. *.hcl matches 0x0409.Estonian.hcl
	bint bWildCardSearch = false;
	
	while (true)
	{
		while (*pParse && *pPattern)
		{
			if (*pPattern == '*')
			{
				bWildCardSearch = true;
				++pPattern;
				mint iWildCardAttempt = 0;
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
			bint bAccept = fsg_MatchPattern(FileName, pFind->m_SearchPattern);

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
	delete pFind;
}

