#include <iostream>
#include <cassert>

#include "contexts/SocketContext.h"
#include "EventProcessor.h"
#include "events/DNSLookupEvent.h"
#include "events/UDPReceiveEvent.h"

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
            cout << "God UDP Data: [";
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
