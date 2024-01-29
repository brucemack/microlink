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

#include "../Context.h"

namespace kc1fsz {

class EventProcessor;

class SocketContext : public Context {
public:

    SocketContext();

    /**
     * This should be called from the event loop.  It attempts to make forward
     * progress and passes all events to the event processor.
    */
    void poll(EventProcessor* ep);

    // ------ Request Methods -------------------------------------------------

    virtual uint32_t getTimeMs();

    virtual void startDNSLookup(HostName hostName);

    virtual Channel createTCPChannel();
    virtual void closeTCPChannel(Channel c);
    virtual void connectTCPChannel(Channel c, IPAddress ipAddr);
    virtual void sendTCPChannel(Channel c, const uint8_t* b, uint16_t len);

    virtual Channel createUDPChannel(uint32_t localPort);
    virtual void closeUDPChannel(Channel c);
    virtual void sendUDPChannel(Channel c, IPAddress targetAddr, uint32_t targetPort, 
        const uint8_t* b, uint16_t len);

private:

    bool _dnsResultPending;
    IPAddress _dnsResult;
};

}

#endif
