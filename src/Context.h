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
#ifndef _Context_h
#define _Context_h

#include <cstdint>

#include "HostName.h"
#include "TCPChannel.h"
#include "IPAddress.h"
#include "StateMachine.h"

namespace kc1fsz {

/**
 * An attempt to completely abstract the management of an EchoLink 
 * session from the hardware environment it runs in.
 * 
 * This class defines the interface for communications with 
 * the outside world.
 */
class Context {
public:

    Context();
    virtual ~Context();

    /**
     * This should be called periodically to allow the Context
     * to push events back into the state machine (i.e. as
     * asynchronous events happen).
    */
    virtual void applyEvents(StateMachine<Context>* machine) = 0;

    virtual uint32_t getTimeMs() { return 0; }

    virtual TCPChannel createTCPChannel() { return TCPChannel(); }
    virtual void closeTCPChannel(TCPChannel c) { }
    virtual void connectTCPChannel(TCPChannel c, IPAddress ipAddr) { }
    virtual void sendTCPChannel(TCPChannel c, const uint8_t* b, uint16_t len) { }

    virtual void startDNSLookup(HostName hostName) { }
};

}

#endif
