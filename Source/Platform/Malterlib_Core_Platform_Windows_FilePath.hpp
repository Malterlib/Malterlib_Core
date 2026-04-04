// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Windows.h>
#include <Mib/Core/PlatformSpecific/WindowsString>

namespace NMib
{
	namespace NFile
	{
		namespace NPlatform
		{
			template <typename tf_CRet, typename tf_CStr>
			tf_CRet fg_ConvertToWindowsPathLocal(const tf_CStr &_Path, bool _bForceLong)
			{
			#ifdef DMibAlwaysUseLongWindowsPaths
				_bForceLong = true;
			#endif
				if (_bForceLong)
					return fg_ConvertToWindowsPath<tf_CRet, tf_CRet>(_Path, true, -1);
				else
					return fg_ConvertToWindowsPath<tf_CRet, tf_CRet>(_Path, true, _MAX_PATH, false);
			}

			template <typename tf_CWindows, typename tf_CRet, typename tf_CSource>
			tf_CRet fg_ConvertToWindowsPath(const tf_CSource &_Path, bool _bAddCurrentDir, aint _MaxLen, bool _bTryShorten)
			{
				if (_Path.f_IsEmpty())
					return tf_CRet();

				if (_Path.f_StartsWith("//."))
				{
					return _Path.f_ReplaceChar('/', '\\');
				}
				auto ToRet = NFile::CFile::fs_GetExpandedPath(_Path, _bAddCurrentDir);
				fg_StrReplaceChar(ToRet, '\\', '/');

				if (ToRet.f_Cmp("//?/", 4) == 0)
				{
					ToRet = fg_ConvertFromWindowsPath<tf_CWindows, tf_CRet>(ToRet);
				}

				auto ToRetW = NStr::NPlatform::fg_StrToWindows<tf_CWindows>(ToRet);
				fg_StrReplaceChar(ToRetW, '/', '\\');

				aint MaxLenToUse = _MaxLen;
				if (MaxLenToUse > _MAX_PATH)
					MaxLenToUse = _MAX_PATH;

				if ((_MaxLen < 0 || ToRetW.f_GetLen() >= MaxLenToUse) && ((ToRet.f_GetLen() > 1 && ToRet[1] == ':') || ToRet.f_Cmp("\\\\", 2) != 0))
				{
					auto Path = NStr::NPlatform::fg_StrToWindows<tf_CWindows>(NFile::CFile::fs_GetPath(ToRet));
					auto File = NStr::NPlatform::fg_StrToWindows<tf_CWindows>(NFile::CFile::fs_GetFile(ToRet));

					if (_MaxLen > 0 && _bTryShorten)
					{
						auto TempW = NStr::NPlatform::fg_StrToWindows<tf_CWindows>(Path);
						fg_StrReplaceChar(TempW, '/', '\\');
						if (TempW.f_Cmp("\\\\", 2) == 0)
							TempW = "\\\\?\\UNC\\" + TempW.f_Extract(2);
						else
							TempW = "\\\\?\\" + TempW;
						umint NeededLen = GetShortPathNameW(TempW, nullptr, 0);
						if (NeededLen)
						{
							tf_CWindows ShortPathW;
							[[maybe_unused]] umint NeededLen2 = GetShortPathNameW(TempW, ShortPathW.f_GetStr(NeededLen), NeededLen);
							DMibSafeCheck(NeededLen2 <= NeededLen, "");
							TempW = ShortPathW + "\\" + File;
							if (TempW.f_CmpNoCase("\\\\?\\UNC\\", 8) == 0)
							{
								TempW = "\\\\" + TempW.f_Extract(8);
							}
							else if (TempW.f_CmpNoCase("\\\\?\\", 4) == 0)
							{
								TempW = TempW.f_Extract(4);
							}

							if (TempW.f_GetLen() < _MaxLen)
								return TempW;
						}
					}

					if (ToRetW.f_Cmp("\\\\", 2) == 0)
						ToRetW = "\\\\?\\UNC\\" + ToRetW.f_Extract(2);
					else
						ToRetW = "\\\\?\\" + ToRetW;

					if (_MaxLen > 0 && _bTryShorten)
					{
						umint NeededLen = GetShortPathNameW(ToRetW, nullptr, 0);
						if (NeededLen)
						{
							tf_CWindows ShortPathW;
							[[maybe_unused]] umint NeededLen2 = GetShortPathNameW(ToRetW, ShortPathW.f_GetStr(NeededLen), NeededLen);
							DMibSafeCheck(NeededLen2 <= NeededLen, "");
							auto TempW = ShortPathW;
							if (TempW.f_CmpNoCase("\\\\?\\UNC\\", 8) == 0)
							{
								TempW = "\\\\" + TempW.f_Extract(8);
							}
							else if (TempW.f_CmpNoCase("\\\\?\\", 4) == 0)
							{
								TempW = TempW.f_Extract(4);
							}
							if (TempW.f_GetLen() < _MaxLen)
								return TempW;
						}
					}

					return ToRetW;
				}

				return ToRetW;
			}

