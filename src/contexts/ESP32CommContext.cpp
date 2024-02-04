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
#include <cassert>
#include <iostream>
#include <algorithm>

#include "kc1fsz-tools/events/DNSLookupEvent.h"
#include "kc1fsz-tools/events/TCPConnectEvent.h"
#include "kc1fsz-tools/events/TCPDisconnectEvent.h"
#include "kc1fsz-tools/events/TCPReceiveEvent.h"
#include "kc1fsz-tools/events/UDPReceiveEvent.h"
#include "kc1fsz-tools/events/SendEvent.h"
#include "kc1fsz-tools/Common.h"
#include "kc1fsz-tools/EventProcessor.h"
#include "kc1fsz-tools/AsyncChannel.h"

#include "../common.h"

#include "ESP32CommContext.h"

using namespace std;

namespace kc1fsz {

ESP32CommContext::ESP32CommContext(AsyncChannel* esp32) 
:   _state(State::NONE),
    _esp32(esp32),
    _respProc(this),
    _okIgnores(0) {
}

void ESP32CommContext::setEventProcessor(EventProcessor* ep) {
    _eventProc = ep;
}

uint32_t ESP32CommContext::flush(uint32_t ms) {
    uint32_t start = time_ms();
    const uint32_t bufSize = 256;
    uint8_t buf[bufSize];
    uint32_t total = 0;
    while ((time_ms() - start) < ms) {
        if (_esp32->isReadable()) {
            total += _esp32->read(buf, bufSize);
        }
    }
    return total;
}

bool ESP32CommContext::poll() {

    bool anythingHappened = false;

    anythingHappened = _esp32->poll();

    // Bridge inbound data from the ESP32 into the AT response
    // processor
    if (_esp32->isReadable()) {
        const uint32_t bufSize = 256;
        uint8_t buf[bufSize];
        uint32_t bufLen = _esp32->read(buf, bufSize);
        //cout << "ESP32CommContext GOT:" << endl;
        //prettyHexDump(buf, bufLen, cout);
        _respProc.process(buf, bufLen);
        anythingHappened = true;
    }

    return anythingHappened;
}

void ESP32CommContext::_cleanupTracker() {
}

int ESP32CommContext::getLiveChannelCount() const {
    return 0;
}

void ESP32CommContext::startDNSLookup(HostName hostName) {

    // Remember the name so we can generate an event later
    _lastHostNameReq = hostName;
    _state = State::IN_DNS;

    // Build the AT command
    char buf[64];
    sprintf(buf, "AT+CIPDOMAIN=\"%s\"\r\n", hostName.c_str());    
    _esp32->write((uint8_t*)buf, strlen(buf));
}

Channel ESP32CommContext::createTCPChannel() {
    for (int i = 0; i < 9; i++) {
        if (!_tracker[i].inUse) {
            _tracker[i].inUse = true;
            _tracker[i].type = ChannelTracker::Type::TYPE_TCP;
            _tracker[i].state = ChannelTracker::State::STATE_NONE;
            return Channel(i, true);
        }
    }
    return Channel(0, false);
}

void ESP32CommContext::closeTCPChannel(Channel c) {
    _closeChannel(c);
}

void ESP32CommContext::connectTCPChannel(Channel c, IPAddress ipAddr, uint32_t port) {

    if (!c.isGood()) {
        //panic("Bad channel");
        return;
    }

    char addr[32];
    formatIP4Address(ipAddr.getAddr(), addr, 32);

    _lastChannel = c;
    _state = State::IN_TCP_CONNECT;

    char buf[64];
    sprintf(buf, "AT+CIPSTART=%d,\"TCP\",\"%s\",%lu\r\n",
        c.getId(), addr, port);
    _esp32->write((uint8_t*)buf, strlen(buf));
}

void ESP32CommContext::sendTCPChannel(Channel c, const uint8_t* b, uint16_t len) {
    if (!c.isGood()) {
        return;
    }
    // Grab a copy of the the data
    if (len > _sendHoldSize) {
        // TODO: PANIC
        return;
    } 
    memcpyLimited(_sendHold, b, len, _sendHoldSize);
    _sendHoldLen = len;

    // Make the send request
    char buf[64];
    sprintf(buf, "AT+CIPSEND=%d,%d\r\n", c.getId(), len);
    _esp32->write((uint8_t*)buf, strlen(buf));

    // Now we wait for the prompt to tell us it's OK to send the data
    _state = State::IN_SEND_PROMPT_WAIT;
}

Channel ESP32CommContext::createUDPChannel(uint32_t localPort) {
    return Channel(0);
}

void ESP32CommContext::closeUDPChannel(Channel c) {  
    _closeChannel(c);
}

void ESP32CommContext::_closeChannel(Channel c) {  
}

void ESP32CommContext::sendUDPChannel(Channel c, IPAddress targetAddr, uint32_t targetPort, 
    const uint8_t* b, uint16_t len) {

    char buf[64];
    formatIP4Address(targetAddr.getAddr(), buf, 64);

    cout << "UDP Send to " << buf << ":" << targetPort << endl;
    prettyHexDump(b, len, cout);
}

void ESP32CommContext::ok() {

    if (_okIgnores > 0) {
        _okIgnores--;
        return;
    }

    if (_state == State::IN_DNS) {
        cout << "ESP32CommContext: OK (IN_DNS)" << endl;
        _state = State::NONE;
        DNSLookupEvent ev(_lastHostNameReq, _lastAddrResp);
        _eventProc->processEvent(&ev);
    } else if (_state == State::IN_TCP_CONNECT) {
        cout << "ESP32CommContext: OK (IN_TCP_CONNECT)" << endl;
        _state = State::NONE;
        TCPConnectEvent ev(_lastChannel);
        _eventProc->processEvent(&ev);
    }
    else {
        cout << "ESP32CommContext: OK (?)" << endl;
    }
}

void ESP32CommContext::error() {
    if (_state == State::IN_TCP_CONNECT) {
        cout << "ESP32CommContext: ERROR (IN_TCP_CONNECT)" << endl;
        _state = State::NONE;
        TCPConnectEvent ev(_lastChannel, false);
        _eventProc->processEvent(&ev);
    } else {
        cout << "ESP32CommContext: ERROR (?) " << _state << endl;
    }
}

void ESP32CommContext::sendPrompt() {
    if (_state == State::IN_SEND_PROMPT_WAIT) {
        // Send the actual data
        _esp32->write(_sendHold, _sendHoldLen);
        // TODO: ERROR CHECK
        _state = State::IN_SEND_OK_WAIT;
    }
}

void ESP32CommContext::sendOk() {
    if (_state == State::IN_SEND_OK_WAIT) {
        cout << "ESP32CommContext: SEND OK (IN_SEND_OK_WAIT)" << endl;
        _state = State::NONE;
        SendEvent ev(_lastChannel, true);
        _eventProc->processEvent(&ev);
    } 
    else {
        cout << "ESP32CommContext: SEND OK (?) " << _state << endl;
    }
}

void ESP32CommContext::domain(const char* addr) {
    // Here we grab the address so we are ready to generate the 
    // event when the OK comes in.  I am assuming it's possible
    // for multiple +CIPDOMAIN: messages to come by.
    if (_state == State::IN_DNS) {
        _lastAddrResp = IPAddress(parseIP4Address(addr));
    }
    // TODO: ERROR
}

void ESP32CommContext::connected(uint32_t channel) {
    cout << "ESP32CommContext::connected()" << endl;
    // Not doing anything with this
}

void ESP32CommContext::closed(uint32_t channel) {
    cout << "ESP32CommContext::close()" << endl;
    if (channel < 9) {
        if (_tracker[channel].inUse) {
            if (_tracker[channel].type == ChannelTracker::Type::TYPE_TCP) {
                _tracker[channel].inUse = false;
                // Create an event and forward
                TCPDisconnectEvent ev(Channel(channel, true));
                _eventProc->processEvent(&ev);
            } 
        }
        else {
            // PANIC
        }
    }
    else {
        // PANIC
    }
}

void ESP32CommContext::ipd(uint32_t channel, uint32_t chunk,
    const uint8_t* data, uint32_t len) {   
    if (len > 256) {
        cout << "Length error!" << endl;
        // PANIC
        return;
    }
    if (channel < 9) {
        if (_tracker[channel].inUse) {
            if (_tracker[channel].type == ChannelTracker::Type::TYPE_TCP) {
                TCPReceiveEvent ev(Channel(channel), data, len);
                _eventProc->processEvent(&ev);
            }    
        }
    }
}

}

