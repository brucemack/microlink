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
#include "../events/DNSLookupEvent.h"
#include "LogonMachine.h"

namespace kc1fsz {

LogonMachine::LogonMachine() 
:   _state(IDLE) {
}

void LogonMachine::start(Context* ctx) {
    // Launch the DNS resolution process
    ctx->startDNSLookup(_hostName);
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
            ctx->startTCPConnect(_channel, evt->addr);
            // We give the connect 1 second to complete
            _setTimeoutMs(ctx->getTimeMs() + 1000);
            _state = CONNECTING;
        }
        else if (_isTimedOut(ctx)) {
            _state = FAILED;
        }
    }
}

void LogonMachine::setServerName(HostName h) {
    _hostName = h;
}
bool LogonMachine::isDone() const {
    return _state == FAILED || _state == SUCCEEDED;
}

bool LogonMachine::isGood() const {
    return _state == SUCCEEDED;
}

}

