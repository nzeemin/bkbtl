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


CProcessor::ExecuteMethodRef* CProcessor::m_pExecuteMethodMap = NULL;

void CProcessor::Init()
{
    ASSERT(m_pExecuteMethodMap == NULL);
    m_pExecuteMethodMap = (CProcessor::ExecuteMethodRef*) ::malloc(sizeof(CProcessor::ExecuteMethodRef) * 65536);

    // Сначала заполняем таблицу ссылками на метод ExecuteUNKNOWN, выполняющий TRAP 10
    RegisterMethodRef( 0000000, 0177777, &CProcessor::ExecuteUNKNOWN );

    RegisterMethodRef( 0000000, 0000000, &CProcessor::ExecuteHALT );
    RegisterMethodRef( 0000001, 0000001, &CProcessor::ExecuteWAIT );
    RegisterMethodRef( 0000002, 0000002, &CProcessor::ExecuteRTI );
    RegisterMethodRef( 0000003, 0000003, &CProcessor::ExecuteBPT );
    RegisterMethodRef( 0000004, 0000004, &CProcessor::ExecuteIOT );
    RegisterMethodRef( 0000005, 0000005, &CProcessor::ExecuteRESET );
    RegisterMethodRef( 0000006, 0000006, &CProcessor::ExecuteRTT );

    RegisterMethodRef( 0000010, 0000013, &CProcessor::ExecuteRUN );
    RegisterMethodRef( 0000014, 0000017, &CProcessor::ExecuteSTEP );
    RegisterMethodRef( 0000030, 0000030, &CProcessor::Execute000030 );

    RegisterMethodRef( 0000100, 0000177, &CProcessor::ExecuteJMP );
    RegisterMethodRef( 0000200, 0000207, &CProcessor::ExecuteRTS );  // RTS / RETURN

    RegisterMethodRef( 0000240, 0000240, &CProcessor::ExecuteNOP );
    RegisterMethodRef( 0000241, 0000241, &CProcessor::ExecuteCLC );
    RegisterMethodRef( 0000242, 0000242, &CProcessor::ExecuteCLV );
    RegisterMethodRef( 0000243, 0000243, &CProcessor::ExecuteCLVC );
    RegisterMethodRef( 0000244, 0000244, &CProcessor::ExecuteCLZ );
    RegisterMethodRef( 0000245, 0000245, &CProcessor::ExecuteCLZC );
    RegisterMethodRef( 0000246, 0000246, &CProcessor::ExecuteCLZV );
    RegisterMethodRef( 0000247, 0000247, &CProcessor::ExecuteCLZVC );
    RegisterMethodRef( 0000250, 0000250, &CProcessor::ExecuteCLN );
    RegisterMethodRef( 0000251, 0000251, &CProcessor::ExecuteCLNC );
    RegisterMethodRef( 0000252, 0000252, &CProcessor::ExecuteCLNV );
    RegisterMethodRef( 0000253, 0000253, &CProcessor::ExecuteCLNVC );
    RegisterMethodRef( 0000254, 0000254, &CProcessor::ExecuteCLNZ );
    RegisterMethodRef( 0000255, 0000255, &CProcessor::ExecuteCLNZC );
    RegisterMethodRef( 0000256, 0000256, &CProcessor::ExecuteCLNZV );
    RegisterMethodRef( 0000257, 0000257, &CProcessor::ExecuteCCC );

    RegisterMethodRef( 0000260, 0000260, &CProcessor::ExecuteNOP );
    RegisterMethodRef( 0000261, 0000261, &CProcessor::ExecuteSEC );
    RegisterMethodRef( 0000262, 0000262, &CProcessor::ExecuteSEV );
    RegisterMethodRef( 0000263, 0000263, &CProcessor::ExecuteSEVC );
    RegisterMethodRef( 0000264, 0000264, &CProcessor::ExecuteSEZ );
    RegisterMethodRef( 0000265, 0000265, &CProcessor::ExecuteSEZC );
    RegisterMethodRef( 0000266, 0000266, &CProcessor::ExecuteSEZV );
    RegisterMethodRef( 0000267, 0000267, &CProcessor::ExecuteSEZVC );
    RegisterMethodRef( 0000270, 0000270, &CProcessor::ExecuteSEN );
    RegisterMethodRef( 0000271, 0000271, &CProcessor::ExecuteSENC );
    RegisterMethodRef( 0000272, 0000272, &CProcessor::ExecuteSENV );
    RegisterMethodRef( 0000273, 0000273, &CProcessor::ExecuteSENVC );
    RegisterMethodRef( 0000274, 0000274, &CProcessor::ExecuteSENZ );
    RegisterMethodRef( 0000275, 0000275, &CProcessor::ExecuteSENZC );
    RegisterMethodRef( 0000276, 0000276, &CProcessor::ExecuteSENZV );
    RegisterMethodRef( 0000277, 0000277, &CProcessor::ExecuteSCC );

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
    RegisterMethodRef( 0006700, 0006777, &CProcessor::ExecuteSXT );

    RegisterMethodRef( 0010000, 0017777, &CProcessor::ExecuteMOV );
    RegisterMethodRef( 0020000, 0027777, &CProcessor::ExecuteCMP );
    RegisterMethodRef( 0030000, 0037777, &CProcessor::ExecuteBIT );
    RegisterMethodRef( 0040000, 0047777, &CProcessor::ExecuteBIC );
    RegisterMethodRef( 0050000, 0057777, &CProcessor::ExecuteBIS );
    RegisterMethodRef( 0060000, 0067777, &CProcessor::ExecuteADD );

    RegisterMethodRef( 0074000, 0074777, &CProcessor::ExecuteXOR );
    RegisterMethodRef( 0077000, 0077777, &CProcessor::ExecuteSOB );

    RegisterMethodRef( 0100000, 0100377, &CProcessor::ExecuteBPL );
    RegisterMethodRef( 0100400, 0100777, &CProcessor::ExecuteBMI );
    RegisterMethodRef( 0101000, 0101377, &CProcessor::ExecuteBHI );
    RegisterMethodRef( 0101400, 0101777, &CProcessor::ExecuteBLOS );
    RegisterMethodRef( 0102000, 0102377, &CProcessor::ExecuteBVC );
    RegisterMethodRef( 0102400, 0102777, &CProcessor::ExecuteBVS );
    RegisterMethodRef( 0103000, 0103377, &CProcessor::ExecuteBHIS );  // BCC
    RegisterMethodRef( 0103400, 0103777, &CProcessor::ExecuteBLO );   // BCS
    
    RegisterMethodRef( 0104000, 0104377, &CProcessor::ExecuteEMT );
    RegisterMethodRef( 0104400, 0104777, &CProcessor::ExecuteTRAP );
    
    RegisterMethodRef( 0105000, 0105077, &CProcessor::ExecuteCLR );  // CLRB
    RegisterMethodRef( 0105100, 0105177, &CProcessor::ExecuteCOM );  // COMB
    RegisterMethodRef( 0105200, 0105277, &CProcessor::ExecuteINC );  // INCB
    RegisterMethodRef( 0105300, 0105377, &CProcessor::ExecuteDEC );  // DECB
    RegisterMethodRef( 0105400, 0105477, &CProcessor::ExecuteNEG );  // NEGB
    RegisterMethodRef( 0105500, 0105577, &CProcessor::ExecuteADC );  // ADCB
    RegisterMethodRef( 0105600, 0105677, &CProcessor::ExecuteSBC );  // SBCB
    RegisterMethodRef( 0105700, 0105777, &CProcessor::ExecuteTST );  // TSTB
    RegisterMethodRef( 0106000, 0106077, &CProcessor::ExecuteROR );  // RORB
    RegisterMethodRef( 0106100, 0106177, &CProcessor::ExecuteROL );  // ROLB
    RegisterMethodRef( 0106200, 0106277, &CProcessor::ExecuteASR );  // ASRB
    RegisterMethodRef( 0106300, 0106377, &CProcessor::ExecuteASL );  // ASLB
    
    RegisterMethodRef( 0106400, 0106477, &CProcessor::ExecuteMTPS );
    RegisterMethodRef( 0106700, 0106777, &CProcessor::ExecuteMFPS );

    RegisterMethodRef( 0110000, 0117777, &CProcessor::ExecuteMOV );  // MOVB
    RegisterMethodRef( 0120000, 0127777, &CProcessor::ExecuteCMP );  // CMPB
    RegisterMethodRef( 0130000, 0137777, &CProcessor::ExecuteBIT );  // BITB
    RegisterMethodRef( 0140000, 0147777, &CProcessor::ExecuteBIC );  // BICB
    RegisterMethodRef( 0150000, 0157777, &CProcessor::ExecuteBIS );  // BISB
    RegisterMethodRef( 0160000, 0167777, &CProcessor::ExecuteSUB );
}

