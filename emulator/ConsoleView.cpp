/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// ConsoleView.cpp

#include "stdafx.h"
#include "Main.h"
#include "Views.h"
#include "ToolWindow.h"
#include "Emulator.h"
#include "emubase/Emubase.h"

//////////////////////////////////////////////////////////////////////


COLORREF COLOR_COMMANDFOCUS = RGB(255, 242, 157);

HWND g_hwndConsole = (HWND)INVALID_HANDLE_VALUE;  // Console View window handle
WNDPROC m_wndprocConsoleToolWindow = NULL;  // Old window proc address of the ToolWindow

HWND m_hwndConsoleLog = (HWND)INVALID_HANDLE_VALUE;  // Console log window - read-only edit control
HWND m_hwndConsoleEdit = (HWND)INVALID_HANDLE_VALUE;  // Console line - edit control
HWND m_hwndConsolePrompt = (HWND)INVALID_HANDLE_VALUE;  // Console prompt - static control
HFONT m_hfontConsole = NULL;
WNDPROC m_wndprocConsoleEdit = NULL;  // Old window proc address of the console prompt
HBRUSH m_hbrConsoleFocused = NULL;

CProcessor* ConsoleView_GetCurrentProcessor();
void ConsoleView_AdjustWindowLayout();
LRESULT CALLBACK ConsoleEditWndProc(HWND, UINT, WPARAM, LPARAM);
void ConsoleView_DoConsoleCommand();

void ConsoleView_PrintConsolePrompt();
void ConsoleView_PrintRegister(LPCTSTR strName, WORD value);
void ConsoleView_PrintMemoryDump(const CProcessor* pProc, WORD address, int lines);
BOOL ConsoleView_SaveMemoryDump();

const LPCTSTR MESSAGE_UNKNOWN_COMMAND = _T("  Unknown command.\r\n");
const LPCTSTR MESSAGE_WRONG_VALUE = _T("  Wrong value.\r\n");
const LPCTSTR MESSAGE_INVALID_REGNUM = _T("  Invalid register number, 0..7 expected.\r\n");


//////////////////////////////////////////////////////////////////////


void ConsoleView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = ConsoleViewWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = g_hInst;
    wcex.hIcon          = NULL;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = CLASSNAME_CONSOLEVIEW;
    wcex.hIconSm        = NULL;

    RegisterClassEx(&wcex);
}

// Create Console View as child of Main Window
void ConsoleView_Create(HWND hwndParent, int x, int y, int width, int height)
{
    ASSERT(hwndParent != NULL);

    g_hwndConsole = CreateWindow(
            CLASSNAME_TOOLWINDOW, NULL,
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            x, y, width, height,
            hwndParent, NULL, g_hInst, NULL);
    SetWindowText(g_hwndConsole, _T("Debug Console"));

    // ToolWindow subclassing
    m_wndprocConsoleToolWindow = (WNDPROC)LongToPtr( SetWindowLongPtr(
            g_hwndConsole, GWLP_WNDPROC, PtrToLong(ConsoleViewWndProc)) );

    RECT rcConsole;  GetClientRect(g_hwndConsole, &rcConsole);

    m_hwndConsoleEdit = CreateWindowEx(
            WS_EX_CLIENTEDGE,
            _T("EDIT"), NULL,
            WS_CHILD | WS_VISIBLE,
            90, rcConsole.bottom - 20,
            rcConsole.right - 90, 20,
            g_hwndConsole, NULL, g_hInst, NULL);
    m_hwndConsoleLog = CreateWindowEx(
            WS_EX_CLIENTEDGE,
            _T("EDIT"), NULL,
            WS_CHILD | WS_VSCROLL | WS_VISIBLE | ES_READONLY | ES_MULTILINE,
            0, 0,
            rcConsole.right, rcConsole.bottom - 20,
            g_hwndConsole, NULL, g_hInst, NULL);
    m_hwndConsolePrompt = CreateWindowEx(
            0,
            _T("STATIC"), NULL,
            WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_CENTER | SS_NOPREFIX,
            0, rcConsole.bottom - 20,
            90, 20,
            g_hwndConsole, NULL, g_hInst, NULL);

    m_hfontConsole = CreateMonospacedFont();
    SendMessage(m_hwndConsolePrompt, WM_SETFONT, (WPARAM)m_hfontConsole, 0);
    SendMessage(m_hwndConsoleEdit, WM_SETFONT, (WPARAM)m_hfontConsole, 0);
    SendMessage(m_hwndConsoleLog, WM_SETFONT, (WPARAM)m_hfontConsole, 0);

    // Edit box subclassing
    m_wndprocConsoleEdit = (WNDPROC)LongToPtr( SetWindowLongPtr(
            m_hwndConsoleEdit, GWLP_WNDPROC, PtrToLong(ConsoleEditWndProc)) );

    ShowWindow(g_hwndConsole, SW_SHOW);
    UpdateWindow(g_hwndConsole);

    ConsoleView_Print(_T("Use 'h' command to show help.\r\n\r\n"));
    ConsoleView_PrintConsolePrompt();
    SetFocus(m_hwndConsoleEdit);
}