			template <typename tf_CWindows, typename tf_CRet, typename tf_CSource>
			tf_CRet fg_ConvertToShortWindowsPath(const tf_CSource &_Path, bool _bAddCurrentDir)
			{
				if (_Path.f_IsEmpty())
					return tf_CRet();
				if (_Path.f_StartsWith("//."))
				{
					return _Path.f_ReplaceChar('/', '\\');
				}

				auto ToRet = NFile::CFile::fs_GetExpandedPath(_Path, _bAddCurrentDir);
				fg_StrReplaceChar(ToRet, '\\', '/');

				if (ToRet.f_Cmp("//?/", 4) == 0)
				{
					ToRet = fg_ConvertFromWindowsPath<tf_CWindows, tf_CRet>(ToRet);
				}

				auto ToRetW = NStr::NPlatform::fg_StrToWindows<tf_CWindows>(ToRet);
				fg_StrReplaceChar(ToRetW, '/', '\\');
				umint NeededLen = GetShortPathNameW(ToRetW, nullptr, 0);
				if (NeededLen)
				{
					tf_CWindows ShortPathW;
					[[maybe_unused]] umint NeededLen2 = GetShortPathNameW(ToRetW, ShortPathW.f_GetStr(NeededLen), NeededLen);
					DMibSafeCheck(NeededLen2 <= NeededLen, "");
					auto TempW = ShortPathW;
					if (TempW.f_CmpNoCase("\\\\?\\UNC\\", 8) == 0)
					{
						TempW = "\\\\" + TempW.f_Extract(8);
					}
					else if (TempW.f_CmpNoCase("\\\\?\\", 4) == 0)
					{
						TempW = TempW.f_Extract(4);
					}
					return TempW;
				}
				return ToRetW;
			}

			template <typename tf_CWindows, typename tf_CRet, typename tf_CSource>
			tf_CRet fg_ConvertToLongWindowsPath(const tf_CSource &_Path, bool _bAddCurrentDir)
			{
				if (_Path.f_IsEmpty())
					return tf_CRet();
				auto ToRet = NFile::CFile::fs_GetExpandedPath(_Path, _bAddCurrentDir);
				fg_StrReplaceChar(ToRet, '\\', '/');

				if (ToRet.f_Cmp("//?/", 4) == 0)
				{
					ToRet = fg_ConvertFromWindowsPath<tf_CWindows, tf_CRet>(ToRet);
				}

				auto ToRetW = NStr::NPlatform::fg_StrToWindows<tf_CWindows>(ToRet);
				fg_StrReplaceChar(ToRetW, '/', '\\');
				umint NeededLen = GetLongPathNameW(ToRetW, nullptr, 0);
				if (NeededLen)
				{
					tf_CWindows LongPathW;
					[[maybe_unused]] umint NeededLen2 = GetLongPathNameW(ToRetW, LongPathW.f_GetStr(NeededLen), NeededLen);
					DMibSafeCheck(NeededLen2 <= NeededLen, "");
					return LongPathW;
				}
				return ToRetW;
			}

			template <typename tf_CRet>
			void fg_ConvertFromWindowsPathInternalDriveCase(tf_CRet &o_Path)
			{
				if (o_Path.f_GetLen() >= 2 && o_Path[1] == ':')
					o_Path[0] = NStr::fg_CharLowerCase(o_Path[0]);
			}

			template <typename tf_CRet, typename tf_CSrc>
			tf_CRet fg_ConvertFromWindowsPathInternal(const tf_CSrc &_Path)
			{
				auto ToRet = _Path;
				fg_StrReplaceChar(ToRet, '\\', '/');

				if (ToRet.f_CmpNoCase("//?/UNC/", 8) == 0)
				{
					auto Path = "//" + ToRet.f_Extract(8);
					fg_ConvertFromWindowsPathInternalDriveCase(Path);
					return Path;
				}
				else if (ToRet.f_CmpNoCase("//?/", 4) == 0)
				{
					auto Path = ToRet.f_Extract(4);
					fg_ConvertFromWindowsPathInternalDriveCase(Path);
					return Path;
				}

				auto Path = ToRet.f_TrimRight();
				fg_ConvertFromWindowsPathInternalDriveCase(Path);
				return Path;
			}

			template <typename tf_CWindows, typename tf_CRet, typename tf_CSrc>
			tf_CRet fg_ConvertFromWindowsPath(const tf_CSrc &_Path)
			{
				return fg_ConvertFromWindowsPathInternal<tf_CRet>(fg_ConvertToWindowsPath<tf_CWindows, tf_CRet>(fg_ConvertFromWindowsPathInternal<tf_CRet>(_Path), false, -1));
			}
		}
	}
}
