/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// Processor.h
//

#pragma once

#include "Defines.h"
#include "Board.h"


//////////////////////////////////////////////////////////////////////


class CProcessor  // KM1801VM1 processor
{
public:  // Constructor / initialization
    CProcessor(CMotherboard* pBoard);
    void        AssertHALT();
    void        DeassertHALT();
    void        MemoryError();
    void        AssertIRQ1();
    int			GetInternalTick() const { return m_internalTick; }
    void        SetInternalTick (uint16_t tick) { m_internalTick = tick; }

public:
    static void Init();  // Initialize static tables
    static void Done();  // Release memory used for static tables
protected:  // Statics
    typedef void ( CProcessor::*ExecuteMethodRef )();
    static ExecuteMethodRef* m_pExecuteMethodMap;
    static void RegisterMethodRef(uint16_t start, uint16_t end, CProcessor::ExecuteMethodRef methodref);

protected:  // Processor state
    int         m_internalTick;     // How many ticks waiting to the end of current instruction
    uint16_t    m_psw;              // Processor Status Word (PSW)
    uint16_t    m_R[8];             // Registers (R0..R5, R6=SP, R7=PC)
    bool        m_okStopped;        // "Processor stopped" flag
    bool        m_userspace;        // Read TRUE if user space is used -- CPU is accessing I/O from HALT mode using user space
    bool        m_stepmode;         // Read TRUE if it's step mode
    bool        m_haltpin;			// HALT
    bool        m_waitmode;			// WAIT

protected:  // Current instruction processing
    uint16_t    m_instruction;      // Curent instruction
    uint16_t    m_instructionpc;    // Address of the current instruction
    uint8_t     m_regsrc;           // Source register number
    uint8_t     m_methsrc;          // Source address mode
    uint16_t    m_addrsrc;          // Source address
    uint8_t     m_regdest;          // Destination register number
    uint8_t     m_methdest;         // Destination address mode
    uint16_t    m_addrdest;         // Destination address
protected:  // Interrupt processing
    bool        m_RPLYrq;           // Hangup interrupt pending
    bool        m_RSVDrq;           // Reserved instruction interrupt pending
    bool        m_TBITrq;           // T-bit interrupt pending
    bool        m_ACLOrq;           // Power down interrupt pending
    bool        m_HALTrq;           // HALT command or HALT signal
    bool        m_RPL2rq;           // Double hangup interrupt pending
    bool        m_IRQ1rq;
    bool        m_IRQ2rq;           // Timer event interrupt pending
    bool        m_BPT_rq;           // BPT command interrupt pending
    bool        m_IOT_rq;           // IOT command interrupt pending
    bool        m_EMT_rq;           // EMT command interrupt pending
    bool        m_TRAPrq;           // TRAP command interrupt pending
    //bool        m_VIRQrq;           // VIRQ vector interrupt pending
    //uint16_t    m_VIRQvector;       // VIRQ interrupt vector
    int         m_virqrq;           // VIRQ pending
    uint16_t    m_virq[16];         // VIRQ vector
protected:
    CMotherboard* m_pBoard;

public:  // Register control
    uint16_t    GetPSW() const { return m_psw; }
    void        SetPSW(uint16_t word) { m_psw = word; }
    uint16_t    GetReg(int regno) const { return m_R[regno]; }
    void        SetReg(int regno, uint16_t word) { m_R[regno] = word; }
    uint8_t     GetLReg(int regno) const { return (uint8_t)(m_R[regno] & 0xff); }
    uint16_t    GetSP() const { return m_R[6]; }
    void        SetSP(uint16_t word) { m_R[6] = word; }
    uint16_t    GetPC() const { return m_R[7]; }
    void        SetPC(uint16_t word) { m_R[7] = word; }
    uint16_t    GetInstructionPC() const { return m_instructionpc; }  // Address of the current instruction

public:  // PSW bits control
    void        SetC(bool bFlag);
    uint16_t    GetC() const { return (m_psw & PSW_C) != 0; }
    void        SetV(bool bFlag);
    uint16_t    GetV() const { return (m_psw & PSW_V) != 0; }
    void        SetN(bool bFlag);
    uint16_t    GetN() const { return (m_psw & PSW_N) != 0; }
    void        SetZ(bool bFlag);
    uint16_t    GetZ() const { return (m_psw & PSW_Z) != 0; }
    uint16_t    GetHALT() const { return (m_psw & PSW_HALT) != 0; }

public:  // Processor state
    // "Processor stopped" flag
    bool        IsStopped() const { return m_okStopped; }
    // HALT flag (TRUE - HALT mode, false - USER mode)
    bool        IsHaltMode() const
    {
        bool mode = ((m_psw & 0x100) != 0);
        if (mode)
            if (m_userspace)
                return false;
        return mode;
    }
public:  // Processor control
    void        Start();     // Start processor
    void        Stop();      // Stop processor
    void        TickIRQ2();  // IRQ2 signal
    void        PowerFail();
    void        InterruptVIRQ(int que, uint16_t interrupt);  // External interrupt via VIRQ signal
    void        Execute();   // Execute one instruction - for debugger only

public:  // Saving/loading emulator status (pImage addresses up to 32 bytes)
    void        SaveToImage(uint8_t* pImage);
    void        LoadFromImage(const uint8_t* pImage);

protected:  // Implementation
    void        FetchInstruction();      // Read next instruction
    void        TranslateInstruction();  // Execute the instruction
protected:  // Implementation - instruction processing
    uint16_t    CalculateOperAddr (int meth, int reg);
    uint16_t    CalculateOperAddrSrc (int meth, int reg);
    uint8_t     GetByteSrc();
    uint8_t     GetByteDest();
    void        SetByteDest(uint8_t);
    uint16_t    GetWordSrc();
    uint16_t    GetWordDest();
    void        SetWordDest(uint16_t);
    uint16_t    GetDstWordArgAsBranch();
protected:  // Implementation - memory access
    uint16_t    GetWordExec(uint16_t address) { return m_pBoard->GetWordExec(address, IsHaltMode()); }
    uint16_t    GetWord(uint16_t address) { return m_pBoard->GetWord(address, IsHaltMode()); }
    void        SetWord(uint16_t address, uint16_t word) { m_pBoard->SetWord(address, IsHaltMode(), word); }
    uint8_t     GetByte(uint16_t address) { return m_pBoard->GetByte(address, IsHaltMode()); }
    void        SetByte(uint16_t address, uint8_t byte) { m_pBoard->SetByte(address, IsHaltMode(), byte); }

protected:  // PSW bits calculations
    bool static CheckForNegative(uint8_t byte) { return (byte & 0200) != 0; }
    bool static CheckForNegative(uint16_t word) { return (word & 0100000) != 0; }
    bool static CheckForZero(uint8_t byte) { return byte == 0; }
    bool static CheckForZero(uint16_t word) { return word == 0; }
    bool static CheckAddForOverflow(uint8_t a, uint8_t b);
    bool static CheckAddForOverflow(uint16_t a, uint16_t b);
    bool static CheckSubForOverflow(uint8_t a, uint8_t b);
    bool static CheckSubForOverflow(uint16_t a, uint16_t b);
    bool static CheckAddForCarry(uint8_t a, uint8_t b);
    bool static CheckAddForCarry(uint16_t a, uint16_t b);
    bool static CheckSubForCarry(uint8_t a, uint8_t b);
    bool static CheckSubForCarry(uint16_t a, uint16_t b);

protected:  // Implementation - instruction execution
    // No fields
    uint16_t        GetWordAddr (uint8_t meth, uint8_t reg);
    uint16_t        GetByteAddr (uint8_t meth, uint8_t reg);

