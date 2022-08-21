/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// Processor.cpp
//

#include "stdafx.h"
#include "Processor.h"


// Timings ///////////////////////////////////////////////////////////
// Таблицы таймингов основаны на статье Ю. А. Зальцмана, журнал "Персональный компьютер БК" №1 1995.

const int TIMING_BRANCH =   16;  // 5.4 us - BR, BEQ etc.
const int TIMING_ILLEGAL = 144;
const int TIMING_WAIT   = 1140;  // 380 us - WAIT and RESET
const int TIMING_EMT    =   68;  // 22.8 us - IOT, BPT, EMT, TRAP - 42+5t
const int TIMING_RTI    =   40;  // 13.4 us - RTI and RTT - 24+2t
const int TIMING_RTS    =   32;  // 10.8 us
const int TIMING_NOP    =   12;  // 4 us - NOP and all commands without operands and with register operands
const int TIMING_SOB    =   20;  // 6.8 us
const int TIMING_BR     =   16;  // 5.4 us
const int TIMING_MARK   =   36;

const int TIMING_REGREG =   12;  // Base timing
const int TIMING_A[8]   = { 0, 12, 12, 20, 12, 20, 20, 28 };  // Source
const int TIMING_B[8]   = { 0, 20, 20, 32, 20, 32, 32, 40 };  // Destination
const int TIMING_AB[8]  = { 0, 16, 16, 24, 16, 24, 24, 32 };  // Source and destination are the same
const int TIMING_A2[8]  = { 0, 20, 20, 28, 20, 28, 28, 36 };
const int TIMING_DS[8]  = { 0, 32, 32, 40, 32, 40, 40, 48 };

#define TIMING_A1 TIMING_A
#define TIMING_DJ TIMING_A2

#define TIMING_DST (m_methsrc ? TIMING_AB : TIMING_B)
#define TIMING_CMP (m_methsrc ? TIMING_A1 : TIMING_A2)


//////////////////////////////////////////////////////////////////////


CProcessor::ExecuteMethodRef* CProcessor::m_pExecuteMethodMap = nullptr;

void CProcessor::Init()
{
    ASSERT(m_pExecuteMethodMap == nullptr);
    m_pExecuteMethodMap = static_cast<CProcessor::ExecuteMethodRef*>(::calloc(65536, sizeof(CProcessor::ExecuteMethodRef)));

    // Сначала заполняем таблицу ссылками на метод ExecuteUNKNOWN, выполняющий TRAP 10
    RegisterMethodRef( 0000000, 0177777, &CProcessor::ExecuteUNKNOWN );

    RegisterMethodRef( 0000000, 0000000, &CProcessor::ExecuteHALT );
    RegisterMethodRef( 0000001, 0000001, &CProcessor::ExecuteWAIT );
    RegisterMethodRef( 0000002, 0000002, &CProcessor::ExecuteRTI );
    RegisterMethodRef( 0000003, 0000003, &CProcessor::ExecuteBPT );
    RegisterMethodRef( 0000004, 0000004, &CProcessor::ExecuteIOT );
    RegisterMethodRef( 0000005, 0000005, &CProcessor::ExecuteRESET );
    RegisterMethodRef( 0000006, 0000006, &CProcessor::ExecuteRTT );
    // MFPT            0000007, 0000007
    RegisterMethodRef( 0000010, 0000013, &CProcessor::ExecuteRUN );  // START
    RegisterMethodRef( 0000014, 0000017, &CProcessor::ExecuteSTEP );
    // RESERVED:       0000020, 0000077
    RegisterMethodRef( 0000100, 0000177, &CProcessor::ExecuteJMP );
    RegisterMethodRef( 0000200, 0000207, &CProcessor::ExecuteRTS );  // RTS / RETURN
    // RESERVED:       0000210, 0000227
    // SPL             0000230, 0000237
    RegisterMethodRef( 0000240, 0000240, &CProcessor::ExecuteNOP );
    RegisterMethodRef( 0000241, 0000257, &CProcessor::ExecuteCCC );
    RegisterMethodRef( 0000260, 0000260, &CProcessor::ExecuteNOP );
    RegisterMethodRef( 0000261, 0000277, &CProcessor::ExecuteSCC );
    RegisterMethodRef( 0000300, 0000377, &CProcessor::ExecuteSWAB );
    RegisterMethodRef( 0000400, 0000777, &CProcessor::ExecuteBR );
    RegisterMethodRef( 0001000, 0001377, &CProcessor::ExecuteBNE );
    RegisterMethodRef( 0001400, 0001777, &CProcessor::ExecuteBEQ );
    RegisterMethodRef( 0002000, 0002377, &CProcessor::ExecuteBGE );
    RegisterMethodRef( 0002400, 0002777, &CProcessor::ExecuteBLT );
    RegisterMethodRef( 0003000, 0003377, &CProcessor::ExecuteBGT );
    RegisterMethodRef( 0003400, 0003777, &CProcessor::ExecuteBLE );
    RegisterMethodRef( 0004000, 0004777, &CProcessor::ExecuteJSR );  // JSR / CALL
    RegisterMethodRef( 0005000, 0005077, &CProcessor::ExecuteCLR );
    RegisterMethodRef( 0005100, 0005177, &CProcessor::ExecuteCOM );
    RegisterMethodRef( 0005200, 0005277, &CProcessor::ExecuteINC );
    RegisterMethodRef( 0005300, 0005377, &CProcessor::ExecuteDEC );
    RegisterMethodRef( 0005400, 0005477, &CProcessor::ExecuteNEG );
    RegisterMethodRef( 0005500, 0005577, &CProcessor::ExecuteADC );
    RegisterMethodRef( 0005600, 0005677, &CProcessor::ExecuteSBC );
    RegisterMethodRef( 0005700, 0005777, &CProcessor::ExecuteTST );
    RegisterMethodRef( 0006000, 0006077, &CProcessor::ExecuteROR );
    RegisterMethodRef( 0006100, 0006177, &CProcessor::ExecuteROL );
    RegisterMethodRef( 0006200, 0006277, &CProcessor::ExecuteASR );
    RegisterMethodRef( 0006300, 0006377, &CProcessor::ExecuteASL );
    RegisterMethodRef( 0006400, 0006477, &CProcessor::ExecuteMARK );
    // MFPI            0006500, 0006577
    // MTPI            0006600, 0006677
    RegisterMethodRef( 0006700, 0006777, &CProcessor::ExecuteSXT );
    // RESERVED:       0007000, 0007777
    RegisterMethodRef( 0010000, 0017777, &CProcessor::ExecuteMOV );
    RegisterMethodRef( 0020000, 0027777, &CProcessor::ExecuteCMP );
    RegisterMethodRef( 0030000, 0037777, &CProcessor::ExecuteBIT );
    RegisterMethodRef( 0040000, 0047777, &CProcessor::ExecuteBIC );
    RegisterMethodRef( 0050000, 0057777, &CProcessor::ExecuteBIS );
    RegisterMethodRef( 0060000, 0067777, &CProcessor::ExecuteADD );
    // MUL             0070000, 0070777
    // DIV             0071000, 0071777
    // ASH             0072000, 0072777
    // ASHC            0073000, 0073777
    RegisterMethodRef( 0074000, 0074777, &CProcessor::ExecuteXOR );
    // FADD etc.       0075000, 0075777
    // RESERVED:       0076000, 0076777
    RegisterMethodRef( 0077000, 0077777, &CProcessor::ExecuteSOB );
    RegisterMethodRef( 0100000, 0100377, &CProcessor::ExecuteBPL );
    RegisterMethodRef( 0100400, 0100777, &CProcessor::ExecuteBMI );
    RegisterMethodRef( 0101000, 0101377, &CProcessor::ExecuteBHI );
    RegisterMethodRef( 0101400, 0101777, &CProcessor::ExecuteBLOS );
    RegisterMethodRef( 0102000, 0102377, &CProcessor::ExecuteBVC );
    RegisterMethodRef( 0102400, 0102777, &CProcessor::ExecuteBVS );
    RegisterMethodRef( 0103000, 0103377, &CProcessor::ExecuteBHIS );  // BCC, BHIS
    RegisterMethodRef( 0103400, 0103777, &CProcessor::ExecuteBLO );   // BCS, BLO
    RegisterMethodRef( 0104000, 0104377, &CProcessor::ExecuteEMT );
    RegisterMethodRef( 0104400, 0104777, &CProcessor::ExecuteTRAP );
    RegisterMethodRef( 0105000, 0105077, &CProcessor::ExecuteCLRB );
    RegisterMethodRef( 0105100, 0105177, &CProcessor::ExecuteCOMB );
    RegisterMethodRef( 0105200, 0105277, &CProcessor::ExecuteINCB );
    RegisterMethodRef( 0105300, 0105377, &CProcessor::ExecuteDECB );
    RegisterMethodRef( 0105400, 0105477, &CProcessor::ExecuteNEGB );
    RegisterMethodRef( 0105500, 0105577, &CProcessor::ExecuteADCB );
    RegisterMethodRef( 0105600, 0105677, &CProcessor::ExecuteSBCB );
    RegisterMethodRef( 0105700, 0105777, &CProcessor::ExecuteTSTB );
    RegisterMethodRef( 0106000, 0106077, &CProcessor::ExecuteRORB );
    RegisterMethodRef( 0106100, 0106177, &CProcessor::ExecuteROLB );
    RegisterMethodRef( 0106200, 0106277, &CProcessor::ExecuteASRB );
    RegisterMethodRef( 0106300, 0106377, &CProcessor::ExecuteASLB );
    RegisterMethodRef( 0106400, 0106477, &CProcessor::ExecuteMTPS );
    // MFPD            0106500, 0106577
    // MTPD            0106600, 0106677
    RegisterMethodRef( 0106700, 0106777, &CProcessor::ExecuteMFPS );
    RegisterMethodRef( 0110000, 0117777, &CProcessor::ExecuteMOVB );
    RegisterMethodRef( 0120000, 0127777, &CProcessor::ExecuteCMPB );
    RegisterMethodRef( 0130000, 0137777, &CProcessor::ExecuteBITB );
    RegisterMethodRef( 0140000, 0147777, &CProcessor::ExecuteBICB );
    RegisterMethodRef( 0150000, 0157777, &CProcessor::ExecuteBISB );
    RegisterMethodRef( 0160000, 0167777, &CProcessor::ExecuteSUB );
    // FPP             0170000, 0177777
}

