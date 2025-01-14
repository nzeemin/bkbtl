﻿/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// Emulator.cpp

#include "stdafx.h"
#include <cstdio>
#include <share.h>
#include "Main.h"
#include "Emulator.h"
#include "Views.h"
#include "Dialogs.h"
#include "emubase/Emubase.h"
#include "SoundGen.h"
#include "Joystick.h"

//////////////////////////////////////////////////////////////////////


CMotherboard* g_pBoard = nullptr;
BKConfiguration g_nEmulatorConfiguration;  // Current configuration
bool g_okEmulatorRunning = false;

int m_wEmulatorCPUBpsCount = 0;
uint16_t m_EmulatorCPUBps[MAX_BREAKPOINTCOUNT + 1];
uint16_t m_wEmulatorTempCPUBreakpoint = 0177777;
int m_wEmulatorWatchesCount = 0;
uint16_t m_EmulatorWatches[MAX_BREAKPOINTCOUNT + 1];

bool m_okEmulatorSound = false;
uint16_t m_wEmulatorSoundSpeed = 100;
bool m_okEmulatorCovox = false;
bool m_okEmulatorSoundAY = false;
int m_nEmulatorSoundChanges = 0;

long m_nFrameCount = 0;
uint32_t m_dwTickCount = 0;
uint32_t m_dwEmulatorUptime = 0;  // Machine uptime, seconds, from turn on or reset, increments every 25 frames
long m_nUptimeFrameCount = 0;

uint8_t* g_pEmulatorRam = nullptr;  // RAM values - for change tracking
uint8_t* g_pEmulatorChangedRam = nullptr;  // RAM change flags
uint16_t g_wEmulatorCpuPC = 0177777;      // Current PC value
uint16_t g_wEmulatorPrevCpuPC = 0177777;  // Previous PC value

void Emulator_FakeTape_ReadFile();
void Emulator_FakeTape_WriteFile();

void CALLBACK Emulator_SoundGenCallback(unsigned short L, unsigned short R);
void CALLBACK Emulator_TeletypeCallback(uint8_t symbol);

enum
{
    TAPEMODE_STOPPED = 0,
    TAPEMODE_STARTED = 1,
    TAPEMODE_READING = 2,
    TAPEMODE_FINISHED = -1
} m_EmulatorTapeMode;
int m_EmulatorTapeCount = 0;

//Прототип функции преобразования экрана
// Input:
//   pVideoBuffer   Исходные данные, биты экрана БК
//   okSmallScreen  Признак "малого" экрана
//   pPalette       Палитра
//   scroll         Текущее значение скроллинга
//   pImageBits     Результат, 32-битный цвет, размер для каждой функции свой
typedef void (CALLBACK* PREPARE_SCREEN_CALLBACK)(const uint8_t* pVideoBuffer, int okSmallScreen, const uint32_t* pPalette, int scroll, void* pImageBits);

void CALLBACK Emulator_PrepareScreenBW512x256(const uint8_t* pVideoBuffer, int okSmallScreen, const uint32_t* pPalette, int scroll, void* pImageBits);
void CALLBACK Emulator_PrepareScreenColor512x256(const uint8_t* pVideoBuffer, int okSmallScreen, const uint32_t* pPalette, int scroll, void* pImageBits);
void CALLBACK Emulator_PrepareScreenBW512x384(const uint8_t* pVideoBuffer, int okSmallScreen, const uint32_t* pPalette, int scroll, void* pImageBits);
void CALLBACK Emulator_PrepareScreenColor512x384(const uint8_t* pVideoBuffer, int okSmallScreen, const uint32_t* pPalette, int scroll, void* pImageBits);
void CALLBACK Emulator_PrepareScreenBW896x512(const uint8_t* pVideoBuffer, int okSmallScreen, const uint32_t* pPalette, int scroll, void* pImageBits);
void CALLBACK Emulator_PrepareScreenColor896x512(const uint8_t* pVideoBuffer, int okSmallScreen, const uint32_t* pPalette, int scroll, void* pImageBits);
void CALLBACK Emulator_PrepareScreenBW1024x768(const uint8_t* pVideoBuffer, int okSmallScreen, const uint32_t* pPalette, int scroll, void* pImageBits);
void CALLBACK Emulator_PrepareScreenColor1024x768(const uint8_t* pVideoBuffer, int okSmallScreen, const uint32_t* pPalette, int scroll, void* pImageBits);
void CALLBACK Emulator_PrepareScreenBW1536x1024(const uint8_t* pVideoBuffer, int okSmallScreen, const uint32_t* pPalette, int scroll, void* pImageBits);
void CALLBACK Emulator_PrepareScreenColor1536x1024(const uint8_t* pVideoBuffer, int okSmallScreen, const uint32_t* pPalette, int scroll, void* pImageBits);

struct ScreenModeStruct
{
    int width;
    int height;
    PREPARE_SCREEN_CALLBACK callback;
}
static ScreenModeReference[] =
{
    {  512, 256, Emulator_PrepareScreenBW512x256 },
    {  512, 256, Emulator_PrepareScreenColor512x256 },
    {  512, 384, Emulator_PrepareScreenBW512x384 },
    {  512, 384, Emulator_PrepareScreenColor512x384 },
    {  896, 512, Emulator_PrepareScreenBW896x512 },
    {  896, 512, Emulator_PrepareScreenColor896x512 },
    { 1024, 768, Emulator_PrepareScreenBW1024x768 },
    { 1024, 768, Emulator_PrepareScreenColor1024x768 },
    { 1536, 1024, Emulator_PrepareScreenBW1536x1024 },
    { 1536, 1024, Emulator_PrepareScreenColor1536x1024 },
};

//////////////////////////////////////////////////////////////////////


const LPCTSTR FILENAME_BKROM_MONIT10    = _T("monit10.rom");
const LPCTSTR FILENAME_BKROM_FOCAL      = _T("focal.rom");
const LPCTSTR FILENAME_BKROM_TESTS      = _T("tests.rom");
const LPCTSTR FILENAME_BKROM_BASIC10_1  = _T("basic10_1.rom");
const LPCTSTR FILENAME_BKROM_BASIC10_2  = _T("basic10_2.rom");
const LPCTSTR FILENAME_BKROM_BASIC10_3  = _T("basic10_3.rom");
const LPCTSTR FILENAME_BKROM_DISK_326   = _T("disk_326.rom");
const LPCTSTR FILENAME_BKROM_BK11M_BOS  = _T("b11m_bos.rom");
const LPCTSTR FILENAME_BKROM_BK11M_EXT  = _T("b11m_ext.rom");
const LPCTSTR FILENAME_BKROM_BASIC11M_0 = _T("basic11m_0.rom");
const LPCTSTR FILENAME_BKROM_BASIC11M_1 = _T("basic11m_1.rom");
const LPCTSTR FILENAME_BKROM_BK11M_MSTD = _T("b11m_mstd.rom");


//////////////////////////////////////////////////////////////////////
// Colors

const uint32_t ScreenView_BWPalette[4] =
{
    0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF
};

const uint32_t ScreenView_ColorPalette[4] =
{
    0x000000, 0x0000FF, 0x00FF00, 0xFF0000
};

