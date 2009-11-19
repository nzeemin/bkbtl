// ScreenView.cpp

#include "stdafx.h"
#include <mmintrin.h>
#include <vfw.h>
#include "BKBTL.h"
#include "Views.h"
#include "Emulator.h"


//////////////////////////////////////////////////////////////////////


HWND g_hwndScreen = NULL;  // Screen View window handle

HDRAWDIB m_hdd = NULL;
BITMAPINFO m_bmpinfo;
HBITMAP m_hbmp = NULL;
DWORD * m_bits = NULL;
int m_cxScreenWidth;
int m_cyScreenHeight;
BYTE m_ScreenKeyState[256];
ScreenViewMode m_ScreenMode = ColorScreen;
int m_ScreenHeightMode = 1;  // 1 - Normal height, 2 - Double height

void ScreenView_CreateDisplay();
void ScreenView_OnDraw(HDC hdc);
//BOOL ScreenView_OnKeyEvent(WPARAM vkey, BOOL okExtKey, BOOL okPressed);

const int KEYEVENT_QUEUE_SIZE = 32;
WORD m_ScreenKeyQueue[KEYEVENT_QUEUE_SIZE];
int m_ScreenKeyQueueTop = 0;
int m_ScreenKeyQueueBottom = 0;
int m_ScreenKeyQueueCount = 0;
void ScreenView_PutKeyEventToQueue(WORD keyevent);
WORD ScreenView_GetKeyEventFromQueue();


//////////////////////////////////////////////////////////////////////
// Colors

const DWORD ScreenView_ColorPalette[4] = {
    0x000000, 0x0000FF, 0x00FF00, 0xFF0000
};


//////////////////////////////////////////////////////////////////////



void ScreenView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= ScreenViewWndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= g_hInst;
    wcex.hIcon			= NULL;
    wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground	= NULL; //(HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName	= NULL;
    wcex.lpszClassName	= CLASSNAME_SCREENVIEW;
    wcex.hIconSm		= NULL;

    RegisterClassEx(&wcex);
}

void ScreenView_Init()
{
    m_hdd = DrawDibOpen();
    ScreenView_CreateDisplay();
}

void ScreenView_Done()
{
    if (m_hbmp != NULL)
    {
        DeleteObject(m_hbmp);
        m_hbmp = NULL;
    }

    DrawDibClose( m_hdd );
}

ScreenViewMode ScreenView_GetMode()
{
    return m_ScreenMode;
}
void ScreenView_SetMode(ScreenViewMode newMode)
{
    m_ScreenMode = newMode;
}

void ScreenView_CreateDisplay()
{
    ASSERT(g_hwnd != NULL);

	m_cxScreenWidth = BK_SCREEN_WIDTH;
	m_cyScreenHeight = BK_SCREEN_HEIGHT;

	HDC hdc = GetDC( g_hwnd );

	m_bmpinfo.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
	m_bmpinfo.bmiHeader.biWidth = BK_SCREEN_WIDTH;
	m_bmpinfo.bmiHeader.biHeight = BK_SCREEN_HEIGHT;
	m_bmpinfo.bmiHeader.biPlanes = 1;
	m_bmpinfo.bmiHeader.biBitCount = 32;
	m_bmpinfo.bmiHeader.biCompression = BI_RGB;
	m_bmpinfo.bmiHeader.biSizeImage = 0;
	m_bmpinfo.bmiHeader.biXPelsPerMeter = 0;
	m_bmpinfo.bmiHeader.biYPelsPerMeter = 0;
	m_bmpinfo.bmiHeader.biClrUsed = 0;
	m_bmpinfo.bmiHeader.biClrImportant = 0;
	
	m_hbmp = CreateDIBSection( hdc, &m_bmpinfo, DIB_RGB_COLORS, (void **) &m_bits, NULL, 0 );

	ReleaseDC( g_hwnd, hdc );
}

