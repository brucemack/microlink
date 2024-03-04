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
#ifndef _PicoWCommContext_h
#define _PicoWCommContext_h

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
class PicoWCommContext : public CommContext, public Runnable {
public:

    static int traceLevel;

    PicoWCommContext(Log* log);

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
        IN_INIT,
        IN_DNS, 
        IN_TCP_CONNECT, 
        IN_UDP_SETUP, 
        // State 5: 
        IN_SEND_PROMPT_WAIT,
        // State 6:
        IN_SEND_OK_WAIT,
        // State 7: This is the state right after we have requested
        // a send and are waiting for the initial OK response.
        IN_SEND_WAIT 
    };

    Log* _log;
    EventProcessor* _eventProc;

    State _state;

    HostName _lastHostNameReq;
    IPAddress _lastAddrResp;
    Channel _lastChannel;

    static const uint32_t _sendHoldSize = 256;
    uint8_t _sendHold[_sendHoldSize];
    uint32_t _sendHoldLen;
};

}

#endif