void CProcessor::Done()
{
    ::free(m_pExecuteMethodMap);  m_pExecuteMethodMap = nullptr;
}

void CProcessor::RegisterMethodRef(uint16_t start, uint16_t end, CProcessor::ExecuteMethodRef methodref)
{
    for (size_t opcode = start; opcode <= end; opcode++)
        m_pExecuteMethodMap[opcode] = methodref;
}

//////////////////////////////////////////////////////////////////////


CProcessor::CProcessor(CMotherboard* pBoard)
{
    ASSERT(pBoard != nullptr);
    m_pBoard = pBoard;
    ::memset(m_R, 0, sizeof(m_R));
    m_psw = 0340;
    m_okStopped = true;
    m_internalTick = 0;
    m_waitmode = false;
    m_userspace = false;
    m_stepmode = false;
    m_RPLYrq = m_RSVDrq = m_TBITrq = m_ACLOrq = m_HALTrq = m_RPL2rq = m_IRQ1rq = m_IRQ2rq = false;
    m_BPT_rq = m_IOT_rq = m_EMT_rq = m_TRAPrq = false;
    m_haltpin = false;
    m_instruction = m_instructionpc = 0;
    m_regsrc = m_methsrc = 0;
    m_regdest = m_methdest = 0;
    m_addrsrc = m_addrdest = 0;
    m_virqrq = 0;
    memset(m_virq, 0, sizeof(m_virq));
}

void CProcessor::Start()
{
    m_okStopped = false;

    m_userspace = false;
    m_stepmode = false;
    m_waitmode = false;
    m_RPLYrq = m_RSVDrq = m_TBITrq = m_ACLOrq = m_HALTrq = m_RPL2rq = m_IRQ1rq = m_IRQ2rq = false;
    m_BPT_rq = m_IOT_rq = m_EMT_rq = m_TRAPrq = false;
    m_virqrq = 0;  memset(m_virq, 0, sizeof(m_virq));

    // "Turn On" interrupt processing
    uint16_t pc = m_pBoard->GetSelRegister() & 0177400;
    SetPC(pc);
    SetPSW(0340);
    m_internalTick = 1000000;  // Количество тактов на включение процессора (значение с потолка)
}
void CProcessor::Stop()
{
    m_okStopped = true;

    m_userspace = false;
    m_stepmode = false;
    m_waitmode = false;
    m_psw = 0340;
    m_internalTick = 0;
    m_RPLYrq = m_RSVDrq = m_TBITrq = m_ACLOrq = m_HALTrq = m_RPL2rq = m_IRQ1rq = m_IRQ2rq = false;
    m_BPT_rq = m_IOT_rq = m_EMT_rq = m_TRAPrq = false;
    m_virqrq = 0;  memset(m_virq, 0, sizeof(m_virq));
    m_haltpin = false;
}