const uint32_t ScreenView_ColorPalettes[16][4] =
{
    //                                     Palette#     01           10          11
    0x000000, 0x0000FF, 0x00FF00, 0xFF0000,  // 00    синий   |   зеленый  |  красный
    0x000000, 0xFFFF00, 0xFF00FF, 0xFF0000,  // 01   желтый   |  сиреневый |  красный
    0x000000, 0x00FFFF, 0x0000FF, 0xFF00FF,  // 02   голубой  |    синий   | сиреневый
    0x000000, 0x00FF00, 0x00FFFF, 0xFFFF00,  // 03   зеленый  |   голубой  |  желтый
    0x000000, 0xFF00FF, 0x00FFFF, 0xFFFFFF,  // 04  сиреневый |   голубой  |   белый
    0x000000, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF,  // 05    белый   |    белый   |   белый
    0x000000, 0xC00000, 0x8E0000, 0xFF0000,  // 06  темн-красн| красн-корич|  красный
    0x000000, 0xC0FF00, 0x8EFF00, 0xFFFF00,  // 07  салатовый | светл-зелен|  желтый
    0x000000, 0xC000FF, 0x8E00FF, 0xFF00FF,  // 08  фиолетовый| фиол-синий | сиреневый
    0x000000, 0x8EFF00, 0x8E00FF, 0x8E0000,  // 09 светл-зелен| фиол-синий |красн-корич
    0x000000, 0xC0FF00, 0xC000FF, 0xC00000,  // 10  салатовый | фиолетовый |темн-красный
    0x000000, 0x00FFFF, 0xFFFF00, 0xFF0000,  // 11   голубой  |   желтый   |  красный
    0x000000, 0xFF0000, 0x00FF00, 0x00FFFF,  // 12   красный  |   зеленый  |  голубой
    0x000000, 0x00FFFF, 0xFFFF00, 0xFFFFFF,  // 13   голубой  |   желтый   |   белый
    0x000000, 0xFFFF00, 0x00FF00, 0xFFFFFF,  // 14   желтый   |   зеленый  |   белый
    0x000000, 0x00FFFF, 0x00FF00, 0xFFFFFF,  // 15   голубой  |   зеленый  |   белый
};


//////////////////////////////////////////////////////////////////////

bool Emulator_LoadRomFile(LPCTSTR strFileName, uint8_t* buffer, uint32_t fileOffset, uint32_t bytesToRead)
{
    FILE* fpRomFile = ::_tfsopen(strFileName, _T("rb"), _SH_DENYWR);
    if (fpRomFile == nullptr)
        return false;

    ASSERT(bytesToRead <= 8192);
    ::memset(buffer, 0, 8192);

    if (fileOffset > 0)
    {
        ::fseek(fpRomFile, fileOffset, SEEK_SET);
    }

    size_t dwBytesRead = ::fread(buffer, 1, bytesToRead, fpRomFile);
    if (dwBytesRead != bytesToRead)
    {
        ::fclose(fpRomFile);
        return false;
    }

    ::fclose(fpRomFile);

    return true;
}

bool Emulator_Init()
{
    ASSERT(g_pBoard == nullptr);

    CProcessor::Init();

    m_wEmulatorCPUBpsCount = 0;
    for (int i = 0; i <= MAX_BREAKPOINTCOUNT; i++)
    {
        uint16_t address = Settings_GetDebugBreakpoint(i);
        m_EmulatorCPUBps[i] = address;
        if (address != 0177777) m_wEmulatorCPUBpsCount = i + 1;
    }
    m_wEmulatorWatchesCount = 0;
    for (int i = 0; i <= MAX_WATCHESCOUNT; i++)
    {
        m_EmulatorWatches[i] = 0177777;
    }

    g_pBoard = new CMotherboard();

    // Allocate memory for old RAM values
    g_pEmulatorRam = (uint8_t*) ::calloc(65536, 1);
    g_pEmulatorChangedRam = (uint8_t*) ::calloc(65536, 1);

    g_pBoard->Reset();

    if (m_okEmulatorSound)
    {
        SoundGen_Initialize(Settings_GetSoundVolume());
        g_pBoard->SetSoundGenCallback(Emulator_SoundGenCallback);
    }

    g_pBoard->SetTeletypeCallback(Emulator_TeletypeCallback);

    m_EmulatorTapeMode = TAPEMODE_STOPPED;

    return true;
}

void Emulator_Done()
{
    ASSERT(g_pBoard != nullptr);

    // Save breakpoints
    for (int i = 0; i < MAX_BREAKPOINTCOUNT; i++)
        Settings_SetDebugBreakpoint(i, i < m_wEmulatorCPUBpsCount ? m_EmulatorCPUBps[i] : 0177777);

    CProcessor::Done();

    g_pBoard->SetSoundGenCallback(nullptr);
    SoundGen_Finalize();

    delete g_pBoard;
    g_pBoard = nullptr;

    // Free memory used for old RAM values
    ::free(g_pEmulatorRam);
    ::free(g_pEmulatorChangedRam);
}

BKConfiguration Emulator_GetConfiguration()
{
    return (BKConfiguration)g_pBoard->GetConfiguration();
}

LPCTSTR Emulator_GetConfigurationName()
{
    BKConfiguration configuration = Emulator_GetConfiguration();

    switch (configuration)
    {
    case BK_CONF_BK0010_BASIC:  return _T("BK-0010.01 BASIC");
    case BK_CONF_BK0010_FOCAL:  return _T("BK-0010.01 FOCAL");
    case BK_CONF_BK0010_FDD:    return _T("BK-0010.01 FDD");
    case BK_CONF_BK0011:        return _T("BK 0011M");
    case BK_CONF_BK0011_FDD:    return _T("BK 0011M FDD");
    default:  return _T("UNKNOWN");
    }
}

