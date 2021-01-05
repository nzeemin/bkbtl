/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// SpriteView.cpp

#include "stdafx.h"
#include <vfw.h>
#include "Main.h"
#include "Views.h"
#include "ToolWindow.h"
#include "Dialogs.h"
#include "Emulator.h"
#include "emubase\Emubase.h"

//////////////////////////////////////////////////////////////////////


HWND g_hwndSprite = (HWND)INVALID_HANDLE_VALUE;  // Sprite view window handler
WNDPROC m_wndprocSpriteToolWindow = NULL;  // Old window proc address of the ToolWindow

HWND m_hwndSpriteViewer = (HWND)INVALID_HANDLE_VALUE;
HDRAWDIB m_hSpriteDrawDib = NULL;
BITMAPINFO m_bmpinfoSprite;
HBITMAP m_hSpriteBitmap = NULL;
DWORD * m_pSprite_bits = NULL;

const int m_nSprite_scale = 2;
const int m_nSprite_ImageCX = 262;
const int m_nSprite_ImageCY = 256;
const int m_nSprite_ViewCX = m_nSprite_ImageCX * m_nSprite_scale;
const int m_nSprite_ViewCY = m_nSprite_ImageCY * m_nSprite_scale;

WORD m_wSprite_BaseAddress = 0;
int m_nSprite_width = 2;

void SpriteView_OnDraw(HDC hdc);
BOOL SpriteView_OnKeyDown(WPARAM vkey, LPARAM lParam);
BOOL SpriteView_OnVScroll(WPARAM wParam, LPARAM lParam);
BOOL SpriteView_OnHScroll(WPARAM wParam, LPARAM lParam);
BOOL SpriteView_OnMouseWheel(WPARAM wParam, LPARAM lParam);
void SpriteView_InitBitmap();
void SpriteView_DoneBitmap();
void SpriteView_UpdateWindowText();
void SpriteView_PrepareBitmap();
void SpriteView_UpdateScrollPos();


//////////////////////////////////////////////////////////////////////

void SpriteView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = SpriteViewViewerWndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = g_hInst;
    wcex.hIcon = NULL;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = CLASSNAME_SPRITEVIEW;
    wcex.hIconSm = NULL;

    RegisterClassEx(&wcex);
}

void SpriteView_Create(int x, int y)
{
    int cxBorder = ::GetSystemMetrics(SM_CXDLGFRAME);
    int cyBorder = ::GetSystemMetrics(SM_CYDLGFRAME);
    int cxScroll = ::GetSystemMetrics(SM_CXVSCROLL);
    int cyCaption = ::GetSystemMetrics(SM_CYSMCAPTION);

    int width = m_nSprite_ViewCX + cxScroll + cxBorder * 2;
    int height = m_nSprite_ViewCY + cyBorder * 2 + cyCaption;
    g_hwndSprite = CreateWindowEx(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
            CLASSNAME_OVERLAPPEDWINDOW, _T("Sprite Viewer"),
            WS_POPUPWINDOW | WS_CAPTION | WS_VISIBLE,
            x, y, width, height,
            NULL, NULL, g_hInst, NULL);

    // ToolWindow subclassing
    m_wndprocSpriteToolWindow = (WNDPROC)LongToPtr(SetWindowLongPtr(
            g_hwndSprite, GWLP_WNDPROC, PtrToLong(SpriteViewWndProc)));

    RECT rcClient;  GetClientRect(g_hwndSprite, &rcClient);

    m_hwndSpriteViewer = CreateWindow(
            CLASSNAME_SPRITEVIEW, NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP,
            0, 0, rcClient.right, rcClient.bottom,
            g_hwndSprite, NULL, g_hInst, NULL);

    m_wSprite_BaseAddress = Settings_GetSpriteAddress();
    m_nSprite_width = (int)Settings_GetSpriteWidth();
    if (m_nSprite_width <= 0) m_nSprite_width = 1;
    if (m_nSprite_width > 32) m_nSprite_width = 32;

    SpriteView_InitBitmap();
    SpriteView_UpdateWindowText();
    SpriteView_UpdateScrollPos();
    ::SetFocus(m_hwndSpriteViewer);
}

