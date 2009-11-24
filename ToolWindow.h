// ToolWindow.h

#pragma once


//////////////////////////////////////////////////////////////////////


const LPCTSTR CLASSNAME_TOOLWINDOW = _T("BKBTLTOOLWINDOW");
const LPCTSTR CLASSNAME_OVERLAPPEDWINDOW = _T("BKBTLOVERLAPPEDWINDOW");

void ToolWindow_RegisterClass();
LRESULT CALLBACK ToolWindowWndProc(HWND, UINT, WPARAM, LPARAM);

void OverlappedWindow_RegisterClass();


//////////////////////////////////////////////////////////////////////

