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
#include "kc1fsz-tools/CommContext.h"

#include "kc1fsz-tools/events/UDPReceiveEvent.h"
#include "kc1fsz-tools/events/SendEvent.h"
#include "kc1fsz-tools/events/ChannelSetupEvent.h"
#include "kc1fsz-tools/Common.h"

#include "../common.h"

#include "UserInfo.h"
#include "QSOConnectMachine.h"

using namespace std;

namespace kc1fsz {

static const uint32_t RTP_PORT = 5198;
static const uint32_t RTCP_PORT = 5199;

static const uint32_t CHANNEL_SETUP_TIMEOUT_MS = 250;
static const uint32_t SEND_TIMEOUT_MS = 1000;
// How long we wait for the remote station to respond
static const uint32_t REMOTE_TIMEOUT_MS = 10000;
static const uint32_t RETRY_COUNT = 3;

static const char* FAILED_MSG = "Station connection failed";
static const char* WAITING_FOR_RESPONSE_MSG = "Waiting for remote station";

int QSOConnectMachine::traceLevel = 0;

uint32_t QSOConnectMachine::_ssrcCounter = 0xd0000010;

QSOConnectMachine::QSOConnectMachine(CommContext* ctx, UserInfo* userInfo)
:   _ctx(ctx),
    _userInfo(userInfo) {   
}

void QSOConnectMachine::start() {  

    char addr[32];
    formatIP4Address(_targetAddr.getAddr(), addr, 32);
    char message[64];
    snprintf(message, 63, "Connecting to %s", addr);
    _userInfo->setStatus(message);

    // Get UDP connections created
    _rtcpChannel = _ctx->createUDPChannel();
    _rtpChannel = _ctx->createUDPChannel();

    // Assign a unique SSRC
    _ssrc = _ssrcCounter++;
    _state = State::IN_SETUP_0;  

    // Start the RTCP socket setup
    _ctx->setupUDPChannel(_rtcpChannel, RTCP_PORT, _targetAddr, RTCP_PORT);
    _state = State::IN_SETUP_1;
    _setTimeoutMs(time_ms() + CHANNEL_SETUP_TIMEOUT_MS);
}

void QSOConnectMachine::processEvent(const Event* ev) {

    if (traceLevel > 0) {
        cout << "QSOConnectMachine state=" << _state << " event=" << ev->getType() <<  endl;
    }

    // In this state we are waiting for confirmation that the RTCP 
    // socket was setup.
    if (_state == State::IN_SETUP_1) {
        if (ev->getType() == ChannelSetupEvent::TYPE) {
            auto evt = static_cast<const ChannelSetupEvent*>(ev);
            if (evt->isGood()) {
                // Start the RTP socket setup
                _ctx->setupUDPChannel(_rtpChannel, RTP_PORT, _targetAddr, RTP_PORT);
                _state = State::IN_SETUP_2;
                _setTimeoutMs(time_ms() + CHANNEL_SETUP_TIMEOUT_MS);
            } else {
                _userInfo->setStatus(FAILED_MSG);
                _state = State::FAILED;
            }
        }
        else if (_isTimedOut()) {
            _userInfo->setStatus(FAILED_MSG);
            _state = State::FAILED;
        }
    }
    // In this state we are waiting for confirmation that the RTP 
    // socket was setup.
    else if (_state == State::IN_SETUP_2) {
        if (ev->getType() == ChannelSetupEvent::TYPE) {
            auto evt = static_cast<const ChannelSetupEvent*>(ev);
            if (evt->isGood()) {
                // On the first try the timeout zero to force the first
                // connect attempt
                _setTimeoutMs(time_ms());
                _state = State::CONNECTING;  
                _retryCount = 0;
            } else {
                _userInfo->setStatus(FAILED_MSG);
                _state = State::FAILED;
            }
        }
        else if (_isTimedOut()) {
            _userInfo->setStatus(FAILED_MSG);
            _state = State::FAILED;
        }
    } 
    // In this state we are waiting for the reciprocal RTCP message
    else if (_state == CONNECTING) {        
        if (ev->getType() == UDPReceiveEvent::TYPE) {
            const UDPReceiveEvent* evt = (UDPReceiveEvent*)ev;
            if (evt->getChannel() == _rtcpChannel) {

                if (traceLevel > 0) {
                    cout << "QSOConnectMachine: GOT RTCP DATA" << endl;
                    prettyHexDump(evt->getData(), evt->getDataLen(), cout);
                }

                if (isRTCPPacket(evt->getData(), evt->getDataLen())) {
                    _state = SUCCEEDED;
                } 
            } 
            else if (evt->getChannel() == _rtpChannel) {
                if (isOnDataPacket(evt->getData(), evt->getDataLen())) {
                    // Make sure the message is null-terminated one way or the other
                    char temp[64];
                    memcpyLimited((uint8_t*)temp, evt->getData(), evt->getDataLen(), 63);
                    temp[std::min((uint32_t)63, evt->getDataLen())] = 0;
                    // Here we skip past the oNDATA part when we report the message
                    _userInfo->setOnData(temp + 6);
                }
            }
        }
        else if (_isTimedOut()) {

            // Check to see if it's time to give up connecting
            if (_retryCount++ >= RETRY_COUNT) {
                _userInfo->setStatus("No response from station");
                _state = FAILED;
            }
            else {
                const uint16_t packetSize = 128;
                uint8_t packet[packetSize];

                // Make the SDES message and send
                uint32_t packetLen = formatRTCPPacket_SDES(0, _callSign, _fullName, _ssrc, packet, packetSize); 
                // Send it on both connections
                // The EchoLink client sends this, but things seem to work without it?
                //_ctx->sendUDPChannel(_rtpChannel, _targetAddr, RTCP_PORT, packet, packetLen);
                _ctx->sendUDPChannel(_rtcpChannel, packet, packetLen);

                _state = State::CONNECTING_0;
                _setTimeoutMs(time_ms() + SEND_TIMEOUT_MS);
            }
        }
    }
    // In this state we are waiting for acknowledgement that the initial RTCP 
    // message was sent out.
    else if (_state == State::CONNECTING_0) {

        if (ev->getType() == SendEvent::TYPE) {

            auto evt = static_cast<const SendEvent*>(ev);
            if (evt->isGood()) {

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

                const uint16_t packetSize = 128;
                uint8_t packet[packetSize];
                uint32_t packetLen = formatOnDataPacket(buffer, _ssrc, packet, packetSize);

                _ctx->sendUDPChannel(_rtpChannel, packet, packetLen);

                // Transition into the state waiting for the RTP send to complete
                _state = State::CONNECTING_1;
                _setTimeoutMs(time_ms() + SEND_TIMEOUT_MS);

            } else {
                _userInfo->setStatus("Send failed 1");
                _state = FAILED;
            }
        }
        else if (_isTimedOut()) {
            _userInfo->setStatus("Send timed out 1");
            _state = FAILED;
        }
    }

    // In this state we are waiting for acknowledgement that the initial RTP 
    // message was sent out.
    else if (_state == State::CONNECTING_1) {
        if (ev->getType() == SendEvent::TYPE) {
            auto evt = static_cast<const SendEvent*>(ev);
            if (evt->isGood()) {
                // Transition into the state waiting for the first RTCP 
                // message to come back.
                _state = State::CONNECTING;
                _setTimeoutMs(time_ms() + REMOTE_TIMEOUT_MS);
                _userInfo->setStatus(WAITING_FOR_RESPONSE_MSG);
            } else {
                _userInfo->setStatus("Send failed 2");
                _state = FAILED;
            }
        }
        else if (_isTimedOut()) {
            _userInfo->setStatus("Send timed out 2");
            _state = FAILED;
        }
    }

    // It's possible to receive an oNDATA packet even after we have 
    // detected success. Pull it in an process it normally.
    else if (_state == SUCCEEDED) {
        if (ev->getType() == UDPReceiveEvent::TYPE) {
            const UDPReceiveEvent* evt = (UDPReceiveEvent*)ev;
            if (evt->getChannel() == _rtpChannel) {
                if (isOnDataPacket(evt->getData(), evt->getDataLen())) {
                    // Make sure the message is null-terminated one way or the other
                    char temp[64];
                    memcpyLimited((uint8_t*)temp, evt->getData(), evt->getDataLen(), 63);
                    temp[std::min((uint32_t)63, evt->getDataLen())] = 0;
                    // Here we skip past the oNDATA\r part when we report the message
                    _userInfo->setOnData(temp + 7);
                }
            }
        }
    }
}

bool QSOConnectMachine::isDone() const {
    return _state == FAILED || _state == SUCCEEDED;
}

bool QSOConnectMachine::isGood() const {
    return _state == SUCCEEDED;
}

}
