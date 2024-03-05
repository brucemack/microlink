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
#include "kc1fsz-tools/Runnable.h"

#include "StateMachine.h"

namespace kc1fsz {

class CommContext;
class UserInfo;
class Conference;

class ConferenceBridge : public StateMachine, public Runnable {
public:

    static int traceLevel;

    static uint32_t formatOnDataPacket(const char* msg, uint32_t ssrc,
        uint8_t* packet, uint32_t packetSize);

    static uint32_t formatRTCPPacket_SDES(uint32_t ssrc,
        CallSign callSign, 
        FixedString fullName,
        uint32_t ssrc2,
        uint8_t* packet, uint32_t packetSize);      

    ConferenceBridge(CommContext* ctx, UserInfo* userInfo, Conference* conf);

    uint32_t getSSRC() const { return _localSsrc; }

    void requestCleanStop() { }

    // ----- From StateMachine ---------------------------------------------------

    virtual void processEvent(const Event* ev);
    virtual void start();
    virtual bool isDone() const;
    virtual bool isGood() const;

    // ----- From Runnable -------------------------------------------------------

    virtual bool run();

private:

    static uint32_t _ssrcCounter;

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
        SUCCEEDED, 
        FAILED 
    };

    CommContext* _ctx;
    UserInfo* _userInfo;
    Conference* _conf;

    Channel _rtpChannel;
    Channel _rtcpChannel;
    // This is generated
    uint32_t _localSsrc;
    uint32_t _remoteSsrc;
};

}

#endif

