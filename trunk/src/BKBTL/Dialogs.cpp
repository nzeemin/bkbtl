// Dialogs.cpp

#include "stdafx.h"
#include <commdlg.h>
#include "Dialogs.h"
#include "Emulator.h"
#include "BKBTL.h"

//////////////////////////////////////////////////////////////////////


INT_PTR CALLBACK AboutBoxProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK InputBoxProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK LoadBinProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void Dialogs_DoLoadBinPrepare(HWND hDlg, LPCTSTR strFileName);
void Dialogs_DoLoadBinLoad(LPCTSTR strFileName);
BOOL InputBoxValidate(HWND hDlg);

LPCTSTR m_strInputBoxTitle = NULL;
LPCTSTR m_strInputBoxPrompt = NULL;
WORD* m_pInputBoxValueOctal = NULL;


//////////////////////////////////////////////////////////////////////
// About Box

void ShowAboutBox()
{
    DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), g_hwnd, AboutBoxProc);
}

INT_PTR CALLBACK AboutBoxProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        {
            TCHAR buf[64];
            wsprintf(buf, _T("%S %S"), __DATE__, __TIME__);
            ::SetWindowText(::GetDlgItem(hDlg, IDC_BUILDDATE), buf);
            return (INT_PTR)TRUE;
        }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}


//////////////////////////////////////////////////////////////////////


BOOL InputBoxOctal(HWND hwndOwner, LPCTSTR strTitle, LPCTSTR strPrompt, WORD* pValue)
{
    m_strInputBoxTitle = strTitle;
    m_strInputBoxPrompt = strPrompt;
    m_pInputBoxValueOctal = pValue;
    INT_PTR result = DialogBox(g_hInst, MAKEINTRESOURCE(IDD_INPUTBOX), hwndOwner, InputBoxProc);
    if (result != IDOK)
        return FALSE;

    return TRUE;
}


INT_PTR CALLBACK InputBoxProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        {
            SetWindowText(hDlg, m_strInputBoxTitle);
            HWND hStatic = GetDlgItem(hDlg, IDC_STATIC);
            SetWindowText(hStatic, m_strInputBoxPrompt);
            HWND hEdit = GetDlgItem(hDlg, IDC_EDIT1);

            TCHAR buffer[8];
            _snwprintf_s(buffer, 8, _T("%06o"), *m_pInputBoxValueOctal);
            SetWindowText(hEdit, buffer);
            SendMessage(hEdit, EM_SETSEL, 0, -1);

            SetFocus(hEdit);
            return (INT_PTR)FALSE;
        }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            if (! InputBoxValidate(hDlg))
                return (INT_PTR) FALSE;
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        case IDCANCEL:
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        default:
            return (INT_PTR) FALSE;
        }
        break;
    }
    return (INT_PTR) FALSE;
}

BOOL InputBoxValidate(HWND hDlg) {
    HWND hEdit = GetDlgItem(hDlg, IDC_EDIT1);
    TCHAR buffer[8];
    GetWindowText(hEdit, buffer, 8);

    WORD value;
    if (! ParseOctalValue(buffer, &value))
    {
        MessageBox(NULL, _T("Please enter correct octal value."), _T("Input Box Validation"),
                MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
        return FALSE;
    }

    *m_pInputBoxValueOctal = value;

    return TRUE;
}


//////////////////////////////////////////////////////////////////////


BOOL ShowSaveDialog(HWND hwndOwner, LPCTSTR strTitle, LPCTSTR strFilter, TCHAR* bufFileName)
{
    *bufFileName = 0;
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwndOwner;
    ofn.hInstance = g_hInst;
    ofn.lpstrTitle = strTitle;
    ofn.lpstrFilter = strFilter;
    ofn.Flags = OFN_FILEMUSTEXIST;
    ofn.lpstrFile = bufFileName;
    ofn.nMaxFile = MAX_PATH;

    BOOL okResult = GetSaveFileName(&ofn);
    return okResult;
}

BOOL ShowOpenDialog(HWND hwndOwner, LPCTSTR strTitle, LPCTSTR strFilter, TCHAR* bufFileName)
{
    *bufFileName = 0;
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwndOwner;
    ofn.hInstance = g_hInst;
    ofn.lpstrTitle = strTitle;
    ofn.lpstrFilter = strFilter;
    ofn.Flags = OFN_FILEMUSTEXIST;
    ofn.lpstrFile = bufFileName;
    ofn.nMaxFile = MAX_PATH;

    BOOL okResult = GetOpenFileName(&ofn);
    return okResult;
}


//////////////////////////////////////////////////////////////////////
// Create Disk Dialog

void ShowLoadBinDialog()
{
    DialogBox(g_hInst, MAKEINTRESOURCE(IDD_LOADBIN), g_hwnd, LoadBinProc);
}

INT_PTR CALLBACK LoadBinProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        {
            ::PostMessage(hDlg, WM_COMMAND, IDC_BUTTONBROWSE, 0);
            return (INT_PTR)FALSE;
        }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_BUTTONBROWSE:
            {
                TCHAR bufFileName[MAX_PATH];
                BOOL okResult = ShowOpenDialog(g_hwnd,
                    _T("Select BIN file to load"),
                    _T("BK binary files (*.bin)\0*.bin\0All Files (*.*)\0*.*\0\0"),
                    bufFileName);
                if (! okResult) break;

                Dialogs_DoLoadBinPrepare(hDlg, bufFileName);
            }
            break;
        case IDOK:
            {
                TCHAR bufFileName[MAX_PATH];
                ::GetDlgItemText(hDlg, IDC_EDITFILE, bufFileName, sizeof(bufFileName) / sizeof(TCHAR));
                Dialogs_DoLoadBinLoad(bufFileName);
            }
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        case IDCANCEL:
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        default:
            return (INT_PTR)FALSE;
        }
        break;
    }
    return (INT_PTR) FALSE;
}

