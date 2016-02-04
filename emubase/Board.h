/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// Board.h
//

#pragma once

#include "Defines.h"

class CProcessor;


//////////////////////////////////////////////////////////////////////

// Machine configurations
enum BKConfiguration
{
    // Configuration options
    BK_COPT_BK0010 = 0,
    BK_COPT_BK0011 = 1,
    BK_COPT_ROM_BASIC = 2,
    BK_COPT_ROM_FOCAL = 4,
    BK_COPT_FDD = 16,

    // Configurations BK-0010(01)
    BK_CONF_BK0010_BASIC =  // ��-0010(01) � BASIC-86
        BK_COPT_BK0010 | BK_COPT_ROM_BASIC,
    BK_CONF_BK0010_FOCAL =  // ��-0010(01) � ����� + �����
        BK_COPT_BK0010 | BK_COPT_ROM_FOCAL,
    BK_CONF_BK0010_FDD   =  // ��-0010(01) � ���� ����� � 16 �� ���
        BK_COPT_BK0010 | BK_COPT_FDD,
    // Configurations BK-0011M
    BK_CONF_BK0011       =  // ��-0011� ��� ����� �����
        BK_COPT_BK0011,
    BK_CONF_BK0011_FDD   =  // ��-0011� � ���� �����
        BK_COPT_BK0011 | BK_COPT_FDD,
};


//////////////////////////////////////////////////////////////////////


// TranslateAddress result code
#define ADDRTYPE_RAM     0  // RAM
#define ADDRTYPE_ROM    32  // ROM
#define ADDRTYPE_IO     64  // I/O port
#define ADDRTYPE_DENY  128  // Access denied
#define ADDRTYPE_MASK  224  // RAM type mask
#define ADDRTYPE_RAMMASK 7  // RAM chunk number mask

//floppy debug
#define FLOPPY_FSM_WAITFORLSB	0
#define FLOPPY_FSM_WAITFORMSB	1
#define FLOPPY_FSM_WAITFORTERM1	2
#define FLOPPY_FSM_WAITFORTERM2	3

// Emulator image constants
#define BKIMAGE_HEADER_SIZE 256
#define BKIMAGE_SIZE (BKIMAGE_HEADER_SIZE + (32 + 64 * 3) * 1024)
#define BKIMAGE_HEADER1 0x30304B41  // "BK00"
#define BKIMAGE_HEADER2 0x214C5442  // "BTL!"
#define BKIMAGE_VERSION 0x00010000  // 1.0


//////////////////////////////////////////////////////////////////////
// BK special key codes

#define BK_KEY_REPEAT       0201
#define BK_KEY_LOWER        0273
#define BK_KEY_UPPER        0274
#define BK_KEY_BACKSHIFT    0275
#define BK_KEY_AR2          0276
#define BK_KEY_STOP         0277

// ������� �� ��������� - ���������� � ����� KeyboardEvent
#define BK_KEY_JOYSTICK_BUTTON1 0210
#define BK_KEY_JOYSTICK_BUTTON2 0211
#define BK_KEY_JOYSTICK_BUTTON3 0212
#define BK_KEY_JOYSTICK_BUTTON4 0213
#define BK_KEY_JOYSTICK_RIGHT   0214
#define BK_KEY_JOYSTICK_DOWN    0215
#define BK_KEY_JOYSTICK_LEFT    0216
#define BK_KEY_JOYSTICK_UP      0217

// ��������� ���������� �� - ������������ �� ������ GetKeyboardRegister
#define KEYB_RUS		0x01
#define KEYB_LAT		0x02
#define KEYB_LOWERREG	0x10


// Tape emulator callback used to read a tape recorded data.
// Input:
//   samples    Number of samples to play.
// Output:
//   result     Bit to put in tape input port.
typedef bool (CALLBACK* TAPEREADCALLBACK)(unsigned int samples);

// Tape emulator callback used to write a data to tape.
// Input:
//   value      Sample value to write.
typedef void (CALLBACK* TAPEWRITECALLBACK)(int value, unsigned int samples);

