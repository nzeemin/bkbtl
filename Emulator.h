/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// Emulator.h

#pragma once

#include "emubase\Board.h"
#include "Views.h"


//////////////////////////////////////////////////////////////////////


extern CMotherboard* g_pBoard;
extern BKConfiguration g_nEmulatorConfiguration;  // Current configuration
extern bool g_okEmulatorRunning;

extern uint8_t* g_pEmulatorRam;  // RAM values - for change tracking
extern uint8_t* g_pEmulatorChangedRam;  // RAM change flags
extern uint16_t g_wEmulatorCpuPC;      // Current PC value
extern uint16_t g_wEmulatorPrevCpuPC;  // Previous PC value
extern uint16_t g_wEmulatorPpuPC;      // Current PC value
extern uint16_t g_wEmulatorPrevPpuPC;  // Previous PC value


//////////////////////////////////////////////////////////////////////


bool Emulator_Init();
bool Emulator_InitConfiguration(BKConfiguration configuration);
void Emulator_Done();
void Emulator_SetCPUBreakpoint(uint16_t address);
void Emulator_SetPPUBreakpoint(uint16_t address);
bool Emulator_IsBreakpoint();
void Emulator_SetSound(bool soundOnOff);
void Emulator_SetCovox(bool covoxOnOff);
void Emulator_Start();
void Emulator_Stop();
void Emulator_Reset();
int  Emulator_SystemFrame();
void Emulator_ProcessJoystick();
uint32_t Emulator_GetUptime();  // BK uptime, in seconds

void Emulator_GetScreenSize(int scrmode, int* pwid, int* phei);
const uint32_t * Emulator_GetPalette(int screenMode);
void Emulator_PrepareScreenRGB32(void* pBits, int screenMode);

// Update cached values after Run or Step
void Emulator_OnUpdate();
uint16_t Emulator_GetChangeRamStatus(uint16_t address);

void Emulator_SaveImage(LPCTSTR sFilePath);
void Emulator_LoadImage(LPCTSTR sFilePath);


//////////////////////////////////////////////////////////////////////
