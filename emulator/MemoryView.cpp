/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// MemoryView.cpp

#include "stdafx.h"
#include <commctrl.h>
#include <windowsx.h>
#include "Main.h"
#include "Views.h"
#include "ToolWindow.h"
#include "Dialogs.h"
#include "Emulator.h"
#include "emubase\Emubase.h"

//////////////////////////////////////////////////////////////////////


HWND g_hwndMemory = (HWND) INVALID_HANDLE_VALUE;  // Memory view window handler
WNDPROC m_wndprocMemoryToolWindow = NULL;  // Old window proc address of the ToolWindow

HWND m_hwndMemoryViewer = (HWND) INVALID_HANDLE_VALUE;
HWND m_hwndMemoryToolbar = (HWND) INVALID_HANDLE_VALUE;

int m_cxChar = 0;
int m_cyLineMemory = 0;  // Line height in pixels
int m_nPageSize = 100;  // Page size in lines

//enum MemoryViewMode {
//    MEMMODE_RAM0 = 0,  // RAM plane 0
//    MEMMODE_RAM1 = 1,  // RAM plane 1
//    MEMMODE_RAM2 = 2,  // RAM plane 2
//    MEMMODE_ROM  = 3,  // ROM
//    MEMMODE_CPU  = 4,  // CPU memory
//    MEMMODE_LAST = 5,  // Last mode number
//};

//int     m_Mode = MEMMODE_ROM;  // See MemoryViewMode enum
WORD    m_wBaseAddress = 0xFFFF;
WORD    m_wCurrentAddress = 0xFFFF;
BOOL    m_okMemoryByteMode = FALSE;

void MemoryView_AdjustWindowLayout();
BOOL MemoryView_OnKeyDown(WPARAM vkey, LPARAM lParam);
void MemoryView_OnLButtonDown(int mousex, int mousey);
void MemoryView_OnRButtonDown(int mousex, int mousey);
BOOL MemoryView_OnMouseWheel(WPARAM wParam, LPARAM lParam);
BOOL MemoryView_OnVScroll(WORD scrollcmd, WORD scrollpos);
void MemoryView_CopyValueToClipboard(WPARAM command);
void MemoryView_GotoAddress(WORD wAddress);
void MemoryView_ScrollTo(WORD wBaseAddress);
void MemoryView_UpdateWindowText();
//LPCTSTR MemoryView_GetMemoryModeName();
void MemoryView_UpdateScrollPos();
void MemoryView_GetCurrentValueRect(LPRECT pRect, int cxChar, int cyLine);
void MemoryView_OnDraw(HDC hdc);


//////////////////////////////////////////////////////////////////////


void MemoryView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = MemoryViewViewerWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = g_hInst;
    wcex.hIcon          = NULL;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = CLASSNAME_MEMORYVIEW;
    wcex.hIconSm        = NULL;

    RegisterClassEx(&wcex);
}

void MemoryView_Create(HWND hwndParent, int x, int y, int width, int height)
{
    ASSERT(hwndParent != NULL);

    m_okMemoryByteMode = Settings_GetDebugMemoryByte();

    g_hwndMemory = CreateWindow(
            CLASSNAME_TOOLWINDOW, NULL,
            WS_CHILD | WS_VISIBLE,
            x, y, width, height,
            hwndParent, NULL, g_hInst, NULL);
    MemoryView_UpdateWindowText();

    // ToolWindow subclassing
    m_wndprocMemoryToolWindow = (WNDPROC) LongToPtr( SetWindowLongPtr(
            g_hwndMemory, GWLP_WNDPROC, PtrToLong(MemoryViewWndProc)) );

    RECT rcClient;  GetClientRect(g_hwndMemory, &rcClient);

    m_hwndMemoryViewer = CreateWindowEx(
            WS_EX_STATICEDGE,
            CLASSNAME_MEMORYVIEW, NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP,
            0, 0, rcClient.right, rcClient.bottom,
            g_hwndMemory, NULL, g_hInst, NULL);

    m_hwndMemoryToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL,
            WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TRANSPARENT | TBSTYLE_TOOLTIPS | CCS_NOPARENTALIGN | CCS_NODIVIDER | CCS_VERT,
            4, 4, 32, 32 * 6, m_hwndMemoryViewer,
            (HMENU) 102,
            g_hInst, NULL);

    TBADDBITMAP addbitmap;
    addbitmap.hInst = g_hInst;
    addbitmap.nID = IDB_TOOLBAR;
    SendMessage(m_hwndMemoryToolbar, TB_ADDBITMAP, 2, (LPARAM) &addbitmap);

    SendMessage(m_hwndMemoryToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM) sizeof(TBBUTTON), 0);
    SendMessage(m_hwndMemoryToolbar, TB_SETBUTTONSIZE, 0, (LPARAM) MAKELONG (26, 26));

    TBBUTTON buttons[2];
    ZeroMemory(buttons, sizeof(buttons));
    for (int i = 0; i < sizeof(buttons) / sizeof(TBBUTTON); i++)
    {
        buttons[i].fsState = TBSTATE_ENABLED | TBSTATE_WRAP;
        buttons[i].fsStyle = BTNS_BUTTON | TBSTYLE_GROUP;
        buttons[i].iString = -1;
    }
    buttons[0].idCommand = ID_DEBUG_MEMORY_GOTO;
    buttons[0].iBitmap = ToolbarImageGotoAddress;
    buttons[1].idCommand = ID_DEBUG_MEMORY_WORDBYTE;
    buttons[1].iBitmap = ToolbarImageWordByte;

    SendMessage(m_hwndMemoryToolbar, TB_ADDBUTTONS, (WPARAM) sizeof(buttons) / sizeof(TBBUTTON), (LPARAM) &buttons);

    MemoryView_ScrollTo(Settings_GetDebugMemoryBase());
    MemoryView_GotoAddress(Settings_GetDebugMemoryAddress());
}

