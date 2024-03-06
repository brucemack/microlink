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
#ifdef PICO_BUILD
#include <pico/time.h>
#else
#include <sys/time.h>
#include <arpa/inet.h>
#endif

#include <cctype>
#include <locale>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cassert>

#include "kc1fsz-tools/Common.h"
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
#ifdef PICO_BUILD
    return result;
#else
    return htonl(result);
#endif
}

void formatIP4Address(uint32_t addr_nl, char* dottedAddr, uint32_t dottedAddrSize) {
#ifndef PICO_BUILD
    inet_ntop(AF_INET, &addr_nl, dottedAddr, dottedAddrSize);
#else
    // This is the high-order part of the address.
    uint32_t a = (addr_nl & 0xff000000) >> 24;
    uint32_t b = (addr_nl & 0x00ff0000) >> 16;
    uint32_t c = (addr_nl & 0x0000ff00) >> 8;
    uint32_t d = (addr_nl & 0x000000ff);
    char buf[64];
    sprintf(buf, "%lu.%lu.%lu.%lu", a, b, c, d);
    strcpyLimited(dottedAddr, buf, dottedAddrSize);
#endif
}

#ifndef PICO_BUILD
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
#endif

void writeInt32(uint8_t* buf, uint32_t d) {
    buf[0] = (d >> 24) & 0xff;
    buf[1] = (d >> 16) & 0xff;
    buf[2] = (d >>  8) & 0xff;
    buf[3] = (d      ) & 0xff;
}

uint32_t readInt32(const uint8_t* buf) {
    uint32_t r = 0;
    r |= buf[0];
    r <<= 8;
    r |= buf[1];
    r <<= 8;
    r |= buf[2];
    r <<= 8;
    r |= buf[3];
    return r;
}

uint32_t formatRTPPacket(uint16_t seq, uint32_t ssrc,
    const uint8_t gsmFrames[4][33],
    uint8_t* p, uint32_t packetSize) {

    if (packetSize < 144)
        panic("Insufficient space");

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

// TODO ADD MORE TO THIS
bool isRTCPPacket(const uint8_t* d, uint32_t len) {
    return (len > 2 && d[0] == 0xc0 && d[1] == 0xc9);
}

bool isRTCPByePacket(const uint8_t* d, uint32_t len) {
    if (!isRTCPPacket(d, len)) {
        return false;
    }
    if (len < 16) {
        return false;
    }
    return d[8] == 0xe1 && d[9] == 0xcb;
}

/**
 * The EL RTP implementation uses 144-byte packets.
 */
bool isRTPAudioPacket(const uint8_t* d, uint32_t len) {
    return len == 144 && d[0] == 0xc0 && d[1] == 0x03;
}

bool isOnDataPacket(const uint8_t* d, uint32_t len) {
    return (len >= 6 && memcmp(d, "oNDATA", 6) == 0);
}

uint32_t addRTCPPad(uint32_t unpaddedLength,
    uint8_t* p, uint32_t packetSize) {

    // Yes, this is strange that we're padding in the case
    // where the unpadded size is already divisible by 4,
    // but that's how it works!
    uint32_t padSize = 4 - (unpaddedLength % 4);

    if (packetSize < unpaddedLength + padSize)
        panic("Insufficient space");

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
    uint32_t padSize = addRTCPPad(unpaddedLength, p, packetSize);

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

uint32_t parseSDES(const uint8_t* packet, uint32_t packetLen,
    uint32_t* ssrc,
    SDESItem* items, uint32_t itemsSize) {
    
    // Reduce the packet length by the padding
    uint8_t padCount = packet[packetLen - 1];
    uint32_t len = packetLen - (uint32_t)padCount;

    // Pull out the SDES
    *ssrc = readInt32(packet + 4);

    // Skip past all headers
    const uint8_t* p = packet + 16;
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

uint32_t formatRTCPPacket_SDES(uint32_t ssrc,
    CallSign callSign, 
    FixedString fullName,
    uint32_t ssrc2,
    uint8_t* p, uint32_t packetSize) {

    // These are the only variable length fields
    const uint32_t callSignLen = callSign.len();
    const uint32_t fullNameLen = fullName.len();
    const uint32_t spacesLen = 9;
    const uint32_t versionLen = strlen(VERSION_ID);

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
    unpaddedLength += sdesPadSize;
    // Put in the pad now to make sure it fits
    uint32_t padSize = addRTCPPad(unpaddedLength, p, packetSize);
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
    memcpy(p, callSign.c_str(), callSignLen);
    p += callSignLen;
    for (uint32_t i = 0; i < spacesLen; i++)
        *(p++) = ' ';
    memcpy(p, fullName.c_str(), fullNameLen);
    p += fullNameLen;

    // Token 3
    *(p++) = 0x03;
    *(p++) = 0x08;
    memcpy(p, "CALLSIGN", 8);
    p += 8;

    // Token 4
    char buf[9];
    snprintf(buf, 9, "%08X", (unsigned int)ssrc2);
    *(p++) = 0x04;
    *(p++) = 0x08;
    memcpy(p, buf, 8);
    p += 8;

    // Token 6
    *(p++) = 0x06;
    *(p++) = versionLen;
    memcpy(p, VERSION_ID, versionLen);
    p += versionLen;

    // Token 8a - PORT
    *(p++) = 0x08;
    *(p++) = 0x06;

    *(p++) = 0x01;
    *(p++) = 0x50;
    *(p++) = 0x35;
    *(p++) = 0x31;
    *(p++) = 0x39;
    *(p++) = 0x38;

    // Token 8b - DTMF Support
    *(p++) = 0x08;
    *(p++) = 0x03;

    *(p++) = 0x01;
    // Sending D1 (enabled)
    *(p++) = 0x44;
    *(p++) = 0x31;

    // SDES padding (as needed)
    for (uint32_t i = 0; i < sdesPadSize; i++)
        *(p++) = 0x00;

    return unpaddedLength + padSize;
}

/**
 * Writes a data packet.
 */
uint32_t formatOnDataPacket(const char* msg, uint32_t ssrc,
    uint8_t* packet, uint32_t packetSize) {
    // Data + 0 +  32-bit ssrc
    uint32_t len = strlen(msg) + 1 + 4;
    if (len > packetSize) {
        return 0;
    }
    uint32_t ptr = 0;
    memcpy(packet + ptr,(const uint8_t*) msg, strlen(msg));
    ptr += strlen(msg);
    packet[ptr] = 0;
    ptr++;
    writeInt32(packet + ptr, ssrc);

    return len;
}


}
