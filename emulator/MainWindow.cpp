﻿/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// MainWindow.cpp

#include "stdafx.h"
#include <commdlg.h>
#include <crtdbg.h>
#include <mmintrin.h>
#include <Vfw.h>
#include <CommCtrl.h>

#include "Main.h"
#include "Emulator.h"
#include "Dialogs.h"
#include "Views.h"
#include "ToolWindow.h"
#include "Joystick.h"

//////////////////////////////////////////////////////////////////////


TCHAR g_szTitle[MAX_LOADSTRING];            // The title bar text
TCHAR g_szWindowClass[MAX_LOADSTRING];      // Main window class name

HWND m_hwndToolbar = NULL;
HWND m_hwndStatusbar = NULL;
HWND m_hwndSplitter = (HWND)INVALID_HANDLE_VALUE;

int m_MainWindowMinCx = BK_SCREEN_WIDTH + 16;
int m_MainWindowMinCy = BK_SCREEN_HEIGHT + 40;


//////////////////////////////////////////////////////////////////////
// Forward declarations

BOOL MainWindow_InitToolbar();
BOOL MainWindow_InitStatusbar();
void MainWindow_RestorePositionAndShow();
LRESULT CALLBACK MainWindow_WndProc(HWND, UINT, WPARAM, LPARAM);
void MainWindow_AdjustWindowLayout();
bool MainWindow_DoCommand(int commandId);
void MainWindow_DoViewDebug();
void MainWindow_DoDebugMemoryMap();
void MainWindow_DoDebugTeletype();
void MainWindow_DoViewToolbar();
void MainWindow_DoViewKeyboard();
void MainWindow_DoViewTape();
void MainWindow_DoViewScreenColor();
void MainWindow_DoViewScreenMode(int newMode);
void MainWindow_DoViewSpriteViewer();
void MainWindow_DoEmulatorRun();
void MainWindow_DoEmulatorAutostart();
void MainWindow_DoEmulatorReset();
void MainWindow_DoEmulatorSpeed(WORD speed);
void MainWindow_DoEmulatorSound();
void MainWindow_DoEmulatorCovox();
void MainWindow_DoEmulatorSoundAY();
void MainWindow_DoEmulatorJoystick(int joystickNum);
void MainWindow_DoFileSaveState();
void MainWindow_DoFileLoadState();
void MainWindow_DoEmulatorFloppy(int slot);
void MainWindow_DoEmulatorConf(BKConfiguration configuration);
void MainWindow_DoFileScreenshot();
void MainWindow_DoFileScreenshotToClipboard();
void MainWindow_DoFileScreenshotSaveAs();
void MainWindow_DoFileLoadBin();
void MainWindow_DoFileSettings();
void MainWindow_DoFileSettingsColors();
void MainWindow_OnToolbarGetInfoTip(LPNMTBGETINFOTIP);


//////////////////////////////////////////////////////////////////////


void MainWindow_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = MainWindow_WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = g_hInst;
    wcex.hIcon          = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_APPICON));
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);
    wcex.lpszMenuName   = MAKEINTRESOURCE(IDC_APPLICATION);
    wcex.lpszClassName  = g_szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    RegisterClassEx(&wcex);

    ToolWindow_RegisterClass();
    OverlappedWindow_RegisterClass();
    SplitterWindow_RegisterClass();

    // Register view classes
    ScreenView_RegisterClass();
    KeyboardView_RegisterClass();
    MemoryView_RegisterClass();
    DebugView_RegisterClass();
    MemoryMapView_RegisterClass();
    SpriteView_RegisterClass();
    DisasmView_RegisterClass();
    ConsoleView_RegisterClass();
    TapeView_RegisterClass();
}

BOOL CreateMainWindow()
{
    // Create the window
    g_hwnd = CreateWindow(
            g_szWindowClass, g_szTitle,
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            0, 0, 0, 0,
            NULL, NULL, g_hInst, NULL);
    if (!g_hwnd)
        return FALSE;

    // Create and set up the toolbar and the statusbar
    if (!MainWindow_InitToolbar())
        return FALSE;
    if (!MainWindow_InitStatusbar())
        return FALSE;

    DebugView_Init();
    DisasmView_Init();
    ScreenView_Init();

    // Create screen window as a child of the main window
    ScreenView_Create(g_hwnd, 4, 4);

    MainWindow_RestoreSettings();

    MainWindow_ShowHideToolbar();
    MainWindow_ShowHideKeyboard();
    MainWindow_ShowHideTape();
    MainWindow_ShowHideDebug();

    MainWindow_RestorePositionAndShow();

    UpdateWindow(g_hwnd);
    MainWindow_UpdateAllViews();
    MainWindow_UpdateMenu();
    MainWindow_UpdateWindowTitle();

    // Autostart
    if (Settings_GetAutostart())
        ::PostMessage(g_hwnd, WM_COMMAND, ID_EMULATOR_RUN, 0);

    return TRUE;
}

BOOL MainWindow_InitToolbar()
{
    m_hwndToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL,
            WS_CHILD | TBSTYLE_FLAT | TBSTYLE_TRANSPARENT | TBSTYLE_LIST | TBSTYLE_TOOLTIPS | CCS_NOPARENTALIGN | CCS_NODIVIDER,
            4, 4, 0, 0, g_hwnd,
            (HMENU) 102,
            g_hInst, NULL);
    if (! m_hwndToolbar)
        return FALSE;

    SendMessage(m_hwndToolbar, TB_SETEXTENDEDSTYLE, 0, (LPARAM) (DWORD) TBSTYLE_EX_MIXEDBUTTONS);
    SendMessage(m_hwndToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM) sizeof(TBBUTTON), 0);
    SendMessage(m_hwndToolbar, TB_SETBUTTONSIZE, 0, (LPARAM) MAKELONG (26, 26));

    TBADDBITMAP addbitmap;
    addbitmap.hInst = g_hInst;
    addbitmap.nID = IDB_TOOLBAR;
    SendMessage(m_hwndToolbar, TB_ADDBITMAP, 2, (LPARAM) &addbitmap);

    TBBUTTON buttons[11];
    ZeroMemory(buttons, sizeof(buttons));
    for (int i = 0; i < sizeof(buttons) / sizeof(TBBUTTON); i++)
    {
        buttons[i].fsState = TBSTATE_ENABLED;
        buttons[i].iString = -1;
    }
    buttons[0].idCommand = ID_EMULATOR_RUN;
    buttons[0].iBitmap = ToolbarImageRun;
    buttons[0].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[0].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("Run"));
    buttons[1].idCommand = ID_EMULATOR_RESET;
    buttons[1].iBitmap = ToolbarImageReset;
    buttons[1].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[1].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("Reset"));
    buttons[2].fsStyle = BTNS_SEP;
    buttons[3].idCommand = ID_EMULATOR_FLOPPY0;
    buttons[3].iBitmap = ToolbarImageFloppySlot;
    buttons[3].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[3].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("A"));
    buttons[4].idCommand = ID_EMULATOR_FLOPPY1;
    buttons[4].iBitmap = ToolbarImageFloppySlot;
    buttons[4].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[4].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("B"));
    buttons[5].idCommand = ID_EMULATOR_FLOPPY2;
    buttons[5].iBitmap = ToolbarImageFloppySlot;
    buttons[5].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[5].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("C"));
    buttons[6].idCommand = ID_EMULATOR_FLOPPY3;
    buttons[6].iBitmap = ToolbarImageFloppySlot;
    buttons[6].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[6].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("D"));
    buttons[7].fsStyle = BTNS_SEP;
    buttons[8].idCommand = ID_VIEW_RGBSCREEN;
    buttons[8].iBitmap = ToolbarImageColorScreen;
    buttons[8].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[8].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("Color"));
    buttons[9].idCommand = ID_EMULATOR_SOUND;
    buttons[9].iBitmap = ToolbarImageSoundOff;
    buttons[9].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[9].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("Sound"));
    buttons[10].idCommand = ID_FILE_SCREENSHOT;
    buttons[10].iBitmap = ToolbarImageScreenshot;
    buttons[10].fsStyle = BTNS_BUTTON;

    SendMessage(m_hwndToolbar, TB_ADDBUTTONS, (WPARAM) sizeof(buttons) / sizeof(TBBUTTON), (LPARAM) &buttons);

    if (Settings_GetToolbar())
        ShowWindow(m_hwndToolbar, SW_SHOW);

    return TRUE;
}

