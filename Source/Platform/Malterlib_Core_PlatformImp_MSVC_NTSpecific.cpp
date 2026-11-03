// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// -------------------------------------------------------------------------
typedef LONG NTSTATUS;

// NOLINTBEGIN(malterlib-variable-naming): the declarations of the Windows SDK, with their names
// NOLINTBEGIN(malterlib-type-naming): the names in the Windows headers
typedef VOID *POBJECT;

#define DMibAllowCodeStandardViolations 1

namespace NLocalWindows
{
	typedef struct _SYSTEM_HANDLE
	{
		ULONG uIdProcess;
		UCHAR ObjectType; // OB_TYPE_* (OB_TYPE_TYPE, etc.)
		UCHAR Flags;      // HANDLE_FLAG_* (HANDLE_FLAG_INHERIT, etc.)
		USHORT Handle;
		POBJECT pObject;
		ACCESS_MASK GrantedAccess;
	} SYSTEM_HANDLE, *PSYSTEM_HANDLE;

	typedef struct _SYSTEM_HANDLE_INFORMATION
	{
		ULONG uCount;
		SYSTEM_HANDLE Handles[1];
	} SYSTEM_HANDLE_INFORMATION, *PSYSTEM_HANDLE_INFORMATION;

	typedef UNICODE_STRING OBJECT_NAME_INFORMATION;
	typedef UNICODE_STRING *POBJECT_NAME_INFORMATION;

#define DSystemHandleInformation			16

	typedef enum _OBJECT_INFORMATION_CLASS
	{
		ObjectBasicInformation
		, ObjectNameInformation
		, ObjectTypeInformation
		, ObjectAllTypesInformation
		, ObjectHandleInformation
	} OBJECT_INFORMATION_CLASS;
}

// NOLINTEND(malterlib-type-naming)
// NOLINTEND(malterlib-variable-naming)

#define STATUS_SUCCESS					((NTSTATUS)0x00000000L) // NOLINT(malterlib-macro-naming)
#define STATUS_INFO_LENGTH_MISMATCH		((NTSTATUS)0xC0000004L) // NOLINT(malterlib-macro-naming)
#define STATUS_BUFFER_OVERFLOW			((NTSTATUS)0x80000005L) // NOLINT(malterlib-macro-naming)
// -------------------------------------------------------------------------

struct CHandleInformation
{
	CHandleInformation()
	{
		mp_pPSAPIDll = LoadLibrary(str_utf16("psapi.dll"));
		if (mp_pPSAPIDll)
		{
			(FARPROC &)mp_fEnumProcesses = GetProcAddress(mp_pPSAPIDll, "EnumProcesses");
			(FARPROC &)mp_fGetModuleBaseNameW = GetProcAddress(mp_pPSAPIDll, "GetModuleBaseNameW");
			(FARPROC &)mp_fGetModuleFileNameExW = GetProcAddress(mp_pPSAPIDll, "GetModuleFileNameExW");
			(FARPROC &)mp_fGetProcessImageFileNameW = GetProcAddress(mp_pPSAPIDll, "GetProcessImageFileNameW");
		}

		mp_pNTDLLDll = NLocal::g_pNtDll;
		if (mp_pNTDLLDll)
		{
			(FARPROC &)mp_fNTQuerySystemInformation = GetProcAddress(mp_pNTDLLDll, "NtQuerySystemInformation");
			(FARPROC &)mp_fNTQueryObject = GetProcAddress(mp_pNTDLLDll, "NtQueryObject");
		}

		fp_EnableDebugPrivilege();
	}

	~CHandleInformation()
	{
		if (mp_pPSAPIDll)
			FreeLibrary(mp_pPSAPIDll);
		if (mp_pNTDLLDll)
			FreeLibrary(mp_pNTDLLDll);
	}