// Create Screen View as child of Main Window
void CreateScreenView(HWND hwndParent, int x, int y)
{
    ASSERT(hwndParent != NULL);

    int cxBorder = ::GetSystemMetrics(SM_CXBORDER);
    int cyBorder = ::GetSystemMetrics(SM_CYBORDER);
    int xLeft = x;
    int yTop = y;
    int cxWidth = 620;
    int cyScreenHeight = BK_SCREEN_HEIGHT * m_ScreenHeightMode;
    int cyHeight = cyScreenHeight;

    g_hwndScreen = CreateWindow(
            CLASSNAME_SCREENVIEW, NULL,
            WS_CHILD | WS_VISIBLE,
            xLeft, yTop, cxWidth, cyHeight,
            hwndParent, NULL, g_hInst, NULL);

    // Initialize m_ScreenKeyState
    VERIFY(::GetKeyboardState(m_ScreenKeyState));
}

LRESULT CALLBACK ScreenViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            ScreenView_PrepareScreen();  //DEBUG
            ScreenView_OnDraw(hdc);

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_LBUTTONDOWN:
        SetFocus(hWnd);
        break;
    //case WM_KEYDOWN:
    //    //if ((lParam & (1 << 30)) != 0)  // Auto-repeats should be ignored
    //    //    return (LRESULT) TRUE;
    //    //return (LRESULT) ScreenView_OnKeyEvent(wParam, (lParam & (1 << 24)) != 0, TRUE);
    //    return (LRESULT) TRUE;
    //case WM_KEYUP:
    //    //return (LRESULT) ScreenView_OnKeyEvent(wParam, (lParam & (1 << 24)) != 0, FALSE);
    //    return (LRESULT) TRUE;
    case WM_SETCURSOR:
        if (::GetFocus() == g_hwndScreen)
        {
            SetCursor(::LoadCursor(NULL, MAKEINTRESOURCE(IDC_IBEAM)));
            return (LRESULT) TRUE;
        }
        else
            return DefWindowProc(hWnd, message, wParam, lParam);
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

int ScreenView_GetHeightMode()
{
    return m_ScreenHeightMode;
}
void ScreenView_SetHeightMode(int newHeightMode)
{
    if (m_ScreenHeightMode == newHeightMode) return;

    m_ScreenHeightMode = newHeightMode;

    int cyHeight = BK_SCREEN_HEIGHT * m_ScreenHeightMode;
    RECT rc;  ::GetWindowRect(g_hwndScreen, &rc);
    ::SetWindowPos(g_hwndScreen, NULL, 0,0, rc.right - rc.left, cyHeight, SWP_NOZORDER | SWP_NOMOVE);
}

void ScreenView_OnDraw(HDC hdc)
{
    if (m_bits == NULL) return;

    RECT rc;  ::GetClientRect(g_hwndScreen, &rc);
    int x = (rc.right - BK_SCREEN_WIDTH) / 2;

    int dyDst = -1;
    if (m_ScreenHeightMode > 1) dyDst = BK_SCREEN_HEIGHT * m_ScreenHeightMode;

    DrawDibDraw(m_hdd, hdc,
        x, 0, -1, dyDst,
        &m_bmpinfo.bmiHeader, m_bits, 0,0,
        m_cxScreenWidth, m_cyScreenHeight,
        0);

    HGDIOBJ hOldBrush = ::SelectObject(hdc, ::GetSysColorBrush(COLOR_BTNFACE));
    PatBlt(hdc, 0,0, x,rc.bottom, PATCOPY);
    PatBlt(hdc, x + BK_SCREEN_WIDTH, 0, rc.right - x - BK_SCREEN_WIDTH, rc.bottom, PATCOPY);
    ::SelectObject(hdc, hOldBrush);
}

void ScreenView_RedrawScreen()
{
    ScreenView_PrepareScreen();

    HDC hdc = GetDC(g_hwndScreen);
    ScreenView_OnDraw(hdc);
    ::ReleaseDC(g_hwndScreen, hdc);
}

