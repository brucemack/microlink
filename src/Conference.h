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
#ifndef _Conference_h
#define _Conference_h

#include <cstdint>

#include "kc1fsz-tools/IPAddress.h"
#include "kc1fsz-tools/CallSign.h"

namespace kc1fsz {

class Log;
class UserInfo;

enum AudioFormat { PCM16, GSMFR, GSMFR4X };

class StationID {
public:

    StationID() : _null(true) { }
    StationID(IPAddress a, CallSign c) : _addr(a), _call(c), _null(false) { }
    StationID(const StationID& that) : _addr(that._addr), _call(that._call), _null(that._null) { }
    
    bool operator== (const StationID& that) const { 
        return _addr == that._addr && _call == that._call && _null == that._null; 
    }

    IPAddress getAddr() const { return _addr; }
    CallSign getCall() const { return _call; }
    bool isNull() const { return _null; }

private:

    IPAddress _addr;
    CallSign _call;
    bool _null = true;
};

class AudioOutput {
public:
    virtual void sendAudio(StationID dest, 
        const uint8_t* frame, uint32_t frameLen, AudioFormat fmt) = 0;
    virtual void sendPing(StationID dest) = 0;
    virtual void sendBye(StationID dest) = 0;
};

class Authority {
public:

    virtual void validate(StationID id) = 0;
};

/**
When a new StationID is seen (i.e. new connection) a call is made to 
the Authority to validate the StationID.  Until authorization is 
received the station is ignored.

When the Conference is told that the StationID and authorized then
it is allowed to participate in the conference.

When audio is received from a StationID the Conference checks
to see if someone else is already talking.  If so, the audio
is ignored.  If not, the station becomes the talker.  Audio
from the talking StationID is forwarded to all other Stations.
*/
class Conference {
public:

    Conference(Authority* auth, AudioOutput* output);

    void authorize(StationID id);

    void deAuthorize(StationID id);

    void processAudio(IPAddress source,
        const uint8_t* frame, uint32_t frameLen, AudioFormat fmt);
    void processText(IPAddress source,
        const uint8_t* frame, uint32_t frameLen);

    // ----- From Runnable ------------------------------------------------

    bool run();

protected:

    StationID _getTalker() const;

    struct Station {
        bool active;
        StationID id;
        bool authorized;
        uint32_t connectStamp;
        uint32_t lastRxStamp;
        uint32_t lastTxStamp;
        bool talker;
    };

    static const uint32_t _maxStations = 4;
    Station _stations[_maxStations];

    Log* _log;
    AudioOutput* _output;
    Authority* _authority;
};

}

#endif
