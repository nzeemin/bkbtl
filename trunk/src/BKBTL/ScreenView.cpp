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
ScreenViewMode m_ScreenMode = RGBScreen;
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

/*
yrgb  R   G   B  0xRRGGBB
0000 000 000 000 0x000000
0001 000 000 128 0x000080
0010 000 128 000 0x008000
0011 000 128 128 0x008080
0100 128 000 000 0x800000
0101 128 000 128 0x800080
0110 128 128 000 0x808000
0111 128 128 128 0x808080
1000 000 000 000 0x000000
1001 000 000 255 0x0000FF
1010 000 255 000 0x00FF00
1011 000 255 255 0x00FFFF
1100 255 000 000 0xFF0000
1101 255 000 255 0xFF00FF
1110 255 255 000 0xFFFF00
1111 255 255 255 0xFFFFFF
*/

// Table for color conversion yrgb (4 bits) -> DWORD (32 bits)
const DWORD ScreenView_StandardRGBColors[16] = {
    0x000000, 0x000080, 0x008000, 0x008080, 0x800000, 0x800080, 0x808000, 0x808080,
    0x000000, 0x0000FF, 0x00FF00, 0x00FFFF, 0xFF0000, 0xFF00FF, 0xFFFF00, 0xFFFFFF,
};
const DWORD ScreenView_StandardGRBColors[16] = {
    0x000000, 0x000080, 0x800000, 0x800080, 0x008000, 0x008080, 0x808000, 0x808080,
    0x000000, 0x0000FF, 0xFF0000, 0xFF00FF, 0x00FF00, 0x00FFFF, 0xFFFF00, 0xFFFFFF,
};
// Table for color conversion, gray (black and white) display
const DWORD ScreenView_GrayColors[16] = {
    0x000000, 0x242424, 0x484848, 0x6C6C6C, 0x909090, 0xB4B4B4, 0xD8D8D8, 0xFFFFFF,
    0x000000, 0x242424, 0x484848, 0x6C6C6C, 0x909090, 0xB4B4B4, 0xD8D8D8, 0xFFFFFF,
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

    //NOTE: Реализован только черно-белый видеорежим 512 x 256
    WORD addressBits = 040000;
    for (int y = 0; y < 256; y++)
    {
        DWORD* pBits = m_bits + (256 - 1 - y) * 512;
        for (int x = 0; x < 512 / 16; x++)
        {
            WORD src = g_pBoard->GetRAMWord(addressBits);
            for (int bit = 0; bit < 16; bit++)
            {
                DWORD color = (src & 1) ? 0x0ffffff : 0;
                *pBits = color;
                pBits++;
                src = src >> 1;
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

const BYTE arrPcscan2Ukncscan[256] = {
/*       0     1     2     3     4     5     6     7     8     9     a     b     c     d     e     f  */
/*0*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0132, 0026, 0000, 0000, 0000, 0153, 0000, 0000, 
/*1*/    0000, 0000, 0000, 0004, 0107, 0000, 0000, 0000, 0000, 0000, 0000, 0006, 0000, 0000, 0000, 0000, 
/*2*/    0113, 0004, 0151, 0172, 0000, 0116, 0154, 0133, 0134, 0000, 0000, 0000, 0000, 0171, 0152, 0000, 
/*3*/    0176, 0030, 0031, 0032, 0013, 0034, 0035, 0016, 0017, 0177, 0000, 0000, 0000, 0000, 0000, 0000, 
/*4*/    0000, 0072, 0076, 0050, 0057, 0033, 0047, 0055, 0156, 0073, 0027, 0052, 0056, 0112, 0054, 0075, 
/*5*/    0053, 0067, 0074, 0111, 0114, 0051, 0137, 0071, 0115, 0070, 0157, 0000, 0000, 0000, 0000, 0000, 
/*6*/    0126, 0127, 0147, 0167, 0130, 0150, 0170, 0125, 0145, 0165, 0025, 0155, 0000, 0005, 0146, 0131, 
/*7*/    0010, 0011, 0012, 0014, 0015, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
/*8*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
/*9*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
/*a*/    0105, 0106, 0046, 0066, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
/*b*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0173, 0117, 0175, 0135, 0117, 
/*c*/    0007, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
/*d*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0110, 0135, 0174, 0000, 0000, 
/*e*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
/*f*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
};

void ScreenView_ScanKeyboard()
{
    if (! g_okEmulatorRunning) return;
    if (::GetFocus() != g_hwndScreen) return;

    // Read current keyboard state
    BYTE keys[256];
    VERIFY(::GetKeyboardState(keys));

    // Check every key for state change
    for (int scan = 0; scan < 256; scan++)
    {
        BYTE newstate = keys[scan];
        BYTE oldstate = m_ScreenKeyState[scan];
        if ((newstate & 128) != (oldstate & 128))  // Key state changed - key pressed or released
        {
            BYTE pcscan = (BYTE) scan;
            //TODO: Выбирать таблицу маппинга в зависимости от флага РУС/ЛАТ в УКНЦ
            BYTE ukncscan = arrPcscan2Ukncscan[pcscan]; //TranslateVkeyToUkncScan(pcscan, FALSE, FALSE);
            if (ukncscan != 0)
            {
                BYTE pressed = newstate & 128;
                WORD keyevent = MAKEWORD(ukncscan, pressed);
                ScreenView_PutKeyEventToQueue(keyevent);
            }

#if !defined(PRODUCT)
                TCHAR bufoct[7];  PrintOctalValue(bufoct, ukncscan);
                DebugPrintFormat(_T("KeyEvent: pc:0x%02x bk:%s %x\r\n"), scan, bufoct, (newstate & 128) != 0);
#endif
        }
    }

    // Save keyboard state
    ::CopyMemory(m_ScreenKeyState, keys, 256);

    // Process next event in the keyboard queue
    WORD keyevent = ScreenView_GetKeyEventFromQueue();
    if (keyevent != 0)
    {
        BOOL pressed = ((keyevent & 0x8000) != 0);
        BYTE ukncscan = LOBYTE(keyevent);
        g_pBoard->KeyboardEvent(ukncscan, pressed);
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