bool Emulator_InitConfiguration(BKConfiguration configuration)
{
    g_pBoard->SetConfiguration((uint16_t)configuration);

    uint8_t buffer[8192];

    if ((configuration & BK_COPT_BK0011) == 0)
    {
        // Load Monitor ROM file
        if (!Emulator_LoadRomFile(FILENAME_BKROM_MONIT10, buffer, 0, 8192))
        {
            AlertWarning(_T("Failed to load Monitor ROM file."));
            return false;
        }
        g_pBoard->LoadROM(0, buffer);
    }

    if ((configuration & BK_COPT_BK0011) == 0 && (configuration & BK_COPT_ROM_BASIC) != 0 ||
        (configuration & BK_COPT_BK0011) == 0 && (configuration & BK_COPT_FDD) != 0)  // BK 0010 BASIC ROM 1-2
    {
        // Load BASIC ROM 1 file
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BASIC10_1, buffer, 0, 8192))
        {
            AlertWarning(_T("Failed to load BASIC ROM 1 file."));
            return false;
        }
        g_pBoard->LoadROM(1, buffer);
        // Load BASIC ROM 2 file
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BASIC10_2, buffer, 0, 8192))
        {
            AlertWarning(_T("Failed to load BASIC ROM 2 file."));
            return false;
        }
        g_pBoard->LoadROM(2, buffer);
    }
    if ((configuration & BK_COPT_BK0011) == 0 && (configuration & BK_COPT_ROM_BASIC) != 0)  // BK 0010 BASIC ROM 3
    {
        // Load BASIC ROM 3 file
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BASIC10_3, buffer, 0, 8064))
        {
            AlertWarning(_T("Failed to load BASIC ROM 3 file."));
            return false;
        }
        g_pBoard->LoadROM(3, buffer);
    }
    if ((configuration & BK_COPT_BK0011) == 0 && (configuration & BK_COPT_ROM_FOCAL) != 0)  // BK 0010 FOCAL
    {
        // Load Focal ROM file
        if (!Emulator_LoadRomFile(FILENAME_BKROM_FOCAL, buffer, 0, 8192))
        {
            AlertWarning(_T("Failed to load Focal ROM file."));
            return false;
        }
        g_pBoard->LoadROM(1, buffer);
        // Unused 8KB
        ::memset(buffer, 0, 8192);
        g_pBoard->LoadROM(2, buffer);
        // Load Tests ROM file
        if (!Emulator_LoadRomFile(FILENAME_BKROM_TESTS, buffer, 0, 8064))
        {
            AlertWarning(_T("Failed to load Tests ROM file."));
            return false;
        }
        g_pBoard->LoadROM(3, buffer);
    }

    if (configuration & BK_COPT_BK0011)
    {
        // Load BK0011M BASIC 0, part 1
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BASIC11M_0, buffer, 0, 8192))
        {
            AlertWarning(_T("Failed to load BK11M BASIC 0 ROM file."));
            return false;
        }
        g_pBoard->LoadROM(0, buffer);
        // Load BK0011M BASIC 0, part 2
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BASIC11M_0, buffer, 8192, 8192))
        {
            AlertWarning(_T("Failed to load BK11M BASIC 0 ROM file."));
            return false;
        }
        g_pBoard->LoadROM(1, buffer);
        // Load BK0011M BASIC 1
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BASIC11M_1, buffer, 0, 8192))
        {
            AlertWarning(_T("Failed to load BK11M BASIC 1 ROM file."));
            return false;
        }
        g_pBoard->LoadROM(2, buffer);

        // Load BK0011M EXT
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BK11M_EXT, buffer, 0, 8192))
        {
            AlertWarning(_T("Failed to load BK11M EXT ROM file."));
            return false;
        }
        g_pBoard->LoadROM(3, buffer);
        // Load BK0011M BOS
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BK11M_BOS, buffer, 0, 8192))
        {
            AlertWarning(_T("Failed to load BK11M BOS ROM file."));
            return false;
        }
        g_pBoard->LoadROM(4, buffer);
    }

    if (configuration & BK_COPT_FDD)
    {
        // Load disk driver ROM file
        ::memset(buffer, 0, 8192);
        if (!Emulator_LoadRomFile(FILENAME_BKROM_DISK_326, buffer, 0, 4096))
        {
            AlertWarning(_T("Failed to load DISK ROM file."));
            return false;
        }
        g_pBoard->LoadROM((configuration & BK_COPT_BK0011) ? 5 : 3, buffer);
    }

    if ((configuration & BK_COPT_BK0011) && (configuration & BK_COPT_FDD) == 0)
    {
        // Load BK0011M MSTD
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BK11M_MSTD, buffer, 0, 8192))
        {
            AlertWarning(_T("Failed to load BK11M MSTD ROM file."));
            return false;
        }
        g_pBoard->LoadROM(5, buffer);
    }

    g_nEmulatorConfiguration = configuration;

    g_pBoard->Reset();

#if 0  //DEBUG: CPU and memory tests
    //Emulator_LoadRomFile(_T("791401"), buffer, 8192);
    //g_pBoard->LoadRAM(0, buffer, 8192);
    //Emulator_LoadRomFile(_T("791404"), buffer, 6144);
    //g_pBoard->LoadRAM(0, buffer, 6144);
    Emulator_LoadRomFile(_T("791323"), buffer, 4096);
    g_pBoard->LoadRAM(0, buffer, 4096);

    g_pBoard->GetCPU()->SetPC(0200);  //DEBUG
    g_pBoard->GetCPU()->SetPSW(0000);  //DEBUG
#endif

    m_nUptimeFrameCount = 0;
    m_dwEmulatorUptime = 0;

    return true;
}

void Emulator_Start()
{
    g_okEmulatorRunning = true;

    // Set title bar text
    MainWindow_UpdateWindowTitle();
    MainWindow_UpdateMenu();

    m_nFrameCount = 0;
    m_dwTickCount = GetTickCount();

    // For proper breakpoint processing
    if (m_wEmulatorCPUBpsCount != 0)
    {
        g_pBoard->GetCPU()->ClearInternalTick();
    }
}
void Emulator_Stop()
{
    g_okEmulatorRunning = false;

    Emulator_SetTempCPUBreakpoint(0177777);

    // Reset title bar message
    MainWindow_UpdateWindowTitle();
    MainWindow_UpdateMenu();

    // Reset FPS indicator
    MainWindow_SetStatusbarText(StatusbarPartFPS, nullptr);

    MainWindow_UpdateAllViews();
}

void Emulator_Reset()
{
    ASSERT(g_pBoard != nullptr);

    g_pBoard->Reset();

    m_nUptimeFrameCount = 0;
    m_dwEmulatorUptime = 0;

    m_EmulatorTapeMode = TAPEMODE_STOPPED;

    MainWindow_UpdateAllViews();
}

bool Emulator_AddCPUBreakpoint(uint16_t address)
{
    if (m_wEmulatorCPUBpsCount == MAX_BREAKPOINTCOUNT - 1 || address == 0177777)
        return false;
    for (int i = 0; i < m_wEmulatorCPUBpsCount; i++)  // Check if the BP exists
    {
        if (m_EmulatorCPUBps[i] == address)
            return false;  // Already in the list
    }
    for (int i = 0; i < MAX_BREAKPOINTCOUNT; i++)  // Put in the first empty cell
    {
        if (m_EmulatorCPUBps[i] > address)  // found the place
        {
            memcpy(m_EmulatorCPUBps + i + 1, m_EmulatorCPUBps + i, sizeof(uint16_t) * (m_wEmulatorCPUBpsCount - i));
            m_EmulatorCPUBps[i] = address;
            break;
        }
        if (m_EmulatorCPUBps[i] == 0177777)  // found empty place
        {
            m_EmulatorCPUBps[i] = address;
            break;
        }
    }
    m_wEmulatorCPUBpsCount++;
    return true;
}
bool Emulator_RemoveCPUBreakpoint(uint16_t address)
{
    if (m_wEmulatorCPUBpsCount == 0 || address == 0177777)
        return false;
    for (int i = 0; i < MAX_BREAKPOINTCOUNT; i++)
    {
        if (m_EmulatorCPUBps[i] == address)
        {
            m_EmulatorCPUBps[i] = 0177777;
            m_wEmulatorCPUBpsCount--;
            if (m_wEmulatorCPUBpsCount > i)  // fill the hole
            {
                memcpy(m_EmulatorCPUBps + i, m_EmulatorCPUBps + i + 1, sizeof(uint16_t) * (m_wEmulatorCPUBpsCount - i));
                m_EmulatorCPUBps[m_wEmulatorCPUBpsCount] = 0177777;
            }
            return true;
        }
    }
    return false;
}
void Emulator_SetTempCPUBreakpoint(uint16_t address)
{
    if (m_wEmulatorTempCPUBreakpoint != 0177777)
        Emulator_RemoveCPUBreakpoint(m_wEmulatorTempCPUBreakpoint);
    if (address == 0177777)
    {
        m_wEmulatorTempCPUBreakpoint = 0177777;
        return;
    }
    for (int i = 0; i < MAX_BREAKPOINTCOUNT; i++)
    {
        if (m_EmulatorCPUBps[i] == address)
            return;  // We have regular breakpoint with the same address
    }
    m_wEmulatorTempCPUBreakpoint = address;
    m_EmulatorCPUBps[m_wEmulatorCPUBpsCount] = address;
    m_wEmulatorCPUBpsCount++;
}
const uint16_t* Emulator_GetCPUBreakpointList() { return m_EmulatorCPUBps; }
bool Emulator_IsBreakpoint()
{
    uint16_t address = g_pBoard->GetCPU()->GetPC();
    if (m_wEmulatorCPUBpsCount > 0)
    {
        for (int i = 0; i < m_wEmulatorCPUBpsCount; i++)
        {
            if (address == m_EmulatorCPUBps[i])
                return true;
        }
    }
    return false;
}
bool Emulator_IsBreakpoint(uint16_t address)
{
    if (m_wEmulatorCPUBpsCount == 0)
        return false;
    for (int i = 0; i < m_wEmulatorCPUBpsCount; i++)
    {
        if (address == m_EmulatorCPUBps[i])
            return true;
    }
    return false;
}
void Emulator_RemoveAllBreakpoints()
{
    for (int i = 0; i < MAX_BREAKPOINTCOUNT; i++)
        m_EmulatorCPUBps[i] = 0177777;
    m_wEmulatorCPUBpsCount = 0;
}

