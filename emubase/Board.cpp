// Board.cpp
//

#include "stdafx.h"
#include "Emubase.h"
#include "Board.h"


//////////////////////////////////////////////////////////////////////

CMotherboard::CMotherboard ()
{
    // Create devices
    m_pCPU = new CProcessor(this);
    m_pFloppyCtl = NULL;

    m_TapeReadCallback = NULL;
    m_TapeWriteCallback = NULL;
    m_nTapeSampleRate = 0;
    m_SoundGenCallback = NULL;

    // Allocate memory for RAM and ROM
    m_pRAM = (BYTE*) ::malloc(128 * 1024);  ::memset(m_pRAM, 0, 128 * 1024);
    m_pROM = (BYTE*) ::malloc(64 * 1024);  ::memset(m_pROM, 0, 64 * 1024);

    SetConfiguration(BK_CONF_BK0010_BASIC);  // Default configuration

    Reset();
}

CMotherboard::~CMotherboard ()
{
    // Delete devices
    delete m_pCPU;
    if (m_pFloppyCtl != NULL)
        delete m_pFloppyCtl;

    // Free memory
    ::free(m_pRAM);
    ::free(m_pROM);
}

void CMotherboard::SetConfiguration(WORD conf)
{
    m_Configuration = conf;

    // Define memory map
    m_MemoryMap = 0xf0;  // By default, 000000-077777 is RAM, 100000-177777 is ROM
    m_MemoryMapOnOff = 0xff;  // By default, all 8K blocks are valid
    if (m_Configuration & BK_COPT_FDD)  // FDD with 16KB extra memory
        m_MemoryMap = 0xf0 - 32 - 64;  // 16KB extra memory mapped to 120000-157777
    if ((m_Configuration & (BK_COPT_BK0010 | BK_COPT_ROM_FOCAL)) == (BK_COPT_BK0010 | BK_COPT_ROM_FOCAL))
        m_MemoryMapOnOff = 0xbf;

    // Clean RAM/ROM
    ::memset(m_pRAM, 0, 128 * 1024);
    ::memset(m_pROM, 0, 64 * 1024);

    if (m_pFloppyCtl == NULL && (conf & BK_COPT_FDD) != 0)
    {
        m_pFloppyCtl = new CFloppyController();
    }
    if (m_pFloppyCtl != NULL && (conf & BK_COPT_FDD) == 0)
    {
        delete m_pFloppyCtl;  m_pFloppyCtl = NULL;
    }
}

void CMotherboard::Reset () 
{
    m_pCPU->Stop();

    // Reset ports
    m_Port177560 = m_Port177562 = 0;
    m_Port177564 = 0200;
    m_Port177566 = 0;
    m_Port177660 = 0100;
    m_Port177662rd = 0;
    m_Port177662wr = 047400;
    m_Port177664 = 01330;
    m_Port177714in = m_Port177714out = 0;
    m_Port177716 = ((m_Configuration & BK_COPT_BK0011) ? 0140000 : 0100000) | 0200;
    m_Port177716mem = 0000001;
    m_Port177716tap = 0;
    m_timer = m_timerreload = m_timerdivider = 0;
    m_timerflags = 0177400;

    ResetDevices();

    m_pCPU->Start();
}

// Load 8 KB ROM image from the buffer
//   bank - number of 8k ROM bank, 0..7
void CMotherboard::LoadROM(int bank, const BYTE* pBuffer)
{
    ASSERT(bank >= 0 && bank <= 7);
    ::memcpy(m_pROM + 8192 * bank, pBuffer, 8192);
}

void CMotherboard::LoadRAM(int startbank, const BYTE* pBuffer, int length)
{
    ASSERT(pBuffer != NULL);
    ASSERT(startbank >= 0 && startbank < 15);
    int address = 8192 * startbank;
    ASSERT(address + length <= 128 * 1024);
    ::memcpy(m_pRAM + address, pBuffer, length);
}


// Floppy ////////////////////////////////////////////////////////////

BOOL CMotherboard::IsFloppyImageAttached(int slot)
{
    ASSERT(slot >= 0 && slot < 4);
    if (m_pFloppyCtl == NULL)
        return FALSE;
    return m_pFloppyCtl->IsAttached(slot);
}

BOOL CMotherboard::IsFloppyReadOnly(int slot)
{
    ASSERT(slot >= 0 && slot < 4);
    if (m_pFloppyCtl == NULL)
        return FALSE;
    return m_pFloppyCtl->IsReadOnly(slot);
}

BOOL CMotherboard::AttachFloppyImage(int slot, LPCTSTR sFileName)
{
    ASSERT(slot >= 0 && slot < 4);
    if (m_pFloppyCtl == NULL)
        return FALSE;
    return m_pFloppyCtl->AttachImage(slot, sFileName);
}

void CMotherboard::DetachFloppyImage(int slot)
{
    ASSERT(slot >= 0 && slot < 4);
    if (m_pFloppyCtl == NULL)
        return;
    m_pFloppyCtl->DetachImage(slot);
}


