// Board.cpp
//

#include "StdAfx.h"
#include "Emubase.h"
#include "Board.h"


//////////////////////////////////////////////////////////////////////

CMotherboard::CMotherboard ()
{
    // Create devices
    m_pCPU = new CProcessor(this);
    //m_pFloppyCtl = new CFloppyController();

	m_TapeReadCallback = NULL;
	m_nTapeReadSampleRate = 0;
    m_SoundGenCallback = NULL;

    // Allocate memory for RAM and ROM
    m_pRAM = (BYTE*) ::LocalAlloc(LPTR, 128 * 1024);
    m_pROM = (BYTE*) ::LocalAlloc(LPTR, 32 * 1024);

    SetConfiguration(BK_CONF_BK0010_BASIC);  // Default configuration

    Reset();
}

CMotherboard::~CMotherboard ()
{
    // Delete devices
    delete m_pCPU;
    //delete m_pFloppyCtl;

    // Free memory
    ::LocalFree(m_pRAM);
    ::LocalFree(m_pROM);
}

void CMotherboard::SetConfiguration(WORD conf)
{
    m_Configuration = conf;

    // Define memory map
    m_MemoryMap = 0xf0;  // By default, 000000-077777 is RAM, 100000-177777 is ROM
    if (m_Configuration & BK_COPT_FDD)  // FDD with 16KB extra memory
        m_MemoryMap = 0xf0 - 32 - 64;  // 16KB extra memory mapped to 120000-157777

    // Clean RAM/ROM
    ::ZeroMemory(m_pRAM, 128 * 1024);
    ::ZeroMemory(m_pROM, 32 * 1024);
}

void CMotherboard::Reset () 
{
    m_pCPU->Stop();

    //m_pFloppyCtl->Reset();

    // Reset ports
    m_Port177660 = 0100;
    m_Port177662rd = 0;
    m_Port177662wr = 047400;
    m_Port177664 = 0;
    m_Port177716 = ((m_Configuration & BK_COPT_BK0011) ? 0140000 : 0100000) | 0200;
    m_Port177716mem = m_Port177716tap = 0;
    m_timer = m_timerreload = m_timerflags = m_timerdivider = 0;

    m_pCPU->Start();
}

void CMotherboard::LoadROM(int bank, const BYTE* pBuffer)  // Load 8 KB ROM image from the buffer
{
    ASSERT(bank >= 0 && bank < 4);
    ::CopyMemory(m_pROM + 8192 * bank, pBuffer, 8192);
}

void CMotherboard::LoadRAM(const BYTE* pBuffer)  // Load 64 KB RAM image from the buffer
{
    ::CopyMemory(m_pRAM, pBuffer, 65536);
}


// Floppy ////////////////////////////////////////////////////////////

BOOL CMotherboard::IsFloppyImageAttached(int slot)
{
    //ASSERT(slot >= 0 && slot < 4);
    //return m_pFloppyCtl->IsAttached(slot);
    return FALSE;  //STUB
}

BOOL CMotherboard::IsFloppyReadOnly(int slot)
{
    //ASSERT(slot >= 0 && slot < 4);
    //return m_pFloppyCtl->IsReadOnly(slot);
    return FALSE;  //STUB
}

BOOL CMotherboard::AttachFloppyImage(int slot, LPCTSTR sFileName)
{
    //ASSERT(slot >= 0 && slot < 4);
    //return m_pFloppyCtl->AttachImage(slot, sFileName);
    return FALSE;  //STUB
}

void CMotherboard::DetachFloppyImage(int slot)
{
    //ASSERT(slot >= 0 && slot < 4);
    //m_pFloppyCtl->DetachImage(slot);
}


// Работа с памятью //////////////////////////////////////////////////

WORD CMotherboard::GetRAMWord(WORD offset)
{
    return *((WORD*)(m_pRAM + offset)); 
}
BYTE CMotherboard::GetRAMByte(WORD offset) 
{ 
    return m_pRAM[offset]; 
}
void CMotherboard::SetRAMWord(WORD offset, WORD word) 
{
	*((WORD*)(m_pRAM + offset)) = word;
}
void CMotherboard::SetRAMByte(WORD offset, BYTE byte) 
{
    m_pRAM[offset] = byte; 
}

