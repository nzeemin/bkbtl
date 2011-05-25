/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// TeletypeView.cpp

#include "stdafx.h"
#include "BKBTL.h"
#include "Views.h"
#include "ToolWindow.h"
//#include "Emulator.h"
//#include "emubase\Emubase.h"


//////////////////////////////////////////////////////////////////////


HWND g_hwndTeletype = (HWND) INVALID_HANDLE_VALUE;  // Teletype view window handler
WNDPROC m_wndprocTeletypeToolWindow = NULL;  // Old window proc address of the ToolWindow

HWND m_hwndTeletypeLog = (HWND) INVALID_HANDLE_VALUE;  // Teletype log window - read-only edit control
HFONT m_hfontTeletype = NULL;


//////////////////////////////////////////////////////////////////////


void CreateTeletypeView(int x, int y, int width, int height)
{
    int cxBorder = ::GetSystemMetrics(SM_CXDLGFRAME);
    int cyBorder = ::GetSystemMetrics(SM_CYDLGFRAME);
    int cxScroll = ::GetSystemMetrics(SM_CXVSCROLL);
    int cyScroll = ::GetSystemMetrics(SM_CYHSCROLL);
    int cyCaption = ::GetSystemMetrics(SM_CYSMCAPTION);

    g_hwndTeletype = CreateWindowEx(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
            CLASSNAME_OVERLAPPEDWINDOW, _T("BK Teletype"),
            WS_POPUPWINDOW | WS_CAPTION | WS_VISIBLE,
            x, y, width, height,
            NULL, NULL, g_hInst, NULL);

    // ToolWindow subclassing
    m_wndprocTeletypeToolWindow = (WNDPROC) LongToPtr( SetWindowLongPtr(
            g_hwndTeletype, GWLP_WNDPROC, PtrToLong(TeletypeViewWndProc)) );

    RECT rcClient;  ::GetClientRect(g_hwndTeletype, &rcClient);

    m_hwndTeletypeLog = CreateWindowEx(
            WS_EX_CLIENTEDGE,
            _T("EDIT"), NULL,
            WS_CHILD | WS_VSCROLL | WS_VISIBLE | ES_READONLY | ES_MULTILINE,
            0, 0,
            rcClient.right, rcClient.bottom,
            g_hwndTeletype, NULL, g_hInst, NULL);

    m_hfontTeletype = CreateMonospacedFont();
    ::SendMessage(m_hwndTeletypeLog, WM_SETFONT, (WPARAM) m_hfontTeletype, 0);

    ::ShowWindow(g_hwndTeletype, SW_SHOW);
}

// Adjust position of client windows
void TeletypeView_AdjustWindowLayout()
{
    RECT rc;  GetClientRect(g_hwndTeletype, &rc);

    if (m_hwndTeletypeLog != (HWND) INVALID_HANDLE_VALUE)
        SetWindowPos(m_hwndTeletypeLog, NULL, 0, 0, rc.right, rc.bottom, SWP_NOZORDER);
}

LRESULT CALLBACK TeletypeViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
	LRESULT lResult;
    switch (message)
    {
    case WM_DESTROY:
        g_hwndTeletype = (HWND) INVALID_HANDLE_VALUE;  // We are closed! Bye-bye!..
        return CallWindowProc(m_wndprocTeletypeToolWindow, hWnd, message, wParam, lParam);
    case WM_SIZE:
        lResult = CallWindowProc(m_wndprocTeletypeToolWindow, hWnd, message, wParam, lParam);
        TeletypeView_AdjustWindowLayout();
		return lResult;
    default:
        return CallWindowProc(m_wndprocTeletypeToolWindow, hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

void TeletypeView_Output(LPCTSTR message)
{
    // Put selection to the end of text
    SendMessage(m_hwndTeletypeLog, EM_SETSEL, 0x100000, 0x100000);
    // Insert the message
    SendMessage(m_hwndTeletypeLog, EM_REPLACESEL, (WPARAM) FALSE, (LPARAM) message);
    // Scroll to caret
    SendMessage(m_hwndTeletypeLog, EM_SCROLLCARET, 0, 0);
}

void TeletypeView_OutputSymbol(TCHAR symbol)
{
    if (m_hwndTeletypeLog == NULL) return;

    TCHAR message[2];
    message[0] = symbol;
    message[1] = 0;

    TeletypeView_Output(message);
}

//////////////////////////////////////////////////////////////////////