void SpriteView_InitBitmap()
{
    m_hSpriteDrawDib = DrawDibOpen();
    HDC hdc = GetDC(g_hwnd);

    m_bmpinfoSprite.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    m_bmpinfoSprite.bmiHeader.biWidth = m_nSprite_ImageCX;
    m_bmpinfoSprite.bmiHeader.biHeight = m_nSprite_ImageCY;
    m_bmpinfoSprite.bmiHeader.biPlanes = 1;
    m_bmpinfoSprite.bmiHeader.biBitCount = 32;
    m_bmpinfoSprite.bmiHeader.biCompression = BI_RGB;
    m_bmpinfoSprite.bmiHeader.biSizeImage = 0;
    m_bmpinfoSprite.bmiHeader.biXPelsPerMeter = 0;
    m_bmpinfoSprite.bmiHeader.biYPelsPerMeter = 0;
    m_bmpinfoSprite.bmiHeader.biClrUsed = 0;
    m_bmpinfoSprite.bmiHeader.biClrImportant = 0;

    m_hSpriteBitmap = CreateDIBSection(hdc, &m_bmpinfoSprite, DIB_RGB_COLORS, (void **)&m_pSprite_bits, NULL, 0);

    VERIFY(::ReleaseDC(g_hwnd, hdc));
}

void SpriteView_DoneBitmap()
{
    if (m_hSpriteBitmap != NULL)
    {
        VERIFY(::DeleteObject(m_hSpriteBitmap));  m_hSpriteBitmap = NULL;
    }

    DrawDibClose(m_hSpriteDrawDib);
}

