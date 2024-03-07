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
#ifndef _LogonMachine2_h
#define _LogonMachine2_h

#include "kc1fsz-tools/HostName.h"
#include "kc1fsz-tools/Channel.h"
#include "kc1fsz-tools/CallSign.h"
#include "kc1fsz-tools/FixedString.h"
#include "kc1fsz-tools/IPLib.h"

#include "../StateMachine2.h"

namespace kc1fsz {

class UserInfo;
class Log;
class Conference;

/**
 * This state machine is used to manage the process of logging 
 * on to the EchoLink server from time to time.
 */
class LogonMachine2 : public StateMachine2, public IPLibEvents {
public:

    static int traceLevel;

    LogonMachine2(IPLib* ctx, UserInfo* userInfo, Log* log);

    void setServerName(HostName hn) { _serverHostName = hn; }
    void setServerPort(uint32_t p) { _serverPort = p; }
    void setCallSign(CallSign cs) { _callSign = cs; }
    void setPassword(FixedString pw) { _password = pw; }
    void setLocation(FixedString loc) { _location = loc; }
    void setConference(Conference* conf) { _conf = conf; }
    uint32_t secondsSinceLastLogon() const;

    // ----- From IPLibEvents -------------------------------------------------

    virtual void dns(HostName name, IPAddress addr);
    virtual void bind(Channel ch) { }
    virtual void conn(Channel ch);
    virtual void disc(Channel ch);
    virtual void recv(Channel ch, const uint8_t* data, uint32_t dataLen, IPAddress fromAddr,
        uint16_t fromPort);
    virtual void err(Channel ch, int type) { }

    // ----- From StateMachine2 -----------------------------------------------

protected:

    virtual void _process(int state, bool entry);

private:

    enum State { 
        IDLE, 
        LINK_WAIT,
        DNS_WAIT, 
        CONNECT_WAIT, 
        DISCONNECT_WAIT,
        WAIT,
        FAILED,
        SUCCEEDED,
    };

    IPLib* _ctx;
    UserInfo* _userInfo;
    Log* _log;
    Conference* _conf;
    
    HostName _serverHostName;
    uint32_t _serverPort;
    CallSign _callSign;
    FixedString _password;
    FixedString _location;

    Channel _channel;
    
    // Here is were we collect the logon response
    static const uint16_t _logonRespSize = 64;
    uint8_t _logonResp[_logonRespSize];
    uint16_t _logonRespPtr;

    uint32_t _lastLogonStamp;
};

}

#endif
