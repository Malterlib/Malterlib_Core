// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// -------------------------------------------------------------------------
typedef LONG NTSTATUS;

typedef VOID *POBJECT;

#define DMibAllowCodeStandardViolations 1

namespace NLocalWindows
{
	typedef struct _SYSTEM_HANDLE {
		ULONG		uIdProcess;
		UCHAR		ObjectType;    // OB_TYPE_* (OB_TYPE_TYPE, etc.)
		UCHAR		Flags;         // HANDLE_FLAG_* (HANDLE_FLAG_INHERIT, etc.)
		USHORT		Handle;
		POBJECT		pObject;
		ACCESS_MASK	GrantedAccess;
	} SYSTEM_HANDLE, *PSYSTEM_HANDLE;

	typedef struct _SYSTEM_HANDLE_INFORMATION {
		ULONG			uCount;
		SYSTEM_HANDLE	Handles[1];
	} SYSTEM_HANDLE_INFORMATION, *PSYSTEM_HANDLE_INFORMATION;


	typedef UNICODE_STRING OBJECT_NAME_INFORMATION;
	typedef UNICODE_STRING *POBJECT_NAME_INFORMATION;

	#define SystemHandleInformation			16

	typedef enum _OBJECT_INFORMATION_CLASS
	{
		ObjectBasicInformation,
		ObjectNameInformation,
		ObjectTypeInformation,
		ObjectAllTypesInformation,
		ObjectHandleInformation
	} OBJECT_INFORMATION_CLASS;
}

#define STATUS_SUCCESS					((NTSTATUS)0x00000000L)
#define STATUS_INFO_LENGTH_MISMATCH		((NTSTATUS)0xC0000004L)
#define STATUS_BUFFER_OVERFLOW			((NTSTATUS)0x80000005L)
// -------------------------------------------------------------------------

class CHandleInformation
{
	// PSAPI
	HMODULE m_PSAPIDll;
	BOOL (WINAPI *m_fEnumProcesses)(DWORD * lpidProcess, DWORD   cb, DWORD * cbNeeded);
	DWORD (WINAPI *m_fGetModuleBaseNameW)(HANDLE hProcess, HMODULE hModule, LPWSTR lpBaseName, DWORD nSize);
	DWORD (WINAPI *m_fGetModuleFileNameExW)(HANDLE hProcess, HMODULE hModule, LPWSTR lpFilename, DWORD nSize);
	DWORD (WINAPI *m_fGetProcessImageFileNameW)(HANDLE hProcess, LPWSTR lpImageFileName, DWORD nSize);


	// NTDLL
	HMODULE m_NTDLLDll;
	NTSTATUS (WINAPI *m_fNTQuerySystemInformation)(DWORD SystemInformationClass, PVOID SystemInformation, DWORD SystemInformationLength, PDWORD ReturnLength);
	NTSTATUS (WINAPI *m_fNTQueryObject)(HANDLE ObjectHandle, NLocalWindows::OBJECT_INFORMATION_CLASS ObjectInformationClass, PVOID ObjectInformation, DWORD Length, PDWORD ResultLength);


public:

	CHandleInformation()
	{
		m_PSAPIDll = LoadLibrary(str_utf16("psapi.dll"));
		if (m_PSAPIDll)
		{
			(FARPROC &)m_fEnumProcesses = GetProcAddress(m_PSAPIDll, "EnumProcesses");
			(FARPROC &)m_fGetModuleBaseNameW = GetProcAddress(m_PSAPIDll, "GetModuleBaseNameW");
			(FARPROC &)m_fGetModuleFileNameExW = GetProcAddress(m_PSAPIDll, "GetModuleFileNameExW");
			(FARPROC &)m_fGetProcessImageFileNameW = GetProcAddress(m_PSAPIDll, "GetProcessImageFileNameW");
		}

		m_NTDLLDll = NLocal::g_hNtDll;
		if (m_NTDLLDll)
		{
			(FARPROC &)m_fNTQuerySystemInformation = GetProcAddress(m_NTDLLDll, "NtQuerySystemInformation");
			(FARPROC &)m_fNTQueryObject = GetProcAddress(m_NTDLLDll, "NtQueryObject");
		}

		fp_EnableDebugPrivilege();
	}
	~CHandleInformation()
	{
		if (m_PSAPIDll)
		{
			FreeLibrary(m_PSAPIDll);
		}
		if (m_NTDLLDll)
			FreeLibrary(m_NTDLLDll);
	}

	class CHandleInfo
	{
	public:
		uint32 m_ProcessID;
		uint32 m_HandleID;
		CStr m_ProcessName;
		CStr m_HandleName;
	};


private:

