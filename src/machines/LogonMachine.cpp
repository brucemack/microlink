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
#include <sys/time.h>
#include <iostream>
#include <cstring>

#include "../FixedString.h"

#include "../events/DNSLookupEvent.h"
#include "../events/TCPConnectEvent.h"
#include "../events/TCPDisconnectEvent.h"
#include "../events/TCPReceiveEvent.h"

#include "LogonMachine.h"

using namespace std;

namespace kc1fsz {

LogonMachine::LogonMachine() 
:   _state(IDLE),
    _logonRespPtr(0) {
}

void LogonMachine::start(Context* ctx) {
    // Launch the DNS resolution process
    ctx->startDNSLookup(_serverHostName);
    // We give the lookup 5 seconds to complete
    _setTimeoutMs(ctx->getTimeMs() + 5000);
    _state = DNS_WAIT;
}

void LogonMachine::processEvent(const Event* ev, Context* ctx) {
    // In this state we are doing nothing waiting to be started
    if (_state == IDLE) {
    }
    // In this state we are waiting for the DNS resolution to complete
    else if (_state == DNS_WAIT) {
        // Look for good completion
        if (ev->getType() == DNSLookupEvent::TYPE) {
            const DNSLookupEvent* evt = (DNSLookupEvent*)ev;
            // Start the process of opening the TCP connection
            _channel = ctx->createTCPChannel();
            ctx->connectTCPChannel(_channel, evt->addr);
            // We give the connect 1 second to complete
            _setTimeoutMs(ctx->getTimeMs() + 1000);
            _state = CONNECTING;
        }
        else if (_isTimedOut(ctx)) {
            _state = FAILED;
        }
    }
    else if (_state == CONNECTING) {
        if (ev->getType() == TCPConnectEvent::TYPE) {
            const TCPConnectEvent* evt = (TCPConnectEvent*)ev;
            // Grab the channel that is connected
            _channel = evt->getChannel();
            // Build the logon message
            uint8_t buf[256];
            uint32_t bufLen = createOnlineMessage(buf, 256, _callSign, _password, _location);
            ctx->sendTCPChannel(_channel, buf, bufLen);
            // We give the logon 10 seconds to complete
            _setTimeoutMs(ctx->getTimeMs() + 10000);
            _logonRespPtr = 0;
            _state = WAITING_FOR_DISCONNECT;            
        } 
        else if (_isTimedOut(ctx)) {
            _state = FAILED;
        }
    }
    else if (_state == WAITING_FOR_DISCONNECT) {
        // If we get data then accept it
        if (ev->getType() == TCPReceiveEvent::TYPE) {
            const TCPReceiveEvent* evt = (TCPReceiveEvent*)ev;
            //prettyHexDump(evt->getData(), evt->getDataLen(), cout);
            // Accumulate the data (or as much as possible)
            uint32_t spaceLeft = _logonRespSize - _logonRespPtr;
            uint32_t l = std::min(spaceLeft, evt->getDataLen());
            memcpyLimited(_logonResp + _logonRespPtr, evt->getData(), l, spaceLeft);
            _logonRespPtr += l;
        }
        // If we get a disconnect then move forward
        else if (ev->getType() == TCPDisconnectEvent::TYPE) {
            const TCPConnectEvent* evt = (TCPConnectEvent*)ev;
            if (evt->getChannel() == _channel) {
                // Parse the response to make sure we got what we expected
                if (_logonRespPtr >= 1 && _logonResp[0] == 'O' && _logonResp[1] == 'K') {
                    _state = SUCCEEDED;
                } else {
                    // TODO: MESSAGE
                    _state = FAILED;
                }
            } else {
                // TODO: MESSAGE
                _state = FAILED;
            }
        }
        else if (_isTimedOut(ctx)) {
            // TODO: NEED MESSAGE
            _state = FAILED;
        }
    }
}

bool LogonMachine::isDone() const {
    return _state == FAILED || _state == SUCCEEDED;
}

bool LogonMachine::isGood() const {
    return _state == SUCCEEDED;
}

uint32_t createOnlineMessage(uint8_t* buf, uint32_t bufLen,
    CallSign cs, FixedString pwd, FixedString loc) {

    uint8_t* p = buf;

    // TODO: MOVE TO CONTEXT
    time_t t = time(0);
    struct tm tm;
    char local_time_str[6];
    strftime(local_time_str, 6, "%H:%M", localtime_r(&t, &tm));

    (*p++) = 'l';
    memcpy(p, cs.c_str(), cs.len());
    p += cs.len();
    (*p++) = 0xac;
    (*p++) = 0xac;
    memcpy(p, pwd.c_str(), pwd.len());
    p += pwd.len();
    (*p++) = 0x0d;
    memcpy(p, "ONLINE", 10);
    p += 6;
    memcpy(p, VERSION_ID, strlen(VERSION_ID));
    p += strlen(VERSION_ID);
    (*p++) = '(';
    memcpy(p, local_time_str, 5);
    p += 5;
    (*p++) = ')';
    (*p++) = 0x0d;
    memcpy(p, loc.c_str(), loc.len());
    p += loc.len();
    (*p++) = 0x0d;

    return (p - buf);
}

}

