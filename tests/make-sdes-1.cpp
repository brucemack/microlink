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

using namespace std;
using namespace kc1fsz;

int main(int, const char**) {

    const char * fn = "./tests/data/sdes-1.bin";
 
    CallSign callSign("KC1FSZ");
    FixedString fullName("Bruce R. MacKinnon");
    uint32_t ssrc = 0;

    const uint32_t packetSize = 256;
    uint8_t packet[packetSize];

    uint32_t packetLen = QSOConnectMachine::formatRTCPPacket_SDES(0, 
        callSign, fullName, ssrc, packet, packetSize); 

    ofstream f(fn);
    f.write((const char*)packet, packetLen);
    f.close();
}

