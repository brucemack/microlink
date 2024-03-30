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
#include "kc1fsz-tools/AudioProcessor.h"
#include "kc1fsz-tools/CircularQueuePtr.h"
#include "kc1fsz-tools/Runnable.h"

#include "gsm-0610-codec/Decoder.h"
#include "gsm-0610-codec/Encoder.h"

namespace kc1fsz {

class UserInfo;
class Conference;
class AudioOutputContext;

class ConferenceBridge : public Runnable, public IPLibEvents, 
    public ConferenceOutput, public AudioProcessor {

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

    uint32_t getRadio0GSMQueueOFCount() const {
        return _radio0GSMQueuePtr.getOverflowCount();
    }

    // ----- From IPLibEvents -------------------------------------------------

    virtual void reset();
    virtual void dns(HostName name, IPAddress addr) { }
    virtual void bind(Channel ch);
    virtual void conn(Channel ch) { }
    virtual void disc(Channel ch) { }
    virtual void recv(Channel ch, const uint8_t* data, uint32_t dataLen, IPAddress fromAddr,
        uint16_t fromPort);
    virtual void err(Channel ch, int type) { }

    // ----- From ConferenceOutput ---------------------------------------------

    virtual void sendAudio(const IPAddress& dest, uint32_t ssrc, uint16_t seq,
        const uint8_t* frame, uint32_t frameLen, AudioFormat fmt);

    virtual void sendText(const IPAddress& dest,
        const uint8_t* frame, uint32_t frameLen);

    // ----- From AudioProcessor -----------------------------------------------

    /**
     * @param frame 160 x 4 samples of 16-bit PCM audio.
     * @return true if the audio was taken, or false if the 
     *   session is busy and the TX will need to be 
     *   retried.
    */
    virtual bool play(const int16_t* frame, uint32_t frameLen);

    // ----- From Runnable ------------------------------------------------------

    virtual bool run();

private:

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
    uint32_t _radio0Ssrc = 7;
    uint16_t _radio0Seq = 0;

    void _serviceRadio0GSMQueue();

    /**
     * Puts a *single* GSM frame (i.e. 33 bytes) on a queue for later 
     * processing by radio 0.
    */
    void _writeRadio0GSMQueue(const uint8_t* gsmFrame, uint32_t gsmFrameLen);

    // Used to create a small delay before servicing the radio GSM queue
    uint32_t _delayCount = 0;

    // A circular buffer of GSM frames headed to radio0 (pre-decode)
    static const uint32_t _radio0GSMQueueSize = 16;
    uint8_t _radio0GSMQueue[_radio0GSMQueueSize][33];
    CircularQueuePtr _radio0GSMQueuePtr;

    // This is used to accumulate a complete 160x4 sample PCM audio frame
    int16_t _pcmFrame[160 * 4];
    uint32_t _pcmFramePtr = 0;
};

}

#endif
