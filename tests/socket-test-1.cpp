#include <iostream>
#include "contexts/SocketContext.h"
#include "EventProcessor.h"
#include "events/DNSLookupEvent.h"

using namespace std;
using namespace kc1fsz;

class TestEventProcessor : public EventProcessor {
public:

    virtual void processEvent(const Event* ev) {

        cout << "EVENT TYPE " << ev->getType() << endl;

        if (ev->getType() == DNSLookupEvent::TYPE) {
            const DNSLookupEvent* evt = (const DNSLookupEvent*)ev;
            cout << "ADDR: " << std::hex << evt->addr.getAddr() << endl;
            char buf[64];
            formatIP4Address(evt->addr.getAddr(), buf, 64);
            cout << "ADDR: " << buf << endl;
        }
    }
};

int main(int,const char**) {

    TestEventProcessor evp;
    SocketContext ctx;
    
    {
        ctx.startDNSLookup(HostName("www.google.com"));
        ctx.poll(&evp);
    }


    return 0;

}