// Работа с памятью //////////////////////////////////////////////////

WORD CMotherboard::GetRAMWord(WORD offset)
{
    return *((WORD*)(m_pRAM + offset)); 
}
WORD CMotherboard::GetRAMWord(BYTE chunk, WORD offset)
{
    DWORD dwOffset = (((DWORD)chunk & 7) << 14) + offset;
    return *((WORD*)(m_pRAM + dwOffset)); 
}
BYTE CMotherboard::GetRAMByte(WORD offset) 
{ 
    return m_pRAM[offset]; 
}
BYTE CMotherboard::GetRAMByte(BYTE chunk, WORD offset) 
{ 
    DWORD dwOffset = (((DWORD)chunk & 7) << 14) + offset;
    return m_pRAM[dwOffset]; 
}
void CMotherboard::SetRAMWord(WORD offset, WORD word) 
{
    *((WORD*)(m_pRAM + offset)) = word;
}
void CMotherboard::SetRAMWord(BYTE chunk, WORD offset, WORD word) 
{
    DWORD dwOffset = (((DWORD)chunk & 7) << 14) + offset;
    *((WORD*)(m_pRAM + dwOffset)) = word;
}
void CMotherboard::SetRAMByte(WORD offset, BYTE byte) 
{
    m_pRAM[offset] = byte; 
}
void CMotherboard::SetRAMByte(BYTE chunk, WORD offset, BYTE byte) 
{
    DWORD dwOffset = (((DWORD)chunk & 7) << 14) + offset;
    m_pRAM[dwOffset] = byte; 
}

WORD CMotherboard::GetROMWord(WORD offset)
{
    ASSERT(offset < 1024 * 64);
    return *((WORD*)(m_pROM + offset)); 
}
BYTE CMotherboard::GetROMByte(WORD offset) 
{ 
    ASSERT(offset < 1024 * 64);
    return m_pROM[offset]; 
}


//////////////////////////////////////////////////////////////////////


void CMotherboard::ResetDevices()
{
    if (m_pFloppyCtl != NULL)
        m_pFloppyCtl->Reset();

    // Reset ports
    m_Port177560 = m_Port177562 = 0;
    m_Port177564 = 0200;
    m_Port177566 = 0;
}

void CMotherboard::Tick50()  // 50 Hz timer
{
    if ((m_Port177662wr & 040000) == 0)
    {
        m_pCPU->TickIRQ2();
    }
}

void CMotherboard::ExecuteCPU()
{
    m_pCPU->Execute();
}

void CMotherboard::TimerTick() // Timer Tick, 31250 Hz = 32 мкс (BK-0011), 23437.5 Hz = 42.67 мкс (BK-0010)
{
    if ((m_timerflags & 1) == 1)  // Timer is off, nothing to do
        return;

    m_timerdivider++;
    
    BOOL flag = FALSE;
    switch ((m_timerflags >> 5) & 3)  // биты 5,6 -- prescaler
    {
        case 0:  // 32 мкс
            flag = TRUE;
            break;
        case 1:  // 32 * 16 = 512 мкс
            flag = (m_timerdivider >= 16);
            break;
        case 2: // 32 * 4 = 128 мкс
            flag = (m_timerdivider >= 4);
            break;
        case 3:  // 32 * 16 * 4 = 2048 мкс, 8129 тактов процессора
            flag = (m_timerdivider >= 64);
            break;
    }

    if (!flag)  // Nothing happened
        return; 

    m_timerdivider = 0;

    m_timer--;
    if (m_timer != 0)
        return;

    if (m_timerflags & 010)  // If OneShot bit set then stop counting
        m_timerflags &= ~020;
    if ((m_timerflags & 2) == 0)  // If not WrapAround then reload
    {
        m_timer = m_timerreload;
        if (m_timerflags & 4)
            m_timerflags |= 0200;  // Set Ready bit
    }
}

void CMotherboard::SetTimerReload(WORD val)	 // Sets timer reload value
{
    //m_timerreload = val & 077777;
    m_timerreload = val;
    if ((m_timerflags & 1) == 0)
        m_timer = m_timerreload;
}
void CMotherboard::SetTimerState(WORD val) // Sets timer state
{
    if ((val & 1) && ((m_timerflags & 1) == 0))
        //m_timer = m_timerreload & 077777;
        m_timer = m_timerreload;

    //m_timerflags &= 0250;  // Clear everything but bits 7,5,3
    //m_timerflags |= (val & (~0250));  // Preserve bits 753
    m_timerflags = 0177400 | (val & 0x00ff);
}

void CMotherboard::DebugTicks()
{
    m_pCPU->SetInternalTick(0);
    m_pCPU->Execute();
    if (m_pFloppyCtl != NULL)
        m_pFloppyCtl->Periodic();
}