WORD CMotherboard::GetROMWord(WORD offset)
{
    ASSERT(offset < 32768);
    return *((WORD*)(m_pROM + offset)); 
}
BYTE CMotherboard::GetROMByte(WORD offset) 
{ 
    ASSERT(offset < 32768);
    return m_pROM[offset]; 
}


//////////////////////////////////////////////////////////////////////

void CMotherboard::Tick50()  // 50 Hz timer
{
    if ((m_Port177662wr & 040000) == 0)
    {
        m_pCPU->TickEVNT();
    }
}

void CMotherboard::ExecuteCPU()
{
    m_pCPU->Execute();
}

void CMotherboard::TimerTick() // Timer Tick, 31250 Hz = 32uS (BK-0011), 23437.5 Hz = 42.67 uS (BK-0010)
{
    if ((m_timerflags & 1) == 1)  // Timer is off, nothing to do
        return;

	int flag = 0;
    m_timerdivider++;
    
    switch((m_timerflags >> 6) & 3)
    {
        case 0:  // 32 мкс
            flag = 1;
            m_timerdivider = 0;
            break;
        case 1:  // 32 * 16 = 512 мкс
            if (m_timerdivider >= 16)
            {
                flag = 1;
                m_timerdivider = 0;
            }
            break;
        case 2: // 32 * 4 = 128 мкс
            if (m_timerdivider >= 4)
            {
                flag = 1;
                m_timerdivider = 0;
            }
            break;
        case 3:  // 32 * 16 * 4 = 2048 мкс
            if (m_timerdivider >= 64)
            {
                flag = 1;
                m_timerdivider = 0;
            }
            break;
    }

    if (flag == 0)  // Nothing happened
        return; 

    m_timer--;
    m_timer &= 07777;  // 12 bit only

    if (m_timer == 0)
    {
        if(m_timerflags & 0200)
            m_timerflags |= 010;  // Overflow
        m_timerflags |= 0200;  // 0
        m_timer = m_timerreload & 07777; // Reload it
    }
}

void CMotherboard::SetTimerReload(WORD val)	 // Sets timer reload value
{
    m_timerreload = val & 07777;
	if ((m_timerflags & 1) == 0)
		m_timer=m_timerreload;
}
void CMotherboard::SetTimerState(WORD val) // Sets timer state
{
    if ((val & 1) && ((m_timerflags & 1) == 0))
        m_timer = m_timerreload & 07777;

    m_timerflags &= 0250;  // Clear everything but bits 7,5,3
    m_timerflags |= (val & (~0250));  // Preserve bits 753
}

void CMotherboard::DebugTicks()
{
	m_pCPU->SetInternalTick(0);
	m_pCPU->Execute();
	//m_pFloppyCtl->Periodic();
}


/*
Каждый фрейм равен 1/25 секунды = 40 мс = 20000 тиков, 1 тик = 2 мкс.

* 2150 тиков системного таймера - на каждый 16-й тик;
* 2 сигнала IRQ2, в 0-й и 10000-й тик фрейма
* 160000 тиков ЦП - 6 раз за тик (БК-0010, 3 МГц), либо 8 раз за один тик (БК-0011, 4 МГц)
*/
BOOL CMotherboard::SystemFrame()
{
    int frameProcTicks = (m_Configuration & BK_COPT_BK0011) ? 8 : 6;
    const int audioticks = 20286 / (SOUNDSAMPLERATE / 25);

	int frameTapeTicks = 0, tapeSamplesPerFrame = 0, tapeReadError = 0;
	if (m_TapeReadCallback != NULL)
    {
		tapeSamplesPerFrame = m_nTapeReadSampleRate / 25;
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

        //if (frameticks % 32 == 0)  // FDD tick
        //{
        //    m_pFloppyCtl->Periodic();
        //}

		if (frameticks % audioticks == 0)  // AUDIO tick
			DoSound();

		if (m_TapeReadCallback != NULL && frameticks % frameTapeTicks == 0)
		{
			int tapeSamplesToRead = 0;
			const int readsTotal = 20000 / frameTapeTicks;
			while (true)
			{
				tapeSamplesToRead++;
				tapeReadError += readsTotal;
				if (2 * tapeReadError >= tapeSamplesPerFrame)
				{
					tapeReadError -= tapeSamplesPerFrame;
					break;
				}
			}

			// Reading the tape
			BOOL tapeBit = (*m_TapeReadCallback)(tapeSamplesToRead);
			TapeInput(tapeBit);
		}
    }

    return TRUE;
}