	class CProcessingInfo
	{
	public:
		CProcessingInfo(CHandleInformation *_pThis, int32 _FileTypeID)
		{
			m_pThis = _pThis;
			m_hProcess = nullptr;
			m_hObject = nullptr;
			m_FileTypeID = _FileTypeID;
		}
		CHandleInformation *m_pThis;
		int32 m_FileTypeID;
		TCMap<uint32, TCVector<NLocalWindows::SYSTEM_HANDLE> >::CIterator m_Iter;
		TCVector<CHandleInfo> *m_pHandles;
		NThread::CMutual m_Lock;
		NThread::CEventAutoReset m_Event;

		HANDLE m_hProcess;
		HANDLE m_hObject;

		CByteVector m_DataBuffer;

		void f_Cleanup()
		{
			if (m_hObject)
				CloseHandle(m_hObject);
			if (m_hProcess)
				CloseHandle(m_hProcess);
			m_DataBuffer.f_Clear();
		}
	};

	NMib::NStr::CStr fp_GetObjectInfo(HANDLE hObject, NLocalWindows::OBJECT_INFORMATION_CLASS objInfoClass, CProcessingInfo &_ProcessInfo)
	{
		if (m_fNTQueryObject)
		{
			DWORD dwSize = sizeof(NLocalWindows::OBJECT_NAME_INFORMATION);
			CByteVector DataBuffer;
			_ProcessInfo.m_DataBuffer.f_SetLen(dwSize);
			NLocalWindows::POBJECT_NAME_INFORMATION pObjectInfo = (NLocalWindows::POBJECT_NAME_INFORMATION)_ProcessInfo.m_DataBuffer.f_GetArray();
			NTSTATUS ntReturn;
			{
				DMibUnlock(_ProcessInfo.m_Lock);
				ntReturn = m_fNTQueryObject(hObject, objInfoClass, pObjectInfo, dwSize, &dwSize);
			}
			if((ntReturn == STATUS_BUFFER_OVERFLOW) || (ntReturn == STATUS_INFO_LENGTH_MISMATCH))
			{
				_ProcessInfo.m_DataBuffer.f_SetLen(dwSize);
				pObjectInfo = (NLocalWindows::POBJECT_NAME_INFORMATION)_ProcessInfo.m_DataBuffer.f_GetArray();
				{
					DMibUnlock(_ProcessInfo.m_Lock);
					ntReturn = m_fNTQueryObject(hObject, objInfoClass, pObjectInfo, dwSize, &dwSize);
				}
			}
			if((ntReturn >= STATUS_SUCCESS) && (pObjectInfo->Buffer != nullptr))
			{
				NMib::NStr::CWStr TempStr;
				umint StrLen = fg_StrLen(pObjectInfo->Buffer, pObjectInfo->Length/2);
				TempStr.f_AddStr(pObjectInfo->Buffer, StrLen);
				return NFile::NPlatform::fg_ConvertFromWindowsPath(TempStr);
			}
		}
		return NMib::NStr::CWStr();
	}

	static DWORD NTAPI fsp_ProcessThread(CProcessingInfo *_pProcessInfo)
	{

		DMibLock(_pProcessInfo->m_Lock);
		while (_pProcessInfo->m_Iter)
		{
			TCVector<NLocalWindows::SYSTEM_HANDLE> &Iter = *_pProcessInfo->m_Iter;
			DWORD Process = _pProcessInfo->m_Iter.f_GetKey();
			_pProcessInfo->m_hProcess = OpenProcess(PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,	FALSE, Process);

			if(_pProcessInfo->m_hProcess && _pProcessInfo->m_hProcess != INVALID_HANDLE_VALUE)
			{
				umint nHandles = Iter.f_GetLen();
				for (umint i = 0; i < nHandles; ++i)
				{
					NLocalWindows::SYSTEM_HANDLE &SysHandle = Iter[i];
					if (SysHandle.ObjectType != _pProcessInfo->m_FileTypeID)
						continue;
					_pProcessInfo->m_hObject = nullptr;
					if(DuplicateHandle(_pProcessInfo->m_hProcess, (HANDLE)(umint)SysHandle.Handle, GetCurrentProcess(), &_pProcessInfo->m_hObject, STANDARD_RIGHTS_REQUIRED, FALSE, DUPLICATE_SAME_ACCESS) != FALSE)
					{
						CStr Name = _pProcessInfo->m_pThis->fp_GetObjectInfo(_pProcessInfo->m_hObject, NLocalWindows::ObjectNameInformation, *_pProcessInfo);

						if (!Name.f_IsEmpty())
						{
							CHandleInfo &Handle = _pProcessInfo->m_pHandles->f_Insert();
							Handle.m_ProcessName = _pProcessInfo->m_pThis->fp_GetProcessName(_pProcessInfo->m_hProcess);
							Handle.m_ProcessID = Process;
							Handle.m_HandleName = Name;
							Handle.m_HandleID = SysHandle.Handle;
						}
						CloseHandle(_pProcessInfo->m_hObject);
						_pProcessInfo->m_hObject = nullptr;
					}
					_pProcessInfo->m_Event.f_Signal();
				}
				CloseHandle(_pProcessInfo->m_hProcess);
				_pProcessInfo->m_hProcess = nullptr;
			}
			++_pProcessInfo->m_Iter;
		}

		return 0;
	}

