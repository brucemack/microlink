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
#include "kc1fsz-tools/FixedString.h"

namespace kc1fsz {

class Log;
class UserInfo;

enum AudioFormat { PCM16, GSMFR, GSMFR4X, TEXT };

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

class ConferenceOutput {
public:
    virtual void sendAudio(StationID dest, uint32_t ssrc, uint16_t seq,
        const uint8_t* frame, uint32_t frameLen, AudioFormat fmt) = 0;
    virtual void sendText(StationID dest,
        const uint8_t* frame, uint32_t frameLen) = 0;
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

    Conference(Authority* auth, ConferenceOutput* out, Log* log) 
    : _authority(auth), _output(out), _log(log) { }

    void setCallSign(CallSign cs) { _callSign = cs; }
    void setFullName(FixedString fn) { _fullName = fn; }
    void setLocation(FixedString l) { _location = l; }

    void authorize(StationID id);

    void deAuthorize(StationID id);

    void processAudio(IPAddress source, uint32_t ssrc, uint16_t seq,
        const uint8_t* frame, uint32_t frameLen, AudioFormat fmt);
    void processText(IPAddress source,
        const uint8_t* frame, uint32_t frameLen);

    void dropAll();

    void addRadio(CallSign cs, IPAddress addr);

    uint32_t getActiveStationCount() const;

    void dumpStations(std::ostream& str) const;
   
    // ----- From Runnable ------------------------------------------------

    bool run();

private:

    static int _traceLevel;

    static uint32_t _ssrcGenerator;

    StationID _getTalker() const;
    void _sendPing(StationID id);
    void _sendBye(StationID id);

    static StationID _extractStationID(IPAddress source, const uint8_t* data,
        uint32_t dataLen);

    struct Station {
        
        bool active = false;
        StationID id;
        bool authorized = false;
        uint32_t connectStamp = 0;
        uint32_t lastRxStamp = 0;
        uint32_t lastTxStamp = 0;
        uint32_t lastAudioRxStamp = 0;
        bool talker = false;
        // A unique number that identifies traffic from this 
        // station.
        uint32_t ssrc = 0;
        uint16_t seq = 0;
        bool locked = false;

        void reset() {
            active = false;
            authorized = false;
            connectStamp = 0;
            lastRxStamp = 0;
            lastAudioRxStamp = 0;
            lastTxStamp = 0;
            talker = false;
            ssrc = 0;
            seq = 0;
            locked = false;
        }

        /**
         * @returns Time since we received real audio from this
         * station (indicating that it's active)
        */
        uint32_t secondsSinceLastAudioRx() const {
            return (time_ms() - lastAudioRxStamp) / 1000;
        }
    };

    static const uint32_t _maxStations = 4;
    Station _stations[_maxStations];

    Authority* _authority = 0;
    ConferenceOutput* _output = 0;
    Log* _log = 0;

    CallSign _callSign;
    FixedString _fullName;
    FixedString _location;
};

}

#endif