void ScreenView_PrepareScreen()
{
    if (m_bits == NULL) return;

    // Get scroll value
    WORD scroll = g_pBoard->GetPortView(0177664);
    BOOL okSmallScreen = ((scroll & 01000) == 0);
    scroll &= 0377;
    scroll = (scroll >= 0330) ? scroll - 0330 : 050 + scroll;

    // Render to bitmap
    for (int y = 0; y < 256; y++)
    {
        int yy = (y + scroll) & 0377;
        WORD addressBits = 040000 + yy * 0100;
        DWORD* pBits = m_bits + (256 - 1 - y) * 512;
        for (int x = 0; x < 512 / 16; x++)
        {
            WORD src = g_pBoard->GetRAMWord(addressBits);

            if (m_ScreenMode == BlackWhiteScreen)  // Black and white mode 512 x 256
            {
                for (int bit = 0; bit < 16; bit++)
                {
                    DWORD color = (src & 1) ? 0x0ffffff : 0;
                    *pBits = color;
                    pBits++;
                    src = src >> 1;
                }
            }
            else  // Color mode 256 x 256
            {
                for (int bit = 0; bit < 16; bit += 2)
                {
                    DWORD color = ScreenView_ColorPalette[src & 3];
                    *pBits = color;
                    pBits++;
                    *pBits = color;
                    pBits++;
                    src = src >> 2;
                }
            }

            addressBits += 2;
        }
    }
}

void ScreenView_PutKeyEventToQueue(WORD keyevent)
{
    if (m_ScreenKeyQueueCount == KEYEVENT_QUEUE_SIZE) return;  // Full queue

    m_ScreenKeyQueue[m_ScreenKeyQueueTop] = keyevent;
    m_ScreenKeyQueueTop++;
    if (m_ScreenKeyQueueTop >= KEYEVENT_QUEUE_SIZE)
        m_ScreenKeyQueueTop = 0;
    m_ScreenKeyQueueCount++;
}
WORD ScreenView_GetKeyEventFromQueue()
{
    if (m_ScreenKeyQueueCount == 0) return 0;  // Empty queue

    WORD keyevent = m_ScreenKeyQueue[m_ScreenKeyQueueBottom];
    m_ScreenKeyQueueBottom++;
    if (m_ScreenKeyQueueBottom >= KEYEVENT_QUEUE_SIZE)
        m_ScreenKeyQueueBottom = 0;
    m_ScreenKeyQueueCount--;

    return keyevent;
}

