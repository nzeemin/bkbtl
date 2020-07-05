/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// Joystick.cpp

#include "stdafx.h"
#include <Mmsystem.h>
#include "Joystick.h"


//////////////////////////////////////////////////////////////////////


int g_nJoystickCurrent = 0;  // Number of current joystick: 1 or 2; 0 = none
JOYCAPS g_JoystickCaps;  // Capabilities of the current joystick


//////////////////////////////////////////////////////////////////////


void Joystick_Init()
{
}
void Joystick_Done()
{
}

BOOL Joystick_SelectJoystick(int joystickNum)
{
    if (joystickNum == 0)
    {
        g_nJoystickCurrent = 0;
        return TRUE;
    }

    UINT numDevs = joyGetNumDevs();
    if (numDevs == 0)
        return FALSE;

    UINT joystick;
    if (joystickNum == 1)
        joystick = JOYSTICKID1;
    else if (joystickNum == 2)
        joystick = JOYSTICKID2;
    else
        return FALSE;

    MMRESULT result;
    JOYINFO joyinfo;
    result = joyGetPos(joystick, &joyinfo);
    if (result != JOYERR_NOERROR)
        return FALSE;
    result = joyGetDevCaps(joystick, &g_JoystickCaps, sizeof(g_JoystickCaps));
    if (result != JOYERR_NOERROR)
        return FALSE;

    g_nJoystickCurrent = joystickNum;
    return TRUE;
}

UINT Joystick_GetJoystickState()
{
    UINT joystick;
    if (g_nJoystickCurrent == 1)
        joystick = JOYSTICKID1;
    else if (g_nJoystickCurrent == 2)
        joystick = JOYSTICKID2;
    else
        return 0;

    JOYINFO joyinfo;
    MMRESULT result = joyGetPos(joystick, &joyinfo);
    if (result != JOYERR_NOERROR)
        return 0;

    UINT state = 0;
    // Buttons
    if (joyinfo.wButtons & JOY_BUTTON1) state |= JOYSTICK_BUTTON1;
    if (joyinfo.wButtons & JOY_BUTTON2) state |= JOYSTICK_BUTTON2;
    if (joyinfo.wButtons & JOY_BUTTON3) state |= JOYSTICK_BUTTON3;
    if (joyinfo.wButtons & JOY_BUTTON4) state |= JOYSTICK_BUTTON4;

    // Position
    UINT xCenter = (g_JoystickCaps.wXmax - g_JoystickCaps.wXmin) / 2 + g_JoystickCaps.wXmin;
    UINT xLeft = (g_JoystickCaps.wXmin + xCenter) / 2;
    UINT xRight = (g_JoystickCaps.wXmax + xCenter) / 2;
    if (joyinfo.wXpos < xLeft) state |= JOYSTICK_LEFT;
    if (joyinfo.wXpos > xRight) state |= JOYSTICK_RIGHT;

    UINT yCenter = (g_JoystickCaps.wYmax - g_JoystickCaps.wYmin) / 2 + g_JoystickCaps.wYmin;
    UINT yTop = (g_JoystickCaps.wYmin + yCenter) / 2;
    UINT yBottom = (g_JoystickCaps.wYmax + yCenter) / 2;
    if (joyinfo.wYpos < yTop) state |= JOYSTICK_UP;
    if (joyinfo.wYpos > yBottom) state |= JOYSTICK_DOWN;

    return state;
}


//////////////////////////////////////////////////////////////////////
