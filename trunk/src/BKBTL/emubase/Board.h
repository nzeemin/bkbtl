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
    BK_CONF_BK0010_BASIC =  // БК-0010(01) и BASIC-86
        BK_COPT_BK0010 | BK_COPT_ROM_BASIC,
    BK_CONF_BK0010_FOCAL =  // БК-0010(01) и Фокал + тесты
        BK_COPT_BK0010 | BK_COPT_ROM_FOCAL,
    BK_CONF_BK0010_FDD   =  // БК-0010(01) и блок КНГМД с 16 КБ ОЗУ
        BK_COPT_BK0010 | BK_COPT_FDD,
    // Configurations BK-0011M
    BK_CONF_BK0011       =  // БК-0011М без блока КНГМД
        BK_COPT_BK0011,
    BK_CONF_BK0011_FDD   =  // БК-0011М и блок КНГМД
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

// События от джойстика - передавать в метод KeyboardEvent
#define BK_KEY_JOYSTICK_BUTTON1 0210
#define BK_KEY_JOYSTICK_BUTTON2 0211
#define BK_KEY_JOYSTICK_BUTTON3 0212
#define BK_KEY_JOYSTICK_BUTTON4 0213
#define BK_KEY_JOYSTICK_RIGHT   0214
#define BK_KEY_JOYSTICK_DOWN    0215
#define BK_KEY_JOYSTICK_LEFT    0216
#define BK_KEY_JOYSTICK_UP      0217

// Состояния клавиатуры БК - возвращаются из метода GetKeyboardRegister
#define KEYB_RUS		0x01
#define KEYB_LAT		0x02
#define KEYB_LOWERREG	0x10


// Tape emulator callback used to read a tape recorded data.
// Input:
//   samples    Number of samples to play.
// Output:
//   result     Bit to put in tape input port.
typedef BOOL (CALLBACK* TAPEREADCALLBACK)(unsigned int samples);

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
    CProcessor*     m_pCPU;  // CPU device
    CFloppyController*  m_pFloppyCtl;  // FDD control
private:  // Memory
    WORD        m_Configuration;  // See BK_COPT_Xxx flag constants
    BYTE        m_MemoryMap;      // Memory map, every bit defines how 8KB mapped: 0 - RAM, 1 - ROM
    BYTE        m_MemoryMapOnOff; // Memory map, every bit defines how 8KB: 0 - deny, 1 - OK
    BYTE*       m_pRAM;  // RAM, 8 * 16 = 128 KB
    BYTE*       m_pROM;  // ROM, 4 * 16 = 64 KB
public:  // Construct / destruct
    CMotherboard();
    ~CMotherboard();
public:  // Getting devices
    CProcessor*     GetCPU() { return m_pCPU; }
public:  // Memory access  //TODO: Make it private
    WORD        GetRAMWord(WORD offset);
    WORD        GetRAMWord(BYTE chunk, WORD offset);
    BYTE        GetRAMByte(WORD offset);
    BYTE        GetRAMByte(BYTE chunk, WORD offset);
    void        SetRAMWord(WORD offset, WORD word);
    void        SetRAMWord(BYTE chunk, WORD offset, WORD word);
    void        SetRAMByte(WORD offset, BYTE byte);
    void        SetRAMByte(BYTE chunk, WORD offset, BYTE byte);
    WORD        GetROMWord(WORD offset);
    BYTE        GetROMByte(WORD offset);
public:  // Debug
    void        DebugTicks();  // One Debug PPU tick -- use for debug step or debug breakpoint
    void        SetCPUBreakpoint(WORD bp) { m_CPUbp = bp; } // Set CPU breakpoint
public:  // System control
    void        SetConfiguration(WORD conf);
    void        Reset();  // Reset computer
    void        LoadROM(int bank, const BYTE* pBuffer);  // Load 8 KB ROM image from the biffer
    void        LoadRAM(int startbank, const BYTE* pBuffer, int length);  // Load data into the RAM
    void        Tick50();           // Tick 50 Hz - goes to CPU EVNT line
    void		TimerTick();		// Timer Tick, 31250 Hz, 32uS -- dividers are within timer routine
    void        ResetDevices();     // INIT signal
