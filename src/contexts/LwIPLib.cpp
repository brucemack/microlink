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

#include "../common.h"
#include "../BooleanHolder.h"

#include "LwIPLib.h"

using namespace std;

namespace kc1fsz {

int LwIPLib::traceLevel = 0;

LwIPLib::LwIPLib(Log* log) 
:   _log(log) {
}

// ----- Runnable Methods ------------------------------------------------

/**
    * This should be called from the event loop.  It attempts to make forward
    * progress and passes all events to the event processor.
    * 
    * @returns true if any events were dispatched.
*/
bool LwIPLib::run() {

    if (_dnsRespPending) {
        _dnsRespPending = false;
        for (uint32_t i = 0; i < _eventsLen; i++)
            _events[i]->dns(_lastHostNameReq, _lastAddrResp);
    }

    if (_bindRespPending) {
        _bindRespPending = false;
        for (uint32_t i = 0; i < _eventsLen; i++)
            _events[i]->bind(_lastChannel);
    }

    return true;
}

void LwIPLib::addEventSink(IPLibEvents* e) {
    if (_eventsLen < _maxEvents) {
        _events[_eventsLen++] = e;
    } else {
        panic_unsupported();
    }
}

bool LwIPLib::isLinkUp() const {
    return cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP;
}

void LwIPLib::queryDNS(HostName hostName) {

    if (traceLevel > 0)
        _log->info("DNS request for %s", hostName.c_str());

    _lastHostNameReq = hostName;

    ip_addr_t addr;
    err_t e = dns_gethostbyname(hostName.c_str(), &addr, _dnsCb, this);
    // This is the case where the response is immediately avaialble
    if (e == ERR_OK) {
        // TODO: Clean up, this is very limited
        if (_dnsRespPending) {
            _log->info("DNS request already in process");
            return;
        }
        _dnsRespPending = true;        
        _lastAddrResp = IPAddress(ntohl(addr.addr));
    }
    else if (e == ERR_ARG) {
        if (traceLevel > 0)
            _log->error("DNS request failed");
    } else if (e == ERR_INPROGRESS) {
    } else {
        panic_unsupported();
    }
}

void LwIPLib::_dnsCb(const char* name, const ip_addr_t* ipaddr, void* arg) {   
    static_cast<LwIPLib*>(arg)->_dns(name, ipaddr);
}

void LwIPLib::_dns(const char* name, const ip_addr_t* ipaddr) {   

    BooleanHolder h(&_inCallback);

    uint32_t addr = ntohl(ipaddr->addr);

    if (traceLevel > 0) {
        char addrStr[20];
        formatIP4Address(addr, addrStr, 20);
        _log->info("DNS Result %s", addrStr);
    }

    HostName hn(name);
    IPAddress ad(addr);
    for (uint32_t i = 0; i < _eventsLen; i++)
        _events[i]->dns(hn, ad);
}

err_t LwIPLib::_tcpRecvCb(void *arg, tcp_pcb *tpcb, pbuf *p, err_t err) {
    return static_cast<LwIPLib*>(arg)->_tcpRecv(tpcb, p, err);
}

err_t LwIPLib::_tcpRecv(tcp_pcb *tpcb, pbuf *p, err_t err) {

    BooleanHolder h(&_inCallback);

    const int i = _findTracker2(tpcb);
    // Look for disconnect
    if (p == 0) {
        if (i != -1) {
            for (uint32_t k = 0; k < _eventsLen; k++)
                _events[k]->disc(Channel(i, true));
        } else {
            _log->error("Socket not found");
        }
    } 
    // Look for normal data receive
    else {
        if (i != -1) {
            for (uint32_t k = 0; k < _eventsLen; k++)
                _events[k]->recv(Channel(i, true), (const uint8_t*)p->payload, p->len, 
                    IPAddress(0), 0);
        } else {
            _log->error("Socket not found");
        }
        pbuf_free(p);
    }
    return ERR_OK;
}

err_t LwIPLib::_tcpSentCb(void *arg, tcp_pcb *tpcb, u16_t len) {
    return ERR_OK;
}

void LwIPLib::_errCb(void *arg, err_t err) {
    cout << "ERROR CB " << err << endl;
}

err_t LwIPLib::_tcpConnectCb(void *arg, tcp_pcb* pcb, err_t err) {
    return static_cast<LwIPLib*>(arg)->_tcpConnect(pcb, err);
}

err_t LwIPLib::_tcpConnect(tcp_pcb* pcb, err_t err) {

    BooleanHolder h(&_inCallback);

    for (uint32_t t = 0; t < _trackersSize; t++) {
        if (_trackers[t].inUse == true &&
            _trackers[t].state == Tracker::State::IN_CONNECT &&
            _trackers[t].pcb == pcb) {
            _trackers[t].state = Tracker::State::IDLE;
            for (uint32_t i = 0; i < _eventsLen; i++)
                _events[i]->conn(Channel(t, true));
            return ERR_OK;
        }
    }
    _log->error("_connect on invalid socket");
    return ERR_OK;
}

Channel LwIPLib::createTCPChannel() {
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

void LwIPLib::_validateChannel(Channel c, Tracker::Type t) const {
    if (c.getId() >= (int)_trackersSize ||
        !_trackers[c.getId()].inUse ||
        _trackers[c.getId()].type != t) {
        panic("Invalid channel");
    }
}

LwIPLib::Tracker* LwIPLib::_findTracker(void* pcb) {
    for (uint32_t i = 0; i < _trackersSize; i++) {
        if (_trackers[i].inUse == true &&
            _trackers[i].pcb == pcb) {
            return &(_trackers[i]);
        }
    }
    return 0;
}

int LwIPLib::_findTracker2(void* pcb) {
    for (int i = 0; i < (int)_trackersSize; i++) {
        if (_trackers[i].inUse == true &&
            _trackers[i].pcb == pcb) {
            return i;
        }
    }
    return -1;
}

void LwIPLib::closeChannel(Channel c) {

    if (c.getId() >= (int)_trackersSize) {
        panic("Invalid channel (1)");
        return;
    }

    Tracker* t = &(_trackers[c.getId()]);
    if (!t->inUse) {
        panic("Invalid channel (2)");
        return;
    }

    if (t->type == Tracker::Type::TCP) {
        err_t e = tcp_close(static_cast<tcp_pcb*>(t->pcb));
        // TODO: ERROR HANDLING
        if (e != ERR_OK) 
            panic("Invalid channel (3)");
        t->inUse = false;
    } else if (t->type == Tracker::Type::UDP) {
        udp_remove(static_cast<struct udp_pcb*>(t->pcb));
        t->inUse = false;
    }
}

void LwIPLib::connectTCPChannel(Channel c, IPAddress ipAddr, uint32_t port) {

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

void LwIPLib::sendTCPChannel(Channel c, const uint8_t* b, uint16_t len) {    

    _validateChannel(c, Tracker::Type::TCP);

    const Tracker* t = &(_trackers[c.getId()]);
    err_t e = tcp_write(static_cast<tcp_pcb*>(t->pcb), b, len, TCP_WRITE_FLAG_COPY);
    if (e != ERR_OK) {
        _log->error("Write error %d", e);
    }
}

void LwIPLib::_udpRecvCb(void *arg, udp_pcb *upcb, pbuf *p, const ip_addr_t *addr, u16_t port) {
    static_cast<LwIPLib*>(arg)->_udpRecv(upcb, p, addr, port);
}

void LwIPLib::_udpRecv(udp_pcb* upcb, pbuf *p, const ip_addr_t *addr, u16_t port) {
    const int i = _findTracker2(upcb);
    if (traceLevel > 1) {
        _log->info("(RX) channel %d", i);
    }
    //prettyHexDump((const uint8_t*)p->payload, p->len, cout);
    IPAddress a(ntohl(addr->addr));
    for (uint32_t k = 0; k < _eventsLen; k++)
        _events[k]->recv(Channel(i, true), (const uint8_t*)p->payload, p->len, a, port);
    pbuf_free(p);
}

Channel LwIPLib::createUDPChannel() {
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
            return Channel(i, true);
        }
    }
    panic("No chanels available");
    return Channel(0, false);
}

void LwIPLib::bindUDPChannel(Channel c, uint32_t localPort) {

    if (traceLevel > 0)
        _log->info("Binding channel %d to port %d", c.getId(), localPort);

    _validateChannel(c, Tracker::Type::UDP);

    err_t e = udp_bind(static_cast<udp_pcb*>(_trackers[c.getId()].pcb), IP_ANY_TYPE,
        localPort);
    if (e != ERR_OK) {
        _log->error("Failed to bind");
        _trackers[c.getId()].error = true;
        // TODO: NEED ERROR EVENT
    }
    else {
        _lastChannel = c;
        _bindRespPending = true;
    }
}

void LwIPLib::sendUDPChannel(Channel c, IPAddress remoteIpAddr, uint32_t remotePort,
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
    }
}

void LwIPLib::_dumpChannels() const {
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