/*
Каждый фрейм равен 1/25 секунды = 40 мс = 20000 тиков, 1 тик = 2 мкс.
12 МГц = 1 / 12000000 = 0.83(3) мкс
В каждый фрейм происходит:
* 120000 тиков ЦП - 6 раз за тик (БК-0010, 12МГц / 4 = 3 МГц, 3.3(3) мкс), либо
* 160000 тиков ЦП - 8 раз за тик (БК-0011, 12МГц / 3 = 4 МГц, 2.5 мкс)
* программируемый таймер - на каждый 128-й тик процессора; 42.6(6) мкс либо 32 мкс
* 2 тика IRQ2 50 Гц, в 0-й и 10000-й тик фрейма
* 625 тиков FDD - каждый 32-й тик (300 RPM = 5 оборотов в секунду)
* 68571 тиков AY-3-891x: 1.714275 МГц (12МГц / 7 = 1.714 МГц, 5.83(3) мкс)
*/
BOOL CMotherboard::SystemFrame()
{
    int frameProcTicks = (m_Configuration & BK_COPT_BK0011) ? 8 : 6;
    const int audioticks = 20286 / (SOUNDSAMPLERATE / 25);
    const int teletypeTicks = 20000 / (9600 / 25);
    int teletypeTxCount = 0;

    int frameTapeTicks = 0, tapeSamplesPerFrame = 0, tapeReadError = 0;
    if (m_TapeReadCallback != NULL || m_TapeWriteCallback != NULL)
    {
        tapeSamplesPerFrame = m_nTapeSampleRate / 25;
        frameTapeTicks = 20000 / tapeSamplesPerFrame;
    }

    int timerTicks = 0;

    for (int frameticks = 0; frameticks < 20000; frameticks++)
    {
        for (int procticks = 0; procticks < frameProcTicks; procticks++)  // CPU ticks
        {
            if (m_pCPU->GetPC() == m_CPUbp)
                return FALSE;  // Breakpoint
            m_pCPU->Execute();

            timerTicks++;
            if (timerTicks >= 128)
            {
                TimerTick();  // System timer tick: 31250 Hz = 32uS (BK-0011), 23437.5 Hz = 42.67 uS (BK-0010)
                timerTicks = 0;
            }
        }

        if (frameticks % 10000 == 0)
        {
            Tick50();  // 1/50 timer event
        }

        if ((m_Configuration & BK_COPT_FDD) && (frameticks % 42 == 0))  // FDD tick
        {
            if (m_pFloppyCtl != NULL)
                m_pFloppyCtl->Periodic();
        }

        if (frameticks % audioticks == 0)  // AUDIO tick
            DoSound();

        if ((m_TapeReadCallback != NULL || m_TapeWriteCallback != NULL) && frameticks % frameTapeTicks == 0)
        {
            int tapeSamples = 0;
            const int readsTotal = 20000 / frameTapeTicks;
            while (true)
            {
                tapeSamples++;
                tapeReadError += readsTotal;
                if (2 * tapeReadError >= tapeSamples)
                {
                    tapeReadError -= tapeSamplesPerFrame;
                    break;
                }
            }

            // Reading the tape
            if (m_TapeReadCallback != NULL)
            {
                BOOL tapeBit = (*m_TapeReadCallback)(tapeSamples);
                TapeInput(tapeBit);
            }
            else if (m_TapeWriteCallback != NULL)
            {
                unsigned int value = 0;
                switch (m_Port177716tap & 0140)
                {
                case 0000: value = 0;                  break;
                case 0040: value = 0xffffffff / 4;     break;
                case 0100: value = 0xffffffff / 4 * 3; break;
                case 0140: value = 0xffffffff;         break;
                }
                (*m_TapeWriteCallback)(value, tapeSamples);
            }
        }

        if (frameticks % teletypeTicks)
        {
            if (teletypeTxCount > 0)
            {
                teletypeTxCount--;
                if (teletypeTxCount == 0)  // Translation countdown finished - the byte translated
                {
                    (*m_TeletypeCallback)(m_Port177566 & 0xff);
                    m_Port177564 |= 0200;
                    if (m_Port177564 & 0100)
                    {
                         m_pCPU->InterruptVIRQ(1, 064);
                    }
                }
            }
            else if ((m_Port177564 & 0200) == 0)
            {
                teletypeTxCount = 8;  // Start translation countdown
            }
        }
    }

    return TRUE;
}

