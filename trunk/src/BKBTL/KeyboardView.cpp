/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// KeyboardView.cpp

#include "stdafx.h"
#include "BKBTL.h"
#include "Views.h"
#include "Emulator.h"


//////////////////////////////////////////////////////////////////////

#define COLOR_BK_BACKGROUND RGB(115,115,115)


HWND g_hwndKeyboard = (HWND) INVALID_HANDLE_VALUE;  // Keyboard View window handle

int m_nKeyboardBitmapLeft = 0;
int m_nKeyboardBitmapTop = 0;
BYTE m_nKeyboardKeyPressed = 0;  // BK scan-code for the key pressed, or 0

void KeyboardView_OnDraw(HDC hdc);
int KeyboardView_GetKeyByPoint(int x, int y);
void Keyboard_DrawKey(HDC hdc, BYTE keyscan);


//////////////////////////////////////////////////////////////////////

const int KEYBOARD_KEYS_ARRAY_WIDTH = 5;
// Keyboard key mapping to bitmap
const WORD m_arrKeyboardKeys[] = {
/*   x1,y1    w,h      code  AR2code  */
      3,  3, 51,34,    BK_KEY_REPEAT,  // ÏÎÂÒ
     56,  3, 51,34,    0003,  // ÊÒ
    109,  3, 51,34,    0000,  // Arrow right    =|=>|
    162,  3, 51,34,    0026,  // Arrow left     |<===
    215,  3, 51,34,    0027,  // Arrow right    |===>
    268,  3, 51,34,    0000,  // ÈÍÄ ÑÓ
    321,  3, 51,34,    0000,  // ÁËÎÊ ÐÅÄ
    374,  3, 50,34,    0000,  // STEP
    426,  3, 51,34,    0014,  // ÑÁÐ
    478,  3, 70,34,    BK_KEY_STOP,  // STOP

      3,38,  34,34,    BK_KEY_BACKSHIFT,  // Big Arrow Down
     38,38,  34,34,    0073,  // ; +
     74,38,  34,34,    0061,  // 1 !
    109,38,  34,34,    0062,  // 2 "
    144,38,  34,34,    0063,  // 3 #
    179,38,  34,34,    0064,  // 4 $
    214,38,  35,34,    0065,  // 5 %
    250,38,  34,34,    0066,  // 6 &
    285,38,  34,34,    0067,  // 7 '
    320,38,  34,34,    0070,  // 8 (
    355,38,  34,34,    0071,  // 9 )
    391,38,  34,34,    0060,  // 0 {
    426,38,  34,34,    0055,  // - =
    461,38,  34,34,    0057,  // / ?
    498,38,  50,34,    0030,  // Backspace

      4,73,  50,34,    0015,  // TAB
     56,73,  34,34,    0112,  // É J
     91,73,  34,34,    0103,  // Ö C
    126,73,  35,34,    0125,  // Ó U
    162,73,  34,34,    0113,  // Ê K
    197,73,  34,34,    0105,  // Å E
    232,73,  34,34,    0116,  // Í N
    267,73,  34,34,    0107,  // Ã G
    302,73,  35,34,    0133,  // Ø [
    338,73,  34,34,    0135,  // Ù ]
    373,73,  34,34,    0132,  // Ç Z
    408,73,  34,34,    0110,  // Õ H
    443,73,  35,34,    0072,  // : *
    479,73,  34,34,    0137,  // Ú }
    514,73,  34,34,    0023, // ÂÑ

     12,109, 50,34,    0000,  // ÑÓ
     64,109, 34,34,    0106,  // Ô F
     99,109, 35,34,    0131,  // Û Y
    135,109, 34,34,    0127,  // Â W
    170,109, 34,34,    0101,  // À A
    205,109, 34,34,    0120,  // Ï P
    240,109, 34,34,    0122,  // Ð R
    275,109, 35,34,    0117,  // Î O
    311,109, 34,34,    0114,  // Ë L
    346,109, 34,34,    0104,  // Ä D
    381,109, 34,34,    0126,  // Æ V
    416,109, 34,34,    0134,  // Ý Backslash
    452,109, 34,34,    0076,  // . >
    488,109, 50,34,    0012,  // ENTER

     12,144, 34,34,    BK_KEY_LOWER,  // ÑÒÐ
     47,144, 34,34,    BK_KEY_UPPER,  // ÇÀÃË
     82,144, 34,34,    0121,  // ß Q
    117,144, 34,34,    0136,  // × ^
    152,144, 34,34,    0123,  // Ñ S
    187,144, 35,34,    0115,  // Ì M
    223,144, 34,34,    0111,  // È I
    258,144, 34,34,    0124,  // Ò T
    293,144, 34,34,    0130,  // Ü X
    328,144, 34,34,    0102,  // Á B
    363,144, 35,34,    0100,  // Þ @
    399,144, 34,34,    0074,  // , <

     12,179, 50,34,    0016,  // RUS
     64,179, 35,34,    BK_KEY_AR2,  // AR2
     99,179,281,34,    0040,  // Space bar
    382,179, 50,34,    0017,  // LAT

    434,144, 34,69,    0010,  // Left
    469,144, 35,34,    0032,  // Up
    469,179, 35,34,    0033,  // Down
    505,144, 34,69,    0031,  // Right
};
const int m_nKeyboardKeysCount = sizeof(m_arrKeyboardKeys) / sizeof(WORD) / KEYBOARD_KEYS_ARRAY_WIDTH;