void CProcessor::Execute()
{
    if (m_okStopped) return;  // Processor is stopped - nothing to do

    if (m_internalTick > 0)
    {
        m_internalTick--;
        return;
    }
    m_internalTick = TIMING_ILLEGAL;  // ANYTHING UNKNOWN

    m_RPLYrq = false;

    if (!m_waitmode)
    {
        m_instructionpc = m_R[7];  // Store address of the current instruction
        FetchInstruction();  // Read next instruction from memory
        if (!m_RPLYrq)
        {
            TranslateInstruction();  // Execute next instruction
            if (m_internalTick > 0) m_internalTick--;  // Count current tick too
        }
    }

    if (m_stepmode)
        m_stepmode = false;
    else if (m_instruction == PI_RTT && (GetPSW() & PSW_T))
    {
        // Skip interrupt processing for RTT with T bit set
    }
    else  // Processing interrupts
    {
        for (;;)
        {
            m_TBITrq = (m_psw & 020) != 0;  // T-bit

            // Calculate interrupt vector and mode according to priority
            uint16_t intrVector = 0;
            bool currMode = ((m_psw & 0400) != 0);  // Current processor mode: true = HALT mode, false = USER mode
            bool intrMode = false;  // true = HALT mode interrupt, false = USER mode interrupt
            if (m_HALTrq)  // HALT command
            {
                intrVector = 0002;  intrMode = true;
                m_HALTrq = false;
            }
            else if (m_BPT_rq)  // BPT command
            {
                intrVector = 0000014;  intrMode = false;
                m_BPT_rq = false;
            }
            else if (m_IOT_rq)  // IOT command
            {
                intrVector = 0000020;  intrMode = false;
                m_IOT_rq = false;
            }
            else if (m_EMT_rq)  // EMT command
            {
                intrVector = 0000030;  intrMode = false;
                m_EMT_rq = false;
            }
            else if (m_TRAPrq)  // TRAP command
            {
                intrVector = 0000034;  intrMode = false;
                m_TRAPrq = false;
            }
            else if (m_RPLYrq && currMode)  // Зависание в HALT, priority 1
            {
                intrVector = 0004;  intrMode = true;
                m_RPLYrq = false;
            }
            else if (m_RPLYrq && !currMode)  // Зависание в USER, priority 1
            {
                intrVector = 0000004;  intrMode = false;
                m_RPLYrq = false;
            }
            else if (m_RPL2rq)  // Двойное зависание, priority 1
            {
                intrVector = 0174;  intrMode = true;
                m_RPL2rq = false;
            }
            else if (m_RSVDrq)  // Reserved command, priority 2
            {
                intrVector = 000010;  intrMode = false;
                m_RSVDrq = false;
            }
            else if (m_TBITrq && (!m_waitmode))  // T-bit, priority 3
            {
                intrVector = 000014;  intrMode = false;
                m_TBITrq = false;
            }
            else if (m_ACLOrq && (m_psw & 0600) != 0600)  // ACLO, priority 4
            {
                intrVector = 000024;  intrMode = false;
                m_ACLOrq = false;
            }
            else if (m_haltpin && (m_psw & 0400) != 0400)  // HALT signal in USER mode, priority 5
            {
                intrVector = 0170;  intrMode = true;
            }
            else if (m_IRQ2rq && (m_psw & 0200) != 0200)  // EVNT signal, priority 6
            {
                intrVector = 0000100;  intrMode = false;
                m_IRQ2rq = false;
            }
            else if (m_IRQ1rq /*TODO: masking*/)  //TODO: fix priority
            {
                SetWord(0177716, m_pBoard->GetSelRegister() | 010);  // Set bit 3 of SEL1
                MemoryError();  // Instead of this should be writing PSW->0177676, PC->0177674
                m_IRQ1rq = false;
                continue;
            }
            else if (m_virqrq > 0 && (m_psw & 0200) != 0200)  // VIRQ, priority 7
            {
                intrMode = false;
                for (int irq = 0; irq <= 15; irq++)
                {
                    if (m_virq[irq] != 0)
                    {
                        intrVector = m_virq[irq];
                        m_virq[irq] = 0;
                        m_virqrq--;
                        break;
                    }
                }
                if (intrVector == 0) m_virqrq = 0;
            }

            if (intrVector == 0)
                break;  // No more unmasked interrupts

            m_waitmode = false;

            if (intrMode)  // HALT mode interrupt
            {
                MemoryError();
                // PC  -> 0177674
                // PSW -> 0177676
                //intrVector |= 0160000;

                //m_psw |= 0400;

                //SetPC(GetWord(intrVector));
                //m_psw = GetWord(intrVector + 2) & 0777;
            }
            else  // USER mode interrupt
            {
                uint16_t oldpsw = m_psw;
                m_psw &= ~0400;

                // Save PC/PSW to stack
                SetSP(GetSP() - 2);
                SetWord(GetSP(), oldpsw);
                SetSP(GetSP() - 2);
                SetWord(GetSP(), GetPC());

                SetPC(GetWord(intrVector));
                m_psw = GetWord(intrVector + 2) & 0377;
            }
        }  // end while
    }
}

void CProcessor::TickIRQ2()
{
    if (m_okStopped) return;  // Processor is stopped - nothing to do

    m_IRQ2rq = true;
}

void CProcessor::PowerFail()
{
    if (m_okStopped) return;  // Processor is stopped - nothing to do

    m_ACLOrq = true;
}

void CProcessor::InterruptVIRQ(int que, uint16_t interrupt)
{
    if (m_okStopped) return;  // Processor is stopped - nothing to do
    // if (m_virqrq == 1)
    // {
    //  DebugPrintFormat(_T("Lost VIRQ %d %d\r\n"), m_virq, interrupt);
    // }
    m_virqrq += 1;
    m_virq[que] = interrupt;
}
void CProcessor::AssertHALT()
{
    m_haltpin = true;
}
void CProcessor::DeassertHALT()
{
    m_haltpin = false;
}
void CProcessor::MemoryError()
{
    m_RPLYrq = true;
}
void CProcessor::AssertIRQ1()
{
    m_IRQ1rq = true;
}


//////////////////////////////////////////////////////////////////////


void CProcessor::FetchInstruction()
{
    // Считываем очередную инструкцию
    uint16_t pc = GetPC();
    pc = pc & ~1;

    m_instruction = GetWordExec(pc);
    SetPC(GetPC() + 2);
}

void CProcessor::TranslateInstruction()
{
    //if (m_okTrace)
    //    TraceInstruction(this, m_instructionpc);

    // Prepare values to help decode the command
    m_regdest  = GetDigit(m_instruction, 0);
    m_methdest = GetDigit(m_instruction, 1);
    m_regsrc   = GetDigit(m_instruction, 2);
    m_methsrc  = GetDigit(m_instruction, 3);

    // Find command implementation using the command map
    ExecuteMethodRef methodref = m_pExecuteMethodMap[m_instruction];
    (this->*methodref)();  // Call command implementation method
}

void CProcessor::ExecuteUNKNOWN()  // Нет такой инструкции - просто вызывается TRAP 10
{
//    DebugPrintFormat(_T(">>Invalid OPCODE = %06o %06o\r\n"), GetPC()-2, m_instruction);

    m_RSVDrq = true;
}


// Instruction execution /////////////////////////////////////////////

void CProcessor::ExecuteWAIT()  // WAIT - Wait for an interrupt
{
    m_waitmode = true;

    m_internalTick = TIMING_WAIT;
}

