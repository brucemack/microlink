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

struct SDESItem {

    uint8_t type;
    uint8_t len;
    uint8_t content[256];

    void toString(char* str, uint32_t strSize) {
        uint32_t c = 0;
        uint32_t i = 0;
        // NOTE: We are leaving space for the trailing null
        for (i = 0; i < len && i < strSize - 1; i++)
            *(str++) = content[i];
        *str = 0;
    }
};

uint32_t parseSDES(uint8_t* packet, uint32_t packetLen,
    uint32_t* ssrc,
    SDESItem* items, uint32_t itemsSize) {
    
    // Reduce the packet length by the padding
    uint8_t padCount = packet[packetLen - 1];
    uint32_t len = packetLen - (uint32_t)padCount;

    // Pull out the SDES
    *ssrc = readInt32(packet + 4);

    // Skip past all headers
    uint8_t* p = packet + 16;
    uint32_t itemCount = 0;
    uint8_t itemPtr;
    int state = 0;

    while (p < packet + len && itemCount < itemsSize) {
        if (state == 0) {
            items[itemCount].type = *p;
            state = 1;
        } else if (state == 1) {
            items[itemCount].len = *p;
            itemPtr = 0;
            if (*p == 0) {
                state = 0;
            } else {
                state = 2;
            }
        } else if (state == 2) {
            items[itemCount].content[itemPtr++] = *p;
            if (itemPtr == items[itemCount].len) {
                state = 0;
                itemCount++;
            }
        }
        p++;
    }
    return itemCount;
}

int main(int, const char**) {

    const char * fn = "./tests/data/sdes-1.bin";
    const uint32_t packetSize = 256;
    uint8_t packet[packetSize];
    uint32_t packetLen = 0;

    {
        CallSign callSign("KC1FSZ");
        FixedString fullName("Bruce R. MacKinnon");
        uint32_t ssrc = 77;
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
    }
}

