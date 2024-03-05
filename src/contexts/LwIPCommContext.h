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
#ifndef _LwIPCommContext_h
#define _LwIPCommContext_h

#include "lwip/tcp.h"

#include "kc1fsz-tools/Runnable.h"
#include "kc1fsz-tools/Channel.h"
#include "kc1fsz-tools/HostName.h"
#include "kc1fsz-tools/IPAddress.h"
#include "kc1fsz-tools/CommContext.h"
#include "kc1fsz-tools/events/DNSLookupEvent.h"

namespace kc1fsz {

class Log;
class EventProcessor;

/**
 * IMPORTANT: We are assuming that this runs on an embedded processor
 * we so limit the use of C++ features.
 */
class LwIPCommContext : public CommContext, public Runnable {
public:

    static int traceLevel;

    LwIPCommContext(Log* log);

    /**
     * Indicates where the Event objects should be forwarded to
     * when asynchronous events are detected.
     */
    void setEventProcessor(EventProcessor* ep) { _eventProc = ep; }

    int getState() const { return _state; }

    /**
     * Used to receive and discard anything on the channel
     * @returns The number of bytes discarded.
    */
    uint32_t flush(uint32_t ms);

    int getLiveChannelCount() const;

    bool test();

    // ----- Runnable Methods ------------------------------------------------

    /**
     * This should be called from the event loop.  It attempts to make forward
     * progress and passes all events to the event processor.
     * 
     * @returns true if any events were dispatched.
    */
    virtual bool run();

    // ------ CommContext Request Methods -------------------------------------

    virtual void reset();

    virtual void startDNSLookup(HostName hostName);

    virtual Channel createTCPChannel();
    virtual void closeTCPChannel(Channel c);
    virtual void connectTCPChannel(Channel c, IPAddress ipAddr, uint32_t port);
    virtual void sendTCPChannel(Channel c, const uint8_t* b, uint16_t len);

    virtual Channel createUDPChannel();
    virtual void closeUDPChannel(Channel c);

    /**
     * In socket parlance, this performs the bind.
     */
    virtual void setupUDPChannel(Channel c, uint32_t localPort, 
        IPAddress remoteIpAddr, uint32_t remotePort);

    virtual void sendUDPChannel(Channel c, const uint8_t* b, uint16_t len);

    virtual void sendUDPChannel(Channel c, IPAddress remoteIpAddr, uint32_t remotePort,
        const uint8_t* b, uint16_t len);

private:

    enum State { 
        NONE, 
        IN_RESET,
    };

    Log* _log;
    EventProcessor* _eventProc;
    State _state;
    bool _inCallback = false;

    bool _dnsRespPending = false;
    HostName _lastHostNameReq;
    IPAddress _lastAddrResp;
    Channel _lastChannel;

    bool _setupRespPending = false;
    bool _sendRespPending = false;

    static const uint32_t _sendHoldSize = 256;
    uint8_t _sendHold[_sendHoldSize];
    uint32_t _sendHoldLen;

    struct Tracker {

        bool inUse = false;
        bool error = false;

        enum Type {
            NONE, 
            TCP,
            UDP
        } type = Type::NONE;

        enum State {
            IDLE,
            IN_CONNECT
        } state = State::IDLE;

        void* pcb = 0;
    };

    static const uint32_t _trackersSize = 6;
    Tracker _trackers[_trackersSize];
   

    void _validateChannel(Channel c, Tracker::Type t) const;
    Tracker* _findTracker(void* tpcb);
    int _findTracker2(void* tpcb);

    static err_t _tcpRecvCb(void *arg, tcp_pcb *tpcb, pbuf *p, err_t err);
    err_t _tcpRecv(tcp_pcb *tpcb, pbuf *p, err_t err);

    static err_t _tcpSentCb(void *arg, tcp_pcb *tpcb, u16_t len);

    static err_t _tcpConnectCb(void *arg, tcp_pcb *tpcb, err_t err);
    err_t _tcpConnect(tcp_pcb *tpcb, err_t err);

    static void _udpRecvCb(void *arg, udp_pcb *upcb, pbuf *p, const ip_addr_t *addr, u16_t port);
    void _udpRecv(udp_pcb *upcb, pbuf *p, const ip_addr_t *addr, u16_t port);

    static void _errCb(void *arg, err_t err);

    static void _dnsCb(const char* name, const ip_addr_t* ipaddr, void *callback_arg);
    void _dns(const char* name, const ip_addr_t* ipaddr);

    void _dumpChannels() const;
};

}

#endif