void CProcessor::ExecuteRUN()
{
    m_HALTrq = true;

    //SetPC(m_savepc);
    //SetPSW(m_savepsw);
}

void CProcessor::ExecuteHALT()  // HALT - Останов
{
    m_HALTrq = true;
}

void CProcessor::ExecuteSTEP()
{
    m_HALTrq = true;

    m_stepmode = true;
    //SetPC(m_savepc);
    //SetPSW(m_savepsw);
}

void CProcessor::ExecuteRTI()  // RTI - Return from Interrupt
{
    uint16_t word = GetWord(GetSP());
    SetSP( GetSP() + 2 );
    if (m_RPLYrq) return;
    SetPC(word);  // Pop PC

    m_psw &= 0400;  // Store HALT
    uint16_t new_psw = GetWord(GetSP());  // Pop PSW --- saving HALT
    SetSP( GetSP() + 2 );
    if (m_RPLYrq) return;
    if (GetPC() < 0160000)
        SetPSW((new_psw & 0377) | m_psw); // Preserve HALT mode
    else
        SetPSW(new_psw & 0777); //load new mode

    m_internalTick = TIMING_RTI;
}

void CProcessor::ExecuteRTT()  // RTT - Return from Trace Trap
{
    uint16_t word = GetWord(GetSP());
    SetSP( GetSP() + 2 );
    if (m_RPLYrq) return;
    SetPC(word);  // Pop PC

    m_psw &= PSW_HALT;  // Store HALT
    uint16_t new_psw = GetWord(GetSP());  // Pop PSW --- saving HALT
    SetSP( GetSP() + 2 );
    if (m_RPLYrq) return;
    if (GetPC() < 0160000)
        SetPSW((new_psw & 0377) | m_psw);  // Preserve HALT mode
    else
        SetPSW(new_psw & 0777); // Load new mode

    //m_psw |= PSW_T; // set the trap flag ???

    m_internalTick = TIMING_RTI;
}

void CProcessor::ExecuteBPT()  // BPT - Breakpoint
{
    m_BPT_rq = true;
    m_internalTick = TIMING_EMT;
}

void CProcessor::ExecuteIOT()  // IOT - I/O trap
{
    m_IOT_rq = true;
    m_internalTick = TIMING_EMT;
}

void CProcessor::ExecuteRESET()  // Reset input/output devices
{
    m_pBoard->ResetDevices();  // INIT signal

    m_internalTick = TIMING_WAIT;
}

void CProcessor::ExecuteRTS()  // RTS - return from subroutine
{
    SetPC(GetReg(m_regdest));
    uint16_t word = GetWord(GetSP());
    SetSP(GetSP() + 2);
    if (m_RPLYrq) return;
    SetReg(m_regdest, word);
    m_internalTick = TIMING_RTS;
}

void CProcessor::ExecuteNOP()  // NOP - No operation
{
    m_internalTick = TIMING_NOP;
}

void CProcessor::ExecuteCCC()
{
    SetPSW(GetPSW() & ~(static_cast<uint16_t>(m_instruction & 0xff) & 017));

    m_internalTick = TIMING_REGREG;
}

void CProcessor::ExecuteSCC()
{
    SetPSW(GetPSW() | (static_cast<uint16_t>(m_instruction & 0xff) & 017));

    m_internalTick = TIMING_REGREG;
}

void CProcessor::ExecuteJMP()  // JMP - jump: PC = &d (a-mode > 0)
{
    if (m_methdest == 0)  // Неправильный метод адресации
    {
        m_RPLYrq = true;
        m_internalTick = TIMING_EMT;
    }
    else
    {
        uint16_t word = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        SetPC(word);
        m_internalTick = TIMING_DJ[m_methdest];
    }
}

