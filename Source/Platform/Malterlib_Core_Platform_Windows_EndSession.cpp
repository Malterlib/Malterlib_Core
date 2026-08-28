// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Windows.h"

namespace NMib::NPlatform
{
	namespace
	{
		// The session's end reaches a process through WM_QUERYENDSESSION and WM_ENDSESSION,
		// which are broadcast to top-level windows — a message-only window never sees them, so
		// a process without a window of its own needs one that exists purely to listen. Hidden,
		// never shown, on a thread of its own that does nothing but pump it
		struct CEndSessionThread : public NThread::CThread
		{
			NStr::CStr f_GetThreadName() override
			{
				return NStr::CStr("End Session Listener");
			}

			aint f_Main() override
			{
				NStr::CFStr256 ClassName = NStr::CFStr256::CFormat("MalterlibEndSessionClass_PID_0x{nfh}_THIS_0x{nfh}") << (umint)GetCurrentProcessId() << (umint)this;

				WNDCLASSA WndClass;
				memset(&WndClass, 0, sizeof(WndClass));
				WndClass.lpszClassName = ClassName;
				WndClass.lpfnWndProc = &fsp_WindowProc;
				WndClass.hInstance = g_hDllInstance;
				if (!RegisterClassA(&WndClass))
				{
					mp_StartEvent.f_SetSignaled();
					return 0;
				}

				mp_hWnd = CreateWindowExA(0, ClassName, ClassName, WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr, g_hDllInstance, nullptr);
				mp_StartEvent.f_SetSignaled();

				if (!mp_hWnd)
				{
					UnregisterClassA(ClassName, g_hDllInstance);
					return 0;
				}

				MSG Message;
				while (GetMessageA(&Message, nullptr, 0, 0) > 0)
				{
					TranslateMessage(&Message);
					DispatchMessageA(&Message);
				}

				mp_hWnd = nullptr;
				UnregisterClassA(ClassName, g_hDllInstance);

				return 0;
			}

			umint f_Stop(bool _bBlock) override
			{
				// The window closes itself and ends the pump; a thread whose window never came
				// up has already left the pump
				if (HWND hWnd = mp_hWnd)
					PostMessageA(hWnd, WM_CLOSE, 0, 0);

				return NThread::CThread::f_Stop(_bBlock);
			}

			static LRESULT WINAPI fsp_WindowProc(HWND _hWnd, UINT _Message, WPARAM _wParam, LPARAM _lParam)
			{
				switch (_Message)
				{
				case WM_ENDSESSION:
					// The query that precedes this can still be vetoed by another application, and
					// a vetoed session end arrives here with wParam false; only a confirmed end
					// commits the sticky state
					if (_wParam)
						fg_ReportIsShuttingDown();
					break;

				case WM_CLOSE:
					DestroyWindow(_hWnd);
					return 0;

				case WM_DESTROY:
					PostQuitMessage(0);
					return 0;

				default:
					break;
				}

				return DefWindowProcA(_hWnd, _Message, _wParam, _lParam);
			}

			NThread::CEvent mp_StartEvent;
			HWND volatile mp_hWnd = nullptr;
		};

		constinit NStorage::TCAggregate<CEndSessionThread> g_EndSessionThread = {DAggregateInit};

		// 0 = not started, 1 = starting, 2 = running, 3 = stopped
		constinit NAtomic::TCAtomic<uint32> g_EndSessionState{0};
	}

	void fg_EnsureEndSessionReporting()
	{
		uint32 Expected = 0;
		if (!g_EndSessionState.f_CompareExchangeStrong(Expected, 1, NAtomic::gc_MemoryOrder_AcquireRelease, NAtomic::gc_MemoryOrder_Acquire))
			return;

		g_EndSessionThread.f_Construct();
		g_EndSessionThread->mp_StartEvent.f_ResetSignaled();
		g_EndSessionThread->f_Start(EExecutionPriority_Lowest);
		g_EndSessionThread->mp_StartEvent.f_Wait();

		g_EndSessionState.f_Store(2, NAtomic::gc_MemoryOrder_Release);
	}

	void fg_StopEndSessionReporting()
	{
		uint32 Expected = 2;
		if (!g_EndSessionState.f_CompareExchangeStrong(Expected, 3, NAtomic::gc_MemoryOrder_AcquireRelease, NAtomic::gc_MemoryOrder_Acquire))
			return;

		g_EndSessionThread->f_Stop(true);
		g_EndSessionThread.f_Destruct();
	}
}
