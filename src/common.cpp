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
 */
#include <sys/time.h>
#include <cctype>
#include <algorithm> 
#include <locale>
#include <iostream>
#include <algorithm>
#include <cstring>

#include "common.h"

using namespace std;

namespace kc1fsz {

// Per Jonathan K1RFD: Make sure this starts with a number and ends with Z.
const char* VERSION_ID = "0.02MLZ";

uint32_t parseIP4Address(const char* dottedAddr) {
    uint32_t result = 0;
    char acc[8];
    uint32_t accLen = 0;
    const char *p = dottedAddr;
    uint32_t octets = 4;
    while (true) {
        if (*p == '.' || *p == 0) {
            acc[accLen] = 0;
            // Shift up
            result <<= 8;
            // Accumulate LSB
            result |= (uint8_t)atoi(acc);
            accLen = 0;
            // Count octets
            octets++;
            if (octets == 4 || *p == 0) {
                break;
            }
        }
        else {
            acc[accLen++] = *p;
        }
        p++;
    }
    return result;
}

void strcpyLimited(char* target, const char* source, uint32_t limit) {
    uint32_t len = std::min(limit - 1, (uint32_t)std::strlen(source));
    memcpy(target, source, len);
    target[len] = 0;
}

void memcpyLimited(uint8_t* target, const uint8_t* source, 
    uint32_t sourceLen, uint32_t targetLimit) {
    uint32_t len = std::min(targetLimit, sourceLen);
    memcpy(target, source, len);
}

// trim from start (in place)
void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

uint32_t time_ms() {
    struct timeval tp;
    gettimeofday(&tp, NULL);
    long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
    return ms;
}

void writeInt32(uint8_t* buf, uint32_t d) {
    buf[0] = (d >> 24) &0xff;
    buf[1] = (d >> 16) &0xff;
    buf[2] = (d >>  8) &0xff;
    buf[3] = (d      ) &0xff;
}

uint32_t formatRTPPacket(uint16_t seq, uint32_t ssrc,
    const uint8_t gsmFrames[4][33],
    uint8_t* p, uint32_t packetSize) {

    if (packetSize < 144)
        throw std::invalid_argument("Insufficient space");

    *(p++) = 0xc0;
    *(p++) = 0x03;

    // Sequence #
    *(p++) = (seq >> 8) & 0xff;
    *(p++) = (seq     ) & 0xff;

    // Timestamp
    *(p++) = 0x00;
    *(p++) = 0x00;
    *(p++) = 0x00;
    *(p++) = 0x00;

    // SSRC
    writeInt32(p, ssrc);
    p += 4;

    for (uint16_t i = 0; i < 4; i++) {
        memcpy((void*)(p + (i * 33)), gsmFrames[i], 33);
    }

    return 144;
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

bool isOnDataPacket(const uint8_t* d, uint32_t len) {
    return (len > 6 && memcmp(d, "oNDATA", 6) == 0);
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

static uint32_t addPad(uint32_t unpaddedLength,
    uint8_t* p, uint32_t packetSize) {

    // Yes, this is strange that we're padding in the case
    // where the unpadded size is already divisible by 4,
    // but that's how it works!
    uint32_t padSize = 4 - (unpaddedLength % 4);

    if (packetSize < unpaddedLength + padSize)
        throw std::invalid_argument("Insufficient space");

    // Move to where the pad needs to be added
    p += unpaddedLength;

    // Apply standard pad, including the pad length in the final byte
    for (uint32_t i = 0; i < padSize; i++)
        *(p++) = 0x00;

    // Back-track and insert the pad length as the very last byte of the 
    // packet.
    *(p - 1) = (uint8_t)padSize;

    return padSize;
}

uint32_t formatRTCPPacket_SDES(uint32_t ssrc,
    const char* callSign, 
    const char* fullName,
    uint32_t ssrc2,
    uint8_t* p, uint32_t packetSize) {

    // These are the only variable length fields
    uint32_t callSignLen = strlen(callSign);
    uint32_t fullNameLen = strlen(fullName);
    uint32_t spacesLen = 9;

    // Do the length calculation to make sure we have the 
    // space we need.
    //
    // RTCP header = 8
    // BYE = 4
    // SSRC = 4
    uint32_t unpaddedLength = 8 + 4 + 4;
    // Token 1 = 2 + Len("CALLSIGN") = 10
    unpaddedLength += 10;
    // Token 2 = 2 + Len(callSign) + spaces + Len(fullName)
    unpaddedLength += 2 + callSignLen + spacesLen + fullNameLen;
    // Token 3 = 2 + Len("CALLSIGN") = 10
    unpaddedLength += 2 + 8;
    // Token 4 = 2 + 8
    unpaddedLength += 2 + 8;
    // Token 6 = 2 + Len(VERSION_ID) 
    unpaddedLength += 2 + strlen(VERSION_ID);
    // Token 8 = 2 + 6
    unpaddedLength += 2 + 6;
    // Token 8 = 2 + 3
    unpaddedLength += 2 + 3;
    // Now we deal with the extra padding required by SDES to bring
    // the packet up to a 4-byte boundary.
    uint32_t sdesPadSize = 4 - (unpaddedLength % 4);
    //if (sdesPadSize == 4) {
    //    sdesPadSize = 0;
    //}
    unpaddedLength += sdesPadSize;
    // Put in the pad now to make sure it fits
    uint32_t padSize = addPad(unpaddedLength, p, packetSize);
    // Calculate the special SDES length
    uint32_t sdesLength = (unpaddedLength + padSize - 12) / 4;

    // RTCP header
    *(p++) = 0xc0;
    *(p++) = 0xc9;
    // Length
    *(p++) = 0x00;
    *(p++) = 0x01;
    // SSRC
    writeInt32(p, ssrc);
    p += 4;

    // Packet identifier
    *(p++) = 0xe1;
    *(p++) = 0xca;
    // SDES length
    *(p++) = (sdesLength >> 8) & 0xff;
    *(p++) = (sdesLength     ) & 0xff;
    // SSRC
    writeInt32(p, ssrc);
    p += 4;

    // Token 1
    *(p++) = 0x01;
    *(p++) = 0x08;
    memcpy(p, "CALLSIGN", 8);
    p += 8;

    // Token 2
    *(p++) = 0x02;
    *(p++) = (uint8_t)(callSignLen + spacesLen + fullNameLen);
    memcpy(p, callSign, callSignLen);
    p += callSignLen;
    for (uint32_t i = 0; i < spacesLen; i++)
        *(p++) = ' ';
    memcpy(p, fullName, fullNameLen);
    p += fullNameLen;

    // Token 3
    *(p++) = 0x03;
    *(p++) = 0x08;
    memcpy(p, "CALLSIGN", 8);
    p += 8;

    // Token 4
    char buf[9];
    sprintf(buf,"%08X", ssrc2);
    *(p++) = 0x04;
    *(p++) = 0x08;
    memcpy(p, buf, 8);
    p += 8;

    // Token 6
    *(p++) = 0x06;
    *(p++) = 0x08;
    //memcpy(p, "E2.3.122", 8);
    memcpy(p, VERSION_ID, strlen(VERSION_ID));
    p += strlen(VERSION_ID);

    // Token 8a
    *(p++) = 0x08;
    *(p++) = 0x06;

    *(p++) = 0x01;
    *(p++) = 0x50;
    *(p++) = 0x35;
    *(p++) = 0x31;
    *(p++) = 0x39;
    *(p++) = 0x38;

    // Token 8b
    *(p++) = 0x08;
    *(p++) = 0x03;

    *(p++) = 0x01;
    *(p++) = 0x44;
    *(p++) = 0x30;

    // SDES padding (as needed)
    for (uint32_t i = 0; i < sdesPadSize; i++)
        *(p++) = 0x00;

    return unpaddedLength + padSize;
}

uint32_t formatRTCPPacket_BYE(uint32_t ssrc,
    uint8_t* p, uint32_t packetSize) {

    // Do the length calculation to make sure we have the 
    // space we need.
    //
    // RTCP header = 8
    // BYE = 4
    // SSRC = 4
    // Reason text length = 1
    // Reason text = 7
    uint32_t unpaddedLength = 8 + 4 + 4 + 1 + 7;

    // Put in the pad now to make sure it fits
    uint32_t padSize = addPad(unpaddedLength, p, packetSize);

    // Now pack the data

    // RTCP header
    *(p++) = 0xc0;
    *(p++) = 0xc9;
    // Length
    *(p++) = 0x00;
    *(p++) = 0x01;
    // SSRC
    writeInt32(p, ssrc);
    p += 4;

    // Packet identifier
    *(p++) = 0xe1;
    *(p++) = 0xcb;
    *(p++) = 0x00;
    *(p++) = 0x04;
    // SSRC
    writeInt32(p, ssrc);
    p += 4;
    // Length
    *(p++) = 7;
    memcpy(p, "jan2002", 7);
    p += 7;

    return unpaddedLength + padSize;
}

void prettyHexDump(const uint8_t* data, uint32_t len, std::ostream& out,
    bool color) {
    
    uint32_t lines = len / 16;
    if (len % 16 != 0) {
        lines++;
    }

    char buf[16];

    for (uint32_t line = 0; line < lines; line++) {

        // Position counter
        sprintf(buf, "%04X | ", line * 16);
        out << buf;

        // Hex section
        for (uint16_t i = 0; i < 16; i++) {
            uint32_t k = line * 16 + i;
            if (k < len) {
                sprintf(buf, "%02x", (unsigned int)data[k]);
                out << buf << " ";
            } else {
                out << "   ";
            }
            if (i == 7) {
                out << " ";
            }
        }
        // Space between hex and ASCII section
        out << " ";

        if (color) {   
            out << "\u001b[36m";
        }

        // ASCII section
        for (uint16_t i = 0; i < 16; i++) {
            uint32_t k = line * 16 + i;
            if (k < len) {
                if (isprint((char)data[k]) && data[k] != 32) {
                    out << (char)data[k];
                } else {
                    //out << ".";
                    out << "\u00b7";
                }
            } else {
                out << " ";
            }
            if (i == 7) {
                out << " ";
            }
        }

        if (color) {   
            out << "\u001b[0m";
        }
        out << std::endl;
    }
}

}