BOOL MainWindow_InitStatusbar()
{
    TCHAR buffer[100];
    _sntprintf(buffer, sizeof(buffer) / sizeof(TCHAR) - 1, _T("%s version %s"), g_szTitle, _T(APP_VERSION_STRING));
    m_hwndStatusbar = CreateStatusWindow(
            WS_CHILD | WS_VISIBLE | SBT_TOOLTIPS | CCS_NOPARENTALIGN | CCS_NODIVIDER,
            buffer,
            g_hwnd, 101);
    if (! m_hwndStatusbar)
        return FALSE;

    int statusbarParts[6];
    statusbarParts[0] = 280;
    statusbarParts[1] = statusbarParts[0] + 45;  // Sound
    statusbarParts[2] = statusbarParts[1] + 45;  // Motor
    statusbarParts[3] = statusbarParts[2] + 50;  // FPS
    statusbarParts[4] = statusbarParts[3] + 105; // Uptime
    statusbarParts[5] = -1;
    SendMessage(m_hwndStatusbar, SB_SETPARTS, sizeof(statusbarParts) / sizeof(int), (LPARAM)statusbarParts);

    return TRUE;
}

void MainWindow_RestoreSettings()
{
    TCHAR buf[MAX_PATH];

    // Reattach floppy images
    for (int slot = 0; slot < 4; slot++)
    {
        buf[0] = _T('\0');
        Settings_GetFloppyFilePath(slot, buf);
        if (buf[0] != _T('\0'))
        {
            if (! g_pBoard->AttachFloppyImage(slot, buf))
                Settings_SetFloppyFilePath(slot, NULL);
        }
    }

    // Restore ScreenViewMode
    int scrmode = Settings_GetScreenViewMode();
    if (scrmode <= 0 || scrmode > 9) scrmode = 0;
    ScreenView_SetScreenMode(scrmode);
}

void MainWindow_SavePosition()
{
    WINDOWPLACEMENT placement;
    placement.length = sizeof(WINDOWPLACEMENT);
    ::GetWindowPlacement(g_hwnd, &placement);

    Settings_SetWindowRect(&(placement.rcNormalPosition));
    Settings_SetWindowMaximized(placement.showCmd == SW_SHOWMAXIMIZED);
}
void MainWindow_RestorePositionAndShow()
{
    RECT rc;
    if (Settings_GetWindowRect(&rc))
    {
        HMONITOR hmonitor = MonitorFromRect(&rc, MONITOR_DEFAULTTONULL);
        if (hmonitor != NULL)
        {
            ::SetWindowPos(g_hwnd, NULL, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                    SWP_NOACTIVATE | SWP_NOZORDER);
        }
    }

    ShowWindow(g_hwnd, Settings_GetWindowMaximized() ? SW_SHOWMAXIMIZED : SW_SHOW);
}

void MainWindow_UpdateWindowTitle()
{
    LPCTSTR confName = Emulator_GetConfigurationName();
    LPCTSTR emustate = g_okEmulatorRunning ? _T("run") : _T("stop");
    TCHAR buffer[120];
    _sntprintf(buffer, sizeof(buffer) / sizeof(TCHAR) - 1, _T("%s - %s - [%s]"), g_szTitle, confName, emustate);
    SetWindowText(g_hwnd, buffer);
}

// Processes messages for the main window
LRESULT CALLBACK MainWindow_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_ACTIVATE:
        SetFocus(g_hwndScreen);
        break;
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            //int wmEvent = HIWORD(wParam);
            bool okProcessed = MainWindow_DoCommand(wmId);
            if (!okProcessed)
                return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    case WM_DESTROY:
        MainWindow_SavePosition();
        PostQuitMessage(0);
        break;
    case WM_SIZE:
        MainWindow_AdjustWindowLayout();
        break;
    case WM_GETMINMAXINFO:
        {
            DefWindowProc(hWnd, message, wParam, lParam);
            MINMAXINFO* mminfo = (MINMAXINFO*)lParam;
            mminfo->ptMinTrackSize.x = m_MainWindowMinCx;
            mminfo->ptMinTrackSize.y = m_MainWindowMinCy;
        }
        break;
    case WM_NOTIFY:
        {
            //int idCtrl = (int) wParam;
            HWND hwndFrom = ((LPNMHDR) lParam)->hwndFrom;
            UINT code = ((LPNMHDR) lParam)->code;
            if (code == TTN_SHOW)
            {
                return 0;
            }
            else if (hwndFrom == m_hwndToolbar && code == TBN_GETINFOTIP)
            {
                MainWindow_OnToolbarGetInfoTip((LPNMTBGETINFOTIP) lParam);
            }
            else
                return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
        //case WM_DRAWITEM:
        //    {
        //        int idCtrl = (int) wParam;
        //        HWND hwndItem = ((LPDRAWITEMSTRUCT) lParam)->hwndItem;
        //        if (hwndItem == m_hwndStatusbar)
        //            MainWindow_OnStatusbarDrawItem((LPDRAWITEMSTRUCT) lParam);
        //        else
        //            return DefWindowProc(hWnd, message, wParam, lParam);
        //    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void MainWindow_AdjustWindowSize()
{
    const int MAX_DEBUG_WIDTH = 1450;
    const int MAX_DEBUG_HEIGHT = 1400;

    WINDOWPLACEMENT placement;
    placement.length = sizeof(WINDOWPLACEMENT);
    ::GetWindowPlacement(g_hwnd, &placement);
    if (placement.showCmd == SW_MAXIMIZE)
        return;

    // Get metrics
    int cxFrame   = ::GetSystemMetrics(SM_CXSIZEFRAME);
    int cyFrame   = ::GetSystemMetrics(SM_CYSIZEFRAME);
    int cyCaption = ::GetSystemMetrics(SM_CYCAPTION);
    int cyMenu    = ::GetSystemMetrics(SM_CYMENU);

    RECT rcToolbar;  GetWindowRect(m_hwndToolbar, &rcToolbar);
    int cyToolbar = rcToolbar.bottom - rcToolbar.top;
    RECT rcScreen;  GetWindowRect(g_hwndScreen, &rcScreen);
    int cxScreen = rcScreen.right - rcScreen.left;
    int cyScreen = rcScreen.bottom - rcScreen.top;
    RECT rcStatus;  GetWindowRect(m_hwndStatusbar, &rcStatus);
    int cyStatus = rcStatus.bottom - rcStatus.top;

    int cyKeyboard = 0;
    if (Settings_GetKeyboard())
    {
        RECT rcKeyboard;  GetWindowRect(g_hwndKeyboard, &rcKeyboard);
        cyKeyboard = rcKeyboard.bottom - rcKeyboard.top;
    }
    int cyTape = 0;
    if (Settings_GetTape())
    {
        RECT rcTape;  GetWindowRect(g_hwndTape, &rcTape);
        cyTape = rcTape.bottom - rcTape.top;
    }

    // Adjust main window size
    int xLeft, yTop;
    int cxWidth, cyHeight;
    if (Settings_GetDebug())
    {
        RECT rcWorkArea;  SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWorkArea, 0);
        xLeft = rcWorkArea.left;
        yTop = rcWorkArea.top;
        cxWidth = rcWorkArea.right - rcWorkArea.left;
        if (cxWidth > MAX_DEBUG_WIDTH) cxWidth = MAX_DEBUG_WIDTH;
        cyHeight = rcWorkArea.bottom - rcWorkArea.top;
        if (cyHeight > MAX_DEBUG_HEIGHT) cyHeight = MAX_DEBUG_HEIGHT;
    }
    else
    {
        RECT rcCurrent;  ::GetWindowRect(g_hwnd, &rcCurrent);
        xLeft = rcCurrent.left;
        yTop = rcCurrent.top;
        cxWidth = cxScreen + cxFrame * 2;
        cyHeight = cyCaption + cyMenu + 4 + cyScreen + 4 + cyStatus + cyFrame * 2;
        if (Settings_GetToolbar())
            cyHeight += cyToolbar + 4;
        if (Settings_GetKeyboard())
            cyHeight += cyKeyboard + 4;
        if (Settings_GetTape())
            cyHeight += cyTape + 4;
    }

    SetWindowPos(g_hwnd, NULL, xLeft, yTop, cxWidth, cyHeight, SWP_NOZORDER | SWP_NOMOVE);
}

