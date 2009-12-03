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
    BK_CONF_BK0010_MONIT =  // ¡ -0010(01), ÚÓÎ¸ÍÓ ÃÓÌËÚÓ
        BK_COPT_BK0010,
    BK_CONF_BK0010_BASIC =  // ¡ -0010(01) Ë BASIC-86
        BK_COPT_BK0010 | BK_COPT_ROM_BASIC,
    BK_CONF_BK0010_FOCAL =  // ¡ -0010(01) Ë ‘ÓÍ‡Î + ÚÂÒÚ˚
        BK_COPT_BK0010 | BK_COPT_ROM_FOCAL,
    BK_CONF_BK0010_FDD   =  // ¡ -0010(01) Ë ·ÎÓÍ  Õ√Ãƒ Ò 16  ¡ Œ«”
        BK_COPT_BK0010 | BK_COPT_FDD,
    // Configurations BK-0011M
    //TODO
};


//////////////////////////////////////////////////////////////////////


// TranslateAddress result code
#define ADDRTYPE_RAM     0  // RAM
#define ADDRTYPE_ROM     8  // ROM
#define ADDRTYPE_IO     16  // I/O port
#define ADDRTYPE_NONE  128  // No data
#define ADDRTYPE_DENY  255  // Access denied

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

#define BK_KEY_STOP         0201
#define BK_KEY_AR2          0202
#define BK_KEY_BACKSHIFT    0203
#define BK_KEY_LOWER        0204
#define BK_KEY_UPPER        0205
#define BK_KEY_REPEAT       0206


#define KEYB_RUS		0x01
#define KEYB_LAT		0x02
#define KEYB_LOWERREG	0x10

// Tape emulator callback used to read a tape recorded data.
// Input:
//   samples    Number of samples to play.
// Output:
//   result     Bit to put in tape input port.
typedef BOOL (CALLBACK* TAPEREADCALLBACK)(UINT samples);

// Sound generator callback function type
typedef void (CALLBACK* SOUNDGENCALLBACK)(unsigned short L, unsigned short R);

class CFloppyController;

//////////////////////////////////////////////////////////////////////

class CMotherboard  // BK computer
{
private:  // Devices
    CProcessor*     m_pCPU;  // CPU device
    //CFloppyController*  m_pFloppyCtl;  // FDD control
private:  // Memory
    WORD        m_Configuration;  // See BK_COPT_Xxx flag constants
    BYTE        m_MemoryMap;  // Memory map, every bit defines how 8KB mapped: 0 - RAM, 1 - ROM
    BYTE*       m_pRAM;  // RAM, 128 KB
    BYTE*       m_pROM;  // ROM, 32 KB
public:  // Construct / destruct
    CMotherboard();
    ~CMotherboard();
public:  // Getting devices
    CProcessor*     GetCPU() { return m_pCPU; }
public:  // Memory access
    WORD        GetRAMWord(WORD offset);
    BYTE        GetRAMByte(WORD offset);
    void        SetRAMWord(WORD offset, WORD word);
    void        SetRAMByte(WORD offset, BYTE byte);
    WORD        GetROMWord(WORD offset);
    BYTE        GetROMByte(WORD offset);
public:  // Debug
    void        DebugTicks();  // One Debug PPU tick -- use for debug step or debug breakpoint
    void        SetCPUBreakpoint(WORD bp) { m_CPUbp = bp; } // Set CPU breakpoint
public:  // System control
    void        SetConfiguration(WORD conf);
    void        Reset();  // Reset computer
    void        LoadROM(int bank, const BYTE* pBuffer);  // Load 8 KB ROM image from the biffer
    void        LoadRAM(const BYTE* pBuffer);  // Load 32 KB RAM image from the biffer
    void        Tick50();           // Tick 50 Hz - goes to CPU EVNT line
	void		TimerTick();		// Timer Tick, 31250 Hz, 32uS -- dividers are within timer routine
public:
    void        ExecuteCPU();  // Execute one CPU instruction
    void        ExecutePPU();  // Execute one PPU instruction
    BOOL        SystemFrame();  // Do one frame -- use for normal run
    void        KeyboardEvent(BYTE scancode, BOOL okPressed, BOOL okAr2);  // Key pressed or released
	WORD        GetKeyboardRegister(void);
    void        TapeInput(BOOL inputBit);  // Tape input bit received
public:  // Floppy    
    BOOL        AttachFloppyImage(int slot, LPCTSTR sFileName);
    void        DetachFloppyImage(int slot);
    BOOL        IsFloppyImageAttached(int slot);
    BOOL        IsFloppyReadOnly(int slot);
	WORD		GetFloppyState();
	WORD		GetFloppyData();
	void		SetFloppyState(WORD val);
	void		SetFloppyData(WORD val);
public:  // Callbacks
	void		SetTapeReadCallback(TAPEREADCALLBACK callback, int sampleRate);
	void		SetSoundGenCallback(SOUNDGENCALLBACK callback);
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
    WORD GetWordView(WORD address, BOOL okHaltMode, BOOL okExec, BOOL* pValid);
    // Read word from port for debugger
    WORD GetPortView(WORD address);
    // Read SEL register
    WORD GetSelRegister() { return m_Port177716; }
private:
    // Determite memory type for given address - see ADDRTYPE_Xxx constants
    //   okHaltMode - processor mode (USER/HALT)
    //   okExec - TRUE: read instruction for execution; FALSE: read memory
    //   pOffset - result - offset in memory plane
    int TranslateAddress(WORD address, BOOL okHaltMode, BOOL okExec, WORD* pOffset);
private:  // Access to I/O ports
    WORD GetPortWord(WORD address);
    void SetPortWord(WORD address, WORD word);
    BYTE GetPortByte(WORD address);
    void SetPortByte(WORD address, BYTE byte);
public:  // Saving/loading emulator status
    //void        SaveToImage(BYTE* pImage);
    //void        LoadFromImage(const BYTE* pImage);
private:  // Ports: implementation
    WORD        m_Port177660;       // Keyboard status register
    WORD        m_Port177662rd;     // Keyboard register
    WORD        m_Port177662wr;     // Palette register
    WORD        m_Port177664;       // Scroll register
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
	int			m_nTapeReadSampleRate;
    SOUNDGENCALLBACK m_SoundGenCallback;
private:
	void DoSound(void);
};


//////////////////////////////////////////////////////////////////////
