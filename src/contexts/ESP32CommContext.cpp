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
#include "kc1fsz-tools/Common.h"
#include "kc1fsz-tools/EventProcessor.h"
#include "kc1fsz-tools/AsyncChannel.h"

#include "../common.h"

#include "ESP32CommContext.h"

using namespace std;

namespace kc1fsz {

ESP32CommContext::ESP32CommContext(AsyncChannel* esp32) 
:   _esp32(esp32),
    _respProc(this) {
}

void ESP32CommContext::setEventProcessor(EventProcessor* ep) {
    _eventProc = ep;
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
        cout << "GOT:" << endl;
        prettyHexDump(buf, bufLen, cout);
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
    // Remember the name so we can generate an event
    _lastHostNameRequest = hostName;
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
        return;
    }
    char addr[32];
    formatIP4Address(ipAddr.getAddr(), addr, 32);
    char buf[64];
    sprintf(buf, "AT+CIPSTART=%d,\"TCP\",\"%s\",%lu\r\n",
        c.getId(), addr, port);
    cout << "Connect: " << buf << endl;
    _esp32->write((uint8_t*)buf, strlen(buf));
}

void ESP32CommContext::sendTCPChannel(Channel c, const uint8_t* b, uint16_t len) {
    if (c.isGood()) {
    }
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
    cout << "GOT OK!" << endl;
}

void ESP32CommContext::domain(const char* addr) {
    cout << "DOMAIN: " << addr << endl;
    // Create an event and forward
    IPAddress a(parseIP4Address(addr));
    DNSLookupEvent ev(_lastHostNameRequest, a);
    _eventProc->processEvent(&ev);
}


}

