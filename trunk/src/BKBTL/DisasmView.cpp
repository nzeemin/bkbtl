/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// DisasmView.cpp

#include "stdafx.h"
#include <commdlg.h>
#include "BKBTL.h"
#include "Views.h"
#include "ToolWindow.h"
#include "Dialogs.h"
#include "Emulator.h"
#include "emubase\Emubase.h"


//////////////////////////////////////////////////////////////////////

// Colors
#define COLOR_RED       RGB(255,0,0)
#define COLOR_BLUE      RGB(0,0,255)
#define COLOR_SUBTITLE  RGB(0,128,0)
#define COLOR_VALUE     RGB(128,128,128)


HWND g_hwndDisasm = (HWND) INVALID_HANDLE_VALUE;  // Disasm View window handle
WNDPROC m_wndprocDisasmToolWindow = NULL;  // Old window proc address of the ToolWindow

HWND m_hwndDisasmViewer = (HWND) INVALID_HANDLE_VALUE;

WORD m_wDisasmBaseAddr = 0;
WORD m_wDisasmNextBaseAddr = 0;

void DisasmView_DoDraw(HDC hdc);
int  DisasmView_DrawDisassemble(HDC hdc, CProcessor* pProc, WORD base, WORD previous, int x, int y);
BOOL DisasmView_OnKeyDown(WPARAM vkey, LPARAM lParam);
void DisasmView_SetBaseAddr(WORD base);
void DisasmView_DoSubtitles();
BOOL DisasmView_ParseSubtitles();

enum DisasmSubtitleType
{
    SUBTYPE_NONE = 0,
    SUBTYPE_COMMENT = 1,
    SUBTYPE_BLOCKCOMMENT = 2,
    SUBTYPE_DATA = 4,
};

struct DisasmSubtitleItem
{
    WORD address;
    DisasmSubtitleType type;
    LPCTSTR comment;
};

BOOL m_okDisasmSubtitles = FALSE;
TCHAR* m_strDisasmSubtitles = NULL;
DisasmSubtitleItem* m_pDisasmSubtitleItems = NULL;
int m_nDisasmSubtitleMax = 0;
int m_nDisasmSubtitleCount = 0;

//////////////////////////////////////////////////////////////////////


void DisasmView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= DisasmViewViewerWndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= g_hInst;
    wcex.hIcon			= NULL;
    wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName	= NULL;
    wcex.lpszClassName	= CLASSNAME_DISASMVIEW;
    wcex.hIconSm		= NULL;

    RegisterClassEx(&wcex);
}

void CreateDisasmView(HWND hwndParent, int x, int y, int width, int height)
{
    ASSERT(hwndParent != NULL);

    g_hwndDisasm = CreateWindow(
            CLASSNAME_TOOLWINDOW, NULL,
            WS_CHILD | WS_VISIBLE,
            x, y, width, height,
            hwndParent, NULL, g_hInst, NULL);
    ::SetWindowText(g_hwndDisasm, _T("Disassemble"));

    // ToolWindow subclassing
    m_wndprocDisasmToolWindow = (WNDPROC) LongToPtr( SetWindowLongPtr(
            g_hwndDisasm, GWLP_WNDPROC, PtrToLong(DisasmViewWndProc)) );

    RECT rcClient;  GetClientRect(g_hwndDisasm, &rcClient);

    m_hwndDisasmViewer = CreateWindowEx(
            WS_EX_STATICEDGE,
            CLASSNAME_DISASMVIEW, NULL,
            WS_CHILD | WS_VISIBLE,
            0, 0, rcClient.right, rcClient.bottom,
            g_hwndDisasm, NULL, g_hInst, NULL);
}