bool Emulator_AddWatch(uint16_t address)
{
    if (m_wEmulatorWatchesCount == MAX_WATCHESCOUNT - 1 || address == 0177777)
        return false;
    for (int i = 0; i < m_wEmulatorWatchesCount; i++)  // Check if the BP exists
    {
        if (m_EmulatorWatches[i] == address)
            return false;  // Already in the list
    }
    for (int i = 0; i < MAX_BREAKPOINTCOUNT; i++)  // Put in the first empty cell
    {
        if (m_EmulatorWatches[i] == 0177777)
        {
            m_EmulatorWatches[i] = address;
            break;
        }
    }
    m_wEmulatorWatchesCount++;
    return true;
}
const uint16_t* Emulator_GetWatchList() { return m_EmulatorWatches; }
bool Emulator_RemoveWatch(uint16_t address)
{
    if (m_wEmulatorWatchesCount == 0 || address == 0177777)
        return false;
    for (int i = 0; i < MAX_WATCHESCOUNT; i++)
    {
        if (m_EmulatorWatches[i] == address)
        {
            m_EmulatorWatches[i] = 0177777;
            m_wEmulatorWatchesCount--;
            if (m_wEmulatorWatchesCount > i)  // fill the hole
            {
                m_EmulatorWatches[i] = m_EmulatorWatches[m_wEmulatorWatchesCount];
                m_EmulatorWatches[m_wEmulatorWatchesCount] = 0177777;
            }
            return true;
        }
    }
    return false;
}
void Emulator_RemoveAllWatches()
{
    for (int i = 0; i < MAX_WATCHESCOUNT; i++)
        m_EmulatorWatches[i] = 0177777;
    m_wEmulatorWatchesCount = 0;
}

void Emulator_SetSpeed(uint16_t realspeed)
{
    uint16_t speedpercent = 100;
    switch (realspeed)
    {
    case 0: speedpercent = 500; break;
    case 1: speedpercent = 100; break;
    case 2: speedpercent = 200; break;
    case 3: speedpercent = 400; break;
    case 0x7fff: speedpercent = 50; break;
    case 0x7ffe: speedpercent = 25; break;
    case 0x7ffd: speedpercent = 10; break;
    default: speedpercent = 100; break;
    }
    m_wEmulatorSoundSpeed = speedpercent;

    if (m_okEmulatorSound)
        SoundGen_SetSpeed(m_wEmulatorSoundSpeed);
}

void Emulator_SetSound(bool soundOnOff)
{
    if (m_okEmulatorSound != soundOnOff)
    {
        if (soundOnOff)
        {
            SoundGen_Initialize(Settings_GetSoundVolume());
            SoundGen_SetSpeed(m_wEmulatorSoundSpeed);
            g_pBoard->SetSoundGenCallback(Emulator_SoundGenCallback);
        }
        else
        {
            g_pBoard->SetSoundGenCallback(nullptr);
            SoundGen_Finalize();
        }
    }

    m_okEmulatorSound = soundOnOff;
}

void Emulator_SetCovox(bool covoxOnOff)
{
    m_okEmulatorCovox = covoxOnOff;
}

void Emulator_SetSoundAY(bool onoff)
{
    m_okEmulatorSoundAY = onoff;
    g_pBoard->SetSoundAY(onoff);
}

bool Emulator_SystemFrame()
{
    SoundGen_SetVolume(Settings_GetSoundVolume());

    g_pBoard->SetCPUBreakpoints(m_wEmulatorCPUBpsCount > 0 ? m_EmulatorCPUBps : nullptr);

    ScreenView_ScanKeyboard();
    ScreenView_ProcessKeyboard();
    Emulator_ProcessJoystick();

    if (!g_pBoard->SystemFrame())
    {
        uint16_t pc = g_pBoard->GetCPU()->GetPC();
        if (pc != m_wEmulatorTempCPUBreakpoint)
            DebugPrintFormat(_T("Breakpoint hit at %06ho\r\n"), pc);

        return false;
    }

    // Calculate frames per second
    m_nFrameCount++;
    uint32_t dwCurrentTicks = GetTickCount();
    long nTicksElapsed = dwCurrentTicks - m_dwTickCount;
    if (nTicksElapsed >= 1200)
    {
        double dFramesPerSecond = m_nFrameCount * 1000.0 / nTicksElapsed;
        double dSpeed = dFramesPerSecond / 25.0 * 100;
        TCHAR buffer[16];
        _sntprintf(buffer, sizeof(buffer) / sizeof(TCHAR) - 1, _T("%03.f%%"), dSpeed);
        MainWindow_SetStatusbarText(StatusbarPartFPS, buffer);

        bool floppyEngine = g_pBoard->IsFloppyEngineOn();
        MainWindow_SetStatusbarText(StatusbarPartFloppyEngine, floppyEngine ? _T("Motor") : nullptr);

        m_nFrameCount = 0;
        m_dwTickCount = dwCurrentTicks;
    }

    // Calculate emulator uptime (25 frames per second)
    m_nUptimeFrameCount++;
    if (m_nUptimeFrameCount >= 25)
    {
        m_dwEmulatorUptime++;
        m_nUptimeFrameCount = 0;

        int seconds = (int) (m_dwEmulatorUptime % 60);
        int minutes = (int) (m_dwEmulatorUptime / 60 % 60);
        int hours   = (int) (m_dwEmulatorUptime / 3600 % 60);

        TCHAR buffer[20];
        _sntprintf(buffer, sizeof(buffer) / sizeof(TCHAR) - 1, _T("Uptime: %02d:%02d:%02d"), hours, minutes, seconds);
        MainWindow_SetStatusbarText(StatusbarPartUptime, buffer);
    }

    // Update "Sound" indicator every 5 frames
    m_nEmulatorSoundChanges += g_pBoard->GetSoundChanges();
    if (m_nUptimeFrameCount % 5 == 0)
    {
        bool soundOn = m_nEmulatorSoundChanges > 0;
        MainWindow_SetStatusbarText(StatusbarPartSound, soundOn ? _T("Sound") : nullptr);
        m_nEmulatorSoundChanges = 0;
    }

    bool okTapeMotor = g_pBoard->IsTapeMotorOn();
    if (Settings_GetTape())
    {
        m_EmulatorTapeMode = okTapeMotor ? TAPEMODE_FINISHED : TAPEMODE_STOPPED;
    }
    else  // Fake tape mode
    {
        switch (m_EmulatorTapeMode)
        {
        case TAPEMODE_STOPPED:
            if (okTapeMotor)
            {
                m_EmulatorTapeMode = TAPEMODE_STARTED;
                m_EmulatorTapeCount = 10;  // wait 2/5 sec
            }
            break;
        case TAPEMODE_STARTED:
            if (!okTapeMotor)
                m_EmulatorTapeMode = TAPEMODE_STOPPED;
            else
            {
                m_EmulatorTapeCount--;
                if (m_EmulatorTapeCount <= 0)
                {
                    uint16_t pc = g_pBoard->GetCPU()->GetPC();
                    // Check if BK-0010 and PC=116722,116724 for tape reading
                    if ((g_nEmulatorConfiguration & 1) == BK_COPT_BK0010 &&
                        (pc == 0116722 || pc == 0116724))
                    {
                        Emulator_FakeTape_ReadFile();
                        m_EmulatorTapeMode = TAPEMODE_FINISHED;
                    }
                    // Check if BK-0011 and PC=155676,155700 for tape reading
                    else if ((g_nEmulatorConfiguration & 1) == BK_COPT_BK0011 &&
                            (pc == 0155676 || pc == 0155700))
                    {
                        Emulator_FakeTape_ReadFile();
                        m_EmulatorTapeMode = TAPEMODE_FINISHED;
                    }
                    // Check for tape save start on BK-0010
                    else if ((g_nEmulatorConfiguration & 1) == BK_COPT_BK0010 &&
                            (pc == 0116414 || pc == 0116426))
                    {
                        Emulator_FakeTape_WriteFile();
                        m_EmulatorTapeMode = TAPEMODE_FINISHED;
                    }
                }
            }
            break;
        case TAPEMODE_FINISHED:
            if (!okTapeMotor)
                m_EmulatorTapeMode = TAPEMODE_STOPPED;
            break;
        }
    }

    return true;
}

