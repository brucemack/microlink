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

#include "kc1fsz-tools/CallSign.h"
#include "kc1fsz-tools/IPAddress.h"
#include "kc1fsz-tools/FixedString.h"

namespace kc1fsz {

extern const char* VERSION_ID;

// trim from start (in place)
void ltrim(std::string &s);

// trim from end (in place)
void rtrim(std::string &s);

bool isOnDataPacket(const uint8_t* d, uint32_t len);

bool isRTCPPacket(const uint8_t* d, uint32_t len);

bool isRTCPSDESPacket(const uint8_t* d, uint32_t len);

bool isRTCPPINGPacket(const uint8_t* d, uint32_t len);

bool isRTCPOPENPacket(const uint8_t* d, uint32_t len);

struct CallAndAddress {
    CallSign call;
    IPAddress address;
};

CallAndAddress parseRTCPOPENPacket(const uint8_t* d, uint32_t len);

bool isRTCPByePacket(const uint8_t* d, uint32_t len);

bool isRTPAudioPacket(const uint8_t* d, uint32_t len);

void writeInt32(uint8_t* buf, uint32_t d);

uint32_t readInt32(const uint8_t* buf);

uint32_t formatRTCPPacket_BYE(uint32_t ssrc,
    uint8_t* packet, uint32_t packetSize);

uint32_t formatRTPPacket(uint16_t seq, uint32_t ssrc,
    const uint8_t* gsmFrames4x33,
    uint8_t* packet, uint32_t packetSize);

uint32_t addRTCPPad(uint32_t unpaddedLength, uint8_t* p, uint32_t packetSize);

struct SDESItem {

    uint8_t type;
    uint8_t len;
    uint8_t content[256];

    void toString(char* str, uint32_t strSize) {
        // NOTE: We are leaving space for the trailing null
        for (uint32_t i = 0; i < len && i < strSize - 1; i++)
            *(str++) = content[i];
        *str = 0;
    }
};

/**
 * NOTE: Resulting items will be null-terminated even if they were 
 * not in the actual message.
*/
uint32_t parseSDES(const uint8_t* packet, uint32_t packetLen,
    uint32_t* ssrc,
    SDESItem* items, uint32_t itemsSize);

// IMPORTANT: MUST BE EXACTLY 512 BYTES!!
struct StationConfig {
    
    uint32_t version;
    uint8_t useHardCos;
    uint8_t pad1;
    uint8_t pad2;
    uint8_t pad3;
    char addressingServerHost[32];
    uint32_t addressingServerPort;
    char callSign[32];
    char password[32];
    char fullName[32];
    char location[32];
    char wifiSsid[64];
    char wifiPassword[32];
    // How long a station can stay quiet before 
    // being kicked out.
    uint32_t silentTimeoutS;
    // How long a station can stay connected with no TX/RX before 
    // being kicked out.
    uint32_t idleTimeoutS;
    // Used for soft COS detection
    uint32_t rxNoiseThreshold;
    // Used for ADC calibration
    int32_t adcRawOffset;

    char padding[512 - (4 + 4 + 32 + 4 + 32 + 32 + 32 + 32 + 64 + 32 + 4 + 4 + 4 + 4)];
};

uint32_t formatOnDataPacket(const char* msg, uint32_t ssrc,
    uint8_t* packet, uint32_t packetSize);

uint32_t formatRTCPPacket_SDES(uint32_t ssrc,
    const CallSign& callSign, 
    const FixedString& fullName,
    uint32_t ssrc2,
    uint8_t* packet, uint32_t packetSize);      

/**
 * @returns The lenth of the actual packet in bytes.
*/
uint32_t formatRTCPPacket_PING(uint32_t ssrc,
    CallSign callSign, uint8_t* packet, uint32_t packetSize);      

/**
 * @returns The lenth of the actual packet in bytes.
*/
uint32_t formatRTCPPacket_OVER(uint32_t ssrc,
    uint8_t* packet, uint32_t packetSize);      

/**
 * @returns The lenth of the actual packet in bytes.
*/
uint32_t formatRTPPacket_McAD(uint8_t* p, uint32_t packetSize);

/**
 * A utility function for building Logon/ONLINE request messages.
*/
uint32_t createOnlineMessage(uint8_t* buf, uint32_t bufLen,
    CallSign cs, FixedString pwd, FixedString loc,
    const FixedString& versionId);
}


#endif
