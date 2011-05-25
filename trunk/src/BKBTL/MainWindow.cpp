/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// MainWindow.cpp
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
#include "ToolWindow.h"
#include "Joystick.h"


//////////////////////////////////////////////////////////////////////


TCHAR g_szTitle[MAX_LOADSTRING];            // The title bar text
TCHAR g_szWindowClass[MAX_LOADSTRING];      // Main window class name

HWND m_hwndToolbar = NULL;
HWND m_hwndStatusbar = NULL;


//////////////////////////////////////////////////////////////////////
// Forward declarations

LRESULT CALLBACK MainWindow_WndProc(HWND, UINT, WPARAM, LPARAM);
void MainWindow_AdjustWindowLayout();
bool MainWindow_DoCommand(int commandId);
void MainWindow_DoViewDebug();
void MainWindow_DoDebugMemoryMap();
void MainWindow_DoDebugTeletype();
void MainWindow_DoViewToolbar();
void MainWindow_DoViewKeyboard();
void MainWindow_DoViewTape();
void MainWindow_DoViewColorScreen();
void MainWindow_DoViewHeightMode(int newMode);
void MainWindow_DoEmulatorRun();
void MainWindow_DoEmulatorAutostart();
void MainWindow_DoEmulatorReset();
void MainWindow_DoEmulatorRealSpeed();
void MainWindow_DoEmulatorSound();
void MainWindow_DoEmulatorCovox();
void MainWindow_DoEmulatorJoystick(int joystickNum);
void MainWindow_DoFileSaveState();
void MainWindow_DoFileLoadState();
void MainWindow_DoEmulatorFloppy(int slot);
void MainWindow_DoEmulatorConf(BKConfiguration configuration);
void MainWindow_DoFileScreenshot();
void MainWindow_DoFileLoadBin();
void MainWindow_OnStatusbarClick(LPNMMOUSE lpnm);
void MainWindow_OnStatusbarDrawItem(LPDRAWITEMSTRUCT);
void MainWindow_OnToolbarGetInfoTip(LPNMTBGETINFOTIP);


//////////////////////////////////////////////////////////////////////


void MainWindow_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= MainWindow_WndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= g_hInst;
    wcex.hIcon			= LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_BKBTL));
    wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground	= (HBRUSH)(COLOR_BTNFACE+1);
    wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_BKBTL);
    wcex.lpszClassName	= g_szWindowClass;
    wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    RegisterClassEx(&wcex);

	ToolWindow_RegisterClass();
    OverlappedWindow_RegisterClass();

    // Register view classes
    ScreenView_RegisterClass();
    KeyboardView_RegisterClass();
    MemoryView_RegisterClass();
    DebugView_RegisterClass();
    MemoryMapView_RegisterClass();
    DisasmView_RegisterClass();
    ConsoleView_RegisterClass();
	TapeView_RegisterClass();
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
    buttons[3].idCommand = ID_VIEW_RGBSCREEN;
    buttons[3].iBitmap = ToolbarImageColorScreen;
    buttons[3].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[3].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("Color"));
    buttons[4].fsStyle = BTNS_SEP;
    buttons[5].idCommand = ID_EMULATOR_SOUND;
    buttons[5].iBitmap = 8;
    buttons[5].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[5].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("Sound"));
    buttons[6].fsStyle = BTNS_SEP;
    buttons[7].idCommand = ID_EMULATOR_FLOPPY0;
    buttons[7].iBitmap = ToolbarImageFloppySlot;
    buttons[7].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[7].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("A"));
    buttons[8].idCommand = ID_EMULATOR_FLOPPY1;
    buttons[8].iBitmap = ToolbarImageFloppySlot;
    buttons[8].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[8].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("B"));
    buttons[9].idCommand = ID_EMULATOR_FLOPPY2;
    buttons[9].iBitmap = ToolbarImageFloppySlot;
    buttons[9].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[9].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("C"));
    buttons[10].idCommand = ID_EMULATOR_FLOPPY3;
    buttons[10].iBitmap = ToolbarImageFloppySlot;
    buttons[10].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[10].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("D"));

    SendMessage(m_hwndToolbar, TB_ADDBUTTONS, (WPARAM) sizeof(buttons) / sizeof(TBBUTTON), (LPARAM) &buttons); 

    if (Settings_GetToolbar())
        ShowWindow(m_hwndToolbar, SW_SHOW);

    return TRUE;
}