void Emulator_GetEmt36FileName(TCHAR* filename)
{
    uint16_t nameaddr = 0326; //g_pBoard->GetRAMWord(0306) + 6;
    for (uint16_t i = 0; i < 16; i++)
    {
        uint8_t ch = g_pBoard->GetRAMByte(nameaddr + i);
        filename[i] = (ch < 32) ? 0 : Translate_BK_Unicode(ch);
    }
    filename[16] = 0;
    // Trim trailing spaces
    for (int i = 15; i >= 0 && filename[i] == _T(' '); i--)
        filename[i] = 0;
    TCHAR* pdot = NULL;
    if (*filename != 0)
    {
        // Check if we have filename extension
        pdot = _tcsrchr(filename, _T('.'));
        if (pdot == NULL)  // Have no dot so append default '.BIN' extension
            _tcsncat(filename, _T(".BIN"), 4);
        else
        {
            // We have dot in string so cut off spaces before the dot
            if (pdot != filename)
            {
                TCHAR* pspace = pdot;
                while (pspace > filename && *(pspace - 1) == _T(' '))
                    pspace--;
                if (pspace < pdot)
                    _tcscpy(pspace, pdot);
            }
        }
    }
}

void Emulator_FakeTape_ReadFile()
{
    TCHAR filename[24];
    Emulator_GetEmt36FileName(filename);

    FILE* fpFile = nullptr;
    // First, if the filename specified, try to find it
    if (*filename != 0)
        fpFile = ::_tfsopen(filename, _T("rb"), _SH_DENYWR);
    // If file not found then ask user for the file
    if (fpFile == nullptr)
    {
        TCHAR title[36];
        _sntprintf(title, 36, _T("Reading tape %s"), filename);
        TCHAR filter[36];
        TCHAR* pdot = _tcsrchr(filename, _T('.'));  // Find the extension
        if (pdot == NULL)
            memcpy(filter, _T("*.BIN\0*.BIN"), 12 * sizeof(TCHAR));
        else
        {
            //TODO: Now we assume extension always 4-char
            filter[0] = _T('*');
            _tcsncpy(filter + 1, pdot, 4);
            filter[5] = 0;
            filter[6] = _T('*');
            _tcsncpy(filter + 7, pdot, 4);
            filter[11] = 0;
        }
        memcpy(filter + 12, _T("*.*\0*.*\0"), 9 * sizeof(TCHAR));
        TCHAR filepath[MAX_PATH];
        if (ShowOpenDialog(g_hwnd, title, filter, filepath))
        {
            fpFile = ::_tfsopen(filepath, _T("rb"), _SH_DENYWR);
        }
    }

    uint8_t result = 2;  // EMT36 result = checksum error
    uint8_t* pData = nullptr;
    if (fpFile != nullptr)
    {
        for (;;)  // For breaks only
        {
            // Read the file header
            uint16_t header[2];
            if (::fread(header, 1, 4, fpFile) != 4)
                break;  // Reading error
            uint16_t filestart = header[0];
            uint16_t filesize = header[1];

            g_pBoard->SetRAMWord(0350, filesize);
            g_pBoard->SetRAMWord(0346, filestart);
            //TODO: Copy 16-char file name from 0326..0345 to 0352..0371

            if (filesize == 0)
                break;  // Wrong Length

            // Read the file
            pData = (uint8_t*)calloc(filesize, 1);
            if (pData == nullptr)
                break;
            if (::fread(pData, 1, filesize, fpFile) != filesize)
                break;  // Reading error

            // Copy to memory
            uint16_t start = g_pBoard->GetRAMWord(0322);
            if (start == 0)
                start = filestart;
            for (uint16_t i = 0; i < filesize; i++)
            {
                g_pBoard->SetRAMByte(start + i, pData[i]);
            }

            result = 0;  // EMT36 result = OK
            break;
        }
        fclose(fpFile);
    }

    if (pData != nullptr)
        free(pData);

    // Report EMT36 result
    g_pBoard->SetRAMByte(0321, result);

    // Execute RTS twice -- return from EMT36
    CProcessor* pCPU = g_pBoard->GetCPU();
    pCPU->SetPC(g_pBoard->GetRAMWord(pCPU->GetSP()));
    pCPU->SetSP(pCPU->GetSP() + 2);
    pCPU->SetPC(g_pBoard->GetRAMWord(pCPU->GetSP()));
    pCPU->SetSP(pCPU->GetSP() + 2);
    //TODO: Set flags
}

void Emulator_FakeTape_WriteFile()
{
    TCHAR filename[24];
    Emulator_GetEmt36FileName(filename);

    FILE* fpFile = nullptr;
    // First, if the filename specified, try to open if
    if (*filename != 0)
        fpFile = ::_tfsopen(filename, _T("wb"), _SH_DENYWR);
    //TODO: If failed, ask user for file name

    uint8_t result = 2;  // EMT36 result = checksum error
    uint8_t* pData = nullptr;
    if (fpFile != nullptr)
    {
        for (;;)  // For breaks only
        {
            uint16_t filesize = g_pBoard->GetRAMWord(0324);
            uint16_t filestart = g_pBoard->GetRAMWord(0322);

            pData = (uint8_t*)calloc(filesize, 1);
            if (pData == nullptr)
                break;

            // Copy from memory
            for (uint16_t i = 0; i < filesize; i++)
            {
                pData[i] = g_pBoard->GetRAMByte(filestart + i);
            }

            uint16_t header[2];
            header[0] = filestart;
            header[1] = filesize;
            if (::fwrite(header, 1, 4, fpFile) != 4)
                break;  // Writing error

            if (::fwrite(pData, 1, filesize, fpFile) != filesize)
                break;  // Writing error

            result = 0;  // EMT36 result = OK
            break;
        }
        fclose(fpFile);
    }

    if (pData != nullptr)
        free(pData);

    // Report EMT36 result
    g_pBoard->SetRAMByte(0321, result);

    // Execute RTS twice -- return from EMT36
    CProcessor* pCPU = g_pBoard->GetCPU();
    pCPU->SetPC(g_pBoard->GetRAMWord(pCPU->GetSP()));
    pCPU->SetSP(pCPU->GetSP() + 2);
    pCPU->SetPC(g_pBoard->GetRAMWord(pCPU->GetSP()));
    pCPU->SetSP(pCPU->GetSP() + 2);
    //TODO: Set flags
}