	struct CHandleInfo
	{
		uint32 m_ProcessID;
		uint32 m_HandleID;
		CStr m_ProcessName;
		CStr m_HandleName;
	};

private:
	struct CProcessingInfo
	{
		CProcessingInfo(CHandleInformation *_pThis, int32 _FileTypeID)
		{
			m_pThis = _pThis;
			m_pProcess = nullptr;
			m_pObject = nullptr;
			m_FileTypeID = _FileTypeID;
		}

		CHandleInformation *m_pThis;
		int32 m_FileTypeID;
		TCMap<uint32, TCVector<NLocalWindows::SYSTEM_HANDLE>>::CIterator m_iProcess;
		TCVector<CHandleInfo> *m_pHandles;
		NThread::CMutual m_Lock;
		NThread::CEventAutoReset m_Event;

		HANDLE m_pProcess;
		HANDLE m_pObject;

		CByteVector m_DataBuffer;

		void f_Cleanup()
		{
			if (m_pObject)
				CloseHandle(m_pObject);
			if (m_pProcess)
				CloseHandle(m_pProcess);
			m_DataBuffer.f_Clear();
		}
	};

	NMib::NStr::CStr fp_GetObjectInfo(HANDLE _pObject, NLocalWindows::OBJECT_INFORMATION_CLASS _ObjectInfoClass, CProcessingInfo &_ProcessInfo)
	{
		if (mp_fNTQueryObject)
		{
			DWORD Size = sizeof(NLocalWindows::OBJECT_NAME_INFORMATION);
			CByteVector DataBuffer;
			_ProcessInfo.m_DataBuffer.f_SetLen(Size);
			NLocalWindows::POBJECT_NAME_INFORMATION pObjectInfo = (NLocalWindows::POBJECT_NAME_INFORMATION)_ProcessInfo.m_DataBuffer.f_GetArray();
			NTSTATUS Status;
			{
				DMibUnlock(_ProcessInfo.m_Lock);
				Status = mp_fNTQueryObject(_pObject, _ObjectInfoClass, pObjectInfo, Size, &Size);
			}
			if ((Status == STATUS_BUFFER_OVERFLOW) || (Status == STATUS_INFO_LENGTH_MISMATCH))
			{
				_ProcessInfo.m_DataBuffer.f_SetLen(Size);
				pObjectInfo = (NLocalWindows::POBJECT_NAME_INFORMATION)_ProcessInfo.m_DataBuffer.f_GetArray();
				{
					DMibUnlock(_ProcessInfo.m_Lock);
					Status = mp_fNTQueryObject(_pObject, _ObjectInfoClass, pObjectInfo, Size, &Size);
				}
			}
			if ((Status >= STATUS_SUCCESS) && (pObjectInfo->Buffer != nullptr))
			{
				NMib::NStr::CWStr TempStr;
				umint StrLen = fg_StrLen(pObjectInfo->Buffer, pObjectInfo->Length / 2);
				TempStr.f_AddStr(pObjectInfo->Buffer, StrLen);
				return NFile::NPlatform::fg_ConvertFromWindowsPath(TempStr);
			}
		}
		return NMib::NStr::CWStr();
	}

