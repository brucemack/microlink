/**
 * MicroLink EchoLink Station
 * Copyright (C) 2024, Bruce MacKinnon KC1FSZ
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * FOR AMATEUR RADIO USE ONLY.
 * NOT FOR COMMERCIAL USE WITHOUT PERMISSION.
 * 
 * =================================================================================
 * This file is unit-test code only.  None of this should be use for 
 * real applications!
 * =================================================================================
 */
#include <iostream>
#include <fstream>
#include <cassert>
#include <cstring>
#include <string>

#include "Windows.h"

#include "kc1fsz-tools/Common.h"
#include "common.h"
#include "PerfTimer.h"

using namespace std;
using namespace kc1fsz;

static void test_ip_addr() {
    uint32_t addr = parseIP4Address("1.2.3.4");
    char buf[64];
    formatIP4Address(addr, buf, 64);
    assert(strcmp(buf, "1.2.3.4") == 0);
}

static void test_print() {
    uint8_t test[20] = { 0, 1, 2, 3, 4, 65, 6, 7, 8, 9, 10, 11, 12, 13, 
        48, 15, 16, 17, 18, 19 };
    prettyHexDump(test, 20, cout);
}

static void test_timing() {
#ifdef __CYGWIN__
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq); 

    // Notice here that it is difficult to sleep for only 5ms on Windows
    {
        LARGE_INTEGER pc0;
        QueryPerformanceCounter(&pc0);

        Sleep(5);

        LARGE_INTEGER pc1;
        QueryPerformanceCounter(&pc1);

        cout << "Elapsed Ticks " << pc1.QuadPart - pc0.QuadPart << endl;
        cout << "Elapsed uS    " << (pc1.QuadPart - pc0.QuadPart) * 1000000 / freq.QuadPart << endl;
    }

    // Put sleeping for 50ms is possible
    {
        LARGE_INTEGER pc0;
        QueryPerformanceCounter(&pc0);

        Sleep(50);

        LARGE_INTEGER pc1;
        QueryPerformanceCounter(&pc1);

        cout << "Elapsed Ticks " << pc1.QuadPart - pc0.QuadPart << endl;
        cout << "Elapsed uS    " << (pc1.QuadPart - pc0.QuadPart) * 1000000 / freq.QuadPart << endl;
    }
#endif

    // Test the timer class
    PerfTimer timer;
    Sleep(50);
    cout << "Elapsed uS    " << timer.elapsedUs() << endl;
}

int main(int, const char**) {
    test_print();
    test_ip_addr();
    test_timing();
}
