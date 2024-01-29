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
            cout << "ADDR: " << evt->addr.getAddr() << endl;
        }
    }
};

int main(int,const char**) {

    TestEventProcessor evp;
    SocketContext ctx;
    
    {
        ctx.startDNSLookup(HostName("naeast.echolink.org"));
        ctx.poll(&evp);
    }


    return 0;

}
