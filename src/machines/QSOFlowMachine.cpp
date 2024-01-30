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
#include "../events/TickEvent.h"

#include "QSOConnectMachine.h"
#include "QSOFlowMachine.h"

using namespace std;

namespace kc1fsz {

// TODO: CONSOLIDATE
static const uint32_t RTP_PORT = 5198;
static const uint32_t RTCP_PORT = 5199;

QSOFlowMachine::QSOFlowMachine(CommContext* ctx, UserInfo* userInfo, 
    AudioOutputContext* audioOutput) 
:   _ctx(ctx),
    _userInfo(userInfo),
    _audioOutput(audioOutput) {
}

void QSOFlowMachine::start() {
    _lastKeepAliveSentMs = 0;
    _lastKeepAliveRecvMs = time_ms();
    _state = OPEN;
}

void QSOFlowMachine::processEvent(const Event* ev) {
    // In this state we are waiting for the reciprocal RTCP message
    if (_state == OPEN) {
        if (ev->getType() == UDPReceiveEvent::TYPE) {

            const UDPReceiveEvent* evt = (UDPReceiveEvent*)ev;

            if (evt->getChannel() == _rtcpChannel) {
                
                cout << "QSOConnectMachine: GOT RTCP DATA" << endl;
                prettyHexDump(evt->getData(), evt->getDataLen(), cout);
               
               _lastKeepAliveRecvMs = time_ms();

                if (isRTCPPacket(evt->getData(), evt->getDataLen())) {
                }

            }
            else if (evt->getChannel() == _rtpChannel) {

                _lastKeepAliveRecvMs = time_ms();

                if (isRTPAudioPacket(evt->getData(), evt->getDataLen())) {

                    cout << "Got Audio " << endl;

                    // TODO: MAKE THIS MORE EFFICIENT - WE DON'T NEED TWO DATA MOVES
                    // Unload the GSM frames from the RTP packet
                    uint8_t gsmFrames[4][33];
                    uint16_t remoteSeq = 0;
                    uint32_t remoteSSRC = 0;
                    parseRTPAudioPacket(evt->getData(), &remoteSeq, &remoteSSRC, gsmFrames);

                    // TODO: LOOK AT SSRC and ignore echos

                    // Decode the GSM data
                    Parameters params;
                    int16_t pcmData[4 * 160];
                    uint32_t pcmPtr = 0;
                    for (uint32_t f = 0; f < 4; f++) {
                        // TODO: PROVIDE AN UNPACK THAT CONTAINS STATE
                        kc1fsz::PackingState state;
                        params.unpack(gsmFrames[f], &state);
                        _gsmDecoder.decode(&params, pcmData + pcmPtr);
                        pcmPtr += 160;
                    }

                    // Hand off to the audio context to play the sound
                    _audioOutput->play(pcmData);
                } 
                else if (isOnDataPacket(evt->getData(), evt->getDataLen())) {
                    // Make sure the message is null-terminated one way or the other
                    char temp[64];
                    memcpyLimited((uint8_t*)temp, evt->getData(), evt->getDataLen(), 63);
                    temp[std::min((uint32_t)63, evt->getDataLen())] = 0;
                    // Here we skip past the oNDATA part when we report the message
                    _userInfo->setOnData(temp + 6);
                }
            }
        }

        // Always check to make sure it's not time for keep-alive
        if (time_ms() > _lastKeepAliveSentMs + 10000) {

            _lastKeepAliveSentMs = time_ms();

            const uint16_t packetSize = 128;
            uint8_t packet[packetSize];
            // Make the SDES message and send
            uint32_t packetLen = QSOConnectMachine::formatRTCPPacket_SDES(0, _callSign, _fullName, _ssrc, packet, packetSize); 
            _ctx->sendUDPChannel(_rtcpChannel, _targetAddr, RTCP_PORT, packet, packetLen);

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
            _ctx->sendUDPChannel(_rtpChannel, _targetAddr, RTP_PORT, packet, packetLen);
        }

        // Always check to make sure the other side is still there
        if (time_ms() > _lastKeepAliveRecvMs + 30000) {
            // Wrap up
            // TODO: ADD CLEANUP STATE
            _state = SUCCEEDED;
        }
    }
}

bool QSOFlowMachine::isDone() const {
    return (_state == SUCCEEDED);
}

bool QSOFlowMachine::isGood() const {
    return (_state == SUCCEEDED);
}

}