// Ins = ВС, Tab = ТАБ
const BYTE arrPcscan2Bkscan[256] = {
/*       0     1     2     3     4     5     6     7     8     9     a     b     c     d     e     f  */
/*0*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0030, 0015, 0000, 0000, 0000, 0012, 0000, 0000, 
/*1*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
/*2*/    0040, 0000, 0000, 0000, 0000, 0010, 0032, 0031, 0033, 0000, 0000, 0000, 0000, 0023, 0000, 0000, 
/*3*/    0060, 0061, 0062, 0063, 0064, 0065, 0066, 0067, 0070, 0071, 0000, 0000, 0000, 0000, 0000, 0000, 
/*4*/    0000, 0146, 0151, 0163, 0167, 0165, 0141, 0160, 0162, 0173, 0157, 0154, 0144, 0170, 0164, 0175, 
/*5*/    0172, 0152, 0153, 0171, 0145, 0147, 0155, 0143, 0176, 0156, 0161, 0000, 0000, 0000, 0000, 0000, 
/*6*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
/*7*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
/*8*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
/*9*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
/*a*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
/*b*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0166, 0000, 0142, 0055, 0140, 0000, 
/*c*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
/*d*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0150, 0000, 0000, 0174, 0000, 
/*e*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
/*f*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
};

void ScreenView_ScanKeyboard()
{
    if (! g_okEmulatorRunning) return;
    if (::GetFocus() == g_hwndScreen)
    {
        // Read current keyboard state
        BYTE keys[256];
        VERIFY(::GetKeyboardState(keys));

        // Check every key for state change
        for (int scan = 0; scan < 256; scan++)
        {
            BYTE newstate = keys[scan];
            BYTE oldstate = m_ScreenKeyState[scan];
            if ((newstate & 128) != 0 && (oldstate & 128) == 0)  // Key pressed
            {
                BYTE pcscan = (BYTE) scan;
#if !defined(PRODUCT)
                DebugPrintFormat(_T("Key PC: 0x%0x\r\n"), scan);
#endif
                //TODO: Выбирать таблицу маппинга в зависимости от флага РУС/ЛАТ в БК
                BYTE bkscan = arrPcscan2Bkscan[pcscan]; //TranslateVkeyToUkncScan(pcscan, FALSE, FALSE);
                if (bkscan != 0)
                {
                    BYTE pressed = newstate & 128;
                    WORD keyevent = MAKEWORD(bkscan, pressed);
                    ScreenView_PutKeyEventToQueue(keyevent);
                }
            }
        }

        // Save keyboard state
        ::CopyMemory(m_ScreenKeyState, keys, 256);
    }

    // Process next event in the keyboard queue
    WORD keyevent = ScreenView_GetKeyEventFromQueue();
    if (keyevent != 0)
    {
        BOOL pressed = ((keyevent & 0x8000) != 0);
        BYTE bkscan = LOBYTE(keyevent);

#if !defined(PRODUCT)
        TCHAR bufoct[7];  PrintOctalValue(bufoct, bkscan);
        DebugPrintFormat(_T("KeyEvent: %s %d\r\n"), bufoct, pressed);
#endif

        g_pBoard->KeyboardEvent(bkscan, pressed);
    }
}

// External key event - e.g. from KeyboardView
void ScreenView_KeyEvent(BYTE keyscan, BOOL pressed)
{
    ScreenView_PutKeyEventToQueue(MAKEWORD(keyscan, pressed ? 128 : 0));
}

void ScreenView_SaveScreenshot(LPCTSTR sFileName)
{
    ASSERT(sFileName != NULL);
    ASSERT(m_bits != NULL);

    // Create file
    HANDLE hFile = ::CreateFile(sFileName,
            GENERIC_WRITE, FILE_SHARE_READ, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    //TODO: Check if hFile == INVALID_HANDLE_VALUE

    BITMAPFILEHEADER hdr;
    ::ZeroMemory(&hdr, sizeof(hdr));
    hdr.bfType = 0x4d42;  // "BM"
    BITMAPINFOHEADER bih;
    ::ZeroMemory(&bih, sizeof(bih));
	bih.biSize = sizeof( BITMAPINFOHEADER );
	bih.biWidth = m_cxScreenWidth;
	bih.biHeight = m_cyScreenHeight;
    bih.biSizeImage = bih.biWidth * bih.biHeight * 4;
	bih.biPlanes = 1;
	bih.biBitCount = 32;
	bih.biCompression = BI_RGB;
    bih.biXPelsPerMeter = bih.biXPelsPerMeter = 2000;
    hdr.bfSize = (DWORD) sizeof(BITMAPFILEHEADER) + bih.biSize + bih.biSizeImage;
    hdr.bfOffBits = (DWORD) sizeof(BITMAPFILEHEADER) + bih.biSize;

    DWORD dwBytesWritten = 0;

    WriteFile(hFile, &hdr, sizeof(BITMAPFILEHEADER), &dwBytesWritten, NULL);
    //TODO: Check if dwBytesWritten != sizeof(BITMAPFILEHEADER)
    WriteFile(hFile, &bih, sizeof(BITMAPINFOHEADER), &dwBytesWritten, NULL);
    //TODO: Check if dwBytesWritten != sizeof(BITMAPINFOHEADER)
    WriteFile(hFile, m_bits, bih.biSizeImage, &dwBytesWritten, NULL);
    //TODO: Check if dwBytesWritten != bih.biSizeImage

    // Close file
    CloseHandle(hFile);
}


//////////////////////////////////////////////////////////////////////
