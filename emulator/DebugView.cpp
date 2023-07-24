/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// DebugView.cpp

#include "stdafx.h"
#include <windowsx.h>
#include <CommCtrl.h>
#include "Main.h"
#include "Views.h"
#include "ToolWindow.h"
#include "Emulator.h"
#include "emubase/Emubase.h"

//////////////////////////////////////////////////////////////////////


HWND g_hwndDebug = (HWND)INVALID_HANDLE_VALUE;  // Debug View window handle
WNDPROC m_wndprocDebugToolWindow = NULL;  // Old window proc address of the ToolWindow

HWND m_hwndDebugViewer = (HWND)INVALID_HANDLE_VALUE;
HWND m_hwndDebugToolbar = (HWND)INVALID_HANDLE_VALUE;

WORD m_wDebugCpuR[9];  // Old register values - R0..R7, PSW
BOOL m_okDebugCpuRChanged[9];  // Register change flags
WORD m_wDebugCpuPswOld;  // PSW value on previous step
WORD m_wDebugCpuR6Old;  // SP value on previous step

void DebugView_OnRButtonDown(int mousex, int mousey);
void DebugView_DoDraw(HDC hdc);
BOOL DebugView_OnKeyDown(WPARAM vkey, LPARAM lParam);
void DebugView_DrawProcessor(HDC hdc, const CProcessor* pProc, int x, int y, WORD* arrR, BOOL* arrRChanged, WORD oldPsw);
void DebugView_DrawMemoryForRegister(HDC hdc, int reg, const CProcessor* pProc, int x, int y, WORD oldValue);
int DebugView_DrawWatchpoints(HDC hdc, int x, int y);
void DebugView_DrawPorts(HDC hdc, int x, int y);
void DebugView_DrawBreakpoints(HDC hdc, int x, int y);
void DebugView_DrawMemoryMap(HDC hdc, int x, int y, const CProcessor* pProc);
void DebugView_UpdateWindowText();


//////////////////////////////////////////////////////////////////////


void DebugView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = DebugViewViewerWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = g_hInst;
    wcex.hIcon          = NULL;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = CLASSNAME_DEBUGVIEW;
    wcex.hIconSm        = NULL;

    RegisterClassEx(&wcex);
}

void DebugView_Init()
{
    memset(m_wDebugCpuR, 255, sizeof(m_wDebugCpuR));
    memset(m_okDebugCpuRChanged, 1, sizeof(m_okDebugCpuRChanged));
    m_wDebugCpuPswOld = 0;
    m_wDebugCpuR6Old = 0;
}

void DebugView_Create(HWND hwndParent, int x, int y, int width, int height)
{
    ASSERT(hwndParent != NULL);

    g_hwndDebug = CreateWindow(
            CLASSNAME_TOOLWINDOW, NULL,
            WS_CHILD | WS_VISIBLE,
            x, y, width, height,
            hwndParent, NULL, g_hInst, NULL);
    DebugView_UpdateWindowText();

    // ToolWindow subclassing
    m_wndprocDebugToolWindow = (WNDPROC)LongToPtr( SetWindowLongPtr(
            g_hwndDebug, GWLP_WNDPROC, PtrToLong(DebugViewWndProc)) );

    RECT rcClient;  GetClientRect(g_hwndDebug, &rcClient);

    m_hwndDebugViewer = CreateWindowEx(
            WS_EX_STATICEDGE,
            CLASSNAME_DEBUGVIEW, NULL,
            WS_CHILD | WS_VISIBLE,
            0, 0, rcClient.right, rcClient.bottom,
            g_hwndDebug, NULL, g_hInst, NULL);

    m_hwndDebugToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL,
            WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TRANSPARENT | TBSTYLE_TOOLTIPS | CCS_NOPARENTALIGN | CCS_NODIVIDER | CCS_VERT,
            4, 4, 32, rcClient.bottom, m_hwndDebugViewer,
            (HMENU)102,
            g_hInst, NULL);

    TBADDBITMAP addbitmap;
    addbitmap.hInst = g_hInst;
    addbitmap.nID = IDB_TOOLBAR;
    SendMessage(m_hwndDebugToolbar, TB_ADDBITMAP, 2, (LPARAM)&addbitmap);

    SendMessage(m_hwndDebugToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM) sizeof(TBBUTTON), 0);
    SendMessage(m_hwndDebugToolbar, TB_SETBUTTONSIZE, 0, (LPARAM) MAKELONG (26, 26));

    TBBUTTON buttons[5];
    ZeroMemory(buttons, sizeof(buttons));
    for (int i = 0; i < sizeof(buttons) / sizeof(TBBUTTON); i++)
    {
        buttons[i].fsState = TBSTATE_ENABLED | TBSTATE_WRAP;
        buttons[i].fsStyle = BTNS_BUTTON;
        buttons[i].iString = -1;
    }
    buttons[0].idCommand = ID_VIEW_DEBUG;
    buttons[0].iBitmap = ToolbarImageDebugger;
    buttons[0].fsState = TBSTATE_ENABLED | TBSTATE_WRAP | TBSTATE_CHECKED;
    buttons[1].idCommand = ID_DEBUG_SPRITES;
    buttons[1].iBitmap = ToolbarImageSpriteViewer;
    buttons[2].fsStyle = BTNS_SEP;
    buttons[3].idCommand = ID_DEBUG_STEPINTO;
    buttons[3].iBitmap = ToolbarImageStepInto;
    buttons[4].idCommand = ID_DEBUG_STEPOVER;
    buttons[4].iBitmap = ToolbarImageStepOver;

    SendMessage(m_hwndDebugToolbar, TB_ADDBUTTONS, (WPARAM) sizeof(buttons) / sizeof(TBBUTTON), (LPARAM)&buttons);
}