LRESULT CALLBACK DisasmViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_DESTROY:
        g_hwndDisasm = (HWND) INVALID_HANDLE_VALUE;  // We are closed! Bye-bye!..
        return CallWindowProc(m_wndprocDisasmToolWindow, hWnd, message, wParam, lParam);
    default:
        return CallWindowProc(m_wndprocDisasmToolWindow, hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

LRESULT CALLBACK DisasmViewViewerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            DisasmView_DoDraw(hdc);

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_LBUTTONDOWN:
        SetFocus(hWnd);
        break;
    case WM_KEYDOWN:
        return (LRESULT) DisasmView_OnKeyDown(wParam, lParam);
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        ::InvalidateRect(hWnd, NULL, TRUE);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

BOOL DisasmView_OnKeyDown(WPARAM vkey, LPARAM lParam)
{
    switch (vkey)
    {
    case VK_DOWN:
        DisasmView_SetBaseAddr(m_wDisasmNextBaseAddr);
        break;
    case 0x47:  // G - Go To Address
        {
            WORD value = m_wDisasmBaseAddr;
            if (InputBoxOctal(m_hwndDisasmViewer, _T("Go To Address"), _T("Address (octal):"), &value))
                DisasmView_SetBaseAddr(value);
            break;
        }
    case 0x53:  // S - Load/Unload Subtitles
        DisasmView_DoSubtitles();
        break;
    case VK_ESCAPE:
        ConsoleView_Activate();
        break;
    default:
        return TRUE;
    }
    return FALSE;
}

void DisasmView_UpdateWindowText()
{
    if (m_okDisasmSubtitles)
        ::SetWindowText(g_hwndDisasm, _T("Disassemble - Subtitles"));
    else
        ::SetWindowText(g_hwndDisasm, _T("Disassemble"));
}

void DisasmView_ResizeSubtitleArray(int newSize)
{
    DisasmSubtitleItem* pNewMemory = (DisasmSubtitleItem*) ::LocalAlloc(LPTR, sizeof(DisasmSubtitleItem) * newSize);
    if (m_pDisasmSubtitleItems != NULL)
    {
        ::memcpy(pNewMemory, m_pDisasmSubtitleItems, sizeof(DisasmSubtitleItem) * m_nDisasmSubtitleMax);
        ::LocalFree(m_pDisasmSubtitleItems);
    }

    m_pDisasmSubtitleItems = pNewMemory;
    m_nDisasmSubtitleMax = newSize;
}
void DisasmView_AddSubtitle(WORD address, int type, LPCTSTR pCommentText)
{
    if (m_nDisasmSubtitleCount >= m_nDisasmSubtitleMax)
    {
        // ��������� ������
        int newsize = m_nDisasmSubtitleMax + m_nDisasmSubtitleMax / 2;
        DisasmView_ResizeSubtitleArray(newsize);
    }

    m_pDisasmSubtitleItems[m_nDisasmSubtitleCount].address = address;
    m_pDisasmSubtitleItems[m_nDisasmSubtitleCount].type = (DisasmSubtitleType) type;
    m_pDisasmSubtitleItems[m_nDisasmSubtitleCount].comment = pCommentText;
    m_nDisasmSubtitleCount++;
}

void DisasmView_DoSubtitles()
{
    if (m_okDisasmSubtitles)  // Reset subtitles
    {
        ::LocalFree(m_strDisasmSubtitles);  m_strDisasmSubtitles = NULL;
        ::LocalFree(m_pDisasmSubtitleItems);  m_pDisasmSubtitleItems = NULL;
        m_nDisasmSubtitleMax = m_nDisasmSubtitleCount = 0;
        m_okDisasmSubtitles = FALSE;
        DisasmView_UpdateWindowText();
        return;
    }

    // File Open dialog
    TCHAR bufFileName[MAX_PATH];
    *bufFileName = 0;
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.hInstance = g_hInst;
    ofn.lpstrTitle = _T("Open Disassemble Subtitles");
    ofn.lpstrFilter = _T("Subtitles (*.lst)\0*.lst\0All Files (*.*)\0*.*\0\0");
    ofn.Flags = OFN_FILEMUSTEXIST;
    ofn.lpstrFile = bufFileName;
    ofn.nMaxFile = sizeof(bufFileName) / sizeof(TCHAR);

    BOOL okResult = GetOpenFileName(&ofn);
    if (! okResult) return;

    // Load subtitles text from the file
    HANDLE hSubFile = CreateFile(bufFileName, GENERIC_READ, FILE_SHARE_READ, NULL,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSubFile == INVALID_HANDLE_VALUE)
    {
        AlertWarning(_T("Failed to load subtitles file."));
        return;
    }
    DWORD dwSubFileSize = ::GetFileSize(hSubFile, NULL);
    if (dwSubFileSize > 1024 * 1024)
    {
        ::CloseHandle(hSubFile);
        AlertWarning(_T("Subtitles file is too big (over 1 MB)."));
        return;
    }

    m_strDisasmSubtitles = (TCHAR*) ::LocalAlloc(LPTR, dwSubFileSize + 2);
    DWORD dwBytesRead;
    ::ReadFile(hSubFile, m_strDisasmSubtitles, dwSubFileSize, &dwBytesRead, NULL);
    ASSERT(dwBytesRead == dwSubFileSize);
    ::CloseHandle(hSubFile);

    // Estimate comment count and allocate memory
    int estimateSubtitleCount = dwSubFileSize / (75 * sizeof(TCHAR));
    if (estimateSubtitleCount < 256)
        estimateSubtitleCount = 256;
    DisasmView_ResizeSubtitleArray(estimateSubtitleCount);

    // Parse subtitles
    if (!DisasmView_ParseSubtitles())
    {
        ::LocalFree(m_strDisasmSubtitles);  m_strDisasmSubtitles = NULL;
        ::LocalFree(m_pDisasmSubtitleItems);  m_pDisasmSubtitleItems = NULL;
        AlertWarning(_T("Failed to parse subtitles file."));
        return;
    }

    m_okDisasmSubtitles = TRUE;
    DisasmView_UpdateWindowText();
}

// ������ ������ "���������".
// �� ����� -- ����� � m_strDisasmSubtitles � ������� UTF16 LE, ������������� �������� � ����� 0.
// �� ������ -- ������ �������� [����� � ������, ���, ����� ������ �����������] � m_pDisasmSubtitleItems.
BOOL DisasmView_ParseSubtitles()
{
    ASSERT(m_strDisasmSubtitles != NULL);
    TCHAR* pText = m_strDisasmSubtitles;
    if (*pText == 0 || *pText == 0xFFFE)  // EOF or Unicode Big Endian
        return FALSE;
    if (*pText == 0xFEFF)
        pText++;  // Skip Unicode LE mark

    m_nDisasmSubtitleCount = 0;
    TCHAR* pBlockCommentStart = NULL;

    for (;;)  // Text reading loop - line by line
    {
        // Line starts
        if (*pText == 0) break;
        if (*pText == _T('\n') || *pText == _T('\r'))
        {
            pText++;
            continue;
        }

        if (*pText >= _T('0') && *pText <= _T('9'))  // ����� -- ������� ��� ��� �����
        {
            // ������ �����
            TCHAR* pAddrStart = pText;
            while (*pText != 0 && *pText >= _T('0') && *pText <= _T('9')) pText++;
            if (*pText == 0) break;
            TCHAR chSave = *pText;
            *pText++ = 0;
            WORD address;
            ParseOctalValue(pAddrStart, &address);
            *pText = chSave;

            if (pBlockCommentStart != NULL)  // �� ���������� ������ ��� ����������� � �����
            {
                // ��������� ����������� � ����� � �������
                DisasmView_AddSubtitle(address, SUBTYPE_BLOCKCOMMENT, pBlockCommentStart);
                pBlockCommentStart = NULL;
            }

            // ���������� �����������
            while (*pText != 0 &&
                   (*pText == _T(' ') || *pText == _T('\t') || *pText == _T('$') || *pText == _T(':')))
                pText++;
            BOOL okDirective = (*pText == _T('.'));

            // ���� ������ ����������� � ����� ������
            while (*pText != 0 && *pText != _T(';') && *pText != _T('\n') && *pText != _T('\r')) pText++;
            if (*pText == 0) break;
            if (*pText == _T('\n') || *pText == _T('\r'))  // EOL, ����������� �� ���������
            {
                pText++;

                if (okDirective)
                    DisasmView_AddSubtitle(address, SUBTYPE_DATA, NULL);
                continue;
            }

            // ����� ������ ����������� -- ���� ����� ������ ��� �����
            TCHAR* pCommentStart = pText;
            while (*pText != 0 && *pText != _T('\n') && *pText != _T('\r')) pText++;

            // ��������� ����������� � �������
            DisasmView_AddSubtitle(address,
                    (okDirective ? SUBTYPE_COMMENT | SUBTYPE_DATA : SUBTYPE_COMMENT),
                    pCommentStart);

            if (*pText == 0) break;
            *pText = 0;  // ���������� ����� �����������
            pText++;
        }
        else  // �� ����� -- ���������� �� ����� ������
        {
            if (*pText == _T(';'))  // ������ ���������� � ����������� - ����������������, ����������� � �����
                pBlockCommentStart = pText;
            else
                pBlockCommentStart = NULL;

            while (*pText != 0 && *pText != _T('\n') && *pText != _T('\r')) pText++;
            if (*pText == 0) break;
            if (*pText == _T('\n') || *pText == _T('\r'))  // EOL
            {
                *pText = 0;  // ���������� ����� ����������� - ��� ����������� � �����
                pText++;
                continue;
            }
        }
    }

    return TRUE;
}


//////////////////////////////////////////////////////////////////////


// Update after Run or Step
void DisasmView_OnUpdate()
{
    CProcessor* pDisasmPU = g_pBoard->GetCPU();
    ASSERT(pDisasmPU != NULL);
    m_wDisasmBaseAddr = pDisasmPU->GetPC();
}

void DisasmView_SetBaseAddr(WORD base)
{
    m_wDisasmBaseAddr = base;
    InvalidateRect(m_hwndDisasmViewer, NULL, TRUE);
}


//////////////////////////////////////////////////////////////////////
// Draw functions

void DisasmView_DoDraw(HDC hdc)
{
    ASSERT(g_pBoard != NULL);

    // Create and select font
    HFONT hFont = CreateMonospacedFont();
    HGDIOBJ hOldFont = SelectObject(hdc, hFont);
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);
    COLORREF colorOld = SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
    COLORREF colorBkOld = SetBkColor(hdc, GetSysColor(COLOR_WINDOW));

    CProcessor* pDisasmPU = g_pBoard->GetCPU();

    // Draw disasseble for the current processor
    WORD prevPC = g_wEmulatorPrevCpuPC;
    int yFocus = DisasmView_DrawDisassemble(hdc, pDisasmPU, m_wDisasmBaseAddr, prevPC, 0, 2 + 0 * cyLine);

    SetTextColor(hdc, colorOld);
    SetBkColor(hdc, colorBkOld);
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);

    if (::GetFocus() == m_hwndDisasmViewer)
    {
        RECT rcFocus;
        GetClientRect(m_hwndDisasmViewer, &rcFocus);
        if (yFocus >= 0)
        {
            rcFocus.top = yFocus;
            rcFocus.bottom = yFocus + cyLine;
        }
        DrawFocusRect(hdc, &rcFocus);
    }
}