	void fp_EnableDebugPrivilege()
	{
		HANDLE hToken;
		TOKEN_PRIVILEGES tokenPriv;
		LUID luidDebug;
		if(OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken) != FALSE)
		{
			if(LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &luidDebug) != FALSE)
			{
				tokenPriv.PrivilegeCount           = 1;
				tokenPriv.Privileges[0].Luid       = luidDebug;
				tokenPriv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
				if (AdjustTokenPrivileges(hToken, FALSE, &tokenPriv, sizeof(tokenPriv), nullptr, nullptr) != FALSE)
				{
					DMibDTraceSafe("AdjustTokenPrivileges successful\r\n", 0);
				}
			}
			CloseHandle(hToken);
		}
	}

	NMib::NStr::CStr fp_GetProcessName(HANDLE _hProcess)
	{
		CWStr BaseName;
		if (m_fGetProcessImageFileNameW)
			m_fGetProcessImageFileNameW(_hProcess, BaseName.f_GetStr(1024), 1024);
		else
		if (m_fGetModuleFileNameExW)
			m_fGetModuleFileNameExW(_hProcess, nullptr, BaseName.f_GetStr(1024), 1024);
		else if (m_fGetModuleBaseNameW)
			m_fGetModuleBaseNameW(_hProcess, nullptr, BaseName.f_GetStr(1024), 1024);

		return NFile::NPlatform::fg_ConvertFromWindowsPath(BaseName);
	}


public:

	void f_EnumHandles(TCVector<CHandleInfo> &_Handles)
	{

		if (!m_fNTQuerySystemInformation)
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


		DWORD dwSize = 16*1024*1024;
		NLocalWindows::PSYSTEM_HANDLE_INFORMATION pHandleInfo = (NLocalWindows::PSYSTEM_HANDLE_INFORMATION) DMibNew BYTE[dwSize];
		NTSTATUS ntReturn = m_fNTQuerySystemInformation(SystemHandleInformation, pHandleInfo, dwSize, &dwSize);
		if(ntReturn == STATUS_INFO_LENGTH_MISMATCH)
		{
			delete [] pHandleInfo;
			pHandleInfo = (NLocalWindows::PSYSTEM_HANDLE_INFORMATION) DMibNew BYTE[dwSize];
			ntReturn = m_fNTQuerySystemInformation(SystemHandleInformation, pHandleInfo, dwSize, &dwSize);
		}
		if(ntReturn == STATUS_INFO_LENGTH_MISMATCH)
		{
			dwSize *= 2;
			delete [] pHandleInfo;
			pHandleInfo = (NLocalWindows::PSYSTEM_HANDLE_INFORMATION) DMibNew BYTE[dwSize];
			ntReturn = m_fNTQuerySystemInformation(SystemHandleInformation, pHandleInfo, dwSize, &dwSize);
		}
		if(ntReturn == STATUS_SUCCESS)
		{
			TCMap<uint32, TCVector<NLocalWindows::SYSTEM_HANDLE> > ProcessMap;
			for(DWORD dwIdx = 0; dwIdx < pHandleInfo->uCount; dwIdx++)
			{
				DWORD Process = pHandleInfo->Handles[dwIdx].uIdProcess;
				ProcessMap[Process].f_Insert(pHandleInfo->Handles[dwIdx]);
			}


			CProcessingInfo ProcessInfo(this, FileTypeID);
			ProcessInfo.m_pHandles = &_Handles;
			DWORD tid;
			HANDLE hthread;
			{
				DMibLock(ProcessInfo.m_Lock);
				hthread = CreateThread(nullptr, 0,	(LPTHREAD_START_ROUTINE)fsp_ProcessThread, &ProcessInfo, 0, &tid);
				ProcessInfo.m_Iter = ProcessMap;
				while (ProcessInfo.m_Iter)
				{
					bool bWasTimeout;
					{
						DMibUnlock(ProcessInfo.m_Lock);
						bWasTimeout = ProcessInfo.m_Event.f_WaitTimeout(0.05f);
					}
					if (bWasTimeout)
					{
						DMibDTraceSafe("Killed Thread!!\r\n", 0);
						TerminateThread(hthread, 0);
						CloseHandle (hthread);
						ProcessInfo.f_Cleanup();
						++ProcessInfo.m_Iter;
						hthread = CreateThread(nullptr, 0,	(LPTHREAD_START_ROUTINE)fsp_ProcessThread, &ProcessInfo, 0, &tid);
					}
				}
			}
			WaitForSingleObject(hthread, INFINITE);
			CloseHandle (hthread);
		}

		delete [] pHandleInfo;
	}


};



