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

//#include "kc1fsz-tools/Event.h"
#include "kc1fsz-tools/IPAddress.h"
#include "kc1fsz-tools/CallSign.h"
#include "kc1fsz-tools/FixedString.h"
#include "kc1fsz-tools/Channel.h"
//#include "kc1fsz-tools/AudioOutputContext.h"
#include "gsm-0610-codec/Decoder.h"

#include "../StateMachine.h"
//#include "../UserInfo.h"

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

private:
    
    void _processUDPReceive(const UDPReceiveEvent* evt);
    void _processTick(const TickEvent* evt);

    enum State { 
        IDLE,
        OPEN, 
        OPEN_RTCP_PING_0,
        // STATE #3: 
        OPEN_RTCP_PING_1,
        OPEN_RTP_PING_0,
        OPEN_RTP_PING_1,
        SUCCEEDED, 
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
    uint32_t _lastKeepAliveSentMs;
    uint32_t _lastKeepAliveRecvMs;

    Decoder _gsmDecoder;
};

}

#endif

