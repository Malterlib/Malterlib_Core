// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Core_Platform_Windows_Inject.h"

#include <Mib/Core/PlatformSpecific/WindowsError>
#include <Mib/Core/PlatformSpecific/WindowsOptional>
#include <Psapi.h>
#include <winternl.h>

namespace NMib
{
	namespace NPlatform
	{
		#define RVA2PTR(base, rva) (((PBYTE) base) + rva)

		//http://jpassing.wordpress.com/2008/01/06/using-import-address-table-hooking-for-testing/
		/*++
		Routine Description:
		Replace the function pointer in a module's IAT.

		Parameters:
		_hMod              - Module to use IAT from.
		_pImportedModuleName  - Name of imported DLL from which
		function is imported.
		_pImportedProcName    - Name of imported function.
		_pHookingProc       - Function to be written to IAT.
		_pOriginalProc             - Original function.

		Return Value:
		S_OK on success.
		(any HRESULT) on failure.
		--*/
		HRESULT fg_PatchIAT(HMODULE _hMod, CHAR const *_pImportedModuleName, CHAR const *_pImportedProcName, PVOID _pHookingProc, PVOID *_pOriginalProc)
		{
			PIMAGE_DOS_HEADER pDOSHeader;
			PIMAGE_NT_HEADERS pNTHeader;
			PIMAGE_IMPORT_DESCRIPTOR pImportDescriptor;
			UINT uiIter;

			pDOSHeader = (PIMAGE_DOS_HEADER) _hMod;
			pNTHeader = (PIMAGE_NT_HEADERS) RVA2PTR(pDOSHeader, pDOSHeader->e_lfanew);
			if (IMAGE_NT_SIGNATURE != pNTHeader->Signature)
				return HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
			pImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR) RVA2PTR(pDOSHeader, pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

			// Iterate over import descriptors/DLLs.
			for (uiIter = 0; pImportDescriptor[uiIter].Characteristics != 0; uiIter++)
			{
				if (pImportDescriptor[uiIter].Name == 65535)
					continue;
				PIMAGE_THUNK_DATA pFirstThunkIter;
				PIMAGE_THUNK_DATA pOriginalFirstThunkIter;
				PSTR psDLLName;
				PIMAGE_IMPORT_BY_NAME pImportByName;

				psDLLName = (PSTR) RVA2PTR(pDOSHeader, pImportDescriptor[uiIter].Name);
				if (!NStr::fg_StrCmpNoCase(psDLLName, _pImportedModuleName))
				{
					if (!pImportDescriptor[uiIter].FirstThunk || !pImportDescriptor[uiIter].OriginalFirstThunk)
						return E_INVALIDARG;

					pFirstThunkIter = (PIMAGE_THUNK_DATA) RVA2PTR(pDOSHeader, pImportDescriptor[uiIter].FirstThunk);
					pOriginalFirstThunkIter = (PIMAGE_THUNK_DATA) RVA2PTR(pDOSHeader, pImportDescriptor[uiIter].OriginalFirstThunk);

					for (; pOriginalFirstThunkIter->u1.Function != NULL; pOriginalFirstThunkIter++, pFirstThunkIter++)
					{
						if (pOriginalFirstThunkIter->u1.Ordinal & IMAGE_ORDINAL_FLAG) // Ordinal import - we can handle named imports only, so skip it.
							continue;

						pImportByName = (PIMAGE_IMPORT_BY_NAME) RVA2PTR(pDOSHeader, pOriginalFirstThunkIter->u1.AddressOfData);
						if (!NStr::fg_StrCmpNoCase(pImportByName->Name, _pImportedProcName))
						{
							DWORD dwDummy;
							MEMORY_BASIC_INFORMATION memInfoThunk;

							//DMibDTraceSafe("{} {} (0x{}) 0x{} -> 0x{}\r\n", psDLLName << (const ch8 *)pImportByName->Name << (void *)(&(pFirstThunkIter->u1.Function)) << (void *)pFirstThunkIter->u1.Function << _pHookingProc);

							// Make page writable.
							VirtualQuery(pFirstThunkIter, &memInfoThunk, sizeof(MEMORY_BASIC_INFORMATION));
							if (!VirtualProtect(memInfoThunk.BaseAddress, memInfoThunk.RegionSize, PAGE_EXECUTE_READWRITE, &memInfoThunk.Protect))
								return HRESULT_FROM_WIN32(GetLastError());

							// Replace function pointers (non-atomically).
							if (_pOriginalProc)
								*_pOriginalProc = (void *) pFirstThunkIter->u1.Function;
							pFirstThunkIter->u1.Function = (mint) _pHookingProc;

							// Restore page protection.
							if (!VirtualProtect(memInfoThunk.BaseAddress, memInfoThunk.RegionSize, memInfoThunk.Protect, &dwDummy))
								return HRESULT_FROM_WIN32(GetLastError());

							return S_OK;
						}
					}
					return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
				}
			}
			return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
		}

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

