#ifndef _DNSLookupEvent_h
#define _DNSLookupEvent_h

#include "../Event.h"
#include "../IPAddress.h"

namespace kc1fsz {

class DNSLookupEvent : public Event {
public:

    static const int TYPE = 100;

    DNSLookupEvent(IPAddress a) : Event(TYPE), addr(a) { }
    
    IPAddress addr;
};

}

#endif