void CProcessor::Done()
{
    ::free(m_pExecuteMethodMap);  m_pExecuteMethodMap = NULL;
}

void CProcessor::RegisterMethodRef(WORD start, WORD end, CProcessor::ExecuteMethodRef methodref)
{
    for (int opcode = start; opcode <= end; opcode++ )
        m_pExecuteMethodMap[opcode] = methodref;
}

//////////////////////////////////////////////////////////////////////


CProcessor::CProcessor (CMotherboard* pBoard)
{
    ASSERT(pBoard != NULL);
    m_pBoard = pBoard;
    ::memset(m_R, 0, sizeof(m_R));
    m_psw = 0340;
    m_okStopped = TRUE;
    m_internalTick = 0;
    m_waitmode = FALSE;
    m_userspace = FALSE;
    m_stepmode = FALSE;
    m_RPLYrq = m_RSVDrq = m_TBITrq = m_ACLOrq = m_HALTrq = m_RPL2rq = m_IRQ2rq = FALSE;
    m_BPT_rq = m_IOT_rq = m_EMT_rq = m_TRAPrq = FALSE;
    //m_VIRQrq = FALSE;
    m_haltpin = FALSE;
}

void CProcessor::Start ()
{
    m_okStopped = FALSE;

    m_userspace = FALSE;
    m_stepmode = FALSE;
    m_waitmode = FALSE;
    m_RPLYrq = m_RSVDrq = m_TBITrq = m_ACLOrq = m_HALTrq = m_RPL2rq = m_IRQ2rq = FALSE;
    m_BPT_rq = m_IOT_rq = m_EMT_rq = m_TRAPrq = FALSE;
    m_virqrq = 0;  memset(m_virq, 0, sizeof(m_virq));

    // "Turn On" interrupt processing
    WORD pc = m_pBoard->GetSelRegister() & 0177400;
    SetPC(pc);
    SetPSW(0340);
    m_internalTick = 1000000;  // Количество тактов на включение процессора (значение с потолка)
}
void CProcessor::Stop ()
{
    m_okStopped = TRUE;

    m_userspace = FALSE;
    m_stepmode = FALSE;
    m_waitmode = FALSE;
    m_psw = 0340;
    m_internalTick = 0;
    m_RPLYrq = m_RSVDrq = m_TBITrq = m_ACLOrq = m_HALTrq = m_RPL2rq = m_IRQ2rq = FALSE;
    m_BPT_rq = m_IOT_rq = m_EMT_rq = m_TRAPrq = FALSE;
    m_virqrq = 0;  memset(m_virq, 0, sizeof(m_virq));
    m_haltpin = FALSE;
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

    m_RPLYrq = FALSE;
    
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
        m_stepmode = FALSE;
    else if (m_instruction == PI_RTT && (GetPSW() & PSW_T))
    {
        // Skip interrupt processing for RTT with T bit set
    }
    else  // Processing interrupts
    {
        while (TRUE)
        {
            m_TBITrq = (m_psw & 020);  // T-bit

            // Calculate interrupt vector and mode accoding to priority
            WORD intrVector = 0;
            BOOL currMode = ((m_psw & 0400) != 0);  // Current processor mode: TRUE = HALT mode, FALSE = USER mode
            BOOL intrMode;  // TRUE = HALT mode interrupt, FALSE = USER mode interrupt
            if (m_HALTrq)  // HALT command
            {
                intrVector = 0004;  intrMode = TRUE;
                m_HALTrq = FALSE;
            }
            else if (m_BPT_rq)  // BPT command
            {
                intrVector = 0000014;  intrMode = FALSE;
                m_BPT_rq = FALSE;
            }
            else if (m_IOT_rq)  // IOT command
            {
                intrVector = 0000020;  intrMode = FALSE;
                m_IOT_rq = FALSE;
            }
            else if (m_EMT_rq)  // EMT command
            {
                intrVector = 0000030;  intrMode = FALSE;
                m_EMT_rq = FALSE;
            }
            else if (m_TRAPrq)  // TRAP command
            {
                intrVector = 0000034;  intrMode = FALSE;
                m_TRAPrq = FALSE;
            }
            else if (m_RPLYrq && currMode)  // Зависание в HALT, priority 1
            {
                intrVector = 0004;  intrMode = TRUE;
                m_RPLYrq = FALSE;
            }
            else if (m_RPLYrq && !currMode)  // Зависание в USER, priority 1
            {
                intrVector = 0000004;  intrMode = FALSE;
                m_RPLYrq = FALSE;
            }
            else if (m_RPL2rq)  // Двойное зависание, priority 1
            {
                intrVector = 0174;  intrMode = TRUE;
                m_RPL2rq = FALSE;
            }
            else if (m_RSVDrq)  // Reserved command, priority 2
            {
                intrVector = 000010;  intrMode = FALSE;
                m_RSVDrq = FALSE;
            }
            else if (m_TBITrq && (!m_waitmode))  // T-bit, priority 3
            {
                intrVector = 000014;  intrMode = FALSE;
                m_TBITrq = FALSE;
            }
            else if (m_ACLOrq && (m_psw & 0600) != 0600)  // ACLO, priority 4
            {
                intrVector = 000024;  intrMode = FALSE;
                m_ACLOrq = FALSE;
            }
            else if (m_haltpin && (m_psw & 0400) != 0400)  // HALT signal in USER mode, priority 5
            {
                intrVector = 0170;  intrMode = TRUE;
            }
            else if (m_IRQ2rq && (m_psw & 0200) != 0200)  // EVNT signal, priority 6
            {
                intrVector = 0000100;  intrMode = FALSE;
                m_IRQ2rq = FALSE;
            }
            else if (m_virqrq > 0 && (m_psw & 0200) != 0200)  // VIRQ, priority 7
            {
                intrMode = FALSE;
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

            m_waitmode = FALSE;

            if (intrMode)  // HALT mode interrupt
            {
                WORD selVector = m_pBoard->GetSelRegister() & 0x0ff00;
                intrVector |= selVector;

                // Save PC/PSW to CPC/CPSW
                //m_savepc = GetPC();
                //m_savepsw = GetPSW();

                m_psw |= 0400;

                SetPC(GetWord(intrVector));
                m_psw = GetWord(intrVector + 2) & 0777;
            }
            else  // USER mode interrupt
            {
                WORD oldpsw = m_psw;
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

    m_IRQ2rq = TRUE;
}

void CProcessor::PowerFail()
{
    if (m_okStopped) return;  // Processor is stopped - nothing to do

    m_ACLOrq = TRUE;
}

void CProcessor::InterruptVIRQ(int que, WORD interrupt)
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
    m_haltpin = TRUE;
}

void CProcessor::DeassertHALT()
{
    m_haltpin = FALSE;
}

void CProcessor::MemoryError()
{
    m_RPLYrq = TRUE;
}


//////////////////////////////////////////////////////////////////////


// Вычисление адреса операнда, в зависимости от метода адресации
//   meth - метод адресации
//   reg  - номер регистра
WORD CProcessor::CalculateOperAddrSrc (int meth, int reg)
{
    WORD arg;

        switch (meth) {
            case 0:  // R0,     PC 
                return GetReg(reg);
            case 1:  // (R0),   (PC)
                return GetReg(reg);
            case 2:  // (R0)+,  #012345
                //if(reg==7) // is it immediate?
                //	arg = GetWord(GetReg(reg));
                //else
                    arg = GetReg(reg);
                if ((m_instruction & 0100000)&&(reg<6))
                    SetReg(reg, GetReg(reg) + 1);
                else
                    SetReg(reg, GetReg(reg) + 2);
                return arg;
            case 3:  // @(R0)+, @#012345
                //if(reg==7) //abs index
                //	arg =  GetWord(GetWord(GetReg(reg))) ;
                //else
                    arg =  GetWord(GetReg(reg)) ;
                //if ((m_instruction & 0100000)&&(reg!=7))
            //		SetReg(reg, GetReg(reg) + 1);
        //		else
                    SetReg(reg, GetReg(reg) + 2);
                return arg;
            case 4:  // -(R0),  -(PC)
                if ((m_instruction & 0100000)&&(reg<6))
                    SetReg(reg, GetReg(reg) - 1);
                else
                    SetReg(reg, GetReg(reg) - 2);
                return GetReg(reg);
            case 5:  // @-(R0), @-(PC)
        //		if (m_instruction & 0100000)
        //			SetReg(reg, GetReg(reg) - 1);
        //		else
                    SetReg(reg, GetReg(reg) - 2);
                return  GetWord(GetReg(reg));
            case 6: {  // 345(R0),  345
                WORD pc=0;
                //if(reg==7) //relative direct
                //	pc = GetWord(GetWordExec( GetPC() ));
                //else
                    pc = (GetWordExec( GetPC() ));

                SetPC( GetPC() + 2 );
                arg=(WORD)(pc + GetReg(reg));
                return arg;
            }
            case 7: {  // @345(R0),@345
                WORD pc;
                //if(reg==7) //relative direct
                //	pc = GetWord(GetWordExec( GetPC() ));
                //else
                    pc = GetWordExec( GetPC() );
                SetPC( GetPC() + 2 );
                arg=( GetWord(pc + GetReg(reg)) );
                return arg;
            }
        }
    
    return 0;
}

WORD CProcessor::CalculateOperAddr (int meth, int reg)
{
    WORD arg;
        switch (meth) {
            case 0:  // R0,     PC 
                return reg;
            case 1:  // (R0),   (PC)
                return GetReg(reg);
            case 2:  // (R0)+,  #012345
                //if(reg==7) // is it immediate?
                //	arg = GetWord(GetReg(reg));
                //else
                    arg = GetReg(reg);
                if ((m_instruction & 0100000)&&(reg<6))
                    SetReg(reg, GetReg(reg) + 1);
                else
                    SetReg(reg, GetReg(reg) + 2);
                return arg;
            case 3:  // @(R0)+, @#012345
                //if(reg==7) //abs index
                    //arg =  GetWord(GetWord(GetReg(reg))) ;
                //else
                    arg =  GetWord(GetReg(reg)) ;
                //if ((m_instruction & 0100000)&&(reg!=7))
                //	SetReg(reg, GetReg(reg) + 1);
                //else
                    SetReg(reg, GetReg(reg) + 2);
                return arg;
            case 4:  // -(R0),  -(PC)
                if ((m_instruction & 0100000)&&(reg<6))
                    SetReg(reg, GetReg(reg) - 1);
                else
                    SetReg(reg, GetReg(reg) - 2);
                return GetReg(reg);
            case 5:  // @-(R0), @-(PC)
                //if (m_instruction & 0100000)
                //	SetReg(reg, GetReg(reg) - 1);
                //else
                    SetReg(reg, GetReg(reg) - 2);
                return  GetWord(GetReg(reg));
            case 6: {  // 345(R0),  345
                WORD pc=0;
                //if(reg==7) //relative direct
                // pc = GetWord(GetWordExec( GetPC() ));
                //else
                 pc = (GetWordExec( GetPC() ));

                SetPC( GetPC() + 2 );
                arg=(WORD)(pc + GetReg(reg));
                return arg;
            }
            case 7: {  // @345(R0),@345
                WORD pc=0;
                //if(reg==7)
                //	pc = GetWord(GetWordExec( GetPC() ));
                //else
                    pc = GetWordExec( GetPC() );
                SetPC( GetPC() + 2 );
                arg=( GetWord(pc + GetReg(reg)) );
                return arg;
            }
        }

    return 0;
}


BYTE CProcessor::GetByteSrc ()
{
    if (m_methsrc == 0)
        return (BYTE) GetReg(m_regsrc)&0377;
    else
        return GetByte( m_addrsrc );
}
BYTE CProcessor::GetByteDest ()
{
    if (m_methdest == 0)
        return (BYTE) GetReg(m_regdest);
    else
        return GetByte( m_addrdest );
}

void CProcessor::SetByteDest (BYTE byte)
{
    if (m_methdest == 0)
    {
        if(byte&0200)
            SetReg(m_regdest, 0xff00|byte);
        else
            SetReg(m_regdest, (GetReg(m_regdest)&0xff00)|byte);
    }
    else
        SetByte( m_addrdest, byte );
}

WORD CProcessor::GetWordSrc ()
{
    if (m_methsrc == 0)
    {
        return GetReg(m_regsrc);
    }
    else
        return GetWord( m_addrsrc );
}
WORD CProcessor::GetWordDest ()
{
    if (m_methdest == 0)
        return GetReg(m_regdest);
    else
        return GetWord( m_addrdest );
}

void CProcessor::SetWordDest (WORD word)
{
    if (m_methdest == 0)
        SetReg(m_regdest, word);
    else
        SetWord( (m_addrdest), word );
}

WORD CProcessor::GetDstWordArgAsBranch ()
{
    int reg = GetDigit(m_instruction, 0);
    int meth = GetDigit(m_instruction, 1);
    WORD arg;

        switch (meth) {
        case 0:  // R0,     PC 
            ASSERT(0);
            return 0;
        case 1:  // (R0),   (PC)
            return GetReg(reg);
        case 2:  // (R0)+,  #012345
            arg = GetReg(reg);
            SetReg(reg, GetReg(reg) + 2);
            return arg;
        case 3:  // @(R0)+, @#012345
            arg = GetWord( GetReg(reg) );
                SetReg(reg, GetReg(reg) + 2);
            return arg;
        case 4:  // -(R0),  -(PC)
                SetReg(reg, GetReg(reg) - 2);
            return GetReg(reg);
        case 5:  // @-(R0), @-(PC)
                SetReg(reg, GetReg(reg) - 2);
            return GetWord( GetReg(reg) );
        case 6: {  // 345(R0),  345
            WORD pc = GetWordExec( GetPC() );
            SetPC( GetPC() + 2 );
            return (WORD)(pc + GetReg(reg));
        }
        case 7: {  // @345(R0),@345
            WORD pc = GetWordExec( GetPC() );
            SetPC( GetPC() + 2 );
            return GetWord( (WORD)(pc + GetReg(reg)) );
        }
    }

    return 0;
}


//////////////////////////////////////////////////////////////////////


void CProcessor::FetchInstruction()
{
    // Считываем очередную инструкцию
    WORD pc = GetPC();
    pc = pc & ~1;

    m_instruction = GetWordExec(pc);
    SetPC(GetPC() + 2);
}

void CProcessor::TranslateInstruction ()
{
    // Prepare values to help decode the command
    m_regdest  = GetDigit(m_instruction, 0);
    m_methdest = GetDigit(m_instruction, 1);
    m_regsrc   = GetDigit(m_instruction, 2);
    m_methsrc  = GetDigit(m_instruction, 3);

    // Find command implementation using the command map
    ExecuteMethodRef methodref = m_pExecuteMethodMap[m_instruction];
    (this->*methodref)();  // Call command implementation method
}

void CProcessor::ExecuteUNKNOWN ()  // Нет такой инструкции - просто вызывается TRAP 10
{
#if !defined(PRODUCT)
    DebugPrintFormat(_T(">>Invalid OPCODE = %06o %06o\r\n"), GetPC()-2, m_instruction);
#endif

    m_RSVDrq = TRUE;
}


// Instruction execution /////////////////////////////////////////////

void CProcessor::ExecuteWAIT ()  // WAIT - Wait for an interrupt
{
    m_waitmode = TRUE;

    m_internalTick = TIMING_WAIT;
}

void CProcessor::ExecuteSTEP()
{
    if ((m_psw & PSW_HALT) == 0)
    {
        m_RSVDrq = TRUE;
        return;
    }

    m_stepmode = TRUE;
    //SetPC(m_savepc);
    //SetPSW(m_savepsw);
}

void CProcessor::Execute000030()  // Unknown command
{
    if ((m_psw & PSW_HALT) == 0)
    {
        m_RSVDrq = TRUE;
        return;
    }

    //TODO: Реализовать команду
    m_RPLYrq = TRUE;
}

void CProcessor::ExecuteRUN()
{
    if ((m_psw & PSW_HALT) == 0)
    {
        m_RSVDrq = TRUE;
        return;
    }

    //SetPC(m_savepc);
    //SetPSW(m_savepsw);
}

void CProcessor::ExecuteHALT ()  // HALT - Останов
{
    m_HALTrq = TRUE;
}

void CProcessor::ExecuteRTI ()  // RTI - Возврат из прерывания
{
    WORD new_psw;
    SetReg(7, GetWord( GetSP() ) );  // Pop PC
    SetSP( GetSP() + 2 );
    
    m_psw &= 0400;  // Store HALT
    new_psw = GetWord ( GetSP() );  // Pop PSW --- saving HALT
    if(GetPC() < 0160000)
        SetPSW((new_psw & 0377)|m_psw);  // Preserve HALT mode
    else
        SetPSW(new_psw&0777); //load new mode

    SetSP( GetSP() + 2 );
    m_internalTick=TIMING_RTI;
}

void CProcessor::ExecuteBPT ()  // BPT - Breakpoint
{
    m_BPT_rq = TRUE;
    m_internalTick = TIMING_EMT;
}

void CProcessor::ExecuteIOT ()  // IOT - I/O trap
{
    m_IOT_rq = TRUE;
    m_internalTick=TIMING_EMT;
}

void CProcessor::ExecuteRESET ()  // Reset input/output devices
{
    m_pBoard->ResetDevices();  // INIT signal

    m_internalTick = TIMING_WAIT;
}

void CProcessor::ExecuteRTT ()  // RTT - return from trace trap
{
    WORD new_psw;
    SetPC( GetWord( GetSP() ) );  // Pop PC
    SetSP( GetSP() + 2 );
    
    m_psw &= PSW_HALT;  // Store HALT
    new_psw = GetWord ( GetSP() );  // Pop PSW --- saving HALT
    if (GetPC() < 0160000)
        SetPSW((new_psw & 0377) | m_psw);  // Preserve HALT mode
    else
        SetPSW(new_psw & 0777); // Load new mode
    SetSP( GetSP() + 2 );

    //m_psw |= PSW_T; // set the trap flag ???
    m_internalTick = TIMING_RTI;
}

void CProcessor::ExecuteRTS ()  // RTS - return from subroutine - Возврат из процедуры
{
    SetPC(GetReg(m_regdest));
    SetReg(m_regdest,GetWord(GetSP()));
    SetSP(GetSP()+2);

    m_internalTick = TIMING_RTS;
}

void CProcessor::ExecuteNOP ()  // NOP - Нет операции
{
    m_internalTick = TIMING_NOP;
}

void CProcessor::ExecuteCLC ()  // CLC - Очистка C
{
    SetC(FALSE);
    m_internalTick = TIMING_NOP;
}
void CProcessor::ExecuteCLV ()
{
    SetV(FALSE);
    m_internalTick = TIMING_NOP;
}
void CProcessor::ExecuteCLVC ()
{
    SetV(FALSE);
    SetC(FALSE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteCLZ ()
{
    SetZ(FALSE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteCLZC ()
{
    SetZ(FALSE);
    SetC(FALSE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteCLZV ()
{
    SetZ(FALSE);
    SetV(FALSE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteCLZVC ()
{
    SetZ(FALSE);
    SetV(FALSE);
    SetC(FALSE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteCLN ()
{
    SetN(FALSE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteCLNC ()
{
    SetN(FALSE);
    SetC(FALSE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteCLNV ()
{
    SetN(FALSE);
    SetV(FALSE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteCLNVC ()
{
    SetN(FALSE);
    SetV(FALSE);
    SetZ(FALSE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteCLNZ ()
{
    SetN(FALSE);
    SetZ(FALSE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteCLNZC ()
{
    SetN(FALSE);
    SetZ(FALSE);
    SetC(FALSE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteCLNZV ()
{
    SetN(FALSE);
    SetZ(FALSE);
    SetV(FALSE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteCCC ()
{
    SetC(FALSE);
    SetV(FALSE);
    SetZ(FALSE);
    SetN(FALSE);
    m_internalTick = TIMING_REGREG;
}
void CProcessor::ExecuteSEC ()
{
    SetC(TRUE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteSEV ()
{
    SetV(TRUE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteSEVC ()
{
    SetV(TRUE);
    SetC(TRUE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteSEZ ()
{
    SetZ(TRUE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteSEZC ()
{
    SetZ(TRUE);
    SetC(TRUE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteSEZV ()
{
    SetZ(TRUE);
    SetV(TRUE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteSEZVC ()
{
    SetZ(TRUE);
    SetV(TRUE);
    SetC(TRUE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteSEN ()
{
    SetN(TRUE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteSENC ()
{
    SetN(TRUE);
    SetZ(TRUE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteSENV ()
{
    SetN(TRUE);
    SetV(TRUE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteSENVC ()
{
    SetN(TRUE);
    SetV(TRUE);
    SetC(TRUE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteSENZ ()
{
    SetN(TRUE);
    SetZ(TRUE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteSENZC ()
{
    SetN(TRUE);
    SetZ(TRUE);
    SetC(TRUE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteSENZV ()
{
    SetN(TRUE);
    SetZ(TRUE);
    SetV(TRUE);
    m_internalTick=TIMING_NOP;
}
void CProcessor::ExecuteSCC ()
{
    SetC(TRUE);
    SetV(TRUE);
    SetZ(TRUE);
    SetN(TRUE);
    m_internalTick = TIMING_REGREG;
}

void CProcessor::ExecuteJMP ()  // JMP - jump: PC = &d (a-mode > 0)
{
    if (m_methdest == 0)  // Неправильный метод адресации
    {
        m_RPLYrq = TRUE;
        
        m_internalTick = TIMING_EMT;
    }
    else 
    {
        SetPC(GetWordAddr(m_methdest,m_regdest));

        m_internalTick = TIMING_DJ[m_methdest];
    }
}

void CProcessor::ExecuteSWAB ()
{
    WORD ea;
    WORD dst;

    dst=m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);
    dst=((dst>>8)&0377) | (dst<<8);

    if(m_methdest)
        SetWord(ea,dst);
    else
        SetReg(m_regdest,dst);
    
    SetN((dst&0200)!=0);
    SetZ(!(dst&0377));
    SetV(0);
    SetC(0);

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteCLR ()  // CLR
{
    if(m_instruction&0100000)
    {
        SetN(0);
        SetZ(1);
        SetV(0);
        SetC(0);

        if(m_methdest)
            SetByte(GetByteAddr(m_methdest,m_regdest),0);
        else
            SetReg(m_regdest,(GetReg(m_regdest)&0177400));
        
        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
    else
    {
        SetN(0);
        SetZ(1);
        SetV(0);
        SetC(0);

        if(m_methdest)
            SetWord(GetWordAddr(m_methdest,m_regdest),0);
        else
            SetReg(m_regdest,0);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
}

void CProcessor::ExecuteCOM ()  // COM
{
    WORD ea;
    if(m_instruction&0100000)
    {
        BYTE dst;

        dst= m_methdest?GetByte(ea=GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);

        dst = dst ^ 0377;

        SetN(dst>>7);
        SetZ(!dst);
        SetV(0);
        SetC(1);

        if(m_methdest)
            SetByte(ea,dst);
        else
            SetReg(m_regdest,(GetReg(m_regdest)&0177400)|dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
    else
    {
        WORD dst;

        dst= m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);
        dst = dst ^ 0177777;
        SetN(dst>>15);
        SetZ(!dst);
        SetV(0);
        SetC(1);
        if(m_methdest)
            SetWord(ea,dst);
        else
            SetReg(m_regdest,dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
}

void CProcessor::ExecuteINC ()  // INC - Инкремент
{
    WORD ea;
    if(m_instruction&0100000)
    {
        BYTE dst;

        dst=m_methdest?GetByte(ea=GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);

        dst =dst + 1;
        
        SetN(dst>>7);
        SetZ(!dst);
        SetV(dst == 0200);

        if(m_methdest)
            SetByte(ea,dst);
        else
            SetReg(m_regdest,(GetReg(m_regdest)&0177400)|dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
    else
    {
        WORD dst;

        dst= m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);

        dst = dst + 1;
        SetN(dst>>15);
        SetZ(!dst);
        SetV(dst == 0100000);

        if(m_methdest)
            SetWord(ea,dst);
        else
            SetReg(m_regdest,dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
}

void CProcessor::ExecuteDEC ()  // DEC - Декремент
{
    WORD ea;
    if(m_instruction&0100000)
    {
        BYTE dst;

        dst=m_methdest?GetByte(ea=GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);

        dst =dst - 1;
        
        SetN(dst>>7);
        SetZ(!dst);
        SetV(dst == 0177);

        if(m_methdest)
            SetByte(ea,dst);
        else
            SetReg(m_regdest,(GetReg(m_regdest)&0177400)|dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
    else
    {
        WORD dst;

        dst= m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);

        dst = dst - 1;
        SetN(dst>>15);
        SetZ(!dst);
        SetV(dst == 077777);
        if(m_methdest)
            SetWord(ea,dst);
        else
            SetReg(m_regdest,dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
}

void CProcessor::ExecuteNEG ()
{
    WORD ea;

    if(m_instruction&0100000)
    {
        BYTE dst;
        
        dst=m_methdest?GetByte(ea=GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);

        dst= 0-dst;

        SetN(dst>>7);
        SetZ(!dst);
        SetV(dst==0200);
        SetC(!GetZ());

        if(m_methdest)
            SetByte(ea,dst);
        else
            SetReg(m_regdest,(GetReg(m_regdest)&0177400)|dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
    else
    {
        WORD dst;
        
        dst=m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);

        dst= 0-dst;

        SetN(dst>>15);
        SetZ(!dst);
        SetV(dst==0100000);
        SetC(!GetZ());

        if(m_methdest)
            SetWord(ea,dst);
        else
            SetReg(m_regdest,dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
}

void CProcessor::ExecuteADC ()  // ADC{B}
{
    WORD ea;
    if(m_instruction&0100000)
    {
        BYTE dst;

        dst=m_methdest?GetByte(ea=GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);
        dst=dst+(GetC()?1:0);

        if(m_methdest)
            SetByte(ea,dst);
        else
            SetReg(m_regdest,(GetReg(m_regdest)&0177400)|dst);


        SetN(dst>>7);
        SetZ(!dst);
        SetV(GetC() && (dst==0200));
        SetC(GetC() && GetZ());

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
    else
    {
        WORD dst;

        dst=m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);
        dst=dst+(GetC()?1:0);

        if(m_methdest)
            SetWord(ea,dst);
        else
            SetReg(m_regdest,dst);


        SetN(dst>>15);
        SetZ(!dst);
        SetV(GetC() && (dst==0100000));
        SetC(GetC() && GetZ());

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
}

void CProcessor::ExecuteSBC ()  // SBC{B}
{
    WORD ea;

    if(m_instruction&0100000)
    {
        BYTE dst;
        dst=m_methdest?GetByte(ea=GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);

        dst=dst-(GetC()?1:0);

        SetN(dst>>7);
        SetZ(!dst);
        SetV(GetC() && (dst==0177));
        SetC(GetC() && (dst==0377));
    
        if(m_methdest)
            SetByte(ea,dst);
        else
            SetReg(m_regdest,(GetReg(m_regdest)&0177400)|dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
    else
    {
        WORD dst;
        dst=m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);

        dst=dst-(GetC()?1:0);

        SetN(dst>>15);
        SetZ(!dst);
        SetV(GetC() && (dst==077777));
        SetC(GetC() && (dst==0177777));
    
        if(m_methdest)
            SetWord(ea,dst);
        else
            SetReg(m_regdest,dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
}

void CProcessor::ExecuteTST ()  // TST{B} - test
{
    if(m_instruction&0100000)
    {
        BYTE dst;

        dst=m_methdest?GetByte(GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);
        SetN(dst>>7);
        SetZ(!dst);
        SetV(0);
        SetC(0);

        m_internalTick = TIMING_REGREG + TIMING_A1[m_methdest];
    }
    else
    {
        WORD dst;

        dst=m_methdest?GetWord(GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);
        SetN(dst>>15);
        SetZ(!dst);
        SetV(0);
        SetC(0);	

        m_internalTick = TIMING_REGREG + TIMING_A1[m_methdest];
    }
}

void CProcessor::ExecuteROR ()  // ROR{B}
{
    WORD ea;

    if(m_instruction&0100000)
    {
        BYTE src;
        BYTE dst;

        src=m_methdest?GetByte(ea=GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);

        dst=(src>>1)|(GetC()?0200:0);
        
        SetN(dst>>7);
        SetZ(!dst);
        SetC(src&1);
        SetV(GetN()!=GetC());
        
        if(m_methdest)
            SetByte(ea,dst);
        else
            SetReg(m_regdest,(GetReg(m_regdest)&0177400)|dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
    else
    {
        WORD src;
        WORD dst;

        src=m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);

        dst=(src>>1)|(GetC()?0100000:0);
        
        SetN(dst>>15);
        SetZ(!dst);
        SetC(src&1);
        SetV(GetN()!=GetC());
        
        if(m_methdest)
            SetWord(ea,dst);
        else
            SetReg(m_regdest,dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
}

void CProcessor::ExecuteROL ()  // ROL{B}
{
    WORD ea;

    if(m_instruction&0100000)
    {
        BYTE src;
        BYTE dst;

        src=m_methdest?GetByte(ea=GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);

        dst=(src<<1)|(GetC()?1:0);
        
        SetN(dst>>7);
        SetZ(!dst);
        SetC(src>>7);
        SetV(GetN()!=GetC());
        
        if(m_methdest)
            SetByte(ea,dst);
        else
            SetReg(m_regdest,(GetReg(m_regdest)&0177400)|dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
    else
    {
        WORD src;
        WORD dst;

        src=m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);

        dst=(src<<1)|(GetC()?1:0);
        
        SetN(dst>>15);
        SetZ(!dst);
        SetC(src>>15);
        SetV(GetN()!=GetC());
        
        if(m_methdest)
            SetWord(ea,dst);
        else
            SetReg(m_regdest,dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
}

void CProcessor::ExecuteASR ()  // ASR{B}
{
    WORD ea;
    if(m_instruction&0100000)
    {
        BYTE src;
        BYTE dst;

        src =m_methdest?GetByte(ea=GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);
        dst = (src >> 1) | (src & 0200);
        SetN(dst>>7);
        SetZ(!dst);
        SetC(src & 1);
        SetV(GetN() != GetC());
        
        if(m_methdest)
            SetByte(ea,dst);
        else
            SetReg(m_regdest,(GetReg(m_regdest)&0177400)|dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
    else
    {
        WORD src;
        WORD dst;

        src =m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);
        dst = (src >> 1) | (src & 0100000);
        SetN(dst>>15);
        SetZ(!dst);
        SetC(src & 1);
        SetV(GetN() != GetC());
        
        if(m_methdest)
            SetWord(ea,dst);
        else
            SetReg(m_regdest,dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
}

void CProcessor::ExecuteASL ()  // ASL{B}
{
    WORD ea;
    if(m_instruction&0100000)
    {
        BYTE src;
        BYTE dst;

        src = m_methdest?GetByte(ea=GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);
        dst = (src << 1) & 0377;
        SetN(dst>>7);
        SetZ(!dst);
        SetC(src>>7);
        SetV(GetN()!=GetC());
        
        if(m_methdest)
            SetByte(ea,dst);
        else
            SetReg(m_regdest,(GetReg(m_regdest)& 0177400)|dst);

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
    else
    {
        WORD src;
        WORD dst;
        src = m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);
        dst = src << 1;
        SetN(dst>>15);
        SetZ(!dst);
        SetC(src>>15);
        SetV(GetN()!=GetC());
        
        if(m_methdest)
            SetWord(ea,dst);
        else
            SetReg(m_regdest,dst);	

        m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
    }
}

void CProcessor::ExecuteSXT ()  // SXT - sign-extend
{
    if(m_methdest)
        SetWord(GetWordAddr(m_methdest,m_regdest),GetN()?0177777:0);
    else
        SetReg(m_regdest,GetN()?0177777:0); //sign extend	

    SetZ(!GetN());
    SetV(0);

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteMTPS ()  // MTPS - move to PS
{
    BYTE dst;

    dst=m_methdest?GetByte(GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);
    
    if(GetPSW()&0400)//in halt?
    { //allow everything
        SetPSW((GetPSW() & 0400) | dst);
    }
    else
    {
        SetPSW((GetPSW() & 0420) | (dst & 0357));  // preserve T
    }

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteMFPS ()  // MFPS - move from PS
{
    BYTE psw = GetPSW() & 0377;
    if (m_methdest)
        SetByte(GetByteAddr(m_methdest, m_regdest), psw);
    else
        SetReg(m_regdest, (char)psw); //sign extend
    SetN(psw & 0200);
    SetZ(psw == 0);
    SetV(0);

    m_internalTick = TIMING_REGREG + TIMING_AB[m_methdest];
}

void CProcessor::ExecuteBR ()
{
    SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    m_internalTick=TIMING_BR;
}

void CProcessor::ExecuteBNE ()
{
    if (! GetZ())
    {
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBEQ ()
{
    if (GetZ())
    {
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBGE ()
{
    if (GetN() == GetV())
    {
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBLT ()
{
    if (GetN() != GetV())
    {
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBGT ()
{
    if (! ((GetN() != GetV()) || GetZ()))
    {
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBLE ()
{
    if ((GetN() != GetV()) || GetZ())
    {
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBPL ()
{
    if (! GetN())
    {
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBMI ()
{
    if (GetN())
    {
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBHI ()
{
    if (! (GetZ() || GetC()))
    {
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBLOS ()
{
    if (GetZ() || GetC())
    {
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBVC ()
{
    if (! GetV())
    {
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBVS ()
{
    if (GetV())
    {
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBHIS ()
{
    if (! GetC())
    {
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBLO ()
{
    if (GetC())
    {
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteXOR ()  // XOR
{
    WORD dst;
    WORD ea;

    dst=m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);
    dst=dst^GetReg(m_regsrc);
    
    SetN(dst>>15);
    SetZ(!dst);
    SetV(0);

    if(m_methdest)
        SetWord(ea,dst);
    else
        SetReg(m_regdest,dst);
    
    m_internalTick = TIMING_REGREG + TIMING_A2[m_methdest];
}

void CProcessor::ExecuteSOB ()  // SOB - subtract one: R = R - 1 ; if R != 0 : PC = PC - 2*nn
{
    WORD dst = GetReg(m_regsrc);
    
    --dst;
    SetReg(m_regsrc, dst);
    
    if (dst)
    {
        SetPC(GetPC() - (m_instruction & (WORD)077) * 2 );
    }

    m_internalTick = TIMING_SOB;
}

void CProcessor::ExecuteMOV ()
{
    if (m_instruction&0100000)  // MOVB
    {
        BYTE dst = m_methsrc ? GetByte(GetByteAddr(m_methsrc, m_regsrc)) : GetReg(m_regsrc);

        SetN(dst >> 7);
        SetZ(!dst);
        SetV(0);

        if (m_methdest)
            SetByte(GetByteAddr(m_methdest, m_regdest), dst);
        else
            SetReg(m_regdest, (dst&0200)?(0177400|dst):dst);

        m_internalTick = TIMING_REGREG + TIMING_A[m_methsrc] + TIMING_DST[m_methdest];
    }
    else  // MOV
    {
        WORD dst = m_methsrc ? GetWord(GetWordAddr(m_methsrc, m_regsrc)) : GetReg(m_regsrc);

        SetN(dst >> 15);
        SetZ(!dst);
        SetV(0);

        if(m_methdest)
            SetWord(GetWordAddr(m_methdest, m_regdest), dst);
        else
            SetReg(m_regdest, dst);

        m_internalTick = TIMING_REGREG + TIMING_A[m_methsrc] + TIMING_DST[m_methdest];
    }
}

void CProcessor::ExecuteCMP ()
{
    if(m_instruction&0100000)
    {
        BYTE src;
        BYTE src2;
        BYTE dst;

        src = m_methsrc?GetByte(GetByteAddr(m_methsrc,m_regsrc)):GetReg(m_regsrc);
        src2 = m_methdest?GetByte(GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);
        
        dst = src - src2;
        SetN( CheckForNegative((BYTE)(src - src2)) );
        SetZ( CheckForZero((BYTE)(src - src2)) );
        SetV( CheckSubForOverflow (src, src2) );
        SetC( CheckSubForCarry (src, src2) );

        m_internalTick = TIMING_REGREG + TIMING_A1[m_methsrc] + TIMING_CMP[m_methdest];
    }
    else
    {
        WORD src;
        WORD src2;
        WORD dst;

        src = m_methsrc?GetWord(GetWordAddr(m_methsrc,m_regsrc)):GetReg(m_regsrc);
        src2 = m_methdest?GetWord(GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);
        
        dst = src - src2;
        
        SetN( CheckForNegative ((WORD)(src - src2)) );
        SetZ( CheckForZero ((WORD)(src - src2)) );
        SetV( CheckSubForOverflow (src, src2) );
        SetC( CheckSubForCarry (src, src2) );
    
        m_internalTick = TIMING_REGREG + TIMING_A1[m_methsrc] + TIMING_CMP[m_methdest];
    }
}

void CProcessor::ExecuteBIT ()  // BIT{B} - bit test
{
    WORD ea;
    if(m_instruction&0100000)
    {
        BYTE src;
        BYTE src2;
        BYTE dst;
        
        src = m_methsrc ? GetByte(GetByteAddr(m_methsrc, m_regsrc)) : GetReg(m_regsrc);
        src2 = m_methdest ? GetByte(ea = GetByteAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

        dst = src2 & src;

        SetN(dst >> 7);
        SetZ(!dst);
        SetV(0);

        m_internalTick = TIMING_REGREG + TIMING_A1[m_methsrc] + TIMING_CMP[m_methdest];
    }
    else
    {
        WORD src;
        WORD src2;
        WORD dst;
        
        src  = m_methsrc  ? GetWord(GetWordAddr(m_methsrc, m_regsrc)) : GetReg(m_regsrc);
        src2 = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

        dst = src2 & src;

        SetN(dst >> 15);
        SetZ(!dst);
        SetV(0);

        m_internalTick = TIMING_REGREG + TIMING_A1[m_methsrc] + TIMING_CMP[m_methdest];
    }
}

void CProcessor::ExecuteBIC ()  // BIC{B} - bit clear
{
    WORD ea;
    if(m_instruction&0100000)
    {
        BYTE src;
        BYTE src2;
        BYTE dst;
        
        src=m_methsrc?GetByte(GetByteAddr(m_methsrc,m_regsrc)):GetReg(m_regsrc);
        src2=m_methdest?GetByte(ea=GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);

        dst=src2 & (~src);

        SetN(dst>>7);
        SetZ(!dst);
        SetV(0);

        if(m_methdest)
            SetByte(ea,dst);
        else
            SetReg(m_regdest,(GetReg(m_regdest)&0177400)|dst);
        
        m_internalTick = TIMING_REGREG + TIMING_A[m_methsrc] + TIMING_DST[m_methdest];
    }
    else
    {
        WORD src;
        WORD src2;
        WORD dst;
        
        src=m_methsrc?GetWord(GetWordAddr(m_methsrc,m_regsrc)):GetReg(m_regsrc);
        src2=m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);

        dst=src2 & (~src);

        SetN(dst>>15);
        SetZ(!dst);
        SetV(0);

        if(m_methdest)
            SetWord(ea,dst);
        else
            SetReg(m_regdest,dst);

        m_internalTick = TIMING_REGREG + TIMING_A[m_methsrc] + TIMING_DST[m_methdest];
    }
}

void CProcessor::ExecuteBIS ()  // BIS{B} - bit set
{
    WORD ea;
    if(m_instruction&0100000)
    {
        BYTE src;
        BYTE src2;
        BYTE dst;
        
        src=m_methsrc?GetByte(GetByteAddr(m_methsrc,m_regsrc)):GetReg(m_regsrc);
        src2=m_methdest?GetByte(ea=GetByteAddr(m_methdest,m_regdest)):GetReg(m_regdest);

        dst=src2 | src;

        SetN(dst>>7);
        SetZ(!dst);
        SetV(0);

        if(m_methdest)
            SetByte(ea,dst);
        else
            SetReg(m_regdest,(GetReg(m_regdest)&0177400)|dst);

        m_internalTick = TIMING_REGREG + TIMING_A[m_methsrc] + TIMING_DST[m_methdest];
    }
    else
    {
        WORD src;
        WORD src2;
        WORD dst;
        
        src=m_methsrc?GetWord(GetWordAddr(m_methsrc,m_regsrc)):GetReg(m_regsrc);
        src2=m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);

        dst=src2 | src;

        SetN(dst>>15);
        SetZ(!dst);
        SetV(0);

        if(m_methdest)
            SetWord(ea,dst);
        else
            SetReg(m_regdest,dst);

        m_internalTick = TIMING_REGREG + TIMING_A[m_methsrc] + TIMING_DST[m_methdest];
    }
}

void CProcessor::ExecuteADD ()  // ADD
{
    WORD src;
    WORD src2;
    signed short dst;
    signed long dst2;
    WORD ea;
    
    src=m_methsrc?GetWord(GetWordAddr(m_methsrc,m_regsrc)):GetReg(m_regsrc);
    src2=m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);

    SetN(CheckForNegative ((WORD)(src2 + src)));
    SetZ(CheckForZero ((WORD)(src2 + src)));
    SetV(CheckAddForOverflow (src2, src));
    SetC(CheckAddForCarry (src2, src));


    dst=src2+src;
    dst2=(short)src2+(short)src;


    if(m_methdest)
        SetWord(ea,dst);
    else
        SetReg(m_regdest,dst);

    m_internalTick = TIMING_REGREG + TIMING_A[m_methsrc] + TIMING_DST[m_methdest];
}

void CProcessor::ExecuteSUB ()
{
    WORD src;
    WORD src2;
    WORD dst;
    WORD ea;
    
    src = m_methsrc?GetWord(GetWordAddr(m_methsrc,m_regsrc)):GetReg(m_regsrc);
    src2 = m_methdest?GetWord(ea=GetWordAddr(m_methdest,m_regdest)):GetReg(m_regdest);

    SetN(CheckForNegative ((WORD)(src2 - src)));
    SetZ(CheckForZero ((WORD)(src2 - src)));
    SetV(CheckSubForOverflow (src2, src));
    SetC(CheckSubForCarry (src2, src));

    dst = src2 - src;

    if(m_methdest)
        SetWord(ea,dst);
    else
        SetReg(m_regdest,dst);

    m_internalTick = TIMING_REGREG + TIMING_A[m_methsrc] + TIMING_DST[m_methdest];
}

void CProcessor::ExecuteEMT ()  // EMT - emulator trap
{
    m_EMT_rq = TRUE;
    m_internalTick=TIMING_EMT;
}

void CProcessor::ExecuteTRAP ()
{
    m_TRAPrq = TRUE;
    m_internalTick=TIMING_EMT;
}

void CProcessor::ExecuteJSR ()  // JSR - Jump subroutine: *--SP = R; R = PC; PC = &d (a-mode > 0)
{
    //int meth = GetDigit(m_instruction, DST + 1);
    if (m_methdest == 0) 
    {  // Неправильный метод адресации
        m_RPLYrq = TRUE;
        m_internalTick=TIMING_EMT;
    }
    else 
    {
        WORD dst;
        //WORD pc = GetDstWordArgAsBranch();
        dst= GetWordAddr(m_methdest,m_regdest);
        
        SetSP( GetSP() - 2 );
        
        SetWord( GetSP(), GetReg(m_regsrc) );

        SetReg(m_regsrc, GetPC());

        SetPC(dst);

        m_internalTick = TIMING_DS[m_methdest];
    }
}

void CProcessor::ExecuteMARK ()  // MARK
{
    SetSP( GetPC() + (m_instruction & 0x003F) * 2 );
    SetPC( GetReg(5) );
    SetReg(5, GetWord( GetSP() ));
    SetSP( GetSP() + 2 );
    
    m_internalTick = TIMING_MARK;
}

//////////////////////////////////////////////////////////////////////
//
// CPU image format (32 bytes):
//   2 bytes        PSW
//   2*8 bytes      Registers R0..R7
//   2*2 bytes      Saved PC and PSW
//   2 bytes        Stopped flag: !0 - stopped, 0 - not stopped

void CProcessor::SaveToImage(BYTE* pImage)
{
    WORD* pwImage = (WORD*) pImage;
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

void CProcessor::LoadFromImage(const BYTE* pImage)
{
    WORD* pwImage = (WORD*) pImage;
    // PSW
    m_psw = *pwImage++;
    // Registers R0..R7
    ::memcpy(m_R, pwImage, 2 * 8);
    // Saved PC and PSW - skip
    *pwImage++;
    *pwImage++;
    // Stopped flag
    m_okStopped = (*pwImage++ != 0);
}

WORD CProcessor::GetWordAddr (BYTE meth, BYTE reg)
{
    WORD addr;

    addr=0;

    switch(meth)
    {
        case 1:   //(R)
            addr=GetReg(reg);
            break;
        case 2:   //(R)+
            addr=GetReg(reg);
            SetReg(reg,addr+2);
            break;
        case 3:  //@(R)+
            addr=GetReg(reg);
            SetReg(reg,addr+2);
            addr=GetWord(addr);
        break;
        case 4: //-(R)
            SetReg(reg,GetReg(reg)-2);
            addr=GetReg(reg);
            break;
        case 5: //@-(R)
            SetReg(reg,GetReg(reg)-2);
            addr=GetReg(reg);
            addr=GetWord(addr);
            break;
        case 6: //d(R)
            addr=GetWord(GetPC());
            SetPC(GetPC()+2);
            addr=GetReg(reg)+addr;
            break;
        case 7: //@d(r)
            addr=GetWord(GetPC());
            SetPC(GetPC()+2);
            addr=GetReg(reg)+addr;
            addr=GetWord(addr);
            break;
    }
    return addr;
}

WORD CProcessor::GetByteAddr (BYTE meth, BYTE reg)
{
    WORD addr,delta;

    addr=0;
    switch(meth)
    {
        case 1:
            addr=GetReg(reg);
        break;
        case 2:
            delta=1+(reg>=6);
            addr=GetReg(reg);
            SetReg(reg,addr+delta);
            break;
        case 3:
            addr=GetReg(reg);
            SetReg(reg,addr+2);
            addr=GetWord(addr);
            break;
        case 4:
            delta=1+(reg>=6);
            SetReg(reg,GetReg(reg)-delta);
            addr=GetReg(reg);
            break;
        case 5:
            SetReg(reg,GetReg(reg)-2);
            addr=GetReg(reg);
            addr=GetWord(addr);
            break;
        case 6: //d(R)
            addr=GetWord(GetPC());
            SetPC(GetPC()+2);
            addr=GetReg(reg)+addr;
            break;
        case 7: //@d(r)
            addr=GetWord(GetPC());
            SetPC(GetPC()+2);
            addr=GetReg(reg)+addr;
            addr=GetWord(addr);
            break;
    }

    return addr;
}


//////////////////////////////////////////////////////////////////////
