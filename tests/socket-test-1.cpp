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
 * 
 * =================================================================================
 * This file is unit-test code only.  None of this should be use for 
 * real applications!
 * =================================================================================
 */
#include <iostream>
#include <cassert>

#include "kc1fsz-tools/events/DNSLookupEvent.h"
#include "kc1fsz-tools/events/UDPReceiveEvent.h"

#include "common.h"
#include "contexts/SocketContext.h"
#include "EventProcessor.h"

using namespace std;
using namespace kc1fsz;

class TestEventProcessor : public EventProcessor {
public:

    int dnsCount = 0;
    int udpCount = 0;

    virtual void processEvent(const Event* ev) {

        cout << "EVENT TYPE " << ev->getType() << endl;

        if (ev->getType() == DNSLookupEvent::TYPE) {
            const DNSLookupEvent* evt = (const DNSLookupEvent*)ev;
            char buf[64];
            formatIP4Address(evt->getAddr().getAddr(), buf, 64);
            cout << "ADDR: " << buf << endl;
            dnsCount++;
        }
        else if (ev->getType() == UDPReceiveEvent::TYPE) {
            const UDPReceiveEvent* evt = (const UDPReceiveEvent*)ev;
            cout << "Got UDP Data: [";
            cout.write((const char*)evt->getData(), evt->getDataLen());
            cout << "]" << endl;
            udpCount++;
        }
    }
};

int main(int,const char**) {

    TestEventProcessor evp;
    SocketContext ctx;
    
    ctx.startDNSLookup(HostName("www.google.com"));
    assert(evp.dnsCount == 0);
    ctx.poll(&evp);
    assert(evp.dnsCount == 1);

    // Setup a UDP socket that talks to ourselves
    IPAddress a = parseIP4Address("127.0.0.1");
    Channel u = ctx.createUDPChannel(9999);
    assert(ctx.getLiveChannelCount() == 1);
    ctx.sendUDPChannel(u, a, 9999, (const uint8_t*)"TEST", 4);

    ctx.poll(&evp);
    ctx.poll(&evp);

    assert(evp.udpCount == 1);

    ctx.closeUDPChannel(u);
    assert(ctx.getLiveChannelCount() == 0);

    return 0;

}
