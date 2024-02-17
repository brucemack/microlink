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
#ifndef _ValidationMachine_h
#define _ValidationMachine_h

#include "kc1fsz-tools/Channel.h"
#include "kc1fsz-tools/Event.h"
#include "kc1fsz-tools/IPAddress.h"
#include "kc1fsz-tools/HostName.h"
#include "kc1fsz-tools/CallSign.h"

#include "../StateMachine.h"

namespace kc1fsz {

class CommContext;
class UserInfo;

/**
 * A state machine used for managing the EL directory lookup
 * in order to validate a connection from a potential 
 * client.
 */
class ValidationMachine : public StateMachine {
public:

    static int traceLevel;

    ValidationMachine(CommContext* ctx, UserInfo* userInfo);

    virtual void processEvent(const Event* ev);
    virtual void start();
    virtual void cleanup();
    virtual bool isDone() const;
    virtual bool isGood() const;

    void setServerName(HostName hn) { _serverHostName = hn; }
    void setServerPort(uint32_t p) { _serverPort = p; }
    void setRequestCallSign(CallSign cs) { _requestCallSign = cs; }
    void setRequestAddr(IPAddress addr) { _requestAddr = addr; }

    CallSign getRequestCallSign() const { return _requestCallSign; }
    IPAddress getRequestAddr() const { return _requestAddr; }
    bool isValid() const { return _isValid; }

private:

    enum State { IDLE, DNS_WAIT, CONNECTING, WAITING_FOR_DISCONNECT, 
        FAILED, SUCCEEDED } _state;

    CommContext* _ctx;
    UserInfo* _userInfo;

    HostName _serverHostName;
    uint32_t _serverPort;
    CallSign _requestCallSign;
    IPAddress _requestAddr;
    // The result of the validation request
    bool _isValid;

    Channel _channel;
};

}

#endif

