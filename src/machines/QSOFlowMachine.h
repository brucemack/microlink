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

#include "../Event.h"
#include "../StateMachine.h"
#include "../IPAddress.h"
#include "../CallSign.h"
#include "../FixedString.h"
#include "../Channel.h"
#include "../UserInfo.h"

namespace kc1fsz {

class UserInfo;
class CommContext;

class QSOFlowMachine : public StateMachine {
public:

    QSOFlowMachine(CommContext* ctx, UserInfo* userInfo);

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

    void _audioTick();
    void _decodeGSMFrame(uint8_t* frame);

    enum State { IDLE, OPEN, SUCCEEDED } _state;

    CommContext* _ctx;
    UserInfo* _userInfo;

    CallSign _callSign;
    FixedString _fullName;
    FixedString _location;
    IPAddress _targetAddr;
    Channel _rtpChannel;
    Channel _rtcpChannel;
    uint32_t _ssrc;
    uint32_t _lastKeepAliveSentMs;
    uint32_t _lastKeepAliveRecvMs;

    // This is a circular buffer used to keep track
    // of audio frames waiting for the next audio pulse
    static const uint32_t _frameQueueDepth = 16;
    uint8_t _frameQueue[_frameQueueDepth][33];
    uint32_t _frameQueueSize = 0;
    uint32_t _frameQueueWritePtr = 0;
    uint32_t _frameQueueReadPtr = 0;
};

}

#endif

