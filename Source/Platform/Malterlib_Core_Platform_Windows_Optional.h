// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Windows.h>
#include <winternl.h>

#include "Malterlib_Core_Platform_Windows_Undocumented.h"

namespace NLocal
{
	extern HMODULE g_hNtDll;
	extern HMODULE g_hKernel32;
	extern HMODULE g_hAdvAPI32;

	extern NTSTATUS (NTAPI *g_fNtQueryInformationThread)(
		IN HANDLE _ThreadHandle,
		IN THREADINFOCLASS _ThreadInformationClass,
		OUT PVOID _ThreadInformation,
		IN ULONG _ThreadInformationLength,
		OUT PULONG _ReturnLength OPTIONAL
		);

	extern BOOL (WINAPI *g_fGetLogicalProcessorInformation)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION _pBuffer, PDWORD _pReturnLength);
	extern VOID (WINAPI *g_fRtlAcquirePebLock)(void);
	extern VOID (WINAPI *g_fRtlReleasePebLock)(void);
	extern ULONG  (WINAPI *g_fRtlFindClearBitsAndSet)(IN PRTL_BITMAP  _pBitMapHeader, IN ULONG  _NumberToFind, IN ULONG  _HintIndex);
	extern VOID (WINAPI *g_fRtlClearBits)(IN PRTL_BITMAP  _pBitMapHeader, IN ULONG  _StartingIndex, IN ULONG  _NumberToClear);

	extern void (WINAPI *g_fGetNativeSystemInfo)(__out LPSYSTEM_INFO _pSystemInfo);
	extern PVOID (WINAPI *g_fAddVectoredExceptionHandler)(ULONG _First, PVECTORED_EXCEPTION_HANDLER _pHandler);
	extern ULONG (WINAPI *g_fRemoveVectoredExceptionHandler)(PVOID _pHandler);
	
	
	extern SIZE_T (WINAPI *g_fLargePageMinimum)();

	extern char *(WINAPI *g_fWineGetVersion)();

	extern void *g_pKiUserApcDispatcher;
	extern void *g_pKiUserCallbackDispatcher;

	// Numa functions
	extern LPVOID (WINAPI *g_fVirtualAllocExNuma)(HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD  flAllocationType, DWORD  flProtect, DWORD  nndPreferred);
	extern BOOL (WINAPI *g_fGetNumaNodeProcessorMaskEx)(USHORT Node, PGROUP_AFFINITY ProcessorMask);
	extern BOOL (WINAPI *g_fSetThreadGroupAffinity)(HANDLE hThread, const GROUP_AFFINITY *GroupAffinity, PGROUP_AFFINITY PreviousGroupAffinity);

	extern BOOL (WINAPI *g_fSetProcessUserModeExceptionPolicy)(DWORD dwFlags);
	extern BOOL (WINAPI *g_fGetProcessUserModeExceptionPolicy)(LPDWORD lpFlags);

	extern DWORD (WINAPI *g_fWTSGetActiveConsoleSessionId)();

	extern BOOL (WINAPI *g_fCreateProcessWithTokenW)(HANDLE hToken, DWORD dwLogonFlags, LPCWSTR lpApplicationName, LPWSTR lpCommandLine, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation );

	extern BOOLEAN (APIENTRY *g_fCreateSymbolicLinkW)(LPCWSTR lpSymlinkFileName, LPCWSTR lpTargetFileName, DWORD dwFlags);

	extern BOOL (WINAPI *g_fCreateHardLinkW)(LPCWSTR lpFileName, LPCWSTR lpExistingFileName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
	
	extern OSVERSIONINFOEX g_VersionInfo;

	extern BOOL (WINAPI *g_fWow64DisableWow64FsRedirection)(PVOID * OldValue);
	extern BOOL (WINAPI *g_fWow64RevertWow64FsRedirection)(PVOID OlValue);

	extern NTSTATUS (NTAPI *g_fNtSetInformationProcess)(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass, PVOID pProcessInformation, ULONG ProcessInformationLength);

	extern BOOL (WINAPI *g_fSetProcessInformation)(HANDLE hProcess, PROCESS_INFORMATION_CLASS ProcessInformationClass, LPVOID ProcessInformation, DWORD ProcessInformationSize);

	extern BOOL (WINAPI *g_fCancelSynchronousIo)(HANDLE hThread);
	extern BOOL (WINAPI *g_fCancelIoEx)(HANDLE hFile, LPOVERLAPPED lpOverlapped);

	extern NTSTATUS (WINAPI *g_fNtQuerySystemInformation)(DWORD SystemInformationClass, PVOID SystemInformation, DWORD SystemInformationLength, PDWORD ReturnLength);

	extern NTSTATUS (WINAPI *g_fNtGetNextThread)(HANDLE ProcessHandle, HANDLE ThreadHandle, ACCESS_MASK DesiredAccess, ULONG HandleAttributes, ULONG Flags, PHANDLE NewThreadHandle);

	extern DWORD (WINAPI *g_fGetThreadId)(HANDLE Thread);

	extern NTSTATUS (WINAPI *g_fNtQueryInformationProcess)(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength);

	extern NTSTATUS (WINAPI *g_fLdrDisableThreadCalloutsForDll)(IN PVOID BaseAddress);

	extern BOOL (WINAPI *g_fGetFileInformationByHandleEx)(HANDLE hFile, FILE_INFO_BY_HANDLE_CLASS FileInformationClass, LPVOID lpFileInformation, DWORD dwBufferSize);

}