void MainWindow_AdjustWindowLayout()
{
    RECT rcStatus;  GetWindowRect(m_hwndStatusbar, &rcStatus);
    int cyStatus = rcStatus.bottom - rcStatus.top;

    int yScreen = 0;
    int cxScreen = 0, cyScreen = 0;

    int cyToolbar = 0;
    if (Settings_GetToolbar())
    {
        RECT rcToolbar;  GetWindowRect(m_hwndToolbar, &rcToolbar);
        cyToolbar = rcToolbar.bottom - rcToolbar.top;
        yScreen += cyToolbar + 4;
    }

    RECT rc;  GetClientRect(g_hwnd, &rc);

    if (!Settings_GetDebug())  // No debug views -- tape/keyboard snapped to bottom
    {
        cxScreen = rc.right;

        int yTape = rc.bottom - cyStatus + 4;
        int cyTape = 0;
        if (Settings_GetTape())  // Snapped to bottom
        {
            RECT rcTape;  GetWindowRect(g_hwndTape, &rcTape);
            cyTape = rcTape.bottom - rcTape.top;
            yTape = rc.bottom - cyStatus - cyTape - 4;
        }

        int yKeyboard = yTape;
        int cxKeyboard = 0, cyKeyboard = 0;
        if (Settings_GetKeyboard())  // Snapped to bottom
        {
            RECT rcKeyboard;  GetWindowRect(g_hwndKeyboard, &rcKeyboard);
            cxKeyboard = cxScreen;
            cyKeyboard = rcKeyboard.bottom - rcKeyboard.top;
            yKeyboard = yTape - cyKeyboard - 4;
        }

        cyScreen = yKeyboard - yScreen - (Settings_GetKeyboard() ? 0 : 4);
        if (cyScreen < BK_SCREEN_HEIGHT) cyScreen = BK_SCREEN_HEIGHT;

        //m_MainWindowMinCx = cxScreen + ::GetSystemMetrics(SM_CXSIZEFRAME) * 2;
        //m_MainWindowMinCy = ::GetSystemMetrics(SM_CYCAPTION) + ::GetSystemMetrics(SM_CYMENU) +
        //    cyScreen + cyStatus + ::GetSystemMetrics(SM_CYSIZEFRAME) * 2;

        if (Settings_GetKeyboard())
        {
            SetWindowPos(g_hwndKeyboard, NULL, 0, yKeyboard, cxScreen, cyKeyboard, SWP_NOZORDER);
        }
        if (Settings_GetTape())
        {
            SetWindowPos(g_hwndTape, NULL, 0, yTape, cxScreen, cyTape, SWP_NOZORDER);
        }
    }
    if (Settings_GetDebug())  // Debug views shown -- keyboard/tape snapped to top
    {
        cxScreen = 620;
        cyScreen = BK_SCREEN_HEIGHT + 8;

        int yKeyboard = yScreen + cyScreen + (Settings_GetKeyboard() ? 0 : 4);
        int yTape = yKeyboard;
        int yConsole = yTape;

        if (Settings_GetKeyboard())
        {
            RECT rcKeyboard;  GetWindowRect(g_hwndKeyboard, &rcKeyboard);
            int cxKeyboard = cxScreen;
            int cyKeyboard = rcKeyboard.bottom - rcKeyboard.top;
            int xKeyboard = (cxScreen - cxKeyboard) / 2;
            SetWindowPos(g_hwndKeyboard, NULL, xKeyboard, yKeyboard, cxKeyboard, cyKeyboard, SWP_NOZORDER);
            yTape += cyKeyboard + 4;
            yConsole += cyKeyboard + 4;
        }
        if (Settings_GetTape())
        {
            RECT rcTape;  GetWindowRect(g_hwndTape, &rcTape);
            int cyTape = rcTape.bottom - rcTape.top;
            SetWindowPos(g_hwndTape, NULL, 0, yTape, cxScreen, cyTape, SWP_NOZORDER);
            yConsole += cyTape + 4;
        }

        int cyConsole = rc.bottom - cyStatus - yConsole - 4;
        SetWindowPos(g_hwndConsole, NULL, 0, yConsole, cxScreen, cyConsole, SWP_NOZORDER);

        RECT rcDebug;  GetWindowRect(g_hwndDebug, &rcDebug);
        int cxDebug = rc.right - cxScreen - 4;
        int cyDebug = rcDebug.bottom - rcDebug.top;
        SetWindowPos(g_hwndDebug, NULL, cxScreen + 4, 0, cxDebug, cyDebug, SWP_NOZORDER);

        int yMemory = yConsole;
        int cxMemory = rc.right - cxScreen - 4;
        int cyMemory = rc.bottom - yMemory;
        SetWindowPos(g_hwndMemory, NULL, cxScreen + 4, yMemory, cxMemory, cyMemory, SWP_NOZORDER);

        int yDisasm = cyDebug + 4;
        int cyDisasm = yMemory - yDisasm - 4;
        int cxDisasm = cxDebug;

        if (Settings_GetMemoryMap())
        {
            RECT rcMemoryMap;  GetWindowRect(g_hwndMemoryMap, &rcMemoryMap);
            int cxMemoryMap = rcMemoryMap.right - rcMemoryMap.left;
            int cyMemoryMap = rcMemoryMap.bottom - rcMemoryMap.top;
            cxDisasm -= cxMemoryMap + 4;
            int xMemoryMap = rc.right - cxMemoryMap;
            SetWindowPos(g_hwndMemoryMap, NULL, xMemoryMap, yDisasm, cxMemoryMap, cyMemoryMap, SWP_NOZORDER);
        }

        SetWindowPos(g_hwndDisasm, NULL, cxScreen + 4, yDisasm, cxDisasm, cyDisasm, SWP_NOZORDER);

        SetWindowPos(m_hwndSplitter, NULL, cxScreen + 4, yMemory - 4, cxDebug, 4, SWP_NOZORDER);
    }

    SetWindowPos(m_hwndToolbar, NULL, 4, 4, cxScreen, cyToolbar, SWP_NOZORDER);

    SetWindowPos(g_hwndScreen, NULL, 0, yScreen, cxScreen, cyScreen, SWP_NOZORDER);

    int cyStatusReal = rcStatus.bottom - rcStatus.top;
    SetWindowPos(m_hwndStatusbar, NULL, 0, rc.bottom - cyStatusReal, cxScreen, cyStatusReal, SWP_NOZORDER | SWP_SHOWWINDOW);
}

