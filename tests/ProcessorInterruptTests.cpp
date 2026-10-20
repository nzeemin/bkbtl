/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// ProcessorInterruptTests.cpp : tests for the interrupts of the K1801VM1 as the BK uses them:
// the bus timeout and the STOP key (both through the vector 4), the 50 Hz event, the keyboard, the T bit,
// WAIT, RESET and the timer interrupt of the K1801VM1G
//
// Expected values come from the BK-0010 behavior documented in CLAUDE.md (the monitor ROM handles the STOP key
// through the vector 4, like a bus timeout; the write flag, bit 2 of the system register 177716, tells them
// apart) and from the K1801VM1 datasheet (priorities and PSW masks), not from the emulator output.
// The double hangup is not covered.

#include "stdafx.h"
#include <CppUnitTest.h>
#include "TestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace
{

const uint16_t PROG = 01000;
const uint16_t STACK = 0400;
const uint16_t HANDLER = 02000;     // the handler of the vector 4
const uint16_t HANDLER10 = 03000;   // the handler of the vector 010
const uint16_t HANDLER100 = 04000;  // the handler of the vector 0100 (IRQ2, the 50 Hz event)
const uint16_t HANDLER60 = 05000;   // the handler of the vector 060 (a key of the keyboard)
const uint16_t HANDLER274 = 06000;  // the handler of the vector 0274 (AR2 and the other special keys)
const uint16_t HANDLER14 = 07000;   // the handler of the vector 014 (the T bit)
const uint16_t HANDLER270 = 010000; // the handler of the vector 0270 (the timer of the K1801VM1G)

// PSW bits, defined here on purpose: the tests must not depend on Defines.h
const uint16_t TPSW_P = 0200;           // PSW7
const uint16_t TPSW_ACLO = 02000;   // PSW10: masks the STOP key and the other interrupts
const uint16_t TPSW_IRQ1 = 04000;   // PSW11: additionally masks the STOP key

const uint16_t TPSW_T = 020;           // the trace bit

const uint16_t WRITE_FLAG = 4;         // bit 2 of the system register 177716, read side

const uint16_t PORT_KEYBOARD_STATE = 0177660;   // bit 6: the interrupt is masked (1) or enabled (0), bit 7: a key is ready
const uint16_t PORT_KEYBOARD_DATA = 0177662;    // reading the key code on the BK-0010; on the BK-0011 writing the palette,
                                                // bit 14 masks the 50 Hz event (IRQ2)
const uint8_t KEY_A = 0101;

// The system timer registers and the bits of the control register
const uint16_t PORT_TIMER_RELOAD = 0177706;
const uint16_t PORT_TIMER_COUNTER = 0177710;
const uint16_t PORT_TIMER_CONTROL = 0177712;
const uint16_t TIMER_STOP = 1;
const uint16_t TIMER_WRAPAROUND = 2;
const uint16_t TIMER_EXPENABLE = 4;
const uint16_t TIMER_ONESHOT = 8;
const uint16_t TIMER_RUN = 16;
const uint16_t TIMER_EXPIRY = 0200;

const uint16_t NOP = 0000240;
const uint16_t RTI = 0000002;
const uint16_t RTT = 0000006;

// Fill the memory with NOP, so that a handler can run a few instructions
void FillNops(TestBoard& tb, uint16_t address, int count)
{
    for (int i = 0; i < count; i++)
        tb.SetWord(static_cast<uint16_t>(address + i * 2), NOP);
}

// Configure the timer to expire after the given number of the ticks, with the given control bits
void StartTimer(TestBoard& tb, uint16_t reload, uint16_t control)
{
    tb.SetWord(PORT_TIMER_RELOAD, reload);
    tb.SetWord(PORT_TIMER_CONTROL, control);
}

void TimerTicks(TestBoard& tb, int count)
{
    for (int i = 0; i < count; i++)
        tb.Board()->TimerTick();
}

// The CPU at PROG with the given program, R0..R5 zero, SP = STACK, the vector 4 points to HANDLER
void Prepare(TestBoard& tb, const uint16_t* program, size_t count)
{
    uint16_t regs[8] = { 0, 0, 0, 0, 0, 0, STACK, 0 };
    tb.SetRegs(regs);
    tb.LoadProgram(PROG, program, count);
    tb.SetWord(004, HANDLER);
    tb.SetWord(006, 0);
}

// The PSW and the PC pushed by an interrupt: the PC is on the top of the stack
void CheckFrame(TestBoard& tb, uint16_t sp, uint16_t pc, uint16_t psw, const TCHAR* msg)
{
    Eq(sp, tb.CPU()->GetSP(), msg);
    Eq(pc, tb.GetWord(sp), msg);
    Eq(psw, tb.GetWord(static_cast<uint16_t>(sp + 2)), msg);
}

// The write flag of the system register without side effects (reading the register clears it)
bool WriteFlag(TestBoard& tb)
{
    return (tb.Board()->GetSelRegister() & WRITE_FLAG) != 0;
}

}

