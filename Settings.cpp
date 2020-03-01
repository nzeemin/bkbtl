/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// Settings.cpp

#include "stdafx.h"
#include "Main.h"


//////////////////////////////////////////////////////////////////////


const TCHAR m_Settings_IniAppName[] = _T("BKBTL");
TCHAR m_Settings_IniPath[MAX_PATH];


//////////////////////////////////////////////////////////////////////


void Settings_Init()
{
    // Prepare m_Settings_IniPath: get .exe file path and change extension to .ini
    ::GetModuleFileName(GetModuleHandle(NULL), m_Settings_IniPath, MAX_PATH);
    TCHAR* pExt = m_Settings_IniPath + _tcslen(m_Settings_IniPath) - 3;
    *pExt++ = _T('i');
    *pExt++ = _T('n');
    *pExt++ = _T('i');
}
void Settings_Done()
{
}

BOOL Settings_SaveStringValue(LPCTSTR sName, LPCTSTR sValue)
{
    BOOL result = WritePrivateProfileString(
            m_Settings_IniAppName, sName, sValue, m_Settings_IniPath);
    return result;
}
BOOL Settings_LoadStringValue(LPCTSTR sName, LPTSTR sBuffer, int nBufferLengthChars)
{
    DWORD result = GetPrivateProfileString(
            m_Settings_IniAppName, sName, NULL, sBuffer, nBufferLengthChars, m_Settings_IniPath);
    if (result > 0)
        return TRUE;

    sBuffer[0] = _T('\0');
    return FALSE;
}

BOOL Settings_SaveDwordValue(LPCTSTR sName, DWORD dwValue)
{
    TCHAR buffer[12];
    wsprintf(buffer, _T("%lu"), dwValue);

    return Settings_SaveStringValue(sName, buffer);
}
BOOL Settings_LoadDwordValue(LPCTSTR sName, DWORD* dwValue)
{
    TCHAR buffer[12];
    if (!Settings_LoadStringValue(sName, buffer, 12))
        return FALSE;

    int result = swscanf(buffer, _T("%lu"), dwValue);
    if (result == 0)
        return FALSE;

    return TRUE;
}

BOOL Settings_SaveBinaryValue(LPCTSTR sName, const void * pData, int size)
{
    TCHAR* buffer = static_cast<TCHAR*>(::calloc(size * 2 + 1, sizeof(TCHAR)));
    if (buffer == NULL)
        return FALSE;
    const BYTE* p = (const BYTE*) pData;
    TCHAR* buf = buffer;
    for (int i = 0; i < size; i++)
    {
        int a = *p;
        wsprintf(buf, _T("%02X"), a);
        p++;
        buf += 2;
    }

    BOOL result = Settings_SaveStringValue(sName, buffer);

    free(buffer);

    return result;
}

BOOL Settings_LoadBinaryValue(LPCTSTR sName, void * pData, int size)
{
    size_t buffersize = size * 2 + 1;
    TCHAR* buffer = static_cast<TCHAR*>(::calloc(buffersize, sizeof(TCHAR)));
    if (buffer == NULL)
        return FALSE;
    if (!Settings_LoadStringValue(sName, buffer, buffersize))
    {
        free(buffer);
        return FALSE;
    }

    BYTE* p = (BYTE*) pData;
    TCHAR* buf = buffer;
    for (int i = 0; i < size; i++)
    {
        BYTE v = 0;

        TCHAR ch = *buf;
        if (ch >= _T('0') && ch <= _T('9'))
            v = (BYTE)(ch - _T('0'));
        else if (ch >= _T('A') && ch <= _T('F'))
            v = (BYTE)(ch - _T('A') + 10);
        else  // Not hex
        {
            free(buffer);
            return FALSE;
        }
        buf++;

        v = v << 4;

        ch = *buf;
        if (ch >= _T('0') && ch <= _T('9'))
            v |= (BYTE)(ch - _T('0'));
        else if (ch >= _T('A') && ch <= _T('F'))
            v |= (BYTE)(ch - _T('A') + 10);
        else  // Not hex
        {
            free(buffer);
            return FALSE;
        }
        buf++;

        *p = v;
        p++;
    }

    free(buffer);

    return TRUE;
}


//////////////////////////////////////////////////////////////////////


