/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// Emubase.h  Header for use of all emubase classes
//

#pragma once

#include "Board.h"
#include "Processor.h"


//////////////////////////////////////////////////////////////////////


#define SOUNDSAMPLERATE  22050


//////////////////////////////////////////////////////////////////////
// Disassembler

// Disassemble one instruction of KM1801VM2 processor
//   pMemory - memory image (we read only words of the instruction)
//   sInstr  - instruction mnemonics buffer - at least 8 characters
//   sArg    - instruction arguments buffer - at least 32 characters
//   Return value: number of words in the instruction
uint16_t DisassembleInstruction(const uint16_t* pMemory, uint16_t addr, TCHAR* sInstr, TCHAR* sArg);

bool Disasm_CheckForJump(const uint16_t* memory, int* pDelta);

// Prepare "Jump Hint" string, and also calculate condition for conditional jump
// Returns: jump prediction flag: true = will jump, false = will not jump
bool Disasm_GetJumpConditionHint(
    const uint16_t* memory, const CProcessor * pProc, const CMotherboard * pBoard, LPTSTR buffer);

// Prepare "Instruction Hint" for a regular instruction (not a branch/jump one)
// buffer, buffer2 - buffers for 1st and 2nd lines of the instruction hint, min size 42
// Returns: number of hint lines; 0 = no hints
int Disasm_GetInstructionHint(
    const uint16_t* memory, const CProcessor * pProc, const CMotherboard * pBoard,
    LPTSTR buffer, LPTSTR buffer2);


//////////////////////////////////////////////////////////////////////
// CFloppy

#define FLOPPY_FSM_IDLE         0

#define FLOPPY_CMD_CORRECTION250             04
#define FLOPPY_CMD_CORRECTION500            010
#define FLOPPY_CMD_ENGINESTART              020
#define FLOPPY_CMD_SIDEUP                   040
#define FLOPPY_CMD_DIR                     0100
#define FLOPPY_CMD_STEP                    0200
#define FLOPPY_CMD_SEARCHSYNC              0400
#define FLOPPY_CMD_SKIPSYNC               01000
//dir == 0 to center (towards trk0)
//dir == 1 from center (towards trk80)

#define FLOPPY_STATUS_TRACK0                 01
#define FLOPPY_STATUS_RDY                    02
#define FLOPPY_STATUS_WRITEPROTECT           04
#define FLOPPY_STATUS_MOREDATA             0200
#define FLOPPY_STATUS_CHECKSUMOK         040000
#define FLOPPY_STATUS_INDEXMARK         0100000

#define FLOPPY_RAWTRACKSIZE             6250     // Raw track size, bytes
#define FLOPPY_RAWMARKERSIZE            (FLOPPY_RAWTRACKSIZE / 2)
#define FLOPPY_INDEXLENGTH              30       // Length of index hole, in bytes of raw track image

struct CFloppyDrive
{
    FILE* fpFile;
    bool okReadOnly;    // Write protection flag
    uint16_t dataptr;       // Data offset within m_data - "head" position
    uint8_t data[FLOPPY_RAWTRACKSIZE];  // Raw track image for the current track
    uint8_t marker[FLOPPY_RAWMARKERSIZE];  // Marker positions
    uint16_t datatrack;     // Track number of data in m_data array
    uint16_t dataside;      // Disk side of data in m_data array

public:
    CFloppyDrive();
    void Reset();
};

class CFloppyController
{
protected:
    CFloppyDrive m_drivedata[4];
    int m_drive;            // Drive number: from 0 to 3; -1 if not selected
    CFloppyDrive* m_pDrive; // Current drive; nullptr if not selected
    uint16_t m_track;       // Track number: from 0 to 79
    uint16_t m_side;        // Disk side: 0 or 1
    uint16_t m_status;      // See FLOPPY_STATUS_XXX defines
    uint16_t m_flags;       // See FLOPPY_CMD_XXX defines
    uint16_t m_datareg;     // Read mode data register
    uint16_t m_writereg;    // Write mode data register
    bool m_writeflag;       // Write mode data register has data
    bool m_writemarker;     // Write marker in m_marker
    uint16_t m_shiftreg;    // Write mode shift register
    bool m_shiftflag;       // Write mode shift register has data
    bool m_shiftmarker;     // Write marker in m_marker
    bool m_writing;         // TRUE = write mode, false = read mode
    bool m_searchsync;      // Read sub-mode: TRUE = search for sync, false = just read
    bool m_crccalculus;     // TRUE = CRC is calculated now
    bool m_trackchanged;    // TRUE = m_data was changed - need to save it into the file
    bool m_okTrace;         // Trace mode on/off

public:
    CFloppyController();
    ~CFloppyController();
    void Reset();

public:
    bool AttachImage(int drive, LPCTSTR sFileName);
    void DetachImage(int drive);
    bool IsAttached(int drive) const { return (m_drivedata[drive].fpFile != nullptr); }
    bool IsReadOnly(int drive) const { return m_drivedata[drive].okReadOnly; } // return (m_status & FLOPPY_STATUS_WRITEPROTECT) != 0; }
    bool IsEngineOn() { return (m_flags & FLOPPY_CMD_ENGINESTART) != 0; }
    uint16_t GetData(void);         // Reading port 177132 - data
    uint16_t GetState(void);        // Reading port 177130 - device status
    uint16_t GetDataView() const { return m_datareg; }  // Get port 177132 value for debugger
    uint16_t GetStateView() const { return m_status; }  // Get port 177130 value for debugger
    void SetCommand(uint16_t cmd);  // Writing to port 177130 - commands
    void WriteData(uint16_t Data);  // Writing to port 177132 - data
    void Periodic();            // Rotate disk; call it each 64 us - 15625 times per second
    void SetTrace(bool okTrace) { m_okTrace = okTrace; }  // Set trace mode on/off

private:
    void PrepareTrack();
    void FlushChanges();  // If current track was changed - save it
};


//////////////////////////////////////////////////////////////////////

class CSoundAY
{
protected:
    int index;
    int ready;
    unsigned Regs[16];
    int32_t lastEnable;
    int32_t PeriodA, PeriodB, PeriodC, PeriodN, PeriodE;
    int32_t CountA, CountB, CountC, CountN, CountE;
    uint32_t VolA, VolB, VolC, VolE;
    uint8_t EnvelopeA, EnvelopeB, EnvelopeC;
    uint8_t OutputA, OutputB, OutputC, OutputN;
    int8_t CountEnv;
    uint8_t Hold, Alternate, Attack, Holding;
    int32_t RNG;
protected:
    static unsigned int VolTable[32];

public:
    CSoundAY();
    void Reset();

public:
    void SetReg(int r, int v);
    void Callback(uint8_t* stream, int length);

protected:
    static void BuildMixerTable();
};


//////////////////////////////////////////////////////////////////////
