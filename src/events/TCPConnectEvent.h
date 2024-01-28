#ifndef _TCPConnectEvent_h
#define _TCPConnectEvent_h

#include "../Event.h"
#include "../Channel.h"

namespace kc1fsz {

class TCPConnectEvent : public Event {
public:

    static const int TYPE = 101;

    TCPConnectEvent(Channel c) : Event(TYPE), _channel(c) { }
    Channel getChannel() const { return _channel; }

private: 

    Channel _channel;
};

}

#endif

