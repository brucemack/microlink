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
#ifndef _QSOFlowMachine_h
#define _QSOFlowMachine_h

#include "kc1fsz-tools/IPAddress.h"
#include "kc1fsz-tools/CallSign.h"
#include "kc1fsz-tools/FixedString.h"
#include "kc1fsz-tools/Channel.h"
#include "gsm-0610-codec/Decoder.h"
#include "gsm-0610-codec/Encoder.h"

#include "../StateMachine.h"

namespace kc1fsz {

class UserInfo;
class CommContext;
class Event;
class UDPReceiveEvent;
class TickEvent;
class AudioOutputContext;

class QSOFlowMachine : public StateMachine {
public:

    static int traceLevel;

    QSOFlowMachine(CommContext* ctx, UserInfo* userInfo, 
        AudioOutputContext* audioOutput);

    virtual void processEvent(const Event* ev);
    virtual void start();
    virtual bool isDone() const;
    virtual bool isGood() const;

    void setCallSign(CallSign cs) { _callSign = cs; }
    void setFullName(FixedString fn) { _fullName = fn; }
    void setLocation(FixedString l) { _location = l; }
    void setTargetAddress(IPAddress addr) { _targetAddr = addr; }
    void setRTCPChannel(Channel c) { _rtcpChannel = c; }
    void setRTPChannel(Channel c)  { _rtpChannel = c; }
    void setSSRC(uint32_t s) { _ssrc = s; }

    /**
     * @param frame 160 x 4 samples of 16-bit PCM audio.
     * @return true if the audio was taken, or false if the 
     *   session is busy and the TX will need to be 
     *   retried.
    */
    bool txAudio(const int16_t* frame);

private:
    
    void _processRXReceive(const UDPReceiveEvent* evt);
    void _processTXReceive(const UDPReceiveEvent* evt);
    void _processONDATA(const uint8_t* d, uint32_t len);
    void _sendONDATA();

    enum State { 
        IDLE,
        // This is the normal RX message flow
        OPEN_RX, 
        // RX - waiting to send the RTCP ping
        OPEN_RX_RTCP_PING_0, 
        // RX - waiting for the send ACK on the RTCP ping
        OPEN_RX_RTCP_PING_1, 
        // RX - waiting to send the RTP ping
        OPEN_RX_RTP_PING_0, 
        // RX - waiting for the send ACK on the RTP ping
        OPEN_RX_RTP_PING_1, 
        // This is the normal TX message flow
        OPEN_TX,
        // TX - waiting for the send ACK on an audio packet
        OPEN_TX_AUDIO_1,
        // TX - waiting to send the RTCP ping
        OPEN_TX_RTCP_PING_0, 
        // TX - waiting for the send ACK on the RTCP ping
        OPEN_TX_RTCP_PING_1, 
        // QSO finished normally
        SUCCEEDED, 
        // QSO failed/aborted unexpectedly
        FAILED 
    } _state;

    CommContext* _ctx;
    UserInfo* _userInfo;
    AudioOutputContext* _audioOutput;

    CallSign _callSign;
    FixedString _fullName;
    FixedString _location;
    IPAddress _targetAddr;
    Channel _rtpChannel;
    Channel _rtcpChannel;
    uint32_t _ssrc;
    uint32_t _lastRTCPKeepAliveSentMs;
    uint32_t _lastRTPKeepAliveSentMs;
    uint32_t _lastRecvMs;

    Decoder _gsmDecoder;
    Encoder _gsmEncoder;

    bool _txAudioPending;
    int16_t _txAudio[160 * 4];
    uint32_t _lastTxAudioTime;
    uint16_t _txSequenceCounter;
};

}

#endif

