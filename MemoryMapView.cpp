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

void MemoryMapView_OnDraw(HDC hdc);
void MemoryMapView_InitBitmap();
void MemoryMapView_DoneBitmap();
void MemoryMap_PrepareBitmap();


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
    int cxBorder = ::GetSystemMetrics(SM_CXBORDER);
    int cyBorder = ::GetSystemMetrics(SM_CYBORDER);
    int cxScroll = ::GetSystemMetrics(SM_CXVSCROLL);
    int cyScroll = ::GetSystemMetrics(SM_CYHSCROLL);
    int cyCaption = ::GetSystemMetrics(SM_CYCAPTION);

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
    
	m_hwndMemoryMapViewer = CreateWindowEx(
            WS_EX_STATICEDGE,
            CLASSNAME_MEMORYMAPVIEW, NULL,
            WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL | WS_TABSTOP,
            0, 0, rcClient.right, rcClient.bottom,
            g_hwndMemoryMap, NULL, g_hInst, NULL);

    MemoryMapView_InitBitmap();
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
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

void MemoryMapView_OnDraw(HDC hdc)
{
    ASSERT(g_pBoard != NULL);

    MemoryMap_PrepareBitmap();

    DrawDibDraw(m_hMemoryMapDrawDib, hdc,
        0, 0, 256 * 2, 256 * 2,
        &m_bmpinfoMemoryMap.bmiHeader, m_pMemoryMap_bits, 0,0,
        256, 256,
        0);
}

void MemoryMap_PrepareBitmap()
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
                val = value & 0xff00;
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


//////////////////////////////////////////////////////////////////////