void CProcessor::ExecuteSWAB()
{
    uint16_t ea = 0;
    uint16_t dst;

    if (m_methdest)
    {
        ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetReg(m_regdest);

    dst = ((dst >> 8) & 0377) | (dst << 8);

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);

    if (m_RPLYrq) return;

    SetN((dst & 0200) != 0);
    SetZ(!(dst & 0377));
    SetV(false);
    SetC(false);

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteCLR()
{
    if (m_methdest)
    {
        uint16_t dst_addr = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        SetWord(dst_addr, 0);
        if (m_RPLYrq) return;
    }
    else
        SetReg(m_regdest, 0);

    SetN(false);
    SetZ(true);
    SetV(false);
    SetC(false);

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteCLRB()
{
    if (m_methdest)
    {
        uint16_t dst_addr = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        GetByte(dst_addr);
        if (m_RPLYrq) return;
        SetByte(dst_addr, 0);
        if (m_RPLYrq) return;
    }
    else
        SetReg(m_regdest, (GetReg(m_regdest) & 0177400));

    SetN(false);
    SetZ(true);
    SetV(false);
    SetC(false);

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteCOM()
{
    uint16_t ea = 0;
    uint16_t dst;

    if (m_methdest)
    {
        ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetReg(m_regdest);

    dst = dst ^ 0177777;

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    SetN((dst >> 15) != 0);
    SetZ(!dst);
    SetV(false);
    SetC(true);

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteCOMB()
{
    uint16_t ea = 0;
    uint8_t dst;

    if (m_methdest)
    {
        ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetByte(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetLReg(m_regdest);

    dst = dst ^ 0377;

    if (m_methdest)
        SetByte(ea, dst);
    else
        SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);
    if (m_RPLYrq) return;

    SetN((dst >> 7) != 0);
    SetZ(!dst);
    SetV(false);
    SetC(true);

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteINC()
{
    uint16_t ea = 0;
    uint16_t dst;

    if (m_methdest)
    {
        ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetReg(m_regdest);

    dst = dst + 1;

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    SetN((dst >> 15) != 0);
    SetZ(!dst);
    SetV(dst == 0100000);

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteINCB()
{
    uint16_t ea = 0;
    uint8_t dst;

    if (m_methdest)
    {
        ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetByte(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetLReg(m_regdest);

    dst = dst + 1;

    if (m_methdest)
        SetByte(ea, dst);
    else
        SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);
    if (m_RPLYrq) return;

    SetN((dst >> 7) != 0);
    SetZ(!dst);
    SetV(dst == 0200);

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteDEC()
{
    uint16_t ea = 0;
    uint16_t dst;

    if (m_methdest)
    {
        ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetReg(m_regdest);

    dst = dst - 1;

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    SetN((dst >> 15) != 0);
    SetZ(!dst);
    SetV(dst == 077777);

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteDECB()
{
    uint16_t ea = 0;
    uint8_t dst;

    if (m_methdest)
    {
        ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetByte(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetLReg(m_regdest);

    dst = dst - 1;

    if (m_methdest)
        SetByte(ea, dst);
    else
        SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);
    if (m_RPLYrq) return;

    SetN((dst >> 7) != 0);
    SetZ(!dst);
    SetV(dst == 0177);

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteNEG()
{
    uint16_t ea = 0;
    uint16_t dst;

    if (m_methdest)
    {
        ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetReg(m_regdest);

    dst = 0 - dst;

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    SetN((dst >> 15) != 0);
    SetZ(!dst);
    SetV(dst == 0100000);
    SetC(!GetZ());

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteNEGB()
{
    uint16_t ea = 0;
    uint8_t dst;

    if (m_methdest)
    {
        ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetByte(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetLReg(m_regdest);

    dst = 0 - dst;

    if (m_methdest)
        SetByte(ea, dst);
    else
        SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);
    if (m_RPLYrq) return;

    SetN((dst >> 7) != 0);
    SetZ(!dst);
    SetV(dst == 0200);
    SetC(!GetZ());

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteADC()
{
    uint16_t ea = 0;
    uint16_t dst;

    if (m_methdest)
    {
        ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetReg(m_regdest);

    dst = dst + (GetC() ? 1 : 0);

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    SetN((dst >> 15) != 0);
    SetZ(!dst);
    SetV(GetC() && (dst == 0100000));
    SetC(GetC() && GetZ());

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteADCB()
{
    uint16_t ea = 0;
    uint8_t dst;

    if (m_methdest)
    {
        ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetByte(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetLReg(m_regdest);

    dst = dst + (GetC() ? 1 : 0);

    if (m_methdest)
        SetByte(ea, dst);
    else
        SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);
    if (m_RPLYrq) return;

    SetN((dst >> 7) != 0);
    SetZ(!dst);
    SetV(GetC() && (dst == 0200));
    SetC(GetC() && GetZ());

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteSBC()
{
    uint16_t ea = 0;
    uint16_t dst;

    if (m_methdest)
    {
        ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetReg(m_regdest);

    dst = dst - (GetC() ? 1 : 0);

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    SetN((dst >> 15) != 0);
    SetZ(!dst);
    SetV(GetC() && (dst == 077777));
    SetC(GetC() && (dst == 0177777));

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteSBCB()
{
    uint16_t ea = 0;
    uint8_t dst;

    if (m_methdest)
    {
        ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetByte(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetLReg(m_regdest);

    dst = dst - (GetC() ? 1 : 0);

    if (m_methdest)
        SetByte(ea, dst);
    else
        SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);
    if (m_RPLYrq) return;

    SetN((dst >> 7) != 0);
    SetZ(!dst);
    SetV(GetC() && (dst == 0177));
    SetC(GetC() && (dst == 0377));

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteTST()
{
    uint16_t dst;

    if (m_methdest)
    {
        uint16_t ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetReg(m_regdest);

    SetN((dst >> 15) != 0);
    SetZ(!dst);
    SetV(false);
    SetC(false);

    m_internalTick = TIMING_REGREG + TIMING_A1[m_methdest];
}

void CProcessor::ExecuteTSTB()
{
    uint8_t dst;

    if (m_methdest)
    {
        uint16_t ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetByte(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetLReg(m_regdest);

    SetN((dst >> 7) != 0);
    SetZ(!dst);
    SetV(false);
    SetC(false);

    m_internalTick = TIMING_REGREG + TIMING_A1[m_methdest];
}

void CProcessor::ExecuteROR()
{
    uint16_t ea = 0;
    uint16_t src;

    if (m_methdest)
    {
        ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        src = GetReg(m_regdest);

    uint16_t dst = (src >> 1) | (GetC() ? 0100000 : 0);

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    SetN((dst >> 15) != 0);
    SetZ(!dst);
    SetC(src & 1);
    SetV(GetN() != GetC());

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteRORB()
{
    uint16_t ea = 0;
    uint8_t src;

    if (m_methdest)
    {
        ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src = GetByte(ea);
        if (m_RPLYrq) return;
    }
    else
        src = GetLReg(m_regdest);

    uint8_t dst = (src >> 1) | (GetC() ? 0200 : 0);

    if (m_methdest)
        SetByte(ea, dst);
    else
        SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);
    if (m_RPLYrq) return;

    SetN((dst >> 7) != 0);
    SetZ(!dst);
    SetC(src & 1);
    SetV(GetN() != GetC());

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteROL()
{
    uint16_t ea = 0;
    uint16_t src;

    if (m_methdest)
    {
        ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        src = GetReg(m_regdest);

    uint16_t dst = (src << 1) | (GetC() ? 1 : 0);

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    SetN((dst >> 15) != 0);
    SetZ(!dst);
    SetC((src >> 15) != 0);
    SetV(GetN() != GetC());

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteROLB()
{
    uint16_t ea = 0;
    uint8_t src;

    if (m_methdest)
    {
        ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src = GetByte(ea);
        if (m_RPLYrq) return;
    }
    else
        src = GetLReg(m_regdest);

    uint8_t dst = (src << 1) | (GetC() ? 1 : 0);

    if (m_methdest)
        SetByte(ea, dst);
    else
        SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);
    if (m_RPLYrq) return;

    SetN((dst >> 7) != 0);
    SetZ(!dst);
    SetC((src >> 7) != 0);
    SetV(GetN() != GetC());

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteASR()
{
    uint16_t ea = 0;
    uint16_t src;

    if (m_methdest)
    {
        ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        src = GetReg(m_regdest);

    uint16_t dst = (src >> 1) | (src & 0100000);

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    SetN((dst >> 15) != 0);
    SetZ(!dst);
    SetC(src & 1);
    SetV(GetN() != GetC());

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteASRB()
{
    uint16_t ea = 0;
    uint8_t src;

    if (m_methdest)
    {
        ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src = GetByte(ea);
        if (m_RPLYrq) return;
    }
    else
        src = GetLReg(m_regdest);

    uint8_t dst = (src >> 1) | (src & 0200);

    if (m_methdest)
        SetByte(ea, dst);
    else
        SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);
    if (m_RPLYrq) return;

    SetN((dst >> 7) != 0);
    SetZ(!dst);
    SetC(src & 1);
    SetV(GetN() != GetC());

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteASL()
{
    uint16_t ea = 0;
    uint16_t src;

    if (m_methdest)
    {
        ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        src = GetReg(m_regdest);

    uint16_t dst = src << 1;

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    SetN((dst >> 15) != 0);
    SetZ(!dst);
    SetC((src >> 15) != 0);
    SetV(GetN() != GetC());

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteASLB()
{
    uint16_t ea = 0;
    uint8_t src;

    if (m_methdest)
    {
        ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src = GetByte(ea);
        if (m_RPLYrq) return;
    }
    else
        src = GetLReg(m_regdest);

    uint8_t dst = (src << 1) & 0377;

    if (m_methdest)
        SetByte(ea, dst);
    else
        SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);
    if (m_RPLYrq) return;

    SetN((dst >> 7) != 0);
    SetZ(!dst);
    SetC((src >> 7) != 0);
    SetV(GetN() != GetC());

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteSXT()  // SXT - sign-extend
{
    if (m_methdest)
    {
        uint16_t ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        SetWord(ea, GetN() ? 0177777 : 0);
        if (m_RPLYrq) return;
    }
    else
        SetReg(m_regdest, GetN() ? 0177777 : 0); //sign extend

    SetZ(!GetN());
    SetV(false);

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteMTPS()  // MTPS - move to PS
{
    uint8_t dst;
    if (m_methdest)
    {
        uint16_t ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetByte(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetLReg(m_regdest);

    if (GetPSW() & 0400) //in halt?
    {
        //allow everything
        SetPSW((GetPSW() & 0400) | dst);
    }
    else
    {
        SetPSW((GetPSW() & 0420) | (dst & 0357));  // preserve T
    }

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteMFPS()  // MFPS - move from PS
{
    uint8_t psw = GetPSW() & 0377;

    if (m_methdest)
    {
        uint16_t ea = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        GetByte(ea);
        if (m_RPLYrq) return;
        SetByte(ea, psw);
        if (m_RPLYrq) return;
    }
    else
        SetReg(m_regdest, (uint16_t)(signed short)(char)psw); //sign extend

    SetN((psw & 0200) != 0);
    SetZ(psw == 0);
    SetV(false);

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteBR()
{
    SetPC(GetPC() + ((short)(char)(uint8_t)(m_instruction & 0xff)) * 2 );

    m_internalTick = TIMING_BR;
}

void CProcessor::ExecuteBNE()
{
    if (! GetZ())
        SetReg(7, GetPC() + ((short)(char)(m_instruction & 0xff)) * 2 );

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBEQ()
{
    if (GetZ())
        SetReg(7, GetPC() + ((short)(char)(m_instruction & 0xff)) * 2 );

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBGE()
{
    if (GetN() == GetV())
        SetReg(7, GetPC() + ((short)(char)(m_instruction & 0xff)) * 2 );

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBLT()
{
    if (GetN() != GetV())
        SetReg(7, GetPC() + ((short)(char)(m_instruction & 0xff)) * 2 );

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBGT()
{
    if (! ((GetN() != GetV()) || GetZ()))
        SetReg(7, GetPC() + ((short)(char)(m_instruction & 0xff)) * 2 );

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBLE()
{
    if ((GetN() != GetV()) || GetZ())
        SetReg(7, GetPC() + ((short)(char)(m_instruction & 0xff)) * 2 );

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBPL()
{
    if (! GetN())
        SetReg(7, GetPC() + ((short)(char)(m_instruction & 0xff)) * 2 );

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBMI()
{
    if (GetN())
        SetReg(7, GetPC() + ((short)(char)(m_instruction & 0xff)) * 2 );

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBHI()
{
    if (! (GetZ() || GetC()))
        SetReg(7, GetPC() + ((short)(char)(m_instruction & 0xff)) * 2 );

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBLOS()
{
    if (GetZ() || GetC())
        SetReg(7, GetPC() + ((short)(char)(m_instruction & 0xff)) * 2 );

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBVC()
{
    if (! GetV())
        SetReg(7, GetPC() + ((short)(char)(m_instruction & 0xff)) * 2 );

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBVS()
{
    if (GetV())
        SetReg(7, GetPC() + ((short)(char)(m_instruction & 0xff)) * 2 );

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBHIS()
{
    if (! GetC())
        SetReg(7, GetPC() + ((short)(char)(m_instruction & 0xff)) * 2 );

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBLO()
{
    if (GetC())
        SetReg(7, GetPC() + ((short)(char)(m_instruction & 0xff)) * 2 );

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteXOR()
{
    uint16_t ea = 0;
    uint16_t dst;

    if (m_methdest)
    {
        ea = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        dst = GetWord(ea);
        if (m_RPLYrq) return;
    }
    else
        dst = GetReg(m_regdest);

    dst = dst ^ GetReg(m_regsrc);

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    SetN((dst >> 15) != 0);
    SetZ(!dst);
    SetV(false);

    m_internalTick = TIMING_REGREG + TIMING_A2[m_methdest];
}

void CProcessor::ExecuteSOB()  // SOB - subtract one: R = R - 1; if R != 0 : PC = PC - 2*nn
{
    uint16_t dst = GetReg(m_regsrc);

    --dst;
    SetReg(m_regsrc, dst);

    if (dst)
        SetPC(GetPC() - (m_instruction & static_cast<uint16_t>(077)) * 2 );

    m_internalTick = TIMING_SOB;
}

void CProcessor::ExecuteMOV()  // MOV - move
{
    uint16_t src_addr, dst_addr;
    uint16_t dst;

    if (m_methsrc)
    {
        src_addr = GetWordAddr(m_methsrc, m_regsrc);
        if (m_RPLYrq) return;
        dst = GetWord(src_addr);
        if (m_RPLYrq) return;
    }
    else
        dst = GetReg(m_regsrc);

    if (m_methdest)
    {
        dst_addr = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        SetWord(dst_addr, dst);
        if (m_RPLYrq) return;
    }
    else
        SetReg(m_regdest, dst);

    SetN((dst >> 15) != 0);
    SetZ(!dst);
    SetV(false);

    m_internalTick = TIMING_REGREG + TIMING_A[m_methsrc] + TIMING_DST[m_methdest];
}

void CProcessor::ExecuteMOVB()  // MOVB - move byte
{
    uint16_t src_addr, dst_addr;
    uint8_t dst;

    if (m_methsrc)
    {
        src_addr = GetByteAddr(m_methsrc, m_regsrc);
        if (m_RPLYrq) return;
        dst = GetByte(src_addr);
        if (m_RPLYrq) return;
    }
    else
        dst = GetLReg(m_regsrc);

    if (m_methdest)
    {
        dst_addr = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        GetByte(dst_addr);
        if (m_RPLYrq) return;
        SetByte(dst_addr, dst);
        if (m_RPLYrq) return;
    }
    else
        SetReg(m_regdest, (dst & 0200) ? (0177400 | dst) : dst);

    SetN((dst >> 7) != 0);
    SetZ(!dst);
    SetV(false);

    m_internalTick = TIMING_REGREG + TIMING_A[m_methsrc] + TIMING_DST[m_methdest];
}

void CProcessor::ExecuteCMP()  // CMP - compare
{
    uint16_t src_addr, dst_addr;
    uint16_t src, src2;

    if (m_methsrc)
    {
        src_addr = GetWordAddr(m_methsrc, m_regsrc);
        if (m_RPLYrq) return;
        src = GetWord(src_addr);
        if (m_RPLYrq) return;
    }
    else
        src = GetReg(m_regsrc);

    if (m_methdest)
    {
        dst_addr = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src2 = GetWord(dst_addr);
        if (m_RPLYrq) return;
    }
    else
        src2 = GetReg(m_regdest);

    uint16_t dst = src - src2;

    SetN(CheckForNegative(dst));
    SetZ(CheckForZero(dst));
    SetV(CheckSubForOverflow(src, src2));
    SetC(CheckSubForCarry(src, src2));

    m_internalTick = TIMING_REGREG + TIMING_A1[m_methsrc] + TIMING_CMP[m_methdest];
}

void CProcessor::ExecuteCMPB()  // CMPB - compare byte
{
    uint16_t src_addr, dst_addr;
    uint8_t src, src2;

    if (m_methsrc)
    {
        src_addr = GetByteAddr(m_methsrc, m_regsrc);
        if (m_RPLYrq) return;
        src = GetByte(src_addr);
        if (m_RPLYrq) return;
    }
    else
        src = GetLReg(m_regsrc);

    if (m_methdest)
    {
        dst_addr = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src2 = GetByte(dst_addr);
        if (m_RPLYrq) return;
    }
    else
        src2 = GetLReg(m_regdest);

    uint8_t dst = src - src2;

    SetN(CheckForNegative(dst));
    SetZ(CheckForZero(dst));
    SetV(CheckSubForOverflow(src, src2));
    SetC(CheckSubForCarry(src, src2));

    m_internalTick = TIMING_REGREG + TIMING_A1[m_methsrc] + TIMING_CMP[m_methdest];
}

void CProcessor::ExecuteBIT()  // BIT - bit test
{
    uint16_t src_addr, dst_addr;
    uint16_t src, src2;

    if (m_methsrc)
    {
        src_addr = GetWordAddr(m_methsrc, m_regsrc);
        if (m_RPLYrq) return;
        src = GetWord(src_addr);
        if (m_RPLYrq) return;
    }
    else
        src = GetReg(m_regsrc);

    if (m_methdest)
    {
        dst_addr = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src2 = GetWord(dst_addr);
        if (m_RPLYrq) return;
    }
    else
        src2 = GetReg(m_regdest);

    uint16_t dst = src2 & src;

    SetN((dst >> 15) != 0);
    SetZ(!dst);
    SetV(false);

    m_internalTick = TIMING_REGREG + TIMING_A1[m_methsrc] + TIMING_CMP[m_methdest];
}

void CProcessor::ExecuteBITB()  // BITB - bit test on byte
{
    uint16_t src_addr, dst_addr;
    uint8_t src, src2;

    if (m_methsrc)
    {
        src_addr = GetByteAddr(m_methsrc, m_regsrc);
        if (m_RPLYrq) return;
        src = GetByte(src_addr);
        if (m_RPLYrq) return;
    }
    else
        src = GetLReg(m_regsrc);

    if (m_methdest)
    {
        dst_addr = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src2 = GetByte(dst_addr);
        if (m_RPLYrq) return;
    }
    else
        src2 = GetLReg(m_regdest);

    uint8_t dst = src2 & src;

    SetN((dst >> 7) != 0);
    SetZ(!dst);
    SetV(false);

    m_internalTick = TIMING_REGREG + TIMING_A1[m_methsrc] + TIMING_CMP[m_methdest];
}

void CProcessor::ExecuteBIC()  // BIC - bit clear
{
    uint16_t src_addr, dst_addr = 0;
    uint16_t src, src2;

    if (m_methsrc)
    {
        src_addr = GetWordAddr(m_methsrc, m_regsrc);
        if (m_RPLYrq) return;
        src = GetWord(src_addr);
        if (m_RPLYrq) return;
    }
    else
        src = GetReg(m_regsrc);

    if (m_methdest)
    {
        dst_addr = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src2 = GetWord(dst_addr);
        if (m_RPLYrq) return;
    }
    else
        src2 = GetReg(m_regdest);

    uint16_t dst = src2 & (~src);

    if (m_methdest)
        SetWord(dst_addr, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    SetN((dst >> 15) != 0);
    SetZ(!dst);
    SetV(false);

    m_internalTick = TIMING_REGREG + TIMING_A[m_methsrc] + TIMING_DST[m_methdest];
}

void CProcessor::ExecuteBICB()  // BICB - bit clear
{
    uint16_t src_addr, dst_addr = 0;
    uint8_t src, src2;

    if (m_methsrc)
    {
        src_addr = GetByteAddr(m_methsrc, m_regsrc);
        if (m_RPLYrq) return;
        src = GetByte(src_addr);
        if (m_RPLYrq) return;
    }
    else
        src = GetLReg(m_regsrc);

    if (m_methdest)
    {
        dst_addr = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src2 = GetByte(dst_addr);
        if (m_RPLYrq) return;
    }
    else
        src2 = GetLReg(m_regdest);

    uint8_t dst = src2 & (~src);

    if (m_methdest)
        SetByte(dst_addr, dst);
    else
        SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);
    if (m_RPLYrq) return;

    SetN((dst >> 7) != 0);
    SetZ(!dst);
    SetV(false);

    m_internalTick = TIMING_REGREG + TIMING_A[m_methsrc] + TIMING_DST[m_methdest];
}

void CProcessor::ExecuteBIS()  // BIS - bit set
{
    uint16_t src_addr, dst_addr = 0;
    uint16_t src, src2;

    if (m_methsrc)
    {
        src_addr = GetWordAddr(m_methsrc, m_regsrc);
        if (m_RPLYrq) return;
        src = GetWord(src_addr);
        if (m_RPLYrq) return;
    }
    else
        src = GetReg(m_regsrc);

    if (m_methdest)
    {
        dst_addr = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src2 = GetWord(dst_addr);
        if (m_RPLYrq) return;
    }
    else
        src2 = GetReg(m_regdest);

    uint16_t dst = src2 | src;

    if (m_methdest)
        SetWord(dst_addr, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    SetN((dst >> 15) != 0);
    SetZ(!dst);
    SetV(false);

    m_internalTick = TIMING_REGREG + TIMING_A[m_methsrc] + TIMING_DST[m_methdest];
}

void CProcessor::ExecuteBISB()  // BISB - bit set on byte
{
    uint16_t src_addr, dst_addr = 0;
    uint8_t src, src2;

    if (m_methsrc)
    {
        src_addr = GetByteAddr(m_methsrc, m_regsrc);
        if (m_RPLYrq) return;
        src = GetByte(src_addr);
        if (m_RPLYrq) return;
    }
    else
        src = GetLReg(m_regsrc);

    if (m_methdest)
    {
        dst_addr = GetByteAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src2 = GetByte(dst_addr);
        if (m_RPLYrq) return;
    }
    else
        src2 = GetLReg(m_regdest);

    uint8_t dst = src2 | src;

    if (m_methdest)
        SetByte(dst_addr, dst);
    else
        SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);
    if (m_RPLYrq) return;

    SetN((dst >> 7) != 0);
    SetZ(!dst);
    SetV(false);

    m_internalTick = TIMING_REGREG + TIMING_A[m_methsrc] + TIMING_DST[m_methdest];
}

void CProcessor::ExecuteADD()
{
    uint16_t src_addr, dst_addr = 0;
    uint16_t src, src2;

    if (m_methsrc)
    {
        src_addr = GetWordAddr(m_methsrc, m_regsrc);
        if (m_RPLYrq) return;
        src = GetWord(src_addr);
        if (m_RPLYrq) return;
    }
    else
        src = GetReg(m_regsrc);

    if (m_methdest)
    {
        dst_addr = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src2 = GetWord(dst_addr);
        if (m_RPLYrq) return;
    }
    else
        src2 = GetReg(m_regdest);

    signed short dst = src2 + src;

    if (m_methdest)
        SetWord(dst_addr, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    SetN(CheckForNegative(static_cast<uint16_t>(src2 + src)));
    SetZ(CheckForZero(static_cast<uint16_t>(src2 + src)));
    SetV(CheckAddForOverflow(src2, src));
    SetC(CheckAddForCarry(src2, src));

    m_internalTick = TIMING_REGREG + TIMING_A[m_methsrc] + TIMING_DST[m_methdest];
}

void CProcessor::ExecuteSUB()
{
    uint16_t src_addr, dst_addr = 0;
    uint16_t src, src2;

    if (m_methsrc)
    {
        src_addr = GetWordAddr(m_methsrc, m_regsrc);
        if (m_RPLYrq) return;
        src = GetWord(src_addr);
        if (m_RPLYrq) return;
    }
    else
        src = GetReg(m_regsrc);

    if (m_methdest)
    {
        dst_addr = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;
        src2 = GetWord(dst_addr);
        if (m_RPLYrq) return;
    }
    else
        src2 = GetReg(m_regdest);

    uint16_t dst = src2 - src;

    if (m_methdest)
        SetWord(dst_addr, dst);
    else
        SetReg(m_regdest, dst);
    if (m_RPLYrq) return;

    SetN(CheckForNegative(static_cast<uint16_t>(src2 - src)));
    SetZ(CheckForZero(static_cast<uint16_t>(src2 - src)));
    SetV(CheckSubForOverflow(src2, src));
    SetC(CheckSubForCarry(src2, src));

    m_internalTick = TIMING_REGREG + TIMING_A[m_methsrc] + TIMING_DST[m_methdest];
}

void CProcessor::ExecuteEMT()  // EMT - emulator trap
{
    m_EMT_rq = true;
    m_internalTick = TIMING_EMT;
}

void CProcessor::ExecuteTRAP()
{
    m_TRAPrq = true;
    m_internalTick = TIMING_EMT;
}

void CProcessor::ExecuteJSR()  // JSR - jump subroutine: *--SP = R; R = PC; PC = &d (a-mode > 0)
{
    if (m_methdest == 0)
    {
        // Неправильный метод адресации
        m_RPLYrq = true;
        m_internalTick = TIMING_EMT;
    }
    else
    {
        uint16_t dst = GetWordAddr(m_methdest, m_regdest);
        if (m_RPLYrq) return;

        SetSP( GetSP() - 2 );
        SetWord( GetSP(), GetReg(m_regsrc) );
        SetReg(m_regsrc, GetPC());
        SetPC(dst);
        if (m_RPLYrq) return;

        m_internalTick = TIMING_DS[m_methdest];
    }
}

void CProcessor::ExecuteMARK()  // MARK
{
    SetSP( GetPC() + (m_instruction & 0x003F) * 2 );
    SetPC( GetReg(5) );
    SetReg(5, GetWord( GetSP() ));
    SetSP( GetSP() + 2 );
    if (m_RPLYrq) return;

    m_internalTick = TIMING_MARK;
}


//////////////////////////////////////////////////////////////////////
//
// CPU image format (32 bytes):
//   2 bytes        PSW
//   2*8 bytes      Registers R0..R7
//   2*2 bytes      Saved PC and PSW
//   2 bytes        Stopped flag: !0 - stopped, 0 - not stopped

void CProcessor::SaveToImage(uint8_t* pImage)
{
    uint16_t* pwImage = reinterpret_cast<uint16_t*>(pImage);
    // PSW
    *pwImage++ = m_psw;
    // Registers R0..R7
    ::memcpy(pwImage, m_R, 2 * 8);
    pwImage += 2 * 8;
    // Saved PC and PSW
    *pwImage++ = 0;  //m_savepc;
    *pwImage++ = 0;  //m_savepsw;
    // Stopped flag
    *pwImage++ = (m_okStopped ? 1 : 0);
}

void CProcessor::LoadFromImage(const uint8_t* pImage)
{
    const uint16_t* pwImage = reinterpret_cast<const uint16_t*>(pImage);
    // PSW
    m_psw = *pwImage++;
    // Registers R0..R7
    ::memcpy(m_R, pwImage, 2 * 8);
    // Saved PC and PSW - skip
    pwImage++;
    pwImage++;
    // Stopped flag
    m_okStopped = (*pwImage++ != 0);
}

uint16_t CProcessor::GetWordAddr(uint8_t meth, uint8_t reg)
{
    switch (meth)
    {
    case 1:   //(R)
        return GetReg(reg);
    case 2:   //(R)+
        {
            uint16_t addr = GetReg(reg);
            SetReg(reg, addr + 2);
            return addr;
        }
    case 3:  //@(R)+
        {
            uint16_t addr = GetReg(reg);
            SetReg(reg, addr + 2);
            return GetWord(addr);
        }
    case 4: //-(R)
        SetReg(reg, GetReg(reg) - 2);
        return GetReg(reg);
    case 5: //@-(R)
        {
            SetReg(reg, GetReg(reg) - 2);
            uint16_t addr = GetReg(reg);
            return GetWord(addr);
        }
    case 6: //d(R)
        {
            uint16_t addr = GetWord(GetPC());
            SetPC(GetPC() + 2);
            return GetReg(reg) + addr;
        }
    case 7: //@d(r)
        {
            uint16_t addr = GetWord(GetPC());
            SetPC(GetPC() + 2);
            addr = GetReg(reg) + addr;
            if (!m_RPLYrq)
                return GetWord(addr);
            return addr;
        }
    }
    return 0;
}

uint16_t CProcessor::GetByteAddr(uint8_t meth, uint8_t reg)
{
    uint16_t addr = 0;

    switch (meth)
    {
    case 1:
        addr = GetReg(reg);
        break;
    case 2:
        addr = GetReg(reg);
        SetReg(reg, addr + (reg < 6 ? 1 : 2));
        break;
    case 3:
        addr = GetReg(reg);
        SetReg(reg, addr + 2);
        addr = GetWord(addr);
        break;
    case 4:
        SetReg(reg, GetReg(reg) - (reg < 6 ? 1 : 2));
        addr = GetReg(reg);
        break;
    case 5:
        SetReg(reg, GetReg(reg) - 2);
        addr = GetReg(reg);
        addr = GetWord(addr);
        break;
    case 6: //d(R)
        addr = GetWord(GetPC());
        SetPC(GetPC() + 2);
        addr = GetReg(reg) + addr;
        break;
    case 7: //@d(r)
        addr = GetWord(GetPC());
        SetPC(GetPC() + 2);
        addr = GetReg(reg) + addr;
        if (!m_RPLYrq) addr = GetWord(addr);
        break;
    }

    return addr;
}


//////////////////////////////////////////////////////////////////////
