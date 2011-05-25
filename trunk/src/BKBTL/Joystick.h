/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// Joystick.h
//

#pragma once


//////////////////////////////////////////////////////////////////////

// "Standard" SWCorp. joystick
//#define JOYSTICK_BUTTON1  00001
//#define JOYSTICK_BUTTON2  00002
//#define JOYSTICK_BUTTON3  00004
//#define JOYSTICK_BUTTON4  00010
//#define JOYSTICK_RIGHT    00020
//#define JOYSTICK_DOWN     00040
//#define JOYSTICK_LEFT     01000
//#define JOYSTICK_UP       02000

// "Break House" joystick
#define JOYSTICK_BUTTON1  0x01
#define JOYSTICK_BUTTON2  0x02
#define JOYSTICK_BUTTON3  0x04
#define JOYSTICK_BUTTON4  0x08
#define JOYSTICK_LEFT     0x10
#define JOYSTICK_DOWN     0x20
#define JOYSTICK_RIGHT    0x40
#define JOYSTICK_UP       0x80

void    Joystick_Init();
void    Joystick_Done();

BOOL    Joystick_SelectJoystick(int joystickNum);
UINT    Joystick_GetJoystickState();


//////////////////////////////////////////////////////////////////////