//////////////////////////////////////////////////////////////////////


void KeyboardView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= KeyboardViewWndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= g_hInst;
    wcex.hIcon			= NULL;
    wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground	= (HBRUSH)(COLOR_BTNFACE+1);
    wcex.lpszMenuName	= NULL;
    wcex.lpszClassName	= CLASSNAME_KEYBOARDVIEW;
    wcex.hIconSm		= NULL;

    RegisterClassEx(&wcex);
}

void KeyboardView_Init()
{
}

void KeyboardView_Done()
{
}

void CreateKeyboardView(HWND hwndParent, int x, int y, int width, int height)
{
    ASSERT(hwndParent != NULL);

    g_hwndKeyboard = CreateWindow(
        CLASSNAME_KEYBOARDVIEW, NULL,
        WS_CHILD | WS_VISIBLE,
        x, y, width, height,
        hwndParent, NULL, g_hInst, NULL);
}

LRESULT CALLBACK KeyboardViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            KeyboardView_OnDraw(hdc);

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_SETCURSOR:
        {
            POINT ptCursor;  ::GetCursorPos(&ptCursor);
            ::ScreenToClient(g_hwndKeyboard, &ptCursor);
            int keyindex = KeyboardView_GetKeyByPoint(ptCursor.x, ptCursor.y);
            LPCTSTR cursor = (keyindex == -1) ? IDC_ARROW : IDC_HAND;
            ::SetCursor(::LoadCursor(NULL, cursor));
        }
        return (LRESULT)TRUE;
    case WM_LBUTTONDOWN:
        {
            int x = LOWORD(lParam); 
            int y = HIWORD(lParam);
            WORD fwkeys = wParam;

            int keyindex = KeyboardView_GetKeyByPoint(x, y);
            if (keyindex == -1) break;
            BYTE keyscan = (BYTE) m_arrKeyboardKeys[keyindex * KEYBOARD_KEYS_ARRAY_WIDTH + 4];
            if (keyscan == 0) break;

            BOOL okShift = ((fwkeys & MK_SHIFT) != 0);
            if (okShift && keyscan >= 0100 && keyscan <= 0137)
                keyscan += 040;
            else if (okShift && keyscan >= 0060 && keyscan <= 0077)
                keyscan -= 020;

            // Fire keydown event and capture mouse
            ScreenView_KeyEvent(keyscan, TRUE);
            ::SetCapture(g_hwndKeyboard);

            // Draw focus frame for the key pressed
            HDC hdc = ::GetDC(g_hwndKeyboard);
            Keyboard_DrawKey(hdc, keyscan);
            ::ReleaseDC(g_hwndKeyboard, hdc);

            // Remember key pressed
            m_nKeyboardKeyPressed = keyscan;
        }
        break;
    case WM_LBUTTONUP:
        if (m_nKeyboardKeyPressed != 0)
        {
            // Fire keyup event and release mouse
            ScreenView_KeyEvent(m_nKeyboardKeyPressed, FALSE);
            ::ReleaseCapture();

            // Draw focus frame for the released key
            HDC hdc = ::GetDC(g_hwndKeyboard);
            Keyboard_DrawKey(hdc, m_nKeyboardKeyPressed);
            ::ReleaseDC(g_hwndKeyboard, hdc);

            m_nKeyboardKeyPressed = 0;
        }
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

