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
#include "kc1fsz-tools/Runnable.h"
#include "kc1fsz-tools/rp2040/PicoPollTimer.h"

#include "common.h"

namespace kc1fsz {

class Log;
class UserInfo;
class DNSMachine;

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
    virtual void sendAudio(const IPAddress& dest, uint32_t ssrc, uint16_t seq,
        const uint8_t* frame, uint32_t frameLen, AudioFormat fmt) = 0;
    virtual void sendText(const IPAddress& dest,
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
class Conference : public Runnable {
public:

    static int traceLevel;

    Conference(Authority* auth, ConferenceOutput* out, Log* log,
        DNSMachine* addressingDNS, DNSMachine* monitorDNS);

    void setCallSign(CallSign cs) { _callSign = cs; }
    void setFullName(FixedString fn) { _fullName = fn; }
    void setLocation(FixedString l) { _location = l; }
    void setSilentTimeoutS(uint32_t s) { _silentTimeoutS = s; }
    void setIdleTimeoutS(uint32_t s) { _idleTimeoutS = s; }

    void authorize(StationID id);

    void deAuthorize(StationID id);

    void processAudio(IPAddress source, uint32_t ssrc, uint16_t seq,
        const uint8_t* frame, uint32_t frameLen, AudioFormat fmt);

    void processText(IPAddress source,
        const uint8_t* frame, uint32_t frameLen);

    void dropAll();
    void drop(const CallSign& cs) { _drop(cs); }

    void addRadio(CallSign cs, IPAddress addr);

    uint32_t getActiveStationCount() const;

    void dumpStations(Log* log) const;

    /**
     * @returns How long has it been since we got a message from the monitor?
    */
    uint32_t getSecondsSinceLastMonitorRx() const { 
        // Dummy result for startup
        if (_lastMonitorRxStamp == 0) {
            return 1000;
        }
        return (time_ms() - _lastMonitorRxStamp) / 1000; 
    }

    /**
     * @returns How long has it been since any stations 
     * send/received audio.
    */
    uint32_t getSecondsSinceLastActivity() const { 
        return (time_ms() - _lastActivityStamp) / 1000; 

    }

    uint32_t getSecondsSinceStart() const { 
        return (time_ms() - _startStamp) / 1000; 
    }

    // Diagnostic stuff
    void setWifiRssi(int16_t p) { _wifiRssi = p; }
    void setRxPower(uint32_t p) { _rxPower = p; }
    void setRxSample(int16_t s) { _rxSample = s; }
   
    // ----- From Runnable ------------------------------------------------

    bool run();

private:

    static uint32_t _ssrcGenerator;

    void _sendPing();

    StationID _getTalker() const;
    void _sendStationPing(const StationID& id);
    void _sendBye(StationID id);

    static CallSign _extractCallSign(const uint8_t* data,
        uint32_t dataLen);

    void _processChat(const IPAddress& source,
        const char* callSign, const char* message);

    void _drop(const CallSign& cs);

    struct Station {
        
        bool active = false;
        StationID id;
        bool authorized = false;
        uint32_t connectStamp = 0;
        // The time of the last data of any kind received
        uint32_t lastRxStamp = 0;
        // The time of the last audio packet received
        uint32_t lastAudioRxStamp = 0;
        // The sequence of the last audio packet received
        uint32_t lastRxSeq = 0;
        // Counter of sequence errors
        uint32_t rxSeqErr = 0;
        // Longest gap between packets
        uint32_t longestRxGapMs = 0;
        // Count for current talking session
        uint32_t audioRxPacketCount = 0;
        // The last time we sent the RTCP/oNDATA packet
        uint32_t lastTextTxStamp = 0;
        // The last time we sent an audio packet
        uint32_t lastAudioTxStamp = 0;
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
            lastTextTxStamp = 0;
            lastAudioTxStamp = 0;
            talker = false;
            ssrc = 0;
            seq = 0;
            locked = false;
            lastRxSeq = 0;
            rxSeqErr = 0;
            longestRxGapMs = 0;
            audioRxPacketCount = 0;
        }

        /**
         * @returns Time since we received real audio from this
         * station (indicating that it's active)
        */
        uint32_t msSinceLastAudioRx() const {
            return (time_ms() - lastAudioRxStamp);
        }

        /**
         * @returns Time since we sent real audio to this
         * station (indicating that it's active)
        */
        uint32_t msSinceLastAudioTx() const {
            return (time_ms() - lastAudioTxStamp);
        }

        /**
         * @returns Time since we received real audio from this
         * station (indicating that it's active)
        */
        uint32_t secondsSinceLastAudioRx() const {
            return msSinceLastAudioRx() / 1000;
        }

        /**
         * @returns Time since we sent real audio to this
         * station (indicating that it's active)
        */
        uint32_t secondsSinceLastAudioTx() const {
            return (time_ms() - lastAudioTxStamp) / 1000;
        }
    };

    static const uint32_t _maxStations = 4;
    Station _stations[_maxStations];

    Authority* _authority = 0;
    ConferenceOutput* _output = 0;
    Log* _log = 0;
    DNSMachine* _addressingDNS = 0;
    DNSMachine* _monitorDNS = 0;

    CallSign _callSign;
    FixedString _fullName;
    FixedString _location;
    uint32_t _silentTimeoutS = 30 * 1000;
    uint32_t _idleTimeoutS = 5 * 1000;
    PicoPollTimer _pingTimer;
    uint32_t _lastPingRxStamp = 0;

    // Monitor related
    void _sendMonitorPing();
    void _processMonitorText(IPAddress source,
        const uint8_t* frame, uint32_t frameLen);

    uint32_t _startStamp;
    PicoPollTimer _monitorTimer;
    uint32_t _monitorTxCount = 0;
    IPAddress _monitorAddr;
    uint32_t _lastMonitorTxStamp = 0;
    uint32_t _lastMonitorRxStamp = 0;
    uint32_t _lastActivityStamp = 0;

    // Diags
    int16_t _wifiRssi = 0;
    uint32_t _rxPower = 0;
    int16_t _rxSample = 0;
};

}

#endif