DisasmSubtitleItem* DisasmView_FindSubtitle(WORD address, int typemask)
{
    DisasmSubtitleItem* pItem = m_pDisasmSubtitleItems;
    while (pItem->type != 0)
    {
        if (pItem->address == address && (pItem->type & typemask) != 0)
            return pItem;
        pItem++;
    }

    return NULL;
}

int DisasmView_DrawDisassemble(HDC hdc, CProcessor* pProc, WORD base, WORD previous, int x, int y)
{
    int result = -1;
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);
    COLORREF colorText = GetSysColor(COLOR_WINDOWTEXT);

    WORD proccurrent = pProc->GetPC();
    WORD current = base;

    // ������ �� ������ ���������� � �����
    const int nWindowSize = 30;
    WORD memory[nWindowSize + 2];
    for (int idx = 0; idx < nWindowSize; idx++)
    {
        int addrtype;
        memory[idx] = g_pBoard->GetWordView(
                current + idx * 2 - 10, pProc->IsHaltMode(), TRUE, &addrtype);
    }

    WORD address = current - 10;
    WORD disasmfrom = current;
    if (previous >= address && previous < current)
        disasmfrom = previous;

    int length = 0;
    WORD wNextBaseAddr = 0;
    for (int index = 0; index < nWindowSize; index++)  // ������ ������
    {
        if (m_okDisasmSubtitles)  // Subtitles - ����������� � �����
        {
            DisasmSubtitleItem* pSubItem = DisasmView_FindSubtitle(address, SUBTYPE_BLOCKCOMMENT);
            if (pSubItem != NULL && pSubItem->comment != NULL)
            {
                LPCTSTR strBlockSubtitle = pSubItem->comment;

                ::SetTextColor(hdc, COLOR_SUBTITLE);
                TextOut(hdc, x + 21 * cxChar, y, strBlockSubtitle, (int) wcslen(strBlockSubtitle));
                ::SetTextColor(hdc, colorText);

                y += cyLine;
            }
        }

        DrawOctalValue(hdc, x + 5 * cxChar, y, address);  // Address
        // Value at the address
        WORD value = memory[index];
        ::SetTextColor(hdc, COLOR_VALUE);
        DrawOctalValue(hdc, x + 13 * cxChar, y, value);
        ::SetTextColor(hdc, colorText);

        // Current position
        if (address == current)
        {
            TextOut(hdc, x + 1 * cxChar, y, _T("  >"), 3);
            result = y;  // Remember line for the focus rect
        }
        if (address == proccurrent)
        {
            BOOL okPCchanged = DebugView_IsRegisterChanged(7);
            if (okPCchanged) ::SetTextColor(hdc, COLOR_RED);
            TextOut(hdc, x + 1 * cxChar, y, _T("PC"), 2);
            ::SetTextColor(hdc, colorText);
            TextOut(hdc, x + 3 * cxChar, y, _T(">>"), 2);
        }
        else if (address == previous)
        {
            ::SetTextColor(hdc, COLOR_BLUE);
            TextOut(hdc, x + 1 * cxChar, y, _T("  >"), 3);
        }

        BOOL okData = FALSE;
        if (m_okDisasmSubtitles)  // Show subtitle
        {
            DisasmSubtitleItem* pSubItem = DisasmView_FindSubtitle(address, SUBTYPE_COMMENT | SUBTYPE_DATA);
            if (pSubItem != NULL && (pSubItem->type & SUBTYPE_DATA) != 0)
                okData = TRUE;
            if (pSubItem != NULL && (pSubItem->type & SUBTYPE_COMMENT) != 0 && pSubItem->comment != NULL)
            {
                LPCTSTR strSubtitle = pSubItem->comment;

                ::SetTextColor(hdc, COLOR_SUBTITLE);
                TextOut(hdc, x + 52 * cxChar, y, strSubtitle, (int) wcslen(strSubtitle));
                ::SetTextColor(hdc, colorText);

                // ������ � ��������� �� ����� ������������ ��� ������� ��� �������������
                if (disasmfrom > address)
                    disasmfrom = address;
            }
        }

        if (address >= disasmfrom && length == 0)
        {
            TCHAR strInstr[8];
            TCHAR strArg[32];
            if (okData)  // �� ����� ������ ����� ������ -- ��� ������ �����������������
            {
                lstrcpy(strInstr, _T("data"));
                PrintOctalValue(strArg, *(memory + index));
                length = 1;
            }
            else
            {
                length = DisassembleInstruction(memory + index, address, strInstr, strArg);
            }
            if (index + length <= nWindowSize)
            {
                TextOut(hdc, x + 21 * cxChar, y, strInstr, (int) wcslen(strInstr));
                TextOut(hdc, x + 29 * cxChar, y, strArg, (int) wcslen(strArg));
            }
            ::SetTextColor(hdc, colorText);
            if (wNextBaseAddr == 0)
                wNextBaseAddr = address + length * 2;
        }
        if (length > 0) length--;

        address += 2;
        y += cyLine;
    }

    m_wDisasmNextBaseAddr = wNextBaseAddr;

    return result;
}


//////////////////////////////////////////////////////////////////////
