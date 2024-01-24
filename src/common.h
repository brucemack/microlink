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
#ifndef _common_h
#define _common_h

#include <algorithm> 
#include <cctype>
#include <locale>
#include <cstdint>
#include <iostream>

namespace kc1fsz {

// trim from start (in place)
inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

bool isOnDataPacket(const uint8_t* d, uint32_t len);

bool isRTPPacket(const uint8_t* d, uint32_t len);

void parseRTPPacket(const uint8_t* d, uint16_t* seq, uint32_t* ssrc,
    uint8_t gsmFrames[4][33]);

void writeInt32(uint8_t* buf, uint32_t d);

uint32_t formatOnDataPacket(const char* msg, uint32_t ssrc,
    uint8_t* packet, uint32_t packetSize);

uint32_t formatRTCPSDESPacket(uint32_t ssrc,
    const char* callSign, 
    const char* fullName);

uint32_t formatRTCPBYEPacket(uint32_t ssrc);

uint32_t formatRTPPacket(uint16_t seq, uint32_t ssrc,
    const uint8_t gsmFrames[4][33],
    uint8_t* packet, uint32_t packetSize);

/**
 * Produces a pretty hex-dump to aid in debugging.
 */
void prettyHexDump(const uint8_t* data, uint32_t len, std::ostream& out);

}

#endif
