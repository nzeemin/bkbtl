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
#include "Main.h"
#include "Views.h"
#include "Emulator.h"
#include "util/BitmapFile.h"

//////////////////////////////////////////////////////////////////////


#define COLOR_BK_BACKGROUND RGB(115,115,115)


HWND g_hwndKeyboard = (HWND)INVALID_HANDLE_VALUE;  // Keyboard View window handle

int m_nKeyboardBitmapLeft = 0;
int m_nKeyboardBitmapTop = 0;
BYTE m_nKeyboardKeyPressed = 0;  // BK scan-code for the key pressed, or 0

void KeyboardView_OnDraw(HDC hdc);
int KeyboardView_GetKeyByPoint(int x, int y);
void Keyboard_DrawKey(HDC hdc, BYTE keyscan);


//////////////////////////////////////////////////////////////////////


struct KeyboardKey
{
    int x, y, w, h;
    uint16_t code;
};

// Keyboard key mapping to bitmap for BK0010-01 keyboard
const KeyboardKey m_arrKeyboardKeys1[] =
{
    /*  x,   y    w,  h  code */
    {   3,   3,  51, 34, BK_KEY_REPEAT }, // ПОВТ
    {  56,   3,  51, 34, 0003 }, // КТ
    { 109,   3,  51, 34, 0213 }, // Arrow right    =|=>|
    { 162,   3,  51, 34, 0026 }, // Arrow left     |<===
    { 215,   3,  51, 34, 0027 }, // Arrow right    |===>
    { 268,   3,  51, 34, 0202 }, // ИНД СУ
    { 321,   3,  51, 34, 0204 }, // БЛОК РЕД
    { 374,   3,  50, 34, 0200 }, // STEP
    { 426,   3,  51, 34, 0014 }, // СБР
    { 478,   3,  70, 34, BK_KEY_STOP }, // STOP

    {   3,  38,  34, 34, BK_KEY_BACKSHIFT }, // Big Arrow Down
    {  38,  38,  34, 34, 0073 }, // ; +
    {  74,  38,  34, 34, 0061 }, // 1 !
    { 109,  38,  34, 34, 0062 }, // 2 "
    { 144,  38,  34, 34, 0063 }, // 3 #
    { 179,  38,  34, 34, 0064 }, // 4 $
    { 214,  38,  35, 34, 0065 }, // 5 %
    { 250,  38,  34, 34, 0066 }, // 6 &
    { 285,  38,  34, 34, 0067 }, // 7 '
    { 320,  38,  34, 34, 0070 }, // 8 (
    { 355,  38,  34, 34, 0071 }, // 9 )
    { 391,  38,  34, 34, 0060 }, // 0 {
    { 426,  38,  34, 34, 0055 }, // - =
    { 461,  38,  34, 34, 0057 }, // / ?
    { 498,  38,  50, 34, 0030 }, // Backspace

    {   4,  73,  50, 34, 0015 }, // TAB
    {  56,  73,  34, 34, 0112 }, // Й J
    {  91,  73,  34, 34, 0103 }, // Ц C
    { 126,  73,  35, 34, 0125 }, // У U
    { 162,  73,  34, 34, 0113 }, // К K
    { 197,  73,  34, 34, 0105 }, // Е E
    { 232,  73,  34, 34, 0116 }, // Н N
    { 267,  73,  34, 34, 0107 }, // Г G
    { 302,  73,  35, 34, 0133 }, // Ш [
    { 338,  73,  34, 34, 0135 }, // Щ ]
    { 373,  73,  34, 34, 0132 }, // З Z
    { 408,  73,  34, 34, 0110 }, // Х H
    { 443,  73,  35, 34, 0072 }, // : *
    { 479,  73,  34, 34, 0137 }, // Ъ }
    { 514,  73,  34, 34, 0023 }, // ВС

    {  12, 109,  50, 34, 0000 }, // СУ
    {  64, 109,  34, 34, 0106 }, // Ф F
    {  99, 109,  35, 34, 0131 }, // Ы Y
    { 135, 109,  34, 34, 0127 }, // В W
    { 170, 109,  34, 34, 0101 }, // А A
    { 205, 109,  34, 34, 0120 }, // П P
    { 240, 109,  34, 34, 0122 }, // Р R
    { 275, 109,  35, 34, 0117 }, // О O
    { 311, 109,  34, 34, 0114 }, // Л L
    { 346, 109,  34, 34, 0104 }, // Д D
    { 381, 109,  34, 34, 0126 }, // Ж V
    { 416, 109,  34, 34, 0134 }, // Э Backslash
    { 452, 109,  34, 34, 0056 }, // . >
    { 488, 109,  50, 34, 0012 }, // ENTER

    {  12, 144,  34, 34, BK_KEY_LOWER }, // СТР
    {  47, 144,  34, 34, BK_KEY_UPPER }, // ЗАГЛ
    {  82, 144,  34, 34, 0121 }, // Я Q
    { 117, 144,  34, 34, 0136 }, // Ч ^
    { 152, 144,  34, 34, 0123 }, // С S
    { 187, 144,  35, 34, 0115 }, // М M
    { 223, 144,  34, 34, 0111 }, // И I
    { 258, 144,  34, 34, 0124 }, // Т T
    { 293, 144,  34, 34, 0130 }, // Ь X
    { 328, 144,  34, 34, 0102 }, // Б B
    { 363, 144,  35, 34, 0100 }, // Ю @
    { 399, 144,  34, 34, 0054 }, // , <

    {  12, 179,  50, 34, 0016 }, // RUS
    {  64, 179,  35, 34, BK_KEY_AR2 }, // AR2
    {  99, 179, 281, 34, 0040 }, // Space bar
    { 382, 179,  50, 34, 0017 }, // LAT

    { 434, 144,  34, 69, 0010 }, // Left
    { 469, 144,  35, 34, 0032 }, // Up
    { 469, 179,  35, 34, 0033 }, // Down
    { 505, 144,  34, 69, 0031 }, // Right
};
const int m_nKeyboardKeys1Count = sizeof(m_arrKeyboardKeys1) / sizeof(KeyboardKey);