// Adjust position of client windows
void MemoryView_AdjustWindowLayout()
{
    RECT rc;  GetClientRect(g_hwndMemory, &rc);

    if (m_hwndMemoryViewer != (HWND) INVALID_HANDLE_VALUE)
        SetWindowPos(m_hwndMemoryViewer, NULL, 0, 0, rc.right, rc.bottom, SWP_NOZORDER);
}

LRESULT CALLBACK MemoryViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    LRESULT lResult;
    switch (message)
    {
    case WM_DESTROY:
        g_hwndMemory = (HWND) INVALID_HANDLE_VALUE;  // We are closed! Bye-bye!..
        return CallWindowProc(m_wndprocMemoryToolWindow, hWnd, message, wParam, lParam);
    case WM_SIZE:
        lResult = CallWindowProc(m_wndprocMemoryToolWindow, hWnd, message, wParam, lParam);
        MemoryView_AdjustWindowLayout();
        return lResult;
    default:
        return CallWindowProc(m_wndprocMemoryToolWindow, hWnd, message, wParam, lParam);
    }
    //return (LRESULT)FALSE;
}

LRESULT CALLBACK MemoryViewViewerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_COMMAND:
        if (wParam == ID_DEBUG_COPY_VALUE || wParam == ID_DEBUG_COPY_ADDRESS)  // Copy command from context menu
            MemoryView_CopyValueToClipboard(wParam);
        else if (wParam == ID_DEBUG_GOTO_ADDRESS)  // "Go to Address" from context menu
            MemoryView_SelectAddress();
        else
            ::PostMessage(g_hwnd, WM_COMMAND, wParam, lParam);
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            MemoryView_OnDraw(hdc);

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_LBUTTONDOWN:
        MemoryView_OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        break;
    case WM_RBUTTONDOWN:
        MemoryView_OnRButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        break;
    case WM_KEYDOWN:
        return (LRESULT) MemoryView_OnKeyDown(wParam, lParam);
    case WM_MOUSEWHEEL:
        return (LRESULT) MemoryView_OnMouseWheel(wParam, lParam);
    case WM_VSCROLL:
        return (LRESULT) MemoryView_OnVScroll(LOWORD(wParam), HIWORD(wParam));
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        ::InvalidateRect(hWnd, NULL, TRUE);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

// Returns even address 0-65534, or -1 if out of area
int MemoryView_GetAddressByPoint(int mousex, int mousey)
{
    int line = mousey / m_cyLineMemory - 1;
    if (line < 0) return -1;
    else if (line >= m_nPageSize) return -1;
    int pos = (mousex - 12 * m_cxChar - m_cxChar / 2) / (m_cxChar * 7);
    if (pos < 0) return -1;
    else if (pos > 7) return -1;

    return (WORD)(m_wBaseAddress + line * 16 + pos * 2);
}

