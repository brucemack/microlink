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

static bool timeFixed = false;
static uint32_t fakeTime = 0;

uint32_t time_ms() {
    if (timeFixed) {
        return fakeTime;
    } else {
#ifdef PICO_BUILD
    absolute_time_t now = get_absolute_time();
        return to_ms_since_boot(now);
#else
        struct timeval tp;
        gettimeofday(&tp, NULL);
        long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
        return ms;
#endif
    }
}

void set_time_ms(uint32_t ms) {
    fakeTime = ms;
    timeFixed = true;
}

void advance_time_ms(uint32_t ms) {
    fakeTime = time_ms() + ms;
    timeFixed = true;
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

#ifndef PICO_BUILD
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
#endif 

#ifndef PICO_BUILD
void panic(const char* msg) {
    cerr << "PANIC: " << msg << endl;
    assert(false);
}
#else 
void panic(const char* msg) {
    cerr << "PANIC: " << msg << endl;
    assert(false);
}
#endif

}