// Adjust position of client windows
void ConsoleView_AdjustWindowLayout()
{
    RECT rc;  GetClientRect(g_hwndConsole, &rc);
    int promptWidth = 65;

    if (m_hwndConsolePrompt != (HWND)INVALID_HANDLE_VALUE)
        SetWindowPos(m_hwndConsolePrompt, NULL, 0, rc.bottom - 20, promptWidth, 20, SWP_NOZORDER);
    if (m_hwndConsoleEdit != (HWND)INVALID_HANDLE_VALUE)
        SetWindowPos(m_hwndConsoleEdit, NULL, promptWidth, rc.bottom - 20, rc.right - promptWidth, 20, SWP_NOZORDER);
    if (m_hwndConsoleLog != (HWND)INVALID_HANDLE_VALUE)
        SetWindowPos(m_hwndConsoleLog, NULL, 0, 0, rc.right, rc.bottom - 24, SWP_NOZORDER);
}

LRESULT CALLBACK ConsoleViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    LRESULT lResult;
    switch (message)
    {
    case WM_DESTROY:
        g_hwndConsole = (HWND)INVALID_HANDLE_VALUE;  // We are closed! Bye-bye!..
        break;
    case WM_CTLCOLORSTATIC:
        if (((HWND)lParam) == m_hwndConsoleLog)
        {
            SetBkColor((HDC)wParam, ::GetSysColor(COLOR_WINDOW));
            return (LRESULT)::GetSysColorBrush(COLOR_WINDOW);
        }
        break;
    case WM_CTLCOLOREDIT:
        if (((HWND)lParam) == m_hwndConsoleEdit && ::GetFocus() == m_hwndConsoleEdit)
        {
            if (m_hbrConsoleFocused == NULL)
                m_hbrConsoleFocused = ::CreateSolidBrush(COLOR_COMMANDFOCUS);
            SetBkColor((HDC)wParam, COLOR_COMMANDFOCUS);
            return (LRESULT)m_hbrConsoleFocused;
        }
        return CallWindowProc(m_wndprocConsoleToolWindow, hWnd, message, wParam, lParam);
    case WM_SIZE:
        lResult = CallWindowProc(m_wndprocConsoleToolWindow, hWnd, message, wParam, lParam);
        ConsoleView_AdjustWindowLayout();
        return lResult;
    }

    return CallWindowProc(m_wndprocConsoleToolWindow, hWnd, message, wParam, lParam);
}

LRESULT CALLBACK ConsoleEditWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CHAR:
        if (wParam == 13)
        {
            ConsoleView_DoConsoleCommand();
            return 0;
        }
        if (wParam == VK_ESCAPE)
        {
            TCHAR command[32];
            GetWindowText(m_hwndConsoleEdit, command, 32);
            if (*command == 0)  // If command is empty
                SetFocus(g_hwndScreen);
            else
                SendMessage(m_hwndConsoleEdit, WM_SETTEXT, 0, (LPARAM)_T(""));  // Clear command
            return 0;
        }
        break;
    }

    return CallWindowProc(m_wndprocConsoleEdit, hWnd, message, wParam, lParam);
}

void ConsoleView_Activate()
{
    if (g_hwndConsole == INVALID_HANDLE_VALUE) return;

    SetFocus(m_hwndConsoleEdit);
}

CProcessor* ConsoleView_GetCurrentProcessor()
{
    return g_pBoard->GetCPU();
}

void ConsoleView_PrintFormat(LPCTSTR pszFormat, ...)
{
    const size_t buffersize = 512;
    TCHAR buffer[buffersize];

    va_list ptr;
    va_start(ptr, pszFormat);
    _vsntprintf_s(buffer, buffersize, buffersize - 1, pszFormat, ptr);
    va_end(ptr);

    ConsoleView_Print(buffer);
}
void ConsoleView_Print(LPCTSTR message)
{
    if (m_hwndConsoleLog == INVALID_HANDLE_VALUE) return;

    // Put selection to the end of text
    SendMessage(m_hwndConsoleLog, EM_SETSEL, 0x100000, 0x100000);
    // Insert the message
    SendMessage(m_hwndConsoleLog, EM_REPLACESEL, (WPARAM)FALSE, (LPARAM)message);
    // Scroll to caret
    SendMessage(m_hwndConsoleLog, EM_SCROLLCARET, 0, 0);
}

