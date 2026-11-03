// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Windows_Inject.h"

#include <Mib/Core/PlatformSpecific/WindowsError>
#include <Mib/Core/PlatformSpecific/WindowsOptional>
#include <Psapi.h>
#include <winternl.h>

namespace NMib
{
	namespace NPlatform
	{
#define DRvaToPtr(d_Base, d_Rva) (((PBYTE) d_Base) + d_Rva)

		//http://jpassing.wordpress.com/2008/01/06/using-import-address-table-hooking-for-testing/
		/*++
		Routine Description:
		Replace the function pointer in a module's IAT.

		Parameters:
		_pModule           - Module to use IAT from.
		_pImportedModuleName  - Name of imported DLL from which
		function is imported.
		_pImportedProcName    - Name of imported function.
		_pHookingProc       - Function to be written to IAT.
		_pOriginalProc             - Original function.

		Return Value:
		S_OK on success.
		(any HRESULT) on failure.
		--*/
		HRESULT fg_PatchIAT(HMODULE _pModule, CHAR const *_pImportedModuleName, CHAR const *_pImportedProcName, PVOID _pHookingProc, PVOID *_pOriginalProc)
		{
			PIMAGE_DOS_HEADER pDOSHeader;
			PIMAGE_NT_HEADERS pNTHeader;
			PIMAGE_IMPORT_DESCRIPTOR pImportDescriptor;
			UINT iDescriptor;

			pDOSHeader = (PIMAGE_DOS_HEADER) _pModule;
			pNTHeader = (PIMAGE_NT_HEADERS) DRvaToPtr(pDOSHeader, pDOSHeader->e_lfanew);
			if (IMAGE_NT_SIGNATURE != pNTHeader->Signature)
				return HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
			pImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR) DRvaToPtr(pDOSHeader, pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

			// Iterate over import descriptors/DLLs.
			for (iDescriptor = 0; pImportDescriptor[iDescriptor].Characteristics != 0; iDescriptor++)
			{
				if (pImportDescriptor[iDescriptor].Name == 65535)
					continue;
				PIMAGE_THUNK_DATA pFirstThunkIter;
				PIMAGE_THUNK_DATA pOriginalFirstThunkIter;
				PSTR pDLLName;
				PIMAGE_IMPORT_BY_NAME pImportByName;

				pDLLName = (PSTR) DRvaToPtr(pDOSHeader, pImportDescriptor[iDescriptor].Name);
				if (!NStr::fg_StrCmpNoCase(pDLLName, _pImportedModuleName))
				{
					if (!pImportDescriptor[iDescriptor].FirstThunk || !pImportDescriptor[iDescriptor].OriginalFirstThunk)
						return E_INVALIDARG;

					pFirstThunkIter = (PIMAGE_THUNK_DATA) DRvaToPtr(pDOSHeader, pImportDescriptor[iDescriptor].FirstThunk);
					pOriginalFirstThunkIter = (PIMAGE_THUNK_DATA) DRvaToPtr(pDOSHeader, pImportDescriptor[iDescriptor].OriginalFirstThunk);

					for (; pOriginalFirstThunkIter->u1.Function != NULL; pOriginalFirstThunkIter++, pFirstThunkIter++)
					{
						if (pOriginalFirstThunkIter->u1.Ordinal & IMAGE_ORDINAL_FLAG) // Ordinal import - we can handle named imports only, so skip it.
							continue;

						pImportByName = (PIMAGE_IMPORT_BY_NAME) DRvaToPtr(pDOSHeader, pOriginalFirstThunkIter->u1.AddressOfData);
						if (!NStr::fg_StrCmpNoCase(pImportByName->Name, _pImportedProcName))
						{
							DWORD PreviousProtect;
							MEMORY_BASIC_INFORMATION ThunkMemoryInfo;

							//DMibDTraceSafe("{} {} (0x{}) 0x{} -> 0x{}\r\n", pDLLName, (const ch8 *)pImportByName->Name, (void *)(&(pFirstThunkIter->u1.Function)),
							//	(void *)pFirstThunkIter->u1.Function, _pHookingProc);

							// Make page writable.
							VirtualQuery(pFirstThunkIter, &ThunkMemoryInfo, sizeof(MEMORY_BASIC_INFORMATION));
							if (!VirtualProtect(ThunkMemoryInfo.BaseAddress, ThunkMemoryInfo.RegionSize, PAGE_EXECUTE_READWRITE, &ThunkMemoryInfo.Protect))
								return HRESULT_FROM_WIN32(GetLastError());

							// Replace function pointers (non-atomically).
							if (_pOriginalProc)
								*_pOriginalProc = (void *)pFirstThunkIter->u1.Function;
							pFirstThunkIter->u1.Function = (umint)_pHookingProc;

							// Restore page protection.
							if (!VirtualProtect(ThunkMemoryInfo.BaseAddress, ThunkMemoryInfo.RegionSize, ThunkMemoryInfo.Protect, &PreviousProtect))
								return HRESULT_FROM_WIN32(GetLastError());

							return S_OK;
						}
					}
					return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
				}
			}
			return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
		}

