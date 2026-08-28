// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#	define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <windows.h>
#include <winternl.h>

// The ancillary function driver's poll request, the readiness primitive under every Windows
// socket: WSAPoll, select and WSAEventSelect are all built on it. Undocumented but stable since
// Vista and relied on in production by wepoll, mio and libuv, which is where these values come
// from. A poll names one socket's base handle and the events of interest, completes once — on the
// completion port the \Device\Afd handle it was issued on is associated with — when any of them
// holds, and reports the events that did. Level at arm, so present readiness completes at once
constexpr ULONG gc_AfdIoctl_Poll = 0x00012024;

constexpr ULONG gc_AfdPoll_Receive = 0x0001;
constexpr ULONG gc_AfdPoll_ReceiveExpedited = 0x0002;
constexpr ULONG gc_AfdPoll_Send = 0x0004;
// The peer closed its writing side; reads still drain what is buffered. Level set from then on
constexpr ULONG gc_AfdPoll_Disconnect = 0x0008;
// The connection was reset or abortively closed. Level set from then on
constexpr ULONG gc_AfdPoll_Abort = 0x0010;
// This side closed the socket
constexpr ULONG gc_AfdPoll_LocalClose = 0x0020;
constexpr ULONG gc_AfdPoll_Connect = 0x0040;
constexpr ULONG gc_AfdPoll_Accept = 0x0080;
constexpr ULONG gc_AfdPoll_ConnectFail = 0x0100;

struct CAfdPollHandleInfo
{
	HANDLE m_Handle;
	ULONG m_Events;
	NTSTATUS m_Status;
};

struct CAfdPollInfo
{
	LARGE_INTEGER m_Timeout;
	ULONG m_nHandles;
	ULONG m_bExclusive;
	CAfdPollHandleInfo m_Handles[1];
};

constexpr NTSTATUS gc_NtStatus_Success = 0;
constexpr NTSTATUS gc_NtStatus_Pending = 0x103;
constexpr NTSTATUS gc_NtStatus_Cancelled = (NTSTATUS)0xC0000120L;
constexpr NTSTATUS gc_NtStatus_Unsuccessful = (NTSTATUS)0xC0000001L;

// NtCreateFile's open disposition, and the information class that rebinds a handle's completion
// port (Windows 8.1 and later); neither is in the public headers
constexpr ULONG gc_NtFile_Open = 1;
constexpr ULONG gc_NtFileInformation_ReplaceCompletionInformation = 61;

struct CNtFileCompletionInformation
{
	HANDLE m_hPort;
	PVOID m_pKey;
};