// Keyboard key mapping to bitmap for BK0011M keyboard
const KeyboardKey m_arrKeyboardKeys2[] =
{
    /*  x,   y    w,  h  code */
    {   3,   2,  51, 34, BK_KEY_REPEAT }, // ПОВТ
    {  56,   2,  51, 34, 0003 }, // КТ
    { 109,   2,  51, 34, 0213 }, // Arrow right    =|=>|
    { 162,   2,  51, 34, 0026 }, // Arrow left     |<===
    { 215,   2,  51, 34, 0027 }, // Arrow right    |===>
    { 268,   2,  51, 34, 0202 }, // ИНД СУ
    { 321,   2,  51, 34, 0204 }, // БЛОК РЕД
    { 374,   2,  50, 34, 0200 }, // STEP
    { 426,   2,  60, 34, 0014 }, // СБР
    { 489,   2,  60, 34, BK_KEY_STOP }, // STOP

    {   3,  38,  34, 34, BK_KEY_BACKSHIFT }, // Big Arrow Down
    {  38,  38,  34, 34, 0073 }, // ; +
    {  73,  38,  34, 34, 0061 }, // 1 !
    { 109,  38,  34, 34, 0062 }, // 2 "
    { 144,  38,  34, 34, 0063 }, // 3 #
    { 179,  38,  34, 34, 0064 }, // 4 $
    { 214,  38,  35, 34, 0065 }, // 5 %
    { 250,  38,  34, 34, 0066 }, // 6 &
    { 285,  38,  34, 34, 0067 }, // 7 '
    { 320,  38,  34, 34, 0070 }, // 8 (
    { 356,  38,  34, 34, 0071 }, // 9 )
    { 391,  38,  34, 34, 0060 }, // 0 {
    { 427,  38,  34, 34, 0055 }, // - =
    { 462,  38,  34, 34, 0072 }, // : *
    { 498,  38,  50, 34, 0030 }, // Backspace

    {   4,  73,  50, 34, 0015 }, // TAB
    {  55,  73,  34, 34, 0112 }, // Й J
    {  90,  73,  34, 34, 0103 }, // Ц C
    { 126,  73,  34, 34, 0125 }, // У U
    { 161,  73,  34, 34, 0113 }, // К K
    { 197,  73,  34, 34, 0105 }, // Е E
    { 232,  73,  34, 34, 0116 }, // Н N
    { 267,  73,  34, 34, 0107 }, // Г G
    { 302,  73,  34, 34, 0133 }, // Ш [
    { 338,  73,  34, 34, 0135 }, // Щ ]
    { 373,  73,  34, 34, 0132 }, // З Z
    { 409,  73,  34, 34, 0110 }, // Х H
    { 444,  73,  34, 34, 0137 }, // Ъ }
    { 480,  73,  34, 34, 0057 }, // / ?
    { 514,  73,  34, 34, 0023 }, // ВС

    {   2, 109,  60, 34, 0000 }, // СУ
    {  64, 109,  34, 34, 0106 }, // Ф F
    {  99, 109,  35, 34, 0131 }, // Ы Y
    { 135, 109,  34, 34, 0127 }, // В W
    { 170, 109,  34, 34, 0101 }, // А A
    { 205, 109,  34, 34, 0120 }, // П P
    { 240, 109,  34, 34, 0122 }, // Р R
    { 276, 109,  34, 34, 0117 }, // О O
    { 311, 109,  34, 34, 0114 }, // Л L
    { 347, 109,  34, 34, 0104 }, // Д D
    { 382, 109,  34, 34, 0126 }, // Ж V
    { 418, 109,  34, 34, 0134 }, // Э Backslash
    { 453, 109,  34, 34, 0056 }, // . >
    { 488, 109,  60, 34, 0012 }, // ENTER

    {   2, 144,  44, 34, BK_KEY_UPPER }, // ЗАГЛ
    {  47, 144,  34, 34, BK_KEY_LOWER }, // СТР
    {  82, 144,  34, 34, 0121 }, // Я Q
    { 117, 144,  34, 34, 0136 }, // Ч ^
    { 152, 144,  34, 34, 0123 }, // С S
    { 188, 144,  34, 34, 0115 }, // М M
    { 223, 144,  34, 34, 0111 }, // И I
    { 258, 144,  34, 34, 0124 }, // Т T
    { 294, 144,  34, 34, 0130 }, // Ь X
    { 329, 144,  34, 34, 0102 }, // Б B
    { 365, 144,  34, 34, 0100 }, // Ю @
    { 400, 144,  43, 34, 0054 }, // , <

    {   2, 179,  69, 34, 0016 }, // RUS
    {  72, 179,  44, 34, BK_KEY_AR2 }, // AR2
    { 117, 180, 256, 34, 0040 }, // Space bar
    { 374, 179,  69, 34, 0017 }, // LAT

    { 444, 144,  34, 69, 0010 }, // Left
    { 479, 144,  35, 34, 0032 }, // Up
    { 479, 179,  35, 34, 0033 }, // Down
    { 515, 144,  34, 69, 0031 }, // Right
};
const int m_nKeyboardKeys2Count = sizeof(m_arrKeyboardKeys2) / sizeof(KeyboardKey);

