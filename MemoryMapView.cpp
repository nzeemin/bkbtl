// MemoryMapView.cpp

#include "stdafx.h"
#include "BKBTL.h"
#include "Views.h"
#include "ToolWindow.h"
#include "Dialogs.h"
#include "Emulator.h"
//#include "emubase\Emubase.h"


//////////////////////////////////////////////////////////////////////


HWND g_hwndMemoryMap = (HWND) INVALID_HANDLE_VALUE;  // MemoryMap view window handler
//WNDPROC m_wndprocMemoryMapToolWindow = NULL;  // Old window proc address of the ToolWindow

HWND m_hwndMemoryMapViewer = (HWND) INVALID_HANDLE_VALUE;

void MemoryMapView_OnDraw(HDC hdc);


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

void CreateMemoryMapView(int x, int y, int width, int height)
{
    g_hwndMemoryMap = CreateWindow(
            CLASSNAME_OVERLAPPEDWINDOW, NULL,
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_VISIBLE,
            x, y, width, height,
            NULL, NULL, g_hInst, NULL);

    //// ToolWindow subclassing
    //m_wndprocMemoryMapToolWindow = (WNDPROC) LongToPtr( SetWindowLongPtr(
    //        g_hwndMemoryMap, GWLP_WNDPROC, PtrToLong(MemoryMapViewWndProc)) );

    RECT rcClient;  GetClientRect(g_hwndMemoryMap, &rcClient);
    
	m_hwndMemoryMapViewer = CreateWindowEx(
            WS_EX_STATICEDGE,
            CLASSNAME_MEMORYMAPVIEW, NULL,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0, 0, rcClient.right, rcClient.bottom,
            g_hwndMemoryMap, NULL, g_hInst, NULL);
}

//LRESULT CALLBACK MemoryMapViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
//{
//    UNREFERENCED_PARAMETER(lParam);
//    switch (message)
//    {
//    case WM_DESTROY:
//        g_hwndMemoryMap = (HWND) INVALID_HANDLE_VALUE;  // We are closed! Bye-bye!..
//        return CallWindowProc(m_wndprocMemoryMapToolWindow, hWnd, message, wParam, lParam);
//    default:
//        return CallWindowProc(m_wndprocMemoryMapToolWindow, hWnd, message, wParam, lParam);
//    }
//    return (LRESULT)FALSE;
//}

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
    //TODO
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

void MemoryMapView_OnDraw(HDC hdc)
{
    ASSERT(g_pBoard != NULL);

    //TODO: Draw using bitmap
    for (int y = 0; y < 256; y += 1)
    {
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
            ::SetPixel(hdc, x, y, color1);
            ::SetPixel(hdc, x + 1, y, color2);
        }
    }
}


//////////////////////////////////////////////////////////////////////

