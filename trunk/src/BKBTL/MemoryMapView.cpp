/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// MemoryMapView.cpp

#include "stdafx.h"
#include <vfw.h>
#include "BKBTL.h"
#include "Views.h"
#include "ToolWindow.h"
#include "Dialogs.h"
#include "Emulator.h"


//////////////////////////////////////////////////////////////////////


HWND g_hwndMemoryMap = (HWND) INVALID_HANDLE_VALUE;  // MemoryMap view window handler
WNDPROC m_wndprocMemoryMapToolWindow = NULL;  // Old window proc address of the ToolWindow

HWND m_hwndMemoryMapViewer = (HWND) INVALID_HANDLE_VALUE;
HDRAWDIB m_hMemoryMapDrawDib = NULL;
BITMAPINFO m_bmpinfoMemoryMap;
HBITMAP m_hMemoryMapBitmap = NULL;
DWORD * m_pMemoryMap_bits = NULL;

int m_nMemoryMap_xpos = 0;
int m_nMemoryMap_ypos = 0;
int m_nMemoryMap_scale = 2;

void MemoryMapView_OnDraw(HDC hdc);
BOOL MemoryMapView_OnKeyDown(WPARAM vkey, LPARAM lParam);
BOOL MemoryMapView_OnVScroll(WPARAM wParam, LPARAM lParam);
BOOL MemoryMapView_OnHScroll(WPARAM wParam, LPARAM lParam);
BOOL MemoryMapView_OnMouseWheel(WPARAM wParam, LPARAM lParam);
void MemoryMapView_InitBitmap();
void MemoryMapView_DoneBitmap();
void MemoryMapView_PrepareBitmap();
void MemoryMapView_Zoom(BOOL inout);
void MemoryMapView_Scroll(int dx, int dy);
void MemoryMapView_UpdateScrollPos();


//////////////////////////////////////////////////////////////////////


void MemoryMapView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= MemoryMapViewViewerWndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= g_hInst;
    wcex.hIcon			= NULL;
    wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName	= NULL;
    wcex.lpszClassName	= CLASSNAME_MEMORYMAPVIEW;
    wcex.hIconSm		= NULL;

    RegisterClassEx(&wcex);
}

void CreateMemoryMapView(int x, int y)
{
    int cxBorder = ::GetSystemMetrics(SM_CXDLGFRAME);
    int cyBorder = ::GetSystemMetrics(SM_CYDLGFRAME);
    int cxScroll = ::GetSystemMetrics(SM_CXVSCROLL);
    int cyScroll = ::GetSystemMetrics(SM_CYHSCROLL);
    int cyCaption = ::GetSystemMetrics(SM_CYSMCAPTION);

    int width = 256 * 2 + cxScroll + cxBorder * 2;
    int height = 256 * 2 + cyScroll + cyBorder * 2 + cyCaption;
    g_hwndMemoryMap = CreateWindowEx(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
            CLASSNAME_OVERLAPPEDWINDOW, _T("BK Memory Map"),
            WS_POPUPWINDOW | WS_CAPTION | WS_VISIBLE,
            x, y, width, height,
            NULL, NULL, g_hInst, NULL);

    // ToolWindow subclassing
    m_wndprocMemoryMapToolWindow = (WNDPROC) LongToPtr( SetWindowLongPtr(
            g_hwndMemoryMap, GWLP_WNDPROC, PtrToLong(MemoryMapViewWndProc)) );

    RECT rcClient;  GetClientRect(g_hwndMemoryMap, &rcClient);
    
	m_hwndMemoryMapViewer = CreateWindow(
            CLASSNAME_MEMORYMAPVIEW, NULL,
            WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL | WS_TABSTOP,
            0, 0, rcClient.right, rcClient.bottom,
            g_hwndMemoryMap, NULL, g_hInst, NULL);

    MemoryMapView_InitBitmap();
    MemoryMapView_UpdateScrollPos();
    ::SetFocus(m_hwndMemoryMapViewer);
}

void MemoryMapView_InitBitmap()
{
    m_hMemoryMapDrawDib = DrawDibOpen();
    HDC hdc = GetDC( g_hwnd );

    m_bmpinfoMemoryMap.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
    m_bmpinfoMemoryMap.bmiHeader.biWidth = 256;
    m_bmpinfoMemoryMap.bmiHeader.biHeight = 256;
    m_bmpinfoMemoryMap.bmiHeader.biPlanes = 1;
    m_bmpinfoMemoryMap.bmiHeader.biBitCount = 32;
    m_bmpinfoMemoryMap.bmiHeader.biCompression = BI_RGB;
    m_bmpinfoMemoryMap.bmiHeader.biSizeImage = 0;
    m_bmpinfoMemoryMap.bmiHeader.biXPelsPerMeter = 0;
    m_bmpinfoMemoryMap.bmiHeader.biYPelsPerMeter = 0;
    m_bmpinfoMemoryMap.bmiHeader.biClrUsed = 0;
    m_bmpinfoMemoryMap.bmiHeader.biClrImportant = 0;
	
    m_hMemoryMapBitmap = CreateDIBSection( hdc, &m_bmpinfoMemoryMap, DIB_RGB_COLORS, (void **) &m_pMemoryMap_bits, NULL, 0 );

    ReleaseDC( g_hwnd, hdc );
}

