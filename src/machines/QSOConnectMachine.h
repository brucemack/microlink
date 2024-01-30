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
#ifndef _QSOConnectMachine_h
#define _QSOConnectMachine_h

#include "kc1fsz-tools/Event.h"
#include "kc1fsz-tools/IPAddress.h"
#include "kc1fsz-tools/Channel.h"
#include "kc1fsz-tools/FixedString.h"
#include "kc1fsz-tools/CallSign.h"

#include "../StateMachine.h"

namespace kc1fsz {

class CommContext;
class UserInfo;

class QSOConnectMachine : public StateMachine {
public:

    static uint32_t formatOnDataPacket(const char* msg, uint32_t ssrc,
        uint8_t* packet, uint32_t packetSize);

    static uint32_t formatRTCPPacket_SDES(uint32_t ssrc,
        CallSign callSign, 
        FixedString fullName,
        uint32_t ssrc2,
        uint8_t* packet, uint32_t packetSize);      

    QSOConnectMachine(CommContext* ctx, UserInfo* userInfo);

    virtual void processEvent(const Event* ev);
    virtual void start();
    virtual bool isDone() const;
    virtual bool isGood() const;

    void setCallSign(CallSign cs) { _callSign = cs; }
    void setFullName(FixedString s) { _fullName = s; }
    void setLocation(FixedString l) { _location = l; }
    void setTargetAddress(IPAddress addr) { _targetAddr = addr; }

    Channel getRTCPChannel() const { return _rtcpChannel; }
    Channel getRTPChannel() const  { return _rtpChannel; }
    uint32_t getSSRC() const { return _ssrc; }

private:

    static uint32_t _ssrcCounter;

    enum State { IDLE, CONNECTING, SUCCEEDED, FAILED } _state;

    CommContext* _ctx;
    UserInfo* _userInfo;

    CallSign _callSign;
    FixedString _fullName;
    FixedString _location;
    IPAddress _targetAddr;
    Channel _rtpChannel;
    Channel _rtcpChannel;
    uint32_t _ssrc;
    uint32_t _retryCount;
};

}

#endif