// Key pressed or released
void CMotherboard::KeyboardEvent(BYTE scancode, BOOL okPressed, BOOL okAr2)
{
    if ((scancode & 0xf8) == 0210)  // События от джойстика
    {
        WORD mask = 0;
        switch (scancode)
        {
        case BK_KEY_JOYSTICK_BUTTON1: mask = 0x01; break;
        case BK_KEY_JOYSTICK_BUTTON2: mask = 0x02; break;
        case BK_KEY_JOYSTICK_BUTTON3: mask = 0x04; break;
        case BK_KEY_JOYSTICK_BUTTON4: mask = 0x08; break;
        case BK_KEY_JOYSTICK_LEFT:    mask = 0x10; break;
        case BK_KEY_JOYSTICK_DOWN:    mask = 0x20; break;
        case BK_KEY_JOYSTICK_RIGHT:   mask = 0x40; break;
        case BK_KEY_JOYSTICK_UP:      mask = 0x80; break;
        }

        if (okPressed)
            m_Port177714in |= mask;
        else
            m_Port177714in &= ~mask;

        return;
    }

    if (scancode == BK_KEY_STOP)
    {
        if (okPressed)
        {
            m_pCPU->InterruptVIRQ(1, 0000004);
        }
        return;
    }

    if (!okPressed)  // Key released
    {
        m_Port177716 |= 0100;  // Reset "Key pressed" flag in system register
        m_Port177716 |= 4;  // Set "ready" flag
        return;
    }

    m_Port177716 &= ~0100;  // Set "Key pressed" flag in system register
    m_Port177716 |= 4;  // Set "ready" flag

    if ((m_Port177660 & 0200) == 0)
    {
        m_Port177662rd = scancode & 0177;
        m_Port177660 |= 0200;  // "Key ready" flag in keyboard state register
        if ((m_Port177660 & 0100) == 0100)  // Keyboard interrupt enabled
        {
            m_pCPU->InterruptVIRQ(1, (okAr2 ? 0274 : 060));
        }
    }
}

void CMotherboard::TapeInput(BOOL inputBit)
{
    WORD tapeBitOld = (m_Port177716 & 040);
    WORD tapeBitNew = inputBit ? 0 : 040;
    if (tapeBitNew != tapeBitOld)
    {
        m_Port177716 = (m_Port177716 & ~040) | tapeBitNew;  // Write new tape bit
        m_Port177716 |= 4;  // Set "ready" flag
    }
}

void CMotherboard::SetPrinterInPort(BYTE data)
{
    m_Port177714in = data;
}


//////////////////////////////////////////////////////////////////////
// Motherboard: memory management

// Read word from memory for debugger
WORD CMotherboard::GetWordView(WORD address, BOOL okHaltMode, BOOL okExec, int* pAddrType)
{
    WORD offset;
    int addrtype = TranslateAddress(address, okHaltMode, okExec, &offset);

    *pAddrType = addrtype;

    switch (addrtype & ADDRTYPE_MASK)
    {
    case ADDRTYPE_RAM:
        return GetRAMWord(offset & 0177776);  //TODO: Use (addrtype & ADDRTYPE_RAMMASK) bits
    case ADDRTYPE_ROM:
        return GetROMWord(offset);
    case ADDRTYPE_IO:
        return 0;  // I/O port, not memory
    case ADDRTYPE_DENY:
        return 0;  // This memory is inaccessible for reading
    }

    ASSERT(FALSE);  // If we are here - then addrtype has invalid value
    return 0;
}

WORD CMotherboard::GetWord(WORD address, BOOL okHaltMode, BOOL okExec)
{
    WORD offset;
    int addrtype = TranslateAddress(address, okHaltMode, okExec, &offset);

    switch (addrtype & ADDRTYPE_MASK)
    {
    case ADDRTYPE_RAM:
        return GetRAMWord(addrtype & ADDRTYPE_RAMMASK, offset & 0177776);
    case ADDRTYPE_ROM:
        return GetROMWord(offset);
    case ADDRTYPE_IO:
        //TODO: What to do if okExec == TRUE ?
        return GetPortWord(address);
    case ADDRTYPE_DENY:
        m_pCPU->MemoryError();
        return 0;
    }

    ASSERT(FALSE);  // If we are here - then addrtype has invalid value
    return 0;
}

BYTE CMotherboard::GetByte(WORD address, BOOL okHaltMode)
{
    WORD offset;
    int addrtype = TranslateAddress(address, okHaltMode, FALSE, &offset);

    switch (addrtype & ADDRTYPE_MASK)
    {
    case ADDRTYPE_RAM:
        return GetRAMByte(addrtype & ADDRTYPE_RAMMASK, offset);  //TODO: Use (addrtype & ADDRTYPE_RAMMASK) bits
    case ADDRTYPE_ROM:
        return GetROMByte(offset);
    case ADDRTYPE_IO:
        //TODO: What to do if okExec == TRUE ?
        return GetPortByte(address);
    case ADDRTYPE_DENY:
        m_pCPU->MemoryError();
        return 0;
    }

    ASSERT(FALSE);  // If we are here - then addrtype has invalid value
    return 0;
}

void CMotherboard::SetWord(WORD address, BOOL okHaltMode, WORD word)
{
    WORD offset;

    int addrtype = TranslateAddress(address, okHaltMode, FALSE, &offset);

    switch (addrtype & ADDRTYPE_MASK)
    {
    case ADDRTYPE_RAM:
        SetRAMWord(addrtype & ADDRTYPE_RAMMASK, offset & 0177776, word);
        return;
    case ADDRTYPE_ROM:  // Writing to ROM: exception
        m_pCPU->MemoryError();
        return;
    case ADDRTYPE_IO:
        SetPortWord(address, word);
        return;
    case ADDRTYPE_DENY:
        m_pCPU->MemoryError();
        return;
    }

    ASSERT(FALSE);  // If we are here - then addrtype has invalid value
}