	static DWORD NTAPI fsp_ProcessThread(CProcessingInfo *_pProcessInfo)
	{
		DMibLock(_pProcessInfo->m_Lock);
		while (_pProcessInfo->m_iProcess)
		{
			TCVector<NLocalWindows::SYSTEM_HANDLE> &Iter = *_pProcessInfo->m_iProcess;
			DWORD Process = _pProcessInfo->m_iProcess.f_GetKey();
			_pProcessInfo->m_pProcess = OpenProcess(PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, Process);

			if (_pProcessInfo->m_pProcess && _pProcessInfo->m_pProcess != INVALID_HANDLE_VALUE)
			{
				umint nHandles = Iter.f_GetLen();
				for (umint iHandle = 0; iHandle < nHandles; ++iHandle)
				{
					NLocalWindows::SYSTEM_HANDLE &SysHandle = Iter[iHandle];
					if (SysHandle.ObjectType != _pProcessInfo->m_FileTypeID)
						continue;
					_pProcessInfo->m_pObject = nullptr;
					if
					(
						DuplicateHandle
						(
							_pProcessInfo->m_pProcess
							, (HANDLE)(umint)SysHandle.Handle
							, GetCurrentProcess()
							, &_pProcessInfo->m_pObject
							, STANDARD_RIGHTS_REQUIRED
							, FALSE
							, DUPLICATE_SAME_ACCESS
						)
						!= FALSE
					)
					{
						CStr Name = _pProcessInfo->m_pThis->fp_GetObjectInfo(_pProcessInfo->m_pObject, NLocalWindows::ObjectNameInformation, *_pProcessInfo);

						if (!Name.f_IsEmpty())
						{
							CHandleInfo &Handle = _pProcessInfo->m_pHandles->f_Insert();
							Handle.m_ProcessName = _pProcessInfo->m_pThis->fp_GetProcessName(_pProcessInfo->m_pProcess);
							Handle.m_ProcessID = Process;
							Handle.m_HandleName = Name;
							Handle.m_HandleID = SysHandle.Handle;
						}
						CloseHandle(_pProcessInfo->m_pObject);
						_pProcessInfo->m_pObject = nullptr;
					}
					_pProcessInfo->m_Event.f_Signal();
				}
				CloseHandle(_pProcessInfo->m_pProcess);
				_pProcessInfo->m_pProcess = nullptr;
			}
			++_pProcessInfo->m_iProcess;
		}

		return 0;
	}

	void fp_EnableDebugPrivilege()
	{
		HANDLE pToken;
		TOKEN_PRIVILEGES TokenPrivileges;
		LUID DebugLuid;
		if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &pToken) != FALSE)
		{
			if (LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &DebugLuid) != FALSE)
			{
				TokenPrivileges.PrivilegeCount = 1;
				TokenPrivileges.Privileges[0].Luid = DebugLuid;
				TokenPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
				if (AdjustTokenPrivileges(pToken, FALSE, &TokenPrivileges, sizeof(TokenPrivileges), nullptr, nullptr) != FALSE)
					DMibDTraceSafe("AdjustTokenPrivileges successful\r\n", 0);
			}
			CloseHandle(pToken);
		}
	}

	NMib::NStr::CStr fp_GetProcessName(HANDLE _pProcess)
	{
		CWStr BaseName;
		if (mp_fGetProcessImageFileNameW)
			mp_fGetProcessImageFileNameW(_pProcess, BaseName.f_GetStr(1024), 1024);
		else if (mp_fGetModuleFileNameExW)
			mp_fGetModuleFileNameExW(_pProcess, nullptr, BaseName.f_GetStr(1024), 1024);
		else if (mp_fGetModuleBaseNameW)
			mp_fGetModuleBaseNameW(_pProcess, nullptr, BaseName.f_GetStr(1024), 1024);

		return NFile::NPlatform::fg_ConvertFromWindowsPath(BaseName);
	}

