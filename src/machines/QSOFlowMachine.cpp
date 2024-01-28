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
#include "../events/UDPReceiveEvent.h"

#include "QSOConnectMachine.h"
#include "QSOFlowMachine.h"

using namespace std;

namespace kc1fsz {

// TODO: CONSOLIDATE
static const uint32_t RTP_PORT = 5198;
static const uint32_t RTCP_PORT = 5199;

void QSOFlowMachine::start(Context* ctx) {
    _lastKeepAliveMs = 0;
    _state = OPEN;
}

void QSOFlowMachine::processEvent(const Event* ev, Context* ctx) {
    // In this state we are waiting for the reciprocal RTCP message
    if (_state == OPEN) {
        if (ev->getType() == UDPReceiveEvent::TYPE) {

            const UDPReceiveEvent* evt = (UDPReceiveEvent*)ev;

            if (evt->getChannel() == _rtcpChannel) {
                
                cout << "QSOConnectMachine: GOT RTCP DATA" << endl;
                prettyHexDump(evt->getData(), evt->getDataLen(), cout);

                if (isRTCPPacket(evt->getData(), evt->getDataLen())) {
                }

            }
            else if (evt->getChannel() == _rtpChannel) {

                cout << "QSOConnectMachine: GOT RTP DATA" << endl;
                prettyHexDump(evt->getData(), evt->getDataLen(), cout);

                if (isRTPPacket(evt->getData(), evt->getDataLen())) {
                } 
                else if (isOnDataPacket(evt->getData(), evt->getDataLen())) {
                }
            }
        }

        // Always check to make sure it's not time for keep-alive
        if (ctx->getTimeMs() > _lastKeepAliveMs + 10000) {

            _lastKeepAliveMs = ctx->getTimeMs();

            const uint16_t packetSize = 128;
            uint8_t packet[packetSize];
            // Make the SDES message and send
            uint32_t packetLen = QSOConnectMachine::formatRTCPPacket_SDES(0, _callSign, _fullName, _ssrc, packet, packetSize); 
            ctx->sendUDPChannel(_rtcpChannel, _targetAddr, RTCP_PORT, packet, packetLen);

            // Make the initial oNDATA message for the RTP port
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
            packetLen = QSOConnectMachine::formatOnDataPacket(buffer, _ssrc, packet, packetSize);
            ctx->sendUDPChannel(_rtpChannel, _targetAddr, RTP_PORT, packet, packetLen);
        }
    }
}

bool QSOFlowMachine::isDone() const {
    return false;
}

bool QSOFlowMachine::isGood() const {
    return false;
}

}