void DebugView_Redraw()
{
    RedrawWindow(g_hwndDebug, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

// Adjust position of client windows
void DebugView_AdjustWindowLayout()
{
    RECT rc;  GetClientRect(g_hwndDebug, &rc);

    if (m_hwndDebugViewer != (HWND)INVALID_HANDLE_VALUE)
        SetWindowPos(m_hwndDebugViewer, NULL, 0, 0, rc.right, rc.bottom, SWP_NOZORDER);
}

LRESULT CALLBACK DebugViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    LRESULT lResult;
    switch (message)
    {
    case WM_DESTROY:
        g_hwndDebug = (HWND)INVALID_HANDLE_VALUE;  // We are closed! Bye-bye!..
        return CallWindowProc(m_wndprocDebugToolWindow, hWnd, message, wParam, lParam);
    case WM_SIZE:
        lResult = CallWindowProc(m_wndprocDebugToolWindow, hWnd, message, wParam, lParam);
        DebugView_AdjustWindowLayout();
        return lResult;
    default:
        return CallWindowProc(m_wndprocDebugToolWindow, hWnd, message, wParam, lParam);
    }
    //return (LRESULT)FALSE;
}

LRESULT CALLBACK DebugViewViewerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_COMMAND:
        ::PostMessage(g_hwnd, WM_COMMAND, wParam, lParam);
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            DebugView_DoDraw(hdc);

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_LBUTTONDOWN:
        ::SetFocus(hWnd);
        break;
    case WM_RBUTTONDOWN:
        DebugView_OnRButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        break;
    case WM_KEYDOWN:
        return (LRESULT) DebugView_OnKeyDown(wParam, lParam);
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        ::InvalidateRect(hWnd, NULL, TRUE);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

void DebugView_OnRButtonDown(int mousex, int mousey)
{
    ::SetFocus(m_hwndDebugViewer);

    HMENU hMenu = ::CreatePopupMenu();
    ::AppendMenu(hMenu, 0, ID_DEBUG_DELETEALLBREAKPTS, _T("Delete All Breakpoints"));

    POINT pt = { mousex, mousey };
    ::ClientToScreen(m_hwndDebugViewer, &pt);
    ::TrackPopupMenu(hMenu, 0, pt.x, pt.y, 0, m_hwndDebugViewer, NULL);

    VERIFY(::DestroyMenu(hMenu));
}

BOOL DebugView_OnKeyDown(WPARAM vkey, LPARAM /*lParam*/)
{
    switch (vkey)
    {
    case VK_ESCAPE:
        ConsoleView_Activate();
        break;
    default:
        return TRUE;
    }
    return FALSE;
}

void DebugView_UpdateWindowText()
{
    ::SetWindowText(g_hwndDebug, _T("Debug"));
}


//////////////////////////////////////////////////////////////////////

// Update after Run or Step
void DebugView_OnUpdate()
{
    CProcessor* pCPU = g_pBoard->GetCPU();
    ASSERT(pCPU != nullptr);

    // Get new register values and set change flags
    m_wDebugCpuR6Old = m_wDebugCpuR[6];
    for (int r = 0; r < 8; r++)
    {
        WORD value = pCPU->GetReg(r);
        m_okDebugCpuRChanged[r] = (m_wDebugCpuR[r] != value);
        m_wDebugCpuR[r] = value;
    }
    WORD pswCPU = pCPU->GetPSW();
    m_okDebugCpuRChanged[8] = (m_wDebugCpuR[8] != pswCPU);
    m_wDebugCpuPswOld = m_wDebugCpuR[8];
    m_wDebugCpuR[8] = pswCPU;
}


//////////////////////////////////////////////////////////////////////
// Draw functions

void DebugView_DoDraw(HDC hdc)
{
    ASSERT(g_pBoard != nullptr);

    // Create and select font
    HFONT hFont = CreateMonospacedFont();
    HGDIOBJ hOldFont = SelectObject(hdc, hFont);
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);
    int cyHeight = cyLine * 17;
    COLORREF colorOld = SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
    COLORREF colorBkOld = SetBkColor(hdc, GetSysColor(COLOR_WINDOW));

    CProcessor* pDebugPU = g_pBoard->GetCPU();
    ASSERT(pDebugPU != nullptr);
    WORD* arrR = m_wDebugCpuR;
    BOOL* arrRChanged = m_okDebugCpuRChanged;
    WORD oldPsw = m_wDebugCpuPswOld;
    WORD oldSP = m_wDebugCpuR6Old;

    HGDIOBJ hOldBrush = ::SelectObject(hdc, ::GetSysColorBrush(COLOR_BTNFACE));
    int x = 32;
    ::PatBlt(hdc, x, 0, 4, cyHeight, PATCOPY);
    x += 4;
    int xProc = x;
    x += cxChar * 33;
    ::PatBlt(hdc, x, 0, 4, cyHeight, PATCOPY);
    x += 4;
    int xStack = x;
    x += cxChar * 17 + cxChar / 2;
    ::PatBlt(hdc, x, 0, 4, cyHeight, PATCOPY);
    x += 4;
    int xPorts = x;
    x += cxChar * 25;
    ::PatBlt(hdc, x, 0, 4, cyHeight, PATCOPY);
    x += 4;
    int xBreaks = x;
    x += cxChar * 9;
    ::PatBlt(hdc, x, 0, 4, cyHeight, PATCOPY);
    x += 4;
    int xMemmap = x;
    ::SelectObject(hdc, hOldBrush);

    DebugView_DrawProcessor(hdc, pDebugPU, xProc + cxChar, cyLine / 2, arrR, arrRChanged, oldPsw);

    // Draw stack for the current processor
    DebugView_DrawMemoryForRegister(hdc, 6, pDebugPU, xStack + cxChar / 2, cyLine / 2, oldSP);

    int nWatches = DebugView_DrawWatchpoints(hdc, xPorts + cxChar, cyLine / 2);
    DebugView_DrawPorts(hdc, xPorts + cxChar, cyLine / 2 + (nWatches > 0 ? 2 + nWatches : 0) * cyLine);

    DebugView_DrawBreakpoints(hdc, xBreaks + cxChar / 2, cyLine / 2);

    DebugView_DrawMemoryMap(hdc, xMemmap + cxChar, 0, pDebugPU);

    SetTextColor(hdc, colorOld);
    SetBkColor(hdc, colorBkOld);
    SelectObject(hdc, hOldFont);
    VERIFY(::DeleteObject(hFont));

    if (::GetFocus() == m_hwndDebugViewer)
    {
        RECT rcClient;
        GetClientRect(m_hwndDebugViewer, &rcClient);
        DrawFocusRect(hdc, &rcClient);
    }
}

