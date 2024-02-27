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
#include <cstring>
#include <algorithm>

#include "kc1fsz-tools/CommContext.h"

#include "kc1fsz-tools/FixedString.h"

#include "kc1fsz-tools/events/TickEvent.h"
#include "kc1fsz-tools/events/DNSLookupEvent.h"
#include "kc1fsz-tools/events/TCPConnectEvent.h"
#include "kc1fsz-tools/events/TCPDisconnectEvent.h"
#include "kc1fsz-tools/events/TCPReceiveEvent.h"

#include "UserInfo.h"

#include "LookupMachine2.h"

using namespace std;

namespace kc1fsz {

static const uint32_t CONNECT_TIMEOUT_MS = 2000;
static const uint32_t DNS_TIMEOUT_MS = 5000;
static const uint32_t VERIFY_TIMEOUT_MS = 5000;

static const char* SUCCESSFUL_MSG = "Lookup successful";
static const char* UNSUCCESSFUL_MSG = "Callsign not valid or not online";

int LookupMachine2::traceLevel = 0;

LookupMachine2::LookupMachine2(CommContext* ctx, UserInfo* userInfo)
:   _ctx(ctx),
    _userInfo(userInfo),
    _serverPort(0) { 
}

void LookupMachine2::start() {
    _channel = Channel(0, false);
    _targetAddr = 0;
    _saveAreaUsed = 0;
    // Launch the DNS resolution process
    _ctx->startDNSLookup(_serverHostName);
    _setTimeoutMs(time_ms() + DNS_TIMEOUT_MS);
    _state = DNS_WAIT;
}

void LookupMachine2::cleanup() {
    _ctx->closeTCPChannel(_channel);
}

void LookupMachine2::processEvent(const Event* ev) {

    if (traceLevel > 0) {
        if (ev->getType() != TickEvent::TYPE) {
            cout << "LookupMachine2: state=" << _state << " event=" << ev->getType() << endl;
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
            // Send the directory verify message
            char buf[64];
            // NOTE: This one is a capital "V"
            sprintf(buf, "V%s\n", _targetCallSign.c_str());
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
            memcpyLimited(_saveArea + _saveAreaUsed, evt->getData(), evt->getDataLen(),
                _saveAreaSize - _saveAreaUsed);
            _saveAreaUsed +=  evt->getDataLen();
        }
        // If we get a disconnect then look at the response and attempt to 
        // extract the IP address
        else if (ev->getType() == TCPDisconnectEvent::TYPE) {

            auto evt = static_cast<const TCPDisconnectEvent*>(ev);

            // Make sure it's the right channel, otherwise ignore
            if (evt->getChannel() == _channel) {

                // Hunt for the delimiters in the _saveArea
                uint16_t delimCount = 0;
                uint16_t delimPoints[4];

                for (uint32_t i = 0; i < _saveAreaUsed; i++) {                            
                    if (_saveArea[i] == 0x0a) {
                        delimPoints[delimCount++] = i;
                    }
                }

                if (delimCount >= 4) {
                    // Make sure the IP address is under 31 character to avoid 
                    // any overflows.
                    if ((delimPoints[3] - delimPoints[2]) <= 31) {
                        char ipAddr[32];
                        memcpyLimited((uint8_t*)ipAddr, 
                            _saveArea + delimPoints[2] + 1,
                            delimPoints[3] - delimPoints[2], 31);
                        ipAddr[delimPoints[3] - delimPoints[2]] = 0;

                        // In network byte order!
                        _targetAddr = parseIP4Address(ipAddr);
                        _userInfo->setStatus(SUCCESSFUL_MSG);
                        _state = SUCCEEDED;
                    } else {
                        _userInfo->setStatus(UNSUCCESSFUL_MSG);                        
                        _state = FAILED;
                    }
                }
                else {
                    _userInfo->setStatus(UNSUCCESSFUL_MSG);
                    _state = FAILED;
                }
            }
        }
        else if (_isTimedOut()) {
            _userInfo->setStatus(UNSUCCESSFUL_MSG);
            _state = FAILED;
        }
    }
}

bool LookupMachine2::isDone() const {
    return _state == FAILED || _state == SUCCEEDED;
}

bool LookupMachine2::isGood() const {
    return _state == SUCCEEDED;
}

}
