// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Windows.h>
#include <winternl.h>

#include "Malterlib_Core_Platform_Windows_Undocumented.h"

namespace NLocal
{
	extern HMODULE g_pNtDll;
	extern HMODULE g_pKernel32;
	extern HMODULE g_pAdvAPI32;

	struct COptionalFunctions
	{
		NTSTATUS (NTAPI *m_fNtQueryInformationThread)
			(
				IN HANDLE _pThreadHandle
				, IN THREADINFOCLASS _ThreadInformationClass
				, OUT PVOID _pThreadInformation
				, IN ULONG _ThreadInformationLength
				, OUT PULONG _pReturnLength OPTIONAL
			)
		;

		BOOL (WINAPI *m_fGetLogicalProcessorInformation)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION _pBuffer, PDWORD _pReturnLength);
		VOID (WINAPI *m_fRtlAcquirePebLock)(void);
		VOID (WINAPI *m_fRtlReleasePebLock)(void);
		ULONG  (WINAPI *m_fRtlFindClearBitsAndSet)(IN PRTL_BITMAP _pBitMapHeader, IN ULONG _NumberToFind, IN ULONG _HintIndex);
		VOID (WINAPI *m_fRtlClearBits)(IN PRTL_BITMAP _pBitMapHeader, IN ULONG _StartingIndex, IN ULONG _NumberToClear);

		void (WINAPI *m_fGetNativeSystemInfo)(__out LPSYSTEM_INFO _pSystemInfo);
#if !defined(DMibSanitizerEnabled_Address) && !defined(DArchitecture_arm64)
		PVOID (WINAPI *m_fAddVectoredExceptionHandler)(ULONG _First, PVECTORED_EXCEPTION_HANDLER _fHandler);
		ULONG (WINAPI *m_fRemoveVectoredExceptionHandler)(PVOID _pHandler);
#endif

		SIZE_T (WINAPI *m_fLargePageMinimum)();

		char *(WINAPI *m_fWineGetVersion)();

		void *m_pKiUserApcDispatcher;
		void *m_pKiUserCallbackDispatcher;

		// Numa functions
		LPVOID (WINAPI *m_fVirtualAllocExNuma)(HANDLE _pProcess, LPVOID _pAddress, SIZE_T _Size, DWORD _AllocationType, DWORD _Protect, DWORD _PreferredNode);
		BOOL (WINAPI *m_fGetNumaNodeProcessorMaskEx)(USHORT _Node, PGROUP_AFFINITY o_pProcessorMask);
		BOOL (WINAPI *m_fSetThreadGroupAffinity)(HANDLE _pThread, GROUP_AFFINITY const *_pGroupAffinity, PGROUP_AFFINITY o_pPreviousGroupAffinity);

		BOOL (WINAPI *m_fSetProcessUserModeExceptionPolicy)(DWORD _Flags);
		BOOL (WINAPI *m_fGetProcessUserModeExceptionPolicy)(LPDWORD o_pFlags);

		DWORD (WINAPI *m_fWTSGetActiveConsoleSessionId)();

		BOOL (WINAPI *m_fCreateProcessWithTokenW)
			(
				HANDLE _pToken
				, DWORD _LogonFlags
				, LPCWSTR _pApplicationName
				, LPWSTR _pCommandLine
				, DWORD _CreationFlags
				, LPVOID _pEnvironment
				, LPCWSTR _pCurrentDirectory
				, LPSTARTUPINFOW _pStartupInfo
				, LPPROCESS_INFORMATION o_pProcessInformation
			)
		;

		BOOLEAN (APIENTRY *m_fCreateSymbolicLinkW)(LPCWSTR _pSymlinkFileName, LPCWSTR _pTargetFileName, DWORD _Flags);

		BOOL (WINAPI *m_fCreateHardLinkW)(LPCWSTR _pFileName, LPCWSTR _pExistingFileName, LPSECURITY_ATTRIBUTES _pSecurityAttributes);

		BOOL (WINAPI *m_fWow64DisableWow64FsRedirection)(PVOID *o_pOldValue);
		BOOL (WINAPI *m_fWow64RevertWow64FsRedirection)(PVOID _pOldValue);

		NTSTATUS (NTAPI *m_fNtSetInformationProcess)(HANDLE _pProcess, PROCESSINFOCLASS _ProcessInformationClass, PVOID _pProcessInformation, ULONG _ProcessInformationLength);

		BOOL (WINAPI *m_fSetProcessInformation)(HANDLE _pProcess, PROCESS_INFORMATION_CLASS _ProcessInformationClass, LPVOID _pProcessInformation, DWORD _ProcessInformationSize);