		// NOLINTBEGIN(malterlib-variable-naming): the declarations of the Windows SDK, with their names
		// NOLINTBEGIN(malterlib-type-naming): the names in the Windows headers
		// NOLINTBEGIN(malterlib-conceptual-prefix): the declarations of the Windows SDK, with their names
		typedef struct _IMAGE_DELAY_IMPORT_DESCRIPTOR
		{
			DWORD grAttrs;
			DWORD szName;
			DWORD phMod;
			DWORD pIAT;
			DWORD pINT;
			DWORD PBountIAT;
			DWORD pUnloadIAT;
			DWORD dwTimeStamp;
		} IMAGE_DELAY_IMPORT_DESCRIPTOR;

		typedef IMAGE_DELAY_IMPORT_DESCRIPTOR UNALIGNED *PIMAGE_DELAY_IMPORT_DESCRIPTOR;
		// NOLINTEND(malterlib-conceptual-prefix)
		// NOLINTEND(malterlib-type-naming)
		// NOLINTEND(malterlib-variable-naming)

		HRESULT fg_DumpIATs(HMODULE _pModule)
		{
			PIMAGE_DOS_HEADER pDOSHeader;
			PIMAGE_NT_HEADERS pNTHeader;
			PIMAGE_IMPORT_DESCRIPTOR pImportDescriptor;
			UINT iDescriptor;
			PIMAGE_DELAY_IMPORT_DESCRIPTOR pDelayImportDescriptor;

			pDOSHeader = (PIMAGE_DOS_HEADER) _pModule;
			pNTHeader = (PIMAGE_NT_HEADERS) DRvaToPtr(pDOSHeader, pDOSHeader->e_lfanew);
			if (IMAGE_NT_SIGNATURE != pNTHeader->Signature)
				return HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
			pImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR) DRvaToPtr(pDOSHeader, pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

			DMibDTraceSafe("Import Table\r\n", 0);

			// Iterate over import descriptors/DLLs.
			for (iDescriptor = 0; pImportDescriptor[iDescriptor].Characteristics != 0; iDescriptor++)
			{
				PIMAGE_THUNK_DATA pFirstThunkIter;
				PIMAGE_THUNK_DATA pOriginalFirstThunkIter;
				[[maybe_unused]] PSTR pDLLName;
				[[maybe_unused]] PIMAGE_IMPORT_BY_NAME pImportByName;

				pDLLName = (PSTR) DRvaToPtr(pDOSHeader, pImportDescriptor[iDescriptor].Name);
				DMibDTraceSafe("{}\r\n", pDLLName);
				if (!pImportDescriptor[iDescriptor].FirstThunk || !pImportDescriptor[iDescriptor].OriginalFirstThunk)
					return E_INVALIDARG;

				pFirstThunkIter = (PIMAGE_THUNK_DATA) DRvaToPtr(pDOSHeader, pImportDescriptor[iDescriptor].FirstThunk);
				pOriginalFirstThunkIter = (PIMAGE_THUNK_DATA) DRvaToPtr(pDOSHeader, pImportDescriptor[iDescriptor].OriginalFirstThunk);

				for (; pOriginalFirstThunkIter->u1.Function != NULL; pOriginalFirstThunkIter++, pFirstThunkIter++)
				{
					if (pOriginalFirstThunkIter->u1.Ordinal & IMAGE_ORDINAL_FLAG) // Ordinal import - we can handle named imports only, so skip it.
						continue;

					pImportByName = (PIMAGE_IMPORT_BY_NAME) DRvaToPtr(pDOSHeader, pOriginalFirstThunkIter->u1.AddressOfData);
					DMibDTraceSafe("{} {} (0x{}) 0x{}\r\n", pDLLName, (ch8 const *)pImportByName->Name, (void *)(&(pFirstThunkIter->u1.Function)), (void *)pFirstThunkIter->u1.Function);
				}
			}

			DMibDTraceSafe("Delay-Load Import Table\r\n", 0);

			pDelayImportDescriptor = (struct _IMAGE_DELAY_IMPORT_DESCRIPTOR *)DRvaToPtr
				(
					pDOSHeader
					, pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT].VirtualAddress
				)
			;
			// Iterate over import descriptors/DLLs.
			for (iDescriptor = 0; pDelayImportDescriptor[iDescriptor].grAttrs != 0; iDescriptor++)
			{
				PIMAGE_THUNK_DATA pFirstThunkIter;
				PIMAGE_THUNK_DATA pOriginalFirstThunkIter;
				[[maybe_unused]] PSTR pDLLName;
				[[maybe_unused]] PIMAGE_IMPORT_BY_NAME pImportByName;

				pDLLName = (PSTR) DRvaToPtr(pDOSHeader, pDelayImportDescriptor[iDescriptor].szName);
				DMibDTraceSafe("{}\r\n", pDLLName);
				if (!pDelayImportDescriptor[iDescriptor].pIAT || !pDelayImportDescriptor[iDescriptor].pINT)
					return E_INVALIDARG;

				pFirstThunkIter = (PIMAGE_THUNK_DATA) DRvaToPtr(pDOSHeader, pDelayImportDescriptor[iDescriptor].pIAT);
				pOriginalFirstThunkIter = (PIMAGE_THUNK_DATA) DRvaToPtr(pDOSHeader, pDelayImportDescriptor[iDescriptor].pINT);

				for (; pOriginalFirstThunkIter->u1.Function != NULL; pOriginalFirstThunkIter++, pFirstThunkIter++)
				{
					if (pOriginalFirstThunkIter->u1.Ordinal & IMAGE_ORDINAL_FLAG) // Ordinal import - we can handle named imports only, so skip it.
						continue;

					pImportByName = (PIMAGE_IMPORT_BY_NAME) DRvaToPtr(pDOSHeader, pOriginalFirstThunkIter->u1.AddressOfData);
					DMibDTraceSafe("{} {} (0x{}) 0x{}", pDLLName, (ch8 const *)pImportByName->Name, (void *)(&(pFirstThunkIter->u1.Function)), (void *)pFirstThunkIter->u1.Function);
				}
			}

