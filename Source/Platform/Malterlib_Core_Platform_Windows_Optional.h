// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Windows.h>
#include <winternl.h>

#include "Malterlib_Core_Platform_Windows_Undocumented.h"

namespace NLocal
{
	extern HMODULE g_hNtDll;
	extern HMODULE g_hKernel32;
	extern HMODULE g_hAdvAPI32;

	struct COptionalFunctions
	{
		NTSTATUS (NTAPI *m_fNtQueryInformationThread)
			(
				IN HANDLE _ThreadHandle,
				IN THREADINFOCLASS _ThreadInformationClass,
				OUT PVOID _ThreadInformation,
				IN ULONG _ThreadInformationLength,
				OUT PULONG _ReturnLength OPTIONAL
			)
		;

		BOOL (WINAPI *m_fGetLogicalProcessorInformation)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION _pBuffer, PDWORD _pReturnLength);
		VOID (WINAPI *m_fRtlAcquirePebLock)(void);
		VOID (WINAPI *m_fRtlReleasePebLock)(void);
		ULONG  (WINAPI *m_fRtlFindClearBitsAndSet)(IN PRTL_BITMAP  _pBitMapHeader, IN ULONG  _NumberToFind, IN ULONG  _HintIndex);
		VOID (WINAPI *m_fRtlClearBits)(IN PRTL_BITMAP  _pBitMapHeader, IN ULONG  _StartingIndex, IN ULONG  _NumberToClear);

		void (WINAPI *m_fGetNativeSystemInfo)(__out LPSYSTEM_INFO _pSystemInfo);
#if !defined(DMibSanitizerEnabled_Address) && !defined(DArchitecture_arm64)
		PVOID (WINAPI *m_fAddVectoredExceptionHandler)(ULONG _First, PVECTORED_EXCEPTION_HANDLER _pHandler);
		ULONG (WINAPI *m_fRemoveVectoredExceptionHandler)(PVOID _pHandler);
#endif

		SIZE_T (WINAPI *m_fLargePageMinimum)();

		char *(WINAPI *m_fWineGetVersion)();

		void *m_pKiUserApcDispatcher;
		void *m_pKiUserCallbackDispatcher;

		// Numa functions
		LPVOID (WINAPI *m_fVirtualAllocExNuma)(HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD  flAllocationType, DWORD  flProtect, DWORD  nndPreferred);
		BOOL (WINAPI *m_fGetNumaNodeProcessorMaskEx)(USHORT Node, PGROUP_AFFINITY ProcessorMask);
		BOOL (WINAPI *m_fSetThreadGroupAffinity)(HANDLE hThread, const GROUP_AFFINITY *GroupAffinity, PGROUP_AFFINITY PreviousGroupAffinity);

		BOOL (WINAPI *m_fSetProcessUserModeExceptionPolicy)(DWORD dwFlags);
		BOOL (WINAPI *m_fGetProcessUserModeExceptionPolicy)(LPDWORD lpFlags);

		DWORD (WINAPI *m_fWTSGetActiveConsoleSessionId)();

		BOOL (WINAPI *m_fCreateProcessWithTokenW)(HANDLE hToken, DWORD dwLogonFlags, LPCWSTR lpApplicationName, LPWSTR lpCommandLine, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation );

		BOOLEAN (APIENTRY *m_fCreateSymbolicLinkW)(LPCWSTR lpSymlinkFileName, LPCWSTR lpTargetFileName, DWORD dwFlags);

		BOOL (WINAPI *m_fCreateHardLinkW)(LPCWSTR lpFileName, LPCWSTR lpExistingFileName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);

		BOOL (WINAPI *m_fWow64DisableWow64FsRedirection)(PVOID * OldValue);
		BOOL (WINAPI *m_fWow64RevertWow64FsRedirection)(PVOID OlValue);

		NTSTATUS (NTAPI *m_fNtSetInformationProcess)(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass, PVOID pProcessInformation, ULONG ProcessInformationLength);

		BOOL (WINAPI *m_fSetProcessInformation)(HANDLE hProcess, PROCESS_INFORMATION_CLASS ProcessInformationClass, LPVOID ProcessInformation, DWORD ProcessInformationSize);

		BOOL (WINAPI *m_fCancelSynchronousIo)(HANDLE hThread);
		BOOL (WINAPI *m_fCancelIoEx)(HANDLE hFile, LPOVERLAPPED lpOverlapped);

