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
#ifndef _LogonMachine_h
#define _LogonMachine_h

#include "../Event.h"
#include "../Context.h"
#include "../StateMachine.h"

#include "../HostName.h"
#include "../TCPChannel.h"
#include "../CallSign.h"
#include "../FixedString.h"

namespace kc1fsz {

/**
 * This state machine is used to manage the process of logging 
 * on to the EchoLink server.
 */
class LogonMachine : public StateMachine<Context> {
public:

    LogonMachine();

    virtual void processEvent(const Event* ev, Context* context);
    virtual void start(Context* ctx);
    virtual bool isDone() const;
    virtual bool isGood() const;

    void setServerName(HostName hn) { _serverHostName = hn; }
    void setCallSign(CallSign cs) { _callSign = cs; }
    void setPassword(FixedString pw) { _password = pw; }
    void setLocation(FixedString loc) { _location = loc; }

private:

    enum State { IDLE, DNS_WAIT, CONNECTING, WAITING_FOR_DISCONNECT, FAILED, SUCCEEDED } _state;

    HostName _serverHostName;
    CallSign _callSign;
    FixedString _password;
    FixedString _location;

    TCPChannel _channel;

    // Here is were we collect the logon response
    static const uint16_t _logonRespSize = 64;
    uint8_t _logonResp[_logonRespSize];
    uint16_t _logonRespPtr;
};

/**
 * A utility function for building Logon/ONLINE request messages.
*/
uint32_t createOnlineMessage(uint8_t* buf, uint32_t bufLen,
    CallSign cs, FixedString pwd, FixedString loc);

}

#endif
