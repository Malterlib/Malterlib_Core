// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_Platform_Windows_FilePath.h"
#include <Mib/Core/PlatformSpecific/WindowsString>
#include <Mib/Core/PlatformSpecific/WindowsError>

namespace NMib
{
	namespace NFile
	{
		namespace NPlatform
		{
			NStr::CWStr fg_ConvertToWindowsPathLocal(const NStr::CStr &_Path, bool _bForceLong)
			{
			#ifdef DMibAlwaysUseLongWindowsPaths
				_bForceLong = true;
			#endif
				if (_bForceLong)
					return fg_ConvertToWindowsPath(_Path, true, -1);
				else
					return fg_ConvertToWindowsPath(_Path, true, _MAX_PATH, false);
			}

			NStr::CWStr fg_ConvertToShortWindowsPath(const NStr::CStr &_Path, bint _bAddCurrentDir)
			{
				return fg_ConvertToShortWindowsPath<NStr::CWStr, NStr::CWStr>(_Path, _bAddCurrentDir);
			}


			NStr::CWStr fg_ConvertToLongWindowsPath(const NStr::CStr &_Path, bint _bAddCurrentDir)
			{
				return fg_ConvertToLongWindowsPath<NStr::CWStr, NStr::CWStr>(_Path, _bAddCurrentDir);
			}

			NStr::CWStr fg_ConvertToWindowsPath(const NStr::CStr &_Path, bint _bAddCurrentDir, aint _MaxLen)
			{
				return fg_ConvertToWindowsPath<NStr::CWStr, NStr::CWStr>(_Path, _bAddCurrentDir, _MaxLen, true);
			}

			NStr::CWStr fg_ConvertToWindowsPath(const NStr::CStr &_Path, bint _bAddCurrentDir, aint _MaxLen, bool _bTryShorten)
			{
				return fg_ConvertToWindowsPath<NStr::CWStr, NStr::CWStr>(_Path, _bAddCurrentDir, _MaxLen, _bTryShorten);
			}


			NStr::CStr fg_ConvertFromWindowsPath(const NStr::CWStr &_Path)
			{
				NStr::CStr ToRet = NStr::NPlatform::fg_StrFromWindows(_Path);
				return fg_ConvertFromWindowsPath(ToRet);
			}

			NStr::CStr fg_ConvertFromWindowsPath(const NStr::CStr &_Path)
			{
				return fg_ConvertFromWindowsPathInternal<NStr::CStr>(fg_ConvertToWindowsPath<NStr::CWStr, NStr::CWStr, NStr::CStr>(fg_ConvertFromWindowsPathInternal<NStr::CStr>(_Path), false, -1));
			}

			NStr::CStr fg_ConvertToDevicePath(NStr::CStr const &_In)
			{
				NStr::CWStr DeviceName;
				NStr::CWStr Drive = NFile::CFile::fs_GetDrive(_In);
				mint Len = 256;
				bool bFailed = false;
				while (!QueryDosDevice(Drive, DeviceName.f_GetStr(Len), Len))
				{
					if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
					{
						bFailed = true;
						break;
					}
					Len *= 2;
				}

				if (bFailed)
				{
					auto Error = GetLastError();
					DMibError(NStr::CStr::CFormat("In sandbox QueryDosDevice failed: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
				}

				return _In.f_Replace(Drive, DeviceName).f_ReplaceChar('/', '\\');
			}

			void fg_RemoveDosDevice(NStr::CStr const &_Device)
			{
				using namespace NStr;
				uint32 Flags = DDD_REMOVE_DEFINITION;

				if (!DefineDosDevice(Flags, NStr::CWStr(_Device), nullptr))
				{
					auto Error = GetLastError();
					DMibErrorFile("Windows returned an error from DefineDosDevice(0x{nfh}, {}, nullptr): {}"_f << Flags << _Device << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
				}
			}

			void fg_DefineDosDevice(NStr::CStr const &_Device, NStr::CStr const &_Path)
			{
				using namespace NStr;
				uint32 Flags = DDD_RAW_TARGET_PATH;

				if (!DefineDosDevice(Flags, NStr::CWStr(_Device), NStr::CWStr(_Path)))
				{
					auto Error = GetLastError();
					DMibErrorFile(NStr::CStr::CFormat("Windows returned an error from DefineDosDevice(0x{nfh}, {}, {}): {}") << Flags << _Device << _Path << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
				}
			}

			NStr::CStr fg_ConvertToMinGWPath(NStr::CStr const &_In)
			{
				return NStr::fg_Format("/{}/{}", CFile::fs_GetDrive(_In).f_Left(1).f_LowerCase(), _In.f_Extract(3));
			}
		}
	}
}
