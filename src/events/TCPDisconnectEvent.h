#ifndef _TCPDisconnectEvent_h
#define _TCPDisconnectEvent_h

#include "../Event.h"
#include "../TCPChannel.h"

namespace kc1fsz {

class TCPDisconnectEvent : public Event {
public:

    static const int TYPE = 102;

    TCPDisconnectEvent(TCPChannel c) : Event(TYPE), _channel(c) { }
    TCPChannel getChannel() const { return _channel; }

private: 

    TCPChannel _channel;
};

}

#endif