    void        ExecuteUNKNOWN ();  // Нет такой инструкции - просто вызывается TRAP 10
    void        ExecuteHALT ();
    void        ExecuteWAIT ();
    void        ExecuteRTI ();
    void        ExecuteBPT ();
    void        ExecuteIOT ();
    void        ExecuteRESET ();
    void        ExecuteSTEP ();
    void        ExecuteRUN ();
    void        ExecuteRTT ();
    void        ExecuteNOP ();
    void        ExecuteCCC ();
    void        ExecuteSCC ();

    // One fiels
    void        ExecuteRTS ();

    // Two fields
    void        ExecuteJMP ();
    void        ExecuteSWAB ();
    void        ExecuteCLR ();
    void        ExecuteCOM ();
    void        ExecuteINC ();
    void        ExecuteDEC ();
    void        ExecuteNEG ();
    void        ExecuteADC ();
    void        ExecuteSBC ();
    void        ExecuteTST ();
    void        ExecuteTSTB ();
    void        ExecuteROR ();
    void        ExecuteROL ();
    void        ExecuteASR ();
    void        ExecuteASL ();
    void        ExecuteMARK ();
    void        ExecuteSXT ();
    void        ExecuteMTPS ();
    void        ExecuteMFPS ();

    // Branchs & interrupts
    void        ExecuteBR ();
    void        ExecuteBNE ();
    void        ExecuteBEQ ();
    void        ExecuteBGE ();
    void        ExecuteBLT ();
    void        ExecuteBGT ();
    void        ExecuteBLE ();
    void        ExecuteBPL ();
    void        ExecuteBMI ();
    void        ExecuteBHI ();
    void        ExecuteBLOS ();
    void        ExecuteBVC ();
    void        ExecuteBVS ();
    void        ExecuteBHIS ();
    void        ExecuteBLO ();

    void        ExecuteEMT ();
    void        ExecuteTRAP ();

    // Three fields
    void        ExecuteJSR ();
    void        ExecuteXOR ();
    void        ExecuteSOB ();

    // Four fields
    void        ExecuteMOV ();
    void        ExecuteMOVB ();
    void        ExecuteCMP ();
    void        ExecuteCMPB ();
    void        ExecuteBIT ();
    void        ExecuteBIC ();
    void        ExecuteBIS ();

