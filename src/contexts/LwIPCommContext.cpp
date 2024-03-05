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
#include "../BooleanHolder.h"

#include "LwIPCommContext.h"

using namespace std;

namespace kc1fsz {

int LwIPCommContext::traceLevel = 0;

LwIPCommContext::LwIPCommContext(Log* log) 
:   _log(log),
    _state(State::NONE) {
}

/**
    * Used to receive and discard anything on the channel
    * @returns The number of bytes discarded.
*/
uint32_t LwIPCommContext::flush(uint32_t ms) {
    return 0;
}

int LwIPCommContext::getLiveChannelCount() const { 
    return 0;
}

bool LwIPCommContext::test() { 
    return true;
}

// ----- Runnable Methods ------------------------------------------------

/**
    * This should be called from the event loop.  It attempts to make forward
    * progress and passes all events to the event processor.
    * 
    * @returns true if any events were dispatched.
*/
bool LwIPCommContext::run() {

    if (_state == State::IN_RESET) {
        _state = State::NONE;
        StatusEvent ev;
        _eventProc->processEvent(&ev);
    }

    if (_dnsRespPending) {
        _dnsRespPending = false;
        DNSLookupEvent ev(_lastHostNameReq, _lastAddrResp);
        _eventProc->processEvent(&ev);
    }

    if (_setupRespPending) {
        _setupRespPending = false;
        // Create an event and forward
        ChannelSetupEvent ev(_lastChannel, true);
        _eventProc->processEvent(&ev);
    }

    if (_sendRespPending) {
        _sendRespPending = false;
        SendEvent ev(_lastChannel, true);
        _eventProc->processEvent(&ev);
    }

    return true;
}

// ------ CommContext Request Methods ------------------------------------- 

void LwIPCommContext::reset() {
    _state = State::IN_RESET;
}

void LwIPCommContext::startDNSLookup(HostName hostName) {

    if (traceLevel)
        _log->info("DNS request for %s", hostName.c_str());

    _lastHostNameReq = hostName;
    ip_addr_t addr;
    err_t e = dns_gethostbyname(hostName.c_str(), &addr, _dnsCb, this);
    // This is the case where the response is immediately avaialble
    if (e == ERR_OK) {
        _dnsRespPending = true;        
        _lastAddrResp = IPAddress(ntohl(addr.addr));
        _log->info("Fast DNS");
    }
    else if (e == ERR_ARG) {
        panic("Invalid DNS request");
    } else {
        // Otherwise, expect a callback-later
        _log->info("Slow DNS");
    }
}

void LwIPCommContext::_dnsCb(const char* name, const ip_addr_t* ipaddr, void* arg) {   
    static_cast<LwIPCommContext*>(arg)->_dns(name, ipaddr);
}

void LwIPCommContext::_dns(const char* name, const ip_addr_t* ipaddr) {   

    BooleanHolder h(&_inCallback);

    uint32_t addr = ntohl(ipaddr->addr);

    if (traceLevel > 0) {
        char addrStr[20];
        formatIP4Address(addr, addrStr, 20);
        _log->info("DNS Result %s", addrStr);
    }

    HostName hn(name);
    IPAddress ad(addr);
    DNSLookupEvent ev(hn, ad);
    _eventProc->processEvent(&ev);
}

err_t LwIPCommContext::_tcpRecvCb(void *arg, tcp_pcb *tpcb, pbuf *p, err_t err) {
    return static_cast<LwIPCommContext*>(arg)->_tcpRecv(tpcb, p, err);
}

err_t LwIPCommContext::_tcpRecv(tcp_pcb *tpcb, pbuf *p, err_t err) {

    BooleanHolder h(&_inCallback);

    const int i = _findTracker2(tpcb);
    // Look for disconnect
    if (p == 0) {
        if (i != -1) {
            TCPDisconnectEvent ev(Channel(i, true));
            _eventProc->processEvent(&ev);
        } else {
            _log->error("Socket not found");
        }
    } else {
        if (i != -1) {
            TCPReceiveEvent ev(Channel(i, true), (const uint8_t*)p->payload, p->len);
            _eventProc->processEvent(&ev);
        } else {
            _log->error("Socket not found");
        }
        pbuf_free(p);
    }
    return ERR_OK;
}

err_t LwIPCommContext::_tcpSentCb(void *arg, tcp_pcb *tpcb, u16_t len) {
    return ERR_OK;
}

void LwIPCommContext::_errCb(void *arg, err_t err) {
    //_log->error("_errCb");
}

err_t LwIPCommContext::_tcpConnectCb(void *arg, tcp_pcb* pcb, err_t err) {
    return static_cast<LwIPCommContext*>(arg)->_tcpConnect(pcb, err);
}

err_t LwIPCommContext::_tcpConnect(tcp_pcb* pcb, err_t err) {

    BooleanHolder h(&_inCallback);

    for (uint32_t i = 0; i < _trackersSize; i++) {
        if (_trackers[i].inUse == true &&
            _trackers[i].state == Tracker::State::IN_CONNECT &&
            _trackers[i].pcb == pcb) {
            _trackers[i].state = Tracker::State::IDLE;
            TCPConnectEvent ev(Channel(i, true));
            _eventProc->processEvent(&ev);
            return ERR_OK;
        }
    }
    _log->error("_connect on invalid socket");
    return ERR_OK;
}

Channel LwIPCommContext::createTCPChannel() {
    // Find free tracker and allocate
    for (uint32_t i = 0; i < _trackersSize; i++) {
        if (!_trackers[i].inUse) {
            _trackers[i].inUse = true;
            _trackers[i].type = Tracker::Type::TCP;
            _trackers[i].state = Tracker::State::IDLE;
            tcp_pcb* pcb = tcp_new();
            _trackers[i].pcb = pcb;
            // Setup callbacks
            tcp_arg(pcb, this);
            tcp_recv(pcb, _tcpRecvCb);
            tcp_sent(pcb, _tcpSentCb);
            tcp_err(pcb, _errCb);

            _dumpChannels();

            return Channel(i, true);
        }
    }
    panic("No chanels available");
    return Channel(0, false);
}

void LwIPCommContext::_validateChannel(Channel c, Tracker::Type t) const {
    if (c.getId() >= (int)_trackersSize ||
        !_trackers[c.getId()].inUse ||
        _trackers[c.getId()].type != t) {
        panic("Invalid channel");
    }
}

LwIPCommContext::Tracker* LwIPCommContext::_findTracker(void* pcb) {
    for (uint32_t i = 0; i < _trackersSize; i++) {
        if (_trackers[i].inUse == true &&
            _trackers[i].pcb == pcb) {
            return &(_trackers[i]);
        }
    }
    return 0;
}

int LwIPCommContext::_findTracker2(void* pcb) {
    for (int i = 0; i < (int)_trackersSize; i++) {
        if (_trackers[i].inUse == true &&
            _trackers[i].pcb == pcb) {
            return i;
        }
    }
    return -1;
}

void LwIPCommContext::closeTCPChannel(Channel c) {

    _validateChannel(c, Tracker::Type::TCP);

    err_t e = tcp_close(static_cast<tcp_pcb*>(_trackers[c.getId()].pcb));
    if (e == ERR_OK) {
        _trackers[c.getId()].inUse = false;
    } 
    else {
        _trackers[c.getId()].error = true;
        panic("Unable to close");
    }

    _dumpChannels();
}

void LwIPCommContext::connectTCPChannel(Channel c, IPAddress ipAddr, uint32_t port) {

    _validateChannel(c, Tracker::Type::TCP);

    char addrStr[20];
    formatIP4Address(ipAddr.getAddr(), addrStr, 20);

    if (traceLevel > 0)
        _log->info("Connecting to %s:%d", addrStr, port);

    // Here we are building an address from a string, so there will no issues
    // related to byte ordering.
    ip4_addr_t addr;
    ip4addr_aton(addrStr, &addr);

    err_t e = tcp_connect(static_cast<tcp_pcb*>(_trackers[c.getId()].pcb),
        &addr, port, _tcpConnectCb);
    if (e == ERR_OK) {
        _trackers[c.getId()].state = Tracker::State::IN_CONNECT;
    }
    else {
        _trackers[c.getId()].error = true;
        _log->error("Connection attempt failed");
        // TODO: NEED TO ARRANGE A FAILURE EVENT
    }
}

void LwIPCommContext::sendTCPChannel(Channel c, const uint8_t* b, uint16_t len) {    

    _validateChannel(c, Tracker::Type::TCP);

    const Tracker* t = &(_trackers[c.getId()]);
    err_t e = tcp_write(static_cast<tcp_pcb*>(t->pcb), b, len, TCP_WRITE_FLAG_COPY);
    if (e != ERR_OK) {
        _log->error("Write error %d", e);
    }
}

void LwIPCommContext::_udpRecvCb(void *arg, udp_pcb *upcb, pbuf *p, const ip_addr_t *addr, u16_t port) {
    static_cast<LwIPCommContext*>(arg)->_udpRecv(upcb, p, addr, port);
}

void LwIPCommContext::_udpRecv(udp_pcb* upcb, pbuf *p, const ip_addr_t *addr, u16_t port) {
    const int i = _findTracker2(upcb);
    if (traceLevel > 1) {
        _log->info("(RX) channel %d", i);
    }
    //prettyHexDump((const uint8_t*)p->payload, p->len, cout);
    IPAddress a(ntohl(addr->addr));
    UDPReceiveEvent ev(Channel(i, true), (const uint8_t*)p->payload, p->len, a);
    _eventProc->processEvent(&ev);
    pbuf_free(p);
}

Channel LwIPCommContext::createUDPChannel() {
    // Find free tracker and allocate
    for (uint32_t i = 0; i < _trackersSize; i++) {
        if (!_trackers[i].inUse) {
            _trackers[i].inUse = true;
            _trackers[i].type = Tracker::Type::UDP;
            _trackers[i].state = Tracker::State::IDLE;
            udp_pcb* pcb = udp_new();
            _trackers[i].pcb = pcb;
            // Setup callbacks
            udp_recv(pcb, _udpRecvCb, this);
        
            _dumpChannels();

            return Channel(i, true);
        }
    }
    panic("No chanels available");
    return Channel(0, false);
}

void LwIPCommContext::closeUDPChannel(Channel c) {

    _validateChannel(c, Tracker::Type::UDP);
    
    udp_remove(static_cast<struct udp_pcb*>(_trackers[c.getId()].pcb));
    _trackers[c.getId()].inUse = false;

    _dumpChannels();
}

void LwIPCommContext::setupUDPChannel(Channel c, uint32_t localPort, 
    IPAddress remoteIpAddr, uint32_t remotePort) {

    if (traceLevel > 0)
        _log->info("Binding channel %d to port %d", c.getId(), localPort);

    _validateChannel(c, Tracker::Type::UDP);

    err_t e = udp_bind(static_cast<udp_pcb*>(_trackers[c.getId()].pcb),  IP_ANY_TYPE,
        localPort);
    if (e != ERR_OK) {
        _log->error("Failed to bind");
        _trackers[c.getId()].error = true;
    }
    else {
        _lastChannel = c;
        _setupRespPending = true;
    }
}

void LwIPCommContext::sendUDPChannel(Channel c, const uint8_t* b, uint16_t len) {
    panic_unsupported();
}

void LwIPCommContext::sendUDPChannel(Channel c, IPAddress remoteIpAddr, uint32_t remotePort,
    const uint8_t* b, uint16_t len) {

    _validateChannel(c, Tracker::Type::UDP);

    const Tracker* t = &(_trackers[c.getId()]);

    // TODO: NEED A MORE DIRECT WAY
    char addrStr[20];
    formatIP4Address(remoteIpAddr.getAddr(), addrStr, 20);
    // Here we are building an address from a string, so there will no issues
    // related to byte ordering.
    ip4_addr_t addr;
    ip4addr_aton(addrStr, &addr);

    // Allocate pbuf
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
    memcpy((uint8_t*) p->payload, b, len);
    // Do the actual send
    err_t e = udp_sendto(static_cast<udp_pcb*>(t->pcb), p, &addr, remotePort);
    pbuf_free(p);

    if (e != ERR_OK) {
        _log->error("Write error %d", e);
    } else {
        _lastChannel = c;
        _sendRespPending = true;
    }
}

void LwIPCommContext::_dumpChannels() const {
    /*
    cout << "---------------------------------" << endl;
    for (uint32_t i = 0; i < _trackersSize; i++) {
        if (_trackers[i].inUse) {
            cout << "Channel " << i << endl;
            cout << "  type " << _trackers[i].type << endl;
            cout << "  state " << _trackers[i].state << endl;
        }
    }
    */
}

}
