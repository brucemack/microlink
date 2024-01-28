#ifndef _TCPDisconnectEvent_h
#define _TCPDisconnectEvent_h

#include "../Event.h"
#include "../Channel.h"

namespace kc1fsz {

class TCPDisconnectEvent : public Event {
public:

    static const int TYPE = 102;

    TCPDisconnectEvent(Channel c) : Event(TYPE), _channel(c) { }
    Channel getChannel() const { return _channel; }

private: 

    Channel _channel;
};

}

#endif

