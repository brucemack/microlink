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

