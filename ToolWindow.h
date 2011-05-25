/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// ToolWindow.h

#pragma once


//////////////////////////////////////////////////////////////////////


const LPCTSTR CLASSNAME_TOOLWINDOW = _T("BKBTLTOOLWINDOW");
const LPCTSTR CLASSNAME_OVERLAPPEDWINDOW = _T("BKBTLOVERLAPPEDWINDOW");

void ToolWindow_RegisterClass();
LRESULT CALLBACK ToolWindowWndProc(HWND, UINT, WPARAM, LPARAM);

void OverlappedWindow_RegisterClass();


//////////////////////////////////////////////////////////////////////