// Sound generator callback function type
typedef void (CALLBACK* SOUNDGENCALLBACK)(unsigned short L, unsigned short R);

// Teletype callback function type - board calls it if symbol ready to transmit
typedef void (CALLBACK* TELETYPECALLBACK)(unsigned char value);

class CFloppyController;

//////////////////////////////////////////////////////////////////////

class CMotherboard  // BK computer
{
private:  // Devices
    CProcessor* m_pCPU;  // CPU device
    CFloppyController*  m_pFloppyCtl;  // FDD control
private:  // Memory
    uint16_t    m_Configuration;  // See BK_COPT_Xxx flag constants
    uint8_t     m_MemoryMap;      // Memory map, every bit defines how 8KB mapped: 0 - RAM, 1 - ROM
    uint8_t     m_MemoryMapOnOff; // Memory map, every bit defines how 8KB: 0 - deny, 1 - OK
    uint8_t*    m_pRAM;  // RAM, 8 * 16 = 128 KB
    uint8_t*    m_pROM;  // ROM, 4 * 16 = 64 KB
public:  // Construct / destruct
    CMotherboard();
    ~CMotherboard();
public:  // Getting devices
    CProcessor* GetCPU() { return m_pCPU; }
public:  // Memory access  //TODO: Make it private
    uint16_t    GetRAMWord(uint16_t offset) const;
    uint16_t    GetRAMWord(uint8_t chunk, uint16_t offset) const;
    uint8_t     GetRAMByte(uint16_t offset) const;
    uint8_t     GetRAMByte(uint8_t chunk, uint16_t offset) const;
    void        SetRAMWord(uint16_t offset, uint16_t word);
    void        SetRAMWord(uint8_t chunk, uint16_t offset, uint16_t word);
    void        SetRAMByte(uint16_t offset, uint8_t byte);
    void        SetRAMByte(uint8_t chunk, uint16_t offset, uint8_t byte);
    uint16_t    GetROMWord(uint16_t offset) const;
    uint8_t     GetROMByte(uint16_t offset) const;
public:  // Debug
    void        DebugTicks();  // One Debug PPU tick -- use for debug step or debug breakpoint
    void        SetCPUBreakpoint(uint16_t bp) { m_CPUbp = bp; } // Set CPU breakpoint
    bool        GetTrace() const { return m_okTraceCPU; }
    void        SetTrace(bool okTraceCPU) { m_okTraceCPU = okTraceCPU; }
public:  // System control
    void        SetConfiguration(uint16_t conf);
    void        Reset();  // Reset computer
    void        LoadROM(int bank, const uint8_t* pBuffer);  // Load 8 KB ROM image from the biffer
    void        LoadRAM(int startbank, const uint8_t* pBuffer, int length);  // Load data into the RAM
    void        Tick50();           // Tick 50 Hz - goes to CPU EVNT line
    void		TimerTick();		// Timer Tick, 31250 Hz, 32uS -- dividers are within timer routine
    void        ResetDevices();     // INIT signal
public:
    void        ExecuteCPU();  // Execute one CPU instruction
    bool        SystemFrame();  // Do one frame -- use for normal run
    void        KeyboardEvent(uint8_t scancode, bool okPressed, bool okAr2);  // Key pressed or released
    uint16_t    GetKeyboardRegister(void);
    uint16_t    GetPrinterOutPort() const { return m_Port177714out; }
    void        SetPrinterInPort(uint8_t data);
    bool        IsTapeMotorOn() const { return (m_Port177716tap & 0200) == 0; }
public:  // Floppy
    bool        AttachFloppyImage(int slot, LPCTSTR sFileName);
    void        DetachFloppyImage(int slot);
    bool        IsFloppyImageAttached(int slot);
    bool        IsFloppyReadOnly(int slot);
public:  // Callbacks
    void		SetTapeReadCallback(TAPEREADCALLBACK callback, int sampleRate);
    void        SetTapeWriteCallback(TAPEWRITECALLBACK callback, int sampleRate);
    void		SetSoundGenCallback(SOUNDGENCALLBACK callback);
    void		SetTeletypeCallback(TELETYPECALLBACK callback);
public:  // Memory
    // Read command for execution
    uint16_t GetWordExec(uint16_t address, bool okHaltMode) { return GetWord(address, okHaltMode, true); }
    // Read word from memory
    uint16_t GetWord(uint16_t address, bool okHaltMode) { return GetWord(address, okHaltMode, false); }
    // Read word
    uint16_t GetWord(uint16_t address, bool okHaltMode, bool okExec);
    // Write word
    void SetWord(uint16_t address, bool okHaltMode, uint16_t word);
    // Read byte
    uint8_t GetByte(uint16_t address, bool okHaltMode);
    // Write byte
    void SetByte(uint16_t address, bool okHaltMode, uint8_t byte);
    // Read word from memory for debugger
    uint16_t GetWordView(uint16_t address, bool okHaltMode, bool okExec, int* pValid) const;
    // Read word from port for debugger
    uint16_t GetPortView(uint16_t address) const;
    // Read SEL register
    uint16_t GetSelRegister() const { return m_Port177716; }
    // Get palette number 0..15 (BK0011 only)
    uint8_t GetPalette() const { return (m_Port177662wr >> 8) & 0x0f; }
    // Get video buffer address
    const uint8_t* GetVideoBuffer();
private:
    void        TapeInput(bool inputBit);  // Tape input bit received
private:
    // Determite memory type for given address - see ADDRTYPE_Xxx constants
    //   okHaltMode - processor mode (USER/HALT)
    //   okExec - true: read instruction for execution; false: read memory
    //   pOffset - result - offset in memory plane
    int TranslateAddress(uint16_t address, bool okHaltMode, bool okExec, uint16_t* pOffset) const;
private:  // Access to I/O ports
    uint16_t    GetPortWord(uint16_t address);
    void        SetPortWord(uint16_t address, uint16_t word);
    uint8_t     GetPortByte(uint16_t address);
    void        SetPortByte(uint16_t address, uint8_t byte);
public:  // Saving/loading emulator status
    //void        SaveToImage(uint8_t* pImage);
    //void        LoadFromImage(const uint8_t* pImage);
private:  // Ports: implementation
    uint16_t    m_Port177560;       // Serial port input state register
    uint16_t    m_Port177562;       // Serial port input data register
    uint16_t    m_Port177564;       // Serial port output state register
    uint16_t    m_Port177566;       // Serial port output data register
    uint16_t    m_Port177660;       // Keyboard status register
    uint16_t    m_Port177662rd;     // Keyboard register
    uint16_t    m_Port177662wr;     // Palette register
    uint16_t    m_Port177664;       // Scroll register
    uint16_t    m_Port177714in;     // Parallel port, input register
    uint16_t    m_Port177714out;    // Parallel port, output register
    uint16_t    m_Port177716;       // System register (read only)
    uint16_t    m_Port177716mem;    // System register (memory)
    uint16_t    m_Port177716tap;    // System register (tape)
private:  // Timer implementation
    uint16_t	m_timer;
    uint16_t	m_timerreload;
    uint16_t	m_timerflags;
    uint16_t	m_timerdivider;
    void		SetTimerReload(uint16_t val);	//sets timer reload value
    void		SetTimerState(uint16_t val);	//sets timer state
private:
    uint16_t    m_CPUbp;  // CPU breakpoint address
    bool        m_okTraceCPU;
private:
    TAPEREADCALLBACK m_TapeReadCallback;
    TAPEWRITECALLBACK m_TapeWriteCallback;
    int			m_nTapeSampleRate;
    SOUNDGENCALLBACK m_SoundGenCallback;
    TELETYPECALLBACK m_TeletypeCallback;
private:
    void        DoSound(void);
};


//////////////////////////////////////////////////////////////////////
