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

/**
 * This is the state machine for the main flow of an EchoLink QSO.
*/
class QSOFlowMachine : public StateMachine {
public:

    static int traceLevel;

    QSOFlowMachine(CommContext* ctx, UserInfo* userInfo, 
        AudioOutputContext* audioOutput,
        bool useLocalSsrc = false);

    void setCallSign(CallSign cs) { _callSign = cs; }
    void setFullName(FixedString fn) { _fullName = fn; }
    void setLocation(FixedString l) { _location = l; }

    void setRTCPChannel(Channel c) { _rtcpChannel = c; }
    void setRTPChannel(Channel c)  { _rtpChannel = c; }
    void setSSRC(uint32_t s) { _ssrc = s; }
    void setPeerAddress(IPAddress addr) { _peerAddr = addr; }

    bool requestCleanStop();

    /**
     * @param frame 160 x 4 samples of 16-bit PCM audio.
     * @return true if the audio was taken, or false if the 
     *   session is busy and the TX will need to be 
     *   retried.
    */
    bool txAudio(const int16_t* frame);

    uint32_t getAudioSeqErrors() const { return _audioSeqErrors; }

    // ----- From StateMachine ------------------------------------------------

    virtual void processEvent(const Event* ev);
    virtual void start();
    virtual void cleanup();
    virtual bool isDone() const;
    virtual bool isGood() const;

private:

    void _processRXReceive(const UDPReceiveEvent* evt);
    void _processTXReceive(const UDPReceiveEvent* evt);
    void _processONDATA(const uint8_t* d, uint32_t len);
    void _sendONDATA();
    void _sendBYE();

    enum State { 
        // STATE 0:
        IDLE,
        // STATE 1: This is the normal RX message flow
        OPEN_RX, 
        // STATE 2: RX - waiting to send the RTCP ping
        OPEN_RX_RTCP_PING_0, 
        // STATE 3: RX - waiting for the send ACK on the RTCP ping
        OPEN_RX_RTCP_PING_1, 
        // STATE 4: RX - waiting to send the RTP ping
        OPEN_RX_RTP_PING_0, 
        // STATE 5: RX - waiting for the send ACK on the RTP ping
        OPEN_RX_RTP_PING_1, 
        // STATE 6: This is the normal TX message flow
        OPEN_TX,
        // STATE 7: TX - waiting for the send ACK on an audio packet
        OPEN_TX_AUDIO_1,
        // STATE 8: TX - waiting to send the RTCP ping
        OPEN_TX_RTCP_PING_0, 
        // STATE 9: TX - waiting for the send ACK on the RTCP ping
        OPEN_TX_RTCP_PING_1, 
        // STATE 10: RX - waiting for the send ACK on the BYE message
        OPEN_RX_STOP_0,
        // QSO finished normally
        SUCCEEDED, 
        // QSO failed/aborted unexpectedly
        FAILED 
    };

    CommContext* _ctx;
    UserInfo* _userInfo;
    AudioOutputContext* _audioOutput;
    bool _useLocalSsrc;

    CallSign _callSign;
    FixedString _fullName;
    FixedString _location;
    IPAddress _peerAddr;

    Channel _rtpChannel;
    Channel _rtcpChannel;
    uint32_t _ssrc;
    uint32_t _lastRTCPKeepAliveSentMs;
    uint32_t _lastRTPKeepAliveSentMs;
    // The last time we received any kind of activity from the remote
    // station.  Used to manage keep-alive.
    uint32_t _lastRecvMs;

    Decoder _gsmDecoder;
    Encoder _gsmEncoder;

    // This flag is set when the user wants to close an open QSO.
    // This triggers a clean exit.
    bool _stopRequested;

    // Indicates that the remote station has asked for a clean
    // shutdown.
    bool _byeReceived;

    // The last time we had transmit audio queued, which can be 
    // used as the basis for transmit timeouts.
    uint32_t _lastTxAudioTime;

    // The last time we saw receive audio from the remote station, 
    // which can be used to manage squelch.
    uint32_t _lastRxAudioTime;

    // The sequence number of the last audio packet received.
    uint16_t _lastRxAudioSeq;

    // Indicates whether the system is actively receiving. 
    //bool _squelchOpen;

    // Buffer for outbound audio
    static const uint32_t _txAudioBufDepth = 2;
    int16_t _txAudioBuf[_txAudioBufDepth][160 * 4];
    uint32_t _txAudioWriteCount;
    uint32_t _txAudioSentCount;

    // Performance counters
    uint32_t _audioSeqErrors;
};

}

#endif