void MemoryMapView_DoneBitmap()
{
    if (m_hMemoryMapBitmap != NULL)
    {
        DeleteObject(m_hMemoryMapBitmap);  m_hMemoryMapBitmap = NULL;
    }

    DrawDibClose( m_hMemoryMapDrawDib );
}

LRESULT CALLBACK MemoryMapViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_SETFOCUS:
        ::SetFocus(m_hwndMemoryMapViewer);
        break;
    case WM_DESTROY:
        g_hwndMemoryMap = (HWND) INVALID_HANDLE_VALUE;  // We are closed! Bye-bye!..
        return CallWindowProc(m_wndprocMemoryMapToolWindow, hWnd, message, wParam, lParam);
    default:
        return CallWindowProc(m_wndprocMemoryMapToolWindow, hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

LRESULT CALLBACK MemoryMapViewViewerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            MemoryMapView_OnDraw(hdc);  // Draw memory dump

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        // Free resources
        MemoryMapView_DoneBitmap();
        return DefWindowProc(hWnd, message, wParam, lParam);
    case WM_LBUTTONDOWN:
        SetFocus(hWnd);
        break;
    case WM_KEYDOWN:
        return (LRESULT) MemoryMapView_OnKeyDown(wParam, lParam);
    case WM_HSCROLL:
        return (LRESULT) MemoryMapView_OnHScroll(wParam, lParam);
    case WM_VSCROLL:
        return (LRESULT) MemoryMapView_OnVScroll(wParam, lParam);
    case WM_MOUSEWHEEL:
        return (LRESULT) MemoryMapView_OnMouseWheel(wParam, lParam);
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

BOOL MemoryMapView_OnMouseWheel(WPARAM wParam, LPARAM lParam)
{
    short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

    MemoryMapView_Zoom(zDelta > 0);

    return FALSE;
}

void MemoryMapView_Zoom(BOOL inout)
{
    if (inout)
    {
        if (m_nMemoryMap_scale >= 40)
            return;
        m_nMemoryMap_scale += 1;
    }
    else
    {
        if (m_nMemoryMap_scale <= 2)
            return;
        m_nMemoryMap_scale -= 1;
    }

    InvalidateRect(m_hwndMemoryMapViewer, NULL, FALSE);
    MemoryMapView_UpdateScrollPos();
}
void MemoryMapView_Scroll(int dx, int dy)
{
    int newxpos = m_nMemoryMap_xpos + dx;
    int newypos = m_nMemoryMap_ypos + dy;

    int maxpos = 256 * (m_nMemoryMap_scale - 2);  //INCORRECT
    if (newxpos < 0) newxpos = 0; 
    if (newxpos > maxpos) newxpos = maxpos; 
    if (newypos < 0) newypos = 0; 
    if (newypos > maxpos) newypos = maxpos; 

    m_nMemoryMap_xpos = newxpos;
    m_nMemoryMap_ypos = newypos;

    InvalidateRect(m_hwndMemoryMapViewer, NULL, TRUE);

    MemoryMapView_UpdateScrollPos();
}

BOOL MemoryMapView_OnKeyDown(WPARAM vkey, LPARAM lParam)
{
    switch (vkey)
    {
    case VK_OEM_MINUS:
        MemoryMapView_Zoom(FALSE);
        break;
    case VK_OEM_PLUS:
        MemoryMapView_Zoom(TRUE);
        break;
    case VK_LEFT:
        MemoryMapView_Scroll(-2, 0);
        break;
    case VK_RIGHT:
        MemoryMapView_Scroll(2, 0);
        break;
    case VK_UP:
        MemoryMapView_Scroll(0, -2);
        break;
    case VK_DOWN:
        MemoryMapView_Scroll(0, 2);
        break;
    default:
        return TRUE;
    }
    return FALSE;
}

BOOL MemoryMapView_OnHScroll(WPARAM wParam, LPARAM lParam)
{
    WORD scrollpos = HIWORD(wParam);
    WORD scrollcmd = LOWORD(wParam);
    switch (scrollcmd)
    {
    case SB_LINEDOWN:
        MemoryMapView_Scroll(8, 0);
        break;
    case SB_LINEUP:
        MemoryMapView_Scroll(-8, 0);
        break;
    case SB_PAGEDOWN:
        MemoryMapView_Scroll(32, 0);  //TODO
        break;
    case SB_PAGEUP:
        MemoryMapView_Scroll(32, 0);  //TODO
        break;
    //case SB_THUMBPOSITION:
    //    MemoryMapView_ScrollTo(scrollpos * 16);
    //    break;
    }

    return FALSE;
}
BOOL MemoryMapView_OnVScroll(WPARAM wParam, LPARAM lParam)
{
    WORD scrollpos = HIWORD(wParam);
    WORD scrollcmd = LOWORD(wParam);
    switch (scrollcmd)
    {
    case SB_LINEDOWN:
        MemoryMapView_Scroll(0, 8);
        break;
    case SB_LINEUP:
        MemoryMapView_Scroll(0, -8);
        break;
    case SB_PAGEDOWN:
        MemoryMapView_Scroll(0, 32);  //TODO
        break;
    case SB_PAGEUP:
        MemoryMapView_Scroll(0, -32);  //TODO
        break;
    //case SB_THUMBPOSITION:
    //    MemoryMapView_ScrollTo(scrollpos * 16);
    //    break;
    }

    return FALSE;
}

void MemoryMapView_OnDraw(HDC hdc)
{
    ASSERT(g_pBoard != NULL);

    MemoryMapView_PrepareBitmap();

    DrawDibDraw(m_hMemoryMapDrawDib, hdc,
        -m_nMemoryMap_xpos * m_nMemoryMap_scale, -m_nMemoryMap_ypos * m_nMemoryMap_scale,
        256 * m_nMemoryMap_scale, 256 * m_nMemoryMap_scale,
        &m_bmpinfoMemoryMap.bmiHeader, m_pMemoryMap_bits, 0,0,
        256, 256,
        0);
}

void MemoryMapView_RedrawMap()
{
    if (g_hwndMemoryMap == (HWND)INVALID_HANDLE_VALUE) return;

    MemoryMapView_PrepareBitmap();

    HDC hdc = GetDC(g_hwndMemoryMap);
    MemoryMapView_OnDraw(hdc);
    ::ReleaseDC(g_hwndMemoryMap, hdc);
}

void MemoryMapView_PrepareBitmap()
{
    for (int y = 0; y < 256; y += 1)
    {
        DWORD* pBits = m_pMemoryMap_bits + (256 - 1 - y) * 256;
        for (int x = 0; x < 256; x += 2)
        {
            WORD address = (WORD)(x + y * 256);
            int addrtype;
            WORD value = g_pBoard->GetWordView(address, FALSE, FALSE, &addrtype);
            COLORREF color1, color2;
            BYTE val;
            switch (addrtype & ADDRTYPE_MASK)
            {
            case ADDRTYPE_IO:
                color1 = color2 = RGB(128,0,128);
                break;
            case ADDRTYPE_DENY:
                color1 = color2 = RGB(0,0,128);
                break;
            case ADDRTYPE_ROM:
                val = (value & 0xff00) >> 8;
                color1 = RGB(192, val, val);
                val = value & 0x00ff;
                color2 = RGB(192, val, val);
                break;
            case ADDRTYPE_RAM:
                val = (value & 0xff00) >> 8;
                color1 = RGB(val, val, val);
                val = value & 0x00ff;
                color2 = RGB(val, val, val);
                break;
            }

            *pBits = color1;
            pBits++;
            *pBits = color2;
            pBits++;
        }
    }
}

void MemoryMapView_UpdateScrollPos()
{
    SCROLLINFO siV;
    ZeroMemory(&siV, sizeof(siV));
    siV.cbSize = sizeof(siV);
    siV.fMask = SIF_PAGE | SIF_POS | SIF_RANGE | SIF_DISABLENOSCROLL;
    siV.nPage = 256 * 2 / m_nMemoryMap_scale;
    siV.nPos = m_nMemoryMap_ypos;  //TODO
    siV.nMin = 0;
    siV.nMax = m_nMemoryMap_scale * 256 - 512;
    SetScrollInfo(m_hwndMemoryMapViewer, SB_VERT, &siV, TRUE);

    SCROLLINFO siH;
    ZeroMemory(&siH, sizeof(siH));
    siH.cbSize = sizeof(siH);
    siH.fMask = SIF_PAGE | SIF_POS | SIF_RANGE | SIF_DISABLENOSCROLL;
    siH.nPage = 256 * 2 / m_nMemoryMap_scale;
    siH.nPos = m_nMemoryMap_xpos;  //TODO
    siH.nMin = 0;
    siH.nMax = m_nMemoryMap_scale * 256 - 512;
    SetScrollInfo(m_hwndMemoryMapViewer, SB_HORZ, &siH, TRUE);
}


//////////////////////////////////////////////////////////////////////

