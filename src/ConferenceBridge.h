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
#ifndef _ConferenceBridge_h
#define _ConferenceBridge_h

#include "kc1fsz-tools/Event.h"
#include "kc1fsz-tools/IPAddress.h"
#include "kc1fsz-tools/Channel.h"
#include "kc1fsz-tools/FixedString.h"
#include "kc1fsz-tools/CallSign.h"
#include "kc1fsz-tools/IPLib.h"

#include "gsm-0610-codec/Decoder.h"
#include "gsm-0610-codec/Encoder.h"

#include "StateMachine2.h"

namespace kc1fsz {

class UserInfo;
class Conference;
class AudioOutputContext;

class ConferenceBridge : public StateMachine2, public IPLibEvents, public ConferenceOutput {
public:

    static int traceLevel;

    static uint32_t formatOnDataPacket(const char* msg, uint32_t ssrc,
        uint8_t* packet, uint32_t packetSize);

    static uint32_t formatRTCPPacket_SDES(uint32_t ssrc,
        CallSign callSign, 
        FixedString fullName,
        uint32_t ssrc2,
        uint8_t* packet, uint32_t packetSize);      

    ConferenceBridge(IPLib* ctx, UserInfo* userInfo, Log* log, AudioOutputContext* radio0);

    void setConference(Conference* conf) { _conf = conf; }

    // ----- From IPLibEvents -------------------------------------------------

    virtual void dns(HostName name, IPAddress addr) { }
    virtual void bind(Channel ch);
    virtual void conn(Channel ch) { }
    virtual void disc(Channel ch) { }
    virtual void recv(Channel ch, const uint8_t* data, uint32_t dataLen, IPAddress fromAddr,
        uint16_t fromPort);
    virtual void err(Channel ch, int type) { }

    // ----- From StateMachine2 -----------------------------------------------

protected:

    virtual void _process(int state, bool entry);

    // ----- From ConferenceOutput ---------------------------------------------

public:

    virtual void sendAudio(StationID dest, uint32_t ssrc, uint16_t seq,
        const uint8_t* frame, uint32_t frameLen, AudioFormat fmt);

    virtual void sendText(StationID dest,
        const uint8_t* frame, uint32_t frameLen);

private:

    enum State { 
        IDLE, 
        IN_SETUP_0, 
        IN_SETUP_1, 
        // STATE 3
        IN_SETUP_2, 
        // STATE 4:
        IN_SETUP_3, 
        // STATE 5:
        WAITING,
        // STATE 6:
        SUCCEEDED, 
        // STATE 7:
        FAILED 
    };

    IPLib* _ctx;
    UserInfo* _userInfo;
    Conference* _conf;
    Log* _log;
    Channel _rtpChannel;
    Channel _rtcpChannel;

    AudioOutputContext* _radio0;
    // Hard-coded address for the radio
    IPAddress _radio0Addr;
    Decoder _gsmDecoder0;
    Encoder _gsmEncoder0;
};

}

#endif