void Emulator_ProcessJoystick()
{
    if (Settings_GetJoystick() == 0)
        return;  // NumPad joystick processing is inside ScreenView_ScanKeyboard() function

    UINT joystate = Joystick_GetJoystickState();
    g_pBoard->SetPrinterInPort((uint8_t)joystate);
}

void CALLBACK Emulator_SoundGenCallback(unsigned short L, unsigned short R)
{
    if (m_okEmulatorCovox)
    {
        // Get lower byte from printer port output register
        unsigned short data = g_pBoard->GetPrinterOutPort() & 0xff;
        // Merge with channel data
        L += (data << 7);
        R += (data << 7);
    }

    SoundGen_FeedDAC(L, R);
}

// Update cached values after Run or Step
void Emulator_OnUpdate()
{
    // Update stored PC value
    g_wEmulatorPrevCpuPC = g_wEmulatorCpuPC;
    g_wEmulatorCpuPC = g_pBoard->GetCPU()->GetPC();

    // Update memory change flags
    {
        uint8_t* pOld = g_pEmulatorRam;
        uint8_t* pChanged = g_pEmulatorChangedRam;
        uint16_t addr = 0;
        do
        {
            uint8_t newvalue = g_pBoard->GetRAMByte(addr);
            uint8_t oldvalue = *pOld;
            *pChanged = (newvalue != oldvalue) ? 255 : 0;
            *pOld = newvalue;
            addr++;
            pOld++;  pChanged++;
        }
        while (addr < 65535);
    }
}

// Get RAM change flag
//   addrtype - address mode - see ADDRTYPE_XXX constants
uint16_t Emulator_GetChangeRamStatus(uint16_t address)
{
    return *((uint16_t*)(g_pEmulatorChangedRam + address));
}

void CALLBACK Emulator_TeletypeCallback(uint8_t symbol)
{
    if (g_hwndTeletype != (HWND) INVALID_HANDLE_VALUE)
    {
        if (symbol >= 32 || symbol == 13 || symbol == 10)
        {
            TeletypeView_OutputSymbol((TCHAR)symbol);
        }
        else
        {
            TCHAR buffer[32];
            _sntprintf(buffer, sizeof(buffer) / sizeof(TCHAR) - 1, _T("<%02x>"), symbol);
            TeletypeView_Output(buffer);
        }
    }
}

void Emulator_GetScreenSize(int scrmode, int* pwid, int* phei)
{
    if (scrmode < 0 || scrmode >= sizeof(ScreenModeReference) / sizeof(ScreenModeStruct))
        return;
    ScreenModeStruct* pinfo = ScreenModeReference + scrmode;
    *pwid = pinfo->width;
    *phei = pinfo->height;
}

const uint32_t * Emulator_GetPalette(int screenMode)
{
    if ((screenMode & 1) == 0)
        return (const uint32_t *)ScreenView_BWPalette;
    if ((g_nEmulatorConfiguration & BK_COPT_BK0011) == 0)
        return (const uint32_t *)ScreenView_ColorPalette;
    else
        return (const uint32_t *)ScreenView_ColorPalettes[g_pBoard->GetPalette()];
}

void Emulator_PrepareScreenRGB32(void* pImageBits, int screenMode)
{
    if (pImageBits == NULL) return;

    // Get scroll value
    uint16_t scroll = g_pBoard->GetPortView(0177664);
    bool okSmallScreen = ((scroll & 01000) == 0);
    scroll &= 0377;
    scroll = (scroll >= 0330) ? scroll - 0330 : 050 + scroll;

    const uint32_t * pPalette = Emulator_GetPalette(screenMode);

    const uint8_t* pVideoBuffer = g_pBoard->GetVideoBuffer();
    ASSERT(pVideoBuffer != nullptr);

    // Render to bitmap
    PREPARE_SCREEN_CALLBACK callback = ScreenModeReference[screenMode].callback;
    callback(pVideoBuffer, okSmallScreen, pPalette, scroll, pImageBits);
}

#define AVERAGERGB(a, b)  ( (((a) & 0xfefefeffUL) + ((b) & 0xfefefeffUL)) >> 1 )

void CALLBACK Emulator_PrepareScreenBW512x256(const uint8_t* pVideoBuffer, int okSmallScreen, const uint32_t* /*pPalette*/, int scroll, void* pImageBits)
{
    int linesToShow = okSmallScreen ? 64 : 256;
    for (int y = 0; y < linesToShow; y++)
    {
        int yy = (y + scroll) & 0377;
        const uint16_t* pVideo = (uint16_t*)(pVideoBuffer + yy * 0100);
        uint32_t* pBits = (uint32_t*)pImageBits + (255 - y) * 512;
        for (int x = 0; x < 512 / 16; x++)
        {
            uint16_t src = *pVideo;
            for (int bit = 0; bit < 16; bit++)
            {
                uint32_t color = (src & 1) ? 0x0ffffff : 0;
                *pBits = color;
                pBits++;
                src = src >> 1;
            }

            pVideo++;
        }
    }
    if (okSmallScreen)
    {
        memset((uint32_t*)pImageBits, 0, (256 - 64) * 512 * sizeof(uint32_t));
    }
}

void CALLBACK Emulator_PrepareScreenColor512x256(const uint8_t* pVideoBuffer, int okSmallScreen, const uint32_t* pPalette, int scroll, void* pImageBits)
{
    int linesToShow = okSmallScreen ? 64 : 256;
    for (int y = 0; y < linesToShow; y++)
    {
        int yy = (y + scroll) & 0377;
        const uint16_t* pVideo = (uint16_t*)(pVideoBuffer + yy * 0100);
        uint32_t* pBits = (uint32_t*)pImageBits + (255 - y) * 512;
        for (int x = 0; x < 512 / 16; x++)
        {
            uint16_t src = *pVideo;
            for (int bit = 0; bit < 16; bit += 2)
            {
                uint32_t color = pPalette[src & 3];
                *pBits = color;
                pBits++;
                *pBits = color;
                pBits++;
                src = src >> 2;
            }

            pVideo++;
        }
    }
    if (okSmallScreen)
    {
        memset((uint32_t*)pImageBits, 0, (256 - 64) * 512 * sizeof(uint32_t));
    }
}

void CALLBACK Emulator_PrepareScreenBW512x384(const uint8_t* pVideoBuffer, int okSmallScreen, const uint32_t* /*pPalette*/, int scroll, void* pImageBits)
{
    int linesToShow = okSmallScreen ? 64 : 256;
    int bky = 0;
    for (int y = 0; y < 384; y++)
    {
        uint32_t* pBits = (uint32_t*)pImageBits + (383 - y) * 512;
        if (y % 3 == 1)
            continue;  // Skip, fill later

        int yy = (bky + scroll) & 0377;
        const uint16_t* pVideo = (uint16_t*)(pVideoBuffer + yy * 0100);
        for (int x = 0; x < 512 / 16; x++)
        {
            uint16_t src = *pVideo;
            for (int bit = 0; bit < 16; bit++)
            {
                uint32_t color = (src & 1) ? 0x0ffffff : 0;
                *pBits = color;
                pBits++;
                src = src >> 1;
            }

            pVideo++;
        }

        if (y % 3 == 2)  // Fill skipped line
        {
            uint8_t* pBits2 = (uint8_t*)((uint32_t*)pImageBits + (383 - y + 0) * 512);
            uint8_t* pBits1 = (uint8_t*)((uint32_t*)pImageBits + (383 - y + 1) * 512);
            uint8_t* pBits0 = (uint8_t*)((uint32_t*)pImageBits + (383 - y + 2) * 512);
            for (int x = 0; x < 512 * 4; x++)
            {
                *pBits1 = (uint8_t)((((uint16_t) * pBits0) + ((uint16_t) * pBits2)) / 2);
                pBits2++;  pBits1++;  pBits0++;
            }
        }

        bky++;
        if (bky >= linesToShow) break;
    }
    if (okSmallScreen)
    {
        memset((uint32_t*)pImageBits, 0, (384 - 86) * 512 * sizeof(uint32_t));  //TODO
    }
}