void DebugView_DrawProcessor(HDC hdc, const CProcessor* pProc, int x, int y, WORD* arrR, BOOL* arrRChanged, WORD oldPsw)
{
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);
    COLORREF colorText = Settings_GetColor(ColorDebugText);
    COLORREF colorChanged = Settings_GetColor(ColorDebugValueChanged);
    ::SetTextColor(hdc, colorText);

    // Registers
    for (int r = 0; r < 8; r++)
    {
        ::SetTextColor(hdc, arrRChanged[r] ? colorChanged : colorText);

        LPCTSTR strRegName = REGISTER_NAME[r];
        TextOut(hdc, x, y + r * cyLine, strRegName, (int) _tcslen(strRegName));

        WORD value = arrR[r]; //pProc->GetReg(r);
        DrawOctalValue(hdc, x + cxChar * 3, y + r * cyLine, value);
        DrawHexValue(hdc, x + cxChar * 10, y + r * cyLine, value);
        DrawBinaryValue(hdc, x + cxChar * 15, y + r * cyLine, value);
    }
    ::SetTextColor(hdc, colorText);

    // PSW value
    ::SetTextColor(hdc, arrRChanged[8] ? colorChanged : colorText);
    TextOut(hdc, x, y + 10 * cyLine, _T("PS"), 2);
    WORD psw = arrR[8]; // pProc->GetPSW();
    DrawOctalValue(hdc, x + cxChar * 3, y + 10 * cyLine, psw);
    //DrawHexValue(hdc, x + cxChar * 10, y + 10 * cyLine, psw);
    ::SetTextColor(hdc, colorText);
    TextOut(hdc, x + cxChar * 15, y + 9 * cyLine, _T("       HP  TNZVC"), 16);

    // PSW value bits colored bit-by-bit
    TCHAR buffera[2];  buffera[1] = 0;
    for (int i = 0; i < 16; i++)
    {
        WORD bitpos = 1 << i;
        buffera[0] = (psw & bitpos) ? '1' : '0';
        ::SetTextColor(hdc, ((psw & bitpos) != (oldPsw & bitpos)) ? colorChanged : colorText);
        TextOut(hdc, x + cxChar * (15 + 15 - i), y + 10 * cyLine, buffera, 1);
    }

    ::SetTextColor(hdc, colorText);

    // Processor mode - HALT or USER
    BOOL okHaltMode = pProc->IsHaltMode();
    TextOut(hdc, x, y + 12 * cyLine, okHaltMode ? _T("HALT") : _T("USER"), 4);

    // "Stopped" flag
    BOOL okStopped = pProc->IsStopped();
    if (okStopped)
        TextOut(hdc, x + 6 * cxChar, y + 12 * cyLine, _T("STOP"), 4);
}