		// The native entry points the io loop drives the AFD device and completion bindings with
		NTSTATUS (NTAPI *m_fNtCreateFile)
			(
				PHANDLE _phFile
				, ACCESS_MASK _DesiredAccess
				, POBJECT_ATTRIBUTES _pObjectAttributes
				, PIO_STATUS_BLOCK _pIoStatusBlock
				, PLARGE_INTEGER _pAllocationSize
				, ULONG _FileAttributes
				, ULONG _ShareAccess
				, ULONG _CreateDisposition
				, ULONG _CreateOptions
				, PVOID _pEaBuffer
				, ULONG _EaLength
			)
		;
		NTSTATUS (NTAPI *m_fNtDeviceIoControlFile)
			(
				HANDLE _hFile
				, HANDLE _hEvent
				, PIO_APC_ROUTINE _fApcRoutine
				, PVOID _pApcContext
				, PIO_STATUS_BLOCK _pIoStatusBlock
				, ULONG _IoControlCode
				, PVOID _pInputBuffer
				, ULONG _InputBufferLength
				, PVOID _pOutputBuffer
				, ULONG _OutputBufferLength
			)
		;
		NTSTATUS (NTAPI *m_fNtSetInformationFile)(HANDLE _hFile, PIO_STATUS_BLOCK _pIoStatusBlock, PVOID _pFileInformation, ULONG _Length, FILE_INFORMATION_CLASS _FileInformationClass);
		ULONG (NTAPI *m_fRtlNtStatusToDosError)(NTSTATUS _Status);

		NTSTATUS (WINAPI *m_fNtQuerySystemInformation)(DWORD SystemInformationClass, PVOID SystemInformation, DWORD SystemInformationLength, PDWORD ReturnLength);

		NTSTATUS (WINAPI *m_fNtGetNextThread)(HANDLE ProcessHandle, HANDLE ThreadHandle, ACCESS_MASK DesiredAccess, ULONG HandleAttributes, ULONG Flags, PHANDLE NewThreadHandle);

		DWORD (WINAPI *m_fGetThreadId)(HANDLE Thread);

		HRESULT (WINAPI *m_fSetThreadDescription)(HANDLE hThread, PCWSTR lpThreadDescription);

		NTSTATUS (WINAPI *m_fNtQueryInformationProcess)(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength);

		NTSTATUS (WINAPI *m_fLdrDisableThreadCalloutsForDll)(IN PVOID BaseAddress);
		NTSTATUS (WINAPI *m_fRtlGetVersion)(PRTL_OSVERSIONINFOW lpVersionInformation);
		BOOL (WINAPI *m_fPrivIsDllSynchronizationHeld)(PBOOL);

		BOOL (WINAPI *m_fGetFileInformationByHandleEx)(HANDLE hFile, Undocumented_FILE_INFO_BY_HANDLE_CLASS FileInformationClass, LPVOID lpFileInformation, DWORD dwBufferSize);


		BOOL (WINAPI *m_fSystemTimeToTzSpecificLocalTimeEx)(const DYNAMIC_TIME_ZONE_INFORMATION *lpTimeZoneInformation, SYSTEMTIME const *lpUniversalTime, LPSYSTEMTIME lpLocalTime);
		BOOL (WINAPI *m_fTzSpecificLocalTimeToSystemTimeEx)(const DYNAMIC_TIME_ZONE_INFORMATION *lpTimeZoneInformation, SYSTEMTIME const *lpLocalTime, LPSYSTEMTIME lpUniversalTime);
		BOOL (WINAPI *m_fGetTimeZoneInformationForYear)(USHORT wYear, PDYNAMIC_TIME_ZONE_INFORMATION pdtzi, LPTIME_ZONE_INFORMATION ptzi);
	};

	extern OSVERSIONINFOEX g_VersionInfo;
	extern COptionalFunctions g_OptionalFunctions;

	inline_always static uint32 fg_TlsIndexToTebOffset(uint32 _Index)
	{
#if defined(DArchitecture_arm64)
		return _Index * 8 + 0x1480;
#elif defined(DArchitecture_x64)
		return _Index * 8 + 0x1480;
#else
		return _Index * 4 + 0xe10;
#endif
	}

	inline_always static uint32 fg_TebOffsetToTlsIndex(uint32 _Offset)
	{
#if defined(DArchitecture_arm64)
		return (_Offset - 0x1480) / 8;
#elif defined(DArchitecture_x64)
		return (_Offset - 0x1480) / 8;
#else
		return (_Offset - 0xe10) / 4;
#endif
	}
}
