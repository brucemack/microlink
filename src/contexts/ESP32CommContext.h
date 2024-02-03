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

#include "kc1fsz-tools/HostName.h"
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
class ESP32CommContext : public CommContext, public ATResponseProcessor::EventSink {
public:

    ESP32CommContext(AsyncChannel* esp32);

    /**
     * Indicates where the Event objects should be forwarded to
     * when asynchronous events are detected.
     */
    void setEventProcessor(EventProcessor* ep);

    /**
     * This should be called from the event loop.  It attempts to make forward
     * progress and passes all events to the event processor.
     * 
     * @returns true if any events were dispatched.
    */
    bool poll();

    int getLiveChannelCount() const;

    // ------ CommContext Request Methods -------------------------------------

    virtual void startDNSLookup(HostName hostName);

    virtual Channel createTCPChannel();
    virtual void closeTCPChannel(Channel c);
    virtual void connectTCPChannel(Channel c, IPAddress ipAddr, uint32_t port);
    virtual void sendTCPChannel(Channel c, const uint8_t* b, uint16_t len);

    virtual Channel createUDPChannel(uint32_t localPort);
    virtual void closeUDPChannel(Channel c);
    virtual void sendUDPChannel(Channel c, IPAddress targetAddr, uint32_t targetPort, 
        const uint8_t* b, uint16_t len);

    // ----- ATResponseProcessor::EventSink -----------------------------------

    virtual void ok();
    virtual void domain(const char* addr);
    virtual void connected(uint32_t channel);
    virtual void closed(uint32_t channel);

private:

    void _closeChannel(Channel c);
    void _cleanupTracker();

    enum State { NONE, SEND_PROMPT_WAIT };

    State _state;
    AsyncChannel* _esp32;
    ATResponseProcessor _respProc;
    EventProcessor* _eventProc;

    HostName _lastHostNameRequest;

    // This data structure is used to keep track of active sockets
    struct ChannelTracker {
        bool inUse = false;
        enum Type { TYPE_NONE, TYPE_TCP, TYPE_UDP };
        Type type = Type::TYPE_NONE;
        enum State { STATE_NONE };
        State state = State::STATE_NONE;
    };

    // There is a fixed number of channels
    ChannelTracker _tracker[9];
};

}

#endif