public:
	void f_EnumHandles(TCVector<CHandleInfo> &_Handles)
	{
		if (!mp_fNTQuerySystemInformation)
			return;

		int32 FileTypeID = 28;
		if (NLocal::g_VersionInfo.dwMajorVersion == 5 && NLocal::g_VersionInfo.dwMinorVersion == 1)
		{
			// XP
			FileTypeID = 28;
		}
		else if (NLocal::g_VersionInfo.dwMajorVersion == 5)
		{
			// 2000
			FileTypeID = 26;
		}
		else if (NLocal::g_VersionInfo.dwMajorVersion == 6 && NLocal::g_VersionInfo.dwMinorVersion == 1)
		{
			// Windows 7
			FileTypeID = 28;
		}
		else if (NLocal::g_VersionInfo.dwMajorVersion == 6)
		{
			// Vista
			FileTypeID = 25;
		}
		else
			return; // Unsupported

		DWORD Size = 16 * 1024 * 1024;
		NLocalWindows::PSYSTEM_HANDLE_INFORMATION pHandleInfo = (NLocalWindows::PSYSTEM_HANDLE_INFORMATION) DMibNew BYTE[Size];
		NTSTATUS Status = mp_fNTQuerySystemInformation(DSystemHandleInformation, pHandleInfo, Size, &Size);
		if (Status == STATUS_INFO_LENGTH_MISMATCH)
		{
			delete [] pHandleInfo;
			pHandleInfo = (NLocalWindows::PSYSTEM_HANDLE_INFORMATION) DMibNew BYTE[Size];
			Status = mp_fNTQuerySystemInformation(DSystemHandleInformation, pHandleInfo, Size, &Size);
		}
		if (Status == STATUS_INFO_LENGTH_MISMATCH)
		{
			Size *= 2;
			delete [] pHandleInfo;
			pHandleInfo = (NLocalWindows::PSYSTEM_HANDLE_INFORMATION) DMibNew BYTE[Size];
			Status = mp_fNTQuerySystemInformation(DSystemHandleInformation, pHandleInfo, Size, &Size);
		}
		if (Status == STATUS_SUCCESS)
		{
			TCMap<uint32, TCVector<NLocalWindows::SYSTEM_HANDLE>> ProcessMap;
			for (DWORD iHandle = 0; iHandle < pHandleInfo->uCount; iHandle++)
			{
				DWORD Process = pHandleInfo->Handles[iHandle].uIdProcess;
				ProcessMap[Process].f_Insert(pHandleInfo->Handles[iHandle]);
			}

			CProcessingInfo ProcessInfo(this, FileTypeID);
			ProcessInfo.m_pHandles = &_Handles;
			DWORD ThreadID;
			HANDLE pThread;
			{
				DMibLock(ProcessInfo.m_Lock);
				pThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)fsp_ProcessThread, &ProcessInfo, 0, &ThreadID);
				ProcessInfo.m_iProcess = ProcessMap;
				while (ProcessInfo.m_iProcess)
				{
					bool bWasTimeout;
					{
						DMibUnlock(ProcessInfo.m_Lock);
						bWasTimeout = ProcessInfo.m_Event.f_WaitTimeout(0.05f);
					}
					if (bWasTimeout)
					{
						DMibDTraceSafe("Killed Thread!!\r\n", 0);
						TerminateThread(pThread, 0);
						CloseHandle(pThread);
						ProcessInfo.f_Cleanup();
						++ProcessInfo.m_iProcess;
						pThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)fsp_ProcessThread, &ProcessInfo, 0, &ThreadID);
					}
				}
			}
			WaitForSingleObject(pThread, INFINITE);
			CloseHandle(pThread);
		}

		delete [] pHandleInfo;
	}

private:
	// PSAPI
	HMODULE mp_pPSAPIDll;
	BOOL (WINAPI *mp_fEnumProcesses)(DWORD *o_pProcessIDs, DWORD _nBytes, DWORD *o_pBytesNeeded);
	DWORD (WINAPI *mp_fGetModuleBaseNameW)(HANDLE _pProcess, HMODULE _pModule, LPWSTR o_pBaseName, DWORD _nSize);
	DWORD (WINAPI *mp_fGetModuleFileNameExW)(HANDLE _pProcess, HMODULE _pModule, LPWSTR o_pFileName, DWORD _nSize);
	DWORD (WINAPI *mp_fGetProcessImageFileNameW)(HANDLE _pProcess, LPWSTR o_pImageFileName, DWORD _nSize);

	// NTDLL
	HMODULE mp_pNTDLLDll;
	NTSTATUS (WINAPI *mp_fNTQuerySystemInformation)(DWORD _SystemInformationClass, PVOID o_pSystemInformation, DWORD _SystemInformationLength, PDWORD o_pReturnLength);
	NTSTATUS (WINAPI *mp_fNTQueryObject)(HANDLE _pObject, NLocalWindows::OBJECT_INFORMATION_CLASS _ObjectInformationClass, PVOID o_pObjectInformation, DWORD _Length, PDWORD o_pResultLength);
};