void CALLBACK Emulator_PrepareScreenColor512x384(const uint8_t* pVideoBuffer, int okSmallScreen, const uint32_t* pPalette, int scroll, void* pImageBits)
{
    int linesToShow = okSmallScreen ? 64 : 256;
    int bky = 0;
    for (int y = 0; y < 384; y++)
    {
        uint32_t* pBits = (uint32_t*)pImageBits + (383 - y) * 512;
        if (y % 3 == 1)
            continue;  // Skip, fill later

        int yy = (bky + scroll) & 0377;
        const uint16_t* pVideo = (uint16_t*)(pVideoBuffer + yy * 0100);
        for (int x = 0; x < 512 / 16; x++)
        {
            uint16_t src = *pVideo;
            for (int bit = 0; bit < 16; bit += 2)
            {
                uint32_t color = pPalette[src & 3];
                *pBits = color;
                pBits++;
                *pBits = color;
                pBits++;
                src = src >> 2;
            }

            pVideo++;
        }

        if (y % 3 == 2)  // Fill skipped line
        {
            uint8_t* pBits2 = (uint8_t*)((uint32_t*)pImageBits + (383 - y + 0) * 512);
            uint8_t* pBits1 = (uint8_t*)((uint32_t*)pImageBits + (383 - y + 1) * 512);
            uint8_t* pBits0 = (uint8_t*)((uint32_t*)pImageBits + (383 - y + 2) * 512);
            for (int x = 0; x < 512 * 4; x++)
            {
                *pBits1 = (uint8_t)((((uint16_t) * pBits0) + ((uint16_t) * pBits2)) / 2);
                pBits2++;  pBits1++;  pBits0++;
            }
        }

        bky++;
        if (bky >= linesToShow) break;
    }
    if (okSmallScreen)
    {
        memset((uint32_t*)pImageBits, 0, (384 - 86) * 512 * sizeof(uint32_t));  //TODO
    }
}

void CALLBACK Emulator_PrepareScreenBW896x512(const uint8_t* pVideoBuffer, int okSmallScreen, const uint32_t* /*pPalette*/, int scroll, void* pImageBits)
{
    int linesToShow = okSmallScreen ? 64 : 256;
    for (int y = 0; y < linesToShow; y++)
    {
        int yy = (y + scroll) & 0377;
        const uint16_t* pVideo = (uint16_t*)(pVideoBuffer + yy * 0100);
        uint32_t* pBits = (uint32_t*)pImageBits + (255 - y) * 896 * 2;
        uint32_t* pBits2 = pBits + 896;
        for (int x = 0; x < 512 / 16; x++)
        {
            uint16_t src = *pVideo;
            for (int bit = 0; bit < 16; bit += 4)
            {
                uint32_t c1 = (src & 1) ? 0x0ffffff : 0;
                uint32_t c2 = (src & 2) ? 0x0ffffff : 0;
                uint32_t c3 = (src & 4) ? 0x0ffffff : 0;
                uint32_t c4 = (src & 8) ? 0x0ffffff : 0;
                *(pBits++) = *(pBits2++) = c1;
                *(pBits++) = *(pBits2++) = AVERAGERGB(c1, c2);
                *(pBits++) = *(pBits2++) = c2;
                *(pBits++) = *(pBits2++) = AVERAGERGB(c2, c3);
                *(pBits++) = *(pBits2++) = c3;
                *(pBits++) = *(pBits2++) = AVERAGERGB(c3, c4);
                *(pBits++) = *(pBits2++) = c4;
                src = src >> 4;
            }

            pVideo++;
        }
    }
    if (okSmallScreen)
    {
        memset((uint32_t*)pImageBits, 0, (256 - 64) * 896 * 2 * sizeof(uint32_t));
    }
}

void CALLBACK Emulator_PrepareScreenColor896x512(const uint8_t* pVideoBuffer, int okSmallScreen, const uint32_t* pPalette, int scroll, void* pImageBits)
{
    int linesToShow = okSmallScreen ? 64 : 256;
    for (int y = 0; y < linesToShow; y++)
    {
        int yy = (y + scroll) & 0377;
        const uint16_t* pVideo = (uint16_t*)(pVideoBuffer + yy * 0100);
        uint32_t* pBits = (uint32_t*)pImageBits + (255 - y) * 896 * 2;
        uint32_t* pBits2 = pBits + 896;
        for (int x = 0; x < 512 / 16; x++)
        {
            uint16_t src = *pVideo;
            for (int bit = 0; bit < 16; bit += 8)
            {
                uint32_t c1 = pPalette[src & 3];  src = src >> 2;
                uint32_t c2 = pPalette[src & 3];  src = src >> 2;
                uint32_t c3 = pPalette[src & 3];  src = src >> 2;
                uint32_t c4 = pPalette[src & 3];  src = src >> 2;
                uint32_t c12 = AVERAGERGB(c1, c2);
                uint32_t c23 = AVERAGERGB(c2, c3);
                uint32_t c34 = AVERAGERGB(c3, c4);
                *(pBits++) = *(pBits2++) = c1;
                *(pBits++) = *(pBits2++) = c1;
                *(pBits++) = *(pBits2++) = c12;
                *(pBits++) = *(pBits2++) = c12;
                *(pBits++) = *(pBits2++) = c2;
                *(pBits++) = *(pBits2++) = c2;
                *(pBits++) = *(pBits2++) = c23;
                *(pBits++) = *(pBits2++) = c23;
                *(pBits++) = *(pBits2++) = c3;
                *(pBits++) = *(pBits2++) = c3;
                *(pBits++) = *(pBits2++) = c34;
                *(pBits++) = *(pBits2++) = c34;
                *(pBits++) = *(pBits2++) = c4;
                *(pBits++) = *(pBits2++) = c4;
            }

            pVideo++;
        }
    }
    if (okSmallScreen)
    {
        memset((uint32_t*)pImageBits, 0, (256 - 64) * 896 * 2 * sizeof(uint32_t));
    }
}

void CALLBACK Emulator_PrepareScreenBW1024x768(const uint8_t* pVideoBuffer, int okSmallScreen, const uint32_t* /*pPalette*/, int scroll, void* pImageBits)
{
    int linesToShow = okSmallScreen ? 64 : 256;
    for (int y = 0; y < linesToShow; y++)
    {
        int yy = (y + scroll) & 0377;
        const uint16_t* pVideo = (uint16_t*)(pVideoBuffer + yy * 0100);
        uint32_t* pBits = (uint32_t*)pImageBits + (255 - y) * 1024 * 3;
        uint32_t* pBits2 = pBits + 1024;
        for (int x = 0; x < 512 / 16; x++)
        {
            uint16_t src = *pVideo;
            for (int bit = 0; bit < 16; bit++)
            {
                uint32_t color = (src & 1) ? 0x0ffffff : 0;
                *pBits = *pBits2 = color;
                pBits++;  pBits2++;
                *pBits = *pBits2 = color;
                pBits++;  pBits2++;
                src = src >> 1;
            }

            pVideo++;
        }
    }
    if (okSmallScreen)
    {
        memset((uint32_t*)pImageBits, 0, (256 - 64) * 1024 * 3 * sizeof(uint32_t));
    }
}

