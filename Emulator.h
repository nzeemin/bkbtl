// Emulator.h

#pragma once

#include "emubase\Board.h"
#include "Views.h"


//////////////////////////////////////////////////////////////////////


extern CMotherboard* g_pBoard;
extern BKConfiguration g_nEmulatorConfiguration;  // Current configuration
extern BOOL g_okEmulatorRunning;

extern BYTE* g_pEmulatorRam;  // RAM values - for change tracking
extern BYTE* g_pEmulatorChangedRam;  // RAM change flags
extern WORD g_wEmulatorCpuPC;      // Current PC value
extern WORD g_wEmulatorPrevCpuPC;  // Previous PC value
extern WORD g_wEmulatorPpuPC;      // Current PC value
extern WORD g_wEmulatorPrevPpuPC;  // Previous PC value


//////////////////////////////////////////////////////////////////////


BOOL Emulator_Init();
BOOL Emulator_InitConfiguration(BKConfiguration configuration);
void Emulator_Done();
void Emulator_SetCPUBreakpoint(WORD address);
void Emulator_SetPPUBreakpoint(WORD address);
BOOL Emulator_IsBreakpoint();
void Emulator_SetSound(BOOL soundOnOff);
void Emulator_SetCovox(BOOL covoxOnOff);
void Emulator_Start();
void Emulator_Stop();
void Emulator_Reset();
int  Emulator_SystemFrame();
void Emulator_ProcessJoystick();

void Emulator_PrepareScreenRGB32(void* pBits, ScreenViewMode screenMode);

// Update cached values after Run or Step
void Emulator_OnUpdate();
WORD Emulator_GetChangeRamStatus(WORD address);

void Emulator_SaveImage(LPCTSTR sFilePath);
void Emulator_LoadImage(LPCTSTR sFilePath);


//////////////////////////////////////////////////////////////////////
