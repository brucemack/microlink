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
#include <iostream>

#include "kc1fsz-tools/CommContext.h"

#include "kc1fsz-tools/FixedString.h"
#include "kc1fsz-tools/events/TickEvent.h"
#include "kc1fsz-tools/events/DNSLookupEvent.h"
#include "kc1fsz-tools/events/TCPConnectEvent.h"
#include "kc1fsz-tools/events/TCPDisconnectEvent.h"
#include "kc1fsz-tools/events/TCPReceiveEvent.h"

#include "UserInfo.h"

#include "machines/ValidationMachine.h"

using namespace std;

namespace kc1fsz {

static const uint32_t CONNECT_TIMEOUT_MS = 2000;
static const uint32_t DNS_TIMEOUT_MS = 5000;
static const uint32_t VERIFY_TIMEOUT_MS = 5000;
static const char* SUCCESSFUL_MSG = "Lookup successful";
static const char* UNSUCCESSFUL_MSG = "Callsign not valid or not online";

int ValidationMachine::traceLevel = 0;

ValidationMachine::ValidationMachine(CommContext* ctx, UserInfo* userInfo)
:   _state(IDLE),
    _ctx(ctx),
    _userInfo(userInfo) {
}

void ValidationMachine::start() {
    _channel = Channel(0, false);
    _isValid = false;
    // Launch the DNS resolution process
    _ctx->startDNSLookup(_serverHostName);
    _setTimeoutMs(time_ms() + DNS_TIMEOUT_MS);
    _state = DNS_WAIT;
}

void ValidationMachine::cleanup() {    
    _ctx->closeTCPChannel(_channel);
}

void ValidationMachine::processEvent(const Event* ev) {

    if (traceLevel > 0) {
        if (ev->getType() != TickEvent::TYPE) {
            cout << "ValidationMachine: state=" << _state << " event=" << ev->getType() << endl;
        }
    }

    // In this state we are waiting for the DNS resolution to complete
    if (_state == DNS_WAIT) {
        // Look for good completion
        if (ev->getType() == DNSLookupEvent::TYPE) {
            auto evt = static_cast<const DNSLookupEvent*>(ev);
            // Start the process of opening the TCP connection
            _channel = _ctx->createTCPChannel();
            if (!_channel.isGood()) {
                _state = FAILED;
            } else {
                _ctx->connectTCPChannel(_channel, evt->getAddr(), _serverPort);
                // We give the connect some time to complete
                _setTimeoutMs(time_ms() + CONNECT_TIMEOUT_MS);
                _state = CONNECTING;
            }
        }
        else if (_isTimedOut()) {
            _state = FAILED;
        }
    }
    // In this state we are waiting to connect to the EL Server
    else if (_state == CONNECTING) {
        if (ev->getType() == TCPConnectEvent::TYPE) {
            auto evt = static_cast<const TCPConnectEvent*>(ev);
            // Grab the channel that is connected
            _channel = evt->getChannel();

            char addr[32];
            formatIP4Address(_requestAddr.getAddr(), addr, 32);

            // Send the directory verify message
            char buf[64];
            snprintf(buf, 64, "v%s\n%s\n", _requestCallSign.c_str(), addr);

            _ctx->sendTCPChannel(_channel, (const uint8_t*)buf, strlen(buf));
            _setTimeoutMs(time_ms() + VERIFY_TIMEOUT_MS);
            _state = WAITING_FOR_DISCONNECT;            
        } 
        else if (_isTimedOut()) {
            _state = FAILED;
        }
    }
    // In this state we are waiting for the EL Server to drop
    else if (_state == WAITING_FOR_DISCONNECT) {

        // If we get data then accept it into the _saveArea
        if (ev->getType() == TCPReceiveEvent::TYPE) {
            auto evt = static_cast<const TCPReceiveEvent*>(ev);
            if (evt->getDataLen() > 0) {
                // A "1" means valid
                if (evt->getData()[0] == 0x31) {
                    _isValid = true;
                }
            }
        }
        // If we get a disconnect then look at the response and attempt to 
        // extract the IP address
        else if (ev->getType() == TCPDisconnectEvent::TYPE) {
            auto evt = static_cast<const TCPDisconnectEvent*>(ev);
            // Make sure it's the right channel, otherwise ignore
            if (evt->getChannel() == _channel) {
                if (_isValid) {
                    _userInfo->setStatus(SUCCESSFUL_MSG);
                    _state = State::SUCCEEDED;
                } else {
                    _state = State::FAILED;
                }
            }
        }
        else if (_isTimedOut()) {
            _userInfo->setStatus(UNSUCCESSFUL_MSG);
            _state = State::FAILED;
        }
    }
}

bool ValidationMachine::isDone() const {
    return _state == FAILED || _state == SUCCEEDED;
}

bool ValidationMachine::isGood() const {
    return _state == SUCCEEDED;
}

}
