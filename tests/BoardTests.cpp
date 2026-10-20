/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// BoardTests.cpp : tests for CMotherboard: creation, reset, RAM access
//
// Expected values come from the BK-0010 documentation, not from the emulator output.

#include "stdafx.h"
#include <CppUnitTest.h>
#include "TestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

TEST_CLASS(BoardTests)
{
public:

    TEST_METHOD(Board_CreateAndReset)
    {
        TestBoard tb;
        Assert::IsNotNull(tb.Board());
        Assert::IsNotNull(tb.CPU());

        // The board resets with the CPU running; the fixture then clears PSW and the registers
        Assert::IsFalse(tb.CPU()->IsStopped(), _T("CPU runs after Reset"));
        Eq(0, tb.CPU()->GetPSW(), _T("PSW"));
        for (int r = 0; r < 8; r++)
            Eq(0, tb.CPU()->GetReg(r), _T("register"));
    }

    TEST_METHOD(Board_RamWordRoundTrip)
    {
        TestBoard tb;
        // Words in RAM (000000-077777) read back as written, also at both ends of the range
        const uint16_t addresses[] = { 0000000, 0000002, 0001000, 0040000, 0077776 };
        for (size_t i = 0; i < sizeof(addresses) / sizeof(addresses[0]); i++)
        {
            uint16_t value = static_cast<uint16_t>(0123456 + i);
            tb.SetWord(addresses[i], value);
            Eq(value, tb.GetWord(addresses[i]), _T("word in RAM"));
        }
    }
};