		BOOL (WINAPI *m_fCancelSynchronousIo)(HANDLE _pThread);
		BOOL (WINAPI *m_fCancelIoEx)(HANDLE _pFile, LPOVERLAPPED _pOverlapped);

		NTSTATUS (NTAPI *m_fNtCreateFile)
			(
				PHANDLE _pFile
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
				HANDLE _pFile
				, HANDLE _pEvent
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
		// NOLINTNEXTLINE(malterlib-conceptual-prefix): FILE_INFORMATION_CLASS is an enumeration, which the check takes for a function type by its name
		NTSTATUS (NTAPI *m_fNtSetInformationFile)(HANDLE _pFile, PIO_STATUS_BLOCK _pIoStatusBlock, PVOID _pFileInformation, ULONG _Length, FILE_INFORMATION_CLASS _FileInformationClass);
		ULONG (NTAPI *m_fRtlNtStatusToDosError)(NTSTATUS _Status);

		// Optional Windows 8+ API for delivering synchronization-object signals to an IOCP.
		NTSTATUS (NTAPI *m_fNtCreateWaitCompletionPacket)(PHANDLE _pPacket, ACCESS_MASK _DesiredAccess, POBJECT_ATTRIBUTES _pObjectAttributes);
		NTSTATUS (NTAPI *m_fNtAssociateWaitCompletionPacket)
			(
				HANDLE _pPacket
				, HANDLE _pPort
				, HANDLE _pTarget
				, PVOID _pKeyContext
				, PVOID _pApcContext
				, NTSTATUS _IoStatus
				, ULONG_PTR _IoStatusInformation
				, PBOOLEAN _pAlreadySignaled
			)
		;
		NTSTATUS (NTAPI *m_fNtCancelWaitCompletionPacket)(HANDLE _pPacket, BOOLEAN _bRemoveSignaledPacket);

		NTSTATUS (WINAPI *m_fNtQuerySystemInformation)(DWORD _SystemInformationClass, PVOID o_pSystemInformation, DWORD _SystemInformationLength, PDWORD o_pReturnLength);

		NTSTATUS (WINAPI *m_fNtGetNextThread)(HANDLE _pProcess, HANDLE _pThread, ACCESS_MASK _DesiredAccess, ULONG _HandleAttributes, ULONG _Flags, PHANDLE o_pNewThread);

		DWORD (WINAPI *m_fGetThreadId)(HANDLE _pThread);

		HRESULT (WINAPI *m_fSetThreadDescription)(HANDLE _pThread, PCWSTR _pThreadDescription);

		NTSTATUS (WINAPI *m_fNtQueryInformationProcess)
			(
				HANDLE _pProcess
				, PROCESSINFOCLASS _ProcessInformationClass
				, PVOID o_pProcessInformation
				, ULONG _ProcessInformationLength
				, PULONG o_pReturnLength
			)
		;

		NTSTATUS (WINAPI *m_fLdrDisableThreadCalloutsForDll)(IN PVOID _pBaseAddress);
		NTSTATUS (WINAPI *m_fRtlGetVersion)(PRTL_OSVERSIONINFOW o_pVersionInformation);
		BOOL (WINAPI *m_fPrivIsDllSynchronizationHeld)(PBOOL _pSynchronizationHeld);

		BOOL (WINAPI *m_fGetFileInformationByHandleEx)(HANDLE _pFile, Undocumented_FILE_INFO_BY_HANDLE_CLASS _FileInformationClass, LPVOID o_pFileInformation, DWORD _BufferSize);

		BOOL (WINAPI *m_fSystemTimeToTzSpecificLocalTimeEx)(DYNAMIC_TIME_ZONE_INFORMATION const *_pTimeZoneInformation, SYSTEMTIME const *_pUniversalTime, LPSYSTEMTIME o_pLocalTime);
		BOOL (WINAPI *m_fTzSpecificLocalTimeToSystemTimeEx)(DYNAMIC_TIME_ZONE_INFORMATION const *_pTimeZoneInformation, SYSTEMTIME const *_pLocalTime, LPSYSTEMTIME o_pUniversalTime);
		BOOL (WINAPI *m_fGetTimeZoneInformationForYear)(USHORT _Year, PDYNAMIC_TIME_ZONE_INFORMATION o_pDynamicTimeZoneInformation, LPTIME_ZONE_INFORMATION o_pTimeZoneInformation);
	};

	extern OSVERSIONINFOEX g_VersionInfo;
	extern COptionalFunctions g_OptionalFunctions;

	inline_always static uint32 fg_TlsIndexToTebOffset(uint32 _Index);
	inline_always static uint32 fg_TebOffsetToTlsIndex(uint32 _Offset);
}

#include "Malterlib_Core_Platform_Windows_Optional.hpp"
