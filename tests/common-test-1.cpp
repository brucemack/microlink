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

#include "common.h"

using namespace std;
using namespace kc1fsz;

/*
static void test_packets() {

    const uint32_t packetSize = 256;
    uint8_t p[packetSize];
    uint32_t ssrc = 32767;

    {
        const char* callSign = "KC1FSZ";
        const char* fullName = "Bruce R. MacKinnon";

        uint32_t l = formatRTCPPacket_SDES(0x00000000,
            callSign, fullName, 0x90d7678d, p, packetSize);

        cout << "RTCP SDES packet (length=" << l << "):" << endl;
        prettyHexDump(p, l, cout);
    }

    {
        uint32_t l = formatRTCPPacket_BYE(ssrc,
            p, packetSize);
        assert(l == 28);
        assert(p[l - 1] == 4);

        cout << "RTCP BYE packet:" << endl;
        prettyHexDump(p, l, cout);
    }

    {
        const char* msg = "oNDATA\rStation KC1FSZ\rMicroLink 2\rBruce MacKinnon\r";
        uint32_t l = formatOnDataPacket(msg, ssrc,
            p, packetSize);
        cout << "RTP oNDATA packet:" << endl;
        prettyHexDump(p, l, cout);
    }

    {
        uint8_t gsmData[4][33];
        for (uint16_t i = 0; i < 4; i++)
            for (uint16_t k = 0; k < 33; k++)
                gsmData[i][k] = i;
        uint32_t l = formatRTPPacket(7, ssrc, gsmData, p, packetSize);
        assert(l == 144);
        cout << "RTP Audio packet:" << endl;
        prettyHexDump(p, l, cout);
    }
}
*/

static void test_ip_addr() {
    uint32_t addr = parseIP4Address("1.2.3.4");
    cout << std::hex << addr << endl;
    char buf[64];
    formatIP4Address(addr, buf, 64);
    cout << buf << endl;
}

int main(int, const char**) {

    //uint8_t test[20] = { 0, 1, 2, 3, 4, 65, 6, 7, 8, 9, 10, 11, 12, 13, 
    //    48, 15, 16, 17, 18, 19 };
    //prettyHexDump(test, 20, cout);

    //test_packets();
    test_ip_addr();
}