void Dialogs_DoLoadBinPrepare(HWND hDlg, LPCTSTR strFileName)
{
    ::SetDlgItemText(hDlg, IDC_EDITFILE, NULL);

    // Open file for reading
    HANDLE hFile = CreateFile(strFileName,
            GENERIC_READ, FILE_SHARE_READ, NULL,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        AlertWarning(_T("Failed to load binary file."));
        return;
    }
    
    // Load BIN header
    BYTE bufHeader[20];
	DWORD bytesRead;
	::ReadFile(hFile, bufHeader, 4, &bytesRead, NULL);
    if (bytesRead != 4)
    {
        ::CloseHandle(hFile);
        AlertWarning(_T("Failed to load binary file."));
        return;
    }

    WORD baseAddress = *((WORD*)bufHeader);
    WORD dataSize = *(((WORD*)bufHeader) + 1);

    //TCHAR bufName[17];
    //::MultiByteToWideChar(CP_ACP, 0, (LPCSTR)(bufHeader + 4), 16, bufName, 17);
    //bufName[16] = 0;

    // Set controls text
    TCHAR bufValue[8];
    ::SetDlgItemText(hDlg, IDC_EDITFILE, strFileName);
    PrintOctalValue(bufValue, baseAddress);
    ::SetDlgItemText(hDlg, IDC_EDITADDR, bufValue);
    PrintOctalValue(bufValue, dataSize);
    ::SetDlgItemText(hDlg, IDC_EDITSIZE, bufValue);
    //::SetDlgItemText(hDlg, IDC_EDITNAME, bufName);
}

void Dialogs_DoLoadBinLoad(LPCTSTR strFileName)
{
    // Open file for reading
    HANDLE hFile = CreateFile(strFileName,
            GENERIC_READ, FILE_SHARE_READ, NULL,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        AlertWarning(_T("Failed to load binary file."));
        return;
    }

    // Load BIN header
    BYTE bufHeader[20];
	DWORD bytesRead;
	::ReadFile(hFile, bufHeader, 4, &bytesRead, NULL);
    if (bytesRead != 4)
    {
        ::CloseHandle(hFile);
        AlertWarning(_T("Failed to load binary file."));
        return;
    }

    WORD baseAddress = *((WORD*)bufHeader);
    WORD dataSize = *(((WORD*)bufHeader) + 1);

    // Get file size
    DWORD bytesToRead = dataSize;
    WORD memoryBytes = (dataSize + 1) & 0xfffe;

    // Allocate memory
    BYTE* pBuffer = (BYTE*)::LocalAlloc(LPTR, memoryBytes);

    // Load file data
	::ReadFile(hFile, pBuffer, dataSize, &bytesRead, NULL);
    if (bytesRead != bytesToRead)
    {
        ::LocalFree(pBuffer);
        ::CloseHandle(hFile);
        AlertWarning(_T("Failed to load binary file."));
        return;
    }

    // Copy data to BK memory
    WORD address = baseAddress;
    WORD* pData = (WORD*)pBuffer;
    while (address < baseAddress + memoryBytes)
    {
        WORD value = *pData++;
        g_pBoard->SetRAMWord(address, value);
        address += 2;
    }

    ::LocalFree(pBuffer);
    ::CloseHandle(hFile);
}


//////////////////////////////////////////////////////////////////////