void DebugView_DrawAddressAndValue(HDC hdc, uint16_t address, int x, int y, int cxChar)
{
    ASSERT(g_pBoard != nullptr);

    COLORREF colorText = Settings_GetColor(ColorDebugText);
    SetTextColor(hdc, colorText);
    DrawOctalValue(hdc, x, y, address);
    x += 7 * cxChar;

    bool okHaltMode = g_pBoard->GetCPU()->IsHaltMode();
    int addrtype = ADDRTYPE_DENY;
    uint16_t value = g_pBoard->GetWordView(address, okHaltMode, FALSE, &addrtype);
    if (addrtype == ADDRTYPE_RAM)
    {
        uint16_t wChanged = Emulator_GetChangeRamStatus(address);
        if (wChanged != 0) SetTextColor(hdc, Settings_GetColor(ColorDebugValueChanged));
        DrawOctalValue(hdc, x, y, value);
    }
    else if (addrtype == ADDRTYPE_ROM)
    {
        SetTextColor(hdc, Settings_GetColor(ColorDebugMemoryRom));
        DrawOctalValue(hdc, x, y, value);
    }
    else if (addrtype == ADDRTYPE_IO)
    {
        value = g_pBoard->GetPortView(address);
        SetTextColor(hdc, Settings_GetColor(ColorDebugMemoryIO));
        DrawOctalValue(hdc, x, y, value);
    }
    else //if (addrtype == ADDRTYPE_DENY)
    {
        SetTextColor(hdc, Settings_GetColor(ColorDebugMemoryNA));
        TextOut(hdc, x, y, _T("  NA  "), 6);
    }

    SetTextColor(hdc, colorText);
}

void DebugView_DrawMemoryForRegister(HDC hdc, int reg, const CProcessor* pProc, int x, int y, WORD oldValue)
{
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);
    COLORREF colorText = Settings_GetColor(ColorDebugText);
    COLORREF colorChanged = Settings_GetColor(ColorDebugValueChanged);
    COLORREF colorPrev = Settings_GetColor(ColorDebugPrevious);
    COLORREF colorOld = SetTextColor(hdc, colorText);

    uint16_t current = pProc->GetReg(reg) & ~1;
    uint16_t previous = oldValue;
    bool okExec = (reg == 7);

    // Reading from memory into the buffer
    uint16_t memory[16];
    int addrtype[16];
    for (int idx = 0; idx < 16; idx++)
    {
        memory[idx] = g_pBoard->GetWordView(
                (uint16_t)(current + idx * 2 - 16), pProc->IsHaltMode(), okExec, addrtype + idx);
    }

    WORD address = current - 16;
    for (int index = 0; index < 16; index++)
    {
        DebugView_DrawAddressAndValue(hdc, address, x + 3 * cxChar, y, cxChar);

        if (address == current)  // Current position
        {
            SetTextColor(hdc, colorText);
            TextOut(hdc, x + 2 * cxChar, y, _T(">"), 1);
            if (current != previous) SetTextColor(hdc, colorChanged);
            TextOut(hdc, x, y, REGISTER_NAME[reg], 2);
        }
        else if (address == previous)
        {
            SetTextColor(hdc, colorPrev);
            TextOut(hdc, x + 2 * cxChar, y, _T(">"), 1);
        }

        address += 2;
        y += cyLine;
    }

    SetTextColor(hdc, colorOld);
}

