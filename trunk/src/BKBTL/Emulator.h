// Emulator.h

#pragma once

#include "emubase\Board.h"


//////////////////////////////////////////////////////////////////////
// BK special key codes

#define BK_KEY_STOP         0701
#define BK_KEY_AR2          0702
#define BK_KEY_BACKSHIFT    0703
#define BK_KEY_LOWER        0704
#define BK_KEY_UPPER        0705
#define BK_KEY_REPEAT       0706


//////////////////////////////////////////////////////////////////////


extern CMotherboard* g_pBoard;

extern BOOL g_okEmulatorRunning;

extern BYTE* g_pEmulatorRam;  // RAM values - for change tracking
extern BYTE* g_pEmulatorChangedRam;  // RAM change flags
extern WORD g_wEmulatorCpuPC;      // Current PC value
extern WORD g_wEmulatorPrevCpuPC;  // Previous PC value
extern WORD g_wEmulatorPpuPC;      // Current PC value
extern WORD g_wEmulatorPrevPpuPC;  // Previous PC value


//////////////////////////////////////////////////////////////////////


BOOL InitEmulator();
void DoneEmulator();
void Emulator_SetCPUBreakpoint(WORD address);
void Emulator_SetPPUBreakpoint(WORD address);
BOOL Emulator_IsBreakpoint();
void Emulator_SetSound(BOOL soundOnOff);
void Emulator_Start();
void Emulator_Stop();
void Emulator_Reset();
int Emulator_SystemFrame();

// Update cached values after Run or Step
void Emulator_OnUpdate();
WORD Emulator_GetChangeRamStatus(WORD address);

void Emulator_SaveImage(LPCTSTR sFilePath);
void Emulator_LoadImage(LPCTSTR sFilePath);


//////////////////////////////////////////////////////////////////////