BOOL MemoryView_OnKeyDown(WPARAM vkey, LPARAM /*lParam*/)
{
    switch (vkey)
    {
    case VK_ESCAPE:
        ConsoleView_Activate();
        break;
    case VK_HOME:
        MemoryView_GotoAddress(0);
        break;
    case VK_LEFT:
        MemoryView_GotoAddress((WORD)(m_wCurrentAddress - 2));
        break;
    case VK_RIGHT:
        MemoryView_GotoAddress((WORD)(m_wCurrentAddress + 2));
        break;
    case VK_UP:
        MemoryView_GotoAddress((WORD)(m_wCurrentAddress - 16));
        break;
    case VK_DOWN:
        MemoryView_GotoAddress((WORD)(m_wCurrentAddress + 16));
        break;
    case VK_PRIOR:
        MemoryView_GotoAddress((WORD)(m_wCurrentAddress - m_nPageSize * 16));
        break;
    case VK_NEXT:
        MemoryView_GotoAddress((WORD)(m_wCurrentAddress + m_nPageSize * 16));
        break;
    case 0x47:  // G - Go To Address
        MemoryView_SelectAddress();
        break;
    case 0x42:  // B - change byte/word mode
    case 0x57:  // W
        MemoryView_SwitchWordByte();
        break;
    default:
        return TRUE;
    }
    return FALSE;
}

void MemoryView_OnLButtonDown(int mousex, int mousey)
{
    ::SetFocus(m_hwndMemoryViewer);

    int addr = MemoryView_GetAddressByPoint(mousex, mousey);
    if (addr >= 0)
        MemoryView_GotoAddress((WORD)addr);
}

void MemoryView_OnRButtonDown(int mousex, int mousey)
{
    ::SetFocus(m_hwndMemoryViewer);

    int addr = MemoryView_GetAddressByPoint(mousex, mousey);
    if (addr >= 0)
        MemoryView_GotoAddress((WORD)addr);

    RECT rcValue;
    MemoryView_GetCurrentValueRect(&rcValue, m_cxChar, m_cyLineMemory);

    HMENU hMenu = ::CreatePopupMenu();
    ::AppendMenu(hMenu, 0, ID_DEBUG_COPY_VALUE, _T("Copy Value"));
    ::AppendMenu(hMenu, 0, ID_DEBUG_COPY_ADDRESS, _T("Copy Address"));
    ::AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    ::AppendMenu(hMenu, 0, ID_DEBUG_GOTO_ADDRESS, _T("Go to Address..."));

    POINT pt = { rcValue.left, rcValue.bottom };
    ::ClientToScreen(m_hwndMemoryViewer, &pt);
    ::TrackPopupMenu(hMenu, 0, pt.x, pt.y, 0, m_hwndMemoryViewer, NULL);

    VERIFY(::DestroyMenu(hMenu));
}

BOOL MemoryView_OnMouseWheel(WPARAM wParam, LPARAM /*lParam*/)
{
    short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

    int nDelta = zDelta / 120;
    if (nDelta > 5) nDelta = 5;
    if (nDelta < -5) nDelta = -5;

    MemoryView_GotoAddress((WORD)(m_wCurrentAddress - nDelta * 2 * 16));

    return FALSE;
}

BOOL MemoryView_OnVScroll(WORD scrollcmd, WORD scrollpos)
{
    switch (scrollcmd)
    {
    case SB_LINEDOWN:
        MemoryView_GotoAddress((WORD)(m_wCurrentAddress + 16));
        break;
    case SB_LINEUP:
        MemoryView_GotoAddress((WORD)(m_wCurrentAddress - 16));
        break;
    case SB_PAGEDOWN:
        MemoryView_GotoAddress((WORD)(m_wCurrentAddress + m_nPageSize * 16));
        break;
    case SB_PAGEUP:
        MemoryView_GotoAddress((WORD)(m_wCurrentAddress - m_nPageSize * 16));
        break;
    case SB_THUMBPOSITION:
        MemoryView_GotoAddress((WORD)(scrollpos * 16));
        break;
    }

    return FALSE;
}

void MemoryView_UpdateWindowText()
{
    TCHAR buffer[64];
    swprintf_s(buffer, 64, _T("Memory - %06o"), m_wCurrentAddress);
    ::SetWindowText(g_hwndMemory, buffer);
}