TEST_CLASS(ProcessorInterruptTests)
{
public:
    // Bus timeout

    TEST_METHOD(Processor_BusTimeout_TakesVector4)
    {
        // An access to an address that does not answer: an unmapped port (read and write), the ROM (write).
        // The instruction is one word long, so the pushed PC is the address after it.
        struct BusCase
        {
            const TCHAR* name;
            uint16_t opcode;
            uint16_t r0;
        };
        static const BusCase cases[] =
        {
            { _T("MOV (R0),R1 from an unmapped port"),  0011001, 0177610 },
            { _T("CLR (R0) to an unmapped port"),       0005010, 0177610 },
            { _T("CLR (R0) to the ROM"),                0005010, 0100000 },
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        {
            const BusCase& c = cases[i];
            TestBoard tb;
            Prepare(tb, &c.opcode, 1);
            tb.CPU()->SetReg(0, c.r0);
            tb.CPU()->SetReg(1, 0123);
            tb.SetFlags(TF_N | TF_C);

            tb.Run();

            Eq(HANDLER, tb.CPU()->GetPC(), c.name);
            CheckFrame(tb, STACK - 4, PROG + 2, TF_N | TF_C, c.name);
            Assert::IsFalse(WriteFlag(tb), c.name);  // a real bus timeout does not set the write flag
        }
    }

    TEST_METHOD(Processor_BusTimeout_ReadLeavesTheDestinationUnchanged)
    {
        TestBoard tb;
        const uint16_t program[] = { 0011001 };  // MOV (R0),R1
        Prepare(tb, program, 1);
        tb.CPU()->SetReg(0, 0177610);
        tb.CPU()->SetReg(1, 0123);

        tb.Run();

        Eq(0123, tb.CPU()->GetReg(1), _T("R1 is not loaded"));
        Eq(0177610, tb.CPU()->GetReg(0), _T("R0"));
    }

    TEST_METHOD(Processor_BusTimeout_LoadsThePswFromTheVector)
    {
        // The new PSW is the low byte of the word at the vector + 2
        TestBoard tb;
        const uint16_t program[] = { 0005010 };  // CLR (R0)
        Prepare(tb, program, 1);
        tb.SetWord(006, 0177705);  // only the low byte counts
        tb.CPU()->SetReg(0, 0177610);

        tb.Run();

        Eq(HANDLER, tb.CPU()->GetPC(), _T("PC"));
        Eq(0305, tb.CPU()->GetPSW(), _T("PSW"));  // no T bit: it would trace the first instruction of the handler
    }

    TEST_METHOD(Processor_BusTimeout_ReturnsWithRti)
    {
        // The handler returns with RTI to the address after the failed instruction
        TestBoard tb;
        const uint16_t program[] = { 0005010, 0005200 };  // CLR (R0); INC R0
        Prepare(tb, program, 2);
        tb.SetWord(HANDLER, 0000002);  // RTI
        tb.CPU()->SetReg(0, 0100000);
        tb.SetFlags(TF_Z);

        tb.Run(2);  // the failed CLR with the entry, then RTI

        Eq(PROG + 2, tb.CPU()->GetPC(), _T("PC"));
        Eq(TF_Z, tb.GetFlags(), _T("flags are restored"));
        Eq(STACK, tb.CPU()->GetSP(), _T("SP"));
    }

    // The STOP key

    TEST_METHOD(Processor_StopKey_TakesVector4)
    {
        // The request is taken after the current instruction: the pushed PC is the address after it
        TestBoard tb;
        const uint16_t program[] = { 0000240, 0000240 };  // NOP; NOP
        Prepare(tb, program, 2);
        tb.SetFlags(TF_V);

        tb.Board()->KeyboardEvent(BK_KEY_STOP, true, false);
        tb.Run();

        Eq(HANDLER, tb.CPU()->GetPC(), _T("PC"));
        CheckFrame(tb, STACK - 4, PROG + 2, TF_V, _T("the frame"));
    }

    TEST_METHOD(Processor_StopKey_SetsTheWriteFlag)
    {
        // The handler of the vector 4 tells the STOP key from a bus hang with BIT #4,@#177716
        TestBoard tb;
        const uint16_t program[] = { 0000240 };
        Prepare(tb, program, 1);
        Assert::IsFalse(WriteFlag(tb), _T("the flag is clear before the STOP key"));

        tb.Board()->KeyboardEvent(BK_KEY_STOP, true, false);
        tb.Run();

        Assert::IsTrue(WriteFlag(tb), _T("the flag is set by the STOP key"));
    }

    TEST_METHOD(Processor_StopKey_ReleaseDoesNothing)
    {
        TestBoard tb;
        const uint16_t program[] = { 0000240, 0000240 };
        Prepare(tb, program, 2);

        tb.Board()->KeyboardEvent(BK_KEY_STOP, false, false);
        tb.Run(2);

        Eq(PROG + 4, tb.CPU()->GetPC(), _T("PC"));
        Eq(STACK, tb.CPU()->GetSP(), _T("SP"));
    }

    TEST_METHOD(Processor_StopKey_WithoutThePressNothingHappens)
    {
        TestBoard tb;
        const uint16_t program[] = { 0000240 };
        Prepare(tb, program, 1);

        tb.Run();

        Eq(PROG + 2, tb.CPU()->GetPC(), _T("PC"));
        Assert::IsFalse(WriteFlag(tb), _T("write flag"));
    }

    TEST_METHOD(Processor_StopKey_ReturnsWithRti)
    {
        TestBoard tb;
        const uint16_t program[] = { 0000240, 0005200 };  // NOP; INC R0
        Prepare(tb, program, 2);
        tb.SetWord(HANDLER, 0000002);  // RTI

        tb.Board()->KeyboardEvent(BK_KEY_STOP, true, false);
        tb.Run(2);  // NOP with the entry, then RTI
        tb.Run();   // INC R0

        Eq(1, tb.CPU()->GetReg(0), _T("R0: the interrupted program goes on"));
        Eq(PROG + 4, tb.CPU()->GetPC(), _T("PC"));
        Eq(STACK, tb.CPU()->GetSP(), _T("SP"));
    }

    // The mask bits of the PSW

    TEST_METHOD(Processor_StopKey_MaskedByPsw10AndPsw11)
    {
        // PSW10 (ACLO mask) and PSW11 (the STOP key mask) hold the request; it stays pending and is taken
        // as soon as the mask is removed
        static const uint16_t masks[] = { TPSW_ACLO, TPSW_IRQ1, TPSW_ACLO | TPSW_IRQ1 };
        for (size_t i = 0; i < sizeof(masks) / sizeof(masks[0]); i++)
        {
            TestBoard tb;
            const uint16_t program[] = { 0000240, 0000240 };
            Prepare(tb, program, 2);
            tb.CPU()->SetPSW(masks[i]);

            tb.Board()->KeyboardEvent(BK_KEY_STOP, true, false);
            tb.Run();

            TCHAR msg[40];
            _stprintf_s(msg, _T("mask %o"), static_cast<unsigned int>(masks[i]));
            Eq(PROG + 2, tb.CPU()->GetPC(), msg);   // not taken
            Eq(STACK, tb.CPU()->GetSP(), msg);
            Assert::IsFalse(WriteFlag(tb), msg);

            tb.CPU()->SetPSW(0);
            tb.Run();                               // the second NOP, then the pending request

            Eq(HANDLER, tb.CPU()->GetPC(), msg);
            CheckFrame(tb, STACK - 4, PROG + 4, 0, msg);
            Assert::IsTrue(WriteFlag(tb), msg);
        }
    }

    TEST_METHOD(Processor_StopKey_NotMaskedByPsw7)
    {
        // PSW7 masks the timer, IRQ2 and VIRQ, but not the STOP key
        TestBoard tb;
        const uint16_t program[] = { 0000240 };
        Prepare(tb, program, 1);
        tb.CPU()->SetPSW(TPSW_P);

        tb.Board()->KeyboardEvent(BK_KEY_STOP, true, false);
        tb.Run();

        Eq(HANDLER, tb.CPU()->GetPC(), _T("PC"));
        CheckFrame(tb, STACK - 4, PROG + 2, TPSW_P, _T("the frame"));
    }

    // Priority

    TEST_METHOD(Processor_ReservedInstructionIsTakenBeforeTheStopKey)
    {
        // The reserved instruction (vector 010) has a higher priority than the STOP key. The handler of the
        // vector 010 has PSW = 0, so the STOP key is taken at once after its entry: the frame of the vector 010
        // is below the frame of the vector 4, and the PC pushed by the STOP key is the handler of the vector 010.
        TestBoard tb;
        const uint16_t program[] = { 0000007 };  // a reserved opcode
        Prepare(tb, program, 1);
        tb.SetWord(010, HANDLER10);
        tb.SetWord(012, 0);

        tb.Board()->KeyboardEvent(BK_KEY_STOP, true, false);
        tb.Run();

        Eq(HANDLER, tb.CPU()->GetPC(), _T("PC is in the handler of the vector 4"));
        CheckFrame(tb, STACK - 8, HANDLER10, 0, _T("the frame of the STOP key"));
        Eq(PROG + 2, tb.GetWord(STACK - 4), _T("the PC of the reserved instruction"));
        Eq(0, tb.GetWord(STACK - 2), _T("the PSW of the reserved instruction"));
    }

    // The 50 Hz event (IRQ2, the vector 0100)

    TEST_METHOD(Processor_Irq2_NeverOnBk0010)
    {
        // The register 177662 is read-only on the BK-0010, its bit 14 stays set, and the 50 Hz event never reaches the CPU
        TestBoard tb;
        const uint16_t program[] = { 0000240, 0000240 };
        Prepare(tb, program, 2);
        tb.SetWord(0100, HANDLER100);
        tb.SetWord(0102, 0);

        tb.Board()->Tick50();
        tb.Run(2);

        Eq(PROG + 4, tb.CPU()->GetPC(), _T("PC"));
        Eq(STACK, tb.CPU()->GetSP(), _T("SP"));
    }

    TEST_METHOD(Processor_Irq2_TakesVector0100OnBk0011)
    {
        TestBoard tb;
        tb.Configure(BK_CONF_BK0011);
        const uint16_t program[] = { 0000240 };
        Prepare(tb, program, 1);
        tb.SetWord(0100, HANDLER100);
        tb.SetWord(0102, 0);
        tb.SetWord(PORT_KEYBOARD_DATA, 0);  // the bit 14 is clear: the event is enabled

        tb.Board()->Tick50();
        tb.Run();

        Eq(HANDLER100, tb.CPU()->GetPC(), _T("PC"));
        CheckFrame(tb, STACK - 4, PROG + 2, 0, _T("the frame"));
    }

    TEST_METHOD(Processor_Irq2_DisabledByBit14OnBk0011)
    {
        TestBoard tb;
        tb.Configure(BK_CONF_BK0011);
        const uint16_t program[] = { 0000240 };
        Prepare(tb, program, 1);
        tb.SetWord(0100, HANDLER100);
        tb.SetWord(0102, 0);
        tb.SetWord(PORT_KEYBOARD_DATA, 040000);  // the bit 14 is set: the event is disabled

        tb.Board()->Tick50();
        tb.Run();

        Eq(PROG + 2, tb.CPU()->GetPC(), _T("PC"));
        Eq(STACK, tb.CPU()->GetSP(), _T("SP"));
    }

    TEST_METHOD(Processor_Irq2_MaskedByPsw7AndPsw10StaysPending)
    {
        // PSW7 and PSW10 hold the request; it is not dropped and is taken as soon as the mask is removed
        static const uint16_t masks[] = { TPSW_P, TPSW_ACLO, TPSW_P | TPSW_ACLO };
        for (size_t i = 0; i < sizeof(masks) / sizeof(masks[0]); i++)
        {
            TestBoard tb;
            tb.Configure(BK_CONF_BK0011);
            const uint16_t program[] = { 0000240, 0000240 };
            Prepare(tb, program, 2);
            tb.SetWord(0100, HANDLER100);
            tb.SetWord(0102, 0);
            tb.SetWord(PORT_KEYBOARD_DATA, 0);
            tb.CPU()->SetPSW(masks[i]);

            tb.Board()->Tick50();
            tb.Run();

            TCHAR msg[40];
            _stprintf_s(msg, _T("mask %o"), static_cast<unsigned int>(masks[i]));
            Eq(PROG + 2, tb.CPU()->GetPC(), msg);   // not taken
            Eq(STACK, tb.CPU()->GetSP(), msg);

            tb.CPU()->SetPSW(0);
            tb.Run();                               // the second NOP, then the pending request

            Eq(HANDLER100, tb.CPU()->GetPC(), msg);
            CheckFrame(tb, STACK - 4, PROG + 4, 0, msg);
        }
    }

    TEST_METHOD(Processor_StopKeyIsTakenBeforeIrq2)
    {
        // The STOP key has a higher priority than IRQ2. The handler of the vector 4 has PSW = 0, so the
        // request of IRQ2 is taken at once after its entry: the frame of the vector 4 is below the frame of the vector 0100.
        TestBoard tb;
        tb.Configure(BK_CONF_BK0011);
        const uint16_t program[] = { 0000240 };
        Prepare(tb, program, 1);
        tb.SetWord(0100, HANDLER100);
        tb.SetWord(0102, 0);
        tb.SetWord(PORT_KEYBOARD_DATA, 0);

        tb.Board()->Tick50();
        tb.Board()->KeyboardEvent(BK_KEY_STOP, true, false);
        tb.Run();

        Eq(HANDLER100, tb.CPU()->GetPC(), _T("PC is in the handler of the vector 0100"));
        CheckFrame(tb, STACK - 8, HANDLER, 0, _T("the frame of IRQ2"));
        Eq(PROG + 2, tb.GetWord(STACK - 4), _T("the PC of the STOP key frame"));
    }

    // The keyboard (VIRQ, the vectors 060 and 0274)

    TEST_METHOD(Processor_Key_TakesVector060WhenEnabled)
    {
        TestBoard tb;
        const uint16_t program[] = { 0000240 };
        Prepare(tb, program, 1);
        tb.SetWord(060, HANDLER60);
        tb.SetWord(062, 0);
        tb.SetWord(PORT_KEYBOARD_STATE, 0);  // the bit 6 is clear: the keyboard interrupt is enabled

        tb.Board()->KeyboardEvent(KEY_A, true, false);
        tb.Run();

        Eq(HANDLER60, tb.CPU()->GetPC(), _T("PC"));
        CheckFrame(tb, STACK - 4, PROG + 2, 0, _T("the frame"));
    }

    TEST_METHOD(Processor_Key_TakesVector0274ForAr2)
    {
        // A key with AR2 pressed, and the special keys (the code has the bit 7), use the vector 0274
        TestBoard tb;
        const uint16_t program[] = { 0000240 };
        Prepare(tb, program, 1);
        tb.SetWord(0274, HANDLER274);
        tb.SetWord(0276, 0);
        tb.SetWord(PORT_KEYBOARD_STATE, 0);

        tb.Board()->KeyboardEvent(KEY_A, true, true);
        tb.Run();

        Eq(HANDLER274, tb.CPU()->GetPC(), _T("PC"));
        CheckFrame(tb, STACK - 4, PROG + 2, 0, _T("the frame"));
    }

    TEST_METHOD(Processor_Key_IsNotInterruptingWhileMasked)
    {
        // After the reset the bit 6 of 177660 is set: the keyboard interrupt is masked, and the key waits
        TestBoard tb;
        const uint16_t program[] = { 0000240, 0000240 };
        Prepare(tb, program, 2);
        tb.SetWord(060, HANDLER60);
        tb.SetWord(062, 0);

        tb.Board()->KeyboardEvent(KEY_A, true, false);
        tb.Run(2);

        Eq(PROG + 4, tb.CPU()->GetPC(), _T("PC"));
        Eq(STACK, tb.CPU()->GetSP(), _T("SP"));
    }

    TEST_METHOD(Processor_Key_PressedWhileMaskedInterruptsWhenUnmasked)
    {
        // The software unmasks the keyboard interrupt (writes 0 to 177660) while the key is still unread:
        // the interrupt is delivered then, as on the real BK-0010
        TestBoard tb;
        const uint16_t program[] = { 0000240, 0000240, 0000240 };
        Prepare(tb, program, 3);
        tb.SetWord(060, HANDLER60);
        tb.SetWord(062, 0);

        tb.Board()->KeyboardEvent(KEY_A, true, false);
        tb.Run();
        Eq(PROG + 2, tb.CPU()->GetPC(), _T("masked: no interrupt"));

        tb.SetWord(PORT_KEYBOARD_STATE, 0);
        tb.Run();

        Eq(HANDLER60, tb.CPU()->GetPC(), _T("PC"));
        CheckFrame(tb, STACK - 4, PROG + 4, 0, _T("the frame"));
    }

    TEST_METHOD(Processor_Key_ReadBeforeUnmaskingCancelsTheInterrupt)
    {
        // The program reads the key code (177662) while the interrupt is masked: nothing is left to deliver
        TestBoard tb;
        const uint16_t program[] = { 0000240, 0000240, 0000240 };
        Prepare(tb, program, 3);
        tb.SetWord(060, HANDLER60);
        tb.SetWord(062, 0);

        tb.Board()->KeyboardEvent(KEY_A, true, false);
        Eq(KEY_A, tb.Board()->GetWord(PORT_KEYBOARD_DATA), _T("the key code"));
        tb.SetWord(PORT_KEYBOARD_STATE, 0);
        tb.Run(3);

        Eq(PROG + 6, tb.CPU()->GetPC(), _T("PC"));
        Eq(STACK, tb.CPU()->GetSP(), _T("SP"));
    }

    TEST_METHOD(Processor_Key_MaskedByPsw7StaysPending)
    {
        // The keyboard is a VIRQ: PSW7 holds it until the mask is removed
        TestBoard tb;
        const uint16_t program[] = { 0000240, 0000240 };
        Prepare(tb, program, 2);
        tb.SetWord(060, HANDLER60);
        tb.SetWord(062, 0);
        tb.SetWord(PORT_KEYBOARD_STATE, 0);
        tb.CPU()->SetPSW(TPSW_P);

        tb.Board()->KeyboardEvent(KEY_A, true, false);
        tb.Run();
        Eq(PROG + 2, tb.CPU()->GetPC(), _T("masked: no interrupt"));

        tb.CPU()->SetPSW(0);
        tb.Run();

        Eq(HANDLER60, tb.CPU()->GetPC(), _T("PC"));
        CheckFrame(tb, STACK - 4, PROG + 4, 0, _T("the frame"));
    }

    // The T bit (trace trap, the vector 014), RTT

    TEST_METHOD(Processor_TBit_TrapsAfterTheInstruction)
    {
        TestBoard tb;
        const uint16_t program[] = { 0005200, 0005201 };  // INC R0; INC R1
        Prepare(tb, program, 2);
        tb.SetWord(014, HANDLER14);
        tb.SetWord(016, 0);
        tb.CPU()->SetPSW(TPSW_T);

        tb.Run();

        Eq(1, tb.CPU()->GetReg(0), _T("the instruction is executed"));
        Eq(0, tb.CPU()->GetReg(1), _T("the next one is not"));
        Eq(HANDLER14, tb.CPU()->GetPC(), _T("PC"));
        CheckFrame(tb, STACK - 4, PROG + 2, TPSW_T, _T("the frame"));
        Eq(0, tb.CPU()->GetPSW(), _T("the PSW of the vector has no T bit"));
    }

    TEST_METHOD(Processor_TBit_ClearMeansNoTrap)
    {
        TestBoard tb;
        const uint16_t program[] = { 0005200, 0005201 };
        Prepare(tb, program, 2);
        tb.SetWord(014, HANDLER14);
        tb.SetWord(016, 0);

        tb.Run(2);

        Eq(PROG + 4, tb.CPU()->GetPC(), _T("PC"));
        Eq(STACK, tb.CPU()->GetSP(), _T("SP"));
    }

    TEST_METHOD(Processor_TBit_RttSkipsTheTrapAfterItself)
    {
        // RTT restores the T bit, but the trace trap is delayed until the next instruction is executed
        TestBoard tb;
        const uint16_t program[] = { 0005200, 0005201, 0005202 };  // INC R0; INC R1; INC R2
        Prepare(tb, program, 3);
        tb.SetWord(014, HANDLER14);
        tb.SetWord(016, 0);
        tb.SetWord(HANDLER14, RTT);
        tb.CPU()->SetPSW(TPSW_T);

        tb.Run();   // INC R0 and the trace trap
        Eq(HANDLER14, tb.CPU()->GetPC(), _T("the trap"));

        tb.Run();   // RTT
        Eq(PROG + 2, tb.CPU()->GetPC(), _T("RTT: PC is back"));
        Eq(TPSW_T, tb.CPU()->GetPSW(), _T("RTT: the T bit is restored"));
        Eq(STACK, tb.CPU()->GetSP(), _T("RTT: SP"));
        Eq(0, tb.CPU()->GetReg(1), _T("RTT: no trap yet, INC R1 is not executed"));

        tb.Run();   // INC R1 and the trace trap
        Eq(1, tb.CPU()->GetReg(1), _T("INC R1 is executed"));
        Eq(HANDLER14, tb.CPU()->GetPC(), _T("the second trap"));
        CheckFrame(tb, STACK - 4, PROG + 4, TPSW_T, _T("the second frame"));
    }

    TEST_METHOD(Processor_TBit_RtiDoesNotSkipTheTrap)
    {
        // RTI restores the T bit and the trace trap comes at once, before the next instruction
        TestBoard tb;
        const uint16_t program[] = { 0005200, 0005201 };
        Prepare(tb, program, 2);
        tb.SetWord(014, HANDLER14);
        tb.SetWord(016, 0);
        tb.SetWord(HANDLER14, RTI);
        tb.CPU()->SetPSW(TPSW_T);

        tb.Run(2);  // INC R0 with the trap, then RTI

        Eq(HANDLER14, tb.CPU()->GetPC(), _T("the trap again"));
        CheckFrame(tb, STACK - 4, PROG + 2, TPSW_T, _T("the frame"));
        Eq(0, tb.CPU()->GetReg(1), _T("INC R1 is not executed"));
    }

    // WAIT

    TEST_METHOD(Processor_Wait_StopsUntilAnInterrupt)
    {
        TestBoard tb;
        const uint16_t program[] = { 0000001, 0005200 };  // WAIT; INC R0
        Prepare(tb, program, 2);
        tb.SetWord(HANDLER, RTI);

        tb.Run();
        Eq(PROG + 2, tb.CPU()->GetPC(), _T("WAIT is executed"));
        tb.Run(3);
        Eq(PROG + 2, tb.CPU()->GetPC(), _T("the processor waits"));
        Eq(0, tb.CPU()->GetReg(0), _T("INC R0 is not executed"));

        tb.Board()->KeyboardEvent(BK_KEY_STOP, true, false);
        tb.Run();
        Eq(HANDLER, tb.CPU()->GetPC(), _T("the interrupt wakes the processor up"));
        CheckFrame(tb, STACK - 4, PROG + 2, 0, _T("the frame"));

        tb.Run(2);  // RTI, then INC R0
        Eq(1, tb.CPU()->GetReg(0), _T("the program goes on"));
        Eq(PROG + 4, tb.CPU()->GetPC(), _T("PC"));
    }

    TEST_METHOD(Processor_Wait_MaskedInterruptDoesNotWakeUp)
    {
        TestBoard tb;
        const uint16_t program[] = { 0000001, 0005200 };
        Prepare(tb, program, 2);
        tb.CPU()->SetPSW(TPSW_ACLO);

        tb.Run();  // WAIT
        tb.Board()->KeyboardEvent(BK_KEY_STOP, true, false);
        tb.Run(3);

        Eq(PROG + 2, tb.CPU()->GetPC(), _T("PC"));
        Eq(STACK, tb.CPU()->GetSP(), _T("SP"));
        Eq(0, tb.CPU()->GetReg(0), _T("INC R0 is not executed"));
    }

    // RESET

    TEST_METHOD(Processor_Reset_CancelsThePendingDeviceInterrupts)
    {
        // The RESET instruction is the INIT signal: the devices withdraw their interrupt requests
        TestBoard tb;
        const uint16_t program[] = { 0000005, NOP, NOP };  // RESET; NOP; NOP
        Prepare(tb, program, 3);
        tb.SetWord(060, HANDLER60);
        tb.SetWord(062, 0);
        tb.SetWord(PORT_KEYBOARD_STATE, 0);
        tb.CPU()->SetPSW(TPSW_P);

        tb.Board()->KeyboardEvent(KEY_A, true, false);  // the request waits: PSW7 masks it
        tb.Run();                                       // RESET
        tb.CPU()->SetPSW(0);
        tb.Run(2);

        Eq(PROG + 6, tb.CPU()->GetPC(), _T("PC"));
        Eq(STACK, tb.CPU()->GetSP(), _T("no interrupt was taken"));
    }

    // The system timer

    TEST_METHOD(Processor_Timer_PrescalerDividesTheTicks)
    {
        // Bits 6..5 of the control register: 32 us per tick of the counter, or x16, x4, x64
        static const uint16_t dividers[] = { 1, 16, 4, 64 };
        for (uint16_t bits = 0; bits < 4; bits++)
        {
            TestBoard tb;
            StartTimer(tb, 1000, static_cast<uint16_t>(TIMER_RUN | (bits << 5)));

            TimerTicks(tb, dividers[bits] - 1);
            TCHAR msg[40];
            _stprintf_s(msg, _T("prescaler %d: one tick short"), static_cast<int>(bits));
            Eq(1000, tb.Board()->GetWord(PORT_TIMER_COUNTER), msg);

            TimerTicks(tb, 1);
            _stprintf_s(msg, _T("prescaler %d: the counter ticks"), static_cast<int>(bits));
            Eq(999, tb.Board()->GetWord(PORT_TIMER_COUNTER), msg);
        }
    }

    TEST_METHOD(Processor_Timer_StopBitHoldsTheCounterAtReload)
    {
        TestBoard tb;
        StartTimer(tb, 1000, TIMER_RUN | TIMER_STOP);

        TimerTicks(tb, 10);

        Eq(1000, tb.Board()->GetWord(PORT_TIMER_COUNTER), _T("the counter"));
    }

    TEST_METHOD(Processor_Timer_OneShotClearsRunOnExpiry)
    {
        TestBoard tb;
        StartTimer(tb, 3, TIMER_RUN | TIMER_ONESHOT);

        TimerTicks(tb, 3);

        Eq(0, tb.Board()->GetWord(PORT_TIMER_CONTROL) & TIMER_RUN, _T("RUN is cleared"));
        Eq(3, tb.Board()->GetWord(PORT_TIMER_COUNTER), _T("the counter is reloaded"));
    }

    TEST_METHOD(Processor_Timer_ExpirySetOnEveryExpiryOnTheDefaultChip)
    {
        // The K1801VM1A/B/V: EXPIRY (bit 7) is set on every expiry with EXPENABLE, and there is no interrupt
        TestBoard tb;
        const uint16_t program[] = { NOP, NOP };
        Prepare(tb, program, 2);
        tb.SetWord(0270, HANDLER270);
        tb.SetWord(0272, 0);
        StartTimer(tb, 3, TIMER_RUN | TIMER_EXPENABLE);
        Eq(0, tb.Board()->GetWord(PORT_TIMER_CONTROL) & TIMER_EXPIRY, _T("EXPIRY is clear at the start"));

        TimerTicks(tb, 3);
        Eq(TIMER_EXPIRY, tb.Board()->GetWord(PORT_TIMER_CONTROL) & TIMER_EXPIRY, _T("EXPIRY after the first expiry"));
        tb.Run(2);

        Eq(PROG + 4, tb.CPU()->GetPC(), _T("no interrupt"));
        Eq(STACK, tb.CPU()->GetSP(), _T("SP"));
    }

    // The timer interrupt of the K1801VM1G (the vector 0270)

    TEST_METHOD(Processor_TimerInterrupt_TakesVector0270OnVm1g)
    {
        TestBoard tb;
        tb.Configure(BK_CONF_BK0010_BASIC | BK_COPT_VM1G);
        const uint16_t program[] = { NOP, NOP };
        Prepare(tb, program, 2);
        tb.SetWord(0270, HANDLER270);
        tb.SetWord(0272, 0);
        StartTimer(tb, 3, TIMER_RUN | TIMER_EXPENABLE);

        TimerTicks(tb, 2);
        tb.Run();
        Eq(PROG + 2, tb.CPU()->GetPC(), _T("no interrupt before the expiry"));

        TimerTicks(tb, 1);
        tb.Run();
        Eq(HANDLER270, tb.CPU()->GetPC(), _T("PC"));
        CheckFrame(tb, STACK - 4, PROG + 4, 0, _T("the frame"));
        Eq(0, tb.Board()->GetWord(PORT_TIMER_CONTROL) & TIMER_EXPIRY, _T("EXPIRY is clear: the request was accepted"));
    }

    TEST_METHOD(Processor_TimerInterrupt_NeedsExpenable)
    {
        TestBoard tb;
        tb.Configure(BK_CONF_BK0010_BASIC | BK_COPT_VM1G);
        const uint16_t program[] = { NOP };
        Prepare(tb, program, 1);
        tb.SetWord(0270, HANDLER270);
        tb.SetWord(0272, 0);
        StartTimer(tb, 3, TIMER_RUN);

        TimerTicks(tb, 3);
        tb.Run();

        Eq(PROG + 2, tb.CPU()->GetPC(), _T("no interrupt"));
        Eq(0, tb.Board()->GetWord(PORT_TIMER_CONTROL) & TIMER_EXPIRY, _T("EXPIRY is clear without EXPENABLE"));
    }

    TEST_METHOD(Processor_TimerInterrupt_WraparoundNeverExpires)
    {
        TestBoard tb;
        tb.Configure(BK_CONF_BK0010_BASIC | BK_COPT_VM1G);
        const uint16_t program[] = { NOP };
        Prepare(tb, program, 1);
        tb.SetWord(0270, HANDLER270);
        tb.SetWord(0272, 0);
        StartTimer(tb, 3, TIMER_RUN | TIMER_EXPENABLE | TIMER_WRAPAROUND);

        TimerTicks(tb, 10);
        tb.Run();

        Eq(PROG + 2, tb.CPU()->GetPC(), _T("no interrupt"));
        Eq(0, tb.Board()->GetWord(PORT_TIMER_CONTROL) & TIMER_EXPIRY, _T("EXPIRY is never set in the wraparound mode"));
    }

    TEST_METHOD(Processor_TimerInterrupt_MaskedByPsw7AndPsw10StaysPending)
    {
        static const uint16_t masks[] = { TPSW_P, TPSW_ACLO, TPSW_P | TPSW_ACLO };
        for (size_t i = 0; i < sizeof(masks) / sizeof(masks[0]); i++)
        {
            TestBoard tb;
            tb.Configure(BK_CONF_BK0010_BASIC | BK_COPT_VM1G);
            const uint16_t program[] = { NOP, NOP };
            Prepare(tb, program, 2);
            tb.SetWord(0270, HANDLER270);
            tb.SetWord(0272, 0);
            StartTimer(tb, 3, TIMER_RUN | TIMER_EXPENABLE);
            tb.CPU()->SetPSW(masks[i]);

            TimerTicks(tb, 3);
            tb.Run();

            TCHAR msg[40];
            _stprintf_s(msg, _T("mask %o"), static_cast<unsigned int>(masks[i]));
            Eq(PROG + 2, tb.CPU()->GetPC(), msg);   // not taken
            Eq(STACK, tb.CPU()->GetSP(), msg);

            tb.CPU()->SetPSW(0);
            tb.Run();

            Eq(HANDLER270, tb.CPU()->GetPC(), msg);
            CheckFrame(tb, STACK - 4, PROG + 4, 0, msg);
        }
    }

    TEST_METHOD(Processor_TimerInterrupt_SecondExpiryWhilePendingSetsExpiryAndIsNotQueued)
    {
        // The K1801VM1G: EXPIRY (bit 7) is set only when the previous request is still waiting. The requests do not
        // pile up: after the mask is removed there is one interrupt
        TestBoard tb;
        tb.Configure(BK_CONF_BK0010_BASIC | BK_COPT_VM1G);
        const uint16_t program[] = { NOP, NOP, NOP };
        Prepare(tb, program, 3);
        tb.SetWord(0270, HANDLER270);
        tb.SetWord(0272, 0);
        FillNops(tb, HANDLER270, 4);
        StartTimer(tb, 3, TIMER_RUN | TIMER_EXPENABLE);
        tb.CPU()->SetPSW(TPSW_P);

        TimerTicks(tb, 3);
        Eq(0, tb.Board()->GetWord(PORT_TIMER_CONTROL) & TIMER_EXPIRY, _T("EXPIRY is clear after the first expiry"));
        TimerTicks(tb, 3);
        Eq(TIMER_EXPIRY, tb.Board()->GetWord(PORT_TIMER_CONTROL) & TIMER_EXPIRY, _T("EXPIRY after the second expiry"));

        tb.CPU()->SetPSW(0);
        tb.Run();
        Eq(HANDLER270, tb.CPU()->GetPC(), _T("the interrupt"));
        tb.Run();  // the first instruction of the handler
        Eq(HANDLER270 + 2, tb.CPU()->GetPC(), _T("no second interrupt"));
        Eq(STACK - 4, tb.CPU()->GetSP(), _T("SP"));
    }

    TEST_METHOD(Processor_TimerInterrupt_ResetInstructionCancelsThePendingRequest)
    {
        TestBoard tb;
        tb.Configure(BK_CONF_BK0010_BASIC | BK_COPT_VM1G);
        const uint16_t program[] = { 0000005, NOP, NOP };  // RESET; NOP; NOP
        Prepare(tb, program, 3);
        tb.SetWord(0270, HANDLER270);
        tb.SetWord(0272, 0);
        StartTimer(tb, 3, TIMER_RUN | TIMER_EXPENABLE);
        tb.CPU()->SetPSW(TPSW_P);

        TimerTicks(tb, 3);  // the request is pending
        tb.Run();           // RESET
        tb.CPU()->SetPSW(0);
        tb.Run(2);

        Eq(PROG + 6, tb.CPU()->GetPC(), _T("PC"));
        Eq(STACK, tb.CPU()->GetSP(), _T("no interrupt was taken"));
    }

    TEST_METHOD(Processor_TimerInterrupt_BoardResetCancelsThePendingRequest)
    {
        TestBoard tb;
        tb.Configure(BK_CONF_BK0010_BASIC | BK_COPT_VM1G);
        const uint16_t program[] = { NOP, NOP };
        StartTimer(tb, 3, TIMER_RUN | TIMER_EXPENABLE);
        tb.CPU()->SetPSW(TPSW_P);
        TimerTicks(tb, 3);  // the request is pending

        tb.Board()->Reset();
        Prepare(tb, program, 2);
        tb.SetWord(0270, HANDLER270);
        tb.SetWord(0272, 0);
        tb.CPU()->SetPSW(0);
        tb.Run(2);

        Eq(PROG + 4, tb.CPU()->GetPC(), _T("PC"));
        Eq(STACK, tb.CPU()->GetSP(), _T("no interrupt was taken"));
    }

    TEST_METHOD(Processor_TimerInterrupt_NotOnTheDefaultChip)
    {
        TestBoard tb;
        const uint16_t program[] = { NOP };
        Prepare(tb, program, 1);
        tb.SetWord(0270, HANDLER270);
        tb.SetWord(0272, 0);
        StartTimer(tb, 3, TIMER_RUN | TIMER_EXPENABLE);

        TimerTicks(tb, 9);
        tb.Run();

        Eq(PROG + 2, tb.CPU()->GetPC(), _T("PC"));
        Eq(STACK, tb.CPU()->GetSP(), _T("SP"));
    }

    TEST_METHOD(Processor_TimerInterrupt_Irq2IsTakenBeforeTheTimer)
    {
        // IRQ2 has a higher priority than the timer. The handler of the vector 0100 has PSW = 0, so the timer
        // request is taken at once after its entry: the frame of IRQ2 is below the frame of the timer
        TestBoard tb;
        tb.Configure(BK_CONF_BK0011 | BK_COPT_VM1G);
        const uint16_t program[] = { NOP };
        Prepare(tb, program, 1);
        tb.SetWord(0100, HANDLER100);
        tb.SetWord(0102, 0);
        tb.SetWord(0270, HANDLER270);
        tb.SetWord(0272, 0);
        tb.SetWord(PORT_KEYBOARD_DATA, 0);  // the 50 Hz event is enabled
        StartTimer(tb, 3, TIMER_RUN | TIMER_EXPENABLE);

        TimerTicks(tb, 3);
        tb.Board()->Tick50();
        tb.Run();

        Eq(HANDLER270, tb.CPU()->GetPC(), _T("PC is in the handler of the timer"));
        CheckFrame(tb, STACK - 8, HANDLER100, 0, _T("the frame of the timer"));
        Eq(PROG + 2, tb.GetWord(STACK - 4), _T("the PC of the frame of IRQ2"));
    }
};