		HRESULT fg_DumpIATs(HMODULE _hMod)
		{
			PIMAGE_DOS_HEADER pDOSHeader;
			PIMAGE_NT_HEADERS pNTHeader;
			PIMAGE_IMPORT_DESCRIPTOR pImportDescriptor;
			UINT uiIter;
			PIMAGE_DELAY_IMPORT_DESCRIPTOR pDelayImportDescriptor;

			pDOSHeader = (PIMAGE_DOS_HEADER) _hMod;
			pNTHeader = (PIMAGE_NT_HEADERS) RVA2PTR(pDOSHeader, pDOSHeader->e_lfanew);
			if (IMAGE_NT_SIGNATURE != pNTHeader->Signature)
				return HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
			pImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR) RVA2PTR(pDOSHeader, pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

			DMibDTraceSafe("Import Table\r\n", 0);

			// Iterate over import descriptors/DLLs.
			for (uiIter = 0; pImportDescriptor[uiIter].Characteristics != 0; uiIter++)
			{
				PIMAGE_THUNK_DATA pFirstThunkIter;
				PIMAGE_THUNK_DATA pOriginalFirstThunkIter;
				[[maybe_unused]] PSTR psDLLName;
				[[maybe_unused]] PIMAGE_IMPORT_BY_NAME pImportByName;

				psDLLName = (PSTR) RVA2PTR(pDOSHeader, pImportDescriptor[uiIter].Name);
				DMibDTraceSafe("{}\r\n", psDLLName);
				if (!pImportDescriptor[uiIter].FirstThunk || !pImportDescriptor[uiIter].OriginalFirstThunk)
					return E_INVALIDARG;

				pFirstThunkIter = (PIMAGE_THUNK_DATA) RVA2PTR(pDOSHeader, pImportDescriptor[uiIter].FirstThunk);
				pOriginalFirstThunkIter = (PIMAGE_THUNK_DATA) RVA2PTR(pDOSHeader, pImportDescriptor[uiIter].OriginalFirstThunk);

				for (; pOriginalFirstThunkIter->u1.Function != NULL; pOriginalFirstThunkIter++, pFirstThunkIter++)
				{
					if (pOriginalFirstThunkIter->u1.Ordinal & IMAGE_ORDINAL_FLAG) // Ordinal import - we can handle named imports only, so skip it.
						continue;

					pImportByName = (PIMAGE_IMPORT_BY_NAME) RVA2PTR(pDOSHeader, pOriginalFirstThunkIter->u1.AddressOfData);
					DMibDTraceSafe("{} {} (0x{}) 0x{}\r\n", psDLLName << (const ch8 *)pImportByName->Name << (void *)(&(pFirstThunkIter->u1.Function)) << (void *)pFirstThunkIter->u1.Function);
				}
			}

			DMibDTraceSafe("Delay-Load Import Table\r\n", 0);

			pDelayImportDescriptor = (struct _IMAGE_DELAY_IMPORT_DESCRIPTOR *) RVA2PTR(pDOSHeader, pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT].VirtualAddress);
			// Iterate over import descriptors/DLLs.
			for (uiIter = 0; pDelayImportDescriptor[uiIter].grAttrs != 0; uiIter++)
			{
				PIMAGE_THUNK_DATA pFirstThunkIter;
				PIMAGE_THUNK_DATA pOriginalFirstThunkIter;
				[[maybe_unused]] PSTR psDLLName;
				[[maybe_unused]] PIMAGE_IMPORT_BY_NAME pImportByName;

				psDLLName = (PSTR) RVA2PTR(pDOSHeader, pDelayImportDescriptor[uiIter].szName);
				DMibDTraceSafe("{}\r\n", psDLLName);
				if (!pDelayImportDescriptor[uiIter].pIAT || !pDelayImportDescriptor[uiIter].pINT)
					return E_INVALIDARG;

				pFirstThunkIter = (PIMAGE_THUNK_DATA) RVA2PTR(pDOSHeader, pDelayImportDescriptor[uiIter].pIAT);
				pOriginalFirstThunkIter = (PIMAGE_THUNK_DATA) RVA2PTR(pDOSHeader, pDelayImportDescriptor[uiIter].pINT);

				for (; pOriginalFirstThunkIter->u1.Function != NULL; pOriginalFirstThunkIter++, pFirstThunkIter++)
				{
					if (pOriginalFirstThunkIter->u1.Ordinal & IMAGE_ORDINAL_FLAG) // Ordinal import - we can handle named imports only, so skip it.
						continue;

					pImportByName = (PIMAGE_IMPORT_BY_NAME) RVA2PTR(pDOSHeader, pOriginalFirstThunkIter->u1.AddressOfData);
					DMibDTraceSafe("{} {} (0x{}) 0x{}", psDLLName << (const ch8 *)pImportByName->Name << (void *)(&(pFirstThunkIter->u1.Function)) << (void *)pFirstThunkIter->u1.Function);
				}
			}