void CMotherboard::SetByte(WORD address, BOOL okHaltMode, BYTE byte)
{
    WORD offset;
    int addrtype = TranslateAddress(address, okHaltMode, FALSE, &offset);

    switch (addrtype & ADDRTYPE_MASK)
    {
    case ADDRTYPE_RAM:
        SetRAMByte(addrtype & ADDRTYPE_RAMMASK, offset, byte);
        return;
    case ADDRTYPE_ROM:  // Writing to ROM: exception
        m_pCPU->MemoryError();
        return;
    case ADDRTYPE_IO:
        SetPortByte(address, byte);
        return;
    case ADDRTYPE_DENY:
        m_pCPU->MemoryError();
        return;
    }

    ASSERT(FALSE);  // If we are here - then addrtype has invalid value
}

// Calculates video buffer start address, for screen drawing procedure
const BYTE* CMotherboard::GetVideoBuffer()
{
    if (m_Configuration & BK_COPT_BK0011)
    {
        DWORD offset = (m_Port177662wr & 0100000) ? 6 : 5;
        offset *= 040000;
        return (m_pRAM + offset);
    }
    else
    {
        return (m_pRAM + 040000);
    }
}

int CMotherboard::TranslateAddress(WORD address, BOOL okHaltMode, BOOL okExec, WORD* pOffset)
{
    // При подключенном блоке дисковода, его ПЗУ занимает адреса 160000-167776, при этом адреса 170000-177776 остаются под порты.
    // Без подключенного дисковода, порты занимают адреса 177600-177776.
    WORD portStartAddr = (m_Configuration & BK_COPT_FDD) ? 0170000 : 0177600;
    if (address >= portStartAddr)  // Port
    {
        *pOffset = address;
        return ADDRTYPE_IO;
    }

    if ((m_Configuration & BK_COPT_BK0011) == 0)  // БК-0010, нет управления памятью
    {
        int memoryBlock = (address >> 13) & 7;  // 8K block number 0..7
        BOOL okValid = (m_MemoryMapOnOff >> memoryBlock) & 1;  // 1 - OK, 0 - deny
        if (!okValid)
            return ADDRTYPE_DENY;
        BOOL okRom = (m_MemoryMap >> memoryBlock) & 1;  // 1 - ROM, 0 - RAM
        if (okRom)
            address -= 0100000;
        
        *pOffset = address;
        return (okRom) ? ADDRTYPE_ROM : ADDRTYPE_RAM;
    }
    else  // БК-0011, управление памятью через регистр 177716
    {
        const int memoryBlockMap[8] = { 1, 5, 2, 3, 4, 7, 0, 6 };
        int memoryRamChunk = 0;  // Number of 16K RAM chunk, 0..7
        int memoryBank = (address >> 14) & 3;  // 16K bank number 0..3
        int addrType = 0;
        switch (memoryBank)
        {
        case 0:  // 000000-037777: всегда страница ОЗУ 0
            addrType = ADDRTYPE_RAM;
            break;
        case 1:  // 040000-077777, окно 0, страница ОЗУ 0..7
            memoryRamChunk = memoryBlockMap[(m_Port177716mem >> 12) & 7];  // 8 chanks #0..7
            addrType = ADDRTYPE_RAM | memoryRamChunk;
            address &= 037777;
            break;
        case 2:  // 100000-137777, окно 1, страница ОЗУ 0..7 или ПЗУ
            if (m_Port177716mem & 033)  // Включено ПЗУ 0..3
            {
                addrType = ADDRTYPE_ROM;
                int memoryRomChunk = 0;
                if (m_Port177716mem & 1)
                {
                    //addrType = ADDRTYPE_DENY;
                    //break;
                    memoryRomChunk = 0;
                }
                else if (m_Port177716mem & 2)
                    memoryRomChunk = 1;
                else if (m_Port177716mem & 8)
                {
                    addrType = ADDRTYPE_DENY;
                    break;
                    //memoryRomChunk = 2;
                }
                else
                {
                    addrType = ADDRTYPE_DENY;
                    break;
                }
                //else if (m_Port177716mem & 16)
                //    memoryRomChunk = 3;
                address = (address & 037777) + memoryRomChunk * 040000;
                break;
            }
            
            // Включено ОЗУ 0..7
            memoryRamChunk = memoryBlockMap[(m_Port177716mem >> 8) & 7];
            addrType = ADDRTYPE_RAM | memoryRamChunk;
            address &= 037777;
            break;
        case 3:  // 140000-177777
            addrType = ADDRTYPE_ROM;
            address -= 040000;
            break;
        }

        *pOffset = address;
        return addrType;
    }
}

BYTE CMotherboard::GetPortByte(WORD address)
{
    if (address & 1)
        return GetPortWord(address & 0xfffe) >> 8;

    return (BYTE) GetPortWord(address);
}