BOOL MainWindow_InitStatusbar()
{
    m_hwndStatusbar = CreateStatusWindow(WS_CHILD | WS_VISIBLE | SBT_TOOLTIPS,
            _T(""),
            g_hwnd, 101);
    if (! m_hwndStatusbar)
        return FALSE;
    int statusbarParts[2];
    statusbarParts[0] = 350;
    statusbarParts[1] = -1;
    SendMessage(m_hwndStatusbar, SB_SETPARTS, sizeof(statusbarParts)/sizeof(int), (LPARAM) statusbarParts);

    return TRUE;
}

void MainWindow_RestoreSettings()
{
    TCHAR buf[MAX_PATH];

    // Reattach floppy images
    for (int slot = 0; slot < 4; slot++)
    {
        buf[0] = '\0';
        Settings_GetFloppyFilePath(slot, buf);
        if (lstrlen(buf) > 0)
        {
            if (! g_pBoard->AttachFloppyImage(slot, buf))
                Settings_SetFloppyFilePath(slot, NULL);
        }
    }

    // Restore ScreenViewMode
    int mode = Settings_GetScreenViewMode();
    if (mode <= 0 || mode > 2) mode = ColorScreen;
    ScreenView_SetMode((ScreenViewMode) mode);

    // Restore ScreenHeightMode
    int heimode = Settings_GetScreenHeightMode();
    if (heimode < 1) heimode = 1;
    if (heimode > 2) heimode = 2;
    ScreenView_SetHeightMode(heimode);
}

// Processes messages for the main window
LRESULT CALLBACK MainWindow_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_ACTIVATE:
        if (!Settings_GetDebug())
            SetFocus(g_hwndScreen);
        break;
    case WM_COMMAND:
        {
            int wmId    = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);
            bool okProcessed = MainWindow_DoCommand(wmId);
            if (!okProcessed)
                return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_NOTIFY:
        {
            int idCtrl = (int) wParam;
            HWND hwndFrom = ((LPNMHDR) lParam)->hwndFrom;
            UINT code = ((LPNMHDR) lParam)->code;
            if (hwndFrom == m_hwndStatusbar && code == NM_CLICK)
            {
                MainWindow_OnStatusbarClick((LPNMMOUSE) lParam);
            }
            else if (code == TTN_SHOW)
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
    case WM_DRAWITEM:
        {
            int idCtrl = (int) wParam;
            HWND hwndItem = ((LPDRAWITEMSTRUCT) lParam)->hwndItem;
            if (hwndItem == m_hwndStatusbar)
                MainWindow_OnStatusbarDrawItem((LPDRAWITEMSTRUCT) lParam);
            else
                return DefWindowProc(hWnd, message, wParam, lParam);
        }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void MainWindow_AdjustWindowSize()
{
    // Get metrics
    RECT rcWorkArea;  SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWorkArea, 0);
    int cxBorder  = ::GetSystemMetrics(SM_CXBORDER);
    int cyBorder  = ::GetSystemMetrics(SM_CYBORDER);
    int cxFrame   = ::GetSystemMetrics(SM_CXDLGFRAME);
    int cyFrame   = ::GetSystemMetrics(SM_CYDLGFRAME);
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
	int cyTape = 0;

	if (Settings_GetKeyboard())
    {
        RECT rcKeyboard;  GetWindowRect(g_hwndKeyboard, &rcKeyboard);
        cyKeyboard = rcKeyboard.bottom - rcKeyboard.top;
    }
	if (Settings_GetTape())
	{
		RECT rcTape;  GetWindowRect(g_hwndTape, &rcTape);
        cyTape = rcTape.bottom - rcTape.top;
	}

    // Adjust main window size
    int xLeft = rcWorkArea.left;
    int yTop = rcWorkArea.top;
    int cxWidth, cyHeight;
    if (Settings_GetDebug())
    {
        cxWidth = rcWorkArea.right - rcWorkArea.left;
        cyHeight = rcWorkArea.bottom - rcWorkArea.top;
    }
    else
    {
        cxWidth = cxScreen + cxFrame * 2 + 8;
        cyHeight = cyCaption + cyMenu + cyScreen + 4 + cyStatus + cyFrame * 2;
        if (Settings_GetToolbar())
            cyHeight += cyToolbar + 4;
        if (Settings_GetKeyboard())
            cyHeight += cyKeyboard;
		if (Settings_GetTape())
			cyHeight += cyTape + 4;
    }
 
    SetWindowPos(g_hwnd, NULL, xLeft, yTop, cxWidth, cyHeight, SWP_NOZORDER);
}

