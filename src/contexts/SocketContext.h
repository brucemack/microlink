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
#ifndef _SocketContext_h
#define _SocketContext_h

#include <vector>
#include <deque>

#include "kc1fsz-tools/CommContext.h"
//#include "kc1fsz-tools/events/DNSLookupEvent.h"
//#include "kc1fsz-tools/events/StatusEvent.h"
//#include "kc1fsz-tools/events/ChannelSetupEvent.h"

namespace kc1fsz {

class EventProcessor;

/**
 * IMPORTANT: We are assuming that this runs in a full environment so 
 * we are using a wider range of C++ std libraries, including
 * dynamic memory allocation.
 */
class SocketContext : public CommContext {
public:

    static int traceLevel;

    SocketContext();

    /**
     * Indicates where the Event objects should be forwarded to
     * when asynchronous events are detected.
     */
    void setEventProcessor(EventProcessor* ep) { _sink = ep; }

    void reset();

    /**
     * This should be called from the event loop.  It attempts to make forward
     * progress and passes all events to the event processor.
     * 
     * @returns true if any events were dispatched.
    */
    bool poll();

    // ------ Request Methods -------------------------------------------------

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

    int getLiveChannelCount() const;

private:

    void _closeChannel(Channel c);
    void _cleanupTracker();

    std::deque<std::unique_ptr<Event>> _eventQueue;

    /*
    // A one-deep queue of DNS results
    // TODO: MAKE THIS A REAL QUEUE
    bool _dnsResultPending;
    DNSLookupEvent _dnsResult;

    bool _resetResultPending;
    StatusEvent _resetResult;

    bool _setupUDPResultPending;
    ChannelSetupEvent _setupUDPResult;
    */

    // This data structure is used to keep track of active sockets
    struct SocketTracker {
        enum Type { NONE, TCP, UDP };
        Type type = Type::NONE;
        bool connectRequested = false;
        bool connectWaiting = false;
        bool deletePending = false;
        int fd = 0;
        IPAddress remoteAddr;
        uint32_t remotePort;
    };

    std::vector<SocketTracker> _tracker;
    EventProcessor* _sink;
};

}

#endif