WORD CMotherboard::GetPortWord(WORD address)
{
    switch (address)
    {
    case 0177560:  // Serial port recieve status
        return m_Port177560;
    case 0177562:  // Serial port recieve data
        return m_Port177562;
    case 0177564:  // Serial port translate status
        return m_Port177564;
    case 0177566:  // Serial port interrupt vector
        return 060;

    case 0177706:  // System Timer counter start value -- регистр установки таймера
        return m_timerreload;
    case 0177710:  // System Timer Counter -- регистр счетчика таймера
        {
            if (m_timerflags & 0200)
                m_timerflags &= ~0200;  // Clear it
            return m_timer;
        }
    case 0177712:  // System Timer Manage -- регистр управления таймера
        {
            WORD res = m_timerflags;
            //m_timerflags &= ~010;  // Clear overflow
            //m_timerflags &= ~040;  // Clear external int
            return res;
        }
    case 0177660:  // Keyboard status register
        return m_Port177660;
    case 0177662:  // Keyboard register
        m_Port177660 &= ~0200;  // Reset "Ready" bit
        return m_Port177662rd;

    case 0177664:  // Scroll register
        return m_Port177664;

    case 0177714:  // Parallel port register: printer, joystick
        return m_Port177714in;

    case 0177716:  // System register
    {
        WORD value = m_Port177716;
        m_Port177716 &= ~4;  // Reset bit 2
        return value;
    }

    case 0177130:
        if ((m_Configuration & BK_COPT_FDD) == 0)
        {
            m_pCPU->MemoryError();
            return 0;
        }
        if (m_pFloppyCtl != NULL)
        {
            WORD state = m_pFloppyCtl->GetState();
//#if !defined(PRODUCT)
//            DebugLogFormat(_T("Floppy GETSTATE %06o\t\tCPU %06o\n"), state, m_pCPU->GetInstructionPC());
//#endif
            return state;
        }
        return 0;

    case 0177132:
        if ((m_Configuration & BK_COPT_FDD) == 0)
        {
            m_pCPU->MemoryError();
            return 0;
        }
        if (m_pFloppyCtl != NULL)
        {
            WORD word = m_pFloppyCtl->GetData();
//#if !defined(PRODUCT)
//            DebugLogFormat(_T("Floppy READ\t\t%04x\tCPU %06o\n"), word, m_pCPU->GetInstructionPC());
//#endif
            return word;
        }
        return 0;

    default: 
        m_pCPU->MemoryError();
        return 0;
    }

    return 0; 
}

// Read word from port for debugger
WORD CMotherboard::GetPortView(WORD address)
{
    switch (address)
    {
    case 0177560:  // Serial port recieve status
        return m_Port177560;
    case 0177562:  // Serial port recieve data
        return m_Port177562;
    case 0177564:  // Serial port translate status
        return m_Port177564;
    case 0177566:  // Serial port interrupt vector
        return 060;

    case 0177706:  // System Timer counter start value -- регистр установки таймера
        return m_timerreload;
    case 0177710:  // System Timer Counter -- регистр счетчика таймера
        return m_timer;
    case 0177712:  // System Timer Manage -- регистр управления таймера
        return m_timerflags;

    case 0177660:  // Keyboard status register
        return m_Port177660;
    case 0177662:  // Keyboard data register
        return m_Port177662rd;

    case 0177664:  // Scroll register
        return m_Port177664;

    case 0177714:  // Parallel port register
        return m_Port177714in;

    case 0177716:  // System register
        return m_Port177716;

    case 0177130:  // Floppy state
        if (m_pFloppyCtl != NULL)
            return m_pFloppyCtl->GetStateView();
        return 0;
    case 0177132:  // Floppy data
        if (m_pFloppyCtl != NULL)
            return m_pFloppyCtl->GetDataView();
        return 0;

    default:
        return 0;
    }
}

void CMotherboard::SetPortByte(WORD address, BYTE byte)
{
    WORD word;
    if (address & 1)
    {
        word = GetPortWord(address & 0xfffe);
        word &= 0xff;
        word |= byte << 8;
        SetPortWord(address & 0xfffe, word);
    }
    else
    {
        word = GetPortWord(address);
        word &= 0xff00;
        SetPortWord(address, word | byte);
    }
}