void MainWindow_ShowHideDebug()
{
    if (!Settings_GetDebug())
    {
        // Delete debug views
        if (m_hwndSplitter != INVALID_HANDLE_VALUE)
            DestroyWindow(m_hwndSplitter);
        if (g_hwndConsole != INVALID_HANDLE_VALUE)
            DestroyWindow(g_hwndConsole);
        if (g_hwndDebug != INVALID_HANDLE_VALUE)
            DestroyWindow(g_hwndDebug);
        if (g_hwndDisasm != INVALID_HANDLE_VALUE)
            DestroyWindow(g_hwndDisasm);
        if (g_hwndMemory != INVALID_HANDLE_VALUE)
            DestroyWindow(g_hwndMemory);
        if (g_hwndMemoryMap != INVALID_HANDLE_VALUE)
            DestroyWindow(g_hwndMemoryMap);

        MainWindow_AdjustWindowSize();
    }
    else  // Debug Views ON
    {
        MainWindow_AdjustWindowSize();

        // Calculate children positions
        RECT rc;  GetClientRect(g_hwnd, &rc);
        RECT rcScreen;  GetWindowRect(g_hwndScreen, &rcScreen);
        RECT rcStatus;  GetWindowRect(m_hwndStatusbar, &rcStatus);
        int cyStatus = rcStatus.bottom - rcStatus.top;
        int yConsoleTop = rcScreen.bottom - rcScreen.top + 8;
        int cxConsoleWidth = rcScreen.right - rcScreen.left;
        int cyConsoleHeight = rc.bottom - cyStatus - yConsoleTop - 4;
        int xDebugLeft = (rcScreen.right - rcScreen.left) + 8;
        int cxDebugWidth = rc.right - xDebugLeft - 4;
        int cyDebugHeight = 216;
        int yDisasmTop = 4 + cyDebugHeight + 4;
        int cyDisasmHeight = 328;
        int yMemoryTop = cyDebugHeight + 4 + cyDisasmHeight + 8;
        int cyMemoryHeight = rc.bottom - cyStatus - yMemoryTop - 4;

        // Create debug views
        if (g_hwndConsole == INVALID_HANDLE_VALUE)
            ConsoleView_Create(g_hwnd, 4, yConsoleTop, cxConsoleWidth, cyConsoleHeight);
        if (g_hwndDebug == INVALID_HANDLE_VALUE)
            DebugView_Create(g_hwnd, xDebugLeft, 4, cxDebugWidth, cyDebugHeight);
        if (g_hwndDisasm == INVALID_HANDLE_VALUE)
            DisasmView_Create(g_hwnd, xDebugLeft, yDisasmTop, cxDebugWidth, cyDisasmHeight);
        if (g_hwndMemory == INVALID_HANDLE_VALUE)
            MemoryView_Create(g_hwnd, xDebugLeft, yMemoryTop, cxDebugWidth, cyMemoryHeight);
        if (g_hwndMemoryMap == INVALID_HANDLE_VALUE && Settings_GetMemoryMap())
            MemoryMapView_Create(g_hwnd, xDebugLeft, yMemoryTop);
        m_hwndSplitter = SplitterWindow_Create(g_hwnd, g_hwndDisasm, g_hwndMemory);
    }

    MainWindow_AdjustWindowLayout();

    MainWindow_UpdateMenu();

    SetFocus(g_hwndScreen);
}

void MainWindow_ShowHideToolbar()
{
    ShowWindow(m_hwndToolbar, Settings_GetToolbar() ? SW_SHOW : SW_HIDE);

    MainWindow_AdjustWindowSize();
    MainWindow_AdjustWindowLayout();
    MainWindow_UpdateMenu();
}

void MainWindow_ShowHideKeyboard()
{
    if (!Settings_GetKeyboard())
    {
        if (g_hwndKeyboard != INVALID_HANDLE_VALUE)
        {
            ::DestroyWindow(g_hwndKeyboard);
            g_hwndKeyboard = (HWND) INVALID_HANDLE_VALUE;
        }
    }
    else
    {
        // Calculate children positions
        RECT rc;  GetClientRect(g_hwnd, &rc);
        RECT rcScreen;  GetWindowRect(g_hwndScreen, &rcScreen);
        int yKeyboardTop = rcScreen.bottom - rcScreen.top + 8;
        int cxKeyboardWidth = 640;
        int cyKeyboardHeight = 230;

        if (g_hwndKeyboard == INVALID_HANDLE_VALUE)
            KeyboardView_Create(g_hwnd, 4, yKeyboardTop, cxKeyboardWidth, cyKeyboardHeight);
    }

    MainWindow_AdjustWindowSize();
    MainWindow_AdjustWindowLayout();
    MainWindow_UpdateMenu();
}

void MainWindow_ShowHideTape()
{
    if (!Settings_GetTape())
    {
        if (g_hwndTape != INVALID_HANDLE_VALUE)
        {
            ::DestroyWindow(g_hwndTape);
            g_hwndTape = (HWND) INVALID_HANDLE_VALUE;
        }
    }
    else
    {
        RECT rcPrev;
        if (Settings_GetKeyboard())
            GetWindowRect(g_hwndKeyboard, &rcPrev);
        else
            GetWindowRect(g_hwndScreen, &rcPrev);

        // Calculate children positions
        RECT rc;  GetClientRect(g_hwnd, &rc);
        int yTapeTop = rcPrev.bottom + 4;
        int cxTapeWidth = rcPrev.right - rcPrev.left;
        int cyTapeHeight = 64;

        if (g_hwndTape == INVALID_HANDLE_VALUE)
            TapeView_Create(g_hwnd, 4, yTapeTop, cxTapeWidth, cyTapeHeight);
    }

    MainWindow_AdjustWindowSize();
    MainWindow_AdjustWindowLayout();
    MainWindow_UpdateMenu();
}

