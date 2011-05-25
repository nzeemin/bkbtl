/*  This file is part of BKBTL.
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
#include <stdio.h>
#include <Share.h>
#include "BKBTL.h"
#include "Emulator.h"
#include "Views.h"
#include "emubase\Emubase.h"
#include "SoundGen.h"
#include "Joystick.h"


//////////////////////////////////////////////////////////////////////


CMotherboard* g_pBoard = NULL;
BKConfiguration g_nEmulatorConfiguration;  // Current configuration
BOOL g_okEmulatorRunning = FALSE;

WORD m_wEmulatorCPUBreakpoint = 0177777;

BOOL m_okEmulatorSound = FALSE;
BOOL m_okEmulatorCovox = FALSE;

long m_nFrameCount = 0;
DWORD m_dwTickCount = 0;
DWORD m_dwEmulatorUptime = 0;  // BK uptime, seconds, from turn on or reset, increments every 25 frames
long m_nUptimeFrameCount = 0;

BYTE* g_pEmulatorRam;  // RAM values - for change tracking
BYTE* g_pEmulatorChangedRam;  // RAM change flags
WORD g_wEmulatorCpuPC = 0177777;      // Current PC value
WORD g_wEmulatorPrevCpuPC = 0177777;  // Previous PC value


void CALLBACK Emulator_SoundGenCallback(unsigned short L, unsigned short R);
void CALLBACK Emulator_TeletypeCallback(BYTE symbol);


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

const DWORD ScreenView_ColorPalette[4] = {
    0x000000, 0x0000FF, 0x00FF00, 0xFF0000
};

const DWORD ScreenView_ColorPalettes[16][4] = {
    //                                     Palette#     01           10          11
    0x000000, 0x0000FF, 0x00FF00, 0xFF0000,  // 00    синий   |   зеленый  |  красный
    0x000000, 0xFFFF00, 0xFF00FF, 0xFF0000,  // 01   желтый   |  сиреневый |  красный
    0x000000, 0x00FFFF, 0x0000FF, 0xFF00FF,  // 02   голубой  |    синий   | сиреневый
    0x000000, 0x00FF00, 0x00FFFF, 0xFFFF00,  // 03   зеленый  |   голубой  |  желтый
    0x000000, 0xFF00FF, 0x00FFFF, 0xFFFFFF,  // 04  сиреневый |   голубой  |   белый
    0x000000, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF,  // 05    белый   |    белый   |   белый
    0x000000, 0x7F0000, 0x7F0000, 0xFF0000,  // 06  темн-красн| красн-корич|  красный
    0x000000, 0x00FF7F, 0x00FF7F, 0xFFFF00,  // 07  салатовый | светл-зелен|  желтый
    0x000000, 0xFF00FF, 0x7F00FF, 0x7F007F,  // 08  фиолетовый| фиол-синий | сиреневый
    0x000000, 0x00FF7F, 0x7F00FF, 0x7F0000,  // 09 светл-зелен| фиол-синий |красн-корич
    0x000000, 0x00FF7F, 0x7F007F, 0x7F0000,  // 10  салатовый | фиолетовый |темн-красный
    0x000000, 0x00FFFF, 0xFFFF00, 0xFF0000,  // 11   голубой  |   желтый   |  красный
    0x000000, 0xFF0000, 0x00FF00, 0x00FFFF,  // 12   красный  |   зеленый  |  голубой
    0x000000, 0x00FFFF, 0xFFFF00, 0xFFFFFF,  // 13   голубой  |   желтый   |   белый
    0x000000, 0xFFFF00, 0x00FF00, 0xFFFFFF,  // 14   желтый   |   зеленый  |   белый 
    0x000000, 0x00FFFF, 0x00FF00, 0xFFFFFF,  // 15   голубой  |   зеленый  |   белый
};


//////////////////////////////////////////////////////////////////////

BOOL Emulator_LoadRomFile(LPCTSTR strFileName, BYTE* buffer, DWORD fileOffset, DWORD bytesToRead)
{
    FILE* fpRomFile = ::_tfsopen(strFileName, _T("rb"), _SH_DENYWR);
    if (fpRomFile == NULL)
        return FALSE;

    ASSERT(bytesToRead <= 8192);
    ::memset(buffer, 0, 8192);

    if (fileOffset > 0)
    {
        ::fseek(fpRomFile, fileOffset, SEEK_SET);
    }

    DWORD dwBytesRead = ::fread(buffer, 1, bytesToRead, fpRomFile);
    if (dwBytesRead != bytesToRead)
    {
        ::fclose(fpRomFile);
        return FALSE;
    }

    ::fclose(fpRomFile);

    return TRUE;
}

BOOL Emulator_Init()
{
    ASSERT(g_pBoard == NULL);

    CProcessor::Init();

    g_pBoard = new CMotherboard();

    // Allocate memory for old RAM values
    g_pEmulatorRam = (BYTE*) ::malloc(65536);  ::memset(g_pEmulatorRam, 0, 65536);
    g_pEmulatorChangedRam = (BYTE*) ::malloc(65536);  ::memset(g_pEmulatorChangedRam, 0, 65536);

    g_pBoard->Reset();

    if (m_okEmulatorSound)
    {
	    SoundGen_Initialize();
        g_pBoard->SetSoundGenCallback(Emulator_SoundGenCallback);
    }

    g_pBoard->SetTeletypeCallback(Emulator_TeletypeCallback);

    return TRUE;
}

void Emulator_Done()
{
    ASSERT(g_pBoard != NULL);

    CProcessor::Done();

    g_pBoard->SetSoundGenCallback(NULL);
    SoundGen_Finalize();

    delete g_pBoard;
    g_pBoard = NULL;

    // Free memory used for old RAM values
    ::free(g_pEmulatorRam);
    ::free(g_pEmulatorChangedRam);
}

BOOL Emulator_InitConfiguration(BKConfiguration configuration)
{
    g_pBoard->SetConfiguration(configuration);

    BYTE buffer[8192];

    if ((configuration & BK_COPT_BK0011) == 0)
    {
        // Load Monitor ROM file
        if (!Emulator_LoadRomFile(FILENAME_BKROM_MONIT10, buffer, 0, 8192))
        {
            AlertWarning(_T("Failed to load Monitor ROM file."));
            return FALSE;
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
            return FALSE;
        }
        g_pBoard->LoadROM(1, buffer);
        // Load BASIC ROM 2 file
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BASIC10_2, buffer, 0, 8192))
        {
            AlertWarning(_T("Failed to load BASIC ROM 2 file."));
            return FALSE;
        }
        g_pBoard->LoadROM(2, buffer);
    }
    if ((configuration & BK_COPT_BK0011) == 0 && (configuration & BK_COPT_ROM_BASIC) != 0)  // BK 0010 BASIC ROM 3
    {
        // Load BASIC ROM 3 file
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BASIC10_3, buffer, 0, 8064))
        {
            AlertWarning(_T("Failed to load BASIC ROM 3 file."));
            return FALSE;
        }
        g_pBoard->LoadROM(3, buffer);
    }
    if ((configuration & BK_COPT_BK0011) == 0 && (configuration & BK_COPT_ROM_FOCAL) != 0)  // BK 0010 FOCAL
    {
        // Load Focal ROM file
        if (!Emulator_LoadRomFile(FILENAME_BKROM_FOCAL, buffer, 0, 8192))
        {
            AlertWarning(_T("Failed to load Focal ROM file."));
            return FALSE;
        }
        g_pBoard->LoadROM(1, buffer);
        // Unused 8KB
        ::memset(buffer, 0, 8192);
        g_pBoard->LoadROM(2, buffer);
        // Load Tests ROM file
        if (!Emulator_LoadRomFile(FILENAME_BKROM_TESTS, buffer, 0, 8064))
        {
            AlertWarning(_T("Failed to load Tests ROM file."));
            return FALSE;
        }
        g_pBoard->LoadROM(3, buffer);
    }

    if (configuration & BK_COPT_BK0011)
    {
        // Load BK0011M BASIC 0, part 1
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BASIC11M_0, buffer, 0, 8192))
        {
            AlertWarning(_T("Failed to load BK11M BASIC 0 ROM file."));
            return FALSE;
        }
        g_pBoard->LoadROM(0, buffer);
        // Load BK0011M BASIC 0, part 2
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BASIC11M_0, buffer, 8192, 8192))
        {
            AlertWarning(_T("Failed to load BK11M BASIC 0 ROM file."));
            return FALSE;
        }
        g_pBoard->LoadROM(1, buffer);
        // Load BK0011M BASIC 1
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BASIC11M_1, buffer, 0, 8192))
        {
            AlertWarning(_T("Failed to load BK11M BASIC 1 ROM file."));
            return FALSE;
        }
        g_pBoard->LoadROM(2, buffer);

        // Load BK0011M EXT
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BK11M_EXT, buffer, 0, 8192))
        {
            AlertWarning(_T("Failed to load BK11M EXT ROM file."));
            return FALSE;
        }
        g_pBoard->LoadROM(3, buffer);
        // Load BK0011M BOS
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BK11M_BOS, buffer, 0, 8192))
        {
            AlertWarning(_T("Failed to load BK11M BOS ROM file."));
            return FALSE;
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
            return FALSE;
        }
        g_pBoard->LoadROM((configuration & BK_COPT_BK0011) ? 5 : 3, buffer);
    }

    if ((configuration & BK_COPT_BK0011) && (configuration & BK_COPT_FDD) == 0)
    {
        // Load BK0011M MSTD
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BK11M_MSTD, buffer, 0, 8192))
        {
            AlertWarning(_T("Failed to load BK11M MSTD ROM file."));
            return FALSE;
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

    return TRUE;
}

void Emulator_Start()
{
    g_okEmulatorRunning = TRUE;

    // Set title bar text
    SetWindowText(g_hwnd, _T("BK Back to Life [run]"));
    MainWindow_UpdateMenu();

    m_nFrameCount = 0;
    m_dwTickCount = GetTickCount();
}
void Emulator_Stop()
{
    g_okEmulatorRunning = FALSE;
    m_wEmulatorCPUBreakpoint = 0177777;

    // Reset title bar message
    SetWindowText(g_hwnd, _T("BK Back to Life [stop]"));
    MainWindow_UpdateMenu();
    // Reset FPS indicator
    MainWindow_SetStatusbarText(StatusbarPartFPS, _T(""));

    MainWindow_UpdateAllViews();
}

void Emulator_Reset()
{
    ASSERT(g_pBoard != NULL);

    g_pBoard->Reset();

    m_nUptimeFrameCount = 0;
    m_dwEmulatorUptime = 0;

    MainWindow_UpdateAllViews();
}

void Emulator_SetCPUBreakpoint(WORD address)
{
    m_wEmulatorCPUBreakpoint = address;
}

BOOL Emulator_IsBreakpoint()
{
    WORD wCPUAddr = g_pBoard->GetCPU()->GetPC();
    if (wCPUAddr == m_wEmulatorCPUBreakpoint)
        return TRUE;
    return FALSE;
}

void Emulator_SetSound(BOOL soundOnOff)
{
    if (m_okEmulatorSound != soundOnOff)
    {
        if (soundOnOff)
        {
            SoundGen_Initialize();
            g_pBoard->SetSoundGenCallback(Emulator_SoundGenCallback);
        }
        else
        {
            g_pBoard->SetSoundGenCallback(NULL);
            SoundGen_Finalize();
        }
    }

    m_okEmulatorSound = soundOnOff;
}

void Emulator_SetCovox(BOOL covoxOnOff)
{
    m_okEmulatorCovox = covoxOnOff;
}

int Emulator_SystemFrame()
{
    g_pBoard->SetCPUBreakpoint(m_wEmulatorCPUBreakpoint);

    ScreenView_ScanKeyboard();
    ScreenView_ProcessKeyboard();
    Emulator_ProcessJoystick();
    
	if (!g_pBoard->SystemFrame())
        return 0;

	// Calculate frames per second
	m_nFrameCount++;
	DWORD dwCurrentTicks = GetTickCount();
	long nTicksElapsed = dwCurrentTicks - m_dwTickCount;
	if (nTicksElapsed >= 1200)
	{
		double dFramesPerSecond = m_nFrameCount * 1000.0 / nTicksElapsed;
		TCHAR buffer[16];
		swprintf_s(buffer, 16, _T("FPS: %05.2f"), dFramesPerSecond);
		MainWindow_SetStatusbarText(StatusbarPartFPS, buffer);

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
		swprintf_s(buffer, 20, _T("Uptime: %02d:%02d:%02d"), hours, minutes, seconds);
		MainWindow_SetStatusbarText(StatusbarPartUptime, buffer);
	}

    return 1;
}

void Emulator_ProcessJoystick()
{
    if (Settings_GetJoystick() == 0)
        return;  // NumPad joystick processing is inside ScreenView_ScanKeyboard() function

    UINT joystate = Joystick_GetJoystickState();
    g_pBoard->SetPrinterInPort(joystate);
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
        BYTE* pOld = g_pEmulatorRam;
        BYTE* pChanged = g_pEmulatorChangedRam;
        WORD addr = 0;
        do
        {
            BYTE newvalue = g_pBoard->GetRAMByte(addr);
            BYTE oldvalue = *pOld;
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
WORD Emulator_GetChangeRamStatus(WORD address)
{
    return *((WORD*)(g_pEmulatorChangedRam + address));
}

void CALLBACK Emulator_TeletypeCallback(BYTE symbol)
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
            _snwprintf_s(buffer, 32, _T("<%02x>"), symbol);
            TeletypeView_Output(buffer);
        }
    }
}

void Emulator_PrepareScreenRGB32(void* pImageBits, ScreenViewMode screenMode)
{
    if (pImageBits == NULL) return;
    //if (!g_okEmulatorInitialized) return;

    // Get scroll value
    WORD scroll = g_pBoard->GetPortView(0177664);
    BOOL okSmallScreen = ((scroll & 01000) == 0);
    scroll &= 0377;
    scroll = (scroll >= 0330) ? scroll - 0330 : 050 + scroll;

    // Get palette
    DWORD* pPalette;
    if ((g_nEmulatorConfiguration & BK_COPT_BK0011) == 0)
        pPalette = (DWORD*)ScreenView_ColorPalette;
    else
        pPalette = (DWORD*)ScreenView_ColorPalettes[g_pBoard->GetPalette()];

    const BYTE* pVideoBuffer = g_pBoard->GetVideoBuffer();
    ASSERT(pVideoBuffer != NULL);

    // Render to bitmap
    int linesToShow = okSmallScreen ? 64 : 256;
    for (int y = 0; y < linesToShow; y++)
    {
        int yy = (y + scroll) & 0377;
        const WORD* pVideo = (WORD*)(pVideoBuffer + yy * 0100);
        DWORD* pBits = ((DWORD*)pImageBits) + (256 - 1 - y) * 512;
        for (int x = 0; x < 512 / 16; x++)
        {
            WORD src = *pVideo;

            if (screenMode == BlackWhiteScreen)  // Black and white mode 512 x 256
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
                    DWORD color = pPalette[src & 3];
                    *pBits = color;
                    pBits++;
                    *pBits = color;
                    pBits++;
                    src = src >> 2;
                }
            }

            pVideo++;
        }
    }

    if (okSmallScreen)
    {
        ::ZeroMemory(pImageBits, (256 - 64) * 512 * sizeof(DWORD));
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

void Emulator_SaveImage(LPCTSTR sFilePath)
{
    // Create file
    HANDLE hFile = CreateFile(sFilePath,
            GENERIC_WRITE, FILE_SHARE_READ, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        AlertWarning(_T("Failed to save image file."));
        return;
    }

    // Allocate memory
    BYTE* pImage = (BYTE*) ::malloc(BKIMAGE_SIZE);  memset(pImage, 0, BKIMAGE_SIZE);
    ::memset(pImage, 0, BKIMAGE_SIZE);
    // Prepare header
    DWORD* pHeader = (DWORD*) pImage;
    *pHeader++ = BKIMAGE_HEADER1;
    *pHeader++ = BKIMAGE_HEADER2;
    *pHeader++ = BKIMAGE_VERSION;
    *pHeader++ = BKIMAGE_SIZE;
    // Store emulator state to the image
    //g_pBoard->SaveToImage(pImage);
    *(DWORD*)(pImage + 16) = m_dwEmulatorUptime;

    // Save image to the file
    DWORD dwBytesWritten = 0;
    WriteFile(hFile, pImage, BKIMAGE_SIZE, &dwBytesWritten, NULL);
    //TODO: Check if dwBytesWritten != BKIMAGE_SIZE

    // Free memory, close file
    ::free(pImage);
    CloseHandle(hFile);
}

void Emulator_LoadImage(LPCTSTR sFilePath)
{
    // Open file
    HANDLE hFile = CreateFile(sFilePath,
            GENERIC_READ, FILE_SHARE_READ, NULL,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        AlertWarning(_T("Failed to load image file."));
        return;
    }

    // Read header
    DWORD bufHeader[BKIMAGE_HEADER_SIZE / sizeof(DWORD)];
    DWORD dwBytesRead = 0;
    ReadFile(hFile, bufHeader, BKIMAGE_HEADER_SIZE, &dwBytesRead, NULL);
    //TODO: Check if dwBytesRead != BKIMAGE_HEADER_SIZE
    
    //TODO: Check version and size

    // Allocate memory
    BYTE* pImage = (BYTE*) ::malloc(BKIMAGE_SIZE);  ::memset(pImage, 0, BKIMAGE_SIZE);

    // Read image
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    dwBytesRead = 0;
    ReadFile(hFile, pImage, BKIMAGE_SIZE, &dwBytesRead, NULL);
    //TODO: Check if dwBytesRead != BKIMAGE_SIZE

    // Restore emulator state from the image
    //g_pBoard->LoadFromImage(pImage);

    m_dwEmulatorUptime = *(DWORD*)(pImage + 16);

    // Free memory, close file
    ::free(pImage);
    CloseHandle(hFile);

    g_okEmulatorRunning = FALSE;

    MainWindow_UpdateAllViews();
}


//////////////////////////////////////////////////////////////////////
