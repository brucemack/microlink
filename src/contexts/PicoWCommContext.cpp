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

#include "pico/time.h"
#include "hardware/gpio.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"
                                 
#include "kc1fsz-tools/Log.h"
#include "kc1fsz-tools/Common.h"
#include "kc1fsz-tools/EventProcessor.h"
#include "kc1fsz-tools/AsyncChannel.h"
#include "kc1fsz-tools/events/DNSLookupEvent.h"
#include "kc1fsz-tools/events/TCPConnectEvent.h"
#include "kc1fsz-tools/events/ChannelSetupEvent.h"
#include "kc1fsz-tools/events/TCPDisconnectEvent.h"
#include "kc1fsz-tools/events/TCPReceiveEvent.h"
#include "kc1fsz-tools/events/UDPReceiveEvent.h"
#include "kc1fsz-tools/events/SendEvent.h"
#include "kc1fsz-tools/events/StatusEvent.h"

#include "../common.h"

#include "PicoWCommContext.h"

using namespace std;

namespace kc1fsz {

int PicoWCommContext::traceLevel = 0;

PicoWCommContext::PicoWCommContext(Log* log) 
:   _log(log),
    _state(State::NONE) {
}

/**
    * Used to receive and discard anything on the channel
    * @returns The number of bytes discarded.
*/
uint32_t PicoWCommContext::flush(uint32_t ms) {
    return 0;
}

int PicoWCommContext::getLiveChannelCount() const { 
    return 0;
}

bool PicoWCommContext::test() { 
    return true;
}

// ----- Runnable Methods ------------------------------------------------

/**
    * This should be called from the event loop.  It attempts to make forward
    * progress and passes all events to the event processor.
    * 
    * @returns true if any events were dispatched.
*/
bool PicoWCommContext::run() {
    return true;
}

// ------ CommContext Request Methods ------------------------------------- 

void PicoWCommContext::reset() {
}

void PicoWCommContext::startDNSLookup(HostName hostName) {
}

Channel PicoWCommContext::createTCPChannel() {
    return Channel();
}

void PicoWCommContext::closeTCPChannel(Channel c) {

}

void PicoWCommContext::connectTCPChannel(Channel c, IPAddress ipAddr, uint32_t port) {
}

void PicoWCommContext::sendTCPChannel(Channel c, const uint8_t* b, uint16_t len) {    
}

Channel PicoWCommContext::createUDPChannel() {
    return Channel();
}

void PicoWCommContext::closeUDPChannel(Channel c) {
}

void PicoWCommContext::setupUDPChannel(Channel c, uint32_t localPort, 
    IPAddress remoteIpAddr, uint32_t remotePort) {
}

void PicoWCommContext::sendUDPChannel(Channel c, const uint8_t* b, uint16_t len) {
}

void PicoWCommContext::sendUDPChannel(Channel c, IPAddress remoteIpAddr, uint32_t remotePort,
    const uint8_t* b, uint16_t len) {
}

}
