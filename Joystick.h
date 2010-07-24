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
