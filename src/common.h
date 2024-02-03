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

namespace kc1fsz {

extern const char* VERSION_ID;

/**
 * Converts the dotted-decimal IP address into a 32-bit integer in NETWORK order
 */
uint32_t parseIP4Address(const char* dottedAddr);

/**
 * Puts the address into a string in decimal-dotted format.
 *
 * @param addrNetworkOrder IP4 address
 */
void formatIP4Address(uint32_t addrNetworkOrder, char* dottedAddr, uint32_t dottedAddrSize);

/**
 * @returns The current time in milliseconds-since-epoch
*/
uint32_t time_ms();

/**
 * Used for testing purposes - sets time artificially.
 */
void set_time_ms(uint32_t ms);

/**
 * Used for testing purposes - moves time forward artificially.
 */
void advance_time_ms(uint32_t ms);

// trim from start (in place)
void ltrim(std::string &s);

// trim from end (in place)
void rtrim(std::string &s);

bool isOnDataPacket(const uint8_t* d, uint32_t len);

bool isRTCPPacket(const uint8_t* d, uint32_t len);

bool isRTPAudioPacket(const uint8_t* d, uint32_t len);

void writeInt32(uint8_t* buf, uint32_t d);

uint32_t formatRTCPPacket_BYE(uint32_t ssrc,
    uint8_t* packet, uint32_t packetSize);

uint32_t formatRTPPacket(uint16_t seq, uint32_t ssrc,
    const uint8_t gsmFrames[4][33],
    uint8_t* packet, uint32_t packetSize);

uint32_t addRTCPPad(uint32_t unpaddedLength, uint8_t* p, uint32_t packetSize);

}

#endif
