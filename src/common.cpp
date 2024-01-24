/**
 * MicoLink EchoLink Station
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
 */
#include <cstring>
#include "common.h"

using namespace std;

namespace kc1fsz {

bool isOnDataPacket(const uint8_t* d, uint32_t len) {
    return (len > 6 && memcmp(d, "oNDATA", 6) == 0);
}

/**
 * The EL RTP implementation uses 144-byte packets.
 */
bool isRTPPacket(const uint8_t* d, uint32_t len) {
    return len == 144 && d[0] == 0xc0 && d[1] == 0x03;
}

void parseRTPPacket(const uint8_t* d, uint16_t* seq, uint32_t* ssrc,
    uint8_t gsmFrames[4][33]) {    
    *seq = ((uint16_t)d[2] << 8) | (uint16_t)d[3];
    *ssrc = ((uint16_t)d[8] << 24) | ((uint16_t)d[9] << 16) | ((uint16_t)d[10] << 8) |
        ((uint16_t)d[11]);
    for (uint16_t i = 0; i < 4; i++) {
        memcpy(gsmFrames[i], (const void*)(d + 12 + (i * 33)), 33);
    }
}

void writeInt32(uint8_t* buf, uint32_t d) {
    buf[0] = (d >> 24) &0xff;
    buf[1] = (d >> 16) &0xff;
    buf[2] = (d >>  8) &0xff;
    buf[3] = (d      ) &0xff;
}

/**
 * Writes a data packet.
 */
uint32_t formatOnDataPacket(const char* msg, uint32_t ssrc,
    uint8_t* packet, uint32_t packetSize) {
    // Data + 0 +  32-bit ssrc
    uint32_t len = strlen(msg) + 1 + 4;
    if (len > packetSize) {
        throw std::invalid_argument("Insufficient space");
    }
    uint32_t ptr = 0;
    memcpy(packet + ptr,(const uint8_t*) msg, strlen(msg));
    ptr += strlen(msg);
    packet[ptr] = 0;
    ptr++;
    writeInt32(packet + ptr, ssrc);

    return len;
}

uint32_t formatRTPPacket(uint16_t seq, uint32_t ssrc,
    const uint8_t gsmFrames[4][33],
    uint8_t* packet, uint32_t packetSize) {
    return 144;
}

void prettyHexDump(const uint8_t* data, uint32_t len, std::ostream& out) {
    
    uint32_t lines = len / 16;
    if (len % 16 != 0) {
        lines++;
    }

    char buf[16];

    for (uint32_t line = 0; line < lines; line++) {
        sprintf(buf, "%08X | ", line * 16);
        out << buf;
        out << endl;
    }
}

}
