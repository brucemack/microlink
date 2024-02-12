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
#ifndef _ESP32CommContext_h
#define _ESP32CommContext_h

#include "kc1fsz-tools/Channel.h"
#include "kc1fsz-tools/HostName.h"
#include "kc1fsz-tools/IPAddress.h"
#include "kc1fsz-tools/AsyncChannel.h"
#include "kc1fsz-tools/CommContext.h"
#include "kc1fsz-tools/events/DNSLookupEvent.h"

#include "../ATResponseProcessor.h"

namespace kc1fsz {

class EventProcessor;
class AsyncChannel;

/**
 * IMPORTANT: We are assuming that this runs on an embedded processor
 * we so limit the use of C++ features.
 */
class ESP32CommContext : public CommContext, public ATResponseProcessor::EventSink,
    public Runnable {
public:

    static int traceLevel;

    ESP32CommContext(AsyncChannel* esp32);

    /**
     * Indicates where the Event objects should be forwarded to
     * when asynchronous events are detected.
     */
    void setEventProcessor(EventProcessor* ep);

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

    // ----- ATResponseProcessor::EventSink -----------------------------------

    virtual void ok();
    void sendOk();
    virtual void error();
    virtual void sendPrompt();
    virtual void domain(const char* addr);
    virtual void connected(uint32_t channel);
    virtual void closed(uint32_t channel);
    virtual void ipd(uint32_t channel, uint32_t chunk,
        const uint8_t* data, uint32_t len);
     virtual void notification(const char* msg);

private:

    void _closeChannel(Channel c);
    void _cleanupTracker();

    enum State { 
        NONE, 
        IN_INIT,
        IN_DNS, 
        IN_TCP_CONNECT, 
        IN_UDP_SETUP, 
        IN_SEND_PROMPT_WAIT,
        IN_SEND_OK_WAIT 
    };

    AsyncChannel* _esp32;
    ATResponseProcessor _respProc;
    EventProcessor* _eventProc;

    State _state;
    int _initCount;

    HostName _lastHostNameReq;
    IPAddress _lastAddrResp;
    Channel _lastChannel;

    static const uint32_t _sendHoldSize = 256;
    uint8_t _sendHold[_sendHoldSize];
    uint32_t _sendHoldLen;

    // This data structure is used to keep track of active sockets
    struct ChannelTracker {
        bool inUse = false;
        enum Type { TYPE_NONE, TYPE_TCP, TYPE_UDP };
        Type type = Type::TYPE_NONE;
        enum State { STATE_NONE };
        State state = State::STATE_NONE;
        // Used for UDP
        IPAddress addr;
        uint32_t port;
    };

    // There is a fixed number of channels
    ChannelTracker _tracker[9];
};

}

#endif
