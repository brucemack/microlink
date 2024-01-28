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

#include "../Event.h"
#include "../Context.h"
#include "../StateMachine.h"
#include "../IPAddress.h"
#include "../Channel.h"
#include "../FixedString.h"
#include "../CallSign.h"

namespace kc1fsz {

class QSOConnectMachine : public StateMachine<Context> {
public:

    static uint32_t formatOnDataPacket(const char* msg, uint32_t ssrc,
        uint8_t* packet, uint32_t packetSize);

    static uint32_t formatRTCPPacket_SDES(uint32_t ssrc,
        CallSign callSign, 
        FixedString fullName,
        uint32_t ssrc2,
        uint8_t* packet, uint32_t packetSize);      


    virtual void processEvent(const Event* ev, Context* context);
    virtual void start(Context* ctx);
    virtual bool isDone() const;
    virtual bool isGood() const;

    void setCallSign(CallSign cs) { _callSign = cs; }
    void setFullName(FixedString s) { _fullName = s; }
    void setLocation(FixedString l) { _location = l; }
    void setTargetAddress(IPAddress addr) { _targetAddr = addr; }

    Channel getRTCPChannel() const;
    Channel getRTPChannel() const;
    uint32_t getSSCR() const;

private:

    static uint32_t _ssrcCounter;

    enum State { IDLE, CONNECTING, SUCCEEDED, FAILED } _state;

    CallSign _callSign;
    FixedString _fullName;
    FixedString _location;
    IPAddress _targetAddr;
    Channel _rtpChannel;
    Channel _rtcpChannel;
    uint32_t _ssrc;
};

}

#endif

