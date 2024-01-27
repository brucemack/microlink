#ifndef _TCPReceiveEvent_h
#define _TCPReceiveEvent_h

#include <cstdint>
#include <algorithm>

#include "../common.h"
#include "../Event.h"
#include "../TCPChannel.h"

namespace kc1fsz {

/**
 * IMPORTANT: Each event is limited to 256 bytes of data!
*/
class TCPReceiveEvent : public Event {
public:

    static const int TYPE = 103;

    TCPReceiveEvent(TCPChannel c, const uint8_t* data, uint32_t len) 
        : Event(TYPE), _channel(c) {
        memcpyLimited(_data, data, len, _dataSize);
        _dataLen = std::min(len, _dataSize);
    }

    TCPChannel getChannel() const { return _channel; }
    uint32_t getDataLen() const { return _dataLen; }
    const uint8_t* getData() const { return _data; }

private: 

    TCPChannel _channel;
    const uint32_t _dataSize = 256;
    uint8_t _data[256];
    uint32_t _dataLen;
};

}

#endif

