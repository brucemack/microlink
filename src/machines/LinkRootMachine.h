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
#ifndef _LinkRootMachine_h
#define _LinkRootMachine_h

#include "kc1fsz-tools/Event.h"
#include "kc1fsz-tools/Runnable.h"
#include "kc1fsz-tools/AudioProcessor.h"

#include "../StateMachine.h"

#include "QSOAcceptMachine.h"
#include "QSOFlowMachine.h"
#include "WaitMachine.h"

namespace kc1fsz {

class CommContext;
class UserInfo;
class AudioOutputContext;

class LinkRootMachine : public StateMachine, public AudioProcessor, public Runnable {
public:

    static int traceLevel;

    LinkRootMachine(CommContext* ctx, UserInfo* userInfo, AudioOutputContext* audioOutput);

    void setServerName(HostName h);
    void setServerPort(uint32_t p);
    void setCallSign(CallSign cs);
    void setPassword(FixedString s);
    void setFullName(FixedString n);
    void setLocation(FixedString loc);

    bool isInQSO() const;

    /**
     * Call this to put a QSO into a clean shutdown.  This will not be 
     * a synchronous event so the state machine needs to continue to run.
    */
    bool requestCleanStop();

    // ----- From StateMachine ------------------------------------------------

    virtual void processEvent(const Event* event);
    virtual void start();
    virtual bool isDone() const;
    virtual bool isGood() const;

    // ----- From AudioProcessor -----------------------------------------------

    /**
     * @param frame 160 x 4 samples of 16-bit PCM audio.
     * @return true if the audio was taken, or false if the 
     *   session is busy and the TX will need to be 
     *   retried.
    */
    virtual bool play(const int16_t* frame);

    // ----- From Runnable ----------------------------------------------------

    virtual bool run();

private:

    enum State { 
        IDLE, 
        // STATE 1:
        IN_RESET,
        ACCEPTING,
        IN_VALIDATION,
        QSO, 
        BYE, 
        FAILED, 
        SUCCEEDED 
    } _state;

    uint16_t _stateCount;

    CommContext* _ctx;
    UserInfo* _userInfo;
    WaitMachine _connectRetryWaitMachine;
    QSOAcceptMachine _acceptMachine;
    QSOFlowMachine _qsoMachine;
};

}

#endif