#define SETTINGS_GETSET_DWORD(PARAMNAME, PARAMNAMESTR, OUTTYPE, DEFVALUE) \
    OUTTYPE m_Settings_##PARAMNAME = DEFVALUE; \
    BOOL m_Settings_##PARAMNAME##_Valid = FALSE; \
    void Settings_Set##PARAMNAME(OUTTYPE newvalue) { \
        m_Settings_##PARAMNAME = newvalue; \
        m_Settings_##PARAMNAME##_Valid = TRUE; \
        Settings_SaveDwordValue(PARAMNAMESTR, (DWORD) newvalue); \
    } \
    OUTTYPE Settings_Get##PARAMNAME##() { \
        if (!m_Settings_##PARAMNAME##_Valid) { \
            DWORD dwValue = (DWORD) DEFVALUE; \
            Settings_LoadDwordValue(PARAMNAMESTR, &dwValue); \
            m_Settings_##PARAMNAME = (OUTTYPE) dwValue; \
            m_Settings_##PARAMNAME##_Valid = TRUE; \
        } \
        return m_Settings_##PARAMNAME; \
    }


BOOL Settings_GetWindowRect(RECT * pRect)
{
    RECT rc;
    if (Settings_LoadBinaryValue(_T("WindowRect"), &rc, sizeof(RECT)))
    {
        ::memcpy(pRect, &rc, sizeof(RECT));
        return TRUE;
    }

    return FALSE;
}
void Settings_SetWindowRect(const RECT * pRect)
{
    Settings_SaveBinaryValue(_T("WindowRect"), (const void *)pRect, sizeof(RECT));
}

SETTINGS_GETSET_DWORD(WindowMaximized, _T("WindowMaximized"), BOOL, FALSE);

SETTINGS_GETSET_DWORD(WindowFullscreen, _T("WindowFullscreen"), BOOL, FALSE);

SETTINGS_GETSET_DWORD(Configuration, _T("Configuration"), int, 0);

void Settings_GetFloppyFilePath(int slot, LPTSTR buffer)
{
    TCHAR bufValueName[8];
    lstrcpy(bufValueName, _T("Floppy0"));
    bufValueName[6] = _T('0') + (TCHAR)slot;
    Settings_LoadStringValue(bufValueName, buffer, MAX_PATH);
}
void Settings_SetFloppyFilePath(int slot, LPCTSTR sFilePath)
{
    TCHAR bufValueName[8];
    lstrcpy(bufValueName, _T("Floppy0"));
    bufValueName[6] = _T('0') + (TCHAR)slot;
    Settings_SaveStringValue(bufValueName, sFilePath);
}

SETTINGS_GETSET_DWORD(ScreenViewMode, _T("ScreenViewMode"), int, 0);

SETTINGS_GETSET_DWORD(ScreenHeightMode, _T("ScreenHeightMode"), int, 0);

SETTINGS_GETSET_DWORD(Toolbar, _T("Toolbar"), BOOL, TRUE);

SETTINGS_GETSET_DWORD(Debug, _T("Debug"), BOOL, FALSE);

void Settings_GetDebugFontName(LPTSTR buffer)
{
    if (!Settings_LoadStringValue(_T("DebugFontName"), buffer, 32))
    {
        _tcscpy(buffer, _T("Lucida Console"));
    }
}
void Settings_SetDebugFontName(LPCTSTR sFontName)
{
    Settings_SaveStringValue(_T("DebugFontName"), sFontName);
}

SETTINGS_GETSET_DWORD(DebugMemoryAddress, _T("DebugMemoryAddress"), WORD, 3);
SETTINGS_GETSET_DWORD(DebugMemoryByte, _T("DebugMemoryByte"), BOOL, FALSE);

SETTINGS_GETSET_DWORD(Autostart, _T("Autostart"), BOOL, FALSE);

SETTINGS_GETSET_DWORD(RealSpeed, _T("RealSpeed"), WORD, 1);

SETTINGS_GETSET_DWORD(Sound, _T("Sound"), BOOL, FALSE);
SETTINGS_GETSET_DWORD(SoundVolume, _T("SoundVolume"), WORD, 0x3fff);
SETTINGS_GETSET_DWORD(SoundCovox, _T("SoundCovox"), BOOL, FALSE);
SETTINGS_GETSET_DWORD(SoundAY, _T("SoundAY"), BOOL, FALSE);

SETTINGS_GETSET_DWORD(Joystick, _T("Joystick"), int, 0);

SETTINGS_GETSET_DWORD(Keyboard, _T("Keyboard"), BOOL, TRUE);

SETTINGS_GETSET_DWORD(Tape, _T("Tape"), BOOL, FALSE);

SETTINGS_GETSET_DWORD(MemoryMap, _T("MemoryMap"), BOOL, FALSE);

SETTINGS_GETSET_DWORD(SpriteAddress, _T("SpriteAddress"), WORD, 0);

SETTINGS_GETSET_DWORD(SpriteWidth, _T("SpriteWidth"), WORD, 2);


//////////////////////////////////////////////////////////////////////
