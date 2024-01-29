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
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>

#include "../common.h"
#include "../EventProcessor.h"
#include "../events/DNSLookupEvent.h"

#include "SocketContext.h"

using namespace  std;

namespace kc1fsz {

SocketContext::SocketContext()
:   _dnsResultPending(false) {
}

void SocketContext::poll(EventProcessor* ep) {
    // Check for any pending DNS results (making a synchronous event look
    // asynchronous).
    if (_dnsResultPending) {
        _dnsResultPending = false;
        DNSLookupEvent ev(_dnsResult);
        ep->processEvent(&ev);
    }
}

uint32_t SocketContext::getTimeMs() {
    return time_ms();
}

void SocketContext::startDNSLookup(HostName hostName) {
    // TODO: UPGRADE FOR IP6
    const struct hostent* remoteHost = gethostbyname(hostName.c_str());
    if (remoteHost->h_addrtype == AF_INET && remoteHost->h_length == 4) {
        uint32_t addr = ntohl(*(uint32_t*)remoteHost->h_addr_list[0]);
        _dnsResultPending = true;
        _dnsResult = IPAddress(addr);
    }
}

Channel SocketContext::createTCPChannel() {
    cout << "STOPPING" << endl;
    exit(0);
    return Channel(0);
}

void SocketContext::closeTCPChannel(Channel c) {
    // The channel ID maps directly to the fd
    close(c.getId());
}

void SocketContext::connectTCPChannel(Channel c, IPAddress ipAddr) {
}

void SocketContext::sendTCPChannel(Channel c, const uint8_t* b, uint16_t len) {
}

Channel SocketContext::createUDPChannel(uint32_t localPort) {
    return Channel(0);
}

void SocketContext::closeUDPChannel(Channel c) {  
    // The channel ID maps directly to the fd
    close(c.getId());
}

void SocketContext::sendUDPChannel(Channel c, IPAddress targetAddr, uint32_t targetPort, 
    const uint8_t* b, uint16_t len) {

}

}

