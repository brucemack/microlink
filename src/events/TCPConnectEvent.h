#ifndef _TCPConnectEvent_h
#define _TCPConnectEvent_h

#include "../Event.h"
#include "../TCPChannel.h"

namespace kc1fsz {

class TCPConnectEvent : public Event {
public:

    static const int TYPE = 101;

    TCPConnectEvent(TCPChannel c) : Event(TYPE), _channel(c) { }
    TCPChannel getChannel() const { return _channel; }

private: 

    TCPChannel _channel;
};

}

#endif