//void DebugPrintFormat(LPCTSTR pszFormat, ...);  //DEBUG
void CMotherboard::SetPortWord(WORD address, WORD word)
{
    switch (address)
    {
    case 0177560:
        m_Port177560 = word;
        break;
    case 0177562:
        //TODO
        break;
    case 0177564:  // Serial port output status register
//#if !defined(PRODUCT)
//        DebugPrintFormat(_T("177564 write '%06o'\r\n"), word);
//#endif
        m_Port177564 = word;
        break;
    case 0177566:  // Serial port output data
//#if !defined(PRODUCT)
//        DebugPrintFormat(_T("177566 write '%c'\r\n"), (BYTE)word);
//#endif
        m_Port177566 = word;
        m_Port177564 &= ~0200;
        break;

    case 0177700: case 0177702: case 0177704:  // Unknown something
        break;
    
    case 0177706:  // System Timer reload value -- регистр установки таймера
        SetTimerReload(word);
        break;
    case 0177710:  // System Timer Counter -- регистр реверсивного счетчика таймера
        m_timer = word;
        break;
    case 0177712:  // System Timer Manage -- регистр управления таймера
        SetTimerState(word);
        break;

    case 0177714:  // Parallel port register: printer and Covox
        m_Port177714out = word;
        break;

    case 0177716:  // System register - memory management, tape management
        if (word & 04000)
        {
            m_Port177716mem = word;
//#if !defined(PRODUCT)
//            DebugLogFormat(_T("177716mem %06o\t\t%06o\r\n"), word, m_pCPU->GetInstructionPC());
//#endif
        }
        else
        {
            m_Port177716tap = word;
        }
        break;

    case 0177660:  // Keyboard status register
        //TODO
        break;

    case 0177662:  // Palette register
        m_Port177662wr = word;
        break;

    case 0177664:  // Scroll register
        m_Port177664 = word & 01377;
        break;

    case 0177130:  // Регистр управления КНГМД
        if (m_pFloppyCtl != NULL)
        {
//#if !defined(PRODUCT)
//            DebugLogFormat(_T("Floppy COMMAND %06o\t\tCPU %06o\r\n"), word, m_pCPU->GetInstructionPC());
//#endif
            m_pFloppyCtl->SetCommand(word);
        }
        break;
    case 0177132:  // Регистр данных КНГМД
        if (m_pFloppyCtl != NULL)
            m_pFloppyCtl->WriteData(word);
        break;

    default:
        m_pCPU->MemoryError();
        break;
    }
}


//////////////////////////////////////////////////////////////////////
//
// Emulator image format:
//   32 bytes  - Header
//   32 bytes  - Board status
//   32 bytes  - CPU status
//   64 bytes  - CPU memory/IO controller status
//   32 Kbytes - ROM image
//   64 Kbytes - RAM image

//void CMotherboard::SaveToImage(BYTE* pImage)
//{
//    // Board data
//    WORD* pwImage = (WORD*) (pImage + 32);
//    *pwImage++ = m_timer;
//    *pwImage++ = m_timerreload;
//    *pwImage++ = m_timerflags;
//    *pwImage++ = m_timerdivider;
//    *pwImage++ = (WORD) m_chan0disabled;
//
//    // CPU status
//    BYTE* pImageCPU = pImage + 64;
//    m_pCPU->SaveToImage(pImageCPU);
//    // PPU status
//    BYTE* pImagePPU = pImageCPU + 32;
//    m_pPPU->SaveToImage(pImagePPU);
//    // CPU memory/IO controller status
//    BYTE* pImageCpuMem = pImagePPU + 32;
//    m_pFirstMemCtl->SaveToImage(pImageCpuMem);
//    // PPU memory/IO controller status
//    BYTE* pImagePpuMem = pImageCpuMem + 64;
//    m_pSecondMemCtl->SaveToImage(pImagePpuMem);
//
//    // ROM
//    BYTE* pImageRom = pImage + 256;
//    memcpy(pImageRom, m_pROM, 32 * 1024);
//    // RAM planes 0, 1, 2
//    BYTE* pImageRam = pImageRom + 32 * 1024;
//    memcpy(pImageRam, m_pRAM[0], 64 * 1024);
//    pImageRam += 64 * 1024;
//    memcpy(pImageRam, m_pRAM[1], 64 * 1024);
//    pImageRam += 64 * 1024;
//    memcpy(pImageRam, m_pRAM[2], 64 * 1024);
//}
//void CMotherboard::LoadFromImage(const BYTE* pImage)
//{
//    // Board data
//    WORD* pwImage = (WORD*) (pImage + 32);
//    m_timer = *pwImage++;
//    m_timerreload = *pwImage++;
//    m_timerflags = *pwImage++;
//    m_timerdivider = *pwImage++;
//    m_chan0disabled = (BYTE) *pwImage++;
//
//    // CPU status
//    const BYTE* pImageCPU = pImage + 64;
//    m_pCPU->LoadFromImage(pImageCPU);
//    // PPU status
//    const BYTE* pImagePPU = pImageCPU + 32;
//    m_pPPU->LoadFromImage(pImagePPU);
//    // CPU memory/IO controller status
//    const BYTE* pImageCpuMem = pImagePPU + 32;
//    m_pFirstMemCtl->LoadFromImage(pImageCpuMem);
//    // PPU memory/IO controller status
//    const BYTE* pImagePpuMem = pImageCpuMem + 64;
//    m_pSecondMemCtl->LoadFromImage(pImagePpuMem);
//
//    // ROM
//    const BYTE* pImageRom = pImage + 256;
//    memcpy(m_pROM, pImageRom, 32 * 1024);
//    // RAM planes 0, 1, 2
//    const BYTE* pImageRam = pImageRom + 32 * 1024;
//    memcpy(m_pRAM[0], pImageRam, 64 * 1024);
//    pImageRam += 64 * 1024;
//    memcpy(m_pRAM[1], pImageRam, 64 * 1024);
//    pImageRam += 64 * 1024;
//    memcpy(m_pRAM[2], pImageRam, 64 * 1024);
//}

