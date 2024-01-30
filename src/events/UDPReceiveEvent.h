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
 */
#ifndef _UDPReceiveEvent_h
#define _UDPReceiveEvent_h

#include <cstdint>
#include <algorithm>

#include "../common.h"
#include "../Event.h"
#include "../Channel.h"

namespace kc1fsz {

/**
 * IMPORTANT: Each event is limited to 256 bytes of data!
*/
class UDPReceiveEvent : public Event {
public:

    static const int TYPE = 105;

    UDPReceiveEvent(Channel c, const uint8_t* data, uint32_t len) 
        : Event(TYPE), _channel(c) {
        memcpyLimited(_data, data, len, _dataSize);
        _dataLen = std::min(len, _dataSize);
    }

    Channel getChannel() const { return _channel; }
    uint32_t getDataLen() const { return _dataLen; }
    const uint8_t* getData() const { return _data; }

private: 

    Channel _channel;
    const uint32_t _dataSize = 256;
    uint8_t _data[256];
    uint32_t _dataLen;
};

}

#endif