void KeyboardView_OnDraw(HDC hdc)
{
    RECT rc;  ::GetClientRect(g_hwndKeyboard, &rc);

    // Keyboard background
    HBRUSH hBkBrush = ::CreateSolidBrush(COLOR_BK_BACKGROUND);
    HGDIOBJ hOldBrush = ::SelectObject(hdc, hBkBrush);
    ::PatBlt(hdc, 0, 0, rc.right, rc.bottom, PATCOPY);
    ::SelectObject(hdc, hOldBrush);
    ::DeleteObject(hBkBrush);

    HBITMAP hBmp = ::LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_KEYBOARD));
    HDC hdcMem = ::CreateCompatibleDC(hdc);
    HGDIOBJ hOldBitmap = ::SelectObject(hdcMem, hBmp);

    BITMAP bitmap;
    VERIFY(::GetObject(hBmp, sizeof(BITMAP), &bitmap));
    int cxBitmap = (int) bitmap.bmWidth;
    int cyBitmap = (int) bitmap.bmHeight;
    m_nKeyboardBitmapLeft = (rc.right - cxBitmap) / 2;
    m_nKeyboardBitmapTop = (rc.bottom - cyBitmap) / 2;
    ::BitBlt(hdc, m_nKeyboardBitmapLeft, m_nKeyboardBitmapTop, cxBitmap,cyBitmap, hdcMem, 0,0, SRCCOPY);

    ::SelectObject(hdcMem, hOldBitmap);
    ::DeleteDC(hdcMem);
    ::DeleteObject(hBmp);

    if (m_nKeyboardKeyPressed != 0)
        Keyboard_DrawKey(hdc, m_nKeyboardKeyPressed);

    ////DEBUG: Show key mappings
    //for (int i = 0; i < m_nKeyboardKeysCount; i++)
    //{
    //    RECT rcKey;
    //    rcKey.left = m_nKeyboardBitmapLeft + m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH];
    //    rcKey.top = m_nKeyboardBitmapTop + m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH + 1];
    //    rcKey.right = rcKey.left + m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH + 2];
    //    rcKey.bottom = rcKey.top + m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH + 3];

    //    ::DrawFocusRect(hdc, &rcKey);
    //}
}

// Returns: index of key under the cursor position, or -1 if not found
int KeyboardView_GetKeyByPoint(int x, int y)
{
    for (int i = 0; i < m_nKeyboardKeysCount; i++)
    {
        RECT rcKey;
        rcKey.left = m_nKeyboardBitmapLeft + m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH];
        rcKey.top = m_nKeyboardBitmapTop + m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH + 1];
        rcKey.right = rcKey.left + m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH + 2];
        rcKey.bottom = rcKey.top + m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH + 3];

        if (x >= rcKey.left && x < rcKey.right && y >= rcKey.top && y < rcKey.bottom)
        {
            return i;
        }
    }
    return -1;
}

void Keyboard_DrawKey(HDC hdc, BYTE keyscan)
{
    for (int i = 0; i < m_nKeyboardKeysCount; i++)
        if (keyscan == m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH + 4])
        {
            RECT rcKey;
            rcKey.left = m_nKeyboardBitmapLeft + m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH];
            rcKey.top = m_nKeyboardBitmapTop + m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH + 1];
            rcKey.right = rcKey.left + m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH + 2];
            rcKey.bottom = rcKey.top + m_arrKeyboardKeys[i * KEYBOARD_KEYS_ARRAY_WIDTH + 3];
            ::DrawFocusRect(hdc, &rcKey);
        }
}


//////////////////////////////////////////////////////////////////////