//void CMotherboard::FloppyDebug(BYTE val)
//{
////#if !defined(PRODUCT)
////    TCHAR buffer[512];
////#endif
///*
//m_floppyaddr=0;
//m_floppystate=FLOPPY_FSM_WAITFORLSB;
//#define FLOPPY_FSM_WAITFORLSB	0
//#define FLOPPY_FSM_WAITFORMSB	1
//#define FLOPPY_FSM_WAITFORTERM1	2
//#define FLOPPY_FSM_WAITFORTERM2	3
//
//*/
//	switch(m_floppystate)
//	{
//		case FLOPPY_FSM_WAITFORLSB:
//			if(val!=0xff)
//			{
//				m_floppyaddr=val;
//				m_floppystate=FLOPPY_FSM_WAITFORMSB;
//			}
//			break;
//		case FLOPPY_FSM_WAITFORMSB:
//			if(val!=0xff)
//			{
//				m_floppyaddr|=val<<8;
//				m_floppystate=FLOPPY_FSM_WAITFORTERM1;
//			}
//			else
//			{
//				m_floppystate=FLOPPY_FSM_WAITFORLSB;
//			}
//			break;
//		case FLOPPY_FSM_WAITFORTERM1:
//			if(val==0xff)
//			{ //done
//				WORD par;
//				BYTE trk,sector,side;
//
//				par=m_pFirstMemCtl->GetWord(m_floppyaddr,0);
//
////#if !defined(PRODUCT)
////				wsprintf(buffer,_T(">>>>FDD Cmd %d "),(par>>8)&0xff);
////				DebugPrint(buffer);
////#endif
//                par=m_pFirstMemCtl->GetWord(m_floppyaddr+2,0);
//				side=par&0x8000?1:0;
////#if !defined(PRODUCT)
////				wsprintf(buffer,_T("Side %d Drv %d, Type %d "),par&0x8000?1:0,(par>>8)&0x7f,par&0xff);
////				DebugPrint(buffer);
////#endif
//				par=m_pFirstMemCtl->GetWord(m_floppyaddr+4,0);
//				sector=(par>>8)&0xff;
//				trk=par&0xff;
////#if !defined(PRODUCT)
////				wsprintf(buffer,_T("Sect %d, Trk %d "),(par>>8)&0xff,par&0xff);
////				DebugPrint(buffer);
////				PrintOctalValue(buffer,m_pFirstMemCtl->GetWord(m_floppyaddr+6,0));
////				DebugPrint(_T("Addr "));
////				DebugPrint(buffer);
////#endif
//				par=m_pFirstMemCtl->GetWord(m_floppyaddr+8,0);
////#if !defined(PRODUCT)
////				wsprintf(buffer,_T(" Block %d Len %d\n"),trk*20+side*10+sector-1,par);
////				DebugPrint(buffer);
////#endif
//				
//				m_floppystate=FLOPPY_FSM_WAITFORLSB;
//			}
//			break;
//	
//	}
//}


//////////////////////////////////////////////////////////////////////

WORD CMotherboard::GetKeyboardRegister(void)
{
    WORD res = 0;

    WORD mem000042 = GetRAMWord(000042);
    res |= (mem000042 & 0100000) == 0 ? KEYB_LAT : KEYB_RUS;
    
    return res;
}

void CMotherboard::DoSound(void)
{
    if (m_SoundGenCallback == NULL)
        return;

    BOOL bSoundBit = (m_Port177716tap & 0100) != 0;

    if (bSoundBit)
        (*m_SoundGenCallback)(0x1fff,0x1fff);
    else
        (*m_SoundGenCallback)(0x0000,0x0000);
}

void CMotherboard::SetTapeReadCallback(TAPEREADCALLBACK callback, int sampleRate)
{
    if (callback == NULL)  // Reset callback
    {
        m_TapeReadCallback = NULL;
        m_nTapeSampleRate = 0;
    }
    else
    {
        m_TapeReadCallback = callback;
        m_nTapeSampleRate = sampleRate;
        m_TapeWriteCallback = NULL;
    }
}

void CMotherboard::SetTapeWriteCallback(TAPEWRITECALLBACK callback, int sampleRate)
{
    if (callback == NULL)  // Reset callback
    {
        m_TapeWriteCallback = NULL;
        m_nTapeSampleRate = 0;
    }
    else
    {
        m_TapeWriteCallback = callback;
        m_nTapeSampleRate = sampleRate;
        m_TapeReadCallback = NULL;
    }
}

void CMotherboard::SetSoundGenCallback(SOUNDGENCALLBACK callback)
{
    if (callback == NULL)  // Reset callback
    {
        m_SoundGenCallback = NULL;
    }
    else
    {
        m_SoundGenCallback = callback;
    }
}

void CMotherboard::SetTeletypeCallback(TELETYPECALLBACK callback)
{
    if (callback == NULL)  // Reset callback
    {
        m_TeletypeCallback = NULL;
    }
    else
    {
        m_TeletypeCallback = callback;
    }
}


//////////////////////////////////////////////////////////////////////