void MainWindow_AdjustWindowLayout()
{
    RECT rcStatus;  GetWindowRect(m_hwndStatusbar, &rcStatus);
    int cyStatus = rcStatus.bottom - rcStatus.top;

    RECT rcScreen;  GetWindowRect(g_hwndScreen, &rcScreen);
    int cxScreen = rcScreen.right - rcScreen.left;
    int cyScreen = rcScreen.bottom - rcScreen.top;

    RECT rcToolbar;  GetWindowRect(m_hwndToolbar, &rcToolbar);
    int cyToolbar = rcToolbar.bottom - rcToolbar.top;

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
    int yScreen = 0;
    int yKeyboard = yScreen + cyScreen;
    int yTape = yScreen + cyScreen + 4;
    int yConsole = yScreen + cyScreen + 4;

    RECT rc;  GetClientRect(g_hwnd, &rc);

    SetWindowPos(m_hwndStatusbar, NULL, 0, rc.bottom - cyStatus, cxScreen, cyStatus, SWP_NOZORDER);

    SetWindowPos(m_hwndToolbar, NULL, 4, 4, cxScreen, cyToolbar, SWP_NOZORDER);

    if (Settings_GetToolbar())
    {
        yScreen   += cyToolbar + 4;
        yKeyboard += cyToolbar + 4;
        yTape     += cyToolbar + 4;
        yConsole  += cyToolbar + 4;
    }

    SetWindowPos(g_hwndScreen, NULL, 4, yScreen, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOCOPYBITS);

    if (Settings_GetKeyboard())
    {
        SetWindowPos(g_hwndKeyboard, NULL, 4, yKeyboard, 0,0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOCOPYBITS);
        yTape     += cyKeyboard;
        yConsole  += cyKeyboard;
    }
    if (Settings_GetTape())
    {
        SetWindowPos(g_hwndTape, NULL, 4, yTape, 0,0, SWP_NOZORDER | SWP_NOSIZE);
        yConsole  += cyTape + 4;
    }
    if (Settings_GetDebug())
    {
        int cyConsole = rc.bottom - cyStatus - yConsole - 4;
        SetWindowPos(g_hwndConsole, NULL, 4, yConsole, cxScreen, cyConsole, SWP_NOZORDER);
    }
}

