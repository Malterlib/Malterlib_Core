// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Core_Platform_Windows.h"

namespace NMib::NPlatform
{
	namespace
	{
		// Session-end broadcasts need a hidden top-level window; message-only windows never receive them.
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
					mp_WindowSettled.f_SetSignaled();
					return 0;
				}

				mp_hWnd = CreateWindowExA(0, ClassName, ClassName, WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr, g_hDllInstance, nullptr);
				mp_WindowSettled.f_SetSignaled();

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

				UnregisterClassA(ClassName, g_hDllInstance);

				return 0;
			}

			umint f_Stop(bool _bBlock) override
			{
				if (f_GetState() == NThread::EThreadState_Running)
				{
					mp_WindowSettled.f_Wait();
					if (mp_hWnd)
						PostMessageA(mp_hWnd, WM_CLOSE, 0, 0);
				}

				return NThread::CThread::f_Stop(_bBlock);
			}

		private:
			static LRESULT WINAPI fsp_WindowProc(HWND _hWnd, UINT _Message, WPARAM _wParam, LPARAM _lParam)
			{
				switch (_Message)
				{
				case WM_ENDSESSION:
					// Another application can veto session end; commit state only for a confirmed WM_ENDSESSION.
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

			NThread::CEvent mp_WindowSettled;
			HWND mp_hWnd = nullptr;
		};

		struct CSubSystem_Core_EndSession : public CSubSystem
		{
			CSubSystem_Core_EndSession()
			{
				m_Thread.f_Start(EExecutionPriority_High);
			}

			void f_DestroyThreadSpecific() override
			{
				m_Thread.f_Stop(true);
			}

			CEndSessionThread m_Thread;
		};

		constinit TCSubSystem<CSubSystem_Core_EndSession, ESubSystemDestruction_BeforeMemoryManager> g_SubSystem_Core_EndSession = {DAggregateInit};
	}

	// Returns before the window exists; the report it feeds is only read at a child process exit
	void fg_EnsureEndSessionReporting()
	{
		if (fg_GetSys()->f_DestroyingThreadSpecific())
			return;

		(void)*g_SubSystem_Core_EndSession;
	}
}