void MemoryView_CopyValueToClipboard(WPARAM command)
{
    WORD address = m_wCurrentAddress;
    WORD value;

    if (command == ID_DEBUG_COPY_ADDRESS)
    {
        value = address;
    }
    else
    {
        // Get word from memory
        int addrtype;
        BOOL okHalt = g_pBoard->GetCPU()->IsHaltMode();
        value = g_pBoard->GetWordView(address, okHalt, FALSE, &addrtype);
        BOOL okValid = (addrtype != ADDRTYPE_IO) && (addrtype != ADDRTYPE_DENY);

        if (!okValid)
        {
            AlertWarning(_T("Failed to get value: invalid memory type."));
            return;
        }
    }

    TCHAR buffer[7];
    PrintOctalValue(buffer, value);

    // Prepare global memory object for the text
    HGLOBAL hglbCopy = ::GlobalAlloc(GMEM_MOVEABLE, sizeof(buffer));
    LPTSTR lptstrCopy = (LPTSTR) ::GlobalLock(hglbCopy);
    memcpy(lptstrCopy, buffer, sizeof(buffer));
    ::GlobalUnlock(hglbCopy);

    // Send the text to the Clipboard
    ::OpenClipboard(g_hwnd);
    ::EmptyClipboard();
    ::SetClipboardData(CF_UNICODETEXT, hglbCopy);
    ::CloseClipboard();
}

LPCTSTR MemoryView_GetMemoryModeName()
{
    //  switch (m_Mode) {
    //      case MEMMODE_RAM0:  return _T("RAM0");
    //      case MEMMODE_RAM1:  return _T("RAM1");
    //      case MEMMODE_RAM2:  return _T("RAM2");
    //      case MEMMODE_ROM:   return _T("ROM");
    //      case MEMMODE_CPU:   return _T("CPU");
    //default:
    //    return _T("UKWN");  // Unknown mode
    //  }
    return _T("");  //STUB
}

void MemoryView_SwitchWordByte()
{
    m_okMemoryByteMode = !m_okMemoryByteMode;
    Settings_SetDebugMemoryByte(m_okMemoryByteMode);

    InvalidateRect(m_hwndMemoryViewer, NULL, TRUE);
}

void MemoryView_SelectAddress()
{
    WORD value = m_wCurrentAddress;
    if (InputBoxOctal(m_hwndMemoryViewer, _T("Go To Address"), &value))
        MemoryView_GotoAddress(value);
    ::SetFocus(m_hwndMemoryViewer);
}

void MemoryView_GotoAddress(WORD wAddress)
{
    m_wCurrentAddress = wAddress & ((WORD)~1);  // Address should be even
    Settings_SetDebugMemoryAddress(m_wCurrentAddress);

    int addroffset = wAddress - m_wBaseAddress;
    if (addroffset < 0)
    {
        WORD baseaddr = (m_wCurrentAddress & 0xFFF0);  // Base address should be 16-byte aligned
        MemoryView_ScrollTo(baseaddr);
    }
    else if (addroffset >= m_nPageSize * 16)
    {
        WORD baseaddr = (WORD)((m_wCurrentAddress & 0xFFF0) - (m_nPageSize - 1) * 16);
        MemoryView_ScrollTo(baseaddr);
    }

    MemoryView_UpdateWindowText();
}

// Scroll window to the given base address
void MemoryView_ScrollTo(WORD wBaseAddress)
{
    if (wBaseAddress == m_wBaseAddress)
        return;

    m_wBaseAddress = wBaseAddress;
    Settings_SetDebugMemoryBase(wBaseAddress);

    InvalidateRect(m_hwndMemoryViewer, NULL, TRUE);

    MemoryView_UpdateScrollPos();
}

void MemoryView_UpdateScrollPos()
{
    SCROLLINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
    si.nPage = m_nPageSize;
    si.nPos = m_wBaseAddress / 16;
    si.nMin = 0;
    si.nMax = 0x10000 / 16 - 1;
    SetScrollInfo(m_hwndMemoryViewer, SB_VERT, &si, TRUE);
}

void MemoryView_GetCurrentValueRect(LPRECT pRect, int cxChar, int cyLine)
{
    ASSERT(pRect != NULL);

    int addroffset = m_wCurrentAddress - m_wBaseAddress;
    int line = addroffset / 16;
    int pos = addroffset & 15;

    pRect->left = cxChar * (13 + 7 * (pos / 2)) - cxChar / 2;
    pRect->right = pRect->left + cxChar * 7;
    pRect->top = (line + 1) * cyLine - 1;
    pRect->bottom = pRect->top + cyLine + 1;
}

