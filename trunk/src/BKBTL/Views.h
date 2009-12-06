// Views.h
// Defines for all views of the application

#pragma once

//////////////////////////////////////////////////////////////////////


const LPCTSTR CLASSNAME_SCREENVIEW  = _T("BKBTLSCREEN");
const LPCTSTR CLASSNAME_KEYBOARDVIEW = _T("BKBTLKEYBOARD");
const LPCTSTR CLASSNAME_DEBUGVIEW   = _T("BKBTLDEBUG");
const LPCTSTR CLASSNAME_DISASMVIEW  = _T("BKBTLDISASM");
const LPCTSTR CLASSNAME_MEMORYVIEW  = _T("BKBTLMEMORY");
const LPCTSTR CLASSNAME_MEMORYMAPVIEW  = _T("BKBTLMEMORYMAP");
const LPCTSTR CLASSNAME_CONSOLEVIEW = _T("BKBTLCONSOLE");
const LPCTSTR CLASSNAME_TAPEVIEW    = _T("BKBTLTAPE");


//////////////////////////////////////////////////////////////////////
// ScreenView

enum ScreenViewMode {
    ColorScreen = 1,
    BlackWhiteScreen = 2,
};

extern HWND g_hwndScreen;  // Screen View window handle

void ScreenView_RegisterClass();
void ScreenView_Init();
void ScreenView_Done();
ScreenViewMode ScreenView_GetMode();
void ScreenView_SetMode(ScreenViewMode);
int ScreenView_GetHeightMode();
void ScreenView_SetHeightMode(int);
void ScreenView_PrepareScreen();
void ScreenView_ScanKeyboard();
void ScreenView_ProcessKeyboard();
void ScreenView_RedrawScreen();  // Force to call PrepareScreen and to draw the image
void CreateScreenView(HWND hwndParent, int x, int y, int cxWidth);
LRESULT CALLBACK ScreenViewWndProc(HWND, UINT, WPARAM, LPARAM);
void ScreenView_SaveScreenshot(LPCTSTR sFileName);
void ScreenView_KeyEvent(BYTE keyscan, BOOL pressed);


//////////////////////////////////////////////////////////////////////
// KeyboardView

extern HWND g_hwndKeyboard;  // Keyboard View window handle

void KeyboardView_RegisterClass();
void KeyboardView_Init();
void KeyboardView_Done();
void CreateKeyboardView(HWND hwndParent, int x, int y, int width, int height);
LRESULT CALLBACK KeyboardViewWndProc(HWND, UINT, WPARAM, LPARAM);


//////////////////////////////////////////////////////////////////////
// DebugView

extern HWND g_hwndDebug;  // Debug View window handle

void DebugView_RegisterClass();
void CreateDebugView(HWND hwndParent, int x, int y, int width, int height);
LRESULT CALLBACK DebugViewWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK DebugViewViewerWndProc(HWND, UINT, WPARAM, LPARAM);
void DebugView_OnUpdate();
BOOL DebugView_IsRegisterChanged(int regno);


//////////////////////////////////////////////////////////////////////
// DisasmView

extern HWND g_hwndDisasm;  // Disasm View window handle

void DisasmView_RegisterClass();
void CreateDisasmView(HWND hwndParent, int x, int y, int width, int height);
LRESULT CALLBACK DisasmViewWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK DisasmViewViewerWndProc(HWND, UINT, WPARAM, LPARAM);
void DisasmView_OnUpdate();
void DisasmView_SetCurrentProc(BOOL okCPU);


//////////////////////////////////////////////////////////////////////
// MemoryView

extern HWND g_hwndMemory;  // Memory view window handler

void MemoryView_RegisterClass();
void CreateMemoryView(HWND hwndParent, int x, int y, int width, int height);
LRESULT CALLBACK MemoryViewWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK MemoryViewViewerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);


//////////////////////////////////////////////////////////////////////
// MemoryMapView

extern HWND g_hwndMemoryMap;  // MemoryMap view window handler

void MemoryMapView_RegisterClass();
void CreateMemoryMapView(int x, int y);
LRESULT CALLBACK MemoryMapViewWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK MemoryMapViewViewerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);


//////////////////////////////////////////////////////////////////////
// ConsoleView

extern HWND g_hwndConsole;  // Console View window handle

void ConsoleView_RegisterClass();
void CreateConsoleView(HWND hwndParent, int x, int y, int width, int height);
LRESULT CALLBACK ConsoleViewWndProc(HWND, UINT, WPARAM, LPARAM);
void ConsoleView_Print(LPCTSTR message);
void ConsoleView_Activate();
void ConsoleView_Step();


//////////////////////////////////////////////////////////////////////
// TapeView

extern HWND g_hwndTape;  // Tape View window handle

void TapeView_RegisterClass();
void CreateTapeView(HWND hwndParent, int x, int y, int width, int height);
LRESULT CALLBACK TapeViewWndProc(HWND, UINT, WPARAM, LPARAM);


//////////////////////////////////////////////////////////////////////