int DebugView_DrawWatchpoints(HDC hdc, int x, int y)
{
    const uint16_t* pws = Emulator_GetWatchpointList();
    if (*pws == 0177777)
        return 0;

    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);

    int nWatches = 0;
    TextOut(hdc, x, y, _T("Watches"), 7);
    y += cyLine;
    while (*pws != 0177777)
    {
        uint16_t address = *pws;
        DebugView_DrawAddressAndValue(hdc, address, x, y, cxChar);
        y += cyLine;
        pws++;  nWatches++;
    }

    return nWatches;
}

struct DebugViewPortWatch
{
    uint16_t address;
    LPCTSTR description;
}
m_DebugViewPorts[] =
{
    { 0177660, _T("kbd state") },
    { 0177662, _T("kbd data") },
    { 0177664, _T("scroll") },
    { 0177706, _T("timer rel") },
    { 0177710, _T("timer val") },
    { 0177712, _T("timer ctl") },
    { 0177714, _T("parallel") },
    { 0177716, _T("system") },
    { 0177130, _T("fdd state") },
    { 0177132, _T("fdd data") },
};

void DebugView_DrawPorts(HDC hdc, int x, int y)
{
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);

    TextOut(hdc, x, y, _T("Ports"), 5);

    int portsCount = sizeof(m_DebugViewPorts) / sizeof(m_DebugViewPorts[0]);
    for (int i = 0; i < portsCount; i++)
    {
        y += cyLine;
        const DebugViewPortWatch& watch = m_DebugViewPorts[i];
        DebugView_DrawAddressAndValue(hdc, watch.address, x, y, cxChar);
        TextOut(hdc, x + 14 * cxChar, y, watch.description, _tcslen(watch.description));
    }
}

void DebugView_DrawBreakpoints(HDC hdc, int x, int y)
{
    TextOut(hdc, x, y, _T("Breakpts"), 8);

    const uint16_t* pbps = Emulator_GetCPUBreakpointList();
    if (*pbps == 0177777)
        return;

    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);

    x += cxChar;
    y += cyLine;
    while (*pbps != 0177777)
    {
        DrawOctalValue(hdc, x, y, *pbps);
        y += cyLine;
        pbps++;
    }
}

void DebugView_DrawMemoryMap(HDC hdc, int x, int y, const CProcessor* pProc)
{
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);

    int x1 = x + cxChar * 7;
    int y1 = y + cxChar / 2;
    int x2 = x1 + cxChar * 14;
    int y2 = y1 + cyLine * 16;
    //int xtype = x1 + cxChar * 3;

    HGDIOBJ hOldBrush = ::SelectObject(hdc, ::GetSysColorBrush(COLOR_BTNSHADOW));
    PatBlt(hdc, x1, y1, 1, y2 - y1, PATCOPY);
    PatBlt(hdc, x2, y1, 1, y2 - y1 + 1, PATCOPY);
    PatBlt(hdc, x1, y1, x2 - x1, 1, PATCOPY);

    for (uint16_t window = 0; window < 8; window++)
    {
        int yp = y2 - window * cyLine * 2;
        uint16_t address = window << 13;
        DrawOctalValue(hdc, x, yp - cyLine / 2, address);
        PatBlt(hdc, x1, yp, x2 - x1, 1, PATCOPY);
    }

    PatBlt(hdc, x1, y1 + cyLine / 4, x2 - x1, 1, PATCOPY);

    uint16_t sp = pProc->GetSP();
    int ysp = y2 - ((y2 - y1) * sp / 65536);
    PatBlt(hdc, x2, ysp, cxChar, 1, PATCOPY);
    TextOut(hdc, x2 + cxChar, ysp - cyLine / 2, _T("SP"), 2);

    uint16_t pc = pProc->GetPC();
    int ypc = y2 - ((y2 - y1) * pc / 65536);
    PatBlt(hdc, x2, ypc, cxChar, 1, PATCOPY);
    TextOut(hdc, x2 + cxChar, ypc - cyLine / 2, _T("PC"), 2);

    ::SelectObject(hdc, hOldBrush);
}


//////////////////////////////////////////////////////////////////////