const KeyboardKey* m_arrKeyboardKeys = m_arrKeyboardKeys1;
int m_nKeyboardKeysCount = m_nKeyboardKeys1Count;


//////////////////////////////////////////////////////////////////////


void KeyboardView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = KeyboardViewWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = g_hInst;
    wcex.hIcon          = NULL;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = CLASSNAME_KEYBOARDVIEW;
    wcex.hIconSm        = NULL;

    RegisterClassEx(&wcex);
}

void KeyboardView_Init()
{
}

void KeyboardView_Done()
{
}

void KeyboardView_Update()
{
    ::InvalidateRect(g_hwndKeyboard, NULL, TRUE);
}

void KeyboardView_Create(HWND hwndParent, int x, int y, int width, int height)
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
            WORD fwkeys = (WORD)wParam;

            int keyindex = KeyboardView_GetKeyByPoint(x, y);
            if (keyindex == -1) break;
            BYTE keyscan = (BYTE) m_arrKeyboardKeys[keyindex].code;
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
            VERIFY(::ReleaseDC(g_hwndKeyboard, hdc));

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
            VERIFY(::ReleaseDC(g_hwndKeyboard, hdc));

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
    int bitmapresourceid;
    if ((Emulator_GetConfiguration() & BK_COPT_BK0011) == 0)
    {
        m_arrKeyboardKeys = m_arrKeyboardKeys1;
        m_nKeyboardKeysCount = m_nKeyboardKeys1Count;
        bitmapresourceid = IDB_KEYBOARD;
    }
    else
    {
        m_arrKeyboardKeys = m_arrKeyboardKeys2;
        m_nKeyboardKeysCount = m_nKeyboardKeys2Count;
        bitmapresourceid = IDB_KEYBOARD11M;
    }

    RECT rc;  ::GetClientRect(g_hwndKeyboard, &rc);

    // Keyboard background
    HBRUSH hBkBrush = ::CreateSolidBrush(COLOR_BK_BACKGROUND);
    HGDIOBJ hOldBrush = ::SelectObject(hdc, hBkBrush);
    ::PatBlt(hdc, 0, 0, rc.right, rc.bottom, PATCOPY);
    ::SelectObject(hdc, hOldBrush);
    VERIFY(::DeleteObject(hBkBrush));

    HBITMAP hBmp = BitmapFile_LoadPngFromResource(MAKEINTRESOURCE(bitmapresourceid));
    HDC hdcMem = ::CreateCompatibleDC(hdc);
    HGDIOBJ hOldBitmap = ::SelectObject(hdcMem, hBmp);

    BITMAP bitmap;
    VERIFY(::GetObject(hBmp, sizeof(BITMAP), &bitmap));
    int cxBitmap = (int) bitmap.bmWidth;
    int cyBitmap = (int) bitmap.bmHeight;
    m_nKeyboardBitmapLeft = (rc.right - cxBitmap) / 2;
    m_nKeyboardBitmapTop = (rc.bottom - cyBitmap) / 2;
    ::BitBlt(hdc, m_nKeyboardBitmapLeft, m_nKeyboardBitmapTop, cxBitmap, cyBitmap, hdcMem, 0, 0, SRCCOPY);

    ::SelectObject(hdcMem, hOldBitmap);
    VERIFY(::DeleteDC(hdcMem));
    VERIFY(::DeleteObject(hBmp));

    if (m_nKeyboardKeyPressed != 0)
        Keyboard_DrawKey(hdc, m_nKeyboardKeyPressed);

    ////DEBUG: Show key mappings
    //for (int i = 0; i < m_nKeyboardKeysCount; i++)
    //{
    //    RECT rcKey;
    //    rcKey.left = m_nKeyboardBitmapLeft + m_arrKeyboardKeys[i].x;
    //    rcKey.top = m_nKeyboardBitmapTop + m_arrKeyboardKeys[i].y;
    //    rcKey.right = rcKey.left + m_arrKeyboardKeys[i].w;
    //    rcKey.bottom = rcKey.top + m_arrKeyboardKeys[i].h;
    //    ::DrawFocusRect(hdc, &rcKey);
    //}
}

// Returns: index of key under the cursor position, or -1 if not found
int KeyboardView_GetKeyByPoint(int x, int y)
{
    for (int i = 0; i < m_nKeyboardKeysCount; i++)
    {
        RECT rcKey;
        rcKey.left = m_nKeyboardBitmapLeft + m_arrKeyboardKeys[i].x;
        rcKey.top = m_nKeyboardBitmapTop + m_arrKeyboardKeys[i].y;
        rcKey.right = rcKey.left + m_arrKeyboardKeys[i].w;
        rcKey.bottom = rcKey.top + m_arrKeyboardKeys[i].h;

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
        if (keyscan == m_arrKeyboardKeys[i].code)
        {
            RECT rcKey;
            rcKey.left = m_nKeyboardBitmapLeft + m_arrKeyboardKeys[i].x;
            rcKey.top = m_nKeyboardBitmapTop + m_arrKeyboardKeys[i].y;
            rcKey.right = rcKey.left + m_arrKeyboardKeys[i].w;
            rcKey.bottom = rcKey.top + m_arrKeyboardKeys[i].h;
            ::DrawFocusRect(hdc, &rcKey);
        }
}


//////////////////////////////////////////////////////////////////////