// Key pressed or released
void CMotherboard::KeyboardEvent(BYTE scancode, BOOL okPressed, BOOL okAr2)
{
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


//////////////////////////////////////////////////////////////////////
// Motherboard: memory management

// Read word from memory for debugger
WORD CMotherboard::GetWordView(WORD address, BOOL okHaltMode, BOOL okExec, BOOL* pValid)
{
    WORD offset;
    int addrtype = TranslateAddress(address, okHaltMode, okExec, &offset);

    switch (addrtype)
    {
    case ADDRTYPE_RAM:
        *pValid = TRUE;
        return GetRAMWord(offset);
    case ADDRTYPE_ROM:
        *pValid = TRUE;
        return GetROMWord(offset);
    case ADDRTYPE_IO:
        *pValid = FALSE;  // I/O port, not memory
        return 0;
    case ADDRTYPE_DENY:
        *pValid = TRUE;  // This memory is inaccessible for reading
        return 0;
    }

    ASSERT(FALSE);  // If we are here - then addrtype has invalid value
    return 0;
}

WORD CMotherboard::GetWord(WORD address, BOOL okHaltMode, BOOL okExec)
{
    WORD offset;
    int addrtype = TranslateAddress(address, okHaltMode, okExec, &offset);

    switch (addrtype)
    {
    case ADDRTYPE_RAM:
        return GetRAMWord(offset & 0177776);
    case ADDRTYPE_ROM:
        return GetROMWord(offset);
    case ADDRTYPE_IO:
        //TODO: What to do if okExec == TRUE ?
        return GetPortWord(address);
    case ADDRTYPE_DENY:
        //TODO: Exception processing
        return 0;
    }

    ASSERT(FALSE);  // If we are here - then addrtype has invalid value
    return 0;
}

BYTE CMotherboard::GetByte(WORD address, BOOL okHaltMode)
{
    WORD offset;
    int addrtype = TranslateAddress(address, okHaltMode, FALSE, &offset);

    switch (addrtype)
    {
    case ADDRTYPE_RAM:
        return GetRAMByte(offset);
    case ADDRTYPE_ROM:
        return GetROMByte(offset);
    case ADDRTYPE_IO:
        //TODO: What to do if okExec == TRUE ?
        return GetPortByte(address);
    case ADDRTYPE_DENY:
        //TODO: Exception processing
        return 0;
    }

    ASSERT(FALSE);  // If we are here - then addrtype has invalid value
    return 0;
}

void CMotherboard::SetWord(WORD address, BOOL okHaltMode, WORD word)
{
    WORD offset;

    int addrtype = TranslateAddress(address, okHaltMode, FALSE, &offset);

    switch (addrtype)
    {
    case ADDRTYPE_RAM:
        SetRAMWord(offset & 0177776, word);
        return;
	case ADDRTYPE_ROM:
        // Nothing to do: writing to ROM
        return;
    case ADDRTYPE_IO:
        SetPortWord(address, word);
        return;
    case ADDRTYPE_DENY:
        //TODO: Exception processing
        return;
    }

    ASSERT(FALSE);  // If we are here - then addrtype has invalid value
}

void CMotherboard::SetByte(WORD address, BOOL okHaltMode, BYTE byte)
{
    WORD offset;
    int addrtype = TranslateAddress(address, okHaltMode, FALSE, &offset);

    switch (addrtype)
    {
    case ADDRTYPE_RAM:
        SetRAMByte(offset, byte);
        return;
    case ADDRTYPE_ROM:
        // Nothing to do: writing to ROM
        return;
    case ADDRTYPE_IO:
        SetPortByte(address, byte);
        return;
    case ADDRTYPE_DENY:
        //TODO: Exception processing
        return;
    }

    ASSERT(FALSE);  // If we are here - then addrtype has invalid value
}

int CMotherboard::TranslateAddress(WORD address, BOOL okHaltMode, BOOL okExec, WORD* pOffset)
{
    if (address >= 0177000)  // Port
    {
	    *pOffset = address;
        return ADDRTYPE_IO;
    }

    int block = (address >> 13) & 7;  // RAM/ROM block number
    BOOL blockbit = (m_MemoryMap >> block) & 1;  // 1 - ROM, 0 - RAM
    if (blockbit)  // ROM
    {
        *pOffset = address - 0100000;
        return ADDRTYPE_ROM;
    }
    else  // RAM
    {
        *pOffset = address;
        return ADDRTYPE_RAM;
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
            m_timerflags &= ~010;  // Clear overflow
            m_timerflags &= ~040;  // Clear external int
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
        return 0;  //TODO

    case 0177716:  // System register
    {
        WORD value = m_Port177716;
        m_Port177716 &= ~4;  // Reset bit 2
        return value;
    }

	default: 
		m_pCPU->MemoryError();
		return 0;
    }

    return 0; 
}

// Read word from port for debugger
WORD CMotherboard::GetPortView(WORD address)
{
    switch (address) {
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
        return 0;  //TODO

    case 0177716:  // System register
        return m_Port177716;

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

void CMotherboard::SetPortWord(WORD address, WORD word)
{
    switch (address)
    {
    case 0177700: case 0177702: case 0177704:  // Unknown something
        break;
    
    case 0177706:  // System Timer reload value -- регистр установки таймера
        SetTimerReload(word);
        break;
    case 0177710:  // System Timer Counter -- регистр реверсивного счетчика таймера
        //TODO
        break;
    case 0177712:  // System Timer Manage -- регистр управления таймера
        SetTimerState(word);
        break;

    case 0177714:  // Parallel port register: printer and Covox
        //TODO
        break;

    case 0177716:  // System register - memory management, tape management
        if ((word & 04000) != 0)
        {
            m_Port177716mem = word;
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
        //TODO
        break;
    case 0177132:  // Регистр данных КНГМД
        //TODO
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
//    CopyMemory(pImageRom, m_pROM, 32 * 1024);
//    // RAM planes 0, 1, 2
//    BYTE* pImageRam = pImageRom + 32 * 1024;
//    CopyMemory(pImageRam, m_pRAM[0], 64 * 1024);
//    pImageRam += 64 * 1024;
//    CopyMemory(pImageRam, m_pRAM[1], 64 * 1024);
//    pImageRam += 64 * 1024;
//    CopyMemory(pImageRam, m_pRAM[2], 64 * 1024);
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
//    CopyMemory(m_pROM, pImageRom, 32 * 1024);
//    // RAM planes 0, 1, 2
//    const BYTE* pImageRam = pImageRom + 32 * 1024;
//    CopyMemory(m_pRAM[0], pImageRam, 64 * 1024);
//    pImageRam += 64 * 1024;
//    CopyMemory(m_pRAM[1], pImageRam, 64 * 1024);
//    pImageRam += 64 * 1024;
//    CopyMemory(m_pRAM[2], pImageRam, 64 * 1024);
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


WORD	CMotherboard::GetFloppyState()
{
	//return m_pFloppyCtl->GetState();
    return 0;  //STUB
}
WORD	CMotherboard::GetFloppyData()
{
	//return m_pFloppyCtl->GetData();
    return 0;  //STUB
}
void	CMotherboard::SetFloppyState(WORD val)
{
	//if(val&02000)
	//{
	//	m_currentdrive=(val&3)^3;
	//}
	////m_currentdrive=0;
	//m_pFloppyCtl[m_currentdrive]->SetCommand(val&~3); // it should not get select :)
    
    //STUB
    //m_pFloppyCtl->SetCommand(val);
}
void	CMotherboard::SetFloppyData(WORD val)
{
    //STUB
    //m_pFloppyCtl->WriteData(val);
}


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
		m_nTapeReadSampleRate = 0;
	}
	else
	{
		m_TapeReadCallback = callback;
		m_nTapeReadSampleRate = sampleRate;
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


//////////////////////////////////////////////////////////////////////