			return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
		}

		HRESULT fg_PatchDIAT(HMODULE _pModule, CHAR const *_pImportedModuleName, CHAR const *_pImportedProcName, PVOID _pHookingProc, PVOID *_pOriginalProc)
		{
			PIMAGE_DOS_HEADER pDOSHeader;
			PIMAGE_NT_HEADERS pNTHeader;
			PIMAGE_DELAY_IMPORT_DESCRIPTOR pDelayImportDescriptor;
			UINT iDescriptor;

			pDOSHeader = (PIMAGE_DOS_HEADER) _pModule;
			pNTHeader = (PIMAGE_NT_HEADERS) DRvaToPtr(pDOSHeader, pDOSHeader->e_lfanew);
			if (IMAGE_NT_SIGNATURE != pNTHeader->Signature)
				return HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
			pDelayImportDescriptor = (struct _IMAGE_DELAY_IMPORT_DESCRIPTOR *)DRvaToPtr
				(
					pDOSHeader
					, pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT].VirtualAddress
				)
			;

			// Iterate over import descriptors/DLLs.
			for (iDescriptor = 0; pDelayImportDescriptor[iDescriptor].grAttrs != 0; iDescriptor++)
			{
				PIMAGE_THUNK_DATA pFirstThunkIter;
				PIMAGE_THUNK_DATA pOriginalFirstThunkIter;
				PSTR pDLLName;
				PIMAGE_IMPORT_BY_NAME pImportByName;

				pDLLName = (PSTR) DRvaToPtr(pDOSHeader, pDelayImportDescriptor[iDescriptor].szName);
				if (!NStr::fg_StrCmpNoCase(pDLLName, _pImportedModuleName))
				{
					if (!pDelayImportDescriptor[iDescriptor].pIAT || !pDelayImportDescriptor[iDescriptor].pINT)
						return E_INVALIDARG;

					pFirstThunkIter = (PIMAGE_THUNK_DATA) DRvaToPtr(pDOSHeader, pDelayImportDescriptor[iDescriptor].pIAT);
					pOriginalFirstThunkIter = (PIMAGE_THUNK_DATA) DRvaToPtr(pDOSHeader, pDelayImportDescriptor[iDescriptor].pINT);

					for (; pOriginalFirstThunkIter->u1.Function != NULL; pOriginalFirstThunkIter++, pFirstThunkIter++)
					{
						if (pOriginalFirstThunkIter->u1.Ordinal & IMAGE_ORDINAL_FLAG) // Ordinal import - we can handle named imports only, so skip it.
							continue;

						pImportByName = (PIMAGE_IMPORT_BY_NAME) DRvaToPtr(pDOSHeader, pOriginalFirstThunkIter->u1.AddressOfData);
						if (!NStr::fg_StrCmpNoCase(pImportByName->Name, _pImportedProcName))
						{
							DWORD PreviousProtect;
							MEMORY_BASIC_INFORMATION ThunkMemoryInfo;

							//DMibDTraceSafe("{} {} (0x{}) 0x{} -> 0x{}\r\n", pDLLName, (const ch8 *)pImportByName->Name, (void *)(&(pFirstThunkIter->u1.Function)),
							//	(void *)pFirstThunkIter->u1.Function, _pHookingProc);

							// Make page writable.
							VirtualQuery(pFirstThunkIter, &ThunkMemoryInfo, sizeof(MEMORY_BASIC_INFORMATION));
							if (!VirtualProtect(ThunkMemoryInfo.BaseAddress, ThunkMemoryInfo.RegionSize, PAGE_EXECUTE_READWRITE, &ThunkMemoryInfo.Protect))
								return HRESULT_FROM_WIN32(GetLastError());

							// Replace function pointers (non-atomically).
							if (_pOriginalProc)
								*_pOriginalProc = (void *)pFirstThunkIter->u1.Function;
							pFirstThunkIter->u1.Function = (umint)_pHookingProc;

							// Restore page protection.
							if (!VirtualProtect(ThunkMemoryInfo.BaseAddress, ThunkMemoryInfo.RegionSize, ThunkMemoryInfo.Protect, &PreviousProtect))
								return HRESULT_FROM_WIN32(GetLastError());

							return S_OK;
						}
					}
					return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
				}
			}
			return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
		}

