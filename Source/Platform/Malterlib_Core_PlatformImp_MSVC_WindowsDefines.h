// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#define DMibAllowCodeStandardViolations 1

#define WIN32_LEAN_AND_MEAN
//#define _WIN32_WINNT 0x500
//#include "sdkddkver.h"

#ifndef NTDDI_VERSION
#	define _WIN32_WINNT _WIN32_WINNT_VISTA
#	define NTDDI_VERSION NTDDI_VISTA
#	include "sdkddkver.h"
#endif

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <windows.h>
#include <psapi.h>
#include <stdlib.h>

#pragma comment(lib, "psapi.lib")

#include <windows.h>
#include <winuser.h>
#include <shellapi.h>
#pragma warning(push)
#pragma warning(disable:4201)
#include <winternl.h>
#include <winioctl.h>
#include <mmsystem.h>
#pragma warning(pop)

#pragma comment(lib, "kernel32.lib")
//#pragma comment(lib, "user32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "Crypt32.lib")

#ifdef _DEBUG
#pragma comment(lib, "libcmtd.lib")
#else
#pragma comment(lib, "libcmt.lib")
#endif

#include <crtdbg.h>


#include "Malterlib_Core_Platform_Windows_Undocumented.h"