void CALLBACK Emulator_PrepareScreenColor1024x768(const uint8_t* pVideoBuffer, int okSmallScreen, const uint32_t* pPalette, int scroll, void* pImageBits)
{
    int linesToShow = okSmallScreen ? 64 : 256;
    for (int y = 0; y < linesToShow; y++)
    {
        int yy = (y + scroll) & 0377;
        const uint16_t* pVideo = (uint16_t*)(pVideoBuffer + yy * 0100);
        uint32_t* pBits = (uint32_t*)pImageBits + (255 - y) * 1024 * 3;
        uint32_t* pBits2 = pBits + 1024;
        for (int x = 0; x < 512 / 16; x++)
        {
            uint16_t src = *pVideo;
            for (int bit = 0; bit < 16; bit += 2)
            {
                uint32_t color = pPalette[src & 3];
                *pBits = *pBits2 = color;
                pBits++;  pBits2++;
                *pBits = *pBits2 = color;
                pBits++;  pBits2++;
                *pBits = *pBits2 = color;
                pBits++;  pBits2++;
                *pBits = *pBits2 = color;
                pBits++;  pBits2++;
                src = src >> 2;
            }

            pVideo++;
        }
    }
    if (okSmallScreen)
    {
        memset((uint32_t*)pImageBits, 0, (256 - 64) * 1024 * 3 * sizeof(uint32_t));
    }
}

void CALLBACK Emulator_PrepareScreenBW1536x1024(const uint8_t* pVideoBuffer, int okSmallScreen, const uint32_t* /*pPalette*/, int scroll, void* pImageBits)
{
    int linesToShow = okSmallScreen ? 64 : 256;
    for (int y = 0; y < linesToShow; y++)
    {
        int yy = (y + scroll) & 0377;
        const uint16_t* pVideo = (uint16_t*)(pVideoBuffer + yy * 0100);
        uint32_t* pBits = (uint32_t*)pImageBits + (255 - y) * 1536 * 4;
        uint32_t* pBits1 = pBits + 1536;
        uint32_t* pBits2 = pBits + 1536;
        for (int x = 0; x < 512 / 16; x++)
        {
            uint16_t src = *pVideo;
            for (int bit = 0; bit < 16; bit++)
            {
                uint32_t color = (src & 1) ? 0x0ffffff : 0;
                *pBits++ = *pBits1++ = *pBits2++ = color;
                *pBits++ = *pBits1++ = *pBits2++ = color;
                *pBits++ = *pBits1++ = *pBits2++ = color;
                src = src >> 1;
            }
            pVideo++;
        }
    }
    if (okSmallScreen)
    {
        memset((uint32_t*)pImageBits, 0, (256 - 64) * 1536 * 4 * sizeof(uint32_t));
    }
}

void CALLBACK Emulator_PrepareScreenColor1536x1024(const uint8_t* pVideoBuffer, int okSmallScreen, const uint32_t* pPalette, int scroll, void* pImageBits)
{
    int linesToShow = okSmallScreen ? 64 : 256;
    for (int y = 0; y < linesToShow; y++)
    {
        int yy = (y + scroll) & 0377;
        const uint16_t* pVideo = (uint16_t*)(pVideoBuffer + yy * 0100);
        uint32_t* pBits = (uint32_t*)pImageBits + (255 - y) * 1536 * 4;
        uint32_t* pBits1 = pBits + 1536;
        uint32_t* pBits2 = pBits + 1536;
        for (int x = 0; x < 512 / 16; x++)
        {
            uint16_t src = *pVideo;
            for (int bit = 0; bit < 16; bit += 2)
            {
                uint32_t color = pPalette[src & 3];
                *pBits++ = *pBits1++ = *pBits2++ = color;
                *pBits++ = *pBits1++ = *pBits2++ = color;
                *pBits++ = *pBits1++ = *pBits2++ = color;
                *pBits++ = *pBits1++ = *pBits2++ = color;
                *pBits++ = *pBits1++ = *pBits2++ = color;
                *pBits++ = *pBits1++ = *pBits2++ = color;
                src = src >> 2;
            }
            pVideo++;
        }
    }
    if (okSmallScreen)
    {
        memset((uint32_t*)pImageBits, 0, (256 - 64) * 1536 * 4 * sizeof(uint32_t));
    }
}


//////////////////////////////////////////////////////////////////////
//
// Emulator image format - see CMotherboard::SaveToImage()
// Image header format (32 bytes):
//   4 bytes        BK_IMAGE_HEADER1
//   4 bytes        BK_IMAGE_HEADER2
//   4 bytes        BK_IMAGE_VERSION
//   4 bytes        BK_IMAGE_SIZE
//   4 bytes        BK uptime
//   12 bytes       Not used
//TODO: 256 bytes * 4 - Floppy 1..4 path

bool Emulator_SaveImage(LPCTSTR sFilePath)
{
    // Create file
    HANDLE hFile = CreateFile(sFilePath,
            GENERIC_WRITE, FILE_SHARE_READ, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    // Allocate memory
    uint8_t* pImage = static_cast<uint8_t*>(::calloc(BKIMAGE_SIZE, 1));
    if (pImage == nullptr)
    {
        ::CloseHandle(hFile);
        return false;
    }
    ::memset(pImage, 0, BKIMAGE_SIZE);
    // Prepare header
    uint32_t* pHeader = (uint32_t*) pImage;
    *pHeader++ = BKIMAGE_HEADER1;
    *pHeader++ = BKIMAGE_HEADER2;
    *pHeader++ = BKIMAGE_VERSION;
    *pHeader++ = BKIMAGE_SIZE;
    // Store emulator state to the image
    g_pBoard->SaveToImage(pImage);
    *(uint32_t*)(pImage + 16) = m_dwEmulatorUptime;

    // Save image to the file
    DWORD dwBytesWritten = 0;
    WriteFile(hFile, pImage, BKIMAGE_SIZE, &dwBytesWritten, NULL);
    ::free(pImage);
    CloseHandle(hFile);
    if (dwBytesWritten != BKIMAGE_SIZE)
        return false;

    return true;
}

bool Emulator_LoadImage(LPCTSTR sFilePath)
{
    Emulator_Stop();

    // Open file
    HANDLE hFile = CreateFile(sFilePath,
            GENERIC_READ, FILE_SHARE_READ, NULL,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    // Read header
    uint32_t bufHeader[BKIMAGE_HEADER_SIZE / sizeof(uint32_t)];
    DWORD dwBytesRead = 0;
    ReadFile(hFile, bufHeader, BKIMAGE_HEADER_SIZE, &dwBytesRead, NULL);
    if (dwBytesRead != BKIMAGE_HEADER_SIZE)
    {
        CloseHandle(hFile);
        return false;
    }

    //TODO: Check version and size

    // Allocate memory
    uint8_t* pImage = static_cast<uint8_t*>(::calloc(BKIMAGE_SIZE, 1));
    if (pImage == nullptr)
    {
        CloseHandle(hFile);
        return false;
    }

    // Read image
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    dwBytesRead = 0;
    ReadFile(hFile, pImage, BKIMAGE_SIZE, &dwBytesRead, NULL);
    if (dwBytesRead != BKIMAGE_SIZE)
    {
        CloseHandle(hFile);
        return false;
    }

    // Restore emulator state from the image
    g_pBoard->LoadFromImage(pImage);

    m_dwEmulatorUptime = *(uint32_t*)(pImage + 16);
    g_wEmulatorCpuPC = g_pBoard->GetCPU()->GetPC();

    // Free memory, close file
    ::free(pImage);
    CloseHandle(hFile);

    g_okEmulatorRunning = false;

    MainWindow_UpdateAllViews();

    return true;
}


//////////////////////////////////////////////////////////////////////