void MainWindow_ShowHideMemoryMap()
{
    if (!Settings_GetMemoryMap())
    {
        if (g_hwndMemoryMap != INVALID_HANDLE_VALUE)
        {
            ::DestroyWindow(g_hwndMemoryMap);
            g_hwndMemoryMap = (HWND)INVALID_HANDLE_VALUE;
        }
    }
    else if (Settings_GetDebug())
    {
        if (g_hwndMemoryMap == INVALID_HANDLE_VALUE)
            MemoryMapView_Create(g_hwnd, 0, 0);
    }

    MainWindow_AdjustWindowLayout();
    MainWindow_UpdateMenu();
}

void MainWindow_ShowHideSpriteViewer()
{
    if (g_hwndSprite == INVALID_HANDLE_VALUE)
    {
        RECT rcScreen;  ::GetWindowRect(g_hwndScreen, &rcScreen);
        SpriteView_Create(rcScreen.right, rcScreen.top - 4 - ::GetSystemMetrics(SM_CYSMCAPTION));
    }
    else
    {
        ::SetFocus(g_hwndSprite);
    }
}

void MainWindow_ShowHideTeletype()
{
    if (g_hwndTeletype == INVALID_HANDLE_VALUE)
    {
        RECT rcScreen;  ::GetWindowRect(g_hwndScreen, &rcScreen);
        TeletypeView_Create(rcScreen.right, rcScreen.bottom, 460, 320);
    }
    else
    {
        ::SetFocus(g_hwndTeletype);
    }
}

