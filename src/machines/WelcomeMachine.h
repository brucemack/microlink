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
#ifndef _WelcomeMachine_h
#define _WelcomeMachine_h

#include "kc1fsz-tools/Channel.h"
#include "kc1fsz-tools/Event.h"
#include "kc1fsz-tools/IPAddress.h"
#include "kc1fsz-tools/HostName.h"
#include "kc1fsz-tools/CallSign.h"

#include "../StateMachine.h"
#include "../Synth.h"

namespace kc1fsz {

class CommContext;
class UserInfo;
class AudioProcessor;

class WelcomeMachine : public StateMachine {
public:

    static int traceLevel;

    WelcomeMachine(CommContext* ctx, UserInfo* userInfo, AudioProcessor* audioOut);

    virtual void processEvent(const Event* ev);
    virtual void start();
    virtual void cleanup();
    virtual bool isDone() const;
    virtual bool isGood() const;

    void setCallSign(CallSign cs) { _callSign = cs; }

private:

    enum State { IDLE, 
        PLAYING,  
        FAILED, 
        SUCCEEDED 
    } _state;

    CommContext* _ctx;
    UserInfo* _userInfo;

    CallSign _callSign;
    Synth _synth;
};

}

#endif