void MemoryView_OnDraw(HDC hdc)
{
    ASSERT(g_pBoard != NULL);

    HFONT hFont = CreateMonospacedFont();
    HGDIOBJ hOldFont = SelectObject(hdc, hFont);
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);
    COLORREF colorText = Settings_GetColor(ColorDebugText);
    COLORREF colorChanged = Settings_GetColor(ColorDebugValueChanged);
    COLORREF colorMemoryRom = Settings_GetColor(ColorDebugMemoryRom);
    COLORREF colorMemoryIO = Settings_GetColor(ColorDebugMemoryIO);
    COLORREF colorMemoryNA = Settings_GetColor(ColorDebugMemoryNA);
    COLORREF colorOld = SetTextColor(hdc, colorText);
    COLORREF colorBkOld = SetBkColor(hdc, GetSysColor(COLOR_WINDOW));

    m_cxChar = cxChar;
    m_cyLineMemory = cyLine;

    TCHAR buffer[7];
    const TCHAR* ADDRESS_LINE = _T(" addr   0      2      4      6      10     12     14     16");
    TextOut(hdc, cxChar * 5, 0, ADDRESS_LINE, (int)_tcslen(ADDRESS_LINE));

    RECT rcClip;
    GetClipBox(hdc, &rcClip);
    RECT rcClient;
    GetClientRect(m_hwndMemoryViewer, &rcClient);
    m_nPageSize = rcClient.bottom / cyLine - 1;

    WORD address = m_wBaseAddress;
    int y = 1 * cyLine;
    for (;;)    // Draw lines
    {
        DrawOctalValue(hdc, 5 * cxChar, y, address);

        int x = 13 * cxChar;
        TCHAR wchars[16];
        for (int j = 0; j < 8; j++)    // Draw words as octal value
        {
            // Get word from memory
            int addrtype;
            BOOL okHalt = g_pBoard->GetCPU()->IsHaltMode();
            WORD word = g_pBoard->GetWordView(address, okHalt, FALSE, &addrtype);
            BOOL okValid = (addrtype != ADDRTYPE_IO) && (addrtype != ADDRTYPE_DENY);
            WORD wChanged = Emulator_GetChangeRamStatus(address);

            if (okValid)
            {
                if (addrtype == ADDRTYPE_ROM)
                    ::SetTextColor(hdc, colorMemoryRom);
                else
                    ::SetTextColor(hdc, (wChanged != 0) ? colorChanged : colorText);
                if (m_okMemoryByteMode)
                {
                    PrintOctalValue(buffer, (word & 0xff));
                    TextOut(hdc, x, y, buffer + 3, 3);
                    PrintOctalValue(buffer, (word >> 8));
                    TextOut(hdc, x + 3 * cxChar + 3, y, buffer + 3, 3);
                }
                else
                    DrawOctalValue(hdc, x, y, word);
            }
            else  // !okValid
            {
                if (addrtype == ADDRTYPE_IO)
                {
                    ::SetTextColor(hdc, colorMemoryIO);
                    TextOut(hdc, x, y, _T("  IO"), 4);
                }
                else
                {
                    ::SetTextColor(hdc, colorMemoryNA);
                    TextOut(hdc, x, y, _T("  NA"), 4);
                }
            }

            // Prepare characters to draw at right
            BYTE ch1 = LOBYTE(word);
            TCHAR wch1 = Translate_BK_Unicode(ch1);
            if (ch1 < 32) wch1 = _T('·');
            wchars[j * 2] = wch1;
            BYTE ch2 = HIBYTE(word);
            TCHAR wch2 = Translate_BK_Unicode(ch2);
            if (ch2 < 32) wch2 = _T('·');
            wchars[j * 2 + 1] = wch2;

            address += 2;
            x += 7 * cxChar;
        }
        ::SetTextColor(hdc, colorText);

        // Draw characters at right
        int xch = x + cxChar;
        TextOut(hdc, xch, y, wchars, 16);

        y += cyLine;
        if (y > rcClip.bottom) break;
    }

    SetTextColor(hdc, colorOld);
    SetBkColor(hdc, colorBkOld);
    SelectObject(hdc, hOldFont);
    VERIFY(::DeleteObject(hFont));

    if (::GetFocus() == m_hwndMemoryViewer)
    {
        RECT rcFocus;
        MemoryView_GetCurrentValueRect(&rcFocus, cxChar, cyLine);
        DrawFocusRect(hdc, &rcFocus);
    }
}


//////////////////////////////////////////////////////////////////////