			return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
		}

		HRESULT fg_PatchDIAT(HMODULE _hMod, CHAR const *_pImportedModuleName, CHAR const *_pImportedProcName, PVOID _pHookingProc, PVOID *_pOriginalProc)
		{
			PIMAGE_DOS_HEADER pDOSHeader;
			PIMAGE_NT_HEADERS pNTHeader;
			PIMAGE_DELAY_IMPORT_DESCRIPTOR pDelayImportDescriptor;
			UINT uiIter;

			pDOSHeader = (PIMAGE_DOS_HEADER) _hMod;
			pNTHeader = (PIMAGE_NT_HEADERS) RVA2PTR(pDOSHeader, pDOSHeader->e_lfanew);
			if (IMAGE_NT_SIGNATURE != pNTHeader->Signature)
				return HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
			pDelayImportDescriptor = (struct _IMAGE_DELAY_IMPORT_DESCRIPTOR *) RVA2PTR(pDOSHeader, pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT].VirtualAddress);

			// Iterate over import descriptors/DLLs.
			for (uiIter = 0; pDelayImportDescriptor[uiIter].grAttrs != 0; uiIter++)
			{
				PIMAGE_THUNK_DATA pFirstThunkIter;
				PIMAGE_THUNK_DATA pOriginalFirstThunkIter;
				PSTR psDLLName;
				PIMAGE_IMPORT_BY_NAME pImportByName;

				psDLLName = (PSTR) RVA2PTR(pDOSHeader, pDelayImportDescriptor[uiIter].szName);
				if (!NStr::fg_StrCmpNoCase(psDLLName, _pImportedModuleName))
				{
					if (!pDelayImportDescriptor[uiIter].pIAT || !pDelayImportDescriptor[uiIter].pINT)
						return E_INVALIDARG;

					pFirstThunkIter = (PIMAGE_THUNK_DATA) RVA2PTR(pDOSHeader, pDelayImportDescriptor[uiIter].pIAT);
					pOriginalFirstThunkIter = (PIMAGE_THUNK_DATA) RVA2PTR(pDOSHeader, pDelayImportDescriptor[uiIter].pINT);

					for (; pOriginalFirstThunkIter->u1.Function != NULL; pOriginalFirstThunkIter++, pFirstThunkIter++)
					{
						if (pOriginalFirstThunkIter->u1.Ordinal & IMAGE_ORDINAL_FLAG) // Ordinal import - we can handle named imports only, so skip it.
							continue;

						pImportByName = (PIMAGE_IMPORT_BY_NAME) RVA2PTR(pDOSHeader, pOriginalFirstThunkIter->u1.AddressOfData);
						if (!NStr::fg_StrCmpNoCase(pImportByName->Name, _pImportedProcName))
						{
							DWORD dwDummy;
							MEMORY_BASIC_INFORMATION memInfoThunk;

							//DMibDTraceSafe("{} {} (0x{}) 0x{} -> 0x{}\r\n", psDLLName << (const ch8 *)pImportByName->Name << (void *)(&(pFirstThunkIter->u1.Function)) << (void *)pFirstThunkIter->u1.Function << _pHookingProc);

							// Make page writable.
							VirtualQuery(pFirstThunkIter, &memInfoThunk, sizeof(MEMORY_BASIC_INFORMATION));
							if (!VirtualProtect(memInfoThunk.BaseAddress, memInfoThunk.RegionSize, PAGE_EXECUTE_READWRITE, &memInfoThunk.Protect))
								return HRESULT_FROM_WIN32(GetLastError());

							// Replace function pointers (non-atomically).
							if (_pOriginalProc)
								*_pOriginalProc = (void *) pFirstThunkIter->u1.Function;
							pFirstThunkIter->u1.Function = (mint) _pHookingProc;

							// Restore page protection.
							if (!VirtualProtect(memInfoThunk.BaseAddress, memInfoThunk.RegionSize, memInfoThunk.Protect, &dwDummy))
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

			#define MAXMODULES 1024

			// Returns the HMODULE for module szModNameArg in the process with process-id _PID
			// Returns NULL when the mudole is not loaded
			HMODULE fg_GetModuleForProcess(DWORD _PID, const WCHAR *szModNameArg)
			{
				HANDLE hProcess;
				HMODULE ahmModules[MAXMODULES];
				DWORD dwBytes;
				unsigned int uiIter;
				TCHAR szModName[MAX_PATH];

				hProcess = OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, FALSE, _PID);
				if (NULL == hProcess)
					return NULL;

				if (EnumProcessModules(hProcess, ahmModules, sizeof(ahmModules), &dwBytes))
				{
					for (uiIter = 0; uiIter < dwBytes/sizeof(HMODULE); uiIter++)
					{
						if (GetModuleBaseName(hProcess, ahmModules[uiIter], szModName, sizeof(szModName)/sizeof(TCHAR)))
						{
							if (NStr::fg_StrCmpNoCase(szModName, szModNameArg) == 0)
							{
								CloseHandle(hProcess);
								return ahmModules[uiIter];
							}
						}
					}
				}

				CloseHandle(hProcess);

				return NULL;
			}


		}


		// Remove DLL _pDLLName from process with process-id _PID
		// Don't use a full pathname for _pDLLName
		// Returns true on succes, false on failure
		BOOL fg_DejectDLL(DWORD _PID, const WCHAR *_pDLLName, NStr::CStr &_Error)
		{
			HMODULE hmKernel32;
			FARPROC fpFreeLibrary;
			HANDLE hProcess;
			HMODULE hmDLL;
			HANDLE hRemoteThread;
			BOOL bRet;

			hmKernel32 = GetModuleHandle(str_utf16("Kernel32"));
			if (NULL == hmKernel32)
			{
				HRESULT Error = GetLastError();
				_Error = NStr::CStr::CFormat("Get kernel32 module handle failed: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error);
				return FALSE;
			}

			fpFreeLibrary = GetProcAddress(hmKernel32, "FreeLibrary");
			if (NULL == fpFreeLibrary)
			{
				HRESULT Error = GetLastError();
				_Error = NStr::CStr::CFormat("Extracting FreeLibrary failed: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error);
				return FALSE;
			}

			hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, _PID);
			if (NULL == hProcess)
			{
				HRESULT Error = GetLastError();
				_Error = NStr::CStr::CFormat("Open process failed: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error);
				return FALSE;
			}

			hmDLL = fg_GetModuleForProcess(_PID, _pDLLName);
			if (NULL == hmDLL)
			{
				_Error = NStr::CStr::CFormat("Could not find module in process");
				CloseHandle(hProcess);
				return FALSE;
			}

			DWORD ExitCode = 1;
			bRet = true;

			while (ExitCode && bRet)
			{
				hRemoteThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)fpFreeLibrary, hmDLL, 0, NULL);

				if (0 != hRemoteThread)
					bRet = WaitForSingleObject(hRemoteThread, 1000) != WAIT_TIMEOUT;
				else
				{
					HRESULT Error = GetLastError();
					_Error = NStr::CStr::CFormat("Failed to create remote thread (Dll Unload): {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error);
					bRet = FALSE;
				}

				GetExitCodeThread(hRemoteThread, &ExitCode);
				CloseHandle(hRemoteThread);
			}
			CloseHandle(hProcess);


			return bRet;
		}

		inline_never DWORD WINAPI fg_DummyThreadProc(LPVOID lpParameter)
		{
			return 0;
		}

		uint8 *fg_SkipJumps(uint8 *pbCode)
		{
		#if defined(DArchitecture_x86) || defined(DArchitecture_x64)

		#if defined DArchitecture_x64
			if ((pbCode[0] & 0xF0) == 0x40 && pbCode[1] == 0xff && pbCode[2] == 0x25)
			{
				// REX PREFIX, just skip it
				++pbCode;
			}
		#endif

			if (pbCode[0] == 0xff && pbCode[1] == 0x25)
			{
		#ifdef DArchitecture_x86
				// on x86 we have an absolute pointer...
				uint8 *pbTarget = *(uint8 **)&pbCode[2];
				// ... that shows us an absolute pointer.
				return fg_SkipJumps(*(uint8 **)pbTarget);
		#elif defined DArchitecture_x64
				// on x64 we have a 32-bit offset...
				INT32 lOffset = *(INT32 *)&pbCode[2];
				// ... that shows us an absolute pointer
				return fg_SkipJumps(*(uint8 **)(pbCode + 6 + lOffset));
		#endif
			}
			else if (pbCode[0] == 0xe9)
			{
				// here the behavior is identical, we have...
				// ...a 32-bit offset to the destination.
				return fg_SkipJumps(pbCode + 5 + *(INT32 *)&pbCode[1]);
			}
			else if (pbCode[0] == 0xeb)
			{
				// and finally an 8-bit offset to the destination
				return fg_SkipJumps(pbCode + 2 + *(CHAR *)&pbCode[1]);
			}
		#else
		#	error unsupported platform
		#endif
			return pbCode;
		}


		struct CInjectDllData
		{
			NTSTATUS  ( __stdcall *m_pLdrLoadDll)(PWCHAR SearchPath, PULONG DllCharacteristics, PUNICODE_STRING ModuleFileName, PHANDLE ModuleHandle);
			PWCHAR m_pSearchPath;
			ULONG m_DllCharacteristics;
			UNICODE_STRING m_ModuleFileName;
			HANDLE m_pModuleHandle;
		};

		// Runtime checks needs to be off here, as we need this code to be location independant
		#pragma runtime_checks( "", off )

		inline_never DWORD WINAPI fg_InjectDllRemote(CInjectDllData *_pData)
		{
			return _pData->m_pLdrLoadDll(_pData->m_pSearchPath, &_pData->m_DllCharacteristics, &_pData->m_ModuleFileName, &_pData->m_pModuleHandle);
		}

		#pragma runtime_checks( "", restore )

		#ifdef DMibSupportCygwin
		class CWindowsHandle
		{
			HANDLE m_Handle;
			CWindowsHandle(CWindowsHandle const &_Other);
			CWindowsHandle &operator =(CWindowsHandle const &_Other);
		public:
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

			operator HANDLE()
			{
				return m_Handle;
			}
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

			operator void *()
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


		#endif

		extern "C" ULONG
		NTAPI
		LsaNtStatusToWinError(
			_In_ NTSTATUS Status
			);

		#pragma comment(lib, "Advapi32.lib")
		// Inject DLL _pDLLName in process with process-id _PID
		// If the DLL is not in the PATH of the process, use a full pathname for _pDLLName
		// Returns true on succes, false on failure

		// #define DMibSupportCygwin

		PIMAGE_NT_HEADERS fg_GetImageHeaders(HMODULE _hModule)
		{
			PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)_hModule;

			if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE)
				return nullptr;

			PIMAGE_NT_HEADERS pRet = (PIMAGE_NT_HEADERS)((uint8 *)_hModule + pDosHeader->e_lfanew);
			// Our buffer is 4096 bytes max
			if (((uint8 *)(pRet + 1) - (uint8 *)_hModule) > 4096)
				return nullptr;

			if (pRet->Signature != IMAGE_NT_SIGNATURE)
				return nullptr;

			return pRet;
		}



		EInjectDllResult fg_InjectDLL(HANDLE _hProcess, HANDLE _hThread, const WCHAR *_pDLLName, NStr::CStr &_Error)
		{
		//	NMib::NTime::CTimerTraceScope Scope("fg_InjectDLL");
		//	DMibTrace("Injecting Dll: {}\r\n", _pDLLName);
			BOOL bRet = true;
			SIZE_T dwBytes;

		//	LoadLibraryW(_pDLLName);

			HANDLE hProcess = _hProcess;

			auto fl_ReportError
				= [&](NStr::CStr const &_NewError)
				{
					if (!_Error.f_IsEmpty())
						_Error += "\r\n";
					_Error += _NewError;
				}
			;

			HMODULE hThisNtDll = NLocal::g_hNtDll;
			if (!hThisNtDll)
			{
				HRESULT Error = GetLastError();
				fl_ReportError(NStr::CStr::CFormat("Get kernel32 module handle failed: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
				return EInjectDllResult_Failed;
			}

			FARPROC fpLdrLoadDll = GetProcAddress(hThisNtDll, "LdrLoadDll");
			if (!fpLdrLoadDll)
			{
				HRESULT Error = GetLastError();
				fl_ReportError(NStr::CStr::CFormat("Extracting LdrLoadDll failed: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
				return EInjectDllResult_Failed;
			}

			auto pThisNtDllHeaders = fg_GetImageHeaders(hThisNtDll);

			if (!pThisNtDllHeaders)
			{
				HRESULT Error = GetLastError();
				fl_ReportError(NStr::CStr::CFormat("Could not find NT headers in local ntdll.dll: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
				return EInjectDllResult_Failed;
			}

		#ifdef DMibSupportCygwin
			NStr::CWStr ImageFileName;

			{
				UndocumentedPEB Peb;
				PROCESS_BASIC_INFORMATION BasicInfo;
				ULONG RetLen = 0;
				if (NLocal::g_OptionalFunctions.m_fNtQueryInformationProcess && !NLocal::g_OptionalFunctions.m_fNtQueryInformationProcess(hProcess, ProcessBasicInformation, &BasicInfo, sizeof(BasicInfo), &RetLen) && RetLen == sizeof(BasicInfo))
				{
					SIZE_T ReadBytes = 0;
					if (ReadProcessMemory(hProcess, BasicInfo.PebBaseAddress, &Peb, sizeof(Peb), &ReadBytes) && ReadBytes == sizeof(Peb))
					{
						auto pData = ImageFileName.f_GetStr(NMib::NFile::NPlatform::gc_MaxWindowsPath);
						pData[0] = 0;
						GetMappedFileNameW(hProcess, (HMODULE)Peb.ImageBaseAddress, pData, NMib::NFile::NPlatform::gc_MaxWindowsPath);
						ImageFileName.f_GetLen();
					}
				}
			}
		#endif

			HMODULE hRemoteNtDll = 0;
			{
				mint StartAddress = TCLimitsInt<mint>::mc_Min;
				mint CurrentAddress = StartAddress;
				while (true)
				{
					MEMORY_BASIC_INFORMATION MemInfo;
					if (!VirtualQueryEx(_hProcess, (void *)CurrentAddress, &MemInfo, sizeof(MemInfo)))
						break;
					if (MemInfo.BaseAddress == MemInfo.AllocationBase && (MemInfo.Type & MEM_IMAGE))
					{
						//NStr::CWStr BaseName;
						//GetMappedFileNameW(hProcess, (HMODULE)MemInfo.AllocationBase, BaseName.f_GetStr(1024), 1024);
						//BaseName.f_GetLen();
						//DTrace("0x{nfh,sj16,sf0} -> 0x{nfh,sj16,sf0}: State: 0x{nfh,sj8,sf0}: {}\n", MemInfo.BaseAddress << ((mint)MemInfo.BaseAddress + MemInfo.RegionSize) << MemInfo.State << BaseName);

						if (!hRemoteNtDll)
						{
							uint8 Temp[4096];
							if (ReadProcessMemory(hProcess, MemInfo.BaseAddress, Temp, 4096, &dwBytes) && dwBytes == 4096)
							{
								auto pRemoteHeaders = fg_GetImageHeaders((HMODULE)Temp);

								if (pRemoteHeaders && NMemory::fg_MemCmp((uint8 *)pRemoteHeaders, (uint8 *)pThisNtDllHeaders, sizeof(*pThisNtDllHeaders)) == 0)
								{
									// If the headers are the same, it means that the checksum is the same
									hRemoteNtDll = (HMODULE)MemInfo.AllocationBase;
									break;
								}
							}
						}
					}
					if (CurrentAddress == (mint)MemInfo.BaseAddress + MemInfo.RegionSize)
						break;
					else
						CurrentAddress = (mint)MemInfo.BaseAddress + MemInfo.RegionSize;
				}
			}

			if (!hRemoteNtDll)
			{
				fl_ReportError("Failed to find ntdll.dll in remote process.\r\n");
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


			size_t StringMemorySize = (NStr::fg_StrLen(_pDLLName)+1) * 2;
			size_t RemoteMemorySize = StringMemorySize;
			size_t DllDirectoriesSize = (DllDirectories.f_GetLen() + 1) * 2;
			RemoteMemorySize += DllDirectoriesSize;

			RemoteMemorySize += 256+16*4;
			RemoteMemorySize += sizeof(CInjectDllData);

			// Get memory at an address that is free in this process so eventual fork wont have problems
			uint8 *lpLocalMemory = (uint8 *)VirtualAllocEx(GetCurrentProcess(), NULL, RemoteMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_EXECUTE_READWRITE);
			if (!lpLocalMemory)
			{
				HRESULT Error = GetLastError();
				fl_ReportError(NStr::CStr::CFormat("Failed to allocate local memory: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
				return EInjectDllResult_Failed;
			}
			VirtualFreeEx(GetCurrentProcess(), lpLocalMemory, RemoteMemorySize, MEM_RELEASE);

			uint8 *lpRemoteMemory = (uint8 *)VirtualAllocEx(hProcess, lpLocalMemory, RemoteMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_EXECUTE_READWRITE);
			if (!lpRemoteMemory)
				lpRemoteMemory = (uint8 *)VirtualAllocEx(hProcess, nullptr, RemoteMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_EXECUTE_READWRITE); // Just alloc anywhere

			if (!lpRemoteMemory)
			{
				HRESULT Error = GetLastError();
				fl_ReportError(NStr::CStr::CFormat("Failed to allocate remote memory: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
				return EInjectDllResult_Failed;
			}

			auto CleanupMemory
				= fg_OnScopeExit
				(
					[&]
					{
						VirtualFreeEx(hProcess, lpRemoteMemory, RemoteMemorySize, MEM_RELEASE);
					}
				)
			;

			// Write String
			uint8 *lpRemoteMemoryOut = lpRemoteMemory;
			if (!WriteProcessMemory(hProcess, lpRemoteMemoryOut, _pDLLName, StringMemorySize, &dwBytes))
			{
				HRESULT Error = GetLastError();
				fl_ReportError(NStr::CStr::CFormat("Failed to write to remote memory: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
				return EInjectDllResult_Failed;
			}

			CInjectDllData InjectData;
			InjectData.m_ModuleFileName.Buffer = (ch16 *)lpRemoteMemoryOut;
			InjectData.m_ModuleFileName.Length = (uint16)NStr::fg_StrLen(_pDLLName)*sizeof(ch16);
			InjectData.m_ModuleFileName.MaximumLength = (uint16)dwBytes;

			lpRemoteMemoryOut += dwBytes;
			lpRemoteMemoryOut = fg_AlignUp(lpRemoteMemoryOut, 16);

			if (!WriteProcessMemory(hProcess, lpRemoteMemoryOut, DllDirectories.f_GetStr(), DllDirectoriesSize, &dwBytes))
			{
				HRESULT Error = GetLastError();
				fl_ReportError(NStr::CStr::CFormat("Failed to write to remote memory: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
				return EInjectDllResult_Failed;
			}

			InjectData.m_pSearchPath = (PWCHAR)lpRemoteMemoryOut;

			lpRemoteMemoryOut += dwBytes;
			lpRemoteMemoryOut = fg_AlignUp(lpRemoteMemoryOut, 16);

			InjectData.m_pLdrLoadDll = fg_AutoStaticCast((void *)((mint)hRemoteNtDll + ((mint)fpLdrLoadDll - (mint)hThisNtDll)));

			InjectData.m_DllCharacteristics = 0;
			InjectData.m_pModuleHandle = nullptr;

			uint8 *pRemoteFunction;
			{

				mint Size = 256; // Guess maximum of dummy function
				uint8 *pFuncOut = (uint8 *)&fg_InjectDllRemote;
				// Put function into it's own memory page
				pFuncOut = fg_SkipJumps(pFuncOut);
				if (!WriteProcessMemory(hProcess, lpRemoteMemoryOut, pFuncOut, Size, &dwBytes))
				{
					HRESULT Error = GetLastError();
					fl_ReportError(NStr::CStr::CFormat("Failed to write to remote memory: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
					return EInjectDllResult_Failed;
				}

				pRemoteFunction = lpRemoteMemoryOut;
				lpRemoteMemoryOut += dwBytes;
				lpRemoteMemoryOut = fg_AlignUp(lpRemoteMemoryOut, 16);
				// Flush cache needed as we are writing code here
				FlushInstructionCache(hProcess, lpRemoteMemoryOut, dwBytes);
			}

			CInjectDllData *pRemoteInjectData = (CInjectDllData *)lpRemoteMemoryOut;
			if (!WriteProcessMemory(hProcess, lpRemoteMemoryOut, &InjectData, sizeof(InjectData), &dwBytes))
			{
				HRESULT Error = GetLastError();
				fl_ReportError(NStr::CStr::CFormat("Failed to write to remote memory: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
				return EInjectDllResult_Failed;
			}

			lpRemoteMemoryOut += dwBytes;
			lpRemoteMemoryOut = fg_AlignUp(lpRemoteMemoryOut, 16);
		#ifdef DMibSupportCygwin
			bool bIsCygwin = false;
			if (_hThread && !ImageFileName.f_IsEmpty())
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

			if (_hThread && bIsCygwin)
			{
				if (!QueueUserAPC((PAPCFUNC)pRemoteFunction, _hThread, (mint)pRemoteInjectData))
				{
					HRESULT Error = GetLastError();
					fl_ReportError(NStr::CStr::CFormat("Failed to queue user APC: {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
					return EInjectDllResult_Failed;
				}
				else
					CleanupMemory.f_Clear();

				return EInjectDllResult_Delayed;
				//HANDLE hRemoteThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)fpOtherLoadLibraryA, lpRemoteMemory, 0, &ThreadID);
			}
			else
		#endif
			{
				DWORD ThreadID;
				HANDLE hRemoteThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pRemoteFunction, pRemoteInjectData, 0, &ThreadID);


				if (hRemoteThread)
				{
					HRESULT ErrorCreateRemote = GetLastError();
					bRet = WaitForSingleObject(hRemoteThread, INFINITE) != WAIT_TIMEOUT;
					if (!bRet)
					{
						HRESULT Error = GetLastError();
						fl_ReportError
							(
								NStr::CStr::CFormat("Failed to join remote thread({}): {} CreateRemote: {}")
								<< hRemoteThread
								<< NMib::NPlatform::fg_Win32_GetLastErrorStr(Error)
								<< NMib::NPlatform::fg_Win32_GetLastErrorStr(ErrorCreateRemote)
							)
						;
					}
					DWORD ExitCode = 0;
					GetExitCodeThread(hRemoteThread, &ExitCode);
					CloseHandle(hRemoteThread);

					if (ExitCode != 0)
					{
						HRESULT Result = LsaNtStatusToWinError(ExitCode);
						fl_ReportError(NStr::CStr::CFormat("Error loading library remotely: {} ({})") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Result) << ExitCode);
						return EInjectDllResult_Failed;
					}
				}
				else
				{
					HRESULT Error = GetLastError();
					fl_ReportError(NStr::CStr::CFormat("Failed to create remote thread (Dll Load): {}") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error));
					return EInjectDllResult_Failed;
				}
			}

			return EInjectDllResult_Done;
		}

		EInjectDllResult fg_InjectDLL(DWORD _PID, DWORD _ThreadID, const WCHAR *_pDLLName, NStr::CStr &_Error)
		{
			HANDLE hThread = nullptr;
			HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, _PID);
			if (NULL == hProcess)
			{
				HRESULT Error = GetLastError();
				_Error = NStr::CStr::CFormat("Open process failed: {}\r\n") << NMib::NPlatform::fg_Win32_GetLastErrorStr(Error);
				return EInjectDllResult_Failed;
			}

			if (_ThreadID)
			{
				hThread = OpenThread(THREAD_SET_CONTEXT, false, _ThreadID);
			}

			auto Cleanup
				= fg_OnScopeExit
				(
					[&]
					{
						if (hThread)
							CloseHandle(hThread);
						CloseHandle(hProcess);
					}
				)
			;

			return fg_InjectDLL(hProcess, hThread, _pDLLName, _Error);
		}
#endif
	}
}

