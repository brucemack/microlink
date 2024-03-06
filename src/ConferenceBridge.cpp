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

#include "kc1fsz-tools/Common.h"
#include "kc1fsz-tools/CallSign.h"
#include "kc1fsz-tools/events/UDPReceiveEvent.h"
#include "kc1fsz-tools/events/TickEvent.h"
#include "kc1fsz-tools/events/SendEvent.h"
#include "kc1fsz-tools/events/ChannelSetupEvent.h"

#include "common.h"
#include "UserInfo.h"
#include "Conference.h"
#include "ConferenceBridge.h"

using namespace std;

namespace kc1fsz {

static const uint32_t RTP_PORT = 5198;
static const uint32_t RTCP_PORT = 5199;

static const uint32_t CHANNEL_SETUP_TIMEOUT_MS = 250;
static const uint32_t SEND_TIMEOUT_MS = 1000;

static const char* FAILED_MSG = "Station connection failed";

static TickEvent tickEv;

int ConferenceBridge::traceLevel = 0;

ConferenceBridge::ConferenceBridge(CommContext* ctx, UserInfo* userInfo, Conference* conf)
:   _ctx(ctx),
    _userInfo(userInfo),
    _conf(conf) {   
}

void ConferenceBridge::start() {  

    // Get UDP connections created
    _rtcpChannel = _ctx->createUDPChannel();
    _rtpChannel = _ctx->createUDPChannel();

    // A dummy address/port
    IPAddress remoteAddr(0x44444444);
    uint32_t remotePort = 9999;

    // Start the RTCP socket setup (target address)
    _ctx->setupUDPChannel(_rtcpChannel, RTCP_PORT, remoteAddr, remotePort);
    _state = State::IN_SETUP_1;
    _setTimeoutMs(time_ms() + CHANNEL_SETUP_TIMEOUT_MS);
}

void ConferenceBridge::processEvent(const Event* ev) {

    if (traceLevel > 0) {
        if (ev->getType() != TickEvent::TYPE) {
            cout << "ConferenceBridge state=" << _state << " event=" << ev->getType() <<  endl;
        }
    }

    // In this state we are waiting for confirmation that the RTCP 
    // socket was setup.
    if (_state == State::IN_SETUP_1) {
        if (ev->getType() == ChannelSetupEvent::TYPE) {
            auto evt = static_cast<const ChannelSetupEvent*>(ev);
            if (evt->isGood()) {
                // A dummy address/port
                IPAddress remoteAddr(0x44444444);
                uint32_t remotePort = 9999;
                // Start the RTP socket setup
                _ctx->setupUDPChannel(_rtpChannel, RTP_PORT, remoteAddr, remotePort);
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
                _userInfo->setStatus("Ready to receive");
                _state = State::WAITING;  
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
    // In this state we are waiting for messages.
    else if (_state == WAITING) {        
        if (ev->getType() == UDPReceiveEvent::TYPE) {

            const UDPReceiveEvent* evt = (UDPReceiveEvent*)ev;

            if (evt->getChannel() == _rtcpChannel) {

                if (traceLevel > 0) {
                    cout << "ConferenceBridge: GOT RTCP DATA" << endl;
                    prettyHexDump(evt->getData(), evt->getDataLen(), cout);
                }

                _conf->processText(evt->getAddress(),
                    evt->getData(), evt->getDataLen());
            } 
            else if (evt->getChannel() == _rtpChannel) {

                if (traceLevel > 0) {
                    cout << "ConferenceBridge: GOT RTP DATA" << endl;
                    prettyHexDump(evt->getData(), evt->getDataLen(), cout);
                }

                if (isOnDataPacket(evt->getData(), evt->getDataLen())) {
                    _conf->processText(evt->getAddress(),
                        evt->getData(), evt->getDataLen());
                }
                else {
                    // TODO - PARSE OUT AUDIO PACKET AND SSRC
                    _conf->processAudio(evt->getAddress(),
                        0,
                        evt->getData(), evt->getDataLen(), AudioFormat::GSMFR4X);
                }
            }
        }
    }
}

bool ConferenceBridge::isDone() const {
    return _state == FAILED || _state == SUCCEEDED;
}

bool ConferenceBridge::isGood() const {
    return _state == SUCCEEDED;
}

}