LRESULT CALLBACK SpriteViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_SETFOCUS:
        ::SetFocus(m_hwndSpriteViewer);
        break;
    case WM_DESTROY:
        g_hwndSprite = (HWND)INVALID_HANDLE_VALUE;  // We are closed! Bye-bye!..
        return CallWindowProc(m_wndprocSpriteToolWindow, hWnd, message, wParam, lParam);
    default:
        return CallWindowProc(m_wndprocSpriteToolWindow, hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

LRESULT CALLBACK SpriteViewViewerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            SpriteView_OnDraw(hdc);  // Draw memory dump

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        // Free resources
        SpriteView_DoneBitmap();
        return DefWindowProc(hWnd, message, wParam, lParam);
    case WM_LBUTTONDOWN:
        SetFocus(hWnd);
        break;
    case WM_KEYDOWN:
        return (LRESULT)SpriteView_OnKeyDown(wParam, lParam);
        //case WM_HSCROLL:
        //    return (LRESULT)SpriteView_OnHScroll(wParam, lParam);
    case WM_VSCROLL:
        return (LRESULT)SpriteView_OnVScroll(wParam, lParam);
    case WM_MOUSEWHEEL:
        return (LRESULT)SpriteView_OnMouseWheel(wParam, lParam);
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

void SpriteView_OnDraw(HDC hdc)
{
    ASSERT(g_pBoard != NULL);

    SpriteView_PrepareBitmap();

    DrawDibDraw(m_hSpriteDrawDib, hdc,
            0, 0,
            m_nSprite_ImageCX * m_nSprite_scale, m_nSprite_ImageCY * m_nSprite_scale,
            &m_bmpinfoSprite.bmiHeader, m_pSprite_bits, 0, 0,
            m_nSprite_ImageCX, m_nSprite_ImageCY,
            0);
}

void SpriteView_GoToAddress(WORD address)
{
    if (address == m_wSprite_BaseAddress)
        return;

    m_wSprite_BaseAddress = address;
    Settings_SetSpriteAddress(m_wSprite_BaseAddress);

    SpriteView_UpdateWindowText();
    SpriteView_PrepareBitmap();
    InvalidateRect(m_hwndSpriteViewer, NULL, TRUE);
}
void SpriteView_SetSpriteWidth(int width)
{
    if (width < 1) width = 1;
    if (width > m_nSprite_ImageCX / 8) width = m_nSprite_ImageCX / 8;
    if (width == m_nSprite_width)
        return;

    m_nSprite_width = width;
    Settings_SetSpriteWidth((WORD)width);

    SpriteView_UpdateWindowText();
    SpriteView_PrepareBitmap();
    InvalidateRect(m_hwndSpriteViewer, NULL, TRUE);
}

void SpriteView_UpdateWindowText()
{
    TCHAR buffer[48];
    _stprintf_s(buffer, 48, _T("Sprite Viewer - address %06o, width %d"), m_wSprite_BaseAddress, m_nSprite_width);
    ::SetWindowText(g_hwndSprite, buffer);
}

BOOL SpriteView_OnKeyDown(WPARAM vkey, LPARAM /*lParam*/)
{
    switch (vkey)
    {
    case VK_LEFT:
        SpriteView_GoToAddress(m_wSprite_BaseAddress - (WORD)(m_nSprite_ImageCY * m_nSprite_width));
        break;
    case VK_RIGHT:
        SpriteView_GoToAddress(m_wSprite_BaseAddress + (WORD)(m_nSprite_ImageCY * m_nSprite_width));
        break;
    case VK_UP:
        if (GetKeyState(VK_CONTROL) & 0x8000)
            SpriteView_GoToAddress(m_wSprite_BaseAddress - (WORD)m_nSprite_width);
        else
            SpriteView_GoToAddress(m_wSprite_BaseAddress - 1);
        break;
    case VK_DOWN:
        if (GetKeyState(VK_CONTROL) & 0x8000)
            SpriteView_GoToAddress(m_wSprite_BaseAddress + (WORD)m_nSprite_width);
        else
            SpriteView_GoToAddress(m_wSprite_BaseAddress + 1);
        break;
    case VK_OEM_4: // '[' -- Decrement Sprite Width
        SpriteView_SetSpriteWidth(m_nSprite_width - 1);
        break;
    case VK_OEM_6: // ']' -- Increment Sprite Width
        SpriteView_SetSpriteWidth(m_nSprite_width + 1);
        break;
    case 0x47:  // G - Go To Address
        {
            WORD value = m_wSprite_BaseAddress;
            if (InputBoxOctal(m_hwndSpriteViewer, _T("Go To Address"), &value))
                SpriteView_GoToAddress(value);
            break;
        }
    default:
        return TRUE;
    }
    return FALSE;
}

BOOL SpriteView_OnMouseWheel(WPARAM wParam, LPARAM /*lParam*/)
{
    short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

    int nDelta = zDelta / 120;
    if (nDelta > 4) nDelta = 4;
    if (nDelta < -4) nDelta = -4;

    SpriteView_GoToAddress(m_wSprite_BaseAddress - (WORD)(nDelta * 3 * m_nSprite_width));

    return FALSE;
}

BOOL SpriteView_OnVScroll(WPARAM wParam, LPARAM /*lParam*/)
{
    //WORD scrollpos = HIWORD(wParam);
    WORD scrollcmd = LOWORD(wParam);
    switch (scrollcmd)
    {
    case SB_LINEDOWN:
        SpriteView_GoToAddress(m_wSprite_BaseAddress + (WORD)m_nSprite_width);
        break;
    case SB_LINEUP:
        SpriteView_GoToAddress(m_wSprite_BaseAddress - (WORD)m_nSprite_width);
        break;
        //case SB_PAGEDOWN:
        //    break;
        //case SB_PAGEUP:
        //    break;
        //case SB_THUMBPOSITION:
        //    break;
    }

    return FALSE;
}

void SpriteView_UpdateScrollPos()
{
    //TODO
}

void SpriteView_PrepareBitmap()
{
    // Clear bitmap
    memset(m_pSprite_bits, 0x7f, m_nSprite_ImageCX * m_nSprite_ImageCY * 4);

    WORD address = m_wSprite_BaseAddress;

    for (int x = 0; x + m_nSprite_width * 8 <= m_nSprite_ImageCX; x += m_nSprite_width * 8 + 2)
    {
        for (int y = 0; y < m_nSprite_ImageCY; y++)
        {
            DWORD* pBits = m_pSprite_bits + (m_nSprite_ImageCY - 1 - y) * m_nSprite_ImageCX + x;

            for (int w = 0; w < m_nSprite_width; w++)
            {
                // Get byte from memory
                int addrtype = 0;
                BOOL okHalt = FALSE;
                okHalt = g_pBoard->GetCPU()->IsHaltMode();
                WORD value = g_pBoard->GetWordView(address & ~1, okHalt, FALSE, &addrtype);
                if (address & 1)
                    value = value >> 8;

                for (int i = 0; i < 8; i++)
                {
                    COLORREF color = (value & 1) ? 0xffffff : 0;
                    *pBits = color;
                    pBits++;

                    value = value >> 1;
                }

                address++;
            }
        }
    }
}


//////////////////////////////////////////////////////////////////////