    void        ExecuteADD ();
    void        ExecuteSUB ();
};

// PSW bits control - implementation
inline void CProcessor::SetC (bool bFlag)
{
    if (bFlag) m_psw |= PSW_C; else m_psw &= ~PSW_C;
}
inline void CProcessor::SetV (bool bFlag)
{
    if (bFlag) m_psw |= PSW_V; else m_psw &= ~PSW_V;
}
inline void CProcessor::SetN (bool bFlag)
{
    if (bFlag) m_psw |= PSW_N; else m_psw &= ~PSW_N;
}
inline void CProcessor::SetZ (bool bFlag)
{
    if (bFlag) m_psw |= PSW_Z; else m_psw &= ~PSW_Z;
}

// PSW bits calculations - implementation
inline bool CProcessor::CheckAddForOverflow (uint8_t a, uint8_t b)
{
#if defined(_M_IX86) && defined(_MSC_VER)
    bool bOverflow = false;
    _asm
    {
        pushf
        push cx
        mov cl, byte ptr [a]
        add cl, byte ptr [b]
        jno end
        mov dword ptr [bOverflow], 1
        end:
        pop cx
        popf
    }
    return bOverflow;
#else
    //uint16_t sum = a < 0200 ? (uint16_t)a + (uint16_t)b + 0200 : (uint16_t)a + (uint16_t)b - 0200;
    //return HIBYTE (sum) != 0;
    uint8_t sum = a + b;
    return ((~a ^ b) & (a ^ sum)) & 0200;
#endif
}
inline bool CProcessor::CheckAddForOverflow (uint16_t a, uint16_t b)
{
#if defined(_M_IX86) && defined(_MSC_VER)
    bool bOverflow = false;
    _asm
    {
        pushf
        push cx
        mov cx, word ptr [a]
        add cx, word ptr [b]
        jno end
        mov dword ptr [bOverflow], 1
        end:
        pop cx
        popf
    }
    return bOverflow;
#else
    //uint32_t sum =  a < 0100000 ? (uint32_t)a + (uint32_t)b + 0100000 : (uint32_t)a + (uint32_t)b - 0100000;
    //return HIWORD (sum) != 0;
    uint16_t sum = a + b;
    return ((~a ^ b) & (a ^ sum)) & 0100000;
#endif
}
//void        CProcessor::SetReg(int regno, uint16_t word)

inline bool CProcessor::CheckSubForOverflow (uint8_t a, uint8_t b)
{
#if defined(_M_IX86) && defined(_MSC_VER)
    bool bOverflow = false;
    _asm
    {
        pushf
        push cx
        mov cl, byte ptr [a]
        sub cl, byte ptr [b]
        jno end
        mov dword ptr [bOverflow], 1
        end:
        pop cx
        popf
    }
    return bOverflow;
#else
    //uint16_t sum = a < 0200 ? (uint16_t)a - (uint16_t)b + 0200 : (uint16_t)a - (uint16_t)b - 0200;
    //return HIBYTE (sum) != 0;
    uint8_t sum = a - b;
    return ((a ^ b) & (~b ^ sum)) & 0200;
#endif
}
inline bool CProcessor::CheckSubForOverflow (uint16_t a, uint16_t b)
{
#if defined(_M_IX86) && defined(_MSC_VER)
    bool bOverflow = false;
    _asm
    {
        pushf
        push cx
        mov cx, word ptr [a]
        sub cx, word ptr [b]
        jno end
        mov dword ptr [bOverflow], 1
        end:
        pop cx
        popf
    }
    return bOverflow;
#else
    //uint32_t sum =  a < 0100000 ? (uint32_t)a - (uint32_t)b + 0100000 : (uint32_t)a - (uint32_t)b - 0100000;
    //return HIWORD (sum) != 0;
    uint16_t sum = a - b;
    return ((a ^ b) & (~b ^ sum)) & 0100000;
#endif
}
inline bool CProcessor::CheckAddForCarry (uint8_t a, uint8_t b)
{
    uint16_t sum = (uint16_t)a + (uint16_t)b;
    return (sum & 0xff00) != 0;
}
inline bool CProcessor::CheckAddForCarry (uint16_t a, uint16_t b)
{
    uint32_t sum = (uint32_t)a + (uint32_t)b;
    return (uint16_t)((sum >> 16) & 0xffff) != 0;
}
inline bool CProcessor::CheckSubForCarry (uint8_t a, uint8_t b)
{
    uint16_t sum = (uint16_t)a - (uint16_t)b;
    return (sum & 0xff00) != 0;
}
inline bool CProcessor::CheckSubForCarry (uint16_t a, uint16_t b)
{
    uint32_t sum = (uint32_t)a - (uint32_t)b;
    return (uint16_t)((sum >> 16) & 0xffff) != 0;
}


//////////////////////////////////////////////////////////////////////
