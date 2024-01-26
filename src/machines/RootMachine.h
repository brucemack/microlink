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
#ifndef _RootMachine_h
#define _RootMachine_h

#include "../StateMachine.h"
#include "../Event.h"
#include "../Context.h"

#include "LogonMachine.h"
#include "LookupMachine.h"
#include "QSOConnectMachine.h"
#include "QSOFlowMachine.h"
#include "WaitMachine.h"

namespace kc1fsz {

class RootMachine : public StateMachine<Event, Context> {
public:

    RootMachine();

    virtual void processEvent(const Event* event, Context* context);
    virtual void start(Context* ctx);
    virtual bool isDone() const;
    virtual bool isGood() const;

    void setServerName(HostName h);
    void setCallSign(CallSign cs);
    void setPassword(String s);
    void setTargetCallSign(CallSign target);
    void setLocation(String loc);

private:

    enum State { IDLE, LOGON, LOOKUP, CONNECT, CONNECT_RETRY_WAIT, 
        QSO, BYE, FAILED, SUCCEEDED } _state;
    uint16_t _stateCount;

    LogonMachine _logonMachine;
    LookupMachine _lookupMachine;
    QSOConnectMachine _connectMachine;
    WaitMachine _connectRetryWaitMachine;
    QSOFlowMachine _qsoMachine;
};

}

#endif

