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
#ifndef _SIM7600IPLib_h
#define _SIM7600IPLib_h

#include "kc1fsz-tools/Runnable.h"
#include "kc1fsz-tools/Channel.h"
#include "kc1fsz-tools/HostName.h"
#include "kc1fsz-tools/IPAddress.h"
#include "kc1fsz-tools/IPLib.h"

namespace kc1fsz {

class Log;
class AsyncChannel;

/**
 * IMPORTANT: We are assuming that this runs on an embedded processor
 * we so limit the use of C++ features.
 */
class SIM7600IPLib : public IPLib, public Runnable {
public:

    static int traceLevel;

    SIM7600IPLib(Log* log, AsyncChannel* uart);

    // ----- Runnable Methods ------------------------------------------------

    /**
     * This should be called from the event loop.  It attempts to make forward
     * progress and passes all events to the event processor.
     * 
     * @returns true if any events were dispatched.
    */
    virtual bool run();

    // ----- From IPLib ------------------------------------------------------

    virtual void reset();

    virtual bool isLinkUp() const;

    virtual void addEventSink(IPLibEvents* e);

    virtual void queryDNS(HostName hostName);

    virtual Channel createTCPChannel();
    virtual void connectTCPChannel(Channel c, IPAddress ipAddr, uint32_t port);
    virtual void sendTCPChannel(Channel c, const uint8_t* b, uint16_t len);

    virtual Channel createUDPChannel();
    virtual void bindUDPChannel(Channel c, uint32_t port);
    virtual void sendUDPChannel(const Channel& c, 
        const IPAddress& remoteIpAddr, uint32_t remotePort,
        const uint8_t* b, uint16_t len);

    virtual void closeChannel(Channel c);

private:

    Log* _log;
    AsyncChannel* _uart;

    static const uint32_t _maxEvents = 16;
    IPLibEvents* _events[_maxEvents];
    uint32_t _eventsLen = 0;   

    static const uint32_t _sendHoldSize = 256;
    uint8_t _sendHold[_sendHoldSize];
    uint32_t _sendHoldLen;

    static const uint32_t _rxHoldSize = 256;
    uint8_t _rxHold[_rxHoldSize];
    uint32_t _rxHoldLen;

    enum State {
        IDLE,
        INIT_0,
        INIT_1,
        INIT_2,
        INIT_3,
        INIT_4,
        INIT_5,
        INIT_6,
        INIT_7,
        RUN
    };

    State _state;
    bool _isNetOpen = false;

};

}

#endif