void MainWindow_ShowHideDebug()
{
    if (!Settings_GetDebug())
    {
        // Delete debug views
        if (g_hwndConsole != INVALID_HANDLE_VALUE)
            DestroyWindow(g_hwndConsole);
        if (g_hwndDebug != INVALID_HANDLE_VALUE)
            DestroyWindow(g_hwndDebug);
        if (g_hwndDisasm != INVALID_HANDLE_VALUE)
            DestroyWindow(g_hwndDisasm);
        if (g_hwndMemory != INVALID_HANDLE_VALUE)
            DestroyWindow(g_hwndMemory);

        MainWindow_AdjustWindowSize();
        MainWindow_AdjustWindowLayout();

        SetFocus(g_hwndScreen);
    }
    else
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
        int cyDebugHeight = 190;
		int yDisasmTop = 4 + cyDebugHeight + 4;
		int cyDisasmHeight = 380;
        int yMemoryTop = cyDebugHeight + 4 + cyDisasmHeight + 8;
        int cyMemoryHeight = rc.bottom - cyStatus - yMemoryTop - 4;

        // Create debug views
        if (g_hwndConsole == INVALID_HANDLE_VALUE)
            CreateConsoleView(g_hwnd, 4, yConsoleTop, cxConsoleWidth, cyConsoleHeight);
        if (g_hwndDebug == INVALID_HANDLE_VALUE)
            CreateDebugView(g_hwnd, xDebugLeft, 4, cxDebugWidth, cyDebugHeight);
        if (g_hwndDisasm == INVALID_HANDLE_VALUE)
            CreateDisasmView(g_hwnd, xDebugLeft, yDisasmTop, cxDebugWidth, cyDisasmHeight);
        if (g_hwndMemory == INVALID_HANDLE_VALUE)
            CreateMemoryView(g_hwnd, xDebugLeft, yMemoryTop, cxDebugWidth, cyMemoryHeight);

        MainWindow_AdjustWindowLayout();
    }

    MainWindow_UpdateMenu();
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
        int cxKeyboardWidth = rcScreen.right - rcScreen.left;
        int cyKeyboardHeight = 230;

        if (g_hwndKeyboard == INVALID_HANDLE_VALUE)
            CreateKeyboardView(g_hwnd, 4, yKeyboardTop, cxKeyboardWidth, cyKeyboardHeight);
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
            CreateTapeView(g_hwnd, 4, yTapeTop, cxTapeWidth, cyTapeHeight);
    }

    MainWindow_AdjustWindowSize();
    MainWindow_AdjustWindowLayout();
    MainWindow_UpdateMenu();
}

void MainWindow_ShowHideMemoryMap()
{
    if (g_hwndMemoryMap == INVALID_HANDLE_VALUE)
    {
        RECT rcScreen;  ::GetWindowRect(g_hwndScreen, &rcScreen);
        CreateMemoryMapView(rcScreen.right, rcScreen.top);
    }
    else
    {
        ::SetFocus(g_hwndMemoryMap);
    }
}

