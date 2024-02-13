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
#include <cstdint>
#include <iostream>
#include <fstream>
#include <cassert>
#include <cstring>
#include <cmath>

#include "kc1fsz-tools/FixedString.h"
#include "kc1fsz-tools/CallSign.h"

#include "machines/QSOConnectMachine.h"
#include "common.h"

using namespace std;
using namespace kc1fsz;

int main(int, const char**) {

    const char * fn = "./tests/data/sdes-1.bin";
    const uint32_t packetSize = 256;
    uint8_t packet[packetSize];
    uint32_t packetLen = 0;

    {
        CallSign callSign("KC1FSZ");
        FixedString fullName("Bruce R. MacKinnon");
        uint32_t ssrc = 1;
        packetLen = QSOConnectMachine::formatRTCPPacket_SDES(0, 
            callSign, fullName, ssrc, packet, packetSize); 

        ofstream f(fn);
        f.write((const char*)packet, packetLen);
        f.close();
    }

    {
        SDESItem items[8];
        uint32_t ssrc = 0;
        uint32_t itemCount = parseSDES(packet, packetLen, &ssrc, items, 8);
        assert(ssrc == 0);
        assert(itemCount == 7);
        assert(items[1].type == 2);
        char callSignAndName[64];
        items[1].toString(callSignAndName, 64);
        assert(strcmp(callSignAndName, "KC1FSZ         Bruce R. MacKinnon") == 0);
        // Strip off the call
        char callSign[32];
        uint32_t i = 0;
        for (i = 0; i < 31 && callSignAndName[i] != ' '; i++)
            callSign[i] = callSignAndName[i];
        callSign[i] = 0;
        assert(strcmp(callSign, "KC1FSZ") == 0);

        assert(items[3].type == 4);
        char ssrcBuf[32];
        items[3].toString(ssrcBuf, 32);
        assert(1 == atoi(ssrcBuf));
    }
}