void MainWindow_UpdateMenu()
{
    // Get main menu
    HMENU hMenu = GetMenu(g_hwnd);

    // Emulator|Run check
    CheckMenuItem(hMenu, ID_EMULATOR_RUN, (g_okEmulatorRunning ? MF_CHECKED : MF_UNCHECKED));
    SendMessage(m_hwndToolbar, TB_CHECKBUTTON, ID_EMULATOR_RUN, (g_okEmulatorRunning ? 1 : 0));
    //MainWindow_SetToolbarImage(ID_EMULATOR_RUN, g_okEmulatorRunning ? ToolbarImageRun : ToolbarImagePause);

    // View menu
    CheckMenuItem(hMenu, ID_VIEW_TOOLBAR, (Settings_GetToolbar() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_VIEW_MEMORYMAP, (Settings_GetMemoryMap() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_VIEW_KEYBOARD, (Settings_GetKeyboard() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_VIEW_TAPE, (Settings_GetTape() ? MF_CHECKED : MF_UNCHECKED));
    // View|Color Screen
    MainWindow_SetToolbarImage(ID_VIEW_RGBSCREEN,
            (ScreenView_GetScreenMode() & 1) ? ToolbarImageColorScreen : ToolbarImageBWScreen);
    // View|Screen Mode
    UINT scrmodecmd = 0;
    switch (ScreenView_GetScreenMode())
    {
    case 0: scrmodecmd = ID_VIEW_SCREENMODE0; break;
    case 1: scrmodecmd = ID_VIEW_SCREENMODE1; break;
    case 2: scrmodecmd = ID_VIEW_SCREENMODE2; break;
    case 3: scrmodecmd = ID_VIEW_SCREENMODE3; break;
    case 4: scrmodecmd = ID_VIEW_SCREENMODE4; break;
    case 5: scrmodecmd = ID_VIEW_SCREENMODE5; break;
    case 6: scrmodecmd = ID_VIEW_SCREENMODE6; break;
    case 7: scrmodecmd = ID_VIEW_SCREENMODE7; break;
    case 8: scrmodecmd = ID_VIEW_SCREENMODE8; break;
    case 9: scrmodecmd = ID_VIEW_SCREENMODE9; break;
    }
    CheckMenuRadioItem(hMenu, ID_VIEW_SCREENMODE0, ID_VIEW_SCREENMODE9, scrmodecmd, MF_BYCOMMAND);

    // Emulator menu options
    CheckMenuItem(hMenu, ID_EMULATOR_AUTOSTART, (Settings_GetAutostart() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_EMULATOR_SOUND, (Settings_GetSound() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_EMULATOR_COVOX, (Settings_GetSoundCovox() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_EMULATOR_SOUNDAY, (Settings_GetSoundAY() ? MF_CHECKED : MF_UNCHECKED));

    UINT speedcmd = 0;
    switch (Settings_GetRealSpeed())
    {
    case 0x7ffd: speedcmd = ID_EMULATOR_SPEED10;   break;
    case 0x7ffe: speedcmd = ID_EMULATOR_SPEED25;   break;
    case 0x7fff: speedcmd = ID_EMULATOR_SPEED50;   break;
    case 0:      speedcmd = ID_EMULATOR_SPEEDMAX;  break;
    case 1:      speedcmd = ID_EMULATOR_REALSPEED; break;
    case 2:      speedcmd = ID_EMULATOR_SPEED200;  break;
    case 3:      speedcmd = ID_EMULATOR_SPEED400;  break;
    }
    CheckMenuRadioItem(hMenu, ID_EMULATOR_SPEED10, ID_EMULATOR_SPEED400, speedcmd, MF_BYCOMMAND);

    UINT joystickcmd = 0;
    switch (Settings_GetJoystick())
    {
    case 0: joystickcmd = ID_EMULATOR_JOYSTICKNUMPAD; break;
    case 1: joystickcmd = ID_EMULATOR_JOYSTICK1; break;
    case 2: joystickcmd = ID_EMULATOR_JOYSTICK2; break;
    }
    CheckMenuRadioItem(hMenu, ID_EMULATOR_JOYSTICKNUMPAD, ID_EMULATOR_JOYSTICK2, joystickcmd, MF_BYCOMMAND);

    MainWindow_SetToolbarImage(ID_EMULATOR_SOUND, (Settings_GetSound() ? ToolbarImageSoundOn : ToolbarImageSoundOff));
    EnableMenuItem(hMenu, ID_DEBUG_STEPINTO, (g_okEmulatorRunning ? MF_DISABLED : MF_ENABLED));

    UINT configcmd = 0;
    switch (g_nEmulatorConfiguration)
    {
    case BK_CONF_BK0010_BASIC: configcmd = ID_CONF_BK0010BASIC; break;
    case BK_CONF_BK0010_FOCAL: configcmd = ID_CONF_BK0010FOCAL; break;
    case BK_CONF_BK0010_FDD:   configcmd = ID_CONF_BK0010FDD; break;
    case BK_CONF_BK0011:       configcmd = ID_CONF_BK0011; break;
    case BK_CONF_BK0011_FDD:   configcmd = ID_CONF_BK0011FDD; break;
    }
    CheckMenuRadioItem(hMenu, ID_CONF_BK0010BASIC, ID_CONF_BK0011FDD, configcmd, MF_BYCOMMAND);

    // Emulator|FloppyX
    CheckMenuItem(hMenu, ID_EMULATOR_FLOPPY0, (g_pBoard->IsFloppyImageAttached(0) ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_EMULATOR_FLOPPY1, (g_pBoard->IsFloppyImageAttached(1) ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_EMULATOR_FLOPPY2, (g_pBoard->IsFloppyImageAttached(2) ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_EMULATOR_FLOPPY3, (g_pBoard->IsFloppyImageAttached(3) ? MF_CHECKED : MF_UNCHECKED));
    MainWindow_SetToolbarImage(ID_EMULATOR_FLOPPY0,
            g_pBoard->IsFloppyImageAttached(0) ? (g_pBoard->IsFloppyReadOnly(0) ? ToolbarImageFloppyDiskWP : ToolbarImageFloppyDisk) : ToolbarImageFloppySlot);
    MainWindow_SetToolbarImage(ID_EMULATOR_FLOPPY1,
            g_pBoard->IsFloppyImageAttached(1) ? (g_pBoard->IsFloppyReadOnly(1) ? ToolbarImageFloppyDiskWP : ToolbarImageFloppyDisk) : ToolbarImageFloppySlot);
    MainWindow_SetToolbarImage(ID_EMULATOR_FLOPPY2,
            g_pBoard->IsFloppyImageAttached(2) ? (g_pBoard->IsFloppyReadOnly(2) ? ToolbarImageFloppyDiskWP : ToolbarImageFloppyDisk) : ToolbarImageFloppySlot);
    MainWindow_SetToolbarImage(ID_EMULATOR_FLOPPY3,
            g_pBoard->IsFloppyImageAttached(3) ? (g_pBoard->IsFloppyReadOnly(3) ? ToolbarImageFloppyDiskWP : ToolbarImageFloppyDisk) : ToolbarImageFloppySlot);

    // Debug menu
    BOOL okDebug = Settings_GetDebug();
    CheckMenuItem(hMenu, ID_VIEW_DEBUG, (okDebug ? MF_CHECKED : MF_UNCHECKED));
    EnableMenuItem(hMenu, ID_VIEW_MEMORYMAP, (okDebug ? MF_ENABLED : MF_DISABLED));
    EnableMenuItem(hMenu, ID_DEBUG_SPRITES, (okDebug ? MF_ENABLED : MF_DISABLED));
    EnableMenuItem(hMenu, ID_DEBUG_SUBTITLES, (okDebug ? MF_ENABLED : MF_DISABLED));
    EnableMenuItem(hMenu, ID_DEBUG_STEPINTO, (okDebug ? MF_ENABLED : MF_DISABLED));
    EnableMenuItem(hMenu, ID_DEBUG_STEPOVER, (okDebug ? MF_ENABLED : MF_DISABLED));
    EnableMenuItem(hMenu, ID_DEBUG_TELETYPE, (okDebug ? MF_ENABLED : MF_DISABLED));
    EnableMenuItem(hMenu, ID_DEBUG_CLEARCONSOLE, (okDebug ? MF_ENABLED : MF_DISABLED));
    EnableMenuItem(hMenu, ID_DEBUG_DELETEALLBREAKPTS, (okDebug ? MF_ENABLED : MF_DISABLED));
}

// Process menu command
// Returns: true - command was processed, false - command not found
bool MainWindow_DoCommand(int commandId)
{
    switch (commandId)
    {
    case ID_FILE_LOADSTATE:
        MainWindow_DoFileLoadState();
        break;
    case ID_FILE_SAVESTATE:
        MainWindow_DoFileSaveState();
        break;
    case ID_FILE_SCREENSHOT:
        MainWindow_DoFileScreenshot();
        break;
    case ID_FILE_SCREENSHOTTOCLIPBOARD:
        MainWindow_DoFileScreenshotToClipboard();
        break;
    case ID_FILE_SAVESCREENSHOTAS:
        MainWindow_DoFileScreenshotSaveAs();
        break;
    case ID_FILE_LOADBIN:
        MainWindow_DoFileLoadBin();
        break;
    case ID_FILE_SETTINGS:
        MainWindow_DoFileSettings();
        break;
    case ID_FILE_SETTINGS_COLORS:
        MainWindow_DoFileSettingsColors();
        break;
    case IDM_EXIT:
        DestroyWindow(g_hwnd);
        break;
    case ID_VIEW_TOOLBAR:
        MainWindow_DoViewToolbar();
        break;
    case ID_VIEW_KEYBOARD:
        MainWindow_DoViewKeyboard();
        break;
    case ID_VIEW_MEMORYMAP:
        MainWindow_DoDebugMemoryMap();
        break;
    case ID_DEBUG_TELETYPE:
        MainWindow_DoDebugTeletype();
        break;
    case ID_VIEW_TAPE:
        MainWindow_DoViewTape();
        break;
    case ID_VIEW_RGBSCREEN:
        MainWindow_DoViewScreenColor();
        break;
    case ID_VIEW_SCREENMODE0:
        MainWindow_DoViewScreenMode(0);
        break;
    case ID_VIEW_SCREENMODE1:
        MainWindow_DoViewScreenMode(1);
        break;
    case ID_VIEW_SCREENMODE2:
        MainWindow_DoViewScreenMode(2);
        break;
    case ID_VIEW_SCREENMODE3:
        MainWindow_DoViewScreenMode(3);
        break;
    case ID_VIEW_SCREENMODE4:
        MainWindow_DoViewScreenMode(4);
        break;
    case ID_VIEW_SCREENMODE5:
        MainWindow_DoViewScreenMode(5);
        break;
    case ID_VIEW_SCREENMODE6:
        MainWindow_DoViewScreenMode(6);
        break;
    case ID_VIEW_SCREENMODE7:
        MainWindow_DoViewScreenMode(7);
        break;
    case ID_VIEW_SCREENMODE8:
        MainWindow_DoViewScreenMode(8);
        break;
    case ID_VIEW_SCREENMODE9:
        MainWindow_DoViewScreenMode(9);
        break;
    case ID_EMULATOR_RUN:
        MainWindow_DoEmulatorRun();
        break;
    case ID_EMULATOR_RESET:
        MainWindow_DoEmulatorReset();
        break;
    case ID_EMULATOR_AUTOSTART:
        MainWindow_DoEmulatorAutostart();
        break;
    case ID_EMULATOR_SOUND:
        MainWindow_DoEmulatorSound();
        break;
    case ID_EMULATOR_COVOX:
        MainWindow_DoEmulatorCovox();
        break;
    case ID_EMULATOR_SOUNDAY:
        MainWindow_DoEmulatorSoundAY();
        break;
    case ID_EMULATOR_SPEED10:
        MainWindow_DoEmulatorSpeed(0x7ffd);
        break;
    case ID_EMULATOR_SPEED25:
        MainWindow_DoEmulatorSpeed(0x7ffe);
        break;
    case ID_EMULATOR_SPEED50:
        MainWindow_DoEmulatorSpeed(0x7fff);
        break;
    case ID_EMULATOR_SPEEDMAX:
        MainWindow_DoEmulatorSpeed(0);
        break;
    case ID_EMULATOR_REALSPEED:
        MainWindow_DoEmulatorSpeed(1);
        break;
    case ID_EMULATOR_SPEED200:
        MainWindow_DoEmulatorSpeed(2);
        break;
    case ID_EMULATOR_SPEED400:
        MainWindow_DoEmulatorSpeed(3);
        break;
    case ID_EMULATOR_JOYSTICKNUMPAD:
        MainWindow_DoEmulatorJoystick(0);
        break;
    case ID_EMULATOR_JOYSTICK1:
        MainWindow_DoEmulatorJoystick(1);
        break;
    case ID_EMULATOR_JOYSTICK2:
        MainWindow_DoEmulatorJoystick(2);
        break;
    case ID_EMULATOR_FLOPPY0:
        MainWindow_DoEmulatorFloppy(0);
        break;
    case ID_EMULATOR_FLOPPY1:
        MainWindow_DoEmulatorFloppy(1);
        break;
    case ID_EMULATOR_FLOPPY2:
        MainWindow_DoEmulatorFloppy(2);
        break;
    case ID_EMULATOR_FLOPPY3:
        MainWindow_DoEmulatorFloppy(3);
        break;
    case ID_CONF_BK0010BASIC:
        MainWindow_DoEmulatorConf(BK_CONF_BK0010_BASIC);
        break;
    case ID_CONF_BK0010FOCAL:
        MainWindow_DoEmulatorConf(BK_CONF_BK0010_FOCAL);
        break;
    case ID_CONF_BK0010FDD:
        MainWindow_DoEmulatorConf(BK_CONF_BK0010_FDD);
        break;
    case ID_CONF_BK0011:
        MainWindow_DoEmulatorConf(BK_CONF_BK0011);
        break;
    case ID_CONF_BK0011FDD:
        MainWindow_DoEmulatorConf(BK_CONF_BK0011_FDD);
        break;
    case ID_VIEW_DEBUG:
        MainWindow_DoViewDebug();
        break;
    case ID_DEBUG_SPRITES:
        MainWindow_DoViewSpriteViewer();
        break;
    case ID_DEBUG_STEPINTO:
        if (!g_okEmulatorRunning && Settings_GetDebug())
            ConsoleView_StepInto();
        break;
    case ID_DEBUG_STEPOVER:
        if (!g_okEmulatorRunning && Settings_GetDebug())
            ConsoleView_StepOver();
        break;
    case ID_DEBUG_CLEARCONSOLE:
        if (Settings_GetDebug())
            ConsoleView_ClearConsole();
        break;
    case ID_DEBUG_DELETEALLBREAKPTS:
        if (Settings_GetDebug())
            ConsoleView_DeleteAllBreakpoints();
        break;
    case ID_DEBUG_SUBTITLES:
        DisasmView_LoadUnloadSubtitles();
        break;
    case ID_DEBUG_MEMORY_WORDBYTE:
        MemoryView_SwitchWordByte();
        break;
    case ID_DEBUG_MEMORY_GOTO:
        MemoryView_SelectAddress();
        break;
    case IDM_ABOUT:
        ShowAboutBox();
        break;
    case ID_HELP_COMMAND_LINE_HELP:
        ShowCommandLineHelpBox();
        break;
    default:
        return false;
    }
    return true;
}

void MainWindow_DoViewDebug()
{
    int mode = ScreenView_GetScreenMode();
    MainWindow_DoViewScreenMode(mode & 1);  // Switch to Normal Height mode

    Settings_SetDebug(!Settings_GetDebug());
    MainWindow_ShowHideDebug();
}
void MainWindow_DoDebugMemoryMap()
{
    Settings_SetMemoryMap(!Settings_GetMemoryMap());
    MainWindow_ShowHideMemoryMap();
}
void MainWindow_DoDebugTeletype()
{
    MainWindow_ShowHideTeletype();
}
void MainWindow_DoViewToolbar()
{
    Settings_SetToolbar(!Settings_GetToolbar());
    MainWindow_ShowHideToolbar();
}
void MainWindow_DoViewKeyboard()
{
    Settings_SetKeyboard(!Settings_GetKeyboard());
    MainWindow_ShowHideKeyboard();
}
void MainWindow_DoViewTape()
{
    Settings_SetTape(!Settings_GetTape());
    MainWindow_ShowHideTape();
}
void MainWindow_DoViewSpriteViewer()
{
    MainWindow_ShowHideSpriteViewer();
}

void MainWindow_DoViewScreenColor()
{
    int mode = ScreenView_GetScreenMode();
    int newmode = mode ^ 1;

    ScreenView_SetScreenMode(newmode);
    MainWindow_UpdateMenu();

    Settings_SetScreenViewMode(newmode);
}

void MainWindow_DoViewScreenMode(int newMode)
{
    if (Settings_GetDebug() && newMode > 1) return;  // Deny switching to Double Height in Debug mode

    ScreenView_SetScreenMode(newMode);

    MainWindow_AdjustWindowSize();
    MainWindow_AdjustWindowLayout();
    MainWindow_UpdateMenu();

    Settings_SetScreenViewMode(newMode);
}

void MainWindow_DoEmulatorRun()
{
    if (g_okEmulatorRunning)
    {
        Emulator_Stop();
    }
    else
    {
        Emulator_Start();
    }
}
void MainWindow_DoEmulatorAutostart()
{
    Settings_SetAutostart(!Settings_GetAutostart());

    MainWindow_UpdateMenu();
}
void MainWindow_DoEmulatorReset()
{
    Emulator_Reset();
}
void MainWindow_DoEmulatorSpeed(WORD speed)
{
    Settings_SetRealSpeed(speed);
    Emulator_SetSpeed(speed);

    MainWindow_UpdateMenu();
}

void MainWindow_DoEmulatorSound()
{
    Settings_SetSound(!Settings_GetSound());

    Emulator_SetSound(Settings_GetSound() != 0);

    MainWindow_UpdateMenu();
}
void MainWindow_DoEmulatorCovox()
{
    Settings_SetSoundCovox(!Settings_GetSoundCovox());

    Emulator_SetCovox(Settings_GetSoundCovox() != 0);

    MainWindow_UpdateMenu();
}
void MainWindow_DoEmulatorSoundAY()
{
    Settings_SetSoundAY(!Settings_GetSoundAY());

    Emulator_SetSoundAY(Settings_GetSoundAY() != 0);

    MainWindow_UpdateMenu();
}
void MainWindow_DoEmulatorJoystick(int joystick)
{
    if (!Joystick_SelectJoystick(joystick))
    {
        AlertWarning(_T("Failed to select the joystick."));
    }

    Settings_SetJoystick(joystick);

    MainWindow_UpdateMenu();
}

void MainWindow_DoFileLoadState()
{
    TCHAR bufFileName[MAX_PATH];
    BOOL okResult = ShowOpenDialog(g_hwnd,
            _T("Open state image to load"),
            _T("BK state images (*.bkst)\0*.bkst\0All Files (*.*)\0*.*\0\0"),
            bufFileName);
    if (! okResult) return;

    Emulator_LoadImage(bufFileName);
}

void MainWindow_DoFileSaveState()
{
    TCHAR bufFileName[MAX_PATH];
    BOOL okResult = ShowSaveDialog(g_hwnd,
            _T("Save state image as"),
            _T("BK state images (*.bkst)\0*.bkst\0All Files (*.*)\0*.*\0\0"),
            _T("bkst"),
            bufFileName);
    if (! okResult) return;

    if (!Emulator_SaveImage(bufFileName))
    {
        AlertWarning(_T("Failed to save image file."));
    }
}

void MainWindow_DoFileScreenshot()
{
    TCHAR bufFileName[MAX_PATH];
    SYSTEMTIME st;
    ::GetSystemTime(&st);
    _sntprintf(bufFileName, sizeof(bufFileName) / sizeof(TCHAR) - 1,
            _T("%04d%02d%02d%02d%02d%02d%03d.png"),
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    if (!ScreenView_SaveScreenshot(bufFileName))
    {
        AlertWarning(_T("Failed to save screenshot bitmap."));
    }
}

void MainWindow_DoFileScreenshotToClipboard()
{
    HGLOBAL hDIB = ScreenView_GetScreenshotAsDIB();
    if (hDIB != NULL)
    {
        ::OpenClipboard(g_hwnd);
        ::EmptyClipboard();
        ::SetClipboardData(CF_DIB, hDIB);
        ::CloseClipboard();
    }
}

void MainWindow_DoFileScreenshotSaveAs()
{
    TCHAR bufFileName[MAX_PATH];
    BOOL okResult = ShowSaveDialog(g_hwnd,
            _T("Save screenshot as"),
            _T("PNG bitmaps (*.png)\0*.png\0BMP bitmaps (*.bmp)\0*.bmp\0TIFF bitmaps (*.tiff)\0*.tiff\0All Files (*.*)\0*.*\0\0"),
            _T("png"),
            bufFileName);
    if (! okResult) return;

    if (!ScreenView_SaveScreenshot(bufFileName))
    {
        AlertWarning(_T("Failed to save screenshot bitmap."));
    }
}

//TODO: Remove, fake tape replaced it
void MainWindow_DoFileLoadBin()
{
    ShowLoadBinDialog();
}

void MainWindow_DoFileSettings()
{
    ShowSettingsDialog();
}

void MainWindow_DoFileSettingsColors()
{
    if (ShowSettingsColorsDialog())
    {
        RedrawWindow(g_hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
}

void MainWindow_DoEmulatorConf(BKConfiguration configuration)
{
    // Check if configuration changed
    if (g_nEmulatorConfiguration == configuration)
        return;

    // Ask user -- we have to reset machine to change configuration
    if (!AlertOkCancel(_T("Reset required after configuration change.\nAre you agree?")))
        return;

    // Change configuration
    Emulator_InitConfiguration(configuration);

    Settings_SetConfiguration(configuration);

    MainWindow_UpdateMenu();
    MainWindow_UpdateWindowTitle();
    MainWindow_UpdateAllViews();
    KeyboardView_Update();
}

void MainWindow_DoEmulatorFloppy(int slot)
{
    BOOL okImageAttached = g_pBoard->IsFloppyImageAttached(slot);
    if (okImageAttached)
    {
        g_pBoard->DetachFloppyImage(slot);
        Settings_SetFloppyFilePath(slot, NULL);
    }
    else
    {
        if ((g_nEmulatorConfiguration & BK_COPT_FDD) == 0)
        {
            AlertWarning(_T("Current configuration has no floppy controller."));
            return;
        }

        // File Open dialog
        TCHAR bufFileName[MAX_PATH];
        BOOL okResult = ShowOpenDialog(g_hwnd,
                _T("Open floppy image to attach"),
                _T("BK floppy images (*.img, *.bkd)\0*.img;*.bkd\0All Files (*.*)\0*.*\0\0"),
                bufFileName);
        if (! okResult) return;

        if (! g_pBoard->AttachFloppyImage(slot, bufFileName))
        {
            AlertWarning(_T("Failed to attach floppy image."));
            return;
        }

        Settings_SetFloppyFilePath(slot, bufFileName);
    }
    MainWindow_UpdateMenu();
}

void MainWindow_OnToolbarGetInfoTip(LPNMTBGETINFOTIP lpnm)
{
    int commandId = lpnm->iItem;

    if (commandId == ID_EMULATOR_FLOPPY0 || commandId == ID_EMULATOR_FLOPPY1 ||
        commandId == ID_EMULATOR_FLOPPY2 || commandId == ID_EMULATOR_FLOPPY3)
    {
        int floppyslot = 0;
        switch (commandId)
        {
        case ID_EMULATOR_FLOPPY0: floppyslot = 0; break;
        case ID_EMULATOR_FLOPPY1: floppyslot = 1; break;
        case ID_EMULATOR_FLOPPY2: floppyslot = 2; break;
        case ID_EMULATOR_FLOPPY3: floppyslot = 3; break;
        }

        if (g_pBoard->IsFloppyImageAttached(floppyslot))
        {
            TCHAR buffilepath[MAX_PATH];
            Settings_GetFloppyFilePath(floppyslot, buffilepath);

            LPCTSTR lpFileName = GetFileNameFromFilePath(buffilepath);
            _tcsncpy_s(lpnm->pszText, 80, lpFileName, _TRUNCATE);
        }
    }
}

void MainWindow_UpdateAllViews()
{
    // Update cached values in views
    Emulator_OnUpdate();
    DebugView_OnUpdate();
    DisasmView_OnUpdate();

    // Update screen
    InvalidateRect(g_hwndScreen, NULL, TRUE);

    // Update debug windows
    if (g_hwndDebug != NULL)
        InvalidateRect(g_hwndDebug, NULL, TRUE);
    if (g_hwndDisasm != NULL)
        InvalidateRect(g_hwndDisasm, NULL, TRUE);
    if (g_hwndMemory != NULL)
        InvalidateRect(g_hwndMemory, NULL, TRUE);
    if (g_hwndMemoryMap != NULL)
        InvalidateRect(g_hwndMemoryMap, NULL, TRUE);
    if (g_hwndSprite != NULL)
        InvalidateRect(g_hwndSprite, NULL, TRUE);
}

void MainWindow_SetToolbarImage(int commandId, int imageIndex)
{
    TBBUTTONINFO info;
    info.cbSize = sizeof(info);
    info.iImage = imageIndex;
    info.dwMask = TBIF_IMAGE;
    SendMessage(m_hwndToolbar, TB_SETBUTTONINFO, commandId, (LPARAM) &info);
}

void MainWindow_SetStatusbarText(int part, LPCTSTR message)
{
    SendMessage(m_hwndStatusbar, SB_SETTEXT, part, (LPARAM) message);
}
void MainWindow_SetStatusbarBitmap(int part, UINT resourceId)
{
    SendMessage(m_hwndStatusbar, SB_SETTEXT, part | SBT_OWNERDRAW, (LPARAM) resourceId);
}
void MainWindow_SetStatusbarIcon(int part, HICON hIcon)
{
    SendMessage(m_hwndStatusbar, SB_SETICON, part, (LPARAM) hIcon);
}


//////////////////////////////////////////////////////////////////////
