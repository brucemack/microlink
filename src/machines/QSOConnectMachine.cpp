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
#include "../common.h"
#include "../CommContext.h"
#include "../events/UDPReceiveEvent.h"

#include "UserInfo.h"
#include "QSOConnectMachine.h"

using namespace std;

namespace kc1fsz {

static const uint32_t RTP_PORT = 5198;
static const uint32_t RTCP_PORT = 5199;

uint32_t QSOConnectMachine::_ssrcCounter = 0xf0000000;

QSOConnectMachine::QSOConnectMachine(CommContext* ctx, UserInfo* userInfo)
:   _ctx(ctx),
    _userInfo(userInfo) {   
}

void QSOConnectMachine::start() {  

    char addr[32];
    formatIP4Address(_targetAddr.getAddr(), addr, 32);
    char message[64];
    sprintf(message, "Connecting to %s", addr);

    _userInfo->setStatus(message);

    cout << "HALT" << endl;
    exit(0);

    // Get UDP connections created
    _rtpChannel = _ctx->createUDPChannel(RTP_PORT);
    _rtcpChannel = _ctx->createUDPChannel(RTCP_PORT);

    // Assign a unique SSRC
    _ssrc = _ssrcCounter++;
    const uint16_t packetSize = 128;
    uint8_t packet[packetSize];

    // Make the SDES message and send
    uint32_t packetLen = formatRTCPPacket_SDES(0, _callSign, _fullName, _ssrc, packet, packetSize); 
    // Send it on both connections
    _ctx->sendUDPChannel(_rtpChannel, _targetAddr, RTCP_PORT, packet, packetLen);
    _ctx->sendUDPChannel(_rtcpChannel, _targetAddr, RTCP_PORT, packet, packetLen);

    // Make the oNDATA message for the RTP port
    const uint16_t bufferSize = 64;
    char buffer[bufferSize];
    buffer[0] = 0;
    strcatLimited(buffer, "oNDATA\r", bufferSize);
    strcatLimited(buffer, _callSign.c_str(), bufferSize);
    strcatLimited(buffer, "\r", bufferSize);
    strcatLimited(buffer, "MicroLink V ", bufferSize);
    strcatLimited(buffer, VERSION_ID, bufferSize);
    strcatLimited(buffer, "\r", bufferSize);
    strcatLimited(buffer, _fullName.c_str(), bufferSize);
    strcatLimited(buffer, "\r", bufferSize);
    strcatLimited(buffer, _location.c_str(), bufferSize);
    strcatLimited(buffer, "\r", bufferSize);
    packetLen = formatOnDataPacket(buffer, _ssrc, packet, packetSize);
    _ctx->sendUDPChannel(_rtpChannel, _targetAddr, RTP_PORT, packet, packetLen);

    // We give this 5 seconds to receive a response
    _setTimeoutMs(time_ms() + 5000);

    _state = CONNECTING;  
}

void QSOConnectMachine::processEvent(const Event* ev) {
    // In this state we are waiting for the reciprocal RTCP message
    if (_state == CONNECTING) {
        if (ev->getType() == UDPReceiveEvent::TYPE) {
            const UDPReceiveEvent* evt = (UDPReceiveEvent*)ev;
            if (evt->getChannel() == _rtcpChannel) {

                cout << "QSOConnectMachine: GOT RTCP DATA" << endl;
                prettyHexDump(evt->getData(), evt->getDataLen(), cout);

                if (isRTCPPacket(evt->getData(), evt->getDataLen())) {
                    _state = SUCCEEDED;
                } 
            }
        }
        else if (_isTimedOut()) {
            _state = FAILED;
        }
    }
}

bool QSOConnectMachine::isDone() const {
    return _state == FAILED || _state == SUCCEEDED;
}

bool QSOConnectMachine::isGood() const {
    return _state == SUCCEEDED;
}

uint32_t QSOConnectMachine::formatRTCPPacket_SDES(uint32_t ssrc,
    CallSign callSign, 
    FixedString fullName,
    uint32_t ssrc2,
    uint8_t* p, uint32_t packetSize) {

    // These are the only variable length fields
    uint32_t callSignLen = callSign.len();
    uint32_t fullNameLen = fullName.len();
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

/**
 * Writes a data packet.
 */
uint32_t QSOConnectMachine::formatOnDataPacket(const char* msg, uint32_t ssrc,
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