void ConsoleView_ClearConsole()
{
    if (m_hwndConsoleLog == INVALID_HANDLE_VALUE) return;

    SendMessage(m_hwndConsoleLog, WM_SETTEXT, 0, (LPARAM)_T(""));
}

void ConsoleView_PrintConsolePrompt()
{
    CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    TCHAR bufferAddr[7];
    PrintOctalValue(bufferAddr, pProc->GetPC());
    TCHAR buffer[14];
    _sntprintf(buffer, sizeof(buffer) / sizeof(TCHAR) - 1, _T("%s> "), bufferAddr);
    ::SetWindowText(m_hwndConsolePrompt, buffer);
}

// Print register name, octal value and binary value
void ConsoleView_PrintRegister(LPCTSTR strName, WORD value)
{
    TCHAR buffer[31];
    TCHAR* p = buffer;
    *p++ = _T(' ');
    *p++ = _T(' ');
    lstrcpy(p, strName);  p += 2;
    *p++ = _T(' ');
    PrintOctalValue(p, value);  p += 6;
    *p++ = _T(' ');
    PrintBinaryValue(p, value);  p += 16;
    *p++ = _T('\r');
    *p++ = _T('\n');
    *p++ = 0;
    ConsoleView_Print(buffer);
}

void ConsoleView_StepInto()
{
    // Put command to console prompt
    SendMessage(m_hwndConsoleEdit, WM_SETTEXT, 0, (LPARAM)_T("s"));
    // Execute command
    ConsoleView_DoConsoleCommand();
}
void ConsoleView_StepOver()
{
    // Put command to console prompt
    SendMessage(m_hwndConsoleEdit, WM_SETTEXT, 0, (LPARAM)_T("so"));
    // Execute command
    ConsoleView_DoConsoleCommand();
}
void ConsoleView_DeleteAllBreakpoints()
{
    // Put command to console prompt
    SendMessage(m_hwndConsoleEdit, WM_SETTEXT, 0, (LPARAM)_T("bc"));
    // Execute command
    ConsoleView_DoConsoleCommand();
}

