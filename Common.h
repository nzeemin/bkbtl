/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// Common.h

#pragma once

//////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
#define BKBTL_VERSION_STRING "DEBUG"
#elif !defined(PRODUCT)
#define BKBTL_VERSION_STRING "RELEASE"
#else
#include "Version.h"
#endif


//////////////////////////////////////////////////////////////////////
// Assertions checking - MFC-like ASSERT macro

#ifdef _DEBUG

BOOL AssertFailedLine(LPCSTR lpszFileName, int nLine);
#define ASSERT(f)          (void) ((f) || !AssertFailedLine(__FILE__, __LINE__) || (DebugBreak(), 0))
#define VERIFY(f)          ASSERT(f)

#else   // _DEBUG

#define ASSERT(f)          ((void)0)
#define VERIFY(f)          ((void)f)

#endif // !_DEBUG


//////////////////////////////////////////////////////////////////////
// Alerts

void AlertWarning(LPCTSTR sMessage);
void AlertWarningFormat(LPCTSTR sFormat, ...);
BOOL AlertOkCancel(LPCTSTR sMessage);


//////////////////////////////////////////////////////////////////////
// DebugPrint

#if !defined(PRODUCT)

void DebugPrint(LPCTSTR message);
void DebugPrintFormat(LPCTSTR pszFormat, ...);
void DebugLogClear();
void DebugLog(LPCTSTR message);
void DebugLogFormat(LPCTSTR pszFormat, ...);

#endif // !defined(PRODUCT)


//////////////////////////////////////////////////////////////////////


// Processor register names
const TCHAR* REGISTER_NAME[];

const int BK_SCREEN_WIDTH = 512;
const int BK_SCREEN_HEIGHT = 256;


HFONT CreateMonospacedFont();
HFONT CreateDialogFont();
void GetFontWidthAndHeight(HDC hdc, int* pWidth, int* pHeight);
void PrintOctalValue(TCHAR* buffer, WORD value);
void PrintHexValue(TCHAR* buffer, WORD value);
void PrintBinaryValue(TCHAR* buffer, WORD value);
BOOL ParseOctalValue(LPCTSTR text, WORD* pValue);
void DrawOctalValue(HDC hdc, int x, int y, WORD value);
void DrawHexValue(HDC hdc, int x, int y, WORD value);
void DrawBinaryValue(HDC hdc, int x, int y, WORD value);
BOOL ParseOctalValue(LPCTSTR text, WORD* pValue);
TCHAR Translate_BK_Unicode(BYTE ch);
TCHAR Translate_KOI7R_Unicode(BYTE ch);

LPCTSTR GetFileNameFromFilePath(LPCTSTR lpfilepath);


//////////////////////////////////////////////////////////////////////
