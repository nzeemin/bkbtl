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
BOOL MemoryMapView_OnMouseWheel(WPARAM wParam, LPARAM lParam);
void MemoryMapView_InitBitmap();
void MemoryMapView_DoneBitmap();
void MemoryMapView_PrepareBitmap();
void MemoryMapView_Zoom(BOOL inout);
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
    default:
        return TRUE;
    }
    return FALSE;
}

void MemoryMapView_OnDraw(HDC hdc)
{
    ASSERT(g_pBoard != NULL);

    MemoryMapView_PrepareBitmap();

    DrawDibDraw(m_hMemoryMapDrawDib, hdc,
        0, 0, 256 * m_nMemoryMap_scale, 256 * m_nMemoryMap_scale,
        &m_bmpinfoMemoryMap.bmiHeader, m_pMemoryMap_bits, 0,0,
        256, 256,
        0);
}

void MemoryMapView_PrepareBitmap()
{
    for (int y = 0; y < 256; y += 1)
    {
        DWORD* pBits = m_pMemoryMap_bits + (256 - 1 - y) * 256;
        for (int x = 0; x < 256; x += 2)
        {
            WORD address = (WORD)(x + y * 256);
            BOOL valid;
            WORD value = g_pBoard->GetWordView(address, FALSE, FALSE, &valid);
            COLORREF color1, color2;
            if (valid)
            {
                BYTE val;
                val = (value & 0xff00) >> 8;
                color1 = RGB(val, val, val);
                val = value & 0x00ff;
                color2 = RGB(val, val, val);
            }
            else
            {
                color1 = color2 = RGB(128,0,0);
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
    siV.nPos = m_nMemoryMap_xpos;  //TODO
    siV.nMin = 0;
    siV.nMax = m_nMemoryMap_scale * 256 - 512;
    SetScrollInfo(m_hwndMemoryMapViewer, SB_VERT, &siV, TRUE);

    SCROLLINFO siH;
    ZeroMemory(&siH, sizeof(siH));
    siH.cbSize = sizeof(siH);
    siH.fMask = SIF_PAGE | SIF_POS | SIF_RANGE | SIF_DISABLENOSCROLL;
    siH.nPage = 256 * 2 / m_nMemoryMap_scale;
    siH.nPos = m_nMemoryMap_ypos;  //TODO
    siH.nMin = 0;
    siH.nMax = m_nMemoryMap_scale * 256 - 512;
    SetScrollInfo(m_hwndMemoryMapViewer, SB_HORZ, &siH, TRUE);
}


//////////////////////////////////////////////////////////////////////

