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
#ifndef _Common2_h
#define _Common2_h

#include <cstdint>
#include <string>
#include <iostream>

#include "CallSign.h"
#include "FixedString.h"

namespace kc1fsz {

extern const char* VERSION_ID;

uint32_t parseIP4Address(const char* dottedAddr);

/**
 * @returns The current time in milliseconds-since-epoch
*/
uint32_t time_ms();

/**
 * @param targetLimit The actual size of the target buffer.  This 
 * function will automatically save a space for the null.
*/
void strcpyLimited(char* target, const char* source, uint32_t targetLimit);

void memcpyLimited(uint8_t* target, const uint8_t* source, 
    uint32_t sourceLen, uint32_t targetLimit);

// trim from start (in place)
void ltrim(std::string &s);

// trim from end (in place)
void rtrim(std::string &s);

bool isOnDataPacket(const uint8_t* d, uint32_t len);

bool isRTPPacket(const uint8_t* d, uint32_t len);

void parseRTPPacket(const uint8_t* d, uint16_t* seq, uint32_t* ssrc,
    uint8_t gsmFrames[4][33]);

void writeInt32(uint8_t* buf, uint32_t d);

uint32_t formatOnDataPacket(const char* msg, uint32_t ssrc,
    uint8_t* packet, uint32_t packetSize);

uint32_t formatRTCPPacket_SDES(uint32_t ssrc,
    CallSign callSign, 
    FixedString fullName,
    uint32_t ssrc2,
    uint8_t* packet, uint32_t packetSize);

uint32_t formatRTCPPacket_BYE(uint32_t ssrc,
    uint8_t* packet, uint32_t packetSize);

uint32_t formatRTPPacket(uint16_t seq, uint32_t ssrc,
    const uint8_t gsmFrames[4][33],
    uint8_t* packet, uint32_t packetSize);

/**
 * Produces a pretty hex-dump to aid in debugging.
 */
void prettyHexDump(const uint8_t* data, uint32_t len, std::ostream& out,
    bool useColor = true);

}

#endif