public:
    void        ExecuteCPU();  // Execute one CPU instruction
    BOOL        SystemFrame();  // Do one frame -- use for normal run
    void        KeyboardEvent(BYTE scancode, BOOL okPressed, BOOL okAr2);  // Key pressed or released
    WORD        GetKeyboardRegister(void);
    WORD        GetPrinterOutPort() const { return m_Port177714out; }
    void        SetPrinterInPort(BYTE data);
public:  // Floppy    
    BOOL        AttachFloppyImage(int slot, LPCTSTR sFileName);
    void        DetachFloppyImage(int slot);
    BOOL        IsFloppyImageAttached(int slot);
    BOOL        IsFloppyReadOnly(int slot);
public:  // Callbacks
    void		SetTapeReadCallback(TAPEREADCALLBACK callback, int sampleRate);
    void        SetTapeWriteCallback(TAPEWRITECALLBACK callback, int sampleRate);
    void		SetSoundGenCallback(SOUNDGENCALLBACK callback);
    void		SetTeletypeCallback(TELETYPECALLBACK callback);
public:  // Memory
    // Read command for execution
    WORD GetWordExec(WORD address, BOOL okHaltMode) { return GetWord(address, okHaltMode, TRUE); }
    // Read word from memory
    WORD GetWord(WORD address, BOOL okHaltMode) { return GetWord(address, okHaltMode, FALSE); }
    // Read word
    WORD GetWord(WORD address, BOOL okHaltMode, BOOL okExec);
    // Write word
    void SetWord(WORD address, BOOL okHaltMode, WORD word);
    // Read byte
    BYTE GetByte(WORD address, BOOL okHaltMode);
    // Write byte
    void SetByte(WORD address, BOOL okHaltMode, BYTE byte);
    // Read word from memory for debugger
    WORD GetWordView(WORD address, BOOL okHaltMode, BOOL okExec, int* pValid);
    // Read word from port for debugger
    WORD GetPortView(WORD address);
    // Read SEL register
    WORD GetSelRegister() const { return m_Port177716; }
    // Get palette number 0..15 (BK0011 only)
    BYTE GetPalette() const { return (m_Port177662wr >> 8) & 0x0f; }
    // Get video buffer address
    const BYTE* GetVideoBuffer();
private:
    void        TapeInput(BOOL inputBit);  // Tape input bit received
private:
    // Determite memory type for given address - see ADDRTYPE_Xxx constants
    //   okHaltMode - processor mode (USER/HALT)
    //   okExec - TRUE: read instruction for execution; FALSE: read memory
    //   pOffset - result - offset in memory plane
    int TranslateAddress(WORD address, BOOL okHaltMode, BOOL okExec, WORD* pOffset);
private:  // Access to I/O ports
    WORD        GetPortWord(WORD address);
    void        SetPortWord(WORD address, WORD word);
    BYTE        GetPortByte(WORD address);
    void        SetPortByte(WORD address, BYTE byte);
public:  // Saving/loading emulator status
    //void        SaveToImage(BYTE* pImage);
    //void        LoadFromImage(const BYTE* pImage);
private:  // Ports: implementation
    WORD        m_Port177560;       // Serial port input state register
    WORD        m_Port177562;       // Serial port input data register
    WORD        m_Port177564;       // Serial port output state register
    WORD        m_Port177566;       // Serial port output data register
    WORD        m_Port177660;       // Keyboard status register
    WORD        m_Port177662rd;     // Keyboard register
    WORD        m_Port177662wr;     // Palette register
    WORD        m_Port177664;       // Scroll register
    WORD        m_Port177714in;     // Parallel port, input register
    WORD        m_Port177714out;    // Parallel port, output register
    WORD        m_Port177716;       // System register (read only)
    WORD        m_Port177716mem;    // System register (memory)
    WORD        m_Port177716tap;    // System register (tape)
private:  // Timer implementation
    WORD		m_timer;
    WORD		m_timerreload;
    WORD		m_timerflags;
    WORD		m_timerdivider;
    void		SetTimerReload(WORD val);	//sets timer reload value
    void		SetTimerState(WORD val);	//sets timer state
private:
    WORD        m_CPUbp;  // CPU breakpoint address
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