#if !defined(DArchitecture_arm64)
		namespace
		{
#	define DMaxModules 1024

			// Returns the HMODULE for module _pModuleName in the process with process-id _PID
			// Returns NULL when the mudole is not loaded
			HMODULE fg_GetModuleForProcess(DWORD _PID, WCHAR const *_pModuleName)
			{
				HANDLE pProcess;
				HMODULE Modules[DMaxModules];
				DWORD nModuleBytes;
				unsigned int iModule;
				TCHAR ModuleName[MAX_PATH];

				pProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, _PID);
				if (NULL == pProcess)
					return NULL;

				if (EnumProcessModules(pProcess, Modules, sizeof(Modules), &nModuleBytes))
				{
					for (iModule = 0; iModule < nModuleBytes/sizeof(HMODULE); iModule++)
					{
						if (GetModuleBaseName(pProcess, Modules[iModule], ModuleName, sizeof(ModuleName) / sizeof(TCHAR)))
						{
							if (NStr::fg_StrCmpNoCase(ModuleName, _pModuleName) == 0)
							{
								CloseHandle(pProcess);
								return Modules[iModule];
							}
						}
					}
				}

				CloseHandle(pProcess);

				return NULL;
			}
		}

		// Remove DLL _pDLLName from process with process-id _PID
		// Don't use a full pathname for _pDLLName
		// Returns true on succes, false on failure
		BOOL fg_DejectDLL(DWORD _PID, WCHAR const *_pDLLName, NStr::CStr &_Error)
		{
			HMODULE pKernel32;
			FARPROC fFreeLibrary;
			HANDLE pProcess;
			HMODULE pDllModule;
			HANDLE pRemoteThread;
			BOOL bRet;

			pKernel32 = GetModuleHandle(str_utf16("Kernel32"));
			if (NULL == pKernel32)
			{
				HRESULT Error = GetLastError();
				_Error = NStr::CStr::CFormat("Get kernel32 module handle failed: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error);
				return FALSE;
			}

			fFreeLibrary = GetProcAddress(pKernel32, "FreeLibrary");
			if (NULL == fFreeLibrary)
			{
				HRESULT Error = GetLastError();
				_Error = NStr::CStr::CFormat("Extracting FreeLibrary failed: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error);
				return FALSE;
			}

			pProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, _PID);
			if (NULL == pProcess)
			{
				HRESULT Error = GetLastError();
				_Error = NStr::CStr::CFormat("Open process failed: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error);
				return FALSE;
			}

			pDllModule = fg_GetModuleForProcess(_PID, _pDLLName);
			if (NULL == pDllModule)
			{
				_Error = NStr::CStr::CFormat("Could not find module in process");
				CloseHandle(pProcess);
				return FALSE;
			}

			DWORD ExitCode = 1;
			bRet = true;

			while (ExitCode && bRet)
			{
				pRemoteThread = CreateRemoteThread(pProcess, NULL, 0, (LPTHREAD_START_ROUTINE)fFreeLibrary, pDllModule, 0, NULL);

				if (0 != pRemoteThread)
					bRet = WaitForSingleObject(pRemoteThread, 1000) != WAIT_TIMEOUT;
				else
				{
					HRESULT Error = GetLastError();
					_Error = NStr::CStr::CFormat("Failed to create remote thread (Dll Unload): {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error);
					bRet = FALSE;
				}

				GetExitCodeThread(pRemoteThread, &ExitCode);
				CloseHandle(pRemoteThread);
			}
			CloseHandle(pProcess);

			return bRet;
		}

		inline_never DWORD WINAPI fg_DummyThreadProc(LPVOID _pParameter)
		{
			return 0;
		}

		uint8 *fg_SkipJumps(uint8 *_pCode)
		{
#	if defined(DArchitecture_x86) || defined(DArchitecture_x64)

#		if defined DArchitecture_x64
			if ((_pCode[0] & 0xF0) == 0x40 && _pCode[1] == 0xff && _pCode[2] == 0x25)
			{
				// REX PREFIX, just skip it
				++_pCode;
			}
#		endif

			if (_pCode[0] == 0xff && _pCode[1] == 0x25)
			{
#		ifdef DArchitecture_x86
				// on x86 we have an absolute pointer...
				uint8 *pTarget = *(uint8 **)&_pCode[2];
				// ... that shows us an absolute pointer.
				return fg_SkipJumps(*(uint8 **)pTarget);
#		elif defined DArchitecture_x64
				// on x64 we have a 32-bit offset...
				INT32 Offset = *(INT32 *)&_pCode[2];
				// ... that shows us an absolute pointer
				return fg_SkipJumps(*(uint8 **)(_pCode + 6 + Offset));
#		endif
			}
			else if (_pCode[0] == 0xe9)
			{
				// here the behavior is identical, we have...
				// ...a 32-bit offset to the destination.
				return fg_SkipJumps(_pCode + 5 + *(INT32 *)&_pCode[1]);
			}
			else if (_pCode[0] == 0xeb)
			{
				// and finally an 8-bit offset to the destination
				return fg_SkipJumps(_pCode + 2 + *(CHAR *)&_pCode[1]);
			}
#	else
#		error unsupported platform
#	endif
			return _pCode;
		}

		struct CInjectDllData
		{
			NTSTATUS (__stdcall *m_fLdrLoadDll)(PWCHAR _pSearchPath, PULONG _pDllCharacteristics, PUNICODE_STRING _pModuleFileName, PHANDLE o_pModule);
			PWCHAR m_pSearchPath;
			ULONG m_DllCharacteristics;
			UNICODE_STRING m_ModuleFileName;
			HANDLE m_pModuleHandle;
		};

		// Runtime checks needs to be off here, as we need this code to be location independant
#	pragma runtime_checks( "", off )

		inline_never DWORD WINAPI fg_InjectDllRemote(CInjectDllData *_pData)
		{
			return _pData->m_fLdrLoadDll(_pData->m_pSearchPath, &_pData->m_DllCharacteristics, &_pData->m_ModuleFileName, &_pData->m_pModuleHandle);
		}

#	pragma runtime_checks( "", restore )

#	ifdef DMibSupportCygwin
		struct CWindowsHandle
		{
			CWindowsHandle()
				: m_Handle(nullptr)
			{
			}

			CWindowsHandle(CWindowsHandle &&_Other)
				: m_Handle(_Other.m_Handle)
			{
				_Other.m_Handle = nullptr;
			}

			CWindowsHandle(HANDLE _Handle)
				: m_Handle(_Handle)
			{
			}

			~CWindowsHandle()
			{
				if (m_Handle && m_Handle != INVALID_HANDLE_VALUE)
					CloseHandle(m_Handle);
			}

			CWindowsHandle &operator = (HANDLE _Handle)
			{
				if (m_Handle && m_Handle != INVALID_HANDLE_VALUE)
					CloseHandle(m_Handle);

				m_Handle = _Handle;

				return *this;
			}

			operator HANDLE ()
			{
				return m_Handle;
			}

		private:
			HANDLE m_Handle;
			CWindowsHandle(CWindowsHandle const &_Other);
			CWindowsHandle &operator = (CWindowsHandle const &_Other);
		};

		struct CFileMap
		{
			NFile::CFile m_File;
			CWindowsHandle m_FileMapping;
			void *m_pMemory;

			CFileMap(NStr::CWStr &_FileName)
				: m_pMemory(nullptr)
			{
				m_File.f_Open(NStr::CStr(_FileName), EFileOpen_Read | EFileOpen_RawFileName);
				m_FileMapping = CreateFileMappingW(m_File.f_GetOSFile(), nullptr, PAGE_READONLY, 0, 0, nullptr);

				if (!m_FileMapping)
					DMibErrorFile(NStr::CStr::CFormat("Windows returned an error from CreateFileMappingW: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr());

				m_pMemory = MapViewOfFile(m_FileMapping, FILE_MAP_READ, 0, 0, 0);
				//PAGE_READONLY
				//MapViewOfFile();
				//;
			}

			~CFileMap()
			{
				if (m_pMemory)
					UnmapViewOfFile(m_pMemory);
			}

			operator void * ()
			{
				return m_pMemory;
			}
		};

		template <typename tf_CHeaders>
		bool fg_IsCygwin(uint8 *_pMemory, tf_CHeaders *_pHeaders)
		{
			uint32 ImportVirtualAddress = _pHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
			uint32 ImportSize = _pHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;

			if (!ImportVirtualAddress)
				return false;

			PIMAGE_SECTION_HEADER pSectionHeader = (PIMAGE_SECTION_HEADER)(_pHeaders + 1);

			uint32 ImportMaxSize;
			PIMAGE_IMPORT_DESCRIPTOR pFirstImportDesc = nullptr;
			int32 ImportDelta;

			for (int i = 0; i < _pHeaders->FileHeader.NumberOfSections; i++)
			{
				auto &Section = pSectionHeader[i];
				if (ImportVirtualAddress >= Section.VirtualAddress && ImportVirtualAddress < (Section.VirtualAddress + Section.Misc.VirtualSize))
				{
					ImportMaxSize = Section.SizeOfRawData - (ImportVirtualAddress - Section.VirtualAddress);
					pFirstImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)((uint8 *)_pMemory + Section.PointerToRawData + (ImportVirtualAddress - Section.VirtualAddress));
					ImportDelta = Section.VirtualAddress - Section.PointerToRawData;
					break;
				}
			}

			if (!pFirstImportDesc)
				return false;

			for (auto pImportDesc = pFirstImportDesc; pImportDesc->FirstThunk; ++pImportDesc)
			{
				ch8 *pName = (ch8 *)(_pMemory + (pImportDesc->Name - ImportDelta));
				if (fg_StrCmpNoCase(pName, "cygwin1.dll") == 0)
					return true;
			}
			return false;
		}

		bool fg_IsCygwin(uint8 *_pMemory)
		{
			PIMAGE_NT_HEADERS pNtHeaders = fg_GetImageHeaders((HMODULE)_pMemory);

			if (!pNtHeaders)
				return false;

			if (pNtHeaders->FileHeader.Machine == IMAGE_FILE_MACHINE_I386)
				return fg_IsCygwin(_pMemory, (IMAGE_NT_HEADERS32 *)pNtHeaders);
			else if (pNtHeaders->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64)
				return fg_IsCygwin(_pMemory, (IMAGE_NT_HEADERS64 *)pNtHeaders);

			return false;
		}

#	endif

		extern "C" ULONG NTAPI LsaNtStatusToWinError(_In_ NTSTATUS Status);

#	pragma comment(lib, "Advapi32.lib")
		// Inject DLL _pDLLName in process with process-id _PID
		// If the DLL is not in the PATH of the process, use a full pathname for _pDLLName
		// Returns true on succes, false on failure

//#	define DMibSupportCygwin

		PIMAGE_NT_HEADERS fg_GetImageHeaders(HMODULE _pModule)
		{
			PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)_pModule;

			if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE)
				return nullptr;

			PIMAGE_NT_HEADERS pRet = (PIMAGE_NT_HEADERS)((uint8 *)_pModule + pDosHeader->e_lfanew);
			// Our buffer is 4096 bytes max
			if (((uint8 *)(pRet + 1) - (uint8 *)_pModule) > 4096)
				return nullptr;

			if (pRet->Signature != IMAGE_NT_SIGNATURE)
				return nullptr;

			return pRet;
		}

		EInjectDllResult fg_InjectDLL(HANDLE _pProcess, HANDLE _pThread, WCHAR const *_pDLLName, NStr::CStr &_Error)
		{
			// NMib::NTime::CTimeMeasureTraceScope Scope("fg_InjectDLL");
			// DMibTrace("Injecting Dll: {}\r\n", _pDLLName);
			BOOL bRet = true;
			SIZE_T nBytes;

			// LoadLibraryW(_pDLLName);

			HANDLE pProcess = _pProcess;

			auto fReportError = [&](NStr::CStr const &_NewError)
				{
					if (!_Error.f_IsEmpty())
						_Error += "\r\n";
					_Error += _NewError;
				}
			;

			HMODULE pThisNtDll = NLocal::g_pNtDll;
			if (!pThisNtDll)
			{
				HRESULT Error = GetLastError();
				fReportError(NStr::CStr::CFormat("Get kernel32 module handle failed: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
				return EInjectDllResult_Failed;
			}

			FARPROC fLdrLoadDll = GetProcAddress(pThisNtDll, "LdrLoadDll");
			if (!fLdrLoadDll)
			{
				HRESULT Error = GetLastError();
				fReportError(NStr::CStr::CFormat("Extracting LdrLoadDll failed: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
				return EInjectDllResult_Failed;
			}

			auto pThisNtDllHeaders = fg_GetImageHeaders(pThisNtDll);

			if (!pThisNtDllHeaders)
			{
				HRESULT Error = GetLastError();
				fReportError(NStr::CStr::CFormat("Could not find NT headers in local ntdll.dll: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
				return EInjectDllResult_Failed;
			}

#	ifdef DMibSupportCygwin
			NStr::CWStr ImageFileName;

			{
				UndocumentedPEB Peb;
				PROCESS_BASIC_INFORMATION BasicInfo;
				ULONG RetLen = 0;
				if
				(
					NLocal::g_OptionalFunctions.m_fNtQueryInformationProcess
					&& !NLocal::g_OptionalFunctions.m_fNtQueryInformationProcess(pProcess, ProcessBasicInformation, &BasicInfo, sizeof(BasicInfo), &RetLen)
					&& RetLen == sizeof(BasicInfo)
				)
				{
					SIZE_T ReadBytes = 0;
					if (ReadProcessMemory(pProcess, BasicInfo.PebBaseAddress, &Peb, sizeof(Peb), &ReadBytes) && ReadBytes == sizeof(Peb))
					{
						auto pData = ImageFileName.f_GetStr(NMib::NFile::NPlatform::gc_MaxWindowsPath);
						pData[0] = 0;
						GetMappedFileNameW(pProcess, (HMODULE)Peb.ImageBaseAddress, pData, NMib::NFile::NPlatform::gc_MaxWindowsPath);
						ImageFileName.f_GetLen();
					}
				}
			}
#	endif

			HMODULE pRemoteNtDll = 0;
			{
				umint StartAddress = TCLimitsInt<umint>::mc_Min;
				umint CurrentAddress = StartAddress;
				while (true)
				{
					MEMORY_BASIC_INFORMATION MemInfo;
					if (!VirtualQueryEx(_pProcess, (void *)CurrentAddress, &MemInfo, sizeof(MemInfo)))
						break;
					if (MemInfo.BaseAddress == MemInfo.AllocationBase && (MemInfo.Type & MEM_IMAGE))
					{
						//NStr::CWStr BaseName;
						//GetMappedFileNameW(pProcess, (HMODULE)MemInfo.AllocationBase, BaseName.f_GetStr(1024), 1024);
						//BaseName.f_GetLen();
						//DTrace("0x{nfh,sj16,sf0} -> 0x{nfh,sj16,sf0}: State: 0x{nfh,sj8,sf0}: {}\n", MemInfo.BaseAddress, ((umint)MemInfo.BaseAddress + MemInfo.RegionSize), MemInfo.State,
						//	BaseName);

						if (!pRemoteNtDll)
						{
							uint8 Temp[4096];
							if (ReadProcessMemory(pProcess, MemInfo.BaseAddress, Temp, 4096, &nBytes) && nBytes == 4096)
							{
								auto pRemoteHeaders = fg_GetImageHeaders((HMODULE)Temp);

								if (pRemoteHeaders && NMemory::fg_MemCmp((uint8 *)pRemoteHeaders, (uint8 *)pThisNtDllHeaders, sizeof(*pThisNtDllHeaders)) == 0)
								{
									// If the headers are the same, it means that the checksum is the same
									pRemoteNtDll = (HMODULE)MemInfo.AllocationBase;
									break;
								}
							}
						}
					}
					if (CurrentAddress == (umint)MemInfo.BaseAddress + MemInfo.RegionSize)
						break;
					else
						CurrentAddress = (umint)MemInfo.BaseAddress + MemInfo.RegionSize;
				}
			}

			if (!pRemoteNtDll)
			{
				fReportError("Failed to find ntdll.dll in remote process.\r\n");
				return EInjectDllResult_Failed;
			}

			NStr::CWStr DllDirectories = NFile::CFile::fs_GetPath(NStr::CWStr(_pDLLName)).f_ReplaceChar('/', '\\');

			NStr::CWStr Temp;
			auto Ret = GetSystemDirectoryW(Temp.f_GetStr(2048), 2048);
			if (Ret && Ret <= 2047)
			{
				Temp.f_GetLen();
				DllDirectories += ";";
				DllDirectories += Temp;
			}
			Ret = GetWindowsDirectoryW(Temp.f_GetStr(2048), 2048);
			if (Ret && Ret <= 2047)
			{
				Temp.f_GetLen();
				DllDirectories += ";";
				DllDirectories += Temp;
			}
			DllDirectories += ";.";

			size_t StringMemorySize = (NStr::fg_StrLen(_pDLLName) + 1) * 2;
			size_t RemoteMemorySize = StringMemorySize;
			size_t DllDirectoriesSize = (DllDirectories.f_GetLen() + 1) * 2;
			RemoteMemorySize += DllDirectoriesSize;

			RemoteMemorySize += 256 + 16 * 4;
			RemoteMemorySize += sizeof(CInjectDllData);

			// Get memory at an address that is free in this process so eventual fork wont have problems
			uint8 *pLocalMemory = (uint8 *)VirtualAllocEx(GetCurrentProcess(), NULL, RemoteMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
			if (!pLocalMemory)
			{
				HRESULT Error = GetLastError();
				fReportError(NStr::CStr::CFormat("Failed to allocate local memory: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
				return EInjectDllResult_Failed;
			}
			VirtualFreeEx(GetCurrentProcess(), pLocalMemory, RemoteMemorySize, MEM_RELEASE);

			uint8 *pRemoteMemory = (uint8 *)VirtualAllocEx(pProcess, pLocalMemory, RemoteMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
			if (!pRemoteMemory)
				pRemoteMemory = (uint8 *)VirtualAllocEx(pProcess, nullptr, RemoteMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE); // Just alloc anywhere

			if (!pRemoteMemory)
			{
				HRESULT Error = GetLastError();
				fReportError(NStr::CStr::CFormat("Failed to allocate remote memory: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
				return EInjectDllResult_Failed;
			}

			auto CleanupMemory = fg_OnScopeExit
				(
					[&]
					{
						VirtualFreeEx(pProcess, pRemoteMemory, RemoteMemorySize, MEM_RELEASE);
					}
				)
			;

			// Write String
			uint8 *pRemoteMemoryOut = pRemoteMemory;
			if (!WriteProcessMemory(pProcess, pRemoteMemoryOut, _pDLLName, StringMemorySize, &nBytes))
			{
				HRESULT Error = GetLastError();
				fReportError(NStr::CStr::CFormat("Failed to write to remote memory: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
				return EInjectDllResult_Failed;
			}

			CInjectDllData InjectData;
			InjectData.m_ModuleFileName.Buffer = (ch16 *)pRemoteMemoryOut;
			InjectData.m_ModuleFileName.Length = (uint16)NStr::fg_StrLen(_pDLLName) * sizeof(ch16);
			InjectData.m_ModuleFileName.MaximumLength = (uint16)nBytes;

			pRemoteMemoryOut += nBytes;
			pRemoteMemoryOut = fg_AlignUp(pRemoteMemoryOut, 16);

			if (!WriteProcessMemory(pProcess, pRemoteMemoryOut, DllDirectories.f_GetStr(), DllDirectoriesSize, &nBytes))
			{
				HRESULT Error = GetLastError();
				fReportError(NStr::CStr::CFormat("Failed to write to remote memory: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
				return EInjectDllResult_Failed;
			}

			InjectData.m_pSearchPath = (PWCHAR)pRemoteMemoryOut;

			pRemoteMemoryOut += nBytes;
			pRemoteMemoryOut = fg_AlignUp(pRemoteMemoryOut, 16);

			InjectData.m_fLdrLoadDll = fg_AutoStaticCast((void *)((umint)pRemoteNtDll + ((umint)fLdrLoadDll - (umint)pThisNtDll)));

			InjectData.m_DllCharacteristics = 0;
			InjectData.m_pModuleHandle = nullptr;

			uint8 *pRemoteFunction;
			{
				umint Size = 256; // Guess maximum of dummy function
				uint8 *pFuncOut = (uint8 *)&fg_InjectDllRemote;
				// Put function into it's own memory page
				pFuncOut = fg_SkipJumps(pFuncOut);
				if (!WriteProcessMemory(pProcess, pRemoteMemoryOut, pFuncOut, Size, &nBytes))
				{
					HRESULT Error = GetLastError();
					fReportError(NStr::CStr::CFormat("Failed to write to remote memory: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
					return EInjectDllResult_Failed;
				}

				pRemoteFunction = pRemoteMemoryOut;
				pRemoteMemoryOut += nBytes;
				pRemoteMemoryOut = fg_AlignUp(pRemoteMemoryOut, 16);
				// Flush cache needed as we are writing code here
				FlushInstructionCache(pProcess, pRemoteMemoryOut, nBytes);
			}

			CInjectDllData *pRemoteInjectData = (CInjectDllData *)pRemoteMemoryOut;
			if (!WriteProcessMemory(pProcess, pRemoteMemoryOut, &InjectData, sizeof(InjectData), &nBytes))
			{
				HRESULT Error = GetLastError();
				fReportError(NStr::CStr::CFormat("Failed to write to remote memory: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
				return EInjectDllResult_Failed;
			}

			pRemoteMemoryOut += nBytes;
			pRemoteMemoryOut = fg_AlignUp(pRemoteMemoryOut, 16);
#	ifdef DMibSupportCygwin
			bool bIsCygwin = false;
			if (_pThread && !ImageFileName.f_IsEmpty())
			{
				do
				{
					try
					{
						CFileMap FileMap(L"\\\\?\\GLOBALROOT" + ImageFileName);

						if (fg_IsCygwin((uint8 *)FileMap.m_pMemory))
							bIsCygwin = true;
					}
					catch (NException::CException const &_Exception)
					{
						(void)_Exception;
						DDTrace("File mapping failed: {}\n", _Exception.f_GetErrorStr());
					}
				}
				while (false)
					;
			}

			if (_pThread && bIsCygwin)
			{
				if (!QueueUserAPC((PAPCFUNC)pRemoteFunction, _pThread, (umint)pRemoteInjectData))
				{
					HRESULT Error = GetLastError();
					fReportError(NStr::CStr::CFormat("Failed to queue user APC: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
					return EInjectDllResult_Failed;
				}
				else
					CleanupMemory.f_Clear();

				return EInjectDllResult_Delayed;
				//HANDLE pRemoteThread = CreateRemoteThread(pProcess, NULL, 0, (LPTHREAD_START_ROUTINE)fpOtherLoadLibraryA, pRemoteMemory, 0, &ThreadID);
			}
			else
#	endif
			{
				DWORD ThreadID;
				HANDLE pRemoteThread = CreateRemoteThread(pProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pRemoteFunction, pRemoteInjectData, 0, &ThreadID);

				if (pRemoteThread)
				{
					HRESULT ErrorCreateRemote = GetLastError();
					bRet = WaitForSingleObject(pRemoteThread, INFINITE) != WAIT_TIMEOUT;
					if (!bRet)
					{
						HRESULT Error = GetLastError();
						fReportError
							(
								NStr::CStr::CFormat("Failed to join remote thread({}): {} CreateRemote: {}")
								<< pRemoteThread
								<< NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)
								<< NMib::NPlatform::fg_Win32_GetLastErrorStr(ErrorCreateRemote)
							)
						;
					}
					DWORD ExitCode = 0;
					GetExitCodeThread(pRemoteThread, &ExitCode);
					CloseHandle(pRemoteThread);

					if (ExitCode != 0)
					{
						HRESULT Result = LsaNtStatusToWinError(ExitCode);
						fReportError(NStr::CStr::CFormat("Error loading library remotely: {} ({})") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Result) << ExitCode);
						return EInjectDllResult_Failed;
					}
				}
				else
				{
					HRESULT Error = GetLastError();
					fReportError(NStr::CStr::CFormat("Failed to create remote thread (Dll Load): {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
					return EInjectDllResult_Failed;
				}
			}

			return EInjectDllResult_Done;
		}

		EInjectDllResult fg_InjectDLL(DWORD _PID, DWORD _ThreadID, WCHAR const *_pDLLName, NStr::CStr &_Error)
		{
			HANDLE pThread = nullptr;
			HANDLE pProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, _PID);
			if (NULL == pProcess)
			{
				HRESULT Error = GetLastError();
				_Error = NStr::CStr::CFormat("Open process failed: {}\r\n") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error);
				return EInjectDllResult_Failed;
			}

			if (_ThreadID)
				pThread = OpenThread(THREAD_SET_CONTEXT, false, _ThreadID);

			auto Cleanup = fg_OnScopeExit
				(
					[&]
					{
						if (pThread)
							CloseHandle(pThread);
						CloseHandle(pProcess);
					}
				)
			;

			return fg_InjectDLL(pProcess, pThread, _pDLLName, _Error);
		}
#endif
	}
}