void MainWindow_ShowHideTeletype()
{
    if (g_hwndTeletype == INVALID_HANDLE_VALUE)
    {
        RECT rcScreen;  ::GetWindowRect(g_hwndScreen, &rcScreen);
        CreateTeletypeView(rcScreen.right, rcScreen.bottom, 460, 320);
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
    MainWindow_SetToolbarImage(ID_EMULATOR_RUN, g_okEmulatorRunning ? ToolbarImageRun : ToolbarImagePause);
    // View|Debug check
    CheckMenuItem(hMenu, ID_VIEW_TOOLBAR, (Settings_GetToolbar() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_VIEW_DEBUG, (Settings_GetDebug() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_VIEW_KEYBOARD, (Settings_GetKeyboard() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_VIEW_TAPE, (Settings_GetTape() ? MF_CHECKED : MF_UNCHECKED));
    // View|Color Screen
    CheckMenuItem(hMenu, ID_VIEW_RGBSCREEN, ((ScreenView_GetMode() == ColorScreen) ? MF_CHECKED : MF_UNCHECKED));
    MainWindow_SetToolbarImage(ID_VIEW_RGBSCREEN,
        (ScreenView_GetMode() == ColorScreen) ? ToolbarImageColorScreen : ToolbarImageBWScreen);
    // View|Normal Height and View|Double Height radio
    UINT scrheimodecmd = 0;
    switch (ScreenView_GetHeightMode())
    {
    case 1: scrheimodecmd = ID_VIEW_NORMALHEIGHT; break;
    case 2: scrheimodecmd = ID_VIEW_DOUBLEHEIGHT; break;
    }
    CheckMenuRadioItem(hMenu, ID_VIEW_NORMALHEIGHT, ID_VIEW_DOUBLEHEIGHT, scrheimodecmd, MF_BYCOMMAND);

    // Emulator menu options
    CheckMenuItem(hMenu, ID_EMULATOR_AUTOSTART, (Settings_GetAutostart() ? MF_CHECKED : MF_UNCHECKED));
    //CheckMenuItem(hMenu, ID_EMULATOR_REALSPEED, (Settings_GetRealSpeed() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_EMULATOR_SOUND, (Settings_GetSound() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_EMULATOR_COVOX, (Settings_GetCovox() ? MF_CHECKED : MF_UNCHECKED));

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
    MainWindow_SetStatusbarBitmap(StatusbarPartMZ0,
        g_pBoard->IsFloppyImageAttached(0) ? ((g_pBoard->IsFloppyReadOnly(0)) ? IDI_DISKETTEWP : IDI_DISKETTE) : 0);
    MainWindow_SetStatusbarBitmap(StatusbarPartMZ1,
        g_pBoard->IsFloppyImageAttached(1) ? ((g_pBoard->IsFloppyReadOnly(1)) ? IDI_DISKETTEWP : IDI_DISKETTE) : 0);
    MainWindow_SetStatusbarBitmap(StatusbarPartMZ2,
        g_pBoard->IsFloppyImageAttached(2) ? ((g_pBoard->IsFloppyReadOnly(2)) ? IDI_DISKETTEWP : IDI_DISKETTE) : 0);
    MainWindow_SetStatusbarBitmap(StatusbarPartMZ3,
        g_pBoard->IsFloppyImageAttached(3) ? ((g_pBoard->IsFloppyReadOnly(3)) ? IDI_DISKETTEWP : IDI_DISKETTE) : 0);
    MainWindow_SetToolbarImage(ID_EMULATOR_FLOPPY0,
        g_pBoard->IsFloppyImageAttached(0) ? (g_pBoard->IsFloppyReadOnly(0) ? ToolbarImageFloppyDiskWP : ToolbarImageFloppyDisk) : ToolbarImageFloppySlot);
    MainWindow_SetToolbarImage(ID_EMULATOR_FLOPPY1,
        g_pBoard->IsFloppyImageAttached(1) ? (g_pBoard->IsFloppyReadOnly(1) ? ToolbarImageFloppyDiskWP : ToolbarImageFloppyDisk) : ToolbarImageFloppySlot);
    MainWindow_SetToolbarImage(ID_EMULATOR_FLOPPY2,
        g_pBoard->IsFloppyImageAttached(2) ? (g_pBoard->IsFloppyReadOnly(2) ? ToolbarImageFloppyDiskWP : ToolbarImageFloppyDisk) : ToolbarImageFloppySlot);
    MainWindow_SetToolbarImage(ID_EMULATOR_FLOPPY3,
        g_pBoard->IsFloppyImageAttached(3) ? (g_pBoard->IsFloppyReadOnly(3) ? ToolbarImageFloppyDiskWP : ToolbarImageFloppyDisk) : ToolbarImageFloppySlot);
}

// Process menu command
// Returns: true - command was processed, false - command not found
bool MainWindow_DoCommand(int commandId)
{
    switch (commandId)
    {
    case IDM_ABOUT:
        ShowAboutBox();
        break;
    case IDM_EXIT:
        DestroyWindow(g_hwnd);
        break;
    case ID_VIEW_DEBUG:
        MainWindow_DoViewDebug();
        break;
    case ID_VIEW_MEMORYMAP:
        MainWindow_DoDebugMemoryMap();
        break;
    case ID_DEBUG_TELETYPE:
        MainWindow_DoDebugTeletype();
        break;
    case ID_VIEW_TOOLBAR:
        MainWindow_DoViewToolbar();
        break;
    case ID_VIEW_KEYBOARD:
        MainWindow_DoViewKeyboard();
        break;
    case ID_VIEW_TAPE:
        MainWindow_DoViewTape();
        break;
    case ID_VIEW_RGBSCREEN:
        MainWindow_DoViewColorScreen();
        break;
    case ID_VIEW_NORMALHEIGHT:
        MainWindow_DoViewHeightMode(1);
        break;
    case ID_VIEW_DOUBLEHEIGHT:
        MainWindow_DoViewHeightMode(2);
        break;
    case ID_EMULATOR_RUN:
        MainWindow_DoEmulatorRun();
        break;
    case ID_EMULATOR_AUTOSTART:
        MainWindow_DoEmulatorAutostart();
        break;
    case ID_DEBUG_STEPINTO:
        if (!g_okEmulatorRunning && Settings_GetDebug())
            ConsoleView_StepInto();
        break;
    case ID_DEBUG_STEPOVER:
        if (!g_okEmulatorRunning && Settings_GetDebug())
            ConsoleView_StepOver();
        break;
    case ID_EMULATOR_RESET:
        MainWindow_DoEmulatorReset();
        break;
    case ID_EMULATOR_REALSPEED:
        MainWindow_DoEmulatorRealSpeed();
        break;
    case ID_EMULATOR_SOUND:
        MainWindow_DoEmulatorSound();
        break;
    case ID_EMULATOR_COVOX:
        MainWindow_DoEmulatorCovox();
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
    case ID_FILE_LOADSTATE:
        MainWindow_DoFileLoadState();
        break;
    case ID_FILE_SAVESTATE:
        MainWindow_DoFileSaveState();
        break;
    case ID_FILE_SCREENSHOT:
        MainWindow_DoFileScreenshot();
        break;
    case ID_FILE_LOADBIN:
        MainWindow_DoFileLoadBin();
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
    default:
        return false;
    }
    return true;
}

void MainWindow_DoViewDebug()
{
    //MainWindow_DoViewHeightMode(1);  // Switch to Normal Height mode

    Settings_SetDebug(!Settings_GetDebug());
    MainWindow_ShowHideDebug();
}
void MainWindow_DoDebugMemoryMap()
{
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

void MainWindow_DoViewColorScreen()
{
    ScreenViewMode newMode = (ScreenView_GetMode() == ColorScreen) ? BlackWhiteScreen : ColorScreen;
    ScreenView_SetMode(newMode);

    Settings_SetScreenViewMode(newMode);
    MainWindow_UpdateMenu();
    InvalidateRect(g_hwndScreen, NULL, TRUE);  // Update screen
}

void MainWindow_DoViewHeightMode(int newMode)
{
    //if (Settings_GetDebug() && newMode == 2) return;  // Deny switching to Double Height in Debug mode

    ScreenView_SetHeightMode(newMode);

    MainWindow_AdjustWindowSize();
    MainWindow_AdjustWindowLayout();
    MainWindow_UpdateMenu();

    Settings_SetScreenHeightMode(newMode);
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
void MainWindow_DoEmulatorRealSpeed()
{
    Settings_SetRealSpeed(!Settings_GetRealSpeed());

    MainWindow_UpdateMenu();
}
void MainWindow_DoEmulatorSound()
{
    Settings_SetSound(!Settings_GetSound());

    Emulator_SetSound(Settings_GetSound());

    MainWindow_UpdateMenu();
}
void MainWindow_DoEmulatorCovox()
{
    Settings_SetCovox(!Settings_GetCovox());

    Emulator_SetCovox(Settings_GetCovox());

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
    // File Open dialog
    TCHAR bufFileName[MAX_PATH];
    *bufFileName = 0;
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.hInstance = g_hInst;
    ofn.lpstrTitle = _T("Open state image to load");
    ofn.lpstrFilter = _T("BK state images (*.bkst)\0*.bkst\0All Files (*.*)\0*.*\0\0");
    ofn.Flags = OFN_FILEMUSTEXIST;
    ofn.lpstrFile = bufFileName;
    ofn.nMaxFile = sizeof(bufFileName) / sizeof(TCHAR);

    BOOL okResult = GetOpenFileName(&ofn);
    if (! okResult) return;

    Emulator_LoadImage(bufFileName);
}

void MainWindow_DoFileSaveState()
{
    TCHAR bufFileName[MAX_PATH];
    BOOL okResult = ShowSaveDialog(g_hwnd,
        _T("Save state image as"),
        _T("BK state images (*.bkst)\0*.bkst\0All Files (*.*)\0*.*\0\0"),
        bufFileName);
    if (! okResult) return;

    Emulator_SaveImage(bufFileName);
}

void MainWindow_DoFileScreenshot()
{
    TCHAR bufFileName[MAX_PATH];
    BOOL okResult = ShowSaveDialog(g_hwnd,
        _T("Save screenshot as"),
        _T("Bitmaps (*.bmp)\0*.bmp\0All Files (*.*)\0*.*\0\0"),
        bufFileName);
    if (! okResult) return;

    ScreenView_SaveScreenshot(bufFileName);
}

void MainWindow_DoFileLoadBin()
{
    ShowLoadBinDialog();
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
    MainWindow_UpdateAllViews();
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
        *bufFileName = 0;
        OPENFILENAME ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = g_hwnd;
        ofn.hInstance = g_hInst;
        ofn.lpstrTitle = _T("Open floppy image to attach");
        ofn.lpstrFilter = _T("BK floppy images (*.img, *.bkd)\0*.img;*.bkd\0All Files (*.*)\0*.*\0\0");
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        ofn.lpstrFile = bufFileName;
        ofn.nMaxFile = sizeof(bufFileName) / sizeof(TCHAR);

        BOOL okResult = GetOpenFileName(&ofn);
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

void MainWindow_OnStatusbarClick(LPNMMOUSE lpnm)
{
    int nSection = (int) (lpnm->dwItemSpec);
    if (nSection >= StatusbarPartMZ0 && nSection <= StatusbarPartMZ3)
    {
        UINT nCmd = 0;
        switch (nSection)
        {
        case StatusbarPartMZ0: nCmd = ID_EMULATOR_FLOPPY0; break;
        case StatusbarPartMZ1: nCmd = ID_EMULATOR_FLOPPY1; break;
        case StatusbarPartMZ2: nCmd = ID_EMULATOR_FLOPPY2; break;
        case StatusbarPartMZ3: nCmd = ID_EMULATOR_FLOPPY3; break;
        }
        ::PostMessage(g_hwnd, WM_COMMAND, (WPARAM) nCmd, 0);
    }
}

void MainWindow_OnStatusbarDrawItem(LPDRAWITEMSTRUCT lpDrawItem)
{
    HDC hdc = lpDrawItem->hDC;

    // Номер привода дисковода
    TCHAR text[2];
    text[0] = _T('0') + lpDrawItem->itemID - StatusbarPartMZ0;
    text[1] = 0;
    ::DrawStatusText(hdc, &lpDrawItem->rcItem, text, SBT_NOBORDERS);

    // Иконка диска
    UINT resourceId = (UINT) lpDrawItem->itemData;
    if (resourceId != 0)
    {
        HICON hicon = ::LoadIcon(g_hInst, MAKEINTRESOURCE(resourceId));
        int left = lpDrawItem->rcItem.right - 16 - 2; // lpDrawItem->rcItem.left + (lpDrawItem->rcItem.right - lpDrawItem->rcItem.left - 16) / 2;
        int top = lpDrawItem->rcItem.top + (lpDrawItem->rcItem.bottom - lpDrawItem->rcItem.top - 16) / 2;
        ::DrawIconEx(hdc, left, top, hicon, 16, 16, 0, NULL, DI_NORMAL);
        ::DeleteObject(hicon);
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