BOOL ConsoleView_SaveMemoryDump()
{
    BYTE buf[65536];
    for (int i = 0; i < 65536; i++)
    {
        buf[i] = g_pBoard->GetByte((uint16_t)i, 1);
    }

    const TCHAR fname[] = _T("memdump.bin");
    HANDLE file = ::CreateFile(fname,
            GENERIC_WRITE, FILE_SHARE_READ, NULL,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    DWORD dwBytesWritten = 0;
    ::WriteFile(file, buf, 65536, &dwBytesWritten, NULL);
    ::CloseHandle(file);
    if (dwBytesWritten != 65536)
        return FALSE;

    return TRUE;
}

// Print memory dump
void ConsoleView_PrintMemoryDump(const CProcessor* pProc, WORD address, int lines = 8)
{
    address &= ~1;  // Line up to even address

    bool okHaltMode = pProc->IsHaltMode();

    for (int line = 0; line < lines; line++)
    {
        WORD dump[8];
        int addrtype;
        for (int i = 0; i < 8; i++)
            dump[i] = g_pBoard->GetWordView((uint16_t)(address + i * 2), okHaltMode, false, &addrtype);

        TCHAR buffer[2 + 6 + 2 + 7 * 8 + 1 + 16 + 1 + 2];
        TCHAR* pBuf = buffer;
        *pBuf = _T(' ');  pBuf++;
        *pBuf = _T(' ');  pBuf++;
        PrintOctalValue(pBuf, address);  pBuf += 6;
        *pBuf = _T(' ');  pBuf++;
        *pBuf = _T(' ');  pBuf++;
        for (int i = 0; i < 8; i++)
        {
            PrintOctalValue(pBuf, dump[i]);  pBuf += 6;
            *pBuf = _T(' ');  pBuf++;
        }
        *pBuf = _T(' ');  pBuf++;
        for (int i = 0; i < 8; i++)
        {
            WORD word = dump[i];
            BYTE ch1 = LOBYTE(word);
            TCHAR wch1 = Translate_BK_Unicode(ch1);
            if (ch1 < 32) wch1 = _T('·');
            *pBuf = wch1;  pBuf++;
            BYTE ch2 = HIBYTE(word);
            TCHAR wch2 = Translate_BK_Unicode(ch2);
            if (ch2 < 32) wch2 = _T('·');
            *pBuf = wch2;  pBuf++;
        }
        *pBuf++ = _T('\r');
        *pBuf++ = _T('\n');
        *pBuf = 0;

        ConsoleView_Print(buffer);

        address += 16;
    }
}

// Print disassembled instructions
// Return value: number of words in the last instruction
int ConsoleView_PrintDisassemble(CProcessor* pProc, WORD address, BOOL okOneInstr, BOOL okShort)
{
    bool okHaltMode = pProc->IsHaltMode();

    const int nWindowSize = 30;
    WORD memory[nWindowSize + 2];
    int addrtype;
    for (int i = 0; i < nWindowSize + 2; i++)
        memory[i] = g_pBoard->GetWordView((uint16_t)(address + i * 2), okHaltMode, TRUE, &addrtype);

    TCHAR bufaddr[7];
    TCHAR bufvalue[7];
    TCHAR buffer[64];

    int lastLength = 0;
    int length = 0;
    for (int index = 0; index < nWindowSize; index++)  // Рисуем строки
    {
        PrintOctalValue(bufaddr, address);
        WORD value = memory[index];
        PrintOctalValue(bufvalue, value);

        if (length > 0)
        {
            if (!okShort)
            {
                _sntprintf(buffer, sizeof(buffer) / sizeof(TCHAR) - 1, _T("  %s  %s\r\n"), bufaddr, bufvalue);
                ConsoleView_Print(buffer);
            }
        }
        else
        {
            if (okOneInstr && index > 0)
                break;
            TCHAR instr[8];
            TCHAR args[32];
            length = DisassembleInstruction(memory + index, address, instr, args);
            lastLength = length;
            if (index + length > nWindowSize)
                break;
            if (okShort)
                _sntprintf(buffer, sizeof(buffer) / sizeof(TCHAR) - 1, _T("  %s  %-7s %s\r\n"), bufaddr, instr, args);
            else
                _sntprintf(buffer, sizeof(buffer) / sizeof(TCHAR) - 1, _T("  %s  %s  %-7s %s\r\n"), bufaddr, bufvalue, instr, args);
            ConsoleView_Print(buffer);
        }
        length--;
        address += 2;
    }

    return lastLength;
}

void ConsoleView_GotoMonitor()
{
    if ((g_nEmulatorConfiguration & BK_COPT_ROM_BASIC) != 0)
    {
        ScreenView_KeyEvent(0115, TRUE);  // M
        ScreenView_KeyEvent(0115, FALSE);
        ScreenView_KeyEvent(0117, TRUE);  // O
        ScreenView_KeyEvent(0117, FALSE);
        //ScreenView_KeyEvent(0012, TRUE);  // ENTER
        //ScreenView_KeyEvent(0012, FALSE);
    }
    else if ((g_nEmulatorConfiguration & BK_COPT_ROM_FOCAL) != 0)
    {
        ScreenView_KeyEvent(0120, TRUE);  // P
        ScreenView_KeyEvent(0120, FALSE);
        ScreenView_KeyEvent(0040, TRUE);  // SPACE
        ScreenView_KeyEvent(0040, FALSE);
        ScreenView_KeyEvent(0115, TRUE);  // M
        ScreenView_KeyEvent(0115, FALSE);
        //ScreenView_KeyEvent(0012, TRUE);  // ENTER
        //ScreenView_KeyEvent(0012, FALSE);
    }

    SetFocus(g_hwndScreen);  // Move focus to the screen
}

#if !defined(PRODUCT)
void ConsoleView_TraceLog(DWORD value)
{
    g_pBoard->SetTrace(value);
    if (value != TRACE_NONE)
        ConsoleView_PrintFormat(_T("  Trace ON, trace flags %06o\r\n"), (uint16_t)g_pBoard->GetTrace());
    else
    {
        ConsoleView_Print(_T("  Trace OFF.\r\n"));
        DebugLogCloseFile();
    }
}
#endif


//////////////////////////////////////////////////////////////////////
// Console commands handlers

struct ConsoleCommandParams
{
    LPCTSTR     commandText;
    int         paramReg1;
    uint16_t    paramOct1, paramOct2;
};

void ConsoleView_CmdShowHelp(const ConsoleCommandParams& /*params*/)
{
    ConsoleView_Print(_T("Console command list:\r\n")
            _T("  c          Clear console log\r\n")
            _T("  d          Disassemble from PC; use D for short format\r\n")
            _T("  dXXXXXX    Disassemble from address XXXXXX\r\n")
            _T("  g          Go; free run\r\n")
            _T("  gXXXXXX    Go; run and stop at address XXXXXX\r\n")
            _T("  m          Memory dump at current address\r\n")
            _T("  mXXXXXX    Memory dump at address XXXXXX\r\n")
            _T("  mrN        Memory dump at address from register N; N=0..7\r\n")
            _T("  r          Show register values\r\n")
            _T("  rN         Show value of register N; N=0..7,ps\r\n")
            _T("  rN XXXXXX  Set register N to value XXXXXX; N=0..7,ps\r\n")
            _T("  s          Step Into; executes one instruction\r\n")
            _T("  so         Step Over; executes and stops after the current instruction\r\n")
            _T("  b          List all breakpoints\r\n")
            _T("  bXXXXXX    Set breakpoint at address XXXXXX\r\n")
            _T("  bcXXXXXX   Remove breakpoint at address XXXXXX\r\n")
            _T("  bc         Remove all breakpoints\r\n")
            _T("  w          List all watches\r\n")
            _T("  wXXXXXX    Set watch at address XXXXXX\r\n")
            _T("  wcXXXXXX   Remove watch at address XXXXXX\r\n")
            _T("  wc         Remove all watches\r\n")
            _T("  u          Save memory dump to file memdump.bin\r\n")
            _T("  mo         Type MO<Enter> (BASIC) or P M<Enter> (FOCAL) to exit to Monitor\r\n")
#if !defined(PRODUCT)
            _T("  t          Tracing on/off to trace.log file\r\n")
            _T("  tXXXXXX    Set tracing flags\r\n")
            _T("  tc         Clear trace.log file\r\n")
#endif
                     );
}

void ConsoleView_CmdClearConsoleLog(const ConsoleCommandParams& /*params*/)
{
    ConsoleView_ClearConsole();
}

void ConsoleView_CmdPrintRegister(const ConsoleCommandParams& params)
{
    int r = params.paramReg1;

    LPCTSTR name = REGISTER_NAME[r];
    const CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    uint16_t value = pProc->GetReg(r);
    ConsoleView_PrintRegister(name, value);
}
void ConsoleView_CmdPrintRegisterPSW(const ConsoleCommandParams& /*params*/)
{
    const CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    WORD value = pProc->GetPSW();
    ConsoleView_PrintRegister(_T("PS"), value);
}

void ConsoleView_CmdSetRegisterValue(const ConsoleCommandParams& params)
{
    int r = params.paramReg1;
    uint16_t value = params.paramOct1;

    CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    pProc->SetReg(r, value);

    MainWindow_UpdateAllViews();
}
void ConsoleView_CmdSetRegisterPSW(const ConsoleCommandParams& params)
{
    uint16_t value = params.paramOct1;

    CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    pProc->SetPSW(value);

    MainWindow_UpdateAllViews();
}

void ConsoleView_CmdSaveMemoryDump(const ConsoleCommandParams& /*params*/)
{
    ConsoleView_SaveMemoryDump();
}

void ConsoleView_CmdPrintMemoryDumpAtPC(const ConsoleCommandParams& /*params*/)
{
    CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    uint16_t address = pProc->GetPC();
    ConsoleView_PrintMemoryDump(pProc, address);
}
void ConsoleView_CmdPrintMemoryDumpAtAddress(const ConsoleCommandParams& params)
{
    uint16_t address = params.paramOct1;

    CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    ConsoleView_PrintMemoryDump(pProc, address);
}
void ConsoleView_CmdPrintMemoryDumpAtRegister(const ConsoleCommandParams& params)
{
    int r = params.paramReg1;

    CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    uint16_t address = pProc->GetReg(r);
    ConsoleView_PrintMemoryDump(pProc, address);
}

void ConsoleView_CmdPrintDisassembleAtPC(const ConsoleCommandParams& params)
{
    BOOL okShort = (params.commandText[0] == _T('D'));
    CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    uint16_t address = pProc->GetPC();
    ConsoleView_PrintDisassemble(pProc, address, FALSE, okShort);
}
void ConsoleView_CmdPrintDisassembleAtAddress(const ConsoleCommandParams& params)
{
    uint16_t address = params.paramOct1;

    BOOL okShort = (params.commandText[0] == _T('D'));
    CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    ConsoleView_PrintDisassemble(pProc, address, FALSE, okShort);
}

void ConsoleView_CmdPrintAllRegisters(const ConsoleCommandParams& /*params*/)
{
    CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    for (int r = 0; r < 8; r++)
    {
        LPCTSTR name = REGISTER_NAME[r];
        WORD value = pProc->GetReg(r);
        ConsoleView_PrintRegister(name, value);
    }
    ConsoleView_PrintRegister(_T("PS"), pProc->GetPSW());
}

void ConsoleView_CmdStepInto(const ConsoleCommandParams& /*params*/)
{
    CProcessor* pProc = ConsoleView_GetCurrentProcessor();

    ConsoleView_PrintDisassemble(pProc, pProc->GetPC(), TRUE, FALSE);

    g_pBoard->DebugTicks();

    MainWindow_UpdateAllViews();
}
void ConsoleView_CmdStepOver(const ConsoleCommandParams& /*params*/)
{
    CProcessor* pProc = ConsoleView_GetCurrentProcessor();

    int instrLength = ConsoleView_PrintDisassemble(pProc, pProc->GetPC(), TRUE, FALSE);

    int addrtype;
    uint16_t instr = g_pBoard->GetWordView(pProc->GetPC(), pProc->IsHaltMode(), true, &addrtype);

    // For JMP and BR use Step Into logic, not Step Over
    if ((instr & ~(uint16_t)077) == PI_JMP || (instr & ~(uint16_t)0377) == PI_BR)
    {
        g_pBoard->DebugTicks();

        MainWindow_UpdateAllViews();

        return;
    }

    uint16_t bpaddress = (uint16_t)(pProc->GetPC() + instrLength * 2);

    Emulator_SetTempCPUBreakpoint(bpaddress);
    Emulator_Start();
}
void ConsoleView_CmdRun(const ConsoleCommandParams& /*params*/)
{
    Emulator_Start();
}
void ConsoleView_CmdRunToAddress(const ConsoleCommandParams& params)
{
    uint16_t address = params.paramOct1;

    Emulator_SetTempCPUBreakpoint(address);
    Emulator_Start();
}

void ConsoleView_CmdGotoMonitor(const ConsoleCommandParams& /*params*/)
{
    ConsoleView_GotoMonitor();
}

void ConsoleView_CmdPrintAllBreakpoints(const ConsoleCommandParams& /*params*/)
{
    const uint16_t* pbps = Emulator_GetCPUBreakpointList();
    if (pbps == nullptr || *pbps == 0177777)
    {
        ConsoleView_Print(_T("  No breakpoints.\r\n"));
    }
    else
    {
        while (*pbps != 0177777)
        {
            ConsoleView_PrintFormat(_T("  %06ho\r\n"), *pbps);
            pbps++;
        }
    }
}
void ConsoleView_CmdRemoveAllBreakpoints(const ConsoleCommandParams& /*params*/)
{
    Emulator_RemoveAllBreakpoints();

    DebugView_Redraw();
    DisasmView_Redraw();
}
void ConsoleView_CmdSetBreakpointAtAddress(const ConsoleCommandParams& params)
{
    uint16_t address = params.paramOct1;

    bool result = Emulator_AddCPUBreakpoint(address);
    if (!result)
        ConsoleView_Print(_T("  Failed to add breakpoint.\r\n"));

    DebugView_Redraw();
    DisasmView_Redraw();
}
void ConsoleView_CmdRemoveBreakpointAtAddress(const ConsoleCommandParams& params)
{
    uint16_t address = params.paramOct1;

    bool result = Emulator_RemoveCPUBreakpoint(address);
    if (!result)
        ConsoleView_Print(_T("  Failed to remove breakpoint.\r\n"));
    DebugView_Redraw();
    DisasmView_Redraw();
}

void ConsoleView_CmdPrintAllWatches(const ConsoleCommandParams& /*params*/)
{
    CProcessor* pProc = ConsoleView_GetCurrentProcessor();
    bool okHaltMode = pProc->IsHaltMode();

    const uint16_t* pws = Emulator_GetWatchpointList();
    if (pws == nullptr || *pws == 0177777)
    {
        ConsoleView_Print(_T("  No watches defined.\r\n"));
    }
    else
    {
        while (*pws != 0177777)
        {
            uint16_t address = *pws;
            int addrtype;
            uint16_t value = g_pBoard->GetWordView(address, okHaltMode, false, &addrtype);
            ConsoleView_PrintFormat(_T("  %06ho %06ho\r\n"), address, value);
            pws++;
        }
    }
}
void ConsoleView_CmdSetWatchAtAddress(const ConsoleCommandParams& params)
{
    uint16_t address = params.paramOct1;

    bool result = Emulator_AddWatchpoint(address);
    if (!result)
        ConsoleView_Print(_T("  Failed to add the watch.\r\n"));
    DebugView_Redraw();
}
void ConsoleView_CmdRemoveWatchAtAddress(const ConsoleCommandParams& params)
{
    uint16_t address = params.paramOct1;

    bool result = Emulator_RemoveWatchpoint(address);
    if (!result)
        ConsoleView_Print(_T("  Failed to remove the watch.\r\n"));
    DebugView_Redraw();
}
void ConsoleView_CmdRemoveAllWatches(const ConsoleCommandParams& /*params*/)
{
    Emulator_RemoveAllWatchpoints();
    DebugView_Redraw();
}

#if !defined(PRODUCT)
void ConsoleView_CmdClearTraceLog(const ConsoleCommandParams& /*params*/)
{
    DebugLogClear();
    ConsoleView_Print(_T("  Trace log cleared.\r\n"));
}
void ConsoleView_CmdTraceLogWithMask(const ConsoleCommandParams& params)
{
    ConsoleView_TraceLog(params.paramOct1);
}
void ConsoleView_CmdTraceLogOnOff(const ConsoleCommandParams& /*params*/)
{
    DWORD dwTrace = (g_pBoard->GetTrace() == TRACE_NONE ? TRACE_ALL : TRACE_NONE);
    ConsoleView_TraceLog(dwTrace);
}
#endif


//////////////////////////////////////////////////////////////////////

enum ConsoleCommandArgInfo
{
    ARGINFO_NONE,     // No parameters
    ARGINFO_REG,      // Register number 0..7
    ARGINFO_OCT,      // Octal value
    ARGINFO_REG_OCT,  // Register number, octal value
    ARGINFO_OCT_OCT,  // Octal value, octal value
};

typedef void(*CONSOLE_COMMAND_CALLBACK)(const ConsoleCommandParams& params);

struct ConsoleCommandStruct
{
    LPCTSTR pattern;
    ConsoleCommandArgInfo arginfo;
    CONSOLE_COMMAND_CALLBACK callback;
}
static ConsoleCommands[] =
{
    // IMPORTANT! First list more complex forms with more arguments, then less complex forms
    { _T("h"), ARGINFO_NONE, ConsoleView_CmdShowHelp },
    { _T("c"), ARGINFO_NONE, ConsoleView_CmdClearConsoleLog },
    { _T("r%d=%ho"), ARGINFO_REG_OCT, ConsoleView_CmdSetRegisterValue },
    { _T("r%d %ho"), ARGINFO_REG_OCT, ConsoleView_CmdSetRegisterValue },
    { _T("r%d"), ARGINFO_REG, ConsoleView_CmdPrintRegister },
    { _T("r"), ARGINFO_NONE, ConsoleView_CmdPrintAllRegisters },
    { _T("rps=%ho"), ARGINFO_OCT, ConsoleView_CmdSetRegisterPSW },
    { _T("rps %ho"), ARGINFO_OCT, ConsoleView_CmdSetRegisterPSW },
    { _T("rps"), ARGINFO_NONE, ConsoleView_CmdPrintRegisterPSW },
    { _T("s"), ARGINFO_NONE, ConsoleView_CmdStepInto },
    { _T("so"), ARGINFO_NONE, ConsoleView_CmdStepOver },
    { _T("d%ho"), ARGINFO_OCT, ConsoleView_CmdPrintDisassembleAtAddress },
    { _T("D%ho"), ARGINFO_OCT, ConsoleView_CmdPrintDisassembleAtAddress },
    { _T("d"), ARGINFO_NONE, ConsoleView_CmdPrintDisassembleAtPC },
    { _T("D"), ARGINFO_NONE, ConsoleView_CmdPrintDisassembleAtPC },
    { _T("u"), ARGINFO_NONE, ConsoleView_CmdSaveMemoryDump },
    { _T("m%ho"), ARGINFO_OCT, ConsoleView_CmdPrintMemoryDumpAtAddress },
    { _T("mr%d"), ARGINFO_REG, ConsoleView_CmdPrintMemoryDumpAtRegister },
    { _T("m"), ARGINFO_NONE, ConsoleView_CmdPrintMemoryDumpAtPC },
    { _T("mo"), ARGINFO_NONE, ConsoleView_CmdGotoMonitor },
    { _T("g%ho"), ARGINFO_OCT, ConsoleView_CmdRunToAddress },
    { _T("g"), ARGINFO_NONE, ConsoleView_CmdRun },
    { _T("b%ho"), ARGINFO_OCT, ConsoleView_CmdSetBreakpointAtAddress },
    { _T("b"), ARGINFO_NONE, ConsoleView_CmdPrintAllBreakpoints },
    { _T("bc%ho"), ARGINFO_OCT, ConsoleView_CmdRemoveBreakpointAtAddress },
    { _T("bc"), ARGINFO_NONE, ConsoleView_CmdRemoveAllBreakpoints },
    { _T("w%ho"), ARGINFO_OCT, ConsoleView_CmdSetWatchAtAddress },
    { _T("w"), ARGINFO_NONE, ConsoleView_CmdPrintAllWatches },
    { _T("wc%ho"), ARGINFO_OCT, ConsoleView_CmdRemoveWatchAtAddress },
    { _T("wc"), ARGINFO_NONE, ConsoleView_CmdRemoveAllWatches },
#if !defined(PRODUCT)
    { _T("t%ho"), ARGINFO_OCT, ConsoleView_CmdTraceLogWithMask },
    { _T("t"), ARGINFO_NONE, ConsoleView_CmdTraceLogOnOff },
    { _T("tc"), ARGINFO_NONE, ConsoleView_CmdClearTraceLog },
#endif
};
const size_t ConsoleCommandsCount = sizeof(ConsoleCommands) / sizeof(ConsoleCommands[0]);

void ConsoleView_DoConsoleCommand()
{
    // Get command text
    TCHAR command[32];
    GetWindowText(m_hwndConsoleEdit, command, 32);
    SendMessage(m_hwndConsoleEdit, WM_SETTEXT, 0, (LPARAM)_T(""));  // Clear command

    if (command[0] == 0) return;  // Nothing to do

    // Echo command to the log
    TCHAR buffer[36];
    ::GetWindowText(m_hwndConsolePrompt, buffer, 14);
    ConsoleView_Print(buffer);
    _sntprintf(buffer, sizeof(buffer) / sizeof(TCHAR) - 1, _T(" %s\r\n"), command);
    ConsoleView_Print(buffer);

    ConsoleCommandParams params;
    params.commandText = command;
    params.paramReg1 = -1;
    params.paramOct1 = 0;
    params.paramOct2 = 0;

    // Find matching console command from the list, parse and execute the command
    bool parsedOkay = false, parseError = false;
    for (size_t i = 0; i < ConsoleCommandsCount; i++)
    {
        ConsoleCommandStruct& cmd = ConsoleCommands[i];

        int paramsParsed = 0;
        switch (cmd.arginfo)
        {
        case ARGINFO_NONE:
            parsedOkay = (_tcscmp(command, cmd.pattern) == 0);
            break;
        case ARGINFO_REG:
            paramsParsed = _sntscanf_s(command, 32, cmd.pattern, &params.paramReg1);
            parsedOkay = (paramsParsed == 1);
            if (parsedOkay && params.paramReg1 < 0 || params.paramReg1 > 7)
            {
                ConsoleView_Print(MESSAGE_INVALID_REGNUM);
                parseError = true;
            }
            break;
        case ARGINFO_OCT:
            paramsParsed = _sntscanf_s(command, 32, cmd.pattern, &params.paramOct1);
            parsedOkay = (paramsParsed == 1);
            break;
        case ARGINFO_REG_OCT:
            paramsParsed = _sntscanf_s(command, 32, cmd.pattern, &params.paramReg1, &params.paramOct1);
            parsedOkay = (paramsParsed == 2);
            if (parsedOkay && params.paramReg1 < 0 || params.paramReg1 > 7)
            {
                ConsoleView_Print(MESSAGE_INVALID_REGNUM);
                parseError = true;
            }
            break;
        case ARGINFO_OCT_OCT:
            paramsParsed = _sntscanf_s(command, 32, cmd.pattern, &params.paramOct1, &params.paramOct2);
            parsedOkay = (paramsParsed == 2);
            break;
        }

        if (parseError)
            break;  // Validation detected error and printed the message already

        if (parsedOkay)
        {
            cmd.callback(params);  // Execute the command
            break;
        }
    }

    if (!parsedOkay && !parseError)
        ConsoleView_Print(MESSAGE_UNKNOWN_COMMAND);

    ConsoleView_PrintConsolePrompt();
}


//////////////////////////////////////////////////////////////////////
