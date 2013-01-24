/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// BKBTL.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include <commdlg.h>
#include <crtdbg.h>
#include <mmintrin.h>
#include <vfw.h>
#include <commctrl.h>

#include "BKBTL.h"
#include "Emulator.h"
#include "Dialogs.h"
#include "Views.h"
#include "Joystick.h"


//////////////////////////////////////////////////////////////////////
// Global Variables

HINSTANCE g_hInst = NULL; // current instance
HWND g_hwnd = NULL;
long m_nMainLastFrameTicks = 0;


//////////////////////////////////////////////////////////////////////
// Forward declarations

BOOL InitInstance(HINSTANCE, int);
void DoneInstance();


//////////////////////////////////////////////////////////////////////


int APIENTRY _tWinMain(
        HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPTSTR    lpCmdLine,
        int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF);
    int n = 0;
    _CrtSetBreakAlloc(n);
#endif

    g_hInst = hInstance; // Store instance handle in our global variable

    LARGE_INTEGER nFrameStartTime;
    nFrameStartTime.QuadPart = 0;

    // Initialize global strings
    LoadString(g_hInst, IDS_APP_TITLE, g_szTitle, MAX_LOADSTRING);
    LoadString(g_hInst, IDC_BKBTL, g_szWindowClass, MAX_LOADSTRING);
    MainWindow_RegisterClass();

    // Perform application initialization
    if (! InitInstance(hInstance, nCmdShow))
        return FALSE;

    HACCEL hAccelTable = ::LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_BKBTL));

    LARGE_INTEGER nPerformanceFrequency;
    ::QueryPerformanceFrequency(&nPerformanceFrequency);

    // Main message loop
    MSG msg;
    while (true)
    {
        ::QueryPerformanceCounter(&nFrameStartTime);

        if (!g_okEmulatorRunning)
            ::Sleep(20);
        else
        {
            if (Emulator_IsBreakpoint())
                Emulator_Stop();
            else
            {
                if (Emulator_SystemFrame())
                {
                    ScreenView_RedrawScreen();
                    MemoryMapView_RedrawMap();
                }
            }
        }

        // Process all queue
        while (::PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ))
        {
            if (msg.message == WM_QUIT)
                goto endprog;

            if (::TranslateAccelerator(g_hwnd, hAccelTable, &msg))
                continue;

            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
#if 1
        if (g_okEmulatorRunning /*&& Settings_GetRealSpeed()*/)
        {
            // Slow down to 25 frames per second
            LARGE_INTEGER nFrameFinishTime;  // Frame start time
            ::QueryPerformanceCounter(&nFrameFinishTime);
            LONGLONG nTimeElapsed = (nFrameFinishTime.QuadPart - nFrameStartTime.QuadPart)
                    * 1000 / nPerformanceFrequency.QuadPart;
            if (nTimeElapsed > 0 && nTimeElapsed < 20)  // 1000 millisec / 25 = 40 millisec
            {
                LONG nTimeToSleep = (LONG)(20 - nTimeElapsed);
                ::Sleep((DWORD) nTimeToSleep / 2);
                ScreenView_ScanKeyboard();
                ::Sleep((DWORD) nTimeToSleep / 2);
            }
        }
#endif
    }
endprog:

    DoneInstance();

#ifdef _DEBUG
        if (_CrtDumpMemoryLeaks())
            ::MessageBeep(MB_ICONEXCLAMATION);
#endif

    return (int) msg.wParam;
}


//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    INITCOMMONCONTROLSEX ics;  ics.dwSize = sizeof(ics);
    ics.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&ics);

    Settings_Init();
    Joystick_Init();
    Joystick_SelectJoystick(Settings_GetJoystick());
    if (!Emulator_Init())
        return FALSE;

    int conf = Settings_GetConfiguration();
    if (conf == 0) conf = BK_CONF_BK0010_BASIC;
    if (!Emulator_InitConfiguration((BKConfiguration)conf))
        return FALSE;

    Emulator_SetSound(Settings_GetSound());
    Emulator_SetCovox(Settings_GetCovox());

    // Create main window    
    g_hwnd = CreateWindow(g_szWindowClass, g_szTitle,
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_DLGFRAME | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            0, 0, 0, 0,
            NULL, NULL, hInstance, NULL);
    if (! g_hwnd)
        return FALSE;

    // Create and set up the toolbar and the statusbar
    if (!MainWindow_InitToolbar())
        return FALSE;
    if (!MainWindow_InitStatusbar())
        return FALSE;

    // Create screen window as a child of the main window
    CreateScreenView(g_hwnd, 4, 4, 582);

    MainWindow_RestoreSettings();

    MainWindow_ShowHideKeyboard();
	MainWindow_ShowHideTape();
    MainWindow_ShowHideDebug();
    MainWindow_AdjustWindowSize();

    ScreenView_Init();

    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);
    MainWindow_UpdateAllViews();
    MainWindow_UpdateMenu();

    // Autostart
    if (Settings_GetAutostart())
        ::PostMessage(g_hwnd, WM_COMMAND, ID_EMULATOR_RUN, 0);

    return TRUE;
}

// Instance finalization
void DoneInstance()
{
    ScreenView_Done();

    Emulator_Done();

    Joystick_Done();
    Settings_Done();
}


//////////////////////////////////////////////////////////////////////
